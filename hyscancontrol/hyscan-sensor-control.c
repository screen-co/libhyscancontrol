/*
 * \file hyscan-sonar-sensor.c
 *
 * \brief Исходный файл класса управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control-common.h"
#include "hyscan-sensor-control.h"
#include "hyscan-marshallers.h"
#include <string.h>

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
  gint                         id;                             /* Идентификатор порта. */
  gchar                       *name;                           /* Название порта. */
  gchar                       *path;                           /* Путь к описанию порта в схеме. */
  HyScanSensorPortType         type;                           /* Тип порта. */
  HyScanSensorProtocolType     protocol;                       /* Протокол передачи данных. */
  gint64                       time_offset;                    /* Коррекция времени. */
  gint                         channel;                        /* Номер канала данных. */
  HyScanSensorChannelInfo      channel_info;                   /* Параметры канала записи данных. */
} HyScanSensorControlPort;

struct _HyScanSensorControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */

  HyScanDataSchema            *schema;                         /* Схема данных гидролокатора. */
  HyScanDataSchemaNode        *params;                         /* Список параметров гидролокатора. */

  GHashTable                  *ports_by_id;                    /* Список портов для подключения датчиков. */
  GHashTable                  *ports_by_name;                  /* Список портов для подключения датчиков. */

  GMutex                       lock;                           /* Блокировка. */
};

static void          hyscan_sensor_control_set_property        (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void          hyscan_sensor_control_object_constructed  (GObject                   *object);
static void          hyscan_sensor_control_object_finalize     (GObject                   *object);

static void          hyscan_sensor_control_data_receiver       (HyScanSensorControl       *control,
                                                                HyScanSonarMsgData        *data_msg);

static gboolean      hyscan_sensor_control_check_nmea_crc      (const gchar               *nmea_str);

static const gchar  *hyscan_sensor_control_get_channel_name    (const gchar               *nmea_str,
                                                                gint                       channel);

static void          hyscan_sensor_control_free_port           (gpointer                   data);

static guint         hyscan_sensor_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSensorControl, hyscan_sensor_control, HYSCAN_TYPE_WRITE_CONTROL)

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
                  g_cclosure_user_marshal_VOID__POINTER_POINTER,
                  G_TYPE_NONE,
                  2, G_TYPE_POINTER, G_TYPE_POINTER);
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

  HyScanDataSchemaNode *sensors = NULL;
  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: sonar schema version mismatch");
      return;
    }

  /* Схема данных гидролокатора. */
  priv->schema = hyscan_sonar_get_schema (priv->sonar);
  priv->params = hyscan_data_schema_list_nodes (priv->schema);

  /* Список доступных портов. */
  priv->ports_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL, hyscan_sensor_control_free_port);
  priv->ports_by_name = g_hash_table_new (g_str_hash, g_str_equal);

  /* Ветка схемы с описанием портов - "/sensors". */
  for (i = 0; priv->params->n_nodes; i++)
    {
      if (g_strcmp0 (priv->params->nodes[i]->path, "/sensors") == 0)
        {
          sensors = priv->params->nodes[i];
          break;
        }
    }

  if (sensors == NULL)
    return;

  /* Считываем описания портов. */
  for (i = 0; i < sensors->n_nodes; i++)
    {
      HyScanSensorControlPort *port;

      gchar **pathv;
      gchar *key_name;
      gboolean status;

      gint64 id;
      gchar *name;
      gint64 type;
      gint64 protocol;

      /* Идентификатор порта. */
      key_name = g_strdup_printf ("%s/id", sensors->nodes[i]->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &id);
      g_free (key_name);

      if (!status || id <= 0 || id > G_MAXUINT32)
        continue;

      /* Тип порта. */
      key_name = g_strdup_printf ("%s/type", sensors->nodes[i]->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &type);
      g_free (key_name);

      if (!status || (type != HYSCAN_SENSOR_PORT_VIRTUAL &&
                      type != HYSCAN_SENSOR_PORT_UART &&
                      type != HYSCAN_SENSOR_PORT_UDP_IP))
        continue;

      /* Протокол обмена данными с датчиком. */
      key_name = g_strdup_printf ("%s/protocol", sensors->nodes[i]->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &protocol);
      g_free (key_name);

      if (!status || (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
                      protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183))
        continue;

      pathv = g_strsplit (sensors->nodes[i]->path, "/", -1);
      name = g_strdup (pathv[2]);
      g_strfreev (pathv);

      /* Описание порта. */
      port = g_new0 (HyScanSensorControlPort, 1);
      port->id = id;
      port->name = name;
      port->path = g_strdup (sensors->nodes[i]->path);
      port->type = type;
      port->channel = 1;
      port->protocol = protocol;

      if (!g_hash_table_insert (priv->ports_by_id, GINT_TO_POINTER (id), port))
        hyscan_sensor_control_free_port (port);

      if (!g_hash_table_insert (priv->ports_by_name, name, port))
        g_hash_table_remove (priv->ports_by_id, GINT_TO_POINTER (id));
    }

  /* Обработчик данных от гидролокатора. */
  g_signal_connect_swapped (priv->sonar, "data", G_CALLBACK (hyscan_sensor_control_data_receiver), control);
}

