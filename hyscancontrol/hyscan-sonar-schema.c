/*
 * \file hyscan-sonar-schema.c
 *
 * \brief Исходный файл класса генерации схемы гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-schema.h"
#include "hyscan-control-common.h"

enum
{
  PROP_0,
  PROP_TIMEOUT
};

struct _HyScanSonarSchemaPrivate
{
  gint                         id_counter;                     /* Счётчик идентификаторов объектов. */

  gdouble                      timeout;                        /* Таймаут ожидания команд от клиента. */

  GHashTable                  *sources;                        /* Наличие источников данных. */
  GHashTable                  *generators;                     /* Наличие генераторов. */
  GHashTable                  *tvgs;                           /* Наличие ВАРУ. */
  GHashTable                  *channels;                       /* Наличие приёмного канала. */
  GHashTable                  *acoustics;                      /* Наличие источника "акустических" данных. */
};

static void    hyscan_sonar_schema_set_property                (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void    hyscan_sonar_schema_object_constructed          (GObject                       *object);
static void    hyscan_sonar_schema_object_finalize             (GObject                       *object);

static void    hyscan_sonar_schema_enum_add_port_type          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_port_protocol      (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_port_status        (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_ip_port_addresses  (HyScanSonarSchema             *schema,
                                                                const gchar                   *port_name);
static void    hyscan_sonar_schema_enum_add_uart_devs          (HyScanSonarSchema             *schema,
                                                                const gchar                   *port_name);
static void    hyscan_sonar_schema_enum_add_uart_modes         (HyScanSonarSchema             *schema,
                                                                const gchar                   *port_name);

static void    hyscan_sonar_schema_enum_add_sync_type          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_track_type         (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_signal_type        (HyScanSonarSchema             *schema);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarSchema, hyscan_sonar_schema, HYSCAN_TYPE_DATA_SCHEMA_BUILDER)

static void
hyscan_sonar_schema_class_init (HyScanSonarSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_schema_set_property;

  object_class->constructed = hyscan_sonar_schema_object_constructed;
  object_class->finalize = hyscan_sonar_schema_object_finalize;

  g_object_class_install_property (object_class, PROP_TIMEOUT,
    g_param_spec_double ("timeout", "AliveTimeout", "Alive timeout",
                         HYSCAN_SONAR_SCHEMA_MIN_TIMEOUT,
                         HYSCAN_SONAR_SCHEMA_MAX_TIMEOUT,
                         HYSCAN_SONAR_SCHEMA_DEFAULT_TIMEOUT,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_schema_init (HyScanSonarSchema *schema)
{
  schema->priv = hyscan_sonar_schema_get_instance_private (schema);
}

static void
hyscan_sonar_schema_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanSonarSchema *schema = HYSCAN_SONAR_SCHEMA (object);
  HyScanSonarSchemaPrivate *priv = schema->priv;

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      priv->timeout = g_value_get_double (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_schema_object_constructed (GObject *object)
{
  HyScanSonarSchema *schema = HYSCAN_SONAR_SCHEMA (object);
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (object);
  HyScanSonarSchemaPrivate *priv = schema->priv;

  gint id;

  G_OBJECT_CLASS (hyscan_sonar_schema_parent_class)->constructed (object);

  priv->id_counter = 1;

  priv->sources = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->generators = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->tvgs = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->channels = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->acoustics = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Enum значения. */
  hyscan_sonar_schema_enum_add_port_type (schema);
  hyscan_sonar_schema_enum_add_port_protocol (schema);
  hyscan_sonar_schema_enum_add_port_status (schema);
  hyscan_sonar_schema_enum_add_sync_type (schema);
  hyscan_sonar_schema_enum_add_track_type (schema);
  hyscan_sonar_schema_enum_add_signal_type (schema);

  /* Версия и идентификатор схемы данных гидролокатора. */
  hyscan_data_schema_builder_key_integer_create (builder, "/schema/id", "id",
                                                 "Sonar schema id", HYSCAN_SONAR_SCHEMA_ID);
  hyscan_data_schema_builder_key_set_access (builder, "/schema/id", HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_integer_create (builder, "/schema/version", "version",
                                                 "Sonar schema version", HYSCAN_SONAR_SCHEMA_VERSION);
  hyscan_data_schema_builder_key_set_access (builder, "/schema/version", HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  /* Название и модель гидролокатора. */

  /* Параметры управления. */
  hyscan_data_schema_builder_key_double_create (builder, "/control/timeout", "alive-timeout",
                                                "Alive timeout", priv->timeout);
  hyscan_data_schema_builder_key_set_access (builder, "/control/timeout", HYSCAN_DATA_SCHEMA_ACCESS_READONLY);

  hyscan_data_schema_builder_key_boolean_create (builder, "/control/alive", "alive",
                                                 "Sonar watchdog", FALSE);
  hyscan_data_schema_builder_key_boolean_create (builder, "/control/enable", "enable",
                                                 "Enable sonar", FALSE);

  hyscan_data_schema_builder_key_string_create  (builder, "/control/track-name", "track-name",
                                                 "Track name", NULL);
  hyscan_data_schema_builder_key_enum_create    (builder, "/control/track-type", "track-type",
                                                 "Track type", "track-type", HYSCAN_TRACK_SURVEY);

  /* Идентификатор для сообщений от гидролокатора. */
  id = schema->priv->id_counter++;
  hyscan_data_schema_builder_key_integer_create (builder, "/id", "id", "ID", id);
  hyscan_data_schema_builder_key_set_access (builder, "/id", HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
}

static void
hyscan_sonar_schema_object_finalize (GObject *object)
{
  HyScanSonarSchema *schema = HYSCAN_SONAR_SCHEMA (object);
  HyScanSonarSchemaPrivate *priv = schema->priv;

  g_hash_table_unref (priv->sources);
  g_hash_table_unref (priv->generators);
  g_hash_table_unref (priv->tvgs);
  g_hash_table_unref (priv->channels);
  g_hash_table_unref (priv->acoustics);

  G_OBJECT_CLASS (hyscan_sonar_schema_parent_class)->finalize (object);
}

/* Функция создаёт enum значение port-type. */
static void
hyscan_sonar_schema_enum_add_port_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "port-type");

  hyscan_data_schema_builder_enum_value_create (builder, "port-type",
                                                HYSCAN_SENSOR_PORT_VIRTUAL,
                                                "Virtual", NULL);

  hyscan_data_schema_builder_enum_value_create (builder, "port-type",
                                                HYSCAN_SENSOR_PORT_UART,
                                                "UART", NULL);

  hyscan_data_schema_builder_enum_value_create (builder, "port-type",
                                                HYSCAN_SENSOR_PORT_UDP_IP,
                                                "UDP/IP", NULL);
}

/* Функция создаёт enum значение port-protocol. */
static void
hyscan_sonar_schema_enum_add_port_protocol (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "port-protocol");

  hyscan_data_schema_builder_enum_value_create (builder, "port-protocol",
                                                HYSCAN_SENSOR_PROTOCOL_SAS,
                                                "SAS", NULL);

  hyscan_data_schema_builder_enum_value_create (builder, "port-protocol",
                                                HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                "NMEA-0183", NULL);
}

/* Функция создаёт enum значение port-status. */
static void
hyscan_sonar_schema_enum_add_port_status (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "port-status");

  hyscan_data_schema_builder_enum_value_create (builder, "port-status",
                                                HYSCAN_SENSOR_PORT_STATUS_DISABLED,
                                                "Disabled", NULL);

  hyscan_data_schema_builder_enum_value_create (builder, "port-status",
                                                HYSCAN_SENSOR_PORT_STATUS_OK,
                                                "Ok", NULL);

  hyscan_data_schema_builder_enum_value_create (builder, "port-status",
                                                HYSCAN_SENSOR_PORT_STATUS_WARNING,
                                                "Warning", NULL);

  hyscan_data_schema_builder_enum_value_create (builder, "port-status",
                                                HYSCAN_SENSOR_PORT_STATUS_ERROR,
                                                "Error", NULL);
}

/* Функция создаёт enum значение ip-address. */
static void
hyscan_sonar_schema_enum_add_ip_port_addresses (HyScanSonarSchema *schema,
                                                const gchar       *port_name)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  gchar *enum_id;

  enum_id = g_strdup_printf ("%s-ip-address", port_name);
  hyscan_data_schema_builder_enum_create (builder, enum_id);
  hyscan_data_schema_builder_enum_value_create (builder, enum_id, 0, "Disabled", NULL);
  g_free (enum_id);
}

/* Функция создаёт enum значение uart-dev. */
static void
hyscan_sonar_schema_enum_add_uart_devs (HyScanSonarSchema *schema,
                                        const gchar       *port_name)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  gchar *enum_id;

  enum_id = g_strdup_printf ("%s-uart-device", port_name);
  hyscan_data_schema_builder_enum_create (builder, enum_id);
  hyscan_data_schema_builder_enum_value_create (builder, enum_id, 0, "Disabled", NULL);
  g_free (enum_id);
}

/* Функция создаёт enum значение uart-mode. */
static void
hyscan_sonar_schema_enum_add_uart_modes (HyScanSonarSchema *schema,
                                         const gchar       *port_name)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  gchar *enum_id;

  enum_id = g_strdup_printf ("%s-uart-mode", port_name);
  hyscan_data_schema_builder_enum_create (builder, enum_id);
  hyscan_data_schema_builder_enum_value_create (builder, enum_id, 0, "Disabled", NULL);
  g_free (enum_id);
}

/* Функция создаёт enum значение sync-type. */
static void
hyscan_sonar_schema_enum_add_sync_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "sync-type");

  hyscan_data_schema_builder_enum_value_create (builder, "sync-type",
                                                0,
                                                "Default", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "sync-type",
                                                HYSCAN_SONAR_SYNC_INTERNAL,
                                                "Internal", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "sync-type",
                                                HYSCAN_SONAR_SYNC_EXTERNAL,
                                                "External", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "sync-type",
                                                HYSCAN_SONAR_SYNC_SOFTWARE,
                                                "Software", NULL);
}

/* Функция создаёт enum значение track-type. */
static void
hyscan_sonar_schema_enum_add_track_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "track-type");

  hyscan_data_schema_builder_enum_value_create (builder, "track-type",
                                                HYSCAN_TRACK_CALIBRATION,
                                                "Calibration", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "track-type",
                                                HYSCAN_TRACK_SURVEY,
                                                "Survey", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "track-type",
                                                HYSCAN_TRACK_TACK,
                                                "Tack", NULL);
}

