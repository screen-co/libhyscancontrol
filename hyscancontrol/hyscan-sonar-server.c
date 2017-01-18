/*
 * \file hyscan-sonar-server.c
 *
 * \brief Исходный файл сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-messages.h"
#include "hyscan-sonar-server.h"
#include "hyscan-sonar-rpc.h"

#include <hyscan-data-schema.h>
#include <hyscan-slice-pool.h>
#include <urpc-server.h>

#include <gio/gio.h>
#include <string.h>
#include <zlib.h>

#define TARGET_SPEED_LOCAL     5000000000
#define TARGET_SPEED_10M       1250000
#define TARGET_SPEED_100M      12500000
#define TARGET_SPEED_1G        125000000
#define TARGET_SPEED_10G       1250000000

#define TIMER_GRANULARITY      (G_USEC_PER_SEC / 1000)

#define hyscan_sonar_server_set_error(p)   do { \
                                             g_warning ("HyScanSonarServer: can't set '%s->%s' value", \
                                                        __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_sonar_server_get_error(p)   do { \
                                             g_warning ("HyScanSonarServer: can't get '%s->%s' value", \
                                                        __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_SONAR,
  PROP_HOST
};

struct _HyScanSonarServerPrivate
{
  HyScanParam         *sonar;                  /* Указатель на интерфейс управления локатором. */
  uRpcServer          *rpc;                    /* RPC сервер. */
  gchar               *host;                   /* Адрес на котором запускается сервер. */

  gint                 sid;                    /* Идентификатор сессии клиента заблокировавшего гидролокатор. */

  gpointer             buffer;                 /* Буфер данных. */
  guint32              index;                  /* Номер пакета. */

  gdouble              target_speed;           /* Целевая скорость отправки данных. */
  guint32              data_chunk;             /* Размер отправленных данных за период времени. */
  guint32              data_chunk_limit;       /* Размер максимального блока отправленных данных за раз. */
  GTimer              *timer;                  /* Таймер отправки данных. */

  GRWLock              lock;                   /* Блокировка доступа к адресу клиента. */
  GSocket             *socket;                 /* Сокет отправки данных. */
  GSocketAddress      *address;                /* Адрес клиента, для отправки данных. */
};

