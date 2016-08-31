/*
 * \file hyscan-sonar-sensor.c
 *
 * \brief Исходный файл класса управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <hyscan-core-types.h>
#include "hyscan-control-common.h"
#include "hyscan-sensor-control.h"
#include "hyscan-marshallers.h"
#include <string.h>

#define HYSCAN_SENSOR_CONTROL_MAX_CHANNELS     5

enum
{
  PROP_O,
  PROP_SONAR
};

enum
{
  SIGNAL_SENSOR_DATA,
  SIGNAL_LAST
};

/* Описание порта. */
typedef struct
{
  gchar                       *name;                           /* Название порта. */
  gchar                       *path;                           /* Путь к описанию порта в схеме. */
  HyScanSensorPortType         type;                           /* Тип порта. */
  HyScanSensorProtocolType     protocol;                       /* Протокол передачи данных. */
  gint64                       time_offset;                    /* Коррекция времени. */
  guint                        channel;                        /* Номер канала данных. */
} HyScanSensorControlPort;

struct _HyScanSensorControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */
  gulong                       signal_id;                      /* Идентификатор обработчика сигнала data. */

  GHashTable                  *ports_by_id;                    /* Список портов для подключения датчиков. */
  GHashTable                  *ports_by_name;                  /* Список портов для подключения датчиков. */

  GRWLock                      lock;                           /* Блокировка. */
};

static void          hyscan_sensor_control_set_property        (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void          hyscan_sensor_control_object_constructed  (GObject                   *object);
static void          hyscan_sensor_control_object_finalize     (GObject                   *object);

static void          hyscan_sensor_control_data_receiver       (HyScanSensorControl       *control,
                                                                HyScanSonarMessage        *message);

static gboolean      hyscan_sensor_control_check_nmea_crc      (const gchar               *nmea_str);

static HyScanSourceType hyscan_sensor_control_get_source_type  (const gchar               *nmea_str);

static void          hyscan_sensor_control_free_port           (gpointer                   data);

static guint         hyscan_sensor_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSensorControl, hyscan_sensor_control, HYSCAN_TYPE_DATA_WRITER)

static void
hyscan_sensor_control_class_init (HyScanSensorControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sensor_control_set_property;

  object_class->constructed = hyscan_sensor_control_object_constructed;
  object_class->finalize = hyscan_sensor_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sensor_control_signals[SIGNAL_SENSOR_DATA] =
    g_signal_new ("sensor-data", HYSCAN_TYPE_SENSOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__STRING_POINTER,
                  G_TYPE_NONE,
                  2, G_TYPE_STRING, G_TYPE_POINTER);
}

static void
hyscan_sensor_control_init (HyScanSensorControl *control)
{
  control->priv = hyscan_sensor_control_get_instance_private (control);
}

