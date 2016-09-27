/*
 * \file hyscan-sonar-proxy.c
 *
 * \brief Исходный файл класса прокси сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-proxy.h"
#include "hyscan-sonar-control.h"
#include "hyscan-sonar-control-server.h"
#include "hyscan-control-common.h"

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_PROXY_MODE
};

struct _HyScanSonarProxyPrivate
{
  HyScanSonarControl          *control;                        /* Клиент управления проксируемым гидролокатором. */
  HyScanSonarControlServer    *server;                         /* Прокси сервер гидролокатора. */

  HyScanSonarProxyMode         proxy_mode;                     /* Режим трансляции команд и данных. */
};

static void        hyscan_sonar_proxy_set_property             (GObject                     *object,
                                                                guint                        prop_id,
                                                                const GValue                *value,
                                                                GParamSpec                  *pspec);
static void        hyscan_sonar_proxy_object_constructed       (GObject                     *object);
static void        hyscan_sonar_proxy_object_finalize          (GObject                     *object);

static gboolean    hyscan_sonar_proxy_set_sync_type            (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSonarSyncType          sync_type);
static gboolean    hyscan_sonar_proxy_enable_raw_data          (HyScanSonarProxyPrivate     *priv,
                                                                gboolean                     enable);
static gboolean    hyscan_sonar_proxy_set_position             (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                HyScanAntennaPosition       *position);
static gboolean    hyscan_sonar_proxy_set_receive_time         (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                gdouble                      receive_time);
static gboolean    hyscan_sonar_proxy_start                    (HyScanSonarProxyPrivate     *priv,
                                                                const gchar                 *project_name,
                                                                const gchar                 *track_name,
                                                                HyScanTrackType              track_type);
static gboolean    hyscan_sonar_proxy_stop                     (HyScanSonarProxyPrivate     *priv);
static gboolean    hyscan_sonar_proxy_ping                     (HyScanSonarProxyPrivate     *priv);

static void        hyscan_sonar_proxy_raw_data_forwarder       (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                guint                        channel,
                                                                HyScanRawDataInfo           *info,
                                                                HyScanDataWriterData        *data);
static void        hyscan_sonar_proxy_noise_data_forwarder     (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                guint                        channel,
                                                                HyScanRawDataInfo           *info,
                                                                HyScanDataWriterData        *data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarProxy, hyscan_sonar_proxy, HYSCAN_TYPE_TVG_PROXY)