static void    hyscan_sonar_server_set_property                (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void    hyscan_sonar_server_object_finalize             (GObject                       *object);

static void    hyscan_sonar_server_sender                      (HyScanSonarServerPrivate      *priv,
                                                                HyScanSonarMessage            *message);

static gint    hyscan_sonar_server_rpc_proc_version            (guint32                        session,
                                                                uRpcData                      *urpc_data,
                                                                void                          *proc_data,
                                                                void                          *key_data);
static gint    hyscan_sonar_server_rpc_proc_get_schema         (guint32                        session,
                                                                uRpcData                      *urpc_data,
                                                                void                          *proc_data,
                                                                void                          *key_data);
static gint    hyscan_sonar_server_rpc_proc_set_master         (guint32                        session,
                                                                uRpcData                      *urpc_data,
                                                                void                          *proc_data,
                                                                void                          *key_data);
static gint    hyscan_sonar_server_rpc_proc_set                (guint32                        session,
                                                                uRpcData                      *urpc_data,
                                                                void                          *proc_data,
                                                                void                          *key_data);
static gint    hyscan_sonar_server_rpc_proc_get                (guint32                        session,
                                                                uRpcData                      *urpc_data,
                                                                void                          *proc_data,
                                                                void                          *key_data);

static void    hyscan_sonar_server_rpc_disconnect              (guint32                        session,
                                                                void                          *proc_data,
                                                                void                          *key_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarServer, hyscan_sonar_server, G_TYPE_OBJECT)

static void hyscan_sonar_server_class_init (HyScanSonarServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_server_set_property;

  object_class->finalize = hyscan_sonar_server_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "HyScan sonar", HYSCAN_TYPE_PARAM,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_HOST,
    g_param_spec_string ("host", "Host", "HyScan sonar server host", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_server_init (HyScanSonarServer *server)
{
  HyScanSonarServerPrivate *priv;

  server->priv = hyscan_sonar_server_get_instance_private (server);
  priv = server->priv;

  g_rw_lock_init (&priv->lock);
  priv->buffer = g_malloc (65536);

  priv->timer = g_timer_new ();
  priv->target_speed = TARGET_SPEED_LOCAL;
  priv->data_chunk_limit = priv->target_speed / TIMER_GRANULARITY;
}

static void
hyscan_sonar_server_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanSonarServer *server = HYSCAN_SONAR_SERVER (object);
  HyScanSonarServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      priv->sonar = g_value_dup_object (value);
      break;

    case PROP_HOST:
      priv->host = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_server_object_finalize (GObject *object)
{
  HyScanSonarServer *server = HYSCAN_SONAR_SERVER (object);
  HyScanSonarServerPrivate *priv = server->priv;

  g_signal_handlers_disconnect_by_data (priv->sonar, priv);

  if (priv->rpc != NULL)
    urpc_server_destroy (priv->rpc);

  g_clear_object (&priv->sonar);
  g_clear_object (&priv->socket);
  g_clear_object (&priv->address);

  g_rw_lock_clear (&priv->lock);

  g_timer_destroy (priv->timer);

  g_free (priv->buffer);
  g_free (priv->host);

  G_OBJECT_CLASS (hyscan_sonar_server_parent_class)->finalize (object);
}

/* Функция отправляет данные от гидролокатора. */
static void
hyscan_sonar_server_sender (HyScanSonarServerPrivate *priv,
                            HyScanSonarMessage       *message)
{
  HyScanSonarRpcPacket *packet = priv->buffer;
  gdouble elapsed;
  guint32 packet_size;
  guint32 left_size;
  guint32 part_size;
  guint32 offset;
  guint32 crc;

  if (g_atomic_int_get (&priv->sid) == 0)
    return;

  g_rw_lock_reader_lock (&priv->lock);

  if (priv->address == NULL)
    goto exit;

  /* Сбрасываем таймер при длительной паузе. */
  elapsed = g_timer_elapsed (priv->timer, NULL);
  if (elapsed > (4.0 / TIMER_GRANULARITY))
    {
      g_timer_start (priv->timer);
      priv->data_chunk = 0;
    }

  /* Разбиваем сообщения на пакеты. */
  offset = 0;
  left_size = message->size;
  while (left_size > 0)
    {
      part_size = MIN(left_size, HYSCAN_SONAR_MSG_DATA_PART_SIZE);
      packet_size = part_size + offsetof (HyScanSonarRpcPacket, data);

      /* Заголовок пакета. */
      packet->magic = GUINT32_TO_LE (HYSCAN_SONAR_RPC_MAGIC);
      packet->version = GUINT32_TO_LE (HYSCAN_SONAR_RPC_VERSION);
      packet->index = GUINT32_TO_LE (priv->index);
      packet->crc32 = 0;
      packet->time = GUINT64_TO_LE (message->time);
      packet->id = GUINT32_TO_LE (message->id);
      packet->type = GUINT32_TO_LE (message->type);
      packet->rate = hyscan_sonar_rpc_float_to_le (message->rate);
      packet->size = GUINT32_TO_LE (message->size);
      packet->part_size = GUINT32_TO_LE (part_size);
      packet->offset = GUINT32_TO_LE (offset);
      memcpy (packet->data, (guint8*)message->data + offset, part_size);

      /* Контрольная сумма. */
      crc = crc32 (0L, Z_NULL, 0);
      crc = crc32 (crc, (gpointer)packet, packet_size);
      packet->crc32 = GUINT32_TO_LE (crc);

      /* Отправляем сообщение. */
      g_socket_send_to (priv->socket, priv->address, (gpointer)packet, packet_size, NULL, NULL);

      elapsed = g_timer_elapsed (priv->timer, NULL);
      priv->data_chunk += packet_size;

      left_size -= part_size;
      offset += part_size;

      if (priv->index == G_MAXUINT32)
        priv->index = 0;
      else
        priv->index += 1;

      /* Измеряем текущую скорость передачи, приостанавливаем отправку при необходимости. */
      if (priv->data_chunk > priv->data_chunk_limit)
        {
          if ((priv->data_chunk / elapsed) > priv->target_speed)
            g_usleep (G_USEC_PER_SEC * ((priv->data_chunk / priv->target_speed) - elapsed));

          g_timer_start (priv->timer);
          priv->data_chunk = 0;
        }
    }

exit:
  g_rw_lock_reader_unlock (&priv->lock);
}

/* RPC функция HYSCAN_SONAR_RPC_PROC_VERSION. */
static gint
hyscan_sonar_server_rpc_proc_version (guint32   session,
                                      uRpcData *urpc_data,
                                      void     *proc_data,
                                      void     *key_data)
{
  urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VERSION, HYSCAN_SONAR_RPC_VERSION);
  urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_MAGIC, HYSCAN_SONAR_RPC_MAGIC);

  return 0;
}

/* RPC функция HYSCAN_SONAR_RPC_PROC_GET_SCHEMA. */
static gint
hyscan_sonar_server_rpc_proc_get_schema (guint32   session,
                                         uRpcData *urpc_data,
                                         void     *proc_data,
                                         void     *key_data)
{
  HyScanSonarServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_SONAR_RPC_STATUS_FAIL;

  HyScanDataSchema *schema;
  gchar *rpc_schema_data;
  gchar *schema_data;
  gchar *schema_id = NULL;

  GConverterResult converter_result;
  GZlibCompressor *compressor;
  gsize readed, writed;

  schema = hyscan_param_schema (priv->sonar);
  if (schema == NULL)
    goto exit;

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_SIZE, 0) != 0)
    hyscan_sonar_server_set_error ("schema_size");

  schema_id = hyscan_data_schema_get_id (schema);
  if (urpc_data_set_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_ID, schema_id) != 0)
    hyscan_sonar_server_set_error ("schema_id");
  g_free (schema_id);

  schema_data = hyscan_data_schema_get_data (schema, NULL, NULL);
  if (schema_data == NULL)
    hyscan_sonar_server_set_error ("schema_data");

  rpc_schema_data = urpc_data_set (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_DATA,
                                   NULL, HYSCAN_SONAR_MSG_DATA_PART_SIZE);
  if (rpc_schema_data == NULL)
    hyscan_sonar_server_set_error ("schema_data");

  compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_ZLIB, 9);
  converter_result = g_converter_convert (G_CONVERTER (compressor),
                                          schema_data, strlen (schema_data),
                                          rpc_schema_data, HYSCAN_SONAR_MSG_DATA_PART_SIZE,
                                          G_CONVERTER_INPUT_AT_END,
                                          &readed, &writed, NULL);
  g_object_unref (compressor);
  g_free (schema_data);

  if (converter_result != G_CONVERTER_FINISHED)
    {
      urpc_data_set (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_ID, NULL, 0);
      urpc_data_set (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_DATA, NULL, 0);
      goto exit;
    }

  if (urpc_data_set (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_DATA, NULL, writed) == NULL)
    hyscan_sonar_server_set_error ("schema_data");

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_SIZE, readed) != 0)
    hyscan_sonar_server_set_error ("schema_size");

  rpc_status = HYSCAN_SONAR_RPC_STATUS_OK;

