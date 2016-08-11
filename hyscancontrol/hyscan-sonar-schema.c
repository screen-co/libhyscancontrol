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

struct _HyScanSonarSchemaPrivate
{
  gint                         id_counter;                     /* Счётчик идентификаторов объектов. */

  GHashTable                  *boards;                         /* Наличие бортов. */
  GHashTable                  *generators;                     /* Наличие генераторов. */
  GHashTable                  *tvgs;                           /* Наличие ВАРУ. */
  GHashTable                  *channels;                       /* Наличие приёмного канала. */
  GHashTable                  *acoustics;                      /* Наличие источника "акустических" данных. */
};

static void    hyscan_sonar_schema_object_constructed          (GObject                       *object);
static void    hyscan_sonar_schema_object_finalize             (GObject                       *object);

static void    hyscan_sonar_schema_enum_add_port_type          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_port_protocol      (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_port_status        (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_ip_port_addresses  (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_uart_devs          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_uart_modes         (HyScanSonarSchema             *schema);

static void    hyscan_sonar_schema_enum_add_sync_type          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_signal_type        (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_discretization_type(HyScanSonarSchema             *schema);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarSchema, hyscan_sonar_schema, HYSCAN_TYPE_DATA_SCHEMA_BUILDER)

static void
hyscan_sonar_schema_class_init (HyScanSonarSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_sonar_schema_object_constructed;
  object_class->finalize = hyscan_sonar_schema_object_finalize;
}

static void
hyscan_sonar_schema_init (HyScanSonarSchema *schema)
{
  schema->priv = hyscan_sonar_schema_get_instance_private (schema);
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

  priv->boards = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->generators = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->tvgs = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->channels = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->acoustics = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Enum значения. */
  hyscan_sonar_schema_enum_add_port_type (schema);
  hyscan_sonar_schema_enum_add_port_protocol (schema);
  hyscan_sonar_schema_enum_add_port_status (schema);
  hyscan_sonar_schema_enum_add_ip_port_addresses (schema);
  hyscan_sonar_schema_enum_add_uart_devs (schema);
  hyscan_sonar_schema_enum_add_uart_modes (schema);
  hyscan_sonar_schema_enum_add_sync_type (schema);
  hyscan_sonar_schema_enum_add_signal_type (schema);
  hyscan_sonar_schema_enum_add_discretization_type (schema);

  /* Версия и идентификатор схемы данных гидролокатора. */
  hyscan_data_schema_builder_key_integer_create (builder, "/schema/id",
                                                 "id", "Sonar schema id", TRUE,
                                                 HYSCAN_SONAR_SCHEMA_ID,
                                                 HYSCAN_SONAR_SCHEMA_ID,
                                                 HYSCAN_SONAR_SCHEMA_ID,
                                                 0);
  hyscan_data_schema_builder_key_integer_create (builder, "/schema/version",
                                                 "version", "Sonar schema version", TRUE,
                                                 HYSCAN_SONAR_SCHEMA_VERSION,
                                                 HYSCAN_SONAR_SCHEMA_VERSION,
                                                 HYSCAN_SONAR_SCHEMA_VERSION,
                                                 0);

  /* Параметры управления. */
  hyscan_data_schema_builder_key_boolean_create (builder, "/control/alive",
                                                 "alive", "Sonar breaker", FALSE,
                                                 FALSE);
  hyscan_data_schema_builder_key_boolean_create (builder, "/control/enable",
                                                 "enable", "Enable sonar", FALSE,
                                                 FALSE);
  hyscan_data_schema_builder_key_string_create  (builder, "/control/project-name",
                                                 "project-name", "Project name", FALSE,
                                                 NULL);
  hyscan_data_schema_builder_key_string_create  (builder, "/control/track-name",
                                                 "track-name", "Track name", FALSE,
                                                 NULL);
  hyscan_data_schema_builder_key_boolean_create (builder, "/control/raw-data",
                                                 "raw-data", "Enable raw data", FALSE,
                                                 FALSE);

  /* Идентификатор для сообщений от гидролокатора. */
  id = schema->priv->id_counter++;
  hyscan_data_schema_builder_key_integer_create (builder, "/id",
                                                 "id", "ID", TRUE,
                                                 id, id, id, 0);
}

static void
hyscan_sonar_schema_object_finalize (GObject *object)
{
  HyScanSonarSchema *schema = HYSCAN_SONAR_SCHEMA (object);

  g_hash_table_unref (schema->priv->boards);
  g_hash_table_unref (schema->priv->generators);
  g_hash_table_unref (schema->priv->tvgs);
  g_hash_table_unref (schema->priv->channels);
  g_hash_table_unref (schema->priv->acoustics);

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
hyscan_sonar_schema_enum_add_ip_port_addresses (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "ip-address");

  hyscan_data_schema_builder_enum_value_create (builder, "ip-address", 0, "Disabled", NULL);

  hyscan_data_schema_builder_key_enum_create (builder, "/sensors/ip-address", "ip-address", NULL,
                                              FALSE, "ip-address", 0);
}

/* Функция создаёт enum значение uart-dev. */
static void
hyscan_sonar_schema_enum_add_uart_devs (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "uart-device");

  hyscan_data_schema_builder_enum_value_create (builder, "uart-device", 0, "Disabled", NULL);

  hyscan_data_schema_builder_key_enum_create (builder, "/sensors/uart-device", "uart-device", NULL,
                                              FALSE, "uart-device", 0);
}

/* Функция создаёт enum значение uart-mode. */
static void
hyscan_sonar_schema_enum_add_uart_modes (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "uart-mode");

  hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", 0, "Disabled", NULL);

  hyscan_data_schema_builder_key_enum_create (builder, "/sensors/uart-mode", "uart-mode", NULL,
                                              FALSE, "uart-mode", 0);
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

/* Функция создаёт enum значение discretization-type. */
static void
hyscan_sonar_schema_enum_add_discretization_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "discretization-type");

  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_ADC_14LE,
                                                "ADC 14LE", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_ADC_16LE,
                                                "ADC 16LE", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_ADC_24LE,
                                                "ADC 24LE", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_COMPLEX_ADC_14LE,
                                                "ADC 14LE COMPLEX", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_COMPLEX_ADC_16LE,
                                                "ADC 16LE COMPLEX", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_COMPLEX_ADC_24LE,
                                                "ADC 24LE COMPLEX", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_FLOAT,
                                                "FLOAT", NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "discretization-type",
                                                HYSCAN_DATA_COMPLEX_FLOAT,
                                                "FLOAT COMPLEX", NULL);
}