/* Функция создаёт enum значение signal-type. */
static void
hyscan_sonar_schema_enum_add_signal_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "signal-type");

  hyscan_data_schema_builder_enum_value_create (builder, "signal-type",
                                                0,
                                                "Default", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "signal-type",
                                                HYSCAN_GENERATOR_SIGNAL_AUTO,
                                                "Auto", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "signal-type",
                                                HYSCAN_GENERATOR_SIGNAL_TONE,
                                                "Tone", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "signal-type",
                                                HYSCAN_GENERATOR_SIGNAL_LFM,
                                                "LFM", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "signal-type",
                                                HYSCAN_GENERATOR_SIGNAL_LFMD,
                                                "LFMD", NULL);
}

/* Функция создаёт новый объект HyScanSonarSchema. */
HyScanSonarSchema *
hyscan_sonar_schema_new (gdouble timeout)
{
  return g_object_new (HYSCAN_TYPE_SONAR_SCHEMA,
                       "schema-id", "sonar",
                       "timeout", timeout,
                       NULL);
}

/* Функция добавляет описание порта для подключения датчика. */
gint
hyscan_sonar_schema_sensor_add (HyScanSonarSchema        *schema,
                                const gchar              *name,
                                HyScanSensorPortType      type,
                                HyScanSensorProtocolType  protocol)
{
  HyScanSonarSchemaPrivate *priv;

  HyScanDataSchemaBuilder *builder;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  priv = schema->priv;
  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  prefix = g_strdup_printf ("/sensors/%s", name);

  /* Тип порта. */
  key_id = g_strdup_printf ("%s/type", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "type",
                                                       NULL, "port-type", type);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Формат данных. */
  key_id = g_strdup_printf ("%s/protocol", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "protocol",
                                                       NULL, "port-protocol", protocol);
  if (status && (type == HYSCAN_SENSOR_PORT_VIRTUAL))
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Номер канала. */
  key_id = g_strdup_printf ("%s/channel", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "channel", NULL, 1);
  if (status)
    status = hyscan_data_schema_builder_key_integer_range (builder, key_id, 1, HYSCAN_SENSOR_CONTROL_MAX_CHANNELS, 1);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Коррекция времени приёма данных. */
  key_id = g_strdup_printf ("%s/time-offset", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "time-offset", NULL, 0);
  if (status)
    status = hyscan_data_schema_builder_key_integer_range (builder, key_id, 0, 600 * G_USEC_PER_SEC, 1);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Состояние порта */
  key_id = g_strdup_printf ("%s/status", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "status", NULL,
                                                       "port-status", HYSCAN_SENSOR_PORT_STATUS_DISABLED);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Признак включения. */
  key_id = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "enable", NULL, FALSE);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Местоположение антенны. */
  key_id = g_strdup_printf ("%s/position/x", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "x", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/y", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "y", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/z", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "z", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/psi", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "psi", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/gamma", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "gamma", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/theta", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "theta", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Дополнительные параметры UDP/IP порта. */
  if (type == HYSCAN_SENSOR_PORT_UDP_IP)
    {
      gchar *enum_id;

      /* Список IP адресов. */
      hyscan_sonar_schema_enum_add_ip_port_addresses (schema, name);

      /* IP адрес. */
      key_id = g_strdup_printf ("%s/ip-address", prefix);
      enum_id = g_strdup_printf ("%s-ip-address", name);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "ip-address", NULL, enum_id, 0);
      g_free (enum_id);
      g_free (key_id);

      if (!status)
        goto exit;

      /* UDP порт. */
      key_id = g_strdup_printf ("%s/udp-port", prefix);
      status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "udp-port", NULL, 10000);
      if (status)
        status = hyscan_data_schema_builder_key_integer_range (builder, key_id, 1024, 65535, 1);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Дополнительные параметры UART порта. */
  if (type == HYSCAN_SENSOR_PORT_UART)
    {
      gchar *enum_id;

      /* Список UART портов и режимов работы. */
      hyscan_sonar_schema_enum_add_uart_devs (schema, name);
      hyscan_sonar_schema_enum_add_uart_modes (schema, name);

      /* Физическое устройство. */
      key_id = g_strdup_printf ("%s/uart-device", prefix);
      enum_id = g_strdup_printf ("%s-uart-device", name);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "uart-device", NULL, enum_id, 0);
      g_free (enum_id);
      g_free (key_id);

      if (!status)
        goto exit;

      /* Режим работы. */
      key_id = g_strdup_printf ("%s/uart-mode", prefix);
      enum_id = g_strdup_printf ("%s-uart-mode", name);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "uart-mode", NULL, enum_id, 0);
      g_free (enum_id);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Идентификатор порта. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

