#include "hyscan-sonar-dummy-ssse.h"

#include <string.h>
#include <hyscan-sonar-schema.h>
#include <hyscan-data-box.h>

enum
{
  PROP_O,
  PROP_ECHOSOUNDER_FREQUENCY,
  PROP_STARBOARD_FREQUENCY,
  PROP_PORT_FREQUENCY,
  PROP_FREQUENCY_DEVIATION
};

enum
{
  SIGNAL_INFO,
  SIGNAL_GAIN,
  SIGNAL_SIGNAL,
  SIGNAL_DATA,
  SIGNAL_LAST
};

/* Параметры порта. */
typedef struct
{
  gint32                       id;                     /* Идентификатор порта. */
  gchar                       *path;                   /* Путь к параметрам порта в схеме. */
  gboolean                     enable;                 /* Признак включения. */
  gint64                       send_time;              /* Время следующей отправки данных, мкс. */
  gint64                       send_period;            /* Период отправки данных, мкс. */
} HyScanSonarDummySSSESensorPort;

/* Параметры генератора. */
typedef struct
{
  gint32                       id;                     /* Идентификатор генератора. */
  gchar                       *path;                   /* Путь к параметрам генератора в схеме. */
  HyScanSourceType             source;                 /* Тип источника данных. */
  gboolean                     enable;                 /* Признак включения. */
  HyScanGeneratorModeType      mode;                   /* Режим работы генератора. */
} HyScanSonarDummySSSEGenerator;

struct _HyScanSonarDummySSSEPrivate
{
  gdouble                      echosounder_frequency;  /* Рабочая частота эхолота. */
  gdouble                      starboard_frequency;    /* Рабочая частота правого борта. */
  gdouble                      port_frequency;         /* Рабочая частота левого борта. */
  gdouble                      frequency_deviation;    /* Полоса приёмных каналов. */

  HyScanDataBox               *params;                 /* Параметры работы "гидролокатора". */

  GHashTable                  *ports;                  /* Список портов "гидролокатора". */
  GHashTable                  *generators;             /* Список генераторов "гидролокатора". */

  gint                         locked;                 /* Признак блокировки гидролокатора. */
  gint                         started;                /* Признак запуска потока отправки данных. */
  gint                         shutdown;               /* Признак необходимости завершения потока. */
  GThread                     *worker;                 /* Поток отправки данных. */

  GTimer                      *guard;                  /* Сторожевой таймер. */
};

static void            hyscan_sonar_dummy_ssse_interface_init       (HyScanSonarInterface          *iface);

static void            hyscan_sonar_dummy_ssse_set_property         (GObject                       *object,
                                                                     guint                          prop_id,
                                                                     const GValue                  *value,
                                                                     GParamSpec                    *pspec);

static void            hyscan_sonar_dummy_ssse_object_constructed   (GObject                       *object);
static void            hyscan_sonar_dummy_ssse_object_finalize      (GObject                       *object);

static gchar          *hyscan_sonar_dummy_ssse_build_schema         (gdouble                        echosounder_frequency,
                                                                     gdouble                        starboard_frequency,
                                                                     gdouble                        port_frequency,
                                                                     gdouble                        frequency_deviation);

static void            hyscan_sonar_dummy_ssse_free_port            (gpointer                       data);
static void            hyscan_sonar_dummy_ssse_free_generator       (gpointer                       data);

static void            hyscan_sonar_dummy_ssse_set_sensor_enable    (HyScanSonarDummySSSEPrivate   *priv,
                                                                     const gchar                   *name);
static void            hyscan_sonar_dummy_ssse_set_generator_enable (HyScanSonarDummySSSEPrivate   *priv,
                                                                     const gchar                   *name);

static void            hyscan_sonar_dummy_ssse_port_send_data       (gpointer                       key,
                                                                     gpointer                       value,
                                                                     gpointer                       user_data);

