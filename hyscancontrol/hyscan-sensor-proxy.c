/*
 * \file hyscan-sensor-proxy.c
 *
 * \brief Исходный файл класса прокси сервера управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sensor-proxy.h"
#include "hyscan-sensor-control.h"
#include "hyscan-sensor-control-server.h"
#include "hyscan-control-common.h"

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_FORWARD_RAW
};

struct _HyScanSensorProxyPrivate
{
  HyScanSensorControl         *control;                        /* Клиент управления проксируемыми датчиками. */
  HyScanSensorControlServer   *server;                         /* Прокси сервер датчиков. */

  GHashTable                  *uart_devices;                   /* Таблица трансляции UART устройств. */
  GHashTable                  *uart_modes;                     /* Таблица трансляции режимов работы UART устройств. */
  GHashTable                  *ip_addresses;                   /* Таблица трансляции IP адресов. */

  gboolean                     forward_raw;                    /* Признак трансляции данных без преобразования 1:1. */
  gchar                       *virtual_nmea_source;            /* Название порта источника данных для виртуального NMEA порта. */

  GRWLock                      lock;                           /* Блокировка. */
};

static void        hyscan_sensor_proxy_set_property            (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void        hyscan_sensor_proxy_object_constructed      (GObject                   *object);
static void        hyscan_sensor_proxy_object_finalize         (GObject                   *object);

static gboolean    hyscan_sensor_proxy_set_virtual_port_param  (HyScanSensorProxyPrivate  *priv,
                                                                const gchar               *name,
                                                                guint                      channel,
                                                                gint64                     time_offset);
static gboolean    hyscan_sensor_proxy_set_uart_port_param     (HyScanSensorProxyPrivate  *priv,
                                                                const gchar               *name,
                                                                guint                      channel,
                                                                gint64                     time_offset,
                                                                HyScanSensorProtocolType   protocol,
                                                                guint                      uart_device,
                                                                guint                      uart_mode);
static gboolean    hyscan_sensor_proxy_set_udp_ip_port_param   (HyScanSensorProxyPrivate  *priv,
                                                                const gchar               *name,
                                                                guint                      channel,
                                                                gint64                     time_offset,
                                                                HyScanSensorProtocolType   protocol,
                                                                guint                      ip_address,
                                                                guint16                    udp_port);
static gboolean    hyscan_sensor_proxy_set_position            (HyScanSensorProxyPrivate  *priv,
                                                                const gchar               *name,
                                                                HyScanAntennaPosition     *position);
static gboolean    hyscan_sensor_proxy_set_enable              (HyScanSensorProxyPrivate  *priv,
                                                                const gchar               *name,
                                                                gboolean                   enable);

static void        hyscan_sensor_proxy_data_forwarder          (HyScanSensorProxyPrivate  *priv,
                                                                const gchar               *name,
                                                                HyScanSensorProtocolType   protocol,
                                                                HyScanDataType             type,
                                                                HyScanDataWriterData      *data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSensorProxy, hyscan_sensor_proxy, HYSCAN_TYPE_SONAR_BOX)

static void
hyscan_sensor_proxy_class_init (HyScanSensorProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sensor_proxy_set_property;

  object_class->constructed = hyscan_sensor_proxy_object_constructed;
  object_class->finalize = hyscan_sensor_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "Sensor control", HYSCAN_TYPE_SENSOR_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_FORWARD_RAW,
    g_param_spec_boolean ("forward-raw", "ForawrdRAW", "Forward raw data",
                          FALSE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sensor_proxy_init (HyScanSensorProxy *proxy)
{
  proxy->priv = hyscan_sensor_proxy_get_instance_private (proxy);
}

static void
hyscan_sensor_proxy_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanSensorProxy *proxy = HYSCAN_SENSOR_PROXY (object);
  HyScanSensorProxyPrivate *priv = proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      priv->control = g_value_dup_object (value);
      break;

    case PROP_FORWARD_RAW:
      priv->forward_raw = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sensor_proxy_object_constructed (GObject *object)
{
  HyScanSensorProxy *proxy = HYSCAN_SENSOR_PROXY (object);
  HyScanSensorProxyPrivate *priv = proxy->priv;

  HyScanDataSchemaEnumValue **sonar_values;
  HyScanDataSchemaEnumValue **proxy_values;

  gint64 version;
  gint64 id;
  guint i, j;

  G_OBJECT_CLASS (hyscan_sensor_proxy_parent_class)->constructed (object);

  g_rw_lock_init (&priv->lock);

  /* Обязательно должен быть передан указатель на HyScanSensorControl. */
  if (priv->control == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (HYSCAN_DATA_BOX (proxy), "/schema/id", &id))
    {
      g_warning ("HyScanControlProxy: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_warning ("HyScanControlProxy: sonar schema id mismatch");
      return;
    }
  if (!hyscan_data_box_get_integer (HYSCAN_DATA_BOX (proxy), "/schema/version", &version))
    {
      g_warning ("HyScanControlProxy: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_warning ("HyScanControlProxy: sonar schema version mismatch");
      return;
    }

  /* Таблица трансляции идентификаторов UART устройств. */
  priv->uart_devices = g_hash_table_new (g_direct_hash, g_direct_equal);
  sonar_values = hyscan_sensor_control_list_uart_devices (priv->control);
  proxy_values = hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (proxy), "/sensors/uart-devices");

  for (i = 0; proxy_values != NULL && proxy_values[i] != NULL; i++)
    {
      for (j = 0; sonar_values != NULL && sonar_values[j] != NULL; j++)
        {
          if (g_strcmp0 (proxy_values[i]->name, sonar_values[j]->name) == 0)
            {
              guint proxy_id = proxy_values[i]->value;
              guint sonar_id = sonar_values[j]->value;

              g_hash_table_insert (priv->uart_devices, GINT_TO_POINTER (proxy_id), GINT_TO_POINTER (sonar_id));
              break;
            }
        }
    }

  g_clear_pointer (&sonar_values, hyscan_data_schema_free_enum_values);
  g_clear_pointer (&proxy_values, hyscan_data_schema_free_enum_values);

  /* Таблица трансляции идентификаторов режимов работы UART устройств. */
  priv->uart_modes = g_hash_table_new (g_direct_hash, g_direct_equal);
  sonar_values = hyscan_sensor_control_list_uart_modes (priv->control);
  proxy_values = hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (proxy), "/sensors/uart-modes");

  for (i = 0; proxy_values != NULL && proxy_values[i] != NULL; i++)
    {
      for (j = 0; sonar_values != NULL && sonar_values[j] != NULL; j++)
        {
          if (g_strcmp0 (proxy_values[i]->name, sonar_values[j]->name) == 0)
            {
              guint proxy_id = proxy_values[i]->value;
              guint sonar_id = sonar_values[j]->value;

              g_hash_table_insert (priv->uart_modes, GINT_TO_POINTER (proxy_id), GINT_TO_POINTER (sonar_id));
              break;
            }
        }
    }

  g_clear_pointer (&sonar_values, hyscan_data_schema_free_enum_values);
  g_clear_pointer (&proxy_values, hyscan_data_schema_free_enum_values);

  /* Таблица трансляции идентификаторов IP адресов. */
  priv->ip_addresses = g_hash_table_new (g_direct_hash, g_direct_equal);
  sonar_values = hyscan_sensor_control_list_ip_addresses (priv->control);
  proxy_values = hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (proxy), "/sensors/ip-addresses");

  for (i = 0; proxy_values != NULL && proxy_values[i] != NULL; i++)
    {
      for (j = 0; sonar_values != NULL && sonar_values[j] != NULL; j++)
        {
          if (g_strcmp0 (proxy_values[i]->name, sonar_values[j]->name) == 0)
            {
              guint proxy_id = proxy_values[i]->value;
              guint sonar_id = sonar_values[j]->value;

              g_hash_table_insert (priv->ip_addresses, GINT_TO_POINTER (proxy_id), GINT_TO_POINTER (sonar_id));
              break;
            }
        }
    }

  g_clear_pointer (&sonar_values, hyscan_data_schema_free_enum_values);
  g_clear_pointer (&proxy_values, hyscan_data_schema_free_enum_values);

  /* Прокси сервер. */
  priv->server = hyscan_sensor_control_server_new (HYSCAN_SONAR_BOX (proxy));

  /* Обработчики команд. */
  g_signal_connect_swapped (priv->server, "sensor-virtual-port-param",
                            G_CALLBACK (hyscan_sensor_proxy_set_virtual_port_param), priv);
  g_signal_connect_swapped (priv->server, "sensor-uart-port-param",
                            G_CALLBACK (hyscan_sensor_proxy_set_uart_port_param), priv);
  g_signal_connect_swapped (priv->server, "sensor-udp-ip-port-param",
                            G_CALLBACK (hyscan_sensor_proxy_set_udp_ip_port_param), priv);
  g_signal_connect_swapped (priv->server, "sensor-set-position",
                            G_CALLBACK (hyscan_sensor_proxy_set_position), priv);
  g_signal_connect_swapped (priv->server, "sensor-set-enable",
                            G_CALLBACK (hyscan_sensor_proxy_set_enable), priv);

  /* Обработчик данных. */
  g_signal_connect_swapped (priv->control, "sensor-data",
                            G_CALLBACK (hyscan_sensor_proxy_data_forwarder), priv);
}

static void
hyscan_sensor_proxy_object_finalize (GObject *object)
{
  HyScanSensorProxy *proxy = HYSCAN_SENSOR_PROXY (object);
  HyScanSensorProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  g_clear_pointer (&priv->uart_devices, g_hash_table_unref);
  g_clear_pointer (&priv->uart_modes, g_hash_table_unref);
  g_clear_pointer (&priv->ip_addresses, g_hash_table_unref);

  g_rw_lock_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_sensor_proxy_parent_class)->finalize (object);
}

/* Команда - hyscan_sensor_control_set_virtual_port_param. */
static gboolean
hyscan_sensor_proxy_set_virtual_port_param (HyScanSensorProxyPrivate *priv,
                                            const gchar              *name,
                                            guint                     channel,
                                            gint64                    time_offset)
{
  if (priv->forward_raw)
    return hyscan_sensor_control_set_virtual_port_param (priv->control, name, channel, time_offset);

  return FALSE;
}

/* Команда - hyscan_sensor_control_set_uart_port_param. */
static gboolean
hyscan_sensor_proxy_set_uart_port_param (HyScanSensorProxyPrivate *priv,
                                         const gchar              *name,
                                         guint                     channel,
                                         gint64                    time_offset,
                                         HyScanSensorProtocolType  protocol,
                                         guint                     uart_device,
                                         guint                     uart_mode)
{
  if (priv->forward_raw)
    {
      guint sonar_uart_device;
      guint sonar_uart_mode;

      sonar_uart_device = GPOINTER_TO_UINT (g_hash_table_lookup (priv->uart_devices, GUINT_TO_POINTER (uart_device)));
      sonar_uart_mode = GPOINTER_TO_UINT (g_hash_table_lookup (priv->uart_modes, GUINT_TO_POINTER (uart_mode)));

      return hyscan_sensor_control_set_uart_port_param (priv->control, name, channel, time_offset,
                                                        protocol, sonar_uart_device, sonar_uart_mode);
    }

  return FALSE;
}

/* Команда - hyscan_sensor_control_set_udp_ip_port_param. */
static gboolean
hyscan_sensor_proxy_set_udp_ip_port_param (HyScanSensorProxyPrivate *priv,
                                           const gchar              *name,
                                           guint                     channel,
                                           gint64                    time_offset,
                                           HyScanSensorProtocolType  protocol,
                                           guint                     ip_address,
                                           guint16                   udp_port)
{
  if (priv->forward_raw)
    {
      guint sonar_ip_address;

      sonar_ip_address = GPOINTER_TO_INT (g_hash_table_lookup (priv->ip_addresses, GINT_TO_POINTER (ip_address)));

      return hyscan_sensor_control_set_udp_ip_port_param (priv->control, name, channel, time_offset,
                                                          protocol, sonar_ip_address, udp_port);
    }

  return FALSE;
}

/* Команда - hyscan_sensor_control_set_position. */
static gboolean
hyscan_sensor_proxy_set_position (HyScanSensorProxyPrivate *priv,
                                  const gchar              *name,
                                  HyScanAntennaPosition    *position)
{
  if (priv->forward_raw)
    return hyscan_sensor_control_set_position (priv->control, name, position);

  return FALSE;
}

/* Команда - hyscan_sensor_control_set_enable. */
static gboolean
hyscan_sensor_proxy_set_enable (HyScanSensorProxyPrivate *priv,
                                const gchar              *name,
                                gboolean                  enable)
{
  if (priv->forward_raw)
    return hyscan_sensor_control_set_enable (priv->control, name, enable);

  return FALSE;
}

/* Функция перенаправляет данные датчиков. */
static void
hyscan_sensor_proxy_data_forwarder (HyScanSensorProxyPrivate *priv,
                                    const gchar              *name,
                                    HyScanSensorProtocolType  protocol,
                                    HyScanDataType            type,
                                    HyScanDataWriterData     *data)
{
  const gchar *forward_name;

  /* Перенаправляем данные от всех портов. */
  if (priv->forward_raw)
    {
      forward_name = name;
    }

  /* Проверяем название порта и тип данных. */
  else
    {
      gboolean forward = FALSE;

      if ((protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183) || (type != HYSCAN_DATA_STRING))
        return;

      g_rw_lock_reader_lock (&priv->lock);
      if (g_strcmp0 (priv->virtual_nmea_source, name) == 0)
        forward = TRUE;
      g_rw_lock_reader_unlock (&priv->lock);

      if (!forward)
        return;

      forward_name = HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME;
    }

  hyscan_sensor_control_server_send_data (priv->server, forward_name, type, data);
}

/* Функция выбирает порт, используемый как источник данных для виртуального NMEA порта. */
gboolean
hyscan_sensor_proxy_set_source (HyScanSensorProxy *proxy,
                                const gchar       *name)
{
  HyScanSensorProxyPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_PROXY (proxy), FALSE);

  priv = proxy->priv;

  if (!priv->forward_raw)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  g_clear_pointer (&priv->virtual_nmea_source, g_free);
  priv->virtual_nmea_source = g_strdup (name);

  g_rw_lock_writer_unlock (&priv->lock);

  return TRUE;
}
