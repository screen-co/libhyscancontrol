/*
 * \file hyscan-sonar-driver.c
 *
 * \brief Исходный файл класса загрузки драйвера гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarDriver HyScanSonarDriver - класс загрузки драйвера гидролокатора
 *
 *
 */

#include "hyscan-sonar-driver.h"

#include <gmodule.h>

enum
{
  PROP_O,
  PROP_PATH,
  PROP_NAME
};

typedef HyScanSonarDiscover *(*hyscan_sonar_discover_func) (void);

struct _HyScanSonarDriverPrivate
{
  gchar                       *path;
  gchar                       *name;

  GModule                     *module;
  HyScanSonarDiscover         *driver;
};

static void    hyscan_sonar_driver_interface_init      (HyScanSonarDiscoverInterface  *iface);
static void    hyscan_sonar_driver_set_property        (GObject                       *object,
                                                        guint                          prop_id,
                                                        const GValue                  *value,
                                                        GParamSpec                    *pspec);
static void    hyscan_sonar_driver_object_constructed  (GObject                       *object);
static void    hyscan_sonar_driver_object_finalize     (GObject                       *object);

G_DEFINE_TYPE_WITH_CODE (HyScanSonarDriver, hyscan_sonar_driver, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarDriver)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR_DISCOVER, hyscan_sonar_driver_interface_init))

static void
hyscan_sonar_driver_class_init (HyScanSonarDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_driver_set_property;

  object_class->constructed = hyscan_sonar_driver_object_constructed;
  object_class->finalize = hyscan_sonar_driver_object_finalize;

  g_object_class_install_property (object_class, PROP_PATH,
    g_param_spec_string ("path", "Path", "Path to sonar drivers", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Name", "Driver name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_driver_init (HyScanSonarDriver *driver)
{
  driver->priv = hyscan_sonar_driver_get_instance_private (driver);
}

static void
hyscan_sonar_driver_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (object);
  HyScanSonarDriverPrivate *priv = driver->priv;

  switch (prop_id)
    {
    case PROP_PATH:
      priv->path = g_value_dup_string (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_driver_object_constructed (GObject *object)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (object);
  HyScanSonarDriverPrivate *priv = driver->priv;

  gchar *module_name;
  gchar *module_path;

  if (priv->path == NULL || priv->name == NULL)
    return;

  /* Путь к файлу драйвера. */
  module_name = g_strdup_printf ("hyscan-sonar-%s-drv.%s", priv->name, G_MODULE_SUFFIX);
  module_path = g_build_filename (priv->path, module_name, NULL);

  /* Загрузка драйвера. */
  priv->module = g_module_open (module_path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
  if (priv->module != NULL)
    {
      hyscan_sonar_discover_func discover;

      if (g_module_symbol (priv->module, "hyscan_sonar_driver", (gpointer *) &discover))
        if (discover != NULL)
          priv->driver = discover ();
    }

  g_free (module_name);
  g_free (module_path);
}

static void
hyscan_sonar_driver_object_finalize (GObject *object)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (object);
  HyScanSonarDriverPrivate *priv = driver->priv;

  g_free (priv->path);
  g_free (priv->name);

  g_clear_object (&priv->driver);
  g_clear_pointer (&priv->module, g_module_close);

  G_OBJECT_CLASS (hyscan_sonar_driver_parent_class)->finalize (object);
}

static gboolean
hyscan_sonar_driver_discover_begin (HyScanSonarDiscover *discover)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (discover);
  HyScanSonarDriverPrivate *priv = driver->priv;

  if (priv->driver == NULL)
    return FALSE;

  return hyscan_sonar_discover_begin (priv->driver);
}

static gboolean
hyscan_sonar_driver_discover_stop (HyScanSonarDiscover *discover)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (discover);
  HyScanSonarDriverPrivate *priv = driver->priv;

  if (priv->driver == NULL)
    return FALSE;

  return hyscan_sonar_discover_stop (priv->driver);
}


static guint
hyscan_sonar_driver_discover_progress (HyScanSonarDiscover *discover)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (discover);
  HyScanSonarDriverPrivate *priv = driver->priv;

  if (priv->driver == NULL)
    return 0;

  return hyscan_sonar_discover_progress (priv->driver);
}

static HyScanSonarDiscoverInfo **
hyscan_sonar_driver_discover_list (HyScanSonarDiscover *discover)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (discover);
  HyScanSonarDriverPrivate *priv = driver->priv;

  if (priv->driver == NULL)
    return NULL;

  return hyscan_sonar_discover_list (priv->driver);
}

static HyScanParam *
hyscan_sonar_driver_discover_connect (HyScanSonarDiscover *discover,
                                      const gchar         *uri,
                                      const gchar         *config)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (discover);
  HyScanSonarDriverPrivate *priv = driver->priv;

  if (priv->driver == NULL)
    return NULL;

  return hyscan_sonar_discover_connect (priv->driver, uri, config);
}

static HyScanDataBox *
hyscan_sonar_driver_discover_config (HyScanSonarDiscover *discover,
                                     const gchar         *uri)
{
  HyScanSonarDriver *driver = HYSCAN_SONAR_DRIVER (discover);
  HyScanSonarDriverPrivate *priv = driver->priv;

  if (priv->driver == NULL)
    return NULL;

  return hyscan_sonar_discover_config (priv->driver, uri);
}

HyScanSonarDriver *
hyscan_sonar_driver_new (const gchar *path,
                         const gchar *name)
{
  HyScanSonarDriver *driver;

  driver = g_object_new (HYSCAN_TYPE_SONAR_DRIVER, "path", path, "name", name, NULL);
  if (driver->priv->driver == NULL)
    g_clear_object (&driver);

  return driver;
}

static void
hyscan_sonar_driver_interface_init (HyScanSonarDiscoverInterface *iface)
{
  iface->begin = hyscan_sonar_driver_discover_begin;
  iface->stop = hyscan_sonar_driver_discover_stop;
  iface->progress = hyscan_sonar_driver_discover_progress;
  iface->list = hyscan_sonar_driver_discover_list;
  iface->connect = hyscan_sonar_driver_discover_connect;
  iface->config = hyscan_sonar_driver_discover_config;
}