/* Функция создаёт новый объект HyScanSonarSchema. */
HyScanSonarSchema *
hyscan_sonar_schema_new (void)
{
  return g_object_new (HYSCAN_TYPE_SONAR_SCHEMA, "schema-id", "sonar", NULL);
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
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "type", NULL, TRUE,
                                                       "port-type", type);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Формат данных. */
  key_id = g_strdup_printf ("%s/protocol", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "protocol", NULL,
                                                       type == HYSCAN_SENSOR_PORT_VIRTUAL ? TRUE : FALSE,
                                                       "port-protocol", protocol);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Состояние порта */
  key_id = g_strdup_printf ("%s/status", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "status", NULL, FALSE,
                                                       "port-status", HYSCAN_SENSOR_PORT_STATUS_DISABLED);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Признак включения. */
  key_id = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "enable", NULL, FALSE, FALSE);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Дополнительные параметры UDP/IP порта. */
  if (type == HYSCAN_SENSOR_PORT_UDP_IP)
    {
      /* IP адрес. */
      key_id = g_strdup_printf ("%s/ip-address", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "ip-address", NULL, FALSE,
                                                           "ip-address", 0);
      g_free (key_id);

      if (!status)
        goto exit;

      /* UDP порт. */
      key_id = g_strdup_printf ("%s/udp-port", prefix);
      status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "udp-port", NULL, FALSE,
                                                              10000, 1024, 66535, 1);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Дополнительные параметры UART порта. */
  if (type == HYSCAN_SENSOR_PORT_UART)
    {
      /* Физическое устройство. */
      key_id = g_strdup_printf ("%s/uart-device", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "uart-device", NULL, FALSE,
                                                           "uart-device", 0);
      g_free (key_id);

      if (!status)
        goto exit;

      /* Режим работы. */
      key_id = g_strdup_printf ("%s/uart-mode", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "uart-mode", NULL, FALSE,
                                                           "uart-mode", 0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Идентификатор порта. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, TRUE,
                                                          id, id, id, 0);
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
                                            const gchar       *name)
{
  HyScanDataSchemaBuilder *builder;
  gint id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, "uart-device", id, name, NULL))
    id = -1;

  return id;
}

