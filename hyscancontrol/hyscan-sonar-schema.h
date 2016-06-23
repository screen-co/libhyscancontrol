/**
 * \file hyscan-sonar-schema.h
 *
 * \brief Заголовочный файл класса генерации схемы гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarSchema HyScanSonarSchema - класс генерации схемы гидролокатора
 *
 * Класс предназначен для автоматизации процесса создания схемы данных гидролокатора. Класс
 * является наследуемым от \link HyScanDataSchemaBuilder \endlink. Создание объекта
 * производится функцией #hyscan_sonar_schema_new.
 *
 * Класс содержит следующие группы функций.
 *
 * Функции определения портов для подключения датчиков:
 *
 * - #hyscan_sonar_schema_sensor_add - функция добаляет определение порта;
 * - #hyscan_sonar_schema_sensor_add_uart_device - функция добавляет определение UART устройства;
 * - #hyscan_sonar_schema_sensor_add_uart_mode - функция добавляет определение режима работы UART устройства;
 * - #hyscan_sonar_schema_sensor_add_ip_address - функция добавляет определение IP адреса для IP порта.
 *
 * Функции определения генераторов гидролокатора:
 *
 * - #hyscan_sonar_schema_generator_add - функция добавляет определение генератора;
 * - #hyscan_sonar_schema_generator_add_preset - функция добавляет определение преднастройки генератора.
 *
 */

#ifndef __HYSCAN_SONAR_SCHEMA_H__
#define __HYSCAN_SONAR_SCHEMA_H__

#include <hyscan-core-types.h>
#include <hyscan-sensor-control.h>
#include <hyscan-generator-control.h>
#include <hyscan-data-schema-builder.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_SCHEMA             (hyscan_sonar_schema_get_type ())
#define HYSCAN_SONAR_SCHEMA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_SCHEMA, HyScanSonarSchema))
#define HYSCAN_IS_SONAR_SCHEMA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_SCHEMA))
#define HYSCAN_SONAR_SCHEMA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_SCHEMA, HyScanSonarSchemaClass))
#define HYSCAN_IS_SONAR_SCHEMA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_SCHEMA))
#define HYSCAN_SONAR_SCHEMA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_SCHEMA, HyScanSonarSchemaClass))

typedef struct _HyScanSonarSchema HyScanSonarSchema;
typedef struct _HyScanSonarSchemaPrivate HyScanSonarSchemaPrivate;
typedef struct _HyScanSonarSchemaClass HyScanSonarSchemaClass;

struct _HyScanSonarSchema
{
  HyScanDataSchemaBuilder parent_instance;

  HyScanSonarSchemaPrivate *priv;
};

struct _HyScanSonarSchemaClass
{
  HyScanDataSchemaBuilderClass parent_class;
};

GType                  hyscan_sonar_schema_get_type                    (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarSchema \endlink.
 *
 * \return Указатель на объект \link HyScanSonarSchema \endlink.
 *
 */
HyScanSonarSchema     *hyscan_sonar_schema_new                         (void);

/**
 *
 * Функция добавляет в схему описание порта для подключения датчика.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param name название порта;
 * \param type тип порта;
 * \param protocol протокол обмена данными с датчиком.
 *
 * \return Уникальный идентификатор порта в схеме или отрицательное число в случае ошибки.
 *
 */
gint                   hyscan_sonar_schema_sensor_add                  (HyScanSonarSchema             *schema,
                                                                        const gchar                   *name,
                                                                        HyScanSensorPortType           type,
                                                                        HyScanSensorProtocolType       protocol);

/**
 *
 * Функция добавляет в схему вариант значения поля устройства для порта типа UART.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
gint                   hyscan_sonar_schema_sensor_add_uart_device      (HyScanSonarSchema             *schema,
                                                                        const gchar                   *name);

/**
 *
 * Функция добавляет в схему вариант значения поля режима обмена данными с UART устройством.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
gint                   hyscan_sonar_schema_sensor_add_uart_mode        (HyScanSonarSchema             *schema,
                                                                        const gchar                   *name);

/**
 *
 * Функция добавляет в схему вариант значения поля IP адреса для порта типа IP.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
gint                   hyscan_sonar_schema_sensor_add_ip_address       (HyScanSonarSchema             *schema,
                                                                        const gchar                   *name);

/**
 *
 * Функция добавляет в схему описание генератора гидролокатора.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных (идентификатор генератора);
 * \param capabilities флаги возможных режимов работы генератора;
 * \param signals флаги возможных типов сигналов;
 * \param low_frequency нижняя частота полосы излучаемого сигнала;
 * \param high_frequency верхняя частота полосы излучаемого сигнала;
 * \param max_duration максимальная длительность излучаемого сигнала.
 *
 * \return Уникальный идентификатор генератора в схеме или отрицательное число в случае ошибки.
 *
 */
gint                   hyscan_sonar_schema_generator_add               (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        HyScanGeneratorModeType        capabilities,
                                                                        HyScanGeneratorSignalType      signals,
                                                                        gdouble                        low_frequency,
                                                                        gdouble                        high_frequency,
                                                                        gdouble                        max_duration);

/**
 *
 * Функция добавляет в схему вариант значения преднастройки генератора.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных (идентификатор генератора);
 * \param name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
gint                   hyscan_sonar_schema_generator_add_preset        (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        const gchar                   *name);

G_END_DECLS

#endif /* __HYSCAN_SONAR_SCHEMA_H__ */
