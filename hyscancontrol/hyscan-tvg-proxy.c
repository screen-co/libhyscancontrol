/*
 * \file hyscan-tvg-proxy.c
 *
 * \brief Исходный файл класса прокси сервера управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-tvg-proxy.h"
#include "hyscan-tvg-control.h"
#include "hyscan-tvg-control-server.h"
#include "hyscan-control-common.h"

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_SONAR_PROXY_MODE
};

struct _HyScanTVGProxyPrivate
{
  HyScanTVGControl            *control;                        /* Клиент управления проксируемой системой ВАРУ. */
  HyScanTVGControlServer      *server;                         /* Прокси сервер системы ВАРУ. */

  HyScanSonarProxyModeType     proxy_mode;                     /* Режим трансляции команд и данных. */
};

static void        hyscan_tvg_proxy_set_property               (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void        hyscan_tvg_proxy_object_constructed         (GObject               *object);
static void        hyscan_tvg_proxy_object_finalize            (GObject               *object);

static gboolean    hyscan_tvg_proxy_set_auto                   (HyScanTVGProxyPrivate *priv,
                                                                HyScanSourceType       source,
                                                                gdouble                level,
                                                                gdouble                sensitivity);
static gboolean    hyscan_tvg_proxy_set_constant               (HyScanTVGProxyPrivate *priv,
                                                                HyScanSourceType       source,
                                                                gdouble                gain);
static gboolean    hyscan_tvg_proxy_set_linear_db              (HyScanTVGProxyPrivate *priv,
                                                                HyScanSourceType       source,
                                                                gdouble                gain0,
                                                                gdouble                step);
static gboolean    hyscan_tvg_proxy_set_logarithmic            (HyScanTVGProxyPrivate *priv,
                                                                HyScanSourceType       source,
                                                                gdouble                gain0,
                                                                gdouble                beta,
                                                                gdouble                alpha);
static gboolean    hyscan_tvg_proxy_set_enable                 (HyScanTVGProxyPrivate *priv,
                                                                HyScanSourceType       source,
                                                                gboolean               enable);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTVGProxy, hyscan_tvg_proxy, HYSCAN_TYPE_GENERATOR_PROXY)

static void
hyscan_tvg_proxy_class_init (HyScanTVGProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_tvg_proxy_set_property;

  object_class->constructed = hyscan_tvg_proxy_object_constructed;
  object_class->finalize = hyscan_tvg_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "TVG control", HYSCAN_TYPE_TVG_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SONAR_PROXY_MODE,
    g_param_spec_int ("sonar-proxy-mode", "SonarProxyMode", "Sonar proxy mode",
                      HYSCAN_SONAR_PROXY_MODE_ALL, HYSCAN_SONAR_PROXY_MODE_COMPUTED,
                      HYSCAN_SONAR_PROXY_MODE_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_tvg_proxy_init (HyScanTVGProxy *proxy)
{
  proxy->priv = hyscan_tvg_proxy_get_instance_private (proxy);
}

static void
hyscan_tvg_proxy_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanTVGProxy *proxy = HYSCAN_TVG_PROXY (object);
  HyScanTVGProxyPrivate *priv = proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      G_OBJECT_CLASS (hyscan_tvg_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->control = g_value_dup_object (value);
      break;

    case PROP_SONAR_PROXY_MODE:
      G_OBJECT_CLASS (hyscan_tvg_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->proxy_mode = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_tvg_proxy_object_constructed (GObject *object)
{
  HyScanTVGProxy *proxy = HYSCAN_TVG_PROXY (object);
  HyScanTVGProxyPrivate *priv = proxy->priv;

  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_tvg_proxy_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanTVGControl. */
  if (priv->control == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_param_get_integer (HYSCAN_PARAM (proxy), "/schema/id", &id))
    return;
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    return;
  if (!hyscan_param_get_integer (HYSCAN_PARAM (proxy), "/schema/version", &version))
    return;
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    return;

  /* Прокси сервер. */
  priv->server = hyscan_tvg_control_server_new (HYSCAN_SONAR_BOX (proxy));

  /* Обработчики команд. */
  g_signal_connect_swapped (priv->server, "tvg-set-auto",
                            G_CALLBACK (hyscan_tvg_proxy_set_auto), priv);
  g_signal_connect_swapped (priv->server, "tvg-set-constant",
                            G_CALLBACK (hyscan_tvg_proxy_set_constant), priv);
  g_signal_connect_swapped (priv->server, "tvg-set-linear-db",
                            G_CALLBACK (hyscan_tvg_proxy_set_linear_db), priv);
  g_signal_connect_swapped (priv->server, "tvg-set-logarithmic",
                            G_CALLBACK (hyscan_tvg_proxy_set_logarithmic), priv);
  g_signal_connect_swapped (priv->server, "tvg-set-enable",
                            G_CALLBACK (hyscan_tvg_proxy_set_enable), priv);

  /* Обработчик параметров ВАРУ. */
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      g_signal_connect_swapped (priv->control, "gains",
                                G_CALLBACK (hyscan_tvg_control_server_send_gains), priv->server);
    }
}

static void
hyscan_tvg_proxy_object_finalize (GObject *object)
{
  HyScanTVGProxy *proxy = HYSCAN_TVG_PROXY (object);
  HyScanTVGProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv->server);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  G_OBJECT_CLASS (hyscan_tvg_proxy_parent_class)->finalize (object);
}

/* Команда - hyscan_tvg_control_set_auto. */
static gboolean
hyscan_tvg_proxy_set_auto (HyScanTVGProxyPrivate *priv,
                           HyScanSourceType       source,
                           gdouble                level,
                           gdouble                sensitivity)
{
  return hyscan_tvg_control_set_auto (priv->control, source, level, sensitivity);
}

/* Команда - hyscan_tvg_control_set_constant. */
static gboolean
hyscan_tvg_proxy_set_constant (HyScanTVGProxyPrivate *priv,
                               HyScanSourceType       source,
                               gdouble                gain)
{
  return hyscan_tvg_control_set_constant (priv->control, source, gain);
}

/* Команда - hyscan_tvg_control_set_linear_db. */
static gboolean
hyscan_tvg_proxy_set_linear_db (HyScanTVGProxyPrivate *priv,
                                HyScanSourceType       source,
                                gdouble                gain0,
                                gdouble                step)
{
  return hyscan_tvg_control_set_linear_db (priv->control, source, gain0, step);
}

/* Команда - hyscan_tvg_control_set_logarithmic. */
static gboolean
hyscan_tvg_proxy_set_logarithmic (HyScanTVGProxyPrivate *priv,
                                  HyScanSourceType       source,
                                  gdouble                gain0,
                                  gdouble                beta,
                                  gdouble                alpha)
{
  return hyscan_tvg_control_set_logarithmic (priv->control, source, gain0, beta, alpha);
}

/* Команда - hyscan_tvg_control_set_enable. */
static gboolean
hyscan_tvg_proxy_set_enable (HyScanTVGProxyPrivate *priv,
                             HyScanSourceType       source,
                             gboolean               enable)
{
  return hyscan_tvg_control_set_enable (priv->control, source, enable);
}