static gpointer        hyscan_sonar_dummy_ssse_worker               (gpointer                       user_data);

static guint           hyscan_sonar_dummy_ssse_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanSonarDummySSSE, hyscan_sonar_dummy_ssse, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarDummySSSE)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_sonar_dummy_ssse_interface_init));

static void
hyscan_sonar_dummy_ssse_class_init (HyScanSonarDummySSSEClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_sonar_dummy_ssse_set_property;

  object_class->constructed = hyscan_sonar_dummy_ssse_object_constructed;
  object_class->finalize = hyscan_sonar_dummy_ssse_object_finalize;

  g_object_class_install_property (object_class, PROP_ECHOSOUNDER_FREQUENCY,
    g_param_spec_double ("echo-freq", "EchosounderFrequency", "Echosounder frequency",
                         1.0, 1000000.0, 195000.0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_STARBOARD_FREQUENCY,
    g_param_spec_double ("star-freq", "StarboardFrequency", "Starboard frequency",
                         1.0, 1000000.0, 290000.0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PORT_FREQUENCY,
    g_param_spec_double ("port-freq", "PortFrequency", "Port frequency",
                         1.0, 1000000.0, 240000.0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_FREQUENCY_DEVIATION,
    g_param_spec_double ("freq-dev", "FrequencyDeviation", "Frequency deviation",
                         1.0, 100.0, 20.0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_dummy_ssse_signals[SIGNAL_INFO] =
    g_signal_new( "info", HYSCAN_TYPE_SONAR_DUMMY_SSSE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER );

  hyscan_sonar_dummy_ssse_signals[SIGNAL_GAIN] =
    g_signal_new( "gain", HYSCAN_TYPE_SONAR_DUMMY_SSSE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER );

  hyscan_sonar_dummy_ssse_signals[SIGNAL_SIGNAL] =
    g_signal_new( "signal", HYSCAN_TYPE_SONAR_DUMMY_SSSE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER );

  hyscan_sonar_dummy_ssse_signals[SIGNAL_DATA] =
    g_signal_new( "data", HYSCAN_TYPE_SONAR_DUMMY_SSSE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER );
}

static void
hyscan_sonar_dummy_ssse_init (HyScanSonarDummySSSE *dummy_sonar)
{
  dummy_sonar->priv = hyscan_sonar_dummy_ssse_get_instance_private (dummy_sonar);
}

static void
hyscan_sonar_dummy_ssse_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (object);
  HyScanSonarDummySSSEPrivate *priv = dummy_sonar->priv;

  switch (prop_id)
    {
    case PROP_ECHOSOUNDER_FREQUENCY:
      priv->echosounder_frequency = g_value_get_double (value);
      break;

    case PROP_STARBOARD_FREQUENCY:
      priv->starboard_frequency = g_value_get_double (value);
      break;

    case PROP_PORT_FREQUENCY:
      priv->port_frequency = g_value_get_double (value);
      break;

    case PROP_FREQUENCY_DEVIATION:
      priv->frequency_deviation = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_dummy_ssse_object_constructed (GObject *object)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (object);
  HyScanSonarDummySSSEPrivate *priv = dummy_sonar->priv;

  gchar *schema_data;
  HyScanDataSchema *schema;
  HyScanDataSchemaNode *nodes;
  HyScanDataSchemaNode *sensors = NULL;
  HyScanDataSchemaNode *generators = NULL;

  gint i;

  /* Схема "гидролокатора". */
  schema_data = hyscan_sonar_dummy_ssse_build_schema (priv->echosounder_frequency,
                                                      priv->starboard_frequency,
                                                      priv->port_frequency,
                                                      priv->frequency_deviation);
  schema = hyscan_data_schema_new_from_string (schema_data, "sonar");
  g_free (schema_data);

  priv->params = hyscan_data_box_new_from_schema (schema);
  nodes = hyscan_data_schema_list_nodes (schema);

  /* Список портов. */
  priv->ports = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       NULL, hyscan_sonar_dummy_ssse_free_port);

  for (i = 0; i < nodes->n_nodes; i++)
    {
      if (g_strcmp0 (nodes->nodes[i]->path, "/sensors") == 0)
        {
          sensors = nodes->nodes[i];
          break;
        }
    }

  if (sensors != NULL)
    {
      for (i = 0; i < sensors->n_nodes; i++)
        {
          HyScanSonarDummySSSESensorPort *port;

          gchar *key_name;
          gboolean status;
          gint64 id;

          key_name = g_strdup_printf ("%s/id", sensors->nodes[i]->path);
          status = hyscan_data_box_get_integer (priv->params, key_name, &id);
          g_free (key_name);

          if (!status || id <= 0 || id > G_MAXUINT32)
            continue;

          port = g_new (HyScanSonarDummySSSESensorPort, 1);
          port->id = id;
          port->path = g_strdup (sensors->nodes[i]->path);
          port->enable = FALSE;
          port->send_period = (i%2 == 0) ? 500000 : 1000000;
          port->send_time = g_get_monotonic_time () + port->send_period;

          g_hash_table_insert (priv->ports, port->path, port);
          g_message ("loading sensor port %s", port->path);
        }
    }

  /* Список генераторов. */
  priv->generators = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, hyscan_sonar_dummy_ssse_free_generator);

  for (i = 0; i < nodes->n_nodes; i++)
    {
      if (g_strcmp0 (nodes->nodes[i]->path, "/generators") == 0)
        {
          generators = nodes->nodes[i];
          break;
        }
    }

  if (generators != NULL)
    {
      for (i = 0; i < generators->n_nodes; i++)
        {
          HyScanSonarDummySSSEGenerator *generator;

          gchar *key_name;
          gboolean status;
          gint64 id;

          key_name = g_strdup_printf ("%s/id", generators->nodes[i]->path);
          status = hyscan_data_box_get_integer (priv->params, key_name, &id);
          g_free (key_name);

          if (!status || id <= 0 || id > G_MAXUINT32)
            continue;

          generator = g_new (HyScanSonarDummySSSEGenerator, 1);
          generator->id = id;
          generator->path = g_strdup (generators->nodes[i]->path);
          generator->enable = FALSE;
          generator->mode = HYSCAN_GENERATOR_MODE_INVALID;

          g_hash_table_insert (priv->generators, generator->path, generator);
          g_message ("loading generator %s", generator->path);
        }
    }

  hyscan_data_schema_free_nodes (nodes);
  g_object_unref (schema);

  g_signal_connect_swapped (priv->params, "changed",
                            G_CALLBACK (hyscan_sonar_dummy_ssse_set_sensor_enable), priv);
  g_signal_connect_swapped (priv->params, "changed",
                            G_CALLBACK (hyscan_sonar_dummy_ssse_set_generator_enable), priv);

  priv->guard = g_timer_new ();

  g_atomic_int_set (&priv->shutdown, 0);
  priv->worker = g_thread_new ("dummy-sonar-worker", hyscan_sonar_dummy_ssse_worker, dummy_sonar);
  while (g_atomic_int_get (&priv->started) != 1)
    g_usleep (1000);
}

static void
hyscan_sonar_dummy_ssse_object_finalize (GObject *object)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (object);
  HyScanSonarDummySSSEPrivate *priv = dummy_sonar->priv;

  g_atomic_int_set (&priv->shutdown, 1);
  g_thread_join (priv->worker);

  g_timer_destroy (priv->guard);

  g_hash_table_unref (priv->ports);
  g_object_unref (priv->params);

  G_OBJECT_CLASS (hyscan_sonar_dummy_ssse_parent_class)->finalize (object);
}

/* Функция создаёт схему гидролокатора. */
static gchar *
hyscan_sonar_dummy_ssse_build_schema (gdouble echosounder_frequency,
                                      gdouble starboard_frequency,
                                      gdouble port_frequency,
                                      gdouble frequency_deviation)
{
  HyScanSonarSchema *schema = hyscan_sonar_schema_new ();

  gdouble low_frequency;
  gdouble high_frequency;

  gchar *data;

  hyscan_sonar_schema_sensor_add (schema, "virtual1", HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "virtual2", HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "virtual3", HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "virtual4", HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

  hyscan_sonar_schema_sensor_add (schema, "uart1", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "uart2", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "uart3", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "uart4", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

  hyscan_sonar_schema_sensor_add_uart_device (schema, "UART1");
  hyscan_sonar_schema_sensor_add_uart_device (schema, "UART2");
  hyscan_sonar_schema_sensor_add_uart_device (schema, "UART3");
  hyscan_sonar_schema_sensor_add_uart_device (schema, "UART4");

  hyscan_sonar_schema_sensor_add_uart_mode (schema, "MODE1");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "MODE2");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "MODE3");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "MODE4");

  hyscan_sonar_schema_sensor_add (schema, "udp1", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "udp2", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "udp3", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "udp4", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

  hyscan_sonar_schema_sensor_add_ip_address (schema, "IP1");
  hyscan_sonar_schema_sensor_add_ip_address (schema, "IP2");
  hyscan_sonar_schema_sensor_add_ip_address (schema, "IP3");
  hyscan_sonar_schema_sensor_add_ip_address (schema, "IP4");

  low_frequency = echosounder_frequency - (echosounder_frequency * frequency_deviation / 2.0);
  high_frequency = echosounder_frequency + (echosounder_frequency * frequency_deviation / 2.0);
  hyscan_sonar_schema_generator_add (schema, HYSCAN_SOURCE_ECHOSOUNDER, 0xf, 0xf, low_frequency, high_frequency, 0.100);

  low_frequency = starboard_frequency - (starboard_frequency * frequency_deviation / 2.0);
  high_frequency = starboard_frequency + (starboard_frequency * frequency_deviation / 2.0);
  hyscan_sonar_schema_generator_add (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 0xf, 0xf, low_frequency, high_frequency, 0.100);

  low_frequency = port_frequency - (port_frequency * frequency_deviation / 2.0);
  high_frequency = port_frequency + (port_frequency * frequency_deviation / 2.0);
  hyscan_sonar_schema_generator_add (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, 0xf, 0xf, low_frequency, high_frequency, 0.100);

  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_ECHOSOUNDER, "PRESET1");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_ECHOSOUNDER, "PRESET2");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_ECHOSOUNDER, "PRESET3");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_ECHOSOUNDER, "PRESET4");

  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, "PRESET1");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, "PRESET2");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, "PRESET3");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, "PRESET4");

  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, "PRESET1");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, "PRESET2");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, "PRESET3");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, "PRESET4");

  data = hyscan_data_schema_builder_get_data (HYSCAN_DATA_SCHEMA_BUILDER (schema));
  g_object_unref (schema);

  return data;
}