static void
hyscan_sensor_control_object_finalize (GObject *object)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  g_clear_pointer (&priv->ports_by_id, g_hash_table_unref);
  g_clear_pointer (&priv->ports_by_name, g_hash_table_unref);

  g_clear_pointer (&priv->params, hyscan_data_schema_free_nodes);

  g_clear_object (&priv->sonar);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->finalize (object);
}

/* Функция обрабатывает сообщения с данными от гидролокатора. */
static void
hyscan_sensor_control_data_receiver (HyScanSensorControl *control,
                                     HyScanSonarMsgData  *data_msg)
{
  HyScanSensorControlPrivate *priv = control->priv;
  HyScanSensorControlPort *port;

  HyScanWriteData data;

  g_mutex_lock (&priv->lock);

  /* Ищем источник данных. */
  port = g_hash_table_lookup (priv->ports_by_id, GINT_TO_POINTER (data_msg->id));
  if (port == NULL)
    goto exit;

  /* Пока поддерживаем только протокол NMEA 0183. */
  if (port->protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    goto exit;

  /* Проверяем контрольную сумму NMEA строки. */
  if (!hyscan_sensor_control_check_nmea_crc (data_msg->data))
    goto exit;

  /* Данные. */
  data.name = hyscan_sensor_control_get_channel_name (data_msg->data, port->channel);
  data.time = data_msg->time;
  data.size = data_msg->data_size;
  data.data = data_msg->data;

  hyscan_write_control_add_sensor_data (HYSCAN_WRITE_CONTROL (control), &data, &port->channel_info);

  g_signal_emit (control, hyscan_sensor_control_signals[SIGNAL_SENSOR_DATA], 0, &data, &port->channel_info);

exit:
  g_mutex_unlock (&priv->lock);
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
static const gchar *
hyscan_sensor_control_get_channel_name (const gchar *nmea_str,
                                        gint         channel)
{
  HyScanSourceType data_type;

  if (strncmp (nmea_str + 3, "GGA", 3) == 0)
    data_type = HYSCAN_SOURCE_NMEA_GGA;
  else if (strncmp (nmea_str + 3, "RMC", 3) == 0)
    data_type = HYSCAN_SOURCE_NMEA_RMC;
  else if (strncmp (nmea_str + 3, "DPT", 3) == 0)
    data_type = HYSCAN_SOURCE_NMEA_DPT;
  else
    data_type = HYSCAN_SOURCE_NMEA_ANY;

  return hyscan_channel_get_name_by_types (data_type, FALSE, channel);
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
  HyScanSensorControlPrivate *priv;
  gchar **list = NULL;

  GHashTableIter iter;
  gpointer key, value;
  guint n_ports;
  guint i = 0;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  n_ports = g_hash_table_size (priv->ports_by_id);
  if (n_ports == 0)
    return NULL;

  list = g_malloc0 (sizeof (gchar*) * (n_ports + 1));
  g_hash_table_iter_init (&iter, priv->ports_by_id);
  while (g_hash_table_iter_next (&iter, &key, &value))
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

  return hyscan_data_schema_key_get_enum_values (control->priv->schema, "/enums/uart-device");
}

/* Функция возвращает список допустимых режимов обмена данными через UART устройство. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_uart_modes (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (control->priv->schema, "/enums/uart-mode");
}

/* Функция возвращает список допустимых IP адресов для портов типа IP. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_ip_addresses (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (control->priv->schema, "/enums/ip-address");
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
  gboolean status;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), port_status);

  if (control->priv->sonar == NULL)
    return port_status;

  port = g_hash_table_lookup (control->priv->ports_by_name, name);
  if (port == NULL)
    return port_status;

  key_name = g_strdup_printf ("%s/status", port->path);
  status = hyscan_sonar_get_enum (control->priv->sonar, key_name, &port_status);
  g_free (key_name);

  if (!status)
    port_status = HYSCAN_SENSOR_PORT_STATUS_INVALID;

  return port_status;
}

/* Функция устанавливает номер канала для порта типа VIRTUAL. */
gboolean
hyscan_sensor_control_set_virtual_port_param (HyScanSensorControl     *control,
                                              const gchar             *name,
                                              gint                     channel,
                                              gint64                   time_offset)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 0 || channel > 5)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_VIRTUAL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  port->channel = channel;
  port->time_offset = time_offset;

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает режим работы порта типа UART. */
gboolean
hyscan_sensor_control_set_uart_port_param (HyScanSensorControl      *control,
                                           const gchar              *name,
                                           gint                      channel,
                                           gint64                    time_offset,
                                           HyScanSensorProtocolType  protocol,
                                           gint64                    uart_device,
                                           gint64                    uart_mode)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gint64 ivalue;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 0 || channel > 5)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
      protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_UART)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Временно отключаем порт и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", port->path);
  status = hyscan_sonar_get_boolean (control->priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_sensor_control_set_enable (control, name, FALSE))
    goto exit;

  port->channel = channel;
  port->protocol = protocol;
  port->time_offset = time_offset;

  key_name = g_strdup_printf ("%s/protocol", port->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, protocol);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || protocol != ivalue)
    goto exit;

  key_name = g_strdup_printf ("%s/uart-device", port->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, uart_device);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || uart_device != ivalue)
    goto exit;

  key_name = g_strdup_printf ("%s/uart-mode", port->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, uart_mode);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || uart_mode != ivalue)
    goto exit;

  /* Включаем порт. */
  if (enable)
    {
      if (hyscan_sensor_control_set_enable (control, name, TRUE))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
}