static void
hyscan_sensor_control_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      priv->sonar = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sensor_control_object_constructed (GObject *object)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sensors;

  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->constructed (object);

  g_rw_lock_init (&priv->lock);

  /* Список доступных портов. */
  priv->ports_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL, hyscan_sensor_control_free_port);
  priv->ports_by_name = g_hash_table_new (g_str_hash, g_str_equal);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    {
      g_warning ("HyScanControl: empty sonar");
      return;
    }

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanControl: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanControl: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanControl: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanControl: sonar schema version mismatch");
      return;
    }

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (priv->sonar));

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
          HyScanSensorControlPort *port;

          gchar *param_names[4];
          GVariant *param_values[4];

          gint64 id;
          gint64 type;
          gint64 protocol;

          gchar **pathv;
          gchar *name;

          gboolean status;

          param_names[0] = g_strdup_printf ("%s/id", sensors->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/type", sensors->nodes[i]->path);
          param_names[2] = g_strdup_printf ("%s/protocol", sensors->nodes[i]->path);
          param_names[3] = NULL;

          status = hyscan_sonar_get (priv->sonar, (const gchar **)param_names, param_values);

          if (status)
            {
              id = g_variant_get_int64 (param_values[0]);
              type = g_variant_get_int64 (param_values[1]);
              protocol = g_variant_get_int64 (param_values[2]);

              g_variant_unref (param_values[0]);
              g_variant_unref (param_values[1]);
              g_variant_unref (param_values[2]);
            }

          g_free (param_names[0]);
          g_free (param_names[1]);
          g_free (param_names[2]);

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

          if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
              protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
            {
              continue;
            }

          pathv = g_strsplit (sensors->nodes[i]->path, "/", -1);
          name = g_strdup (pathv[2]);
          g_strfreev (pathv);

          /* Описание порта. */
          port = g_new0 (HyScanSensorControlPort, 1);
          port->name = name;
          port->path = g_strdup (sensors->nodes[i]->path);
          port->type = type;
          port->channel = 1;
          port->protocol = protocol;

          g_hash_table_insert (priv->ports_by_id, GINT_TO_POINTER (id), port);
          g_hash_table_insert (priv->ports_by_name, name, port);
        }

      /* Обработчик данных от гидролокатора. */
      priv->signal_id = g_signal_connect_swapped (priv->sonar,
                                                  "data",
                                                  G_CALLBACK (hyscan_sensor_control_data_receiver),
                                                  control);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sensor_control_object_finalize (GObject *object)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->sonar, priv->signal_id);

  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->ports_by_id);
  g_hash_table_unref (priv->ports_by_name);

  g_rw_lock_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->finalize (object);
}

