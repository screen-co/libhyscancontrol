/*
 * \file hyscan-control.c
 *
 * \brief Исходный файл библиотеки высокоуровневого управления гидролокаторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <hyscan-control.h>
#include "hyscan-control-common.h"

/* Функция возвращает тип гидролокатора. */
HyScanSonarType
hyscan_control_sonar_probe (HyScanSonar *sonar)
{
  HyScanDataSchema *schema;

  gchar *echosounder_id_key;
  gchar *starboard_id_key;
  gchar *port_id_key;
  gchar *starboard_hi_id_key;
  gchar *port_hi_id_key;

  gboolean has_echosounder;
  gboolean has_starboard;
  gboolean has_port;
  gboolean has_starboard_hi;
  gboolean has_port_hi;

  if (sonar == NULL)
    return HYSCAN_SONAR_INVALID;

  schema = hyscan_sonar_get_schema (sonar);
  if (schema == NULL)
    return HYSCAN_SONAR_INVALID;

  echosounder_id_key =  g_strdup_printf ("/sources/%s/id",
                                         hyscan_control_get_source_name (HYSCAN_SOURCE_ECHOSOUNDER));
  starboard_id_key =    g_strdup_printf ("/sources/%s/id",
                                         hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD));
  port_id_key =         g_strdup_printf ("/sources/%s/id",
                                         hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_PORT));
  starboard_hi_id_key = g_strdup_printf ("/sources/%s/id",
                                         hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI));
  port_hi_id_key =      g_strdup_printf ("/sources/%s/idd",
                                         hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_PORT_HI));

  has_echosounder = hyscan_data_schema_has_key (schema, echosounder_id_key);
  has_starboard = hyscan_data_schema_has_key (schema, starboard_id_key);
  has_port = hyscan_data_schema_has_key (schema, port_id_key);
  has_starboard_hi = hyscan_data_schema_has_key (schema, starboard_hi_id_key);
  has_port_hi = hyscan_data_schema_has_key (schema, port_hi_id_key);

  g_free (echosounder_id_key);
  g_free (starboard_id_key);
  g_free (port_id_key);
  g_free (starboard_hi_id_key);
  g_free (port_hi_id_key);

  g_object_unref (schema);

  if (has_echosounder && !has_starboard && !has_port && !has_starboard_hi && !has_port_hi)
    return HYSCAN_SONAR_ECHO;

  if (!has_echosounder && (has_starboard || has_port) && !has_starboard_hi && !has_port_hi)
    return HYSCAN_SONAR_SSS;

  if (has_echosounder && (has_starboard || has_port) && !has_starboard_hi && !has_port_hi)
    return HYSCAN_SONAR_SSSE;

  if (!has_echosounder && (has_starboard || has_port) && (has_starboard_hi || has_port_hi))
    return HYSCAN_SONAR_DSSS;

  if (has_echosounder && (has_starboard || has_port) && (has_starboard_hi || has_port_hi))
    return HYSCAN_SONAR_DSSSE;

  return HYSCAN_SONAR_INVALID;
}
