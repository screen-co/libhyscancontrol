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
};

static void    hyscan_sonar_schema_object_constructed          (GObject                       *object);

static void    hyscan_sonar_schema_enum_add_port_type          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_port_protocol      (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_port_status        (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_ip_port_addresses  (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_uart_devs          (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_uart_modes         (HyScanSonarSchema             *schema);

static void    hyscan_sonar_schema_enum_add_signal_type        (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_board_type         (HyScanSonarSchema             *schema);
static void    hyscan_sonar_schema_enum_add_source_type        (HyScanSonarSchema             *schema);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarSchema, hyscan_sonar_schema, HYSCAN_TYPE_DATA_SCHEMA_BUILDER)

static void
hyscan_sonar_schema_class_init (HyScanSonarSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_sonar_schema_object_constructed;
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

  G_OBJECT_CLASS (hyscan_sonar_schema_parent_class)->constructed (object);

  priv->id_counter = 1;

  /* Версия и идентификатор схемы данных гидролокатора. */
  hyscan_data_schema_builder_key_integer_create (builder, "/schema/id",
                                                 "Sonar schema id", NULL, TRUE,
                                                 HYSCAN_SONAR_SCHEMA_ID,
                                                 HYSCAN_SONAR_SCHEMA_ID,
                                                 HYSCAN_SONAR_SCHEMA_ID,
                                                 0);
  hyscan_data_schema_builder_key_integer_create (builder, "/schema/version",
                                                 "Sonar schema version", NULL, TRUE,
                                                 HYSCAN_SONAR_SCHEMA_VERSION,
                                                 HYSCAN_SONAR_SCHEMA_VERSION,
                                                 HYSCAN_SONAR_SCHEMA_VERSION,
                                                 0);

  /* Enum значения. */
  hyscan_sonar_schema_enum_add_port_type (schema);
  hyscan_sonar_schema_enum_add_port_protocol (schema);
  hyscan_sonar_schema_enum_add_port_status (schema);
  hyscan_sonar_schema_enum_add_ip_port_addresses (schema);
  hyscan_sonar_schema_enum_add_uart_devs (schema);
  hyscan_sonar_schema_enum_add_uart_modes (schema);
  hyscan_sonar_schema_enum_add_signal_type (schema);
  hyscan_sonar_schema_enum_add_source_type (schema);
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

  hyscan_data_schema_builder_key_enum_create (builder, "/enums/ip-address", "ip-address", NULL,
                                              FALSE, "ip-address", 0);
}

/* Функция создаёт enum значение uart-dev. */
static void
hyscan_sonar_schema_enum_add_uart_devs (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "uart-device");

  hyscan_data_schema_builder_enum_value_create (builder, "uart-device", 0, "Disabled", NULL);

  hyscan_data_schema_builder_key_enum_create (builder, "/enums/uart-device", "uart-device", NULL,
                                              FALSE, "uart-device", 0);
}

/* Функция создаёт enum значение uart-mode. */
static void
hyscan_sonar_schema_enum_add_uart_modes (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "uart-mode");

  hyscan_data_schema_builder_enum_value_create (builder, "uart-mode", 0, "Disabled", NULL);

  hyscan_data_schema_builder_key_enum_create (builder, "/enums/uart-mode", "uart-mode", NULL,
                                              FALSE, "uart-mode", 0);
}

/* Функция создаёт enum значение signal-type. */
static void
hyscan_sonar_schema_enum_add_signal_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "signal-type");

  hyscan_data_schema_builder_enum_value_create (builder, "signal-type",
                                                0,
                                                "Disabled", NULL);
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

/* Функция создаёт enum значение board-type. */
static void
hyscan_sonar_schema_enum_add_board_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "board-type");

  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_STARBOARD,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_STARBOARD),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_PORT,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_PORT),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_STARBOARD_HI,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_STARBOARD_HI),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_PORT_HI,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_PORT_HI),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_ECHOSOUNDER,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_ECHOSOUNDER),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_PROFILER,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_PROFILER),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_LOOK_AROUND,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_LOOK_AROUND),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "board-type",
                                                HYSCAN_BOARD_FORWARD_LOOK,
                                                hyscan_control_get_board_name (HYSCAN_BOARD_FORWARD_LOOK),
                                                NULL);
}

