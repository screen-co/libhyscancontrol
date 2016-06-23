/*
 * \file hyscan-control-common.h
 *
 * \brief Заголовочный файл общих функций библиотеки управления гидролокаторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_CONTROL_COMMON_H__
#define __HYSCAN_CONTROL_COMMON_H__

#include <hyscan-core-types.h>

#define HYSCAN_SONAR_SCHEMA_ID         0x4D45484353524E53
#define HYSCAN_SONAR_SCHEMA_VERSION    20160100

/* Функция возвращает название источника данных по его идентификатору. */
const gchar           *hyscan_control_get_source_name          (HyScanSourceType               id);

/* Функция возвращает идентификатор источника по его названию. */
HyScanSourceType       hyscan_control_get_source_id            (const gchar                   *name);

#endif /* __HYSCAN_CONTROL_COMMON_H__ */