/* Функция освобождает память, выделеную под структуру HyScanSonarDummySSSESensorPort. */
static void
hyscan_sonar_dummy_ssse_free_port (gpointer data)
{
  HyScanSonarDummySSSESensorPort *port = data;

  g_free (port->path);
  g_free (port);
}

/* Функция освобождает память, выделеную под структуру HyScanSonarDummySSSEGenerator. */
static void
hyscan_sonar_dummy_ssse_free_generator (gpointer data)
{
  HyScanSonarDummySSSEGenerator *generator = data;

  g_free (generator->path);
  g_free (generator);
}

/* Функция управляет состоянием портов. */
static void
hyscan_sonar_dummy_ssse_set_sensor_enable (HyScanSonarDummySSSEPrivate *priv,
                                           const gchar                 *name)
{
  gchar *path;
  const gchar *path_end;

  gboolean status;
  gchar *key_name;

  gboolean enable;
  gint64 type = HYSCAN_SENSOR_PORT_INVALID;

  HyScanSonarDummySSSESensorPort *port;

  /* Обрабатываем только включения и выключения. */
  if (!g_str_has_prefix (name, "/sensors/") ||
      !g_str_has_suffix (name, "/enable"))
    return;

  /* Устанавливаемое состояние порта. */
  if (!hyscan_data_box_get_boolean (priv->params, name, &enable))
    return;

  /* Путь к порту. */
  path_end = g_strrstr (name, "/enable");
  if (path_end == NULL)
    return;
  path = g_strndup (name, path_end - name);

  /* Состояние порта. */
  port = g_hash_table_lookup (priv->ports, path);
  if (port == NULL)
    goto exit;
  else
    port->enable = enable;

  /* Выключили порт. */
  if (!enable)
    goto exit;

  /* Тип порта. */
  key_name = g_strdup_printf ("%s/type", path);
  status = hyscan_data_box_get_enum (priv->params, key_name, &type);
  g_free (key_name);

  if (!status)
    goto exit;

  /* Проверяем настрйки портов. */
  switch (type)
    {
    /* Нет дополнительных настроек. */
    case HYSCAN_SENSOR_PORT_VIRTUAL:
      break;

    /* Необходимо задать IP адрес и UDP порт. */
    case HYSCAN_SENSOR_PORT_UDP_IP:
      {
        gint64 ip_address;
        gint64 udp_port;

        key_name = g_strdup_printf ("%s/ip-address", path);
        status = hyscan_data_box_get_enum (priv->params, key_name, &ip_address);
        g_free (key_name);
        if (!status || ip_address == 0)
          {
            enable = FALSE;
            break;
          }

        key_name = g_strdup_printf ("%s/udp-port", path);
        status = hyscan_data_box_get_integer (priv->params, key_name, &udp_port);
        g_free (key_name);
        if (!status || udp_port < 1024 || udp_port > 65535)
          {
            enable = FALSE;
            break;
          }
      }
      break;

    /* Необходимо задать устройство UART и режим работы. */
    case HYSCAN_SENSOR_PORT_UART:
      {
        gint64 uart_dev;
        gint64 uart_mode;

        key_name = g_strdup_printf ("%s/uart-device", path);
        status = hyscan_data_box_get_enum (priv->params, key_name, &uart_dev);
        g_free (key_name);
        if (!status || uart_dev == 0)
          {
            enable = FALSE;
            break;
          }

        key_name = g_strdup_printf ("%s/uart-mode", path);
        status = hyscan_data_box_get_enum (priv->params, key_name, &uart_mode);
        g_free (key_name);
        if (!status || uart_mode == 0)
          {
            enable = FALSE;
            break;
          }

      }
      break;

    /* Неизвестный тип порта!? */
    default:
      enable = FALSE;
      break;
    }

  /* Изменяем состояние порта. */
  key_name = g_strdup_printf ("%s/status", path);
  if (enable)
    status = hyscan_data_box_set_enum (priv->params, key_name, HYSCAN_SENSOR_PORT_STATUS_OK);
  else
    status = hyscan_data_box_set_enum (priv->params, key_name, HYSCAN_SENSOR_PORT_STATUS_DISABLED);
  g_free (key_name);

  /* Если произошла ошибка, отключаем порт. */
  if (!enable || (enable && !status))
    hyscan_data_box_set_boolean (priv->params, name, FALSE);

exit:
  g_free (path);
}
/* Функция управляет состоянием генераторов. */
static void
hyscan_sonar_dummy_ssse_set_generator_enable (HyScanSonarDummySSSEPrivate *priv,
                                              const gchar                 *name)
{
  gchar *path;
  const gchar *path_end;

  gboolean enable;

  HyScanSonarDummySSSEGenerator *generator;

  /* Обрабатываем только включения и выключения. */
  if (!g_str_has_prefix (name, "/generators/") ||
      !g_str_has_suffix (name, "/enable"))
    return;

  /* Устанавливаемое состояние генератора. */
  if (!hyscan_data_box_get_boolean (priv->params, name, &enable))
    return;

  /* Путь к генератору. */
  path_end = g_strrstr (name, "/enable");
  if (path_end == NULL)
    return;
  path = g_strndup (name, path_end - name);

  /* Состояние генератора. */
  generator = g_hash_table_lookup (priv->generators, path);
  if (generator == NULL)
    goto exit;
  else
    generator->enable = enable;

  /* Выключили генератор. */
  if (!enable)
    goto exit;

  /* Режим работы генератора. */
  if (generator->mode == HYSCAN_GENERATOR_MODE_INVALID)
    hyscan_data_box_set_boolean (priv->params, name, FALSE);

exit:
  g_free (path);
}

