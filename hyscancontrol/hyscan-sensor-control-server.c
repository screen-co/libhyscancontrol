/*
 * \file hyscan-sensor-control-server.c
 *
 * \brief Исходный файл класса сервера управления датчиками
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sensor-control-server.h"
#include "hyscan-sensor-control.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"
#include "hyscan-sonar-box.h"

enum
{
  PROP_0,
  PROP_PARAMS
};

enum
{
  SIGNAL_SENSOR_UART_PORT_PARAM,
  SIGNAL_SENSOR_UDP_IP_PORT_PARAM,
  SIGNAL_SENSOR_SET_ENABLE,
  SIGNAL_LAST
};

typedef struct _HyScanSensorControlServerPort HyScanSensorControlServerPort;

/* Прототип функции выполнения операции. */
typedef gboolean (*hyscan_sensor_control_server_operation)               (HyScanSensorControlServer     *server,
                                                                          HyScanSensorControlServerPort *port,
                                                                          const gchar *const            *paths,
                                                                          GVariant                     **values);

/* Операция над портом. */
struct _HyScanSensorControlServerPort
{
  gchar                                 *name;                           /* Название порта. */
  hyscan_sensor_control_server_operation func;                           /* Функция выполнения операции. */
};

struct _HyScanSensorControlServerPrivate
{
  HyScanDataBox                         *params;                         /* Параметры гидролокатора. */
  gulong                                 signal_id;                      /* Идентификатор обработчика сигнала set. */

  GHashTable                            *operations;                     /* Таблица возможных запросов. */
  GHashTable                            *paths;                          /* Таблица названий параметров запросов. */
  GHashTable                            *ids;                            /* Идентификаторы портов. */
};

static void        hyscan_sensor_control_server_set_property             (GObject                       *object,
                                                                          guint                          prop_id,
                                                                          const GValue                  *value,
                                                                          GParamSpec                    *pspec);
static void        hyscan_sensor_control_server_object_constructed       (GObject                       *object);
static void        hyscan_sensor_control_server_object_finalize          (GObject                       *object);

static void        hyscan_sensor_control_server_free_port                (gpointer                       data);

static gboolean    hyscan_sensor_control_server_set_cb                   (HyScanDataBox                 *params,
                                                                          const gchar *const            *names,
                                                                          GVariant                     **values,
                                                                          HyScanSensorControlServer     *server);

static gboolean    hyscan_sensor_control_server_uart_port_param          (HyScanSensorControlServer     *server,
                                                                          HyScanSensorControlServerPort *port,
                                                                          const gchar *const            *names,
                                                                          GVariant                     **values);
static gboolean    hyscan_sensor_control_server_udp_ip_port_param        (HyScanSensorControlServer     *server,
                                                                          HyScanSensorControlServerPort *port,
                                                                          const gchar *const            *names,
                                                                          GVariant                     **values);
static gboolean    hyscan_sensor_control_server_set_enable               (HyScanSensorControlServer     *server,
                                                                          HyScanSensorControlServerPort *port,
                                                                          const gchar *const            *names,
                                                                          GVariant                     **values);

static guint       hyscan_sensor_control_server_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSensorControlServer, hyscan_sensor_control_server, G_TYPE_OBJECT)