exit:
  g_clear_object (&schema);
  urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC функция HYSCAN_SONAR_RPC_PROC_SET_MASTER. */
static gint
hyscan_sonar_server_rpc_proc_set_master (guint32   session,
                                         uRpcData *urpc_data,
                                         void     *proc_data,
                                         void     *key_data)
{
  HyScanSonarServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_SONAR_RPC_STATUS_FAIL;

  const gchar *host;
  guint32 port;

  host = urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_MASTER_HOST, 0);
  if (host == NULL)
    hyscan_sonar_server_get_error ("host");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_MASTER_PORT, &port) != 0)
    hyscan_sonar_server_get_error ("port");

  if (port < HYSCAN_SONAR_RPC_MIN_PORT || port > HYSCAN_SONAR_RPC_MAX_PORT)
    {
      g_warning ("HyScanSonarServer: port range error");
      goto exit;
    }

  /* Запоминаем идентификатор сессии клиента устанавливающего master соединение. */
  if (!g_atomic_int_compare_and_exchange (&priv->sid, 0, session))
    goto exit;

  /* Если master соединение установлено, запоминаем адрес клиента. */
  g_rw_lock_writer_lock (&priv->lock);
  g_clear_object (&priv->address);
  priv->address = g_inet_socket_address_new_from_string (host, port);
  priv->index = 0;
  g_rw_lock_writer_unlock (&priv->lock);

  if (priv->address != NULL)
    rpc_status = HYSCAN_SONAR_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC функция HYSCAN_SONAR_RPC_PROC_SET. */