static void
hyscan_sonar_proxy_class_init (HyScanSonarProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_proxy_set_property;

  object_class->constructed = hyscan_sonar_proxy_object_constructed;
  object_class->finalize = hyscan_sonar_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "Sonar control", HYSCAN_TYPE_SONAR_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROXY_MODE,
    g_param_spec_int ("proxy-mode", "ProxyMode", "Proxy mode",
                      HYSCAN_SONAR_PROXY_FORWARD_ALL, HYSCAN_SONAR_PROXY_FORWARD_COMPUTED,
                      HYSCAN_SONAR_PROXY_FORWARD_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_proxy_init (HyScanSonarProxy *proxy)
{
  proxy->priv = hyscan_sonar_proxy_get_instance_private (proxy);
}

static void
hyscan_sonar_proxy_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanSonarProxy *proxy = HYSCAN_SONAR_PROXY (object);
  HyScanSonarProxyPrivate *priv = proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->control = g_value_dup_object (value);
      break;

    case PROP_PROXY_MODE:
      G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->proxy_mode = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_proxy_object_constructed (GObject *object)
{
  HyScanSonarProxy *proxy = HYSCAN_SONAR_PROXY (object);
  HyScanSonarProxyPrivate *priv = proxy->priv;

  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanSonarControl. */
  if (priv->control == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (HYSCAN_DATA_BOX (proxy), "/schema/id", &id))
    return;
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    return;
  if (!hyscan_data_box_get_integer (HYSCAN_DATA_BOX (proxy), "/schema/version", &version))
    return;
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    return;

  /* Прокси сервер. */
  priv->server = hyscan_sonar_control_server_new (HYSCAN_SONAR_BOX (proxy));

  /* Обработчики команд. */
  g_signal_connect_swapped (priv->server, "sonar-set-sync-type",
                            G_CALLBACK (hyscan_sonar_proxy_set_sync_type), priv);
  g_signal_connect_swapped (priv->server, "sonar-enable-raw-data",
                            G_CALLBACK (hyscan_sonar_proxy_enable_raw_data), priv);
  g_signal_connect_swapped (priv->server, "sonar-set-position",
                            G_CALLBACK (hyscan_sonar_proxy_set_position), priv);
  g_signal_connect_swapped (priv->server, "sonar-set-receive-time",
                            G_CALLBACK (hyscan_sonar_proxy_set_receive_time), priv);
  g_signal_connect_swapped (priv->server, "sonar-start",
                            G_CALLBACK (hyscan_sonar_proxy_start), priv);
  g_signal_connect_swapped (priv->server, "sonar-stop",
                            G_CALLBACK (hyscan_sonar_proxy_stop), priv);
  g_signal_connect_swapped (priv->server, "sonar-ping",
                            G_CALLBACK (hyscan_sonar_proxy_ping), priv);

  /* Обработчики "сырых" данных от гидролокатора. */
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_ALL)
    {
      g_signal_connect_swapped (priv->control, "raw-data",
                                G_CALLBACK (hyscan_sonar_proxy_raw_data_forwarder), priv);
      g_signal_connect_swapped (priv->control, "noise-data",
                                G_CALLBACK (hyscan_sonar_proxy_noise_data_forwarder), priv);
    }
}

static void
hyscan_sonar_proxy_object_finalize (GObject *object)
{
  HyScanSonarProxy *proxy = HYSCAN_SONAR_PROXY (object);
  HyScanSonarProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->finalize (object);
}

/* Команда - hyscan_sonar_control_set_sync_type. */
static gboolean
hyscan_sonar_proxy_set_sync_type (HyScanSonarProxyPrivate *priv,
                                  HyScanSonarSyncType      sync_type)
{
  return hyscan_sonar_control_set_sync_type (priv->control, sync_type);
}

/* Команда - hyscan_sonar_control_enable_raw_data. */
static gboolean
hyscan_sonar_proxy_enable_raw_data (HyScanSonarProxyPrivate *priv,
                                    gboolean                 enable)
{
  return hyscan_sonar_control_enable_raw_data (priv->control, enable);
}

/* Команда - hyscan_sonar_control_set_position. */
static gboolean
hyscan_sonar_proxy_set_position (HyScanSonarProxyPrivate *priv,
                                 HyScanSourceType         source,
                                 HyScanAntennaPosition   *position)
{
  return hyscan_sonar_control_set_position (priv->control, source, position);
}

/* Команда - hyscan_sonar_control_receive_time. */
static gboolean
hyscan_sonar_proxy_set_receive_time (HyScanSonarProxyPrivate *priv,
                                     HyScanSourceType         source,
                                     gdouble                  receive_time)
{
  return hyscan_sonar_control_set_receive_time (priv->control, source, receive_time);
}

/* Команда - hyscan_sonar_control_start. */
static gboolean
hyscan_sonar_proxy_start (HyScanSonarProxyPrivate *priv,
                          const gchar             *project_name,
                          const gchar             *track_name,
                          HyScanTrackType          track_type)
{
  return hyscan_sonar_control_start (priv->control, project_name, track_name, track_type);
}

/* Команда - hyscan_sonar_control_stop. */
static gboolean
hyscan_sonar_proxy_stop (HyScanSonarProxyPrivate *priv)
{
  return hyscan_sonar_control_stop (priv->control);
}

/* Команда - hyscan_sonar_control_ping. */
static gboolean
hyscan_sonar_proxy_ping (HyScanSonarProxyPrivate *priv)
{
  return hyscan_sonar_control_ping (priv->control);
}

/* Функция перенаправляет "сырые" данные от гидролокатора. */
static void
hyscan_sonar_proxy_raw_data_forwarder (HyScanSonarProxyPrivate *priv,
                                       HyScanSourceType         source,
                                       guint                    channel,
                                       HyScanRawDataInfo       *info,
                                       HyScanDataWriterData    *data)
{
  hyscan_sonar_control_server_send_raw_data (priv->server, source, channel, info->data.type, info->data.rate, data);
}

/* Функция перенаправляет данные по шумам от гидролокатора. */
static void
hyscan_sonar_proxy_noise_data_forwarder (HyScanSonarProxyPrivate *priv,
                                         HyScanSourceType         source,
                                         guint                    channel,
                                         HyScanRawDataInfo       *info,
                                         HyScanDataWriterData    *data)
{
  hyscan_sonar_control_server_send_noise_data (priv->server, source, channel, info->data.type, info->data.rate, data);
}
