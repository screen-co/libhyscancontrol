/*
 * \file hyscan-proxy-common.h
 *
 * \brief Заголовочный файл общих функций схем прокси гидролокаторов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_PROXY_COMMON_H__
#define __HYSCAN_PROXY_COMMON_H__

#include <hyscan-sonar-schema.h>
#include <hyscan-ssse-control.h>

#define HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME  "nmea"

/* Функция создаёт новый объект для определения схемы прокси гидролокатора. */
HyScanSonarSchema     *hyscan_proxy_schema_new                 (HyScanSonar                   *sonar,
                                                                gdouble                        timeout);

/* Функция определяет один виртуальный nmea порт прокси гидролокатора. */
gboolean               hyscan_proxy_schema_sensor_virtual      (HyScanSonarSchema             *schema);

/* Функция определяет акустические источники прокси гидролокатора. */
gboolean               hyscan_proxy_schema_ssse_acoustic       (HyScanSonarSchema             *schema,
                                                                HyScanSonar                   *sonar,
                                                                HyScanSSSEControl             *control);

#endif /* __HYSCAN_PROXY_COMMON_H__ */
