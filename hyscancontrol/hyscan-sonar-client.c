/*
 * \file hyscan-sonar-client.c
 *
 * \brief Исходный файл клиента управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-messages.h"
#include "hyscan-sonar-client.h"
#include "hyscan-sonar-rpc.h"

#include <hyscan-slice-pool.h>
#include <urpc-client.h>

#include <gio/gio.h>
#include <string.h>
#include <zlib.h>

#define hyscan_sonar_client_lock_error()       do { \
                                                 g_warning ("HyScanSonarClient: can't lock '%s'", \
                                                            __FUNCTION__); \
                                                 goto exit; \
                                               } while (0)

#define hyscan_sonar_client_set_error(p)       do { \
                                                 g_warning ("HyScanSonarClient: can't set '%s->%s' value", \
                                                            __FUNCTION__, p); \
                                                 goto exit; \
                                               } while (0)

#define hyscan_sonar_client_get_error(p)       do { \
                                                 g_warning ("HyScanSonarClient: can't get '%s->%s' value", \
                                                            __FUNCTION__, p); \
                                                 goto exit; \
                                               } while (0)

#define hyscan_sonar_client_exec_error(s)     do { \
                                                 if (s == URPC_STATUS_TIMEOUT) \
                                                   g_warning ("HyScanSonarClient: %s: execute timeout", \
                                                              __FUNCTION__); \
                                                 else \
                                                   g_warning ("HyScanSonarClient: %s: can't execute", \
                                                              __FUNCTION__); \
                                                 goto exit; \
                                               } while (0)

enum
{
  PROP_O,
  PROP_HOST,
  PROP_PORT,
  PROP_TIMEOUT,
  PROP_N_EXEC,
  PROP_MASTER,
  PROP_N_BUFFERS
};

enum
{
  SIGNAL_DATA,
  SIGNAL_LAST
};

typedef struct
{
  guint32              id;                     /* Идентификатор источника данных. */
  gint64               time;                   /* Метка времени данных. */
  guint32              type;                   /* Тип данных. */
  gfloat               rate;                   /* Частота дискретизации данных, Гц. */
  guint32              size;                   /* Целевой размер. */
  guint32              cur_size;               /* Текущий размер данных. */
  gchar               *buffer;                 /* Буфер для данных. */
  guint32              buffer_size;            /* Размер буфера для данных. */
  GTimer              *timer;                  /* Таймер. */
} HyScanSonarClientBuffer;

struct _HyScanSonarClientPrivate
{
  gchar               *host;                   /* Адрес гидролокатора. */
  guint16              port;                   /* Порт гидролокатора. */
  gdouble              timeout;                /* Таймаут RPC соединения. */
  guint                n_exec;                 /* Число попыток выполнения RPC запроса. */
  gboolean             master;                 /* Признак "главного" подключения. */

  uRpcClient          *rpc;                    /* RPC клиент. */
  HyScanDataSchema    *schema;                 /* Схема данных гидролокатора. */
  const gchar         *self_address;           /* Локальный адрес RPC клиента. */

  gchar               *receiver_host;          /* Адрес на котором запущен приёмник сообщений от гидролокатора. */
  guint16              receiver_port;          /* Номер UDP порта на котором запущен приёмник сообщений от гидролокатора. */

  GThread             *receiver;               /* Поток приёма сообщений по UDP. */
  GThread             *emitter;                /* Поток доставки сообщений гидролокатора. */
  gint                 started;                /* Признак запуска потоков. */
  gint                 shutdown;               /* Признак необходимости завершения работы. */

  guint                n_buffers;              /* Число буферов данных. */
  HyScanSlicePool     *buffers;                /* Буферы данных. */
  GRWLock              b_lock;                 /* Блокировка доступа к куче буферов. */

  GMutex               queue_lock;             /* Блокировка очереди принятых пакетов. */
  GCond                queue_cond;             /* Семафор очереди принятых пакетов. */
  GQueue              *queue;                  /* Очередь принятых пакетов данных. */
};

static void    hyscan_sonar_client_interface_init              (HyScanParamInterface          *iface);
static void    hyscan_sonar_client_set_property                (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void    hyscan_sonar_client_object_constructed          (GObject                       *object);
static void    hyscan_sonar_client_object_finalize             (GObject                       *object);

static void    hyscan_sonar_client_free_buffer                 (gpointer                       data);

static guint32 hyscan_sonar_client_rpc_check_version           (uRpcClient                    *rpc);
static guint32 hyscan_sonar_client_rpc_get_schema              (uRpcClient                    *rpc,
                                                                gchar                        **schema_data,
                                                                gchar                        **schema_id);
static guint32 hyscan_sonar_client_rpc_set_master              (uRpcClient                    *rpc,
                                                                gchar                         *host,
                                                                guint16                        port);
static guint32 hyscan_sonar_client_rpc_set                     (HyScanSonarClientPrivate      *priv,
                                                                const gchar *const            *names,
                                                                GVariant                     **values);
static guint32 hyscan_sonar_client_rpc_get                     (HyScanSonarClientPrivate      *priv,
                                                                const gchar *const            *names,
                                                                GVariant                     **values);

static gpointer hyscan_sonar_client_receiver                   (gpointer                       data);
static gpointer hyscan_sonar_client_emitter                    (gpointer                       data);

static guint   hyscan_sonar_client_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanSonarClient, hyscan_sonar_client, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarClient)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_sonar_client_interface_init))