/* Функция создаёт enum значение source-type. */
static void
hyscan_sonar_schema_enum_add_source_type (HyScanSonarSchema *schema)
{
  HyScanDataSchemaBuilder *builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  hyscan_data_schema_builder_enum_create (builder, "source-type");

  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_PORT),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_PORT_HI),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_BATHYMETRY_STARBOARD,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_BATHYMETRY_STARBOARD),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_BATHYMETRY_PORT,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_BATHYMETRY_PORT),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_ECHOSOUNDER,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_ECHOSOUNDER),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_PROFILER,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_PROFILER),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_LOOK_AROUND,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_LOOK_AROUND),
                                                NULL);
  hyscan_data_schema_builder_enum_value_create (builder, "source-type",
                                                HYSCAN_SOURCE_FORWARD_LOOK,
                                                hyscan_control_get_source_name (HYSCAN_SOURCE_FORWARD_LOOK),
                                                NULL);
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
  gboolean status = FALSE;
  gchar *prefix = NULL;
  gchar *key = NULL;
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

  priv = schema->priv;
  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  prefix = g_strdup_printf ("/sensors/%s", name);

  /* Тип порта. */
  key = g_strdup_printf ("%s/type", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key, "type", NULL, TRUE,
                                                       "port-type", type);
  g_free (key);

  if (!status)
    goto exit;

  /* Формат данных. */
  key = g_strdup_printf ("%s/protocol", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key, "protocol", NULL,
                                                       type == HYSCAN_SENSOR_PORT_VIRTUAL ? TRUE : FALSE,
                                                       "port-protocol", protocol);
  g_free (key);

  if (!status)
    goto exit;

  /* Состояние порта */
  key = g_strdup_printf ("%s/status", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key, "status", NULL, FALSE,
                                                       "port-status", HYSCAN_SENSOR_PORT_STATUS_DISABLED);
  g_free (key);

  if (!status)
    goto exit;

  /* Признак включения. */
  key = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key, "enable", NULL, FALSE, FALSE);
  g_free (key);

  if (!status)
    goto exit;

  /* Дополнительные параметры UDP/IP порта. */
  if (type == HYSCAN_SENSOR_PORT_UDP_IP)
    {
      /* IP адрес. */
      key = g_strdup_printf ("%s/ip-address", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key, "ip-address", NULL, FALSE,
                                                           "ip-address", 0);
      g_free (key);

      if (!status)
        goto exit;

      /* UDP порт. */
      key = g_strdup_printf ("%s/udp-port", prefix);
      status = hyscan_data_schema_builder_key_integer_create (builder, key, "udp-port", NULL, FALSE,
                                                              10000, 1024, 66535, 1);
      g_free (key);

      if (!status)
        goto exit;
    }

  /* Дополнительные параметры UART порта. */
  if (type == HYSCAN_SENSOR_PORT_UART)
    {
      /* Физическое устройство. */
      key = g_strdup_printf ("%s/uart-device", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key, "uart-device", NULL, FALSE,
                                                           "uart-device", 0);
      g_free (key);

      if (!status)
        goto exit;

      /* Режим работы. */
      key = g_strdup_printf ("%s/uart-mode", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key, "uart-mode", NULL, FALSE,
                                                           "uart-mode", 0);
      g_free (key);

      if (!status)
        goto exit;
    }

  /* Идентификатор порта. */
  key = g_strdup_printf ("%s/id", prefix);
  id = priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key, "id", NULL, TRUE,
                                                          id, id, id, 1);
  g_free (key);

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
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

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
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

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
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  id = schema->priv->id_counter++;

  if (!hyscan_data_schema_builder_enum_value_create (builder, "ip-address", id, name, NULL))
    id = -1;

  return id;
}