/* Функция обрабатывает сообщения с данными от гидролокатора. */
static void
hyscan_sensor_control_data_receiver (HyScanSensorControl *control,
                                     HyScanSonarMessage  *message)
{
  HyScanSensorControlPort *port;
  HyScanDataWriterData data;

  /* Ищем источник данных. */
  port = g_hash_table_lookup (control->priv->ports_by_id, GINT_TO_POINTER (message->id));
  if (port == NULL)
    return;

  /* Коррекция времени. */
  g_rw_lock_reader_lock (&control->priv->lock);
  data.time = message->time + port->time_offset;
  g_rw_lock_reader_unlock (&control->priv->lock);

  /* Обработка данных NMEA 0183. */
  if (port->protocol == HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    {
      gchar **nmeas;
      guint i;

      if (message->type != HYSCAN_DATA_STRING)
        return;

      /* Запись NMEA строк в разные каналы по типу строки. */
      nmeas = g_strsplit (message->data, "\r\n", -1);
      for (i = 0; i < g_strv_length (nmeas); i++)
        {
          HyScanSourceType nmea_type;

          if (!hyscan_sensor_control_check_nmea_crc (nmeas[i]))
            continue;

          nmea_type = hyscan_sensor_control_get_source_type (nmeas[i]);
          if ((nmea_type != HYSCAN_SOURCE_NMEA_GGA) &&
              (nmea_type != HYSCAN_SOURCE_NMEA_RMC) &&
              (nmea_type != HYSCAN_SOURCE_NMEA_DPT))
            {
              continue;
            }

          data.data = nmeas[i];
          data.size = strlen (nmeas[i]);
          hyscan_data_writer_sensor_add_data (HYSCAN_DATA_WRITER (control),
                                              port->name, nmea_type,
                                              port->channel, &data);
        }
      g_strfreev (nmeas);

      /* Запись всего блока NMEA строк. */
      data.data = message->data;
      data.size = message->size;
      hyscan_data_writer_sensor_add_data (HYSCAN_DATA_WRITER (control),
                                          port->name, HYSCAN_SOURCE_NMEA_ANY,
                                          port->channel, &data);
    }
  else
    return;

  data.data = message->data;
  data.size = message->size;
  g_signal_emit (control, hyscan_sensor_control_signals[SIGNAL_SENSOR_DATA], 0, port->name, &data);
}

/* Функция проверяет контрольную сумму NMEA сообщения. */
static gboolean
hyscan_sensor_control_check_nmea_crc (const gchar *nmea_str)
{
  guint32 nmea_crc;
  guchar crc = 0;
  gsize nmea_len = strlen (nmea_str);
  gsize i;

  nmea_crc = g_ascii_strtoull (nmea_str + nmea_len - 2, NULL, 16);
  for (i = 1; i < nmea_len - 3; i++)
    crc ^= nmea_str[i];

  if (nmea_crc != crc)
    return FALSE;

  return TRUE;
}

/* Функция возвращает название канала, в которые необходимо записать данные. */
HyScanSourceType
hyscan_sensor_control_get_source_type (const gchar *nmea_str)
{
  HyScanSourceType source;

  if (strncmp (nmea_str + 3, "GGA", 3) == 0)
    source = HYSCAN_SOURCE_NMEA_GGA;
  else if (strncmp (nmea_str + 3, "RMC", 3) == 0)
    source = HYSCAN_SOURCE_NMEA_RMC;
  else if (strncmp (nmea_str + 3, "DPT", 3) == 0)
    source = HYSCAN_SOURCE_NMEA_DPT;
  else
    source = HYSCAN_SOURCE_NMEA_ANY;

  return source;
}

/* Функция освобождает память, занятую структурой HyScanSensorControlPort. */
static void
hyscan_sensor_control_free_port (gpointer data)
{
  HyScanSensorControlPort *port = data;

  g_free (port->path);
  g_free (port->name);
  g_free (port);
}

/* Функция возвращает список портов, к которым могут быть подключены датчики. */
gchar **
hyscan_sensor_control_list_ports (HyScanSensorControl *control)
{
  gchar **list = NULL;

  GHashTableIter iter;
  gpointer name, value;
  guint n_ports;
  guint i = 0;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  n_ports = g_hash_table_size (control->priv->ports_by_id);
  if (n_ports == 0)
    return NULL;

  list = g_malloc0 (sizeof (gchar*) * (n_ports + 1));
  g_hash_table_iter_init (&iter, control->priv->ports_by_id);
  while (g_hash_table_iter_next (&iter, &name, &value))
    {
      HyScanSensorControlPort *port = value;
      list[i++] = g_strdup (port->name);
    }

  return list;
}

/* Функция возвращает список физических устройств UART. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_uart_devices (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (control->priv->sonar), "/sensors/uart-device");
}

/* Функция возвращает список допустимых режимов обмена данными через UART устройство. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_uart_modes (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (control->priv->sonar), "/sensors/uart-mode");
}

/* Функция возвращает список допустимых IP адресов для портов типа IP. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_ip_addresses (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (control->priv->sonar), "/sensors/ip-address");
}

/* Функция возвращает тип порта. */
HyScanSensorPortType
hyscan_sensor_control_get_port_type (HyScanSensorControl *control,
                                     const gchar         *name)
{
  HyScanSensorControlPort *port;

  HyScanSensorPortType port_type = HYSCAN_SENSOR_PORT_INVALID;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), port_type);

  if (control->priv->sonar == NULL)
    return port_type;

  port = g_hash_table_lookup (control->priv->ports_by_name, name);
  if (port == NULL)
    return port_type;

  return port->type;
}

/* Функция возвращает текущее состояние порта. */
HyScanSensorPortStatus
hyscan_sensor_control_get_port_status (HyScanSensorControl *control,
                                       const gchar         *name)
{
  HyScanSensorControlPort *port;

  gint64 port_status = HYSCAN_SENSOR_PORT_STATUS_INVALID;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), port_status);

  if (control->priv->sonar == NULL)
    return port_status;

  port = g_hash_table_lookup (control->priv->ports_by_name, name);
  if (port == NULL)
    return port_status;

  param_name = g_strdup_printf ("%s/status", port->path);
  status = hyscan_sonar_get_enum (control->priv->sonar, param_name, &port_status);
  g_free (param_name);

  if (!status)
    port_status = HYSCAN_SENSOR_PORT_STATUS_INVALID;

  return port_status;
}

