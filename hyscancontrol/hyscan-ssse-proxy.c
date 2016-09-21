/*
 * \file hyscan-ssse-proxy.c
 *
 * \brief Исходный файл класса прокси сервера ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control.h"
#include "hyscan-ssse-proxy.h"
#include "hyscan-ssse-control.h"
#include "hyscan-ssse-control-server.h"
#include "hyscan-control-common.h"
#include "hyscan-sonar-schema.h"

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_FORWARD_MODE,
  PROP_SIDE_SCALE,
  PROP_TRACK_SCALE
};

struct _HyScanSSSEProxyPrivate
{
  HyScanSSSEControl           *control;                        /* Клиент управления проксируемым гидролокатором. */
  HyScanSSSEControlServer     *server;                         /* Сервер прокси гидролокатора. */

  HyScanSSSEProxyForwardMode   forward_mode;                   /* Режим трансляции команд и данных. */

  guint                        side_scale;                     /* Коэффициент масштабирования по наклонной дальности. */
  guint                        track_scale;                    /* Коэффициент масштабирования по оси движения. */
};

static void        hyscan_ssse_proxy_set_property              (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void        hyscan_ssse_proxy_object_constructed        (GObject                   *object);
static void        hyscan_ssse_proxy_object_finalize           (GObject                   *object);

static void        hyscan_ssse_proxy_data_forwarder            (HyScanSSSEProxyPrivate    *priv,
                                                                HyScanSourceType           source,
                                                                HyScanAcousticDataInfo    *info,
                                                                HyScanDataWriterData      *data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSSSEProxy, hyscan_ssse_proxy, HYSCAN_TYPE_SONAR_PROXY)

static void
hyscan_ssse_proxy_class_init (HyScanSSSEProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_ssse_proxy_set_property;

  object_class->constructed = hyscan_ssse_proxy_object_constructed;
  object_class->finalize = hyscan_ssse_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "SSSE control", HYSCAN_TYPE_SSSE_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_FORWARD_MODE,
    g_param_spec_int ("forward-mode", "ForwardMode", "Forward mode",
                      HYSCAN_SSSE_PROXY_FORWARD_RAW, HYSCAN_SSSE_PROXY_FORWARD_RAW_COMPUTED,
                      HYSCAN_SSSE_PROXY_FORWARD_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SIDE_SCALE,
    g_param_spec_uint ("side-scale", "SideScale", "Side scale",
                      1, 1024, 1, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_SCALE,
    g_param_spec_uint ("track-scale", "TrackScale", "Track scale",
                      1, 1024, 1, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_ssse_proxy_init (HyScanSSSEProxy *proxy)
{
  proxy->priv = hyscan_ssse_proxy_get_instance_private (proxy);
}

static void
hyscan_ssse_proxy_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanSSSEProxy *ssse_proxy = HYSCAN_SSSE_PROXY (object);
  HyScanSSSEProxyPrivate *priv = ssse_proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->control = g_value_dup_object (value);
      break;

    case PROP_FORWARD_MODE:
      priv->forward_mode = g_value_get_int (value);
      break;

    case PROP_SIDE_SCALE:
      priv->side_scale = g_value_get_uint (value);
      break;

    case PROP_TRACK_SCALE:
      priv->track_scale = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_ssse_proxy_object_constructed (GObject *object)
{
  HyScanSSSEProxy *proxy = HYSCAN_SSSE_PROXY (object);
  HyScanSSSEProxyPrivate *priv = proxy->priv;

  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanSSSEControl. */
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
  priv->server = hyscan_ssse_control_server_new (HYSCAN_SONAR_BOX (proxy));

  if ((priv->forward_mode == HYSCAN_SSSE_PROXY_FORWARD_RAW) ||
      (priv->forward_mode == HYSCAN_SSSE_PROXY_FORWARD_COMPUTED))
    {
      g_signal_connect_swapped (priv->control, "acoustic-data",
                                G_CALLBACK (hyscan_ssse_proxy_data_forwarder), priv);
    }
}

static void
hyscan_ssse_proxy_object_finalize (GObject *object)
{
  HyScanSSSEProxy *proxy = HYSCAN_SSSE_PROXY (object);
  HyScanSSSEProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->finalize (object);
}

/* Функция перенаправляет акустические данные от гидролокатора. */
static void
hyscan_ssse_proxy_data_forwarder (HyScanSSSEProxyPrivate *priv,
                                  HyScanSourceType        source,
                                  HyScanAcousticDataInfo *info,
                                  HyScanDataWriterData   *data)
{
  if (priv->forward_mode == HYSCAN_SSSE_PROXY_FORWARD_RAW)
    {
      hyscan_ssse_control_server_send_acoustic_data (priv->server, source, info->data.type, info->data.rate, data);
    }
}

/* Функция создаёт новый объект HyScanSSSEProxy. */
HyScanSSSEProxy *
hyscan_ssse_proxy_new (HyScanSonar                *sonar,
                       HyScanSSSEProxyForwardMode  forward_mode,
                       guint                       side_scale,
                       guint                       track_scale,
                       HyScanDB                   *db)
{
  HyScanSSSEProxy *proxy = NULL;
  HyScanSSSEControl *control;

  gboolean forward_raw = FALSE;
  gchar *schema_data = NULL;

  /* Проверяем тип гидролокатора. */
  if ((sonar == NULL) || (hyscan_control_sonar_probe (sonar) != HYSCAN_SONAR_SSSE))
    return NULL;

  /* Объект управления гидролокатором. */
  control = hyscan_ssse_control_new (sonar, db);

  /* Трансляция 1:1. */
  if (forward_mode == HYSCAN_SSSE_PROXY_FORWARD_RAW)
    {
      forward_raw = TRUE;
      schema_data = hyscan_data_schema_get_data (HYSCAN_DATA_SCHEMA (sonar));
    }
  else
    {
      goto exit;
    }

  proxy = g_object_new (HYSCAN_TYPE_SSSE_PROXY,
                        "control", control,
                        "schema-data", schema_data,
                        "schema-id", "sonar",
                        "forward-mode", forward_mode,
                        "side-scale", side_scale,
                        "track-scale", track_scale,
                        "forward-raw", forward_raw,
                        NULL);

exit:
  g_object_unref (control);
  g_free (schema_data);

  return proxy;
}
