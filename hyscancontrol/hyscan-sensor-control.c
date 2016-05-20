/*
 * \file hyscan-sonar-sensor.c
 *
 * \brief Исходный файл класса управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sensor-control.h"
#include "hyscan-marshallers.h"
#include <string.h>

#define HYSCAN_SONAR_SCHEMA_VERSION 20160400

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
  gchar                       *path;                           /* Путь к описанию порта в схеме. */
  gint                         id;                             /* Идентификатор порта. */
  gchar                       *name;                           /* Название порта. */
  HyScanSonarChannelIndex      channel;                        /* Номер канала данных. */
  HyScanSensorPortType         type;                           /* Тип порта. */
  HyScanSensorProtocolType     protocol;                       /* Протокол передачи данных. */
  gint64                       time_offset;                    /* Коррекция времени. */
  HyScanSensorChannelInfo      channel_info;                   /* Параметры канала записи данных. */
} HyScanSensorControlPort;

struct _HyScanSensorControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */

  HyScanDataSchema            *schema;                         /* Схема данных гидролокатора. */
  HyScanDataSchemaNode        *params;                         /* Список параметров гидролокатора. */

  GHashTable                  *ports;                          /* Список портов для подключения датчиков. */

  GRWLock                      lock;                           /* Блокировка. */

  gboolean                   (*start)                          (HyScanWriteControl            *control,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name);
  void                       (*stop)                           (HyScanWriteControl            *control);
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
                                                                HyScanSonarChannelIndex    channel);

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
  gint i;

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->constructed (object);

  g_rw_lock_init (&priv->lock);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/version", &version))
    {
      g_warning ("HyScanSensor: unknown sonar schema");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_warning ("HyScanSensor: sonar schema version mismatch");
      return;
    }

  /* Схема данных гидролокатора. */
  priv->schema = hyscan_sonar_get_schema (priv->sonar);
  priv->params = hyscan_data_schema_list_nodes (priv->schema);

  /* Список доступных портов. */
  priv->ports = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL, hyscan_sensor_control_free_port);

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

      gchar *key_name;
      gboolean status;

      gboolean exist;
      gchar *name;
      gint64 id;
      gint64 type;
      gint64 protocol;

      /* Проверяем наличие порта. */
      key_name = g_strdup_printf ("%s/exist", sensors->nodes[i]->path);
      status = hyscan_sonar_get_boolean (priv->sonar, key_name, &exist);
      g_free (key_name);

      if (!status || !exist)
        continue;

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
                      type != HYSCAN_SENSOR_PORT_IP &&
                      type != HYSCAN_SENSOR_PORT_RS232))
        continue;

      /* Название порта. */
      key_name = g_strdup_printf ("%s/name", sensors->nodes[i]->path);
      name = hyscan_sonar_get_string (priv->sonar, key_name);
      g_free (key_name);

      if (name == NULL)
        continue;

      /* Протокол обмена данными с датчиком. */
      key_name = g_strdup_printf ("%s/protocol", sensors->nodes[i]->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &protocol);
      g_free (key_name);

      if (!status || (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
                      protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183))
        continue;

      /* Описание порта. */
      port = g_new0 (HyScanSensorControlPort, 1);
      port->path = g_strdup (sensors->nodes[i]->path);
      port->id = id;
      port->name = name;
      port->type = type;
      port->channel = HYSCAN_SONAR_CHANNEL_1;
      port->protocol = protocol;

      if (!g_hash_table_insert (priv->ports, GINT_TO_POINTER (id), port))
        hyscan_sensor_control_free_port (port);
    }

  /* Обработчик данных от гидролокатора. */
  g_signal_connect_swapped (priv->sonar, "data", G_CALLBACK (hyscan_sensor_control_data_receiver), control);
}

