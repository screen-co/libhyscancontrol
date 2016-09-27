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
#include "hyscan-proxy-common.h"

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_PROXY_MODE
};

struct _HyScanSensorProxyPrivate
{
  HyScanSensorControl         *control;                        /* Клиент управления проксируемыми датчиками. */
  HyScanSensorControlServer   *server;                         /* Прокси сервер датчиков. */

  HyScanSonarProxyMode         proxy_mode;                     /* Режим трансляции команд и данных. */
  gchar                       *virtual_source;                 /* Название порта источника данных для виртуального NMEA порта. */
  gboolean                     virtual_enable;                 /* Признак включения виртуального порта. */

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

  g_object_class_install_property (object_class, PROP_PROXY_MODE,
    g_param_spec_int ("proxy-mode", "ProxyMode", "Proxy mode",
                      HYSCAN_SONAR_PROXY_FORWARD_ALL, HYSCAN_SONAR_PROXY_FORWARD_COMPUTED,
                      HYSCAN_SONAR_PROXY_FORWARD_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
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

    case PROP_PROXY_MODE:
      priv->proxy_mode = g_value_get_int (value);
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

  gint64 version;
  gint64 id;

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

  g_free (priv->virtual_source);

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
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      return hyscan_sensor_control_set_virtual_port_param (priv->control, name, channel, time_offset);
    }
  else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_COMPUTED)
    {
      gboolean status = FALSE;

      g_rw_lock_reader_lock (&priv->lock);
      if (g_strcmp0 (priv->virtual_source, name) == 0)
        status = TRUE;
      g_rw_lock_reader_unlock (&priv->lock);

      return status;
    }

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
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      return hyscan_sensor_control_set_uart_port_param (priv->control, name, channel, time_offset,
                                                        protocol, uart_device, uart_mode);
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
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      return hyscan_sensor_control_set_udp_ip_port_param (priv->control, name, channel, time_offset,
                                                          protocol, ip_address, udp_port);
    }

  return FALSE;
}

/* Команда - hyscan_sensor_control_set_position. */
static gboolean
hyscan_sensor_proxy_set_position (HyScanSensorProxyPrivate *priv,
                                  const gchar              *name,
                                  HyScanAntennaPosition    *position)
{
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      return hyscan_sensor_control_set_position (priv->control, name, position);
    }
  else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_COMPUTED)
    {
      gboolean status = FALSE;

      if (g_strcmp0 (name, HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME) == 0)
        status = TRUE;

      return status;
    }

  return FALSE;
}

/* Команда - hyscan_sensor_control_set_enable. */
static gboolean
hyscan_sensor_proxy_set_enable (HyScanSensorProxyPrivate *priv,
                                const gchar              *name,
                                gboolean                  enable)
{
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      return hyscan_sensor_control_set_enable (priv->control, name, enable);
    }
  else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_COMPUTED)
    {
      gboolean status = FALSE;

      g_rw_lock_reader_lock (&priv->lock);
      if (g_strcmp0 (name, HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME) == 0)
        {
          priv->virtual_enable = enable;
          status = TRUE;
        }
      g_rw_lock_reader_unlock (&priv->lock);

      return status;
    }

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
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      hyscan_sensor_control_server_send_data (priv->server, name, type, data);
    }
  else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_COMPUTED)
    {
      gboolean forward = FALSE;

      if ((protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183) || (type != HYSCAN_DATA_STRING))
        return;

      g_rw_lock_reader_lock (&priv->lock);
      if ((g_strcmp0 (name, priv->virtual_source) == 0) && (priv->virtual_enable))
        forward = TRUE;
      g_rw_lock_reader_unlock (&priv->lock);

      if (forward)
        hyscan_sensor_control_server_send_data (priv->server, HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME, type, data);
    }
}

/* Функция выбирает порт, используемый как источник данных для виртуального NMEA порта. */
gboolean
hyscan_sensor_proxy_set_source (HyScanSensorProxy *proxy,
                                const gchar       *name)
{
  HyScanSensorProxyPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_PROXY (proxy), FALSE);

  priv = proxy->priv;

  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);
  g_clear_pointer (&priv->virtual_source, g_free);
  priv->virtual_source = g_strdup (name);
  g_rw_lock_writer_unlock (&priv->lock);

  return TRUE;
}