static gint
hyscan_sonar_server_rpc_proc_set (guint32   session,
                                  uRpcData *urpc_data,
                                  void     *proc_data,
                                  void     *key_data)
{
  HyScanSonarServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_SONAR_RPC_STATUS_FAIL;

  const gchar **names = NULL;
  GVariant **values = NULL;

  gint n_params;
  gint i;

  for (i = 0; i < HYSCAN_SONAR_RPC_MAX_PARAMS; i++)
    {
      const gchar *param_name;
      guint32 param_type;

      param_name = urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_NAME0 + i, 0);
      if (param_name == NULL)
        break;

      if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, &param_type) != 0)
        hyscan_sonar_server_get_error ("type");

      switch (param_type)
        {
        case HYSCAN_SONAR_RPC_TYPE_NULL:
          break;

        case HYSCAN_SONAR_RPC_TYPE_BOOLEAN:
          {
            guint32 param_value;
            if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_sonar_server_get_error ("value");
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_INT64:
          {
            gint64 param_value;
            if (urpc_data_get_int64 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_sonar_server_get_error ("value");
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_DOUBLE:
          {
            gdouble param_value;
            if (urpc_data_get_double (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_sonar_server_get_error ("value");
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_STRING:
          {
            if (urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, 0) == NULL)
              hyscan_sonar_server_get_error ("value");
          }
          break;

        default:
          goto exit;
        }
    }

  if (i == 0 || i >= HYSCAN_SONAR_RPC_MAX_PARAMS)
    hyscan_sonar_server_get_error ("n_params");

  n_params = i;
  names = g_malloc0 ((n_params + 1) * sizeof (gchar*));
  values = g_malloc0 ((n_params + 1) * sizeof (GVariant*));

  for (i = 0; i < n_params; i++)
    {
      guint32 param_type;

      names[i] = urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_NAME0 + i, 0);
      urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, &param_type);

      switch (param_type)
        {
        case HYSCAN_SONAR_RPC_TYPE_NULL:
          break;

        case HYSCAN_SONAR_RPC_TYPE_BOOLEAN:
          {
            guint32 param_value;
            urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value);
            values[i] = g_variant_new_boolean (param_value ? TRUE : FALSE);
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_INT64:
          {
            gint64 param_value;
            urpc_data_get_int64 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value);
            values[i] = g_variant_new_int64 (param_value);
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_DOUBLE:
          {
            gdouble param_value;
            urpc_data_get_double (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value);
            values[i] = g_variant_new_double (param_value);
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_STRING:
          {
            const gchar *param_value;
            param_value = urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, 0);
            values[i] = g_variant_new_string (param_value);
          }
          break;

        default:
          break;
        }
    }

  if (hyscan_param_set (priv->sonar, names, values))
    {
      rpc_status = HYSCAN_SONAR_RPC_STATUS_OK;
    }
  else
    {
      for (i = 0; i < n_params; i++)
        g_variant_unref (values[i]);
    }

exit:
  g_free (names);
  g_free (values);

  urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC функция HYSCAN_SONAR_RPC_PROC_GET. */
static gint
hyscan_sonar_server_rpc_proc_get (guint32   session,
                                  uRpcData *urpc_data,
                                  void     *proc_data,
                                  void     *key_data)
{
  HyScanSonarServerPrivate *priv = proc_data;
  guint32 rpc_status = HYSCAN_SONAR_RPC_STATUS_FAIL;

  const gchar **names = NULL;
  GVariant **values = NULL;

  gint n_params;
  gint i;

  for (i = 0; i < HYSCAN_SONAR_RPC_MAX_PARAMS; i++)
    if (urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_NAME0 + i, 0) == NULL)
      break;

  if (i == 0 || i >= HYSCAN_SONAR_RPC_MAX_PARAMS)
    hyscan_sonar_server_get_error ("name");

  n_params = i;
  names = g_malloc0 ((n_params + 1) * sizeof (gchar*));
  values = g_malloc0 ((n_params + 1) * sizeof (GVariant*));

  for (i = 0; i < n_params; i++)
    names[i] = urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_NAME0 + i, 0);

  if (hyscan_param_get (priv->sonar, names, values))
    {
      GVariantClass value_type;

      for (i = 0; i < n_params; i++)
        {
          value_type = 0;

          if (values[i] != NULL)
            {
              value_type = g_variant_classify (values[i]);
            }
          else
            {
              if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_NULL) != 0)
                hyscan_sonar_server_set_error ("type");
            }

          switch (value_type)
            {
            case G_VARIANT_CLASS_BOOLEAN:
              {
                gboolean value = g_variant_get_boolean (values[i]);

                if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_BOOLEAN) != 0)
                  hyscan_sonar_server_set_error ("type");

                if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value ? 1 : 0) != 0)
                  hyscan_sonar_server_set_error ("value");
              }
              break;

            case G_VARIANT_CLASS_INT64:
              {
                gint64 value = g_variant_get_int64 (values[i]);

                if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_INT64) != 0)
                  hyscan_sonar_server_set_error ("type");

                if (urpc_data_set_int64 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value) != 0)
                  hyscan_sonar_server_set_error ("value");
              }
              break;

            case G_VARIANT_CLASS_DOUBLE:
              {
                gdouble value = g_variant_get_double (values[i]);

                if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_DOUBLE) != 0)
                  hyscan_sonar_server_set_error ("type");

                if (urpc_data_set_double (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value) != 0)
                  hyscan_sonar_server_set_error ("value");
              }
              break;

            case G_VARIANT_CLASS_STRING:
              {
                const gchar *value = g_variant_get_string (values[i], NULL);

                if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_STRING) != 0)
                  hyscan_sonar_server_set_error ("type");

                if (urpc_data_set_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value) != 0)
                  hyscan_sonar_server_set_error ("value");
              }
              break;

            default:
              break;
            }

          g_clear_pointer (&values[i], g_variant_unref);
        }

      rpc_status = HYSCAN_SONAR_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, rpc_status);

  return 0;
}