/* Функция добавляет UART устройство в список допустимых для UART датчика. */
gint
hyscan_sonar_schema_sensor_add_uart_device (HyScanSonarSchema *schema,
                                            const gchar       *port_name,
                                            const gchar       *device_name)
{
  HyScanDataSchemaBuilder *builder;
  gchar *enum_id;
  gint id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  enum_id = g_strdup_printf ("%s-uart-device", port_name);
  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, enum_id, id, device_name, NULL))
    id = -1;

  g_free (enum_id);

  return id;
}

/* Функция добавляет режим работы UART устройства в список допустимых для UART датчика. */
gint
hyscan_sonar_schema_sensor_add_uart_mode (HyScanSonarSchema *schema,
                                          const gchar       *port_name,
                                          const gchar     *mode_name)
{
  HyScanDataSchemaBuilder *builder;
  gchar *enum_id;
  gint id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  enum_id = g_strdup_printf ("%s-uart-mode", port_name);
  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, enum_id, id, mode_name, NULL))
    id= -1;

  g_free (enum_id);

  return id;
}

/* Функция добавляет IP адрес в список допустимых для IP датчика. */
gint
hyscan_sonar_schema_sensor_add_ip_address (HyScanSonarSchema *schema,
                                           const gchar       *port_name,
                                           const gchar       *ip_name)
{
  HyScanDataSchemaBuilder *builder;
  gchar *enum_id;
  gint id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  enum_id = g_strdup_printf ("%s-ip-address", port_name);
  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, enum_id, id, ip_name, NULL))
    id = -1;

  g_free (enum_id);

  return id;
}