/* Функция отправляет NMEA данные. */
static void
hyscan_sonar_dummy_ssse_port_send_data (gpointer key,
                                        gpointer value,
                                        gpointer user_data)
{
  HyScanSonarDummySSSESensorPort *port = value;
  HyScanSonarDummySSSE *dummy_sonar = user_data;
  HyScanSonarMsgData data_msg;

  gint64 time;

  GDateTime *dt;
  gchar *nmea_gga;
  gchar *nmea_rmc;
  guchar nmea_crc;
  gsize nmea_len;
  gsize i;

  /* Проверяем время до отправки данных. */
  time = g_get_monotonic_time ();
  if (port->send_time >= time)
    return;

  /* Время следующей отправки данных. */
  while (port->send_time <= time)
    port->send_time += port->send_period;

  /* Проверяем состояние порта. */
  if (!port->enable)
    return;

  dt = g_date_time_new_now_utc ();
  data_msg.type = HYSCAN_SONAR_MSG_DATA;
  data_msg.time = 1000000 * g_date_time_to_unix (dt) + g_date_time_get_microsecond (dt);
  data_msg.id = port->id;

  /* Строка GGA. */
  nmea_gga = g_strdup_printf ("$GPGGA,%02d%02d%02d.%03d,0000.00,N,00000.00,W,1,12,1.0,1.0,M,1.0,M,,0000*%02X",
                              g_date_time_get_hour (dt),
                              g_date_time_get_minute (dt),
                              g_date_time_get_second (dt),
                              g_date_time_get_microsecond (dt) / 1000,
                              0);

  nmea_crc = 0;
  nmea_len = strlen (nmea_gga);
  for (i = 1; i < nmea_len - 3; i++)
    nmea_crc ^= nmea_gga[i];
  g_snprintf (nmea_gga + nmea_len - 2, 3, "%02X", nmea_crc);

  data_msg.data_size = nmea_len + 1;
  data_msg.data = nmea_gga;
  g_signal_emit (dummy_sonar, hyscan_sonar_dummy_ssse_signals[SIGNAL_DATA], 0, &data_msg);

  /* Строка RMC. */
  nmea_rmc = g_strdup_printf ("$GPRMC,%02d%02d%02d.%03d,A,0000.00,N,00000.00,W,001.0,180.0,%02d%02d%02d,0.0,W*%02X",
                              g_date_time_get_hour (dt),
                              g_date_time_get_minute (dt),
                              g_date_time_get_second (dt),
                              g_date_time_get_microsecond (dt) / 1000,
                              g_date_time_get_day_of_month (dt),
                              g_date_time_get_month (dt),
                              g_date_time_get_year (dt) % 100,
                              0);

  nmea_crc = 0;
  nmea_len = strlen (nmea_rmc);
  for (i = 1; i < nmea_len - 3; i++)
    nmea_crc ^= nmea_rmc[i];
  g_snprintf (nmea_rmc + nmea_len - 2, 3, "%02X", nmea_crc);

  data_msg.data_size = nmea_len + 1;
  data_msg.data = nmea_rmc;
  g_signal_emit (dummy_sonar, hyscan_sonar_dummy_ssse_signals[SIGNAL_DATA], 0, &data_msg);

  g_free (nmea_gga);
  g_free (nmea_rmc);
  g_date_time_unref (dt);
}

