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
 * Каждый гидролокатор имеет некоторое число источников данных, с каждым из которых связан генератор.
 * Для идентификации генераторов используются типы источников данных \link HyScanSourceType \endlink.
 * Состав источников данных зависит от типа гидролокатора.
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
 * В упрощённом режиме работы генератора необходимо выбрать тип сигнала и его энергию. Тип сигнала дожен
 * быть указан явным образом, выбор автоматического типа сигнала не допустим. Энергия сигнала указывается
 * в процентах, от 0 до 100 включительно. Включение упрощённого режима осуществляется функцией
 * #hyscan_generator_control_set_simple.
 *
 * В расширенном режиме работы генератора возможно установить тип сигнала, его энергию и длительность.
 * Энергия и тип сигнала задаются аналогично упрощённому режиму. Диапазоны длительности сигнала
 * можно узнать с помощью функции #hyscan_generator_control_get_duration_range. Включение упрощённого
 * режима осуществляется функцией #hyscan_generator_control_set_extended.
 *
 * Любой из генераторов можно включить или выключить. Эта возможность полезна при проведении
 * технологических работ и экспериментов. Для этого предназначена функция
 * #hyscan_generator_control_set_enable. Если генератор выключен, излучение сигнала не производится.
 *
 * При изменении излучаемого сигнала, класс посылает сигнал "signal-image", в котором передаёт
 * новый образ сигнала для свёртки. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    signal_image_cb    (HyScanGeneratorControl *control,
 *                             HyScanSourceType        source,
 *                             HyScanDataWriterSignal *signal,
 *                             gpointer                user_data);
 *
 * \endcode
 *
 * Где:
 *
 * - source - идентификатор источника данных;
 * - signal - образ сигнала для свёртки.
 *
 * Класс HyScanGeneratorControl поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_GENERATOR_CONTROL_H__
#define __HYSCAN_GENERATOR_CONTROL_H__

#include <hyscan-sensor-control.h>

G_BEGIN_DECLS

/** \brief Режимы работы гинератора */
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

HYSCAN_API
GType                          hyscan_generator_control_get_type           (void);

/**
 *
 * Функция возвращает флаги допустимых режимов работы генератора.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink;
 * \param source идентификатор источника данных.
 *
 * \return Флаги допустимых режимов работы генератора.
 *
 */
HYSCAN_API
HyScanGeneratorModeType        hyscan_generator_control_get_capabilities   (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source);

/**
 *
 * Функция возвращает флаги допустимых сигналов генератора.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink;
 * \param source идентификатор источника данных.
 *
 * \return Флаги допустимых сигналов генератора.
 *
 */
HYSCAN_API
HyScanGeneratorSignalType      hyscan_generator_control_get_signals        (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source);

/**
 *
 * Функция возвращает максимальную длительность сигнала, которую может
 * сформировать генератор.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param min_duration минимальная длительность сигнала, с;
 * \param max_duration максимальная длительность сигнала, с.
 *
 * \return TRUE - если функция выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_generator_control_get_duration_range (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal,
                                                                            gdouble                   *min_duration,
                                                                            gdouble                   *max_duration);

/**
 *
 * Функция возвращает список преднастроек генератора. Пользователь должен освободить
 * память, занимаемую списком, функцией \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink.
 * \param source идентификатор источника данных.
 *
 * \return Список преднастроек генератора.
 *
 */
HYSCAN_API
HyScanDataSchemaEnumValue    **hyscan_generator_control_list_presets       (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source);

/**
 *
 * Функция включает преднастроенный режим работы генератора.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink.
 * \param source идентификатор источника данных;
 * \param preset идентификатор преднастройки.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_generator_control_set_preset         (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            guint                      preset);

/**
 *
 * Функция включает автоматический режим работы генератора.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink.
 * \param source идентификатор источника данных;
 * \param signal тип сигнала.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_generator_control_set_auto           (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal);

/**
 *
 * Функция включает упрощённый режим работы генератора.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink.
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_generator_control_set_simple         (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal,
                                                                            gdouble                    power);

/**
 *
 * Функция включает расширенный режим работы генератора.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink.
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param duration длительность сигнала, с;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_generator_control_set_extended       (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            HyScanGeneratorSignalType  signal,
                                                                            gdouble                    duration,
                                                                            gdouble                    power);

/**
 *
 * Функция включает или выключает формирование сигнала генератором.
 *
 * \param control указатель на класс \link HyScanGeneratorControl \endlink.
 * \param source идентификатор источника данных;
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_generator_control_set_enable         (HyScanGeneratorControl    *control,
                                                                            HyScanSourceType           source,
                                                                            gboolean                   enable);

G_END_DECLS

#endif /* __HYSCAN_GENERATOR_CONTROL_H__ */