static void hyscan_sonar_client_class_init (HyScanSonarClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_client_set_property;

  object_class->constructed = hyscan_sonar_client_object_constructed;
  object_class->finalize = hyscan_sonar_client_object_finalize;

  g_object_class_install_property (object_class, PROP_HOST,
    g_param_spec_string ("host", "Host", "Sonar host", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PORT,
    g_param_spec_uint ("port", "Port", "Sonar port", 1024, 65535, 1024,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TIMEOUT,
    g_param_spec_double ("timeout", "Timeout", "RPC timeout",
                         HYSCAN_SONAR_CLIENT_MIN_TIMEOUT,
                         HYSCAN_SONAR_CLIENT_MAX_TIMEOUT,
                         HYSCAN_SONAR_CLIENT_DEFAULT_TIMEOUT,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_EXEC,
    g_param_spec_uint ("n-exec", "NExec", "Number of execution attempts",
                       HYSCAN_SONAR_CLIENT_MIN_EXEC,
                       HYSCAN_SONAR_CLIENT_MAX_EXEC,
                       HYSCAN_SONAR_CLIENT_DEFAULT_EXEC,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_MASTER,
    g_param_spec_boolean ("master", "Master", "Master connection",
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_BUFFERS,
    g_param_spec_uint ("n-buffers", "NBuffers", "Number of message buffers",
                       HYSCAN_SONAR_CLIENT_MIN_N_BUFFERS,
                       HYSCAN_SONAR_CLIENT_MAX_N_BUFFERS,
                       HYSCAN_SONAR_CLIENT_DEFAULT_N_BUFFERS,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_client_signals[SIGNAL_DATA] =
    g_signal_new ("data", HYSCAN_TYPE_SONAR_CLIENT, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
hyscan_sonar_client_init (HyScanSonarClient *sonar_client)
{
  sonar_client->priv = hyscan_sonar_client_get_instance_private (sonar_client);
}

static void
hyscan_sonar_client_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanSonarClient *sonar_client = HYSCAN_SONAR_CLIENT (object);
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  switch (prop_id)
    {
    case PROP_HOST:
      priv->host = g_value_dup_string (value);
      break;

    case PROP_PORT:
      priv->port = g_value_get_uint (value);
      break;

    case PROP_TIMEOUT:
      priv->timeout = g_value_get_double (value);
      break;

    case PROP_N_EXEC:
      priv->n_exec = g_value_get_uint (value);
      break;

    case PROP_MASTER:
      priv->master = g_value_get_boolean (value);
      break;

    case PROP_N_BUFFERS:
      priv->n_buffers = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_client_object_constructed (GObject *object)
{
  HyScanSonarClient *sonar_client = HYSCAN_SONAR_CLIENT (object);
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  gchar *schema_data = NULL;
  gchar *schema_id = NULL;

  guint32 rpc_status = URPC_STATUS_FAIL;
  guint i;

  G_OBJECT_CLASS (hyscan_sonar_client_parent_class)->constructed (object);

  /* Буферы данных. */
  g_rw_lock_init (&priv->b_lock);
  for (i = 0; i < priv->n_buffers; i++)
    hyscan_slice_pool_push (&priv->buffers, g_new0 (HyScanSonarRpcPacket, 1));

  /* Очередь пакетов. */
  priv->queue = g_queue_new ();

  /* Подключаемся к RPC серверу. Если с первого раза подключиться не удалось,
     можно повторить попытку. Всего priv->n_exec раз. */
  for (i = 0; i < priv->n_exec; i++)
    {
      gchar *uri;

      uri = g_strdup_printf ("udp://%s:%d", priv->host, priv->port);
      priv->rpc = urpc_client_create (uri, URPC_DEFAULT_DATA_SIZE, priv->timeout);
      g_free (uri);

      /* Если подключение не удалось, попробуем еще. */
      if (priv->rpc == NULL || urpc_client_connect (priv->rpc) != 0)
        {
          g_clear_pointer (&priv->rpc, urpc_client_destroy);
          continue;
        }

      break;
    }

  if (priv->rpc == NULL)
    {
      g_warning ("HyScanSonarClient: can't connect to sonar '%s:%d'", priv->host, priv->port);
      return;
    }

  /* Проверяем версию сервера. */
  for (i = 0; i < priv->n_exec; i++)
    {
      rpc_status = hyscan_sonar_client_rpc_check_version (priv->rpc);
      if (rpc_status == URPC_STATUS_OK || rpc_status != URPC_STATUS_TIMEOUT)
        break;
    }

  if (rpc_status != URPC_STATUS_OK)
    goto exit;

  /* Загружаем схему данных гидролокатора. */
  for (i = 0; i < priv->n_exec; i++)
    {
      rpc_status = hyscan_sonar_client_rpc_get_schema (priv->rpc, &schema_data, &schema_id);
      if (rpc_status == URPC_STATUS_OK || rpc_status != URPC_STATUS_TIMEOUT)
        break;
    }

  if (rpc_status != URPC_STATUS_OK)
    goto exit;

  priv->schema = hyscan_data_schema_new_from_string (schema_data, schema_id);

  g_free (schema_data);
  g_free (schema_id);

  priv->self_address = urpc_client_get_self_address (priv->rpc);

  /* Потоки приёма и обработки сообщений от гидролокатора.
     Для приёма данных от гидролокатора используется поток hyscan_sonar_client_receiver.
     В нём создаётся принимающий UDP сокет связанный с IP адресом клиента и случайно выбранным
     UDP портом. Этот адрес и порт автоматически передаются на сервер при вызове функции lock.
     Задача потока приёма - макимально быстро принять данные и сохранить их в очереди на обработку.
     Обработка принятых данных (объединение) происходит в потоке hyscan_sonar_client_emitter.
     Также, этот поток используется для отправки сигналов с принятыми сообщениями. */
  priv->receiver = g_thread_new ("sonar-client-receiver", hyscan_sonar_client_receiver, priv);
  priv->emitter = g_thread_new ("sonar-client-emitter", hyscan_sonar_client_emitter, sonar_client);
  while (g_atomic_int_get (&priv->started) != 2)
    g_usleep (1000);

  return;

exit:
  g_clear_pointer (&priv->rpc, urpc_client_destroy);
}

static void
hyscan_sonar_client_object_finalize (GObject *object)
{
  HyScanSonarClient *sonar_client = HYSCAN_SONAR_CLIENT (object);
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  gpointer buffer;

  g_atomic_int_set (&priv->shutdown, 1);
  g_clear_pointer (&priv->receiver, g_thread_join);
  g_clear_pointer (&priv->emitter, g_thread_join);

  g_clear_pointer (&priv->rpc, urpc_client_destroy);

  g_clear_object (&priv->schema);
  g_free (priv->receiver_host);
  g_free (priv->host);

  g_rw_lock_clear (&priv->b_lock);
  g_queue_free_full (priv->queue, g_free);
  while ((buffer = hyscan_slice_pool_pop (&priv->buffers)) != NULL)
    g_free (buffer);

  G_OBJECT_CLASS (hyscan_sonar_client_parent_class)->finalize (object);
}

/* Функция освобождает память занятую структурой HyScanSonarClientBuffer. */
static void
hyscan_sonar_client_free_buffer (gpointer data)
{
  HyScanSonarClientBuffer *sdata = data;

  g_timer_destroy (sdata->timer);
  g_free (sdata->buffer);

  g_free (sdata);
}

/* Функция проверяет версию сервера. */
static guint32
hyscan_sonar_client_rpc_check_version (uRpcClient *rpc)
{
  uRpcData *data;
  guint32 rpc_status = URPC_STATUS_FAIL;

  guint32 version;
  guint32 magic;

  data = urpc_client_lock (rpc);
  if (data == NULL)
    hyscan_sonar_client_lock_error ();

  rpc_status = urpc_client_exec (rpc, HYSCAN_SONAR_RPC_PROC_VERSION);
  if (rpc_status != URPC_STATUS_OK)
    hyscan_sonar_client_exec_error (rpc_status);

  rpc_status = URPC_STATUS_FAIL;

  if (urpc_data_get_uint32 (data, HYSCAN_SONAR_RPC_PARAM_VERSION, &version) != 0 )
    hyscan_sonar_client_get_error ("version");
  if (urpc_data_get_uint32 (data, HYSCAN_SONAR_RPC_PARAM_MAGIC, &magic) != 0 )
    hyscan_sonar_client_get_error ("magic");

  if (version != HYSCAN_SONAR_RPC_VERSION || magic != HYSCAN_SONAR_RPC_MAGIC)
    {
      g_warning ("HyScanSonarClient: server version mismatch");
      goto exit;
    }

  rpc_status = URPC_STATUS_OK;

exit:
  urpc_client_unlock (rpc);

  return rpc_status;
}

/* Функция считывает схему данных гидролокатора. */
static guint32
hyscan_sonar_client_rpc_get_schema (uRpcClient  *rpc,
                                    gchar      **schema_data,
                                    gchar      **schema_id)
{
  uRpcData *data;
  guint32 rpc_status = URPC_STATUS_FAIL;
  guint32 exec_status;

  const gchar *rpc_schema_data;
  guint32 rpc_schema_size;

  gchar *dec_schema_data;
  guint32 schema_size;

  GConverterResult converter_result;
  GZlibDecompressor *decompressor;
  gsize readed, writed;

  data = urpc_client_lock (rpc);
  if (data == NULL)
    hyscan_sonar_client_lock_error ();

  rpc_status = urpc_client_exec (rpc, HYSCAN_SONAR_RPC_PROC_GET_SCHEMA);
  if (rpc_status != URPC_STATUS_OK)
    hyscan_sonar_client_exec_error (rpc_status);

  rpc_status = URPC_STATUS_FAIL;

  if (urpc_data_get_uint32 (data, HYSCAN_SONAR_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_sonar_client_get_error ("exec_status");
  if (exec_status != HYSCAN_SONAR_RPC_STATUS_OK)
    goto exit;

  rpc_schema_data = urpc_data_get (data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_DATA, &rpc_schema_size);
  if (rpc_schema_data == NULL)
    hyscan_sonar_client_get_error ("schema_data");

  if (urpc_data_get_uint32 (data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_SIZE, &schema_size) != 0)
    hyscan_sonar_client_get_error ("schema_size");

  if (urpc_data_get_string (data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_ID, 0) == NULL)
    hyscan_sonar_client_get_error ("schema_id");

  dec_schema_data = g_malloc0 (schema_size + 1);
  decompressor = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_ZLIB);
  converter_result = g_converter_convert (G_CONVERTER (decompressor),
                                          rpc_schema_data, rpc_schema_size,
                                          dec_schema_data, schema_size,
                                          G_CONVERTER_INPUT_AT_END,
                                          &readed, &writed, NULL);
  g_object_unref (decompressor);

  if (converter_result != G_CONVERTER_FINISHED)
    {
      g_free (dec_schema_data);
      goto exit;
    }

  *schema_data = dec_schema_data;
  *schema_id = g_strdup (urpc_data_get_string (data, HYSCAN_SONAR_RPC_PARAM_SCHEMA_ID, 0));

  rpc_status = URPC_STATUS_OK;

exit:
  urpc_client_unlock (rpc);

  return rpc_status;
}

/* Функция устанавливает "главное" подключение к гидролокатору. */
static guint32
hyscan_sonar_client_rpc_set_master (uRpcClient *rpc,
                                    gchar      *host,
                                    guint16     port)
{
  uRpcData *urpc_data;
  guint32 rpc_status = URPC_STATUS_FAIL;
  guint32 exec_status;

  urpc_data = urpc_client_lock (rpc);
  if (urpc_data == NULL)
    hyscan_sonar_client_lock_error ();

  if (urpc_data_set_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_MASTER_HOST, host) != 0)
    hyscan_sonar_client_set_error ("host");

  if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_MASTER_PORT, port) != 0)
    hyscan_sonar_client_set_error ("port");

  rpc_status = urpc_client_exec (rpc, HYSCAN_SONAR_RPC_PROC_SET_MASTER);
  if (rpc_status != URPC_STATUS_OK)
    hyscan_sonar_client_exec_error (rpc_status);

  rpc_status = URPC_STATUS_FAIL;

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_sonar_client_get_error ("exec_status");
  if (exec_status != HYSCAN_SONAR_RPC_STATUS_OK)
    goto exit;

  rpc_status = URPC_STATUS_OK;

exit:
  urpc_client_unlock (rpc);

  return rpc_status;
}

/* Функция устанавливает значение параметра гидролокатора. */
static guint32
hyscan_sonar_client_rpc_set (HyScanSonarClientPrivate  *priv,
                             const gchar *const        *names,
                             GVariant                 **values)
{
  uRpcData *urpc_data;
  guint32 rpc_status = URPC_STATUS_FAIL;
  guint32 exec_status;

  gint i;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_sonar_client_lock_error ();

  for (i = 0; names[i] != NULL; i++)
    {
      GVariantClass value_type;

      if (i == HYSCAN_SONAR_RPC_MAX_PARAMS - 1)
        hyscan_sonar_client_set_error ("n_params");

      if (urpc_data_set_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_NAME0 + i, names[i]) != 0)
        hyscan_sonar_client_set_error ("name");

      if (values[i] == NULL)
        {
          if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_NULL) != 0)
            hyscan_sonar_client_set_error ("type");

          continue;
        }

      value_type = g_variant_classify (values[i]);
      switch (value_type)
        {
        case G_VARIANT_CLASS_BOOLEAN:
          {
            gboolean value = g_variant_get_boolean (values[i]);

            if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_BOOLEAN) != 0)
              hyscan_sonar_client_set_error ("type");

            if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value ? 1 : 0) != 0)
              hyscan_sonar_client_set_error ("value");
          }
          break;

        case G_VARIANT_CLASS_INT64:
          {
            gint64 value = g_variant_get_int64 (values[i]);

            if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_INT64) != 0)
              hyscan_sonar_client_set_error ("type");

            if (urpc_data_set_int64 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value) != 0)
              hyscan_sonar_client_set_error ("value");
          }
          break;

        case G_VARIANT_CLASS_DOUBLE:
          {
            gdouble value = g_variant_get_double (values[i]);

            if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_DOUBLE) != 0)
              hyscan_sonar_client_set_error ("type");

            if (urpc_data_set_double (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value) != 0)
              hyscan_sonar_client_set_error ("value");
          }
          break;

        case G_VARIANT_CLASS_STRING:
          {
            const gchar *value = g_variant_get_string (values[i], NULL);

            if (urpc_data_set_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, HYSCAN_SONAR_RPC_TYPE_STRING) != 0)
              hyscan_sonar_client_set_error ("type");

            if (urpc_data_set_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, value) != 0)
              hyscan_sonar_client_set_error ("value");
          }
          break;

        default:
          break;
        }
    }

  rpc_status = urpc_client_exec (priv->rpc, HYSCAN_SONAR_RPC_PROC_SET);
  if (rpc_status != URPC_STATUS_OK)
    hyscan_sonar_client_exec_error (rpc_status);

  rpc_status = URPC_STATUS_FAIL;

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_sonar_client_get_error ("exec_status");
  if (exec_status != HYSCAN_SONAR_RPC_STATUS_OK)
    goto exit;

  rpc_status = URPC_STATUS_OK;