static gpointer
hyscan_sonar_dummy_ssse_worker (gpointer user_data)
{
  HyScanSonarDummySSSE *dummy_sonar = user_data;
  HyScanSonarDummySSSEPrivate *priv = dummy_sonar->priv;

  g_atomic_int_set (&priv->started, 1);

  while (g_atomic_int_get (&priv->shutdown) != 1)
    {
      g_usleep (500);

      /* Если гидролокатор не заблокирован, ждём. */
      if (g_atomic_int_get (&priv->locked) == 0)
        continue;

      /* Отправляем данные портов. */
      g_hash_table_foreach (priv->ports, hyscan_sonar_dummy_ssse_port_send_data, dummy_sonar);
    }

  return NULL;
}

HyScanSonar *
hyscan_sonar_dummy_ssse_new (gdouble echosounder_frequency,
                             gdouble starboard_frequency,
                             gdouble port_frequency,
                             gdouble frequency_deviation)
{
  return g_object_new (HYSCAN_TYPE_SONAR_DUMMY_SSSE, NULL);
}

static HyScanDataSchema *
hyscan_sonar_dummy_ssse_get_schema (HyScanSonar *sonar)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (sonar);
  HyScanSonarDummySSSEPrivate *priv = dummy_sonar->priv;

  return hyscan_data_box_get_schema (priv->params);
}

