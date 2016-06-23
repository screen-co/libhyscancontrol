/**
 * \file hyscan-generator-control.h
 *
 * \brief Заголовочный файл класса управления генераторами сигналов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanGeneratorControl HyScanGeneratorControl - класс управления генераторами сигналов
 *
 * Класс предназначен для управления генераторами зондирующих сигналов.
 *
 * Класс HyScanGeneratorControl наследуется от класса \link HyScanSensorControl \endlink и используется
 * как базовый для классов управления локаторами.
 *
 * Каждый гидролокатор имеет некоторое число генераторов, каждый из которых связанных с одним и более
 * приёмных каналов. Для идентификации генератора используется типа источника данных
 * \link HyScanSourceType \endlink, связанных с этим генератором. Число генераторов зависит от
 * типа гидролокатора.
 *
 * Каждый генератор имеет несколько способов установки излучаемого сигнала. Для определения
 * возможностей генератора используются функции #hyscan_generator_control_get_capabilities и
 * #hyscan_generator_control_get_signals. С помощью этих функций можно узнать какие сигналы
 * способен формировать генератор и какие режимы его работы допустимы. Методика задания характеристик
 * сигнала зависит от режима работы генератора.
 *
 * Возможны следующие режимы работы генераторов:
 *
 * - преднастройки;
 * - автоматический;
 * - упрощённый;
 * - расширенный.
 *
 * Выбор режима работы по преднастройкам осуществляется функцией #hyscan_generator_control_set_preset.
 * Список преднастроек можно получить с помощью функции #hyscan_generator_control_list_presets.
 *
 * При выборе автоматического режима работы, генератор автоматически установит характеристики сигнала.
 * Для включения этого режима предназначена функция #hyscan_generator_control_set_auto.
 *
 * В упрощённом режиме работы генератора необходимо выбрать тип сигнала и его энергию. Энергия сигнала
 * указывается в процентах, от 0 до 100 включительно. В этом случае не допускается автоматический выбор
 * сигнала, необходимо явным образом указать его тип. Включение упрощённого режима осуществляется
 * функцией #hyscan_generator_control_set_simple.
 *
 * В расширенном режиме работы генератора возможно установить тип сигнала и его характеристики
 * самостоятельно. Функции #hyscan_generator_control_set_tone и #hyscan_generator_control_set_lfm
 * используются для установки излучения тонального или ЛЧМ сигнала соотвенно. Допустимые пределы
 * излучаемых частот зависят от модели гидролокатора. Энергия сигнала указывается в процентах,
 * от 0 до 100 включительно. Максимально возможную длительность сигнала можно узнать с помощью функции
 * #hyscan_generator_control_get_max_duration.
 *
 * Любой из генераторов можно включить или выключить. Эта возможность полезна при проведении
 * технологических работ и экспериментов. Для этого предназначена функция
 * #hyscan_generator_control_set_enable. Если генератор выключен, излучение сигнала не производится.
 *
 */

#ifndef __HYSCAN_GENERATOR_CONTROL_H__
#define __HYSCAN_GENERATOR_CONTROL_H__

#include <hyscan-sensor-control.h>

G_BEGIN_DECLS

/** \brief Режимы работы гинератора  */
typedef enum
{
  HYSCAN_GENERATOR_MODE_INVALID                = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_GENERATOR_MODE_PRESET                 = (1 << 0),     /**< Режим работы по преднастройкам. */
  HYSCAN_GENERATOR_MODE_AUTO                   = (1 << 1),     /**< Автоматический режим работы. */
  HYSCAN_GENERATOR_MODE_SIMPLE                 = (1 << 2),     /**< Упрощённый режим работы. */
  HYSCAN_GENERATOR_MODE_EXTENDED               = (1 << 3)      /**< Режим работы по установленным параметрам. */
} HyScanGeneratorModeType;

/** \brief Типы сигналов */
typedef enum
{
  HYSCAN_GENERATOR_SIGNAL_INVALID              = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_GENERATOR_SIGNAL_AUTO                 = (1 << 0),     /**< Автоматический выбор типа сигнала. */
  HYSCAN_GENERATOR_SIGNAL_TONE                 = (1 << 1),     /**< Тональный сигнал. */
  HYSCAN_GENERATOR_SIGNAL_LFM                  = (1 << 2),     /**< Линейно-частотно модулированный сигнал. */
  HYSCAN_GENERATOR_SIGNAL_LFMD                 = (1 << 3)      /**< Линейно-частотно модулированный сигнал с уменьшением частоты. */
} HyScanGeneratorSignalType;

