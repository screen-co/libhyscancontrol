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

#include <hyscan-sonar-schema.h>
#include <hyscan-core-types.h>

#define HYSCAN_SONAR_SCHEMA_ID                 0x4D45484353524E53
#define HYSCAN_SONAR_SCHEMA_VERSION            20160100

#define HYSCAN_SENSOR_CONTROL_MAX_CHANNELS     5

/* Функция возвращает название источника данных по его идентификатору. */
const gchar           *hyscan_control_get_source_name          (HyScanSourceType               source);

/* Функция возвращает идентификатор источника данных по его названию. */
HyScanSourceType       hyscan_control_get_source_type          (const gchar                   *name);

/* Функция аккумулирует boolean результаты работы callback'ов. */
gboolean               hyscan_control_boolean_accumulator      (GSignalInvocationHint         *ihint,
                                                                GValue                        *return_accu,
                                                                const GValue                  *handler_return,
                                                                gpointer                       data);

/* Функция ищет boolean параметр в списке и считывает его значение. */
gboolean               hyscan_control_find_boolean_param       (const gchar                   *name,
                                                                const gchar *const            *names,
                                                                GVariant                     **values,
                                                                gboolean                      *value);

/* Функция ищет integet параметр в списке и считывает его значение. */
gboolean               hyscan_control_find_integer_param       (const gchar                   *name,
                                                                const gchar *const            *names,
                                                                GVariant                     **values,
                                                                gint64                        *value);

/* Функция ищет double параметр в списке и считывает его значение. */
gboolean               hyscan_control_find_double_param        (const gchar                   *name,
                                                                const gchar *const            *names,
                                                                GVariant                     **values,
                                                                gdouble                       *value);

/* Функция ищет string параметр в списке и считывает его значение. */
const gchar           *hyscan_control_find_string_param        (const gchar                   *name,
                                                                const gchar *const            *names,
                                                                GVariant                     **values);

#endif /* __HYSCAN_CONTROL_COMMON_H__ */