/* Функция добавляет в схему определение параметров синхронизации излучения. */
gboolean
hyscan_sonar_schema_sync_add (HyScanSonarSchema   *schema,
                              HyScanSonarSyncType  capabilities)
{
  HyScanDataSchemaBuilder *builder;
  gboolean status;
  gchar *key_id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), FALSE);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  key_id = "/sync/capabilities";
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "capabilities", NULL, capabilities);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  if (!status)
    return FALSE;

  key_id = "/sync/type";
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "type", NULL, "sync-type", 0);
  if (!status)
    return FALSE;

  if (capabilities & HYSCAN_SONAR_SYNC_SOFTWARE)
    {
      key_id = "/sync/ping";
      status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "ping", NULL, FALSE);
      if (!status)
        return FALSE;
    }

  return TRUE;
}

/* Функция добавляет в схему описание источника данных. */
gint
hyscan_sonar_schema_source_add (HyScanSonarSchema *schema,
                                HyScanSourceType   source,
                                gdouble            antenna_vpattern,
                                gdouble            antenna_hpattern,
                                gdouble            antenna_frequency,
                                gdouble            antenna_bandwidth,
                                gdouble            max_receive_time,
                                gboolean           auto_receive_time)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *source_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gdouble min_receive_time;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  source_name = hyscan_control_get_source_name (source);
  if (source_name == NULL)
    return -1;

  if (g_hash_table_contains (schema->priv->sources, GINT_TO_POINTER (source)))
    return -1;

  prefix = g_strdup_printf ("/sources/%s", source_name);

  /* Диаграмма направленности антенны в вертикальной плоскости. */
  key_id = g_strdup_printf ("%s/antenna/pattern/vertical", prefix);
  status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "vertical-pattern", NULL, antenna_vpattern);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Диаграмма направленности антенны в горизонтальной плоскости. */
  key_id = g_strdup_printf ("%s/antenna/pattern/horizontal", prefix);
  status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "horizontal-pattern", NULL,  antenna_hpattern);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Центральная частота. */
  key_id = g_strdup_printf ("%s/antenna/frequency", prefix);
  status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "frequency", NULL, antenna_frequency);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Полоса пропускания. */
  key_id = g_strdup_printf ("%s/antenna/bandwidth", prefix);
  status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "bandwidth", NULL, antenna_bandwidth);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Время приёма эхосигнала источником данных. */
  if (auto_receive_time)
    min_receive_time = -G_MAXDOUBLE;
  else
    min_receive_time = 0.0;

  key_id = g_strdup_printf ("%s/control/receive-time", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "receive-time", NULL, 0.0);
  if (status)
    status = hyscan_data_schema_builder_key_double_range (builder, key_id, min_receive_time, max_receive_time, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Местоположение антенны. */
  key_id = g_strdup_printf ("%s/position/x", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "x", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/y", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "y", NULL,  0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/z", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "z", NULL,  0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/psi", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "psi", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/gamma", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "gamma", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  key_id = g_strdup_printf ("%s/position/theta", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "theta", NULL, 0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор источника данных. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->sources, GINT_TO_POINTER (source), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

/* Функция добавляет описание генератора. */
gint
hyscan_sonar_schema_generator_add (HyScanSonarSchema         *schema,
                                   HyScanSourceType           source,
                                   HyScanGeneratorModeType    capabilities,
                                   HyScanGeneratorSignalType  signals,
                                   gdouble                    min_tone_duration,
                                   gdouble                    max_tone_duration,
                                   gdouble                    min_lfm_duration,
                                   gdouble                    max_lfm_duration)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *source_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gchar *preset = NULL;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  source_name = hyscan_control_get_source_name (source);
  if (source_name == NULL)
    return -1;

  if (!g_hash_table_contains (schema->priv->sources, GINT_TO_POINTER (source)))
    return -1;

  if (g_hash_table_contains (schema->priv->generators, GINT_TO_POINTER (source)))
    return -1;

  prefix = g_strdup_printf ("/sources/%s/generator", source_name);

  /* Режимы работы генератора. */
  key_id = g_strdup_printf ("%s/capabilities", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "capabilities", NULL, capabilities);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Возможные сигналы. */
  key_id = g_strdup_printf ("%s/signals", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "signals", NULL, signals);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Признак включения. */
  key_id = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "enable", NULL, FALSE);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Автоматический режим работы генератора. */
  if (capabilities & HYSCAN_GENERATOR_MODE_AUTO)
    {
      key_id = g_strdup_printf ("%s/auto/signal", prefix);
      status =  hyscan_data_schema_builder_key_enum_create (builder, key_id, "signal", NULL,  "signal-type", 0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Преднастройки генератора. */
  if (capabilities & HYSCAN_GENERATOR_MODE_PRESET)
    {
      preset = g_strdup_printf ("%s-generator-preset", source_name);

      status = hyscan_data_schema_builder_enum_create (builder, preset);
      if (!status)
        goto exit;

      status = hyscan_data_schema_builder_enum_value_create (builder, preset, 0, "Disabled", NULL);
      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/preset/id", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "id", NULL,  preset, 0);
      g_free (key_id);

      if (!status)
        goto exit;

      g_clear_pointer (&preset, g_free);
    }

  /* Упрощённый режим управления генератором. */
  if (capabilities & HYSCAN_GENERATOR_MODE_SIMPLE)
    {
      key_id = g_strdup_printf ("%s/simple/signal", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "signal", NULL,  "signal-type", 0);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/simple/power", prefix);
      status = hyscan_data_schema_builder_key_double_create (builder, key_id, "power", NULL,  100.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, 0.0, 100.0, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Расширенный режим управления генератором. */
  if (capabilities & HYSCAN_GENERATOR_MODE_EXTENDED)
    {
      if (signals & HYSCAN_GENERATOR_SIGNAL_TONE)
        {
          key_id = g_strdup_printf ("%s/tone/duration", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "duration", NULL,  min_tone_duration);
          if (status)
            status = hyscan_data_schema_builder_key_double_range (builder, key_id, min_tone_duration, max_tone_duration, 0.00001);
          g_free (key_id);

          if (!status)
            goto exit;

          key_id = g_strdup_printf ("%s/tone/power", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "power", NULL, 100.0);
          if (status)
            status = hyscan_data_schema_builder_key_double_range (builder, key_id, 0.0, 100.0, 1.0);
          g_free (key_id);

          if (!status)
            goto exit;
        }

      if ((signals & HYSCAN_GENERATOR_SIGNAL_LFM) || (signals & HYSCAN_GENERATOR_SIGNAL_LFMD))
        {
          key_id = g_strdup_printf ("%s/lfm/decreasing", prefix);
          status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "decreasing", NULL, FALSE);
          g_free (key_id);

          if (!status)
            goto exit;

          key_id = g_strdup_printf ("%s/lfm/duration", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "duration", NULL,  min_lfm_duration);
          if (status)
            status = hyscan_data_schema_builder_key_double_range (builder, key_id, min_lfm_duration, max_lfm_duration, 0.001);
          g_free (key_id);

          if (!status)
            goto exit;

          key_id = g_strdup_printf ("%s/lfm/power", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "power", NULL, 100.0);
          if (status)
            status = hyscan_data_schema_builder_key_double_range (builder, key_id, 0.0, 100.0, 1.0);
          g_free (key_id);

          if (!status)
            goto exit;
        }
    }

  /* Идентификатор генератора. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->generators, GINT_TO_POINTER (source), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);
  g_clear_pointer (&preset, g_free);

  return id;
}

/* Функция добавляет преднастроенный режим генератора. */
gint
hyscan_sonar_schema_generator_add_preset (HyScanSonarSchema *schema,
                                          HyScanSourceType   source,
                                          const gchar       *name)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *source_name;
  gchar *preset;
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  source_name = hyscan_control_get_source_name (source);
  if (source_name == NULL)
    return -1;

  preset = g_strdup_printf ("%s-generator-preset", source_name);

  id = schema->priv->id_counter++;
  if (!hyscan_data_schema_builder_enum_value_create (builder, preset, id, name, NULL))
    id = -1;

  g_free (preset);

  return id;
}

/* Функция добавляет в схему описание системы ВАРУ. */
gint
hyscan_sonar_schema_tvg_add (HyScanSonarSchema *schema,
                             HyScanSourceType   source,
                             HyScanTVGModeType  capabilities,
                             gdouble            min_gain,
                             gdouble            max_gain)
{
  HyScanDataSchemaBuilder *builder;
  gchar *prefix;
  const gchar *source_name;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  source_name = hyscan_control_get_source_name (source);
  if (source_name == NULL)
    return -1;

  if (!g_hash_table_contains (schema->priv->sources, GINT_TO_POINTER (source)))
    return -1;

  if (g_hash_table_contains (schema->priv->tvgs, GINT_TO_POINTER (source)))
    return -1;

  prefix = g_strdup_printf ("/sources/%s/tvg", source_name);

  /* Режимы работы ВАРУ. */
  key_id = g_strdup_printf ("%s/capabilities", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "capabilities", NULL, capabilities);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Признак включения. */
  key_id = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "enable", NULL, FALSE);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Автоматический режим работы ВАРУ. */
  if (capabilities & HYSCAN_TVG_MODE_AUTO)
    {
      key_id = g_strdup_printf ("%s/auto/level", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "level", NULL, -1.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, -G_MAXDOUBLE, 1.0, 0.1);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/auto/sensitivity", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "sensitivity", NULL, -1.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, -G_MAXDOUBLE, 1.0, 0.1);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Постоянный уровень усиления. */
  if (capabilities & HYSCAN_TVG_MODE_CONSTANT)
    {
      key_id = g_strdup_printf ("%s/constant/gain", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "gain", NULL, min_gain);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, min_gain, max_gain, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Линейное увеличение усиления в дБ / 100 метров. */
  if (capabilities & HYSCAN_TVG_MODE_LINEAR_DB)
    {
      key_id = g_strdup_printf ("%s/linear-db/gain0", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "gain0", NULL, 0.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, -20.0, max_gain, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/linear-db/step", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "step", NULL, 20.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, 0.0, 40.0, 10.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Управление усилением по логарифмическому закону. */
  if (capabilities & HYSCAN_TVG_MODE_LOGARITHMIC)
    {
      key_id = g_strdup_printf ("%s/logarithmic/alpha", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "alpha", NULL, 0.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, -1.0, 1.0, 0.001);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/logarithmic/beta", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "beta", NULL, 10.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, 0.0, 40.0, 10.0);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/logarithmic/gain0", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "gain0", NULL, 0.0);
      if (status)
        status = hyscan_data_schema_builder_key_double_range (builder, key_id, -20.0, max_gain, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Идентификатор ВАРУ. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->tvgs, GINT_TO_POINTER (source), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

/* Функция добавляет в схему описание приёмного канала. */
gint
hyscan_sonar_schema_channel_add (HyScanSonarSchema *schema,
                                 HyScanSourceType   source,
                                 guint              channel,
                                 gdouble            antenna_voffset,
                                 gdouble            antenna_hoffset,
                                 gint               adc_offset,
                                 gfloat             adc_vref)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *source_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  source_name = hyscan_control_get_source_name (source);
  if (source_name == NULL)
    return -1;

  if (channel < 1 || channel > 3)
    return -1;

  if (!g_hash_table_contains (schema->priv->sources, GINT_TO_POINTER (source)))
    return -1;

  prefix = g_strdup_printf ("/sources/%s/channels/%d", source_name, channel);
  if (g_hash_table_contains (schema->priv->channels, GINT_TO_POINTER (g_str_hash (prefix))))
    goto exit;

  /* Вертикальное смещение антенны в блоке. */
  key_id = g_strdup_printf ("%s/antenna/offset/vertical", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "vertical-offset", NULL,  antenna_voffset);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  /* Горизонтальное смещение антенны в блоке. */
  key_id = g_strdup_printf ("%s/antenna/offset/horizontal", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "horizontal-offset", NULL, antenna_hoffset);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Смещение 0 АЦП. */
  key_id = g_strdup_printf ("%s/adc/offset", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "offset", NULL, adc_offset);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Опорное напряжение АЦП. */
  key_id = g_strdup_printf ("%s/adc/vref", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "vref", NULL, adc_vref);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор "шумов". */
  key_id = g_strdup_printf ("%s/noise/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор ВАРУ. */
  key_id = g_strdup_printf ("%s/tvg/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор приёмного канала. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  if (status)
    g_hash_table_insert (schema->priv->channels, GINT_TO_POINTER (g_str_hash (prefix)), NULL);
  else
    id = -1;

  g_free (key_id);

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

/* Функция добавляет в схему описание источника "акустических" данных. */
gint
hyscan_sonar_schema_source_add_acoustic (HyScanSonarSchema *schema,
                                         HyScanSourceType source)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *source_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  source_name = hyscan_control_get_source_name (source);
  if (source_name == NULL)
    return -1;

  if (!g_hash_table_contains (schema->priv->sources, GINT_TO_POINTER (source)))
    return -1;

  if (g_hash_table_contains (schema->priv->acoustics, GINT_TO_POINTER (source)))
    return -1;

  prefix = g_strdup_printf ("/sources/%s/acoustic", source_name);

  /* Идентификатор источника "акустических" данных. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, id);
  if (status)
    status = hyscan_data_schema_builder_key_set_access (builder, key_id, HYSCAN_DATA_SCHEMA_ACCESS_READONLY);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->acoustics, GINT_TO_POINTER (source), NULL);
  else
    id = -1;

  g_clear_pointer (&prefix, g_free);

  return id;
}