static void
hyscan_sensor_control_object_finalize (GObject *object)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  g_clear_pointer (&priv->ports, g_hash_table_unref);

  g_clear_pointer (&priv->params, hyscan_data_schema_free_nodes);

  g_clear_object (&priv->sonar);

  g_rw_lock_clear (&priv->lock);

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

  g_rw_lock_reader_lock (&priv->lock);

  /* Ищем источник данных. */
  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (data_msg->id));
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
  g_rw_lock_reader_unlock (&priv->lock);
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
hyscan_sensor_control_get_channel_name (const gchar             *nmea_str,
                                        HyScanSonarChannelIndex  channel)
{
  HyScanSonarDataType data_type;

  if (strncmp (nmea_str + 3, "GGA", 3) == 0)
    data_type = HYSCAN_SONAR_DATA_NMEA_GGA;
  else if (strncmp (nmea_str + 3, "RMC", 3) == 0)
    data_type = HYSCAN_SONAR_DATA_NMEA_RMC;
  else if (strncmp (nmea_str + 3, "DPT", 3) == 0)
    data_type = HYSCAN_SONAR_DATA_NMEA_DPT;
  else
    data_type = HYSCAN_SONAR_DATA_NMEA_ANY;

  return hyscan_channel_get_name_by_types (data_type, FALSE, FALSE, channel);
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

/* Функция создаёт новый объект HyScanSensorControl. */
HyScanSensorControl *
hyscan_sensor_control_new (HyScanSonar *sonar,
                           HyScanDB    *db)
{
  return g_object_new (HYSCAN_TYPE_SENSOR_CONTROL, "sonar", sonar, "db", db, NULL);
}

/* Функция включает запись данных от датчиков. */
gboolean
hyscan_sensor_control_start (HyScanSensorControl *control,
                             const gchar         *project_name,
                             const gchar         *track_name)
{
  HyScanWriteControlClass *parent_class;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  parent_class = HYSCAN_WRITE_CONTROL_GET_CLASS (control);

  return parent_class->start (HYSCAN_WRITE_CONTROL (control), project_name, track_name);
}

/* Функция отключает запись данных от датчиков. */
void
hyscan_sensor_control_stop (HyScanSensorControl *control)
{
  HyScanWriteControlClass *parent_class;

  g_return_if_fail (HYSCAN_IS_SENSOR_CONTROL (control));

  parent_class = HYSCAN_WRITE_CONTROL_GET_CLASS (control);

  parent_class->stop (HYSCAN_WRITE_CONTROL (control));
}

/* Функция возвращает список портов, к которым могут быть подключены датчики. */
HyScanSensorPort **
hyscan_sensor_control_list_ports (HyScanSensorControl *control)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorPort **list = NULL;

  GHashTableIter iter;
  gpointer key, value;
  guint n_ports;
  guint i = 0;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  n_ports = g_hash_table_size (priv->ports);
  if (n_ports == 0)
    return NULL;

  list = g_malloc0 (sizeof (HyScanSensorControlPort*) * (n_ports + 1));
  g_hash_table_iter_init (&iter, priv->ports);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HyScanSensorControlPort *port = value;

      list[i] = g_new0 (HyScanSensorPort, 1);
      list[i]->id = port->id;
      list[i]->name = g_strdup (port->name);
      list[i]->type = port->type;
      i += 1;
    }

  return list;
}

/* Функция возвращает список допустимых IP адресов для портов типа IP. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_ip_addresses (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (control->priv->schema, "ip-addresses");
}

/* Функция возвращает список физических портов RS232 */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_rs232_ports (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (control->priv->schema, "rs232-ports");
}

/* Функция возвращает список допустимых скоростей работы физических портов RS232. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_rs232_speeds (HyScanSensorControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  return hyscan_data_schema_key_get_enum_values (control->priv->schema, "rs232-speeds");
}

/* Функция возвращает текущее состояние порта. */
HyScanSensorPortStatus
hyscan_sensor_control_get_port_status (HyScanSensorControl *control,
                                       gint                 port_id)
{
  HyScanSensorControlPort *port;

  gint64 port_status = HYSCAN_SENSOR_PORT_STATUS_INVALID;
  gboolean status;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), port_status);

  if (control->priv->sonar == NULL)
    return port_status;

  port = g_hash_table_lookup (control->priv->ports, GINT_TO_POINTER (port_id));
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
                                              gint                     port_id,
                                              HyScanSonarChannelIndex  channel,
                                              gint64                   time_offset)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel != HYSCAN_SONAR_CHANNEL_1 &&
      channel != HYSCAN_SONAR_CHANNEL_2 &&
      channel != HYSCAN_SONAR_CHANNEL_3 &&
      channel != HYSCAN_SONAR_CHANNEL_4 &&
      channel != HYSCAN_SONAR_CHANNEL_5)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  port->channel = channel;
  port->time_offset = time_offset;

  g_rw_lock_writer_unlock (&priv->lock);

  return TRUE;
}

/* Функция считывает номер канала для порта типа VIRTUAL. */
gboolean
hyscan_sensor_control_get_virtual_port_param (HyScanSensorControl     *control,
                                              gint                     port_id,
                                              HyScanSonarChannelIndex *channel,
                                              gint64                  *time_offset)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  if (channel != NULL)
    *channel = port->channel;

  if (time_offset != NULL)
    *time_offset = port->time_offset;

  g_rw_lock_reader_unlock (&priv->lock);

  return TRUE;
}