exit:
  urpc_client_unlock (priv->rpc);

  return rpc_status;
}

/* Функция считывает значение параметра гидролокатора. */
static guint32
hyscan_sonar_client_rpc_get (HyScanSonarClientPrivate  *priv,
                             const gchar *const        *names,
                             GVariant                 **values)
{
  uRpcData *urpc_data;
  guint32 rpc_status = URPC_STATUS_FAIL;
  guint32 exec_status;

  gint i;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_sonar_client_lock_error ();

  for (i = 0; names[i] != NULL; i++)
    {
      if (i == HYSCAN_SONAR_RPC_MAX_PARAMS - 1)
        hyscan_sonar_client_set_error ("n_params");

      if (urpc_data_set_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_NAME0 + i, names[i]) != 0)
        hyscan_sonar_client_set_error ("name");
    }

  rpc_status = urpc_client_exec (priv->rpc, HYSCAN_SONAR_RPC_PROC_GET);
  if (rpc_status != URPC_STATUS_OK)
    hyscan_sonar_client_exec_error (rpc_status);

  rpc_status = URPC_STATUS_FAIL;

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_sonar_client_get_error ("exec_status");
  if (exec_status != HYSCAN_SONAR_RPC_STATUS_OK)
    goto exit;

  for (i = 0; names[i] != NULL; i++)
    {
      guint32 param_type;

      if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, &param_type) != 0)
        hyscan_sonar_client_get_error ("type");

      switch (param_type)
        {
        case HYSCAN_SONAR_RPC_TYPE_NULL:
          break;

        case HYSCAN_SONAR_RPC_TYPE_BOOLEAN:
          {
            guint32 param_value;
            if (urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_sonar_client_get_error ("value");
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_INT64:
          {
            gint64 param_value;
            if (urpc_data_get_int64 (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_sonar_client_get_error ("value");
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_DOUBLE:
          {
            gdouble param_value;
            if (urpc_data_get_double (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, &param_value) != 0)
              hyscan_sonar_client_get_error ("value");
          }
          break;

        case HYSCAN_SONAR_RPC_TYPE_STRING:
          {
            if (urpc_data_get_string (urpc_data, HYSCAN_SONAR_RPC_PARAM_VALUE0 + i, 0) == NULL)
              hyscan_sonar_client_get_error ("value");
          }
          break;

        default:
          goto exit;
        }
    }

  for (i = 0; names[i] != NULL; i++)
    {
      guint32 value_type;

      urpc_data_get_uint32 (urpc_data, HYSCAN_SONAR_RPC_PARAM_TYPE0 + i, &value_type);
      switch (value_type)
        {
        case HYSCAN_SONAR_RPC_TYPE_NULL:
          {
            values[i] = NULL;
          }
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

  rpc_status = URPC_STATUS_OK;

exit:
  urpc_client_unlock (priv->rpc);

  return rpc_status;
}

/* Поток приёма сообщений от гидролокатора. */
static gpointer
hyscan_sonar_client_receiver (gpointer data)
{
  HyScanSonarClientPrivate *priv = data;

  gchar receiver_host[1024];
  const gchar *uri;
  const gchar *end;

  GSocket *socket = NULL;
  GSocketAddress *address = NULL;

  HyScanSonarRpcPacket *packet = NULL;
  gssize received;

  /* Локальный IP адрес с которого подключились к гидролокатору. */
  uri = priv->self_address + 6;
  if (uri[0] == '[')
    {
      uri = uri + 1;
      end = strstr (uri, "]:");
    }
  else
    {
      end = strstr (uri, ":");
    }

  memset (receiver_host, 0, sizeof (receiver_host));
  memcpy (receiver_host, uri, end - uri);
  priv->receiver_host = g_strdup (receiver_host);

  /* Сокет приёма сообщений от гидролокатора. */
  do
    {
      GError *error;
      gint error_code;
      gboolean status;

      /* Выбираем случайный порт. */
      priv->receiver_port = g_random_int_range (HYSCAN_SONAR_RPC_MIN_PORT, HYSCAN_SONAR_RPC_MAX_PORT);

      /* Создаём и подключаем к нему сокет. */
      address = g_inet_socket_address_new_from_string (priv->receiver_host, priv->receiver_port);
      if (address == NULL)
        break;

      socket = g_socket_new (g_socket_address_get_family (address),
                             G_SOCKET_TYPE_DATAGRAM,
                             G_SOCKET_PROTOCOL_UDP,
                             NULL);
      if (socket == NULL)
        break;

      status = g_socket_bind (socket, address, FALSE, &error);
      g_clear_object (&address);

      /* Успешно подключились. */
      if (status)
        break;

      /* Потом создадим сокет еще раз. */
      g_clear_object (&socket);

      error_code = error->code;
      g_error_free (error);

      /* Порт уже используется, выберем другой. */
      if (error_code == G_IO_ERROR_ADDRESS_IN_USE)
        continue;

      /* Другая ошибка, печалька... */
      break;
    }
  while (TRUE);

  g_atomic_int_inc (&priv->started);

  /* Поток запустился с ошибкой */
  if (socket == NULL)
    {
      g_warning ("HyScanSonarClient: can't setup receiver thread");
      g_clear_pointer (&priv->receiver_host, g_free);
      priv->receiver_port = 0;
      return NULL;
    }

  /* Приём данных. */
  while (g_atomic_int_get (&priv->shutdown) != 1)
    {
      /* Проверка наличия входных данных. */
      if (!g_socket_condition_timed_wait (socket, G_IO_IN, 100000, NULL, NULL))
        continue;

      /* Память для пакета с данными. */
      if (packet == NULL)
        {
          g_rw_lock_writer_lock (&priv->b_lock);
          packet = hyscan_slice_pool_pop (&priv->buffers);
          g_rw_lock_writer_unlock (&priv->b_lock);
        }

      if (packet == NULL)
        {
          g_warning ("HyScanSonarClient: buffer overrun");

          continue;
        }

      /* Принимаем пакет с данными. */
      received = g_socket_receive_from (socket, NULL, (gpointer)packet, HYSCAN_SONAR_MSG_MAX_SIZE, NULL, NULL);
      if (received <= 0)
        continue;

      /* Проверяем пакет с данными. */
      if ((received <= offsetof (HyScanSonarRpcPacket, data)) ||
          (GUINT32_FROM_LE (packet->magic) != HYSCAN_SONAR_RPC_MAGIC) ||
          (GUINT32_FROM_LE (packet->version) != HYSCAN_SONAR_RPC_VERSION))
        {
          g_warning ("HyScanSonarClient: unsupported packet format");
          continue;
        }

      if ((GUINT32_FROM_LE (packet->part_size) + GUINT32_FROM_LE (packet->offset) > GUINT32_FROM_LE (packet->size)) ||
          (received - offsetof (HyScanSonarRpcPacket, data) != GUINT32_FROM_LE (packet->part_size)))
        {
          g_warning ("HyScanSonarClient: packet %d size mismatch", packet->index);
          continue;
        }

      /* Отправляем пакет в обработку. */
      g_mutex_lock (&priv->queue_lock);
      g_queue_push_tail (priv->queue, packet);
      g_cond_signal (&priv->queue_cond);
      g_mutex_unlock (&priv->queue_lock);

      packet = NULL;
    }

  g_clear_pointer (&packet, g_free);
  g_clear_object (&socket);

  return NULL;
}

/* Поток обработки принятых пакетов. */
static gpointer
hyscan_sonar_client_emitter (gpointer data)
{
  HyScanSonarClient *sonar_client = data;
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  GHashTable *buffers;
  guint32 next_index = 0;

  /* Буферы для данных. */
  buffers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                   NULL, hyscan_sonar_client_free_buffer);

  g_atomic_int_inc (&priv->started);

  /* Обработка данных. */
  while (g_atomic_int_get (&priv->shutdown) != 1)
    {
      HyScanSonarRpcPacket *packet = NULL;
      HyScanSonarClientBuffer *buffer;
      HyScanSonarMessage message;

      GHashTableIter iter;
      gpointer data;

      guint32 id;
      gint64 time;
      guint32 type;
      gfloat rate;
      guint32 size;
      guint32 offset;
      guint32 part_size;
      guint32 buffer_size;
      guint32 crc1, crc2;

      gint64 cond_time;
      guint queue_len;
      guint i;

      /* Отправим незавершённые сообщения, находящиеся в очереди дольше 1 секунды. */
      g_hash_table_iter_init (&iter, buffers);
      while (g_hash_table_iter_next (&iter, NULL, &data))
        {
          buffer = data;

          if ((buffer->cur_size == 0) || (g_timer_elapsed (buffer->timer, NULL) < 1.0))
            continue;

          message.time = buffer->time;
          message.id = buffer->id;
          message.type = buffer->type;
          message.rate = buffer->rate;
          message.size = buffer->size;
          message.data = buffer->buffer;

          g_signal_emit (sonar_client, hyscan_sonar_client_signals[SIGNAL_DATA], 0, &message);
          memset (buffer->buffer, 0, buffer->size);
          buffer->cur_size = 0;
          buffer->size = 0;
          buffer->type = 0;
          buffer->rate = 0.0;
        }

      /* Ждём пакеты в очереди. */
      g_mutex_lock (&priv->queue_lock);
      cond_time = g_get_monotonic_time () + 100 * G_TIME_SPAN_MILLISECOND;
      if (priv->queue->length == 0)
        if (!g_cond_wait_until (&priv->queue_cond, &priv->queue_lock, cond_time))
          {
            g_mutex_unlock (&priv->queue_lock);
            continue;
          }
      queue_len = priv->queue->length;
      g_mutex_unlock (&priv->queue_lock);

      /* Очередь пустая. */
      if (queue_len == 0)
        continue;

      /* Обрабатываем все пакеты в очереди. */
      while (queue_len > 0)
        {
          /* Ищем пакет с индексом next_index. */
          g_mutex_lock (&priv->queue_lock);
          for (i = 0; i < queue_len; i++)
            {
              packet = g_queue_peek_nth (priv->queue, i);
              if (GUINT32_FROM_LE (packet->index) == next_index)
                break;
            }
          g_mutex_unlock (&priv->queue_lock);

          /* Пакет с требуемым индексом не найден. */
          if (GUINT32_FROM_LE(packet->index) != next_index)
            {
              HyScanSonarRpcPacket *packet_lt = NULL;
              HyScanSonarRpcPacket *packet_gt = NULL;
              guint32 index_lt = G_MAXUINT32;
              guint32 index_gt = G_MAXUINT32;

              /* Очередь заполнена менее чем на четверть, подождём - может придёт. */
              if (queue_len < priv->n_buffers / 4)
                break;

              g_warning ("HyScanSonarClient: packet %d lost", packet->index);

              /* Нужно выбирать другой пакет на обработку.
               * Ищем пакет с минимальным индексом большим требуемого и
               * минимальным индексом меньшим требуемого. */
              g_mutex_lock (&priv->queue_lock);
              for (i = 0; i < queue_len; i++)
                {
                  guint32 cur_index;

                  packet = g_queue_peek_nth (priv->queue, i);
                  cur_index = GUINT32_FROM_LE (packet->index);

                  if (cur_index < next_index && cur_index < index_lt)
                    {
                      index_lt = cur_index;
                      packet_lt = packet;
                    }

                  if (cur_index > next_index && cur_index < index_gt)
                    {
                      index_gt = cur_index;
                      packet_gt = packet;
                    }
                }
              g_mutex_unlock (&priv->queue_lock);

              packet = (packet_gt != NULL) ? packet_gt : packet_lt;
            }

          /* Параметры данных из пакета. */
          id = GUINT32_FROM_LE (packet->id);
          time = GINT64_FROM_LE (packet->time);
          type = GUINT32_FROM_LE (packet->type);
          rate = hyscan_sonar_rpc_float_from_le (packet->rate);
          size = GUINT32_FROM_LE (packet->size);
          part_size = GUINT32_FROM_LE (packet->part_size);
          offset = GUINT32_FROM_LE (packet->offset);

          /* Проверяем контрольную сумму. */
          crc1 = packet->crc32;
          packet->crc32 = 0;

          crc2 = crc32 (0L, Z_NULL, 0);
          crc2 = crc32 (crc2, (gpointer)packet, part_size + offsetof (HyScanSonarRpcPacket, data));
          if (crc1 != crc2)
            {
              g_warning ("HyScanSonarClient: packet %d crc mismatch", packet->index);
              continue;
            }

          /* Буфер для данных. */
          buffer = g_hash_table_lookup (buffers, GINT_TO_POINTER (id));
          if (buffer == NULL)
            {
              buffer = g_new0 (HyScanSonarClientBuffer, 1);
              buffer->id = id;
              buffer->timer = g_timer_new ();
              g_hash_table_insert (buffers, GINT_TO_POINTER (id), buffer);
            }

          /* Корректируем размер буфера для данных. */
          if ((buffer->size == 0) && (size > buffer->buffer_size))
            {
              buffer_size = size / 65536;
              buffer_size += (size % 65536) ? 1 : 0;
              buffer_size *= 65536;

              g_free (buffer->buffer);
              buffer->buffer = g_malloc0 (buffer_size);
              buffer->buffer_size = buffer_size;
            }

          /* Обрабатываем только актуальные пакеты. */
          if ((size <= buffer->buffer_size) &&
              (buffer->size == 0 || buffer->size == size) &&
              (buffer->type == 0 || buffer->type == type) &&
              (buffer->rate == 0.0 || buffer->rate == rate) &&
              (time >= buffer->time))
            {
              /* Изменилось время, отправляем неполный пакет. */
              if ((buffer->cur_size > 0) && (buffer->time != time))
                {
                  message.time = buffer->time;
                  message.id = buffer->id;
                  message.type = buffer->type;
                  message.rate = buffer->rate;
                  message.size = buffer->size;
                  message.data = buffer->buffer;

                  g_signal_emit (sonar_client, hyscan_sonar_client_signals[SIGNAL_DATA], 0, &message);
                  memset (buffer->buffer, 0, buffer->size);
                  buffer->cur_size = 0;
                  buffer->size = 0;
                  buffer->type = 0;
                  buffer->rate = 0.0;
                }

              /* Сохраняем данные в буфере. */
              buffer->time = time;
              buffer->type = type;
              buffer->rate = rate;
              buffer->size = size;
              buffer->cur_size += part_size;
              memcpy (buffer->buffer + offset, packet->data, part_size);
              g_timer_start (buffer->timer);

              /* Собрали все данные. */
              if (buffer->cur_size == size)
                {
                  message.time = buffer->time;
                  message.id = buffer->id;
                  message.type = buffer->type;
                  message.rate = buffer->rate;
                  message.size = buffer->size;
                  message.data = buffer->buffer;

                  g_signal_emit (sonar_client, hyscan_sonar_client_signals[SIGNAL_DATA], 0, &message);
                  memset (buffer->buffer, 0, buffer->size);
                  buffer->cur_size = 0;
                  buffer->size = 0;
                  buffer->type = 0;
                  buffer->rate = 0.0;
                }

              /* Следующий индекс пакета. */
              next_index = GUINT32_FROM_LE (packet->index);
              if (next_index == G_MAXUINT32)
                next_index = 0;
              else
                next_index += 1;
            }
          else
            {
              g_warning ("HyScanSonarClient: corrupted packet");
            }

          /* Убираем пакет из очереди. */
          g_mutex_lock (&priv->queue_lock);
          g_queue_remove (priv->queue, packet);
          g_mutex_unlock (&priv->queue_lock);

          /* Освобождаем буфер. */
          g_rw_lock_writer_lock (&priv->b_lock);
          hyscan_slice_pool_push (&priv->buffers, packet);
          g_rw_lock_writer_unlock (&priv->b_lock);

          queue_len -= 1;
        }
    }

  g_hash_table_unref (buffers);

  return NULL;
}

/* Функция создаёт новый объект HyScanSonarClient. */
HyScanSonarClient *
hyscan_sonar_client_new (const gchar *host,
                         guint16      port)
{
  return hyscan_sonar_client_new_full (host,
                                       port,
                                       HYSCAN_SONAR_CLIENT_DEFAULT_TIMEOUT,
                                       HYSCAN_SONAR_CLIENT_DEFAULT_EXEC,
                                       HYSCAN_SONAR_CLIENT_DEFAULT_N_BUFFERS);
}

/* Функция создаёт новый объект HyScanSonarClient. */
HyScanSonarClient *
hyscan_sonar_client_new_full (const gchar *host,
                              guint16      port,
                              gdouble      timeout,
                              guint        n_exec,
                              guint        n_buffers)
{
  if (timeout < HYSCAN_SONAR_CLIENT_MIN_TIMEOUT)
    timeout = HYSCAN_SONAR_CLIENT_MIN_TIMEOUT;

  if (timeout > HYSCAN_SONAR_CLIENT_MAX_TIMEOUT)
    timeout = HYSCAN_SONAR_CLIENT_MAX_TIMEOUT;

  return g_object_new (HYSCAN_TYPE_SONAR_CLIENT,
                       "host", host,
                       "port", port,
                       "timeout", timeout,
                       "n-exec", n_exec,
                       "n-buffers", n_buffers,
                       NULL);
}

/* Функция переводит подключение к гидролокатору в активный режим. */
gboolean
hyscan_sonar_client_set_master (HyScanSonarClient *client)
{
  HyScanSonarClientPrivate *priv;

  guint32 rpc_status = URPC_STATUS_FAIL;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CLIENT (client), FALSE);

  priv = client->priv;

  if (priv->rpc == NULL)
    return FALSE;

  for (i = 0; i < priv->n_exec; i++)
    {
      rpc_status = hyscan_sonar_client_rpc_set_master (priv->rpc, priv->receiver_host, priv->receiver_port);
      if (rpc_status == URPC_STATUS_OK || rpc_status != URPC_STATUS_TIMEOUT)
        break;
    }

  if (rpc_status == URPC_STATUS_OK)
    return TRUE;

  return FALSE;
}

/* Функция возвращает схему данных гидролокатора. */
static HyScanDataSchema *
hyscan_sonar_client_schema (HyScanParam *sonar)
{
  HyScanSonarClient *sonar_client = HYSCAN_SONAR_CLIENT (sonar);
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  if (priv->rpc == NULL)
    return NULL;

  return g_object_ref (priv->schema);
}

/* Функция устанавливает значение параметра гидролокатора. */
static gboolean
hyscan_sonar_client_set (HyScanParam         *sonar,
                         const gchar *const  *names,
                         GVariant           **values)
{
  HyScanSonarClient *sonar_client = HYSCAN_SONAR_CLIENT (sonar);
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  guint32 rpc_status = URPC_STATUS_FAIL;
  guint i;

  if (priv->rpc == NULL)
    return FALSE;

  for (i = 0; i < priv->n_exec; i++)
    {
      rpc_status = hyscan_sonar_client_rpc_set (priv, names, values);
      if (rpc_status == URPC_STATUS_OK || rpc_status != URPC_STATUS_TIMEOUT)
        break;
    }

  if (rpc_status == URPC_STATUS_OK)
    {
      for (i = 0; names[i] != NULL; i++)
        g_clear_pointer (&values[i], g_variant_unref);

      return TRUE;
    }

  return FALSE;
}

/* Функция считывает значение параметра гидролокатора. */
static gboolean
hyscan_sonar_client_get (HyScanParam         *sonar,
                         const gchar *const  *names,
                         GVariant           **values)
{
  HyScanSonarClient *sonar_client = HYSCAN_SONAR_CLIENT (sonar);
  HyScanSonarClientPrivate *priv = sonar_client->priv;

  guint32 rpc_status = URPC_STATUS_FAIL;
  guint i;

  if (priv->rpc == NULL)
    return FALSE;

  for (i = 0; i < priv->n_exec; i++)
    {
      rpc_status = hyscan_sonar_client_rpc_get (priv, names, values);
      if (rpc_status == URPC_STATUS_OK || rpc_status != URPC_STATUS_TIMEOUT)
        break;
    }

  if (rpc_status == URPC_STATUS_OK)
    return TRUE;

  return FALSE;
}

static void
hyscan_sonar_client_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_sonar_client_schema;
  iface->set = hyscan_sonar_client_set;
  iface->get = hyscan_sonar_client_get;
}