/* Функция добавляет в схему описание борта гидролокатора. */
gint
hyscan_sonar_schema_board_add (HyScanSonarSchema *schema,
                               HyScanBoardType    board)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gboolean status = FALSE;
  gchar *prefix = NULL;
  gchar *key = NULL;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return FALSE;

  prefix = g_strdup_printf ("/boards/%s/", board_name);

  /* Тип борта гидролокатора. */
  key = g_strdup_printf ("%s/type", prefix);
  status = hyscan_data_schema_builder_key_enum_create (builder, key, "type", NULL, TRUE,
                                                       "board-type", board);
  g_free (key);

  if (!status)
    goto exit;

  /* Идентификатор борта гидролокатора. */
  key = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key, "id", NULL, TRUE,
                                                          id, id, id, 1);
  g_free (key);

  if (!status)
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
                                   gdouble                    low_frequency,
                                   gdouble                    high_frequency,
                                   gdouble                    max_duration)
{
  HyScanDataSchemaBuilder *builder;
  const gchar *board_name;
  gboolean status = FALSE;
  gchar *prefix = NULL;
  gchar *preset = NULL;
  gchar *key = NULL;
  gint32 id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);

  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return FALSE;

  prefix = g_strdup_printf ("/boards/%s/generator", board_name);

  /* Режимы работы генератора. */
  key = g_strdup_printf ("%s/capabilities", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key, "capabilities", NULL, TRUE,
                                                          capabilities,
                                                          0, G_MAXINT64,
                                                          0);
  g_free (key);

  if (!status)
    goto exit;

  /* Возможные сигналы. */
  key = g_strdup_printf ("%s/signals", prefix);
  status = hyscan_data_schema_builder_key_integer_create (builder, key, "signals", NULL, TRUE,
                                                          signals,
                                                          0, G_MAXINT64,
                                                          0);
  g_free (key);

  if (!status)
    goto exit;

  /* Признак включения. */
  key = g_strdup_printf ("%s/enable", prefix);
  status = hyscan_data_schema_builder_key_boolean_create (builder, key, "enable", NULL, FALSE, FALSE);
  g_free (key);

  if (!status)
    goto exit;

  /* Автоматический режим работы генератора. */
  if (capabilities | HYSCAN_GENERATOR_MODE_AUTO)
    {
      key = g_strdup_printf ("%s/auto/signal", prefix);
      status =  hyscan_data_schema_builder_key_enum_create (builder, key, "signal", NULL, FALSE,
                                                            "signal-type", 0);
      g_free (key);

      if (!status)
        goto exit;
    }

  /* Преднастройки генератора. */
  if (capabilities | HYSCAN_GENERATOR_MODE_PRESET)
    {
      preset = g_strdup_printf ("%s-generator-preset", board_name);

      status = hyscan_data_schema_builder_enum_create (builder, preset);
      if (!status)
        goto exit;

      status = hyscan_data_schema_builder_enum_value_create (builder, preset, 0, "Disabled", NULL);
      if (!status)
        goto exit;

      key = g_strdup_printf ("/enums/%s", preset);
      hyscan_data_schema_builder_key_enum_create (builder, key, preset, NULL,
                                                  FALSE, preset, 0);
      g_free (key);

      if (!status)
        goto exit;

      key = g_strdup_printf ("%s/preset/id", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key, "id", NULL, FALSE,
                                                           preset, 0);
      g_free (key);

      if (!status)
        goto exit;

      g_clear_pointer (&preset, g_free);
    }

  /* Упрощённый режим управления генератором. */
  if (capabilities | HYSCAN_GENERATOR_MODE_SIMPLE)
    {
      key = g_strdup_printf ("%s/simple/signal", prefix);
      status = hyscan_data_schema_builder_key_enum_create (builder, key, "signal", NULL, FALSE,
                                                           "signal-type", 0);
      g_free (key);

      if (!status)
        goto exit;

      key = g_strdup_printf ("%s/simple/power", prefix);
      status = hyscan_data_schema_builder_key_double_create (builder, key, "power", NULL, FALSE,
                                                             100.0,
                                                             0.0, 100.0,
                                                             1.0);
      g_free (key);

      if (!status)
        goto exit;
    }

  /* Расширенный режим управления генератором. */
  if (capabilities | HYSCAN_GENERATOR_MODE_EXTENDED)
    {
      if (signals | HYSCAN_GENERATOR_SIGNAL_TONE)
        {
          gdouble frequency = low_frequency + (high_frequency - low_frequency) / 2.0;
          gdouble duration = 10.0 / frequency;

          key = g_strdup_printf ("%s/tone/frequency", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "frequency", NULL, FALSE,
                                                                 frequency,
                                                                 low_frequency, high_frequency,
                                                                 1.0);
          g_free (key);

          if (!status)
            goto exit;

          key = g_strdup_printf ("%s/tone/duration", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "duration", NULL, FALSE,
                                                                 duration,
                                                                 0.0, max_duration,
                                                                 0.00001);
          g_free (key);

          if (!status)
            goto exit;

          key = g_strdup_printf ("%s/tone/power", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "power", NULL, FALSE,
                                                                 100.0,
                                                                 0.0, 100.0,
                                                                 1.0);
          g_free (key);

          if (!status)
            goto exit;
        }

      if ((signals | HYSCAN_GENERATOR_SIGNAL_LFM) || (signals | HYSCAN_GENERATOR_SIGNAL_LFMD))
        {
          key = g_strdup_printf ("%s/lfm/decreasing", prefix);
          status = hyscan_data_schema_builder_key_boolean_create (builder, key, "decreasing", NULL, FALSE, FALSE);
          g_free (key);

          if (!status)
            goto exit;

          key = g_strdup_printf ("%s/lfm/low-frequency", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "low-frequency", NULL, FALSE,
                                                                 low_frequency,
                                                                 low_frequency, high_frequency,
                                                                 1.0);
          g_free (key);

          if (!status)
            goto exit;

          key = g_strdup_printf ("%s/lfm/high-frequency", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "high-frequency", NULL, FALSE,
                                                                 high_frequency,
                                                                 low_frequency, high_frequency,
                                                                 1.0);
          g_free (key);

          if (!status)
            goto exit;

          key = g_strdup_printf ("%s/lfm/duration", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "duration", NULL, FALSE,
                                                                 0.001,
                                                                 0.0, max_duration,
                                                                 0.0005);
          g_free (key);

          if (!status)
            goto exit;

          key = g_strdup_printf ("%s/lfm/power", prefix);
          status = hyscan_data_schema_builder_key_double_create (builder, key, "power", NULL, FALSE,
                                                                 100.0,
                                                                 0.0, 100.0,
                                                                 1.0);
          g_free (key);

          if (!status)
            goto exit;
        }
    }

  /* Идентификатор генератора. */
  key = g_strdup_printf ("%s/id", prefix);
  id = schema->priv->id_counter++;
  status = hyscan_data_schema_builder_key_integer_create (builder, key, "id", NULL, TRUE,
                                                          id, id, id, 1);
  g_free (key);

  if (!status)
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
  gchar *preset = NULL;
  gint id = -1;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SCHEMA (schema), id);

  builder = HYSCAN_DATA_SCHEMA_BUILDER (schema);
  board_name = hyscan_control_get_board_name (board);
  if (board_name == NULL)
    return FALSE;

  preset = g_strdup_printf ("%s-generator-preset", board_name);

  id = schema->priv->id_counter++;
  if (!hyscan_data_schema_builder_enum_value_create (builder, preset, id, name, NULL))
    id = -1;

  g_free (preset);

  return id;
}