/* Функция устанавливает режим работы порта типа IP. */
gboolean
hyscan_sensor_control_set_ip_port_param (HyScanSensorControl      *control,
                                         gint                      port_id,
                                         HyScanSonarChannelIndex   channel,
                                         gint64                    time_offset,
                                         HyScanSensorProtocolType  protocol,
                                         gint64                    ip_address,
                                         guint16                   udp_port)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gboolean status = FALSE;
  gboolean enable;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel != HYSCAN_SONAR_CHANNEL_1 &&
      channel != HYSCAN_SONAR_CHANNEL_2 &&
      channel != HYSCAN_SONAR_CHANNEL_3 &&
      channel != HYSCAN_SONAR_CHANNEL_4 &&
      channel != HYSCAN_SONAR_CHANNEL_5)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
      protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  /* Временно отключаем порт и устанавливаем его параметры. */
  enable = hyscan_sensor_control_get_enable (control, port_id);
  if (enable && !hyscan_sensor_control_set_enable (control, port_id, FALSE))
    goto exit;

  port->channel = channel;
  port->time_offset = time_offset;
  port->protocol = protocol;

  key_name = g_strdup_printf ("%s/protocol", port->path);
  status = hyscan_sonar_set_enum (priv->sonar, key_name, protocol);
  g_free (key_name);

  if (!status)
    goto exit;

  key_name = g_strdup_printf ("%s/address", port->path);
  status = hyscan_sonar_set_enum (priv->sonar, key_name, ip_address);
  g_free (key_name);

  if (!status)
    goto exit;

  key_name = g_strdup_printf ("%s/port", port->path);
  status = hyscan_sonar_set_integer (priv->sonar, key_name, udp_port);
  g_free (key_name);

  if (!status)
    goto exit;

  /* Включаем порт. */
  if (enable)
    status = hyscan_sensor_control_set_enable (control, port_id, TRUE);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  return status;
}

/* Функция считывает режим работы порта типа IP. */
gboolean
hyscan_sensor_control_get_ip_port_param (HyScanSensorControl      *control,
                                         gint                      port_id,
                                         HyScanSonarChannelIndex  *channel,
                                         gint64                   *time_offset,
                                         HyScanSensorProtocolType *protocol,
                                         gint64                   *ip_address,
                                         guint16                  *udp_port)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gboolean status = FALSE;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  if (channel != NULL)
    *channel = port->channel;

  if (time_offset != NULL)
    *time_offset = port->time_offset;

  if (protocol != NULL)
    {
      gint64 protocol_id;

      key_name = g_strdup_printf ("%s/protocol", port->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &protocol_id);
      g_free (key_name);

      if (!status)
        goto exit;

      if (protocol_id != HYSCAN_SENSOR_PROTOCOL_SAS &&
          protocol_id != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
        {
          status = FALSE;
          goto exit;
        }

      *protocol = protocol_id;
    }

  if (ip_address != NULL)
    {
      key_name = g_strdup_printf ("%s/address", port->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, ip_address);
      g_free (key_name);

      if (!status)
        goto exit;
    }

  if (udp_port != NULL)
    {
      gint64 raw_udp_port;

      key_name = g_strdup_printf ("%s/port", port->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &raw_udp_port);
      g_free (key_name);

      if (!status)
        goto exit;

      if (raw_udp_port < 1024 || raw_udp_port > 65535)
        {
          status = FALSE;
          goto exit;
        }

      *udp_port = raw_udp_port;
    }

  status = TRUE;

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает режим работы порта типа RS232. */
gboolean
hyscan_sensor_control_set_rs232_port_param (HyScanSensorControl      *control,
                                            gint                      port_id,
                                            HyScanSonarChannelIndex   channel,
                                            gint64                    time_offset,
                                            HyScanSensorProtocolType  protocol,
                                            gint64                    rs232_port,
                                            gint64                    rs232_speed)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gboolean status = FALSE;
  gboolean enable;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel != HYSCAN_SONAR_CHANNEL_1 &&
      channel != HYSCAN_SONAR_CHANNEL_2 &&
      channel != HYSCAN_SONAR_CHANNEL_3 &&
      channel != HYSCAN_SONAR_CHANNEL_4 &&
      channel != HYSCAN_SONAR_CHANNEL_5)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
      protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  /* Временно отключаем порт и устанавливаем его параметры. */
  enable = hyscan_sensor_control_get_enable (control, port_id);
  if (enable && !hyscan_sensor_control_set_enable (control, port_id, FALSE))
    goto exit;

  port->channel = channel;
  port->protocol = protocol;
  port->time_offset = time_offset;

  key_name = g_strdup_printf ("%s/protocol", port->path);
  status = hyscan_sonar_set_enum (priv->sonar, key_name, protocol);
  g_free (key_name);

  if (!status)
    goto exit;

 key_name = g_strdup_printf ("%s/port", port->path);
  status = hyscan_sonar_set_enum (priv->sonar, key_name, rs232_port);
  g_free (key_name);

  if (!status)
    goto exit;

  key_name = g_strdup_printf ("%s/speed", port->path);
  status = hyscan_sonar_set_integer (priv->sonar, key_name, rs232_speed);
  g_free (key_name);

  if (!status)
    goto exit;

  /* Включаем порт. */
  if (enable)
    status = hyscan_sensor_control_set_enable (control, port_id, TRUE);