static HyScanSonarLockType
hyscan_sonar_dummy_ssse_lock (HyScanSonar *sonar,
                              const gchar *address,
                              guint16      port)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (sonar);

  if (address != NULL)
    return HYSCAN_SONAR_LOCK_FAIL;

  if (g_atomic_int_compare_and_exchange (&dummy_sonar->priv->locked, 0, 1))
    return HYSCAN_SONAR_LOCK_OK;

  return HYSCAN_SONAR_LOCK_BUSY;
}

static gboolean
hyscan_sonar_dummy_ssse_unlock (HyScanSonar *sonar)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (sonar);

  return g_atomic_int_compare_and_exchange (&dummy_sonar->priv->locked, 1, 0);
}

static gboolean
hyscan_sonar_dummy_ssse_ping (HyScanSonar *sonar)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (sonar);

  g_timer_reset (dummy_sonar->priv->guard);

  return TRUE;
}

static gboolean
hyscan_sonar_dummy_ssse_set (HyScanSonar          *sonar,
                             const gchar          *name,
                             HyScanDataSchemaType  type,
                             gconstpointer         value,
                             gint32                size)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (sonar);

  return hyscan_data_box_set (dummy_sonar->priv->params, name, type, value, size);
}

static gboolean
hyscan_sonar_dummy_ssse_get (HyScanSonar          *sonar,
                             const gchar          *name,
                             HyScanDataSchemaType  type,
                             gpointer              buffer,
                             gint32               *buffer_size)
{
  HyScanSonarDummySSSE *dummy_sonar = HYSCAN_SONAR_DUMMY_SSSE (sonar);

  return hyscan_data_box_get (dummy_sonar->priv->params, name, type, buffer, buffer_size);
}

static void
hyscan_sonar_dummy_ssse_interface_init (HyScanSonarInterface *iface)
{
  iface->get_schema = hyscan_sonar_dummy_ssse_get_schema;
  iface->lock = hyscan_sonar_dummy_ssse_lock;
  iface->unlock = hyscan_sonar_dummy_ssse_unlock;
  iface->ping = hyscan_sonar_dummy_ssse_ping;
  iface->set = hyscan_sonar_dummy_ssse_set;
  iface->get = hyscan_sonar_dummy_ssse_get;
}