static void
hyscan_sensor_control_server_class_init (HyScanSensorControlServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sensor_control_server_set_property;

  object_class->constructed = hyscan_sensor_control_server_object_constructed;
  object_class->finalize = hyscan_sensor_control_server_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_object ("params", "SonarParams", "Sonar parameters via HyScanSonarBox", HYSCAN_TYPE_SONAR_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sensor_control_server_signals[SIGNAL_SENSOR_UART_PORT_PARAM] =
    g_signal_new ("sensor-uart-port-param", HYSCAN_TYPE_SENSOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__STRING_INT_UINT_UINT,
                  G_TYPE_BOOLEAN,
                  4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_UINT, G_TYPE_INT64);

  hyscan_sensor_control_server_signals[SIGNAL_SENSOR_UDP_IP_PORT_PARAM] =
    g_signal_new ("sensor-udp-ip-port-param", HYSCAN_TYPE_SENSOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__STRING_INT_UINT_UINT,
                  G_TYPE_BOOLEAN,
                  4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_UINT, G_TYPE_UINT);

  hyscan_sensor_control_server_signals[SIGNAL_SENSOR_SET_ENABLE] =
    g_signal_new ("sensor-set-enable", HYSCAN_TYPE_SENSOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__STRING_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}

static void
hyscan_sensor_control_server_init (HyScanSensorControlServer *server)
{
  server->priv = hyscan_sensor_control_server_get_instance_private (server);
}

static void
hyscan_sensor_control_server_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  HyScanSensorControlServer *server = HYSCAN_SENSOR_CONTROL_SERVER (object);
  HyScanSensorControlServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_PARAMS:
      priv->params = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sensor_control_server_object_constructed (GObject *object)
{
  HyScanSensorControlServer *server = HYSCAN_SENSOR_CONTROL_SERVER (object);
  HyScanSensorControlServerPrivate *priv = server->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sensors;

  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_sensor_control_server_parent_class)->constructed (object);

  priv->operations = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, hyscan_sensor_control_server_free_port);
  priv->paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  priv->ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Обязательно должны быть переданы параметры гидролокатора. */
  if (priv->params == NULL)
    {
      g_warning ("HyScanControlServer: empty parameters");
      return;
    }

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (priv->params, "/schema/id", &id))
    {
      g_clear_object (&priv->params);
      g_warning ("HyScanControlServer: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->params);
      g_warning ("HyScanControlServer: sonar schema id mismatch");
      return;
    }
  if (!hyscan_data_box_get_integer (priv->params, "/schema/version", &version))
    {
      g_clear_object (&priv->params);
      g_warning ("HyScanControlServer: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->params);
      g_warning ("HyScanControlServer: sonar schema version mismatch");
      return;
    }

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (priv->params));

  /* Ветка схемы с описанием портов - "/sensors". */
  for (i = 0, sensors = NULL; i < params->n_nodes; i++)
    {
      if (g_strcmp0 (params->nodes[i]->path, "/sensors") == 0)
        {
          sensors = params->nodes[i];
          break;
        }
    }

  if (sensors != NULL)
    {
      /* Считываем описания портов. */
      for (i = 0; i < sensors->n_nodes; i++)
        {
          HyScanSensorControlServerPort *operation;
          gchar *operation_path;

          gchar *param_names[3];
          GVariant *param_values[3];

          gint64 id;
          gint64 type;

          gchar **pathv;
          gchar *name;

          gboolean status;

          param_names[0] = g_strdup_printf ("%s/id", sensors->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/type", sensors->nodes[i]->path);
          param_names[2] = NULL;

          status = hyscan_data_box_get (HYSCAN_DATA_BOX (priv->params), (const gchar **)param_names, param_values);

          if (status)
            {
              id = g_variant_get_int64 (param_values[0]);
              type = g_variant_get_int64 (param_values[1]);

              g_variant_unref (param_values[0]);
              g_variant_unref (param_values[1]);
            }

          g_free (param_names[0]);
          g_free (param_names[1]);

          if (!status)
            continue;

          if (id <= 0 || id > G_MAXINT32)
            continue;

          if (type != HYSCAN_SENSOR_PORT_VIRTUAL &&
              type != HYSCAN_SENSOR_PORT_UART &&
              type != HYSCAN_SENSOR_PORT_UDP_IP)
            {
              continue;
            }

          pathv = g_strsplit (sensors->nodes[i]->path, "/", -1);
          name = pathv[2];

          /* Команда - hyscan_sensor_control_set_uart_port_param. */
          if (type == HYSCAN_SENSOR_PORT_UART)
            {
              operation = g_new0 (HyScanSensorControlServerPort, 1);
              operation->name = g_strdup (name);
              operation->func = hyscan_sensor_control_server_uart_port_param;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sensors/%s/uart-mode", name);
              g_hash_table_insert (priv->paths, operation_path, operation);

              operation_path = g_strdup_printf ("/sensors/%s/uart-device", name);
              g_hash_table_insert (priv->paths, operation_path, operation);

              operation_path = g_strdup_printf ("/sensors/%s/protocol", name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_sensor_control_set_udp_ip_port_param. */
          if (type == HYSCAN_SENSOR_PORT_UDP_IP)
            {
              operation = g_new0 (HyScanSensorControlServerPort, 1);
              operation->name = g_strdup (name);
              operation->func = hyscan_sensor_control_server_udp_ip_port_param;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sensors/%s/ip-address", name);
              g_hash_table_insert (priv->paths, operation_path, operation);

              operation_path = g_strdup_printf ("/sensors/%s/udp-port", name);
              g_hash_table_insert (priv->paths, operation_path, operation);

              operation_path = g_strdup_printf ("/sensors/%s/protocol", name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_sensor_control_set_enable. */
          operation = g_new0 (HyScanSensorControlServerPort, 1);
          operation->name = g_strdup (name);
          operation->func = hyscan_sensor_control_server_set_enable;
          g_hash_table_insert (priv->operations, operation, operation);

          operation_path = g_strdup_printf ("/sensors/%s/enable", name);
          g_hash_table_insert (priv->paths, operation_path, operation);

          /* Идентификатор порта. */
          g_hash_table_insert (priv->ids, g_strdup (name), GINT_TO_POINTER (id));

          g_strfreev (pathv);
        }

      priv->signal_id = g_signal_connect (priv->params,
                                          "set",
                                          G_CALLBACK (hyscan_sensor_control_server_set_cb),
                                          server);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sensor_control_server_object_finalize (GObject *object)
{
  HyScanSensorControlServer *sensor_control_server = HYSCAN_SENSOR_CONTROL_SERVER (object);
  HyScanSensorControlServerPrivate *priv = sensor_control_server->priv;

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->params, priv->signal_id);

  g_hash_table_unref (priv->ids);
  g_hash_table_unref (priv->paths);
  g_hash_table_unref (priv->operations);

  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_sensor_control_server_parent_class)->finalize (object);
}

/* Функция освобождает память, занятую структурой HyScanSensorControlServerPort. */
static void
hyscan_sensor_control_server_free_port (gpointer data)
{
  HyScanSensorControlServerPort *port = data;

  g_free (port->name);
  g_free (port);
}

/* Функция - обработчик параметров. */
static gboolean
hyscan_sensor_control_server_set_cb (HyScanDataBox              *params,
                                     const gchar *const         *names,
                                     GVariant                  **values,
                                     HyScanSensorControlServer  *server)
{
  HyScanSensorControlServerPort *port0;
  HyScanSensorControlServerPort *portn;
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  if (n_names == 0)
    return FALSE;

  port0 = g_hash_table_lookup (server->priv->paths, names[0]);
  if (port0 == NULL)
    return TRUE;

  /* Все параметры, относящиеся к одному запросу, должны быть
   * в таблице и указывать на одну структуру с описанием операции.
   * Параметры должны относиться только к одному запроса. */
  for (i = 1; i < n_names; i++)
    {
      portn = g_hash_table_lookup (server->priv->paths, names[i]);

      if (port0 != portn)
        return FALSE;

      if (port0->name != portn->name)
        return FALSE;
    }

  return port0->func (server, port0, names, values);
}

/* Команда - hyscan_sensor_control_set_uart_port_param. */
static gboolean
hyscan_sensor_control_server_uart_port_param (HyScanSensorControlServer     *server,
                                              HyScanSensorControlServerPort *port,
                                              const gchar *const            *names,
                                              GVariant                     **values)
{
  gchar *name = NULL;
  gboolean cancel;

  gint64 value64;
  HyScanSensorProtocolType protocol = 0;
  guint uart_device = 0;
  guint uart_mode = 0;

  name = g_strdup_printf ("/sensors/%s/protocol", port->name);
  if (hyscan_control_find_integer_param (name, names, values, &value64))
    protocol = value64;
  g_free (name);

  name = g_strdup_printf ("/sensors/%s/uart-device", port->name);
  if (hyscan_control_find_integer_param (name, names, values, &value64))
    uart_device = value64;
  g_free (name);

  name = g_strdup_printf ("/sensors/%s/uart-mode", port->name);
  if (hyscan_control_find_integer_param (name, names, values, &value64))
    uart_mode = value64;
  g_free (name);

  if (protocol == 0 || uart_device == 0 || uart_mode == 0)
    return FALSE;

  g_signal_emit (server, hyscan_sensor_control_server_signals[SIGNAL_SENSOR_UART_PORT_PARAM], 0,
                 port->name, protocol, uart_device, uart_mode, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_sensor_control_set_udp_ip_port_param. */
static gboolean
hyscan_sensor_control_server_udp_ip_port_param (HyScanSensorControlServer     *server,
                                                HyScanSensorControlServerPort *port,
                                                const gchar *const            *names,
                                                GVariant                     **values)
{
  gchar *name = NULL;
  gboolean cancel;

  gint64 value64;
  HyScanSensorProtocolType protocol = 0;
  guint ip_address = 0;
  guint udp_port = 0;

  name = g_strdup_printf ("/sensors/%s/protocol", port->name);
  if (hyscan_control_find_integer_param (name, names, values, &value64))
    protocol = value64;
  g_free (name);

  name = g_strdup_printf ("/sensors/%s/ip-address", port->name);
  if (hyscan_control_find_integer_param (name, names, values, &value64))
    ip_address = value64;
  g_free (name);

  name = g_strdup_printf ("/sensors/%s/udp-port", port->name);
  if (hyscan_control_find_integer_param (name, names, values, &value64))
    udp_port = value64;
  g_free (name);

  if (protocol == 0 || ip_address == 0 || udp_port == 0)
    return FALSE;

  g_signal_emit (server, hyscan_sensor_control_server_signals[SIGNAL_SENSOR_UDP_IP_PORT_PARAM], 0,
                 port->name, protocol, ip_address, udp_port, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_sensor_control_set_enable. */
static gboolean
hyscan_sensor_control_server_set_enable (HyScanSensorControlServer     *server,
                                         HyScanSensorControlServerPort *port,
                                         const gchar *const            *names,
                                         GVariant                     **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gboolean enable;

  name = g_strdup_printf ("/sensors/%s/enable", port->name);
  status = hyscan_control_find_boolean_param (name, names, values, &enable);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_sensor_control_server_signals[SIGNAL_SENSOR_SET_ENABLE], 0,
                 port->name, enable, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Функция передаёт данные датчиков. */
void
hyscan_sensor_control_server_send_data (HyScanSensorControlServer   *server,
                                        const gchar                 *name,
                                        HyScanDataType               type,
                                        HyScanDataWriterData        *data)
{
  HyScanSonarMessage message;
  gpointer id;

  g_return_if_fail (HYSCAN_IS_SENSOR_CONTROL_SERVER (server));

  if (server->priv->params == NULL)
    return;

  id = g_hash_table_lookup (server->priv->ids, name);
  if (id == NULL)
    return;

  message.time = data->time;
  message.id   = GPOINTER_TO_INT (id);
  message.type = type;
  message.rate = 1.0;
  message.size = data->size;
  message.data = data->data;

  hyscan_sonar_box_send (HYSCAN_SONAR_BOX (server->priv->params), &message);
}