/* Функция устанавливает номер канала для порта типа VIRTUAL. */
gboolean
hyscan_sensor_control_set_virtual_port_param (HyScanSensorControl     *control,
                                              const gchar             *name,
                                              guint                    channel,
                                              gint64                   time_offset)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 1 || channel > HYSCAN_SENSOR_CONTROL_MAX_CHANNELS)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_VIRTUAL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  port->channel = channel;
  port->time_offset = time_offset;

  g_rw_lock_writer_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает режим работы порта типа UART. */
gboolean
hyscan_sensor_control_set_uart_port_param (HyScanSensorControl      *control,
                                           const gchar              *name,
                                           guint                     channel,
                                           gint64                    time_offset,
                                           HyScanSensorProtocolType  protocol,
                                           gint64                    uart_device,
                                           gint64                    uart_mode)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 1 || channel > HYSCAN_SENSOR_CONTROL_MAX_CHANNELS)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS && protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_UART)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/protocol", port->path);
  param_names[1] = g_strdup_printf ("%s/uart-device", port->path);
  param_names[2] = g_strdup_printf ("%s/uart-mode", port->path);
  param_names[3] = NULL;

  param_values[0] = g_variant_new_int64 (protocol);
  param_values[1] = g_variant_new_int64 (uart_device);
  param_values[2] = g_variant_new_int64 (uart_mode);
  param_values[3] = NULL;

  status = hyscan_sonar_set (priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);

  if (status)
    {
      g_rw_lock_writer_lock (&priv->lock);

      port->channel = channel;
      port->protocol = protocol;
      port->time_offset = time_offset;

      g_rw_lock_writer_unlock (&priv->lock);
    }

  return status;
}

/* Функция устанавливает режим работы порта типа UDP/IP. */
gboolean
hyscan_sensor_control_set_udp_ip_port_param (HyScanSensorControl      *control,
                                             const gchar              *name,
                                             guint                     channel,
                                             gint64                    time_offset,
                                             HyScanSensorProtocolType  protocol,
                                             gint64                    ip_address,
                                             guint16                   udp_port)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 1 || channel > HYSCAN_SENSOR_CONTROL_MAX_CHANNELS)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS && protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_UDP_IP)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/protocol", port->path);
  param_names[1] = g_strdup_printf ("%s/ip-address", port->path);
  param_names[2] = g_strdup_printf ("%s/udp-port", port->path);
  param_names[3] = NULL;

  param_values[0] = g_variant_new_int64 (protocol);
  param_values[1] = g_variant_new_int64 (ip_address);
  param_values[2] = g_variant_new_int64 (udp_port);
  param_values[3] = NULL;

  status = hyscan_sonar_set (priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);

  if (status)
    {
      g_rw_lock_writer_lock (&priv->lock);

      port->channel = channel;
      port->protocol = protocol;
      port->time_offset = time_offset;

      g_rw_lock_writer_unlock (&priv->lock);
    }

  return status;
}

/* Функция устанавливает информацию о местоположении приёмных антенн. */
gboolean
hyscan_sensor_control_set_position (HyScanSensorControl *control,
                                    const gchar         *name,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              z,
                                    gdouble              psi,
                                    gdouble              gamma,
                                    gdouble              theta)
{
  HyScanSensorControlPort *port;
  HyScanAntennaPosition position;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  port = g_hash_table_lookup (control->priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  position.x = x;
  position.y = y;
  position.z = z;
  position.psi = psi;
  position.gamma = gamma;
  position.theta = theta;
  return hyscan_data_writer_sensor_set_position (HYSCAN_DATA_WRITER (control), port->name, &position);
}

/* Функция включает или выключает приём данных на указанном порту. */
gboolean
hyscan_sensor_control_set_enable (HyScanSensorControl *control,
                                  const gchar         *name,
                                  gboolean             enable)
{
  HyScanSensorControlPort *port;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (control->priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/enable", port->path);
  status = hyscan_sonar_set_boolean (control->priv->sonar, param_name, enable);
  g_free (param_name);

  return status;
}