/* Функция добавляет режим работы UART устройства в список допустимых для UART датчика. */
gint
hyscan_sonar_schema_sensor_add_uart_mode (HyScanSonarSchema *schema,
                                            const gchar       *name)
{
  HyScanDataSchemaBuilder *builder;
  gint id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", id, name, NULL))
    id= -1;

  return id;
}

/* Функция добавляет IP адрес в список допустимых для IP датчика. */
gint
hyscan_sonar_schema_sensor_add_ip_address (HyScanSonarSchema *schema,
                                           const gchar       *name)
{
  HyScanDataSchemaBuilder *builder;
  gint id;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, "ip-address", id, name, NULL))
    id = -1;

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
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "capabilities", NULL, TRUE,
                                                          capabilities,
                                                          capabilities,
                                                          capabilities,
                                                          0);
  if (!status)
    return FALSE;

  key_id = "/sync/type";
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "type", NULL, FALSE,
                                                       "sync-type", 0);
  if (!status)
    return FALSE;

  if (capabilities & HYSCAN_SONAR_SYNC_SOFTWARE)
    {
      key_id = "/sync/ping";
      status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "ping", NULL, FALSE,
                                                              FALSE);
      if (!status)
        return FALSE;
    }

  return TRUE;
}

/* Функция добавляет в схему описание борта гидролокатора. */
gint
hyscan_sonar_schema_board_add (HyScanSonarSchema *schema,
                               HyScanBoardType    board,
                               gdouble            vertical_pattern,
                               gdouble            horizontal_pattern,
                               gdouble            max_receive_time)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return -1;

  if (g_hash_table_contains (schema->priv->boards, GINT_TO_POINTER (board)))
    return -1;

  prefix = g_strdup_printf ("/boards/%s", board_name);

  /* Диаграмма направленности в вертикальной плоскости. */
  key_id = g_strdup_printf ("%s/info/pattern/vertical", prefix);
  status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "vertical-pattern", NULL, TRUE,
                                                          vertical_pattern,
                                                          vertical_pattern,
                                                          vertical_pattern,
                                                          0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Диаграмма направленности в горизонтальной плоскости. */
  key_id = g_strdup_printf ("%s/info/pattern/horizontal", prefix);
  status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "horizontal-pattern", NULL, TRUE,
                                                          horizontal_pattern,
                                                          horizontal_pattern,
                                                          horizontal_pattern,
                                                          0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Время приёма данных бортом. */
  key_id = g_strdup_printf ("%s/control/receive-time", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "receive-time", NULL, FALSE,
                                                         0.0, 0.0, max_receive_time, 0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор борта гидролокатора. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, TRUE,
                                                          id, id, id, 0);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->boards, GINT_TO_POINTER (board), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