#define HYSCAN_TYPE_GENERATOR_CONTROL             (hyscan_generator_control_get_type ())
#define HYSCAN_GENERATOR_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GENERATOR_CONTROL, HyScanGeneratorControl))
#define HYSCAN_IS_GENERATOR_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GENERATOR_CONTROL))
#define HYSCAN_GENERATOR_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GENERATOR_CONTROL, HyScanGeneratorControlClass))
#define HYSCAN_IS_GENERATOR_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GENERATOR_CONTROL))
#define HYSCAN_GENERATOR_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GENERATOR_CONTROL, HyScanGeneratorControlClass))

typedef struct _HyScanGeneratorControl HyScanGeneratorControl;
typedef struct _HyScanGeneratorControlPrivate HyScanGeneratorControlPrivate;
typedef struct _HyScanGeneratorControlClass HyScanGeneratorControlClass;

struct _HyScanGeneratorControl
{
  HyScanSensorControl parent_instance;

  HyScanGeneratorControlPrivate *priv;
};

struct _HyScanGeneratorControlClass
{
  HyScanSensorControlClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                          hyscan_generator_control_get_type           (void);

/**
 *
 * Функция возвращает флаги допустимых режимов работы генератора.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink;
 * \param source тип источника данных (идентификатор генератора).
 *
 * \return Флаги допустимых режимов работы генератора.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanGeneratorModeType        hyscan_generator_control_get_capabilities   (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source);

/**
 *
 * Функция возвращает флаги допустимых сигналов генератора.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink;
 * \param source тип источника данных (идентификатор генератора).
 *
 * \return Флаги допустимых сигналов генератора.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanGeneratorSignalType      hyscan_generator_control_get_signals        (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source);

/**
 *
 * Функция возвращает список преднастроек генератора. Пользователь должен освободить
 * память, занимаемую списком, функцией \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора).
 *
 * \return Список преднастроек генератора.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue    **hyscan_generator_control_list_presets       (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source);

/**
 *
 * Функция включает преднастроенный режим работы генератора.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора);
 * \param preset_id идентификатор преднастройки.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_generator_control_set_preset         (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            gint64                     preset_id);

/**
 *
 * Функция включает автоматический режим работы генератора.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора);
 * \param signal тип сигнала.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_generator_control_set_auto           (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal);

/**
 *
 * Функция включает упрощённый режим работы генератора.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора);
 * \param signal тип сигнала;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_generator_control_set_simple         (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal,
                                                                            gdouble                    power);

/**
 *
 * Функция возвращает максимальную длительность сигнала, которую может
 * сформировать генератор.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink;
 * \param source тип источника данных (идентификатор генератора);
 * \param signal тип сигнала.
 *
 * \return Максимальная длительность сигнала, отрицательное число в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gdouble                        hyscan_generator_control_get_max_duration   (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal);

/**
 *
 * Функция включает тональный сигнал для излучения генератором.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора);
 * \param frequency частота излучаемого сигнала, Гц;
 * \param duration длительность сигнала, с;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_generator_control_set_tone           (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            gdouble                    frequency,
                                                                            gdouble                    duration,
                                                                            gdouble                    power);

/**
 *
 * Функция включает линейно-частотно модулированный сигнал для излучения генератором.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора);
 * \param decreasing увеличение (FALSE) или уменьшение (TRUE) частоты;
 * \param low_frequency нижняя частота излучаемого сигнала, Гц;
 * \param high_frequency верхняя частота излучаемого сигнала, Гц;
 * \param duration длительность сигнала, с,
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_generator_control_set_lfm            (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            gboolean                   decreasing,
                                                                            gdouble                    low_frequency,
                                                                            gdouble                    high_frequency,
                                                                            gdouble                    duration,
                                                                            gdouble                    power);

/**
 *
 * Функция включает или выключает формирование сигнала генератором.
 *
 * \param control указатель на интерфейс \link HyScanGeneratorControl \endlink.
 * \param source тип источника данных (идентификатор генератора);
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_generator_control_set_enable         (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            gboolean                   enable);

G_END_DECLS

#endif /* __HYSCAN_GENERATOR_CONTROL_H__ */