/* Функция вызывается при отключении клиента. */
static void
hyscan_sonar_server_rpc_disconnect (guint32  session,
                                    void    *proc_data,
                                    void    *key_data)
{
  HyScanSonarServerPrivate *priv = proc_data;

  if (g_atomic_int_compare_and_exchange (&priv->sid, session, 0))
    {
      g_rw_lock_writer_lock (&priv->lock);
      g_clear_object (&priv->address);
      g_rw_lock_writer_unlock (&priv->lock);
    }
}

/* Функция создаёт новый объект HyScanSonarServer. */
HyScanSonarServer *
hyscan_sonar_server_new (HyScanParam *sonar,
                         const gchar *host)
{
  return g_object_new (HYSCAN_TYPE_SONAR_SERVER,
                       "sonar", sonar,
                       "host", host,
                       NULL);
}

/* Функция устанавливает целевую скорость передачи данных клиенту. */
gboolean
hyscan_sonar_server_set_target_speed (HyScanSonarServer            *server,
                                      HyScanSonarServerTargetSpeed  speed)
{
  HyScanSonarServerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SERVER (server), FALSE);

  priv = server->priv;

  if (speed == HYSCAN_SONAR_SERVER_TARGET_SPEED_LOCAL)
    priv->target_speed = TARGET_SPEED_LOCAL;
  else if (speed == HYSCAN_SONAR_SERVER_TARGET_SPEED_10M)
    priv->target_speed = TARGET_SPEED_10M;
  else if (speed == HYSCAN_SONAR_SERVER_TARGET_SPEED_100M)
    priv->target_speed = TARGET_SPEED_100M;
  else if (speed == HYSCAN_SONAR_SERVER_TARGET_SPEED_1G)
    priv->target_speed = TARGET_SPEED_1G;
  else if (speed == HYSCAN_SONAR_SERVER_TARGET_SPEED_10G)
    priv->target_speed = TARGET_SPEED_10G;
  else
    return FALSE;

  priv->data_chunk_limit = priv->target_speed / TIMER_GRANULARITY;

  return TRUE;
}