/* Функция добавляет описание генератора для борта. */
gint
hyscan_sonar_schema_generator_add (HyScanSonarSchema         *schema,
                                   HyScanBoardType            board,
                                   HyScanGeneratorModeType    capabilities,
                                   HyScanGeneratorSignalType  signals,
                                   gdouble                    min_tone_duration,
                                   gdouble                    max_tone_duration,
                                   gdouble                    min_lfm_duration,
                                   gdouble                    max_lfm_duration)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gchar *preset = NULL;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return -1;

  if (!g_hash_table_contains (schema->priv->boards, GINT_TO_POINTER (board)))
    return -1;

  if (g_hash_table_contains (schema->priv->generators, GINT_TO_POINTER (board)))
    return -1;

  prefix = g_strdup_printf ("/boards/%s/generator", board_name);

  /* Режимы работы генератора. */
  key_id = g_strdup_printf ("%s/capabilities", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "capabilities", NULL, TRUE,
                                                          capabilities, capabilities, capabilities, 0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Возможные сигналы. */
  key_id = g_strdup_printf ("%s/signals", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "signals", NULL, TRUE,
                                                          signals, signals, signals, 0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Признак включения. */
  key_id = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "enable", NULL, FALSE, FALSE);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Автоматический режим работы генератора. */
  if (capabilities & HYSCAN_GENERATOR_MODE_AUTO)
    {
      key_id = g_strdup_printf ("%s/auto/signal", prefix);
      status =  hyscan_data_schema_builder_key_enum_create (builder, key_id, "signal", NULL, FALSE,
                                                            "signal-type", 0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Преднастройки генератора. */
  if (capabilities & HYSCAN_GENERATOR_MODE_PRESET)
    {
      preset = g_strdup_printf ("%s-generator-preset", board_name);

      status = hyscan_data_schema_builder_enum_create (builder, preset);
      if (!status)
        goto exit;

      status = hyscan_data_schema_builder_enum_value_create (builder, preset, 0, "Disabled", NULL);
      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/preset/id", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "id", NULL, FALSE,
                                                           preset, 0);
      g_free (key_id);

      if (!status)
        goto exit;

      g_clear_pointer (&preset, g_free);
    }

  /* Упрощённый режим управления генератором. */
  if (capabilities & HYSCAN_GENERATOR_MODE_SIMPLE)
    {
      key_id = g_strdup_printf ("%s/simple/signal", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "signal", NULL, FALSE,
                                                           "signal-type", 0);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/simple/power", prefix);
      status = hyscan_data_schema_builder_key_double_create (builder, key_id, "power", NULL, FALSE,
                                                             100.0, 0.0, 100.0, 1.0);
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
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "duration", NULL, FALSE,
                                                                 min_tone_duration,
                                                                 min_tone_duration,
                                                                 max_tone_duration,
                                                                 0.00001);
          g_free (key_id);

          if (!status)
            goto exit;

          key_id = g_strdup_printf ("%s/tone/power", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "power", NULL, FALSE,
                                                                 100.0, 0.0, 100.0, 1.0);
          g_free (key_id);

          if (!status)
            goto exit;
        }

      if ((signals & HYSCAN_GENERATOR_SIGNAL_LFM) || (signals & HYSCAN_GENERATOR_SIGNAL_LFMD))
        {
          key_id = g_strdup_printf ("%s/lfm/decreasing", prefix);
          status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "decreasing", NULL, FALSE, FALSE);
          g_free (key_id);

          if (!status)
            goto exit;

          key_id = g_strdup_printf ("%s/lfm/duration", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "duration", NULL, FALSE,
                                                                 min_lfm_duration,
                                                                 min_lfm_duration,
                                                                 max_lfm_duration,
                                                                 0.0001);
          g_free (key_id);

          if (!status)
            goto exit;

          key_id = g_strdup_printf ("%s/lfm/power", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key_id, "power", NULL, FALSE,
                                                                 100.0, 0.0, 100.0, 1.0);
          g_free (key_id);

          if (!status)
            goto exit;
        }
    }

  /* Идентификатор генератора. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, TRUE,
                                                          id, id, id, 0);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->generators, GINT_TO_POINTER (board), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);
  g_clear_pointer (&preset, g_free);

  return id;
}

/* Функция добавляет преднастроенный режим генератора. */
gint
hyscan_sonar_schema_generator_add_preset (HyScanSonarSchema    *schema,
                                          HyScanBoardType       board,
                                          const gchar          *name)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gchar *preset;
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return -1;

  preset = g_strdup_printf ("%s-generator-preset", board_name);

  id = schema->priv->id_counter++;
  if (!hyscan_data_schema_builder_enum_value_create (builder, preset, id, name, NULL))
    id = -1;

  g_free (preset);

  return id;
}

