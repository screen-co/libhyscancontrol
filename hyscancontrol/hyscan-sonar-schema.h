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
 * - #hyscan_sonar_schema_sensor_add - функция добаляет описание порта;
 * - #hyscan_sonar_schema_sensor_add_uart_device - функция добавляет вариант значения поля устройства для порта типа UART;
 * - #hyscan_sonar_schema_sensor_add_uart_mode - функция добавляет вариант значения поля режима обмена данными с UART устройством;
 * - #hyscan_sonar_schema_sensor_add_ip_address - функция добавляет вариант значения поля IP адреса для порта типа IP.
 *
 * Функция определения параметров гидролокатора:
 *
 * - #hyscan_sonar_schema_sync_add - функция добавляет описание системы синхронизации излучения;
 * - #hyscan_sonar_schema_source_add - функция добавляет описание источника данных;
 * - #hyscan_sonar_schema_generator_add - функция добавляет описание генератора;
 * - #hyscan_sonar_schema_generator_add_preset - функция добавляет вариант значения преднастройки генератора;
 * - #hyscan_sonar_schema_tvg_add - функция добавляет в схему описание системы ВАРУ;
 * - #hyscan_sonar_schema_channel_add - функция добавляет в схему описание приёмного канала гидролокатора;
 * - #hyscan_sonar_schema_source_add_acuostic - функция добавляет в схему описание источника акустических данных;
 *
 */

#ifndef __HYSCAN_SONAR_SCHEMA_H__
#define __HYSCAN_SONAR_SCHEMA_H__

#include <hyscan-sonar-control.h>
#include <hyscan-data-schema-builder.h>

G_BEGIN_DECLS

#define HYSCAN_SONAR_SCHEMA_MIN_TIMEOUT      5.0       /**< Минимальное время ожидания команд от клиента - 5.0 секунд. */
#define HYSCAN_SONAR_SCHEMA_MAX_TIMEOUT      60.0      /**< Максимальное время ожидания команд от клиента - 60 секунд. */
#define HYSCAN_SONAR_SCHEMA_DEFAULT_TIMEOUT  10.0      /**< Время ожидания команд от клиента по умолчанию - 10 секунд. */

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

HYSCAN_API
GType                  hyscan_sonar_schema_get_type                    (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarSchema \endlink.
 *
 * \param timeout таймаут ожидания команд от клиента, с.
 *
 * \return Указатель на объект \link HyScanSonarSchema \endlink.
 *
 */
HYSCAN_API
HyScanSonarSchema     *hyscan_sonar_schema_new                         (gdouble                        timeout);

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
HYSCAN_API
gint                   hyscan_sonar_schema_sensor_add                  (HyScanSonarSchema             *schema,
                                                                        const gchar                   *name,
                                                                        HyScanSensorPortType           type,
                                                                        HyScanSensorProtocolType       protocol);

/**
 *
 * Функция добавляет в схему вариант значения поля устройства для порта типа UART.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param port_name название порта;
 * \param device_name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_sensor_add_uart_device      (HyScanSonarSchema             *schema,
                                                                        const gchar                   *port_name,
                                                                        const gchar                   *device_name);

/**
 *
 * Функция добавляет в схему вариант значения поля режима обмена данными с UART устройством.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param port_name название порта;
 * \param mode_name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_sensor_add_uart_mode        (HyScanSonarSchema             *schema,
                                                                        const gchar                   *port_name,
                                                                        const gchar                   *mode_name);

/**
 *
 * Функция добавляет в схему вариант значения поля IP адреса для порта типа IP.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param port_name название порта;
 * \param ip_name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_sensor_add_ip_address       (HyScanSonarSchema             *schema,
                                                                        const gchar                   *port_name,
                                                                        const gchar                   *ip_name);

/**
 *
 * Функция добавляет в схему описание системы синхронизации излучения.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param capabilities типы синхронизации излучения.
 *
 * \return TRUE - если функция успешно выполнена, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_schema_sync_add                    (HyScanSonarSchema             *schema,
                                                                        HyScanSonarSyncType            capabilities);

/**
 *
 * Функция добавляет в схему описание источника данных.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных;
 * \param antenna_vpattern диаграмма направленности антенны в вертикальной плоскости;
 * \param antenna_hpattern диаграмма направленности антенны в горизонтальной плоскости;
 * \param max_receive_time максимальное время приёма эхосигнала источником данных, с.
 *
 * \return Уникальный идентификатор источника данных в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_source_add                  (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        gdouble                        antenna_vpattern,
                                                                        gdouble                        antenna_hpattern,
                                                                        gdouble                        max_receive_time);

/**
 *
 * Функция добавляет в схему описание генератора гидролокатора.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных;
 * \param capabilities флаги возможных режимов работы генератора;
 * \param signals флаги возможных типов сигналов;
 * \param min_tone_duration минимальная длительность тонального сигнала;
 * \param max_tone_duration максимальная длительность тонального сигнала;
 * \param min_lfm_duration минимальная длительность ЛЧМ сигнала;
 * \param max_lfm_duration максимальная длительность ЛЧМ сигнала.
 *
 * \return Уникальный идентификатор генератора в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_generator_add               (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        HyScanGeneratorModeType        capabilities,
                                                                        HyScanGeneratorSignalType      signals,
                                                                        gdouble                        min_tone_duration,
                                                                        gdouble                        max_tone_duration,
                                                                        gdouble                        min_lfm_duration,
                                                                        gdouble                        max_lfm_duration);

/**
 *
 * Функция добавляет в схему вариант значения преднастройки генератора.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных;
 * \param name название варианта значения.
 *
 * \return Уникальный идентификатор варианта значения в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_generator_add_preset        (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        const gchar                   *name);

/**
 *
 * Функция добавляет в схему описание системы ВАРУ.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных;
 * \param capabilities флаги возможных режимов работы ВАРУ;
 * \param min_gain минимальное значение коэффициента усиления, дБ;
 * \param max_gain максимальное значение коэффициента усиления, дБ.
 *
 * \return Уникальный идентификатор ВАРУ в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_tvg_add                     (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        HyScanTVGModeType              capabilities,
                                                                        gdouble                        min_gain,
                                                                        gdouble                        max_gain);

/**
 *
 * Функция добавляет в схему описание приёмного канала.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных;
 * \param channel индекс канала данных;
 * \param antenna_voffset вертикальное смещение антенны в блоке, м;
 * \param antenna_hoffset горизнотальное смещение антенны в блоке, м;
 * \param adc_offset смещение 0 АЦП;
 * \param adc_vref опорное напряжение АЦП, В.
 *
 * \return Уникальный идентификатор приёмного канала в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_channel_add                 (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source,
                                                                        guint                          channel,
                                                                        gdouble                        antenna_voffset,
                                                                        gdouble                        antenna_hoffset,
                                                                        gint                           adc_offset,
                                                                        gfloat                         adc_vref);

/**
 *
 * Функция добавляет в схему описание источника "акустических" данных.
 *
 * \param schema указатель на объект \link HyScanSonarSchema \endlink;
 * \param source тип источника данных.
 *
 * \return Уникальный идентификатор источника "акустических" данных в схеме или отрицательное число в случае ошибки.
 *
 */
HYSCAN_API
gint                   hyscan_sonar_schema_source_add_acuostic         (HyScanSonarSchema             *schema,
                                                                        HyScanSourceType               source);

G_END_DECLS

#endif /* __HYSCAN_SONAR_SCHEMA_H__ */
