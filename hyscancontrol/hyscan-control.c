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
  HyScanDataSchema *schema = hyscan_sonar_get_schema (sonar);
  HyScanSonarType type = HYSCAN_SONAR_INVALID;

  if (sonar == NULL)
    return HYSCAN_SONAR_INVALID;

  /* Проверка ГБОЭ. */
  {
    gchar *echosounder_id_key;
    gchar *starboard_id_key;
    gchar *port_id_key;
    gchar *starboard_hi_id_key;
    gchar *port_hi_id_key;

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

    if (hyscan_data_schema_has_key (schema, echosounder_id_key) ||
        hyscan_data_schema_has_key (schema, starboard_id_key) ||
        hyscan_data_schema_has_key (schema, port_id_key) ||
        hyscan_data_schema_has_key (schema, starboard_hi_id_key) ||
        hyscan_data_schema_has_key (schema, port_hi_id_key))
      type = HYSCAN_SONAR_SSSE;

    g_free (echosounder_id_key);
    g_free (starboard_id_key);
    g_free (port_id_key);
    g_free (starboard_hi_id_key);
    g_free (port_hi_id_key);
  }

  g_clear_object (&schema);

  return type;
}