exit:
  g_rw_lock_writer_unlock (&priv->lock);

  return status;
}

/* Функция считывает режим работы порта типа RS232. */
gboolean
hyscan_sensor_control_get_rs232_port_param (HyScanSensorControl      *control,
                                            gint                      port_id,
                                            HyScanSonarChannelIndex  *channel,
                                            gint64                   *time_offset,
                                            HyScanSensorProtocolType *protocol,
                                            gint64                   *rs232_port,
                                            gint64                   *rs232_speed)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gboolean status = FALSE;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  if (channel != NULL)
    *channel = port->channel;

  if (time_offset != NULL)
    *time_offset = port->time_offset;

  if (protocol != NULL)
    {
      gint64 protocol_id;

      key_name = g_strdup_printf ("%s/protocol", port->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &protocol_id);
      g_free (key_name);

      if (!status)
        goto exit;

      if (protocol_id != HYSCAN_SENSOR_PROTOCOL_SAS &&
          protocol_id != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
        {
          status = FALSE;
          goto exit;
        }

      *protocol = protocol_id;
    }

  if (rs232_port != NULL)
    {
      key_name = g_strdup_printf ("%s/port", port->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, rs232_port);
      g_free (key_name);

      if (!status)
        goto exit;
    }

  if (rs232_speed != NULL)
    {
      key_name = g_strdup_printf ("%s/speed", port->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, rs232_speed);
      g_free (key_name);

      if (!status)
        goto exit;
    }

  status = TRUE;

exit:
  g_rw_lock_reader_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает информацию о местоположении приёмных антенн. */
gboolean
hyscan_sensor_control_set_position (HyScanSensorControl *control,
                                    gint                 port_id,
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

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  port->channel_info.x = x;
  port->channel_info.y = y;
  port->channel_info.z = z;
  port->channel_info.psi = psi;
  port->channel_info.gamma = gamma;
  port->channel_info.theta = theta;

  g_rw_lock_writer_unlock (&priv->lock);

  return TRUE;
}

/* Функция возвращает информацию о местоположении приёмных антенн. */
gboolean
hyscan_sensor_control_get_position (HyScanSensorControl *control,
                                    gint                 port_id,
                                    gdouble             *x,
                                    gdouble             *y,
                                    gdouble             *z,
                                    gdouble             *psi,
                                    gdouble             *gamma,
                                    gdouble             *theta)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  g_rw_lock_reader_lock (&priv->lock);

  if (x != NULL)
    *x = port->channel_info.x;
  if (y != NULL)
    *y = port->channel_info.y;
  if (z != NULL)
    *z = port->channel_info.z;
  if (psi != NULL)
    *psi = port->channel_info.psi;
  if (gamma != NULL)
    *gamma = port->channel_info.gamma;
  if (theta != NULL)
    *theta = port->channel_info.theta;

  g_rw_lock_reader_unlock (&priv->lock);

  return TRUE;
}

/* Функция включает или выключает приём данных на указанном порту. */
gboolean
hyscan_sensor_control_set_enable (HyScanSensorControl *control,
                                  gint                 port_id,
                                  gboolean             enable)
{
  HyScanSensorControlPort *port;

  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (control->priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  key_name = g_strdup_printf ("%s/enable", port->path);
  hyscan_sonar_set_boolean (control->priv->sonar, key_name, enable);
  g_free (key_name);

  return hyscan_sensor_control_get_enable (control, enable);
}

/* Функция возвращает состояние приёма данных на указанном порту. */
gboolean
hyscan_sensor_control_get_enable (HyScanSensorControl *control,
                                  gint                 port_id)
{
  HyScanSensorControlPort *port;

  gboolean enable;
  gboolean status;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  port = g_hash_table_lookup (control->priv->ports, GINT_TO_POINTER (port_id));
  if (port == NULL)
    return FALSE;

  key_name = g_strdup_printf ("%s/enable", port->path);
  status = hyscan_sonar_get_boolean (control->priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    return FALSE;

  return enable;
}