/* Функция запускает сервер управления гидролокатором в работу. */
gboolean
hyscan_sonar_server_start (HyScanSonarServer *server,
                           gdouble            timeout)
{
  HyScanSonarServerPrivate *priv;

  GSocket *socket = NULL;
  GSocketAddress *address = NULL;

  gchar *uri;
  gint status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SERVER (server), FALSE);

  priv = server->priv;

  if (priv->rpc != NULL)
    return FALSE;

  if (priv->sonar == NULL)
    return FALSE;

  if (priv->host == NULL)
    return FALSE;

  if (timeout < HYSCAN_SONAR_SERVER_MIN_TIMEOUT)
    timeout = HYSCAN_SONAR_SERVER_MIN_TIMEOUT;
  if (timeout > HYSCAN_SONAR_SERVER_MAX_TIMEOUT)
    timeout = HYSCAN_SONAR_SERVER_MAX_TIMEOUT;

  /* Сокет отправки сообщений. */
  address = g_inet_socket_address_new_from_string (priv->host, HYSCAN_SONAR_RPC_UDP_PORT);
  if (address != NULL)
    {
      socket = g_socket_new (g_socket_address_get_family (address),
                             G_SOCKET_TYPE_DATAGRAM,
                             G_SOCKET_PROTOCOL_UDP,
                             NULL);
      g_clear_object (&address);
    }
  else
    {
      return FALSE;
    }

  uri = g_strdup_printf ("udp://%s:%d", priv->host, HYSCAN_SONAR_RPC_UDP_PORT);
  priv->rpc = urpc_server_create (uri, 1, 32, timeout,
                                  URPC_DEFAULT_DATA_SIZE,
                                  URPC_DEFAULT_DATA_TIMEOUT);
  g_free (uri);

  if (priv->rpc == NULL)
    goto fail;

  status = urpc_server_add_disconnect_proc (priv->rpc,
                                            hyscan_sonar_server_rpc_disconnect, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (priv->rpc, HYSCAN_SONAR_RPC_PROC_VERSION,
                                 hyscan_sonar_server_rpc_proc_version, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (priv->rpc, HYSCAN_SONAR_RPC_PROC_GET_SCHEMA,
                                 hyscan_sonar_server_rpc_proc_get_schema, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (priv->rpc, HYSCAN_SONAR_RPC_PROC_SET_MASTER,
                                 hyscan_sonar_server_rpc_proc_set_master, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (priv->rpc, HYSCAN_SONAR_RPC_PROC_SET,
                                 hyscan_sonar_server_rpc_proc_set, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (priv->rpc, HYSCAN_SONAR_RPC_PROC_GET,
                                 hyscan_sonar_server_rpc_proc_get, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_bind (priv->rpc);
  if (status != 0)
    goto fail;

  priv->socket = socket;

  /* Приёмник сообщений от гидролокатора. Эта функция вызывается при поступлении
   * данных от гидролокатора, разбивает их на пакеты и отправляет клиенту. */
  g_signal_connect_swapped (priv->sonar, "data", G_CALLBACK (hyscan_sonar_server_sender), priv);

  return TRUE;

fail:
  g_clear_object (&socket);
  g_clear_pointer (&priv->rpc, urpc_server_destroy);

  return FALSE;
}