/* Функция устанавливает режим работы порта типа UDP/IP. */
gboolean
hyscan_sensor_control_set_udp_ip_port_param (HyScanSensorControl      *control,
                                             const gchar              *name,
                                             gint                      channel,
                                             gint64                    time_offset,
                                             HyScanSensorProtocolType  protocol,
                                             gint64                    ip_address,
                                             guint16                   udp_port)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gint64 ivalue;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 0 || channel > 5)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
      protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_UDP_IP)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Временно отключаем порт и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", port->path);
  status = hyscan_sonar_get_boolean (control->priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_sensor_control_set_enable (control, name, FALSE))
    goto exit;

  port->channel = channel;
  port->time_offset = time_offset;
  port->protocol = protocol;

  key_name = g_strdup_printf ("%s/protocol", port->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, protocol);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || protocol != ivalue)
    goto exit;

  key_name = g_strdup_printf ("%s/ip-address", port->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, ip_address);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || ip_address != ivalue)
    goto exit;

  key_name = g_strdup_printf ("%s/udp-port", port->path);
  hyscan_sonar_set_integer (priv->sonar, key_name, udp_port);
  status = hyscan_sonar_get_integer (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || udp_port != ivalue)
    goto exit;

  /* Включаем порт. */
  if (enable)
    {
      if (hyscan_sensor_control_set_enable (control, name, TRUE))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
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
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  port->channel_info.x = x;
  port->channel_info.y = y;
  port->channel_info.z = z;
  port->channel_info.psi = psi;
  port->channel_info.gamma = gamma;
  port->channel_info.theta = theta;

  g_mutex_unlock (&priv->lock);

  return TRUE;
}

/* Функция включает или выключает приём данных на указанном порту. */
gboolean
hyscan_sensor_control_set_enable (HyScanSensorControl *control,
                                  const gchar         *name,
                                  gboolean             enable)
{
  HyScanSensorControlPort *port;

  gboolean is_enabled;
  gboolean status;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (control->priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  key_name = g_strdup_printf ("%s/enable", port->path);
  hyscan_sonar_set_boolean (control->priv->sonar, key_name, enable);
  status = hyscan_sonar_get_boolean (control->priv->sonar, key_name, &is_enabled);
  g_free (key_name);

  if (!status || is_enabled != enable)
    return FALSE;

  return TRUE;
}