/* Функция добавляет в схему описание системы ВАРУ для борта. */
gint
hyscan_sonar_schema_tvg_add (HyScanSonarSchema *schema,
                             HyScanBoardType    board,
                             HyScanTVGModeType  capabilities,
                             gdouble            min_gain,
                             gdouble            max_gain)
{
  HyScanDataSchemaBuilder *builder;
  gchar *prefix;
  const gchar *board_name;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return -1;

  if (!g_hash_table_contains (schema->priv->boards, GINT_TO_POINTER (board)))
    return -1;

  if (g_hash_table_contains (schema->priv->tvgs, GINT_TO_POINTER (board)))
    return -1;

  prefix = g_strdup_printf ("/boards/%s/tvg", board_name);

  /* Режимы работы ВАРУ. */
  key_id = g_strdup_printf ("%s/capabilities", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "capabilities", NULL, TRUE,
                                                          capabilities, capabilities, capabilities, 0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Признак включения. */
  key_id = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key_id, "enable", NULL, FALSE, FALSE);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Автоматический режим работы ВАРУ. */
  if (capabilities & HYSCAN_TVG_MODE_AUTO)
    {
      key_id = g_strdup_printf ("%s/auto/level", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "level", NULL, FALSE,
                                                              0.9, 0.0, 1.0, 0.1);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/auto/sensitivity", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "sensitivity", NULL, FALSE,
                                                              0.6, 0.0, 1.0, 0.1);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Постоянный уровень усиления. */
  if (capabilities & HYSCAN_TVG_MODE_CONSTANT)
    {
      key_id = g_strdup_printf ("%s/constant/gain", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "gain", NULL, FALSE,
                                                              min_gain, min_gain, max_gain, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Линейное увеличение усиления в дБ / 100 метров. */
  if (capabilities & HYSCAN_TVG_MODE_LINEAR_DB)
    {
      key_id = g_strdup_printf ("%s/linear-db/gain0", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "gain0", NULL, FALSE,
                                                              min_gain, min_gain, max_gain, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/linear-db/step", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "step", NULL, FALSE,
                                                              20.0, 0.0, 100.0, 10.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Управление усилением по логарифмическому закону. */
  if (capabilities & HYSCAN_TVG_MODE_LOGARITHMIC)
    {
      key_id = g_strdup_printf ("%s/logarithmic/alpha", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "alpha", NULL, FALSE,
                                                              0.02, 0.0, 1.0, 0.001);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/logarithmic/beta", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "beta", NULL, FALSE,
                                                              10.0, 0.0, 100.0, 10.0);
      g_free (key_id);

      if (!status)
        goto exit;

      key_id = g_strdup_printf ("%s/logarithmic/gain0", prefix);
      status =  hyscan_data_schema_builder_key_double_create (builder, key_id, "gain0", NULL, FALSE,
                                                              min_gain, min_gain, max_gain, 1.0);
      g_free (key_id);

      if (!status)
        goto exit;
    }

  /* Идентификатор ВАРУ. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, TRUE,
                                                          id, id, id, 0);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->tvgs, GINT_TO_POINTER (board), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

/* Функция добавляет в схему описание приёмного канала борта. */
gint
hyscan_sonar_schema_channel_add (HyScanSonarSchema *schema,
                                 HyScanBoardType    board,
                                 gint               index,
                                 HyScanDataType     discretization_type,
                                 gfloat             discretization_frequency)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return -1;

  if (index < 1 || index > 3)
    return -1;

  if (!g_hash_table_contains (schema->priv->boards, GINT_TO_POINTER (board)))
    return -1;

  prefix = g_strdup_printf ("/boards/%s/sources/channel%d", board_name, index);
  if (g_hash_table_contains (schema->priv->channels, GINT_TO_POINTER (g_str_hash (prefix))))
    goto exit;

  /* Тип дискретизации данных. */
  key_id = g_strdup_printf ("%s/discretization/type", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "discretization-type", NULL, TRUE,
                                                       "discretization-type", discretization_type);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Частота дискретизации данных. */
  key_id = g_strdup_printf ("%s/discretization/frequency", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "discretization-frequency", NULL, TRUE,
                                                         discretization_frequency,
                                                         discretization_frequency,
                                                         discretization_frequency,
                                                         0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор приёмного канала. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, TRUE,
                                                          id, id, id, 0);

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
hyscan_sonar_schema_source_add_acuostic (HyScanSonarSchema *schema,
                                         HyScanBoardType    board,
                                         HyScanDataType     discretization_type,
                                         gfloat             discretization_frequency)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gchar *prefix;
  gboolean status;
  gchar *key_id;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), -1);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return -1;

  if (!g_hash_table_contains (schema->priv->boards, GINT_TO_POINTER (board)))
    return -1;

  if (g_hash_table_contains (schema->priv->acoustics, GINT_TO_POINTER (board)))
    return -1;

  prefix = g_strdup_printf ("/boards/%s/sources/acoustic", board_name);

  /* Тип дискретизации данных. */
  key_id = g_strdup_printf ("%s/discretization/type", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key_id, "discretization-type", NULL, TRUE,
                                                       "discretization-type", discretization_type);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Частота дискретизации данных. */
  key_id = g_strdup_printf ("%s/discretization/frequency", prefix);
  status = hyscan_data_schema_builder_key_double_create (builder, key_id, "discretization-frequency", NULL, TRUE,
                                                         discretization_frequency,
                                                         discretization_frequency,
                                                         discretization_frequency,
                                                         0.0);
  g_free (key_id);

  if (!status)
    goto exit;

  /* Идентификатор источника "акустических" данных. */
  key_id = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key_id, "id", NULL, TRUE,
                                                          id, id, id, 0);
  g_free (key_id);

  if (status)
    g_hash_table_insert (schema->priv->acoustics, GINT_TO_POINTER (board), NULL);
  else
    id = -1;

exit:
  g_clear_pointer (&prefix, g_free);

  return id;
}

#warning "make magic constants tunable"
