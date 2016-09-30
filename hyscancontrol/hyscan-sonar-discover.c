/*
 * \file hyscan-sonar-discover.c
 *
 * \brief Исходный файл интерфейса обнаружения гидролокаторов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-discover.h"

G_DEFINE_INTERFACE (HyScanSonarDiscover, hyscan_sonar_discover, G_TYPE_OBJECT)

static void
hyscan_sonar_discover_default_init (HyScanSonarDiscoverInterface *iface)
{
}

gboolean
hyscan_sonar_discover_begin (HyScanSonarDiscover *discover)
{
  HyScanSonarDiscoverInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_DISCOVER (discover), FALSE);

  iface = HYSCAN_SONAR_DISCOVER_GET_IFACE (discover);
  if (iface->begin != NULL)
    return (* iface->begin) (discover);

  return FALSE;
}

gboolean
hyscan_sonar_discover_stop (HyScanSonarDiscover *discover)
{
  HyScanSonarDiscoverInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_DISCOVER (discover), FALSE);

  iface = HYSCAN_SONAR_DISCOVER_GET_IFACE (discover);
  if (iface->stop != NULL)
    return (* iface->stop) (discover);

  return FALSE;
}

guint
hyscan_sonar_discover_progress (HyScanSonarDiscover *discover)
{
  HyScanSonarDiscoverInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_DISCOVER (discover), 0);

  iface = HYSCAN_SONAR_DISCOVER_GET_IFACE (discover);
  if (iface->progress != NULL)
    return (* iface->progress) (discover);

  return 0;
}

HyScanSonarDiscoverInfo **
hyscan_sonar_discover_list (HyScanSonarDiscover *discover)
{
  HyScanSonarDiscoverInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_DISCOVER (discover), NULL);

  iface = HYSCAN_SONAR_DISCOVER_GET_IFACE (discover);
  if (iface->list != NULL)
    return (* iface->list) (discover);

  return NULL;
}
