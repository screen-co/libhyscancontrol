/**
 * \file hyscan-tvg-control.h
 *
 * \brief Заголовочный файл класса управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTVGControl HyScanTVGControl - класс управления системой ВАРУ
 *
 * Класс предназначен для управления системой ВАРУ приемных каналов.
 *
 * Класс HyScanTVGControl наследуется от класса \link HyScanGeneratorControl \endlink и используется
 * как базовый для классов управления локаторами.
 *
 * Гидролокатор имеет некоторое число источников данных, включая некоторое число приёмных каналов
 * "сырых" данных и (возможно) источники обработанных данных (акустические данные, батиметрические
 * данные и т.п.). Система ВАРУ оказывает прямое влияние только на приёмные каналы "сырых" данных.
 * Для идентификации системы ВАРУ используются типы источников данных \link HyScanSourceType \endlink.
 * Состав источников данных зависит от типа гидролокатора.
 *
 * Каждый источник данных имеет свои характеристики системы ВАРУ. Для определения возможностей системы
 * ВАРУ используется функция #hyscan_tvg_control_get_capabilities. С помощью этой функции можно узнать
 * какие режимы работы системы ВАРУ допустимы. Методика управления системой ВАРУ зависит от
 * режима её работы.
 *
 * Возможны следующие режимы работы системы ВАРУ:
 *
 * - автоматический;
 * - постоянный уровень усиления;
 * - линейное увеличение усиления в дБ на 100 метров;
 * - логарифмический вид закона усиления.
 *
 * При выборе автоматического режима работы, система ВАРУ будет самостоятельно управлять усилением
 * приёмных каналов, добиваясь раномерного уровня сигнала по всей дистанции приёма. Включение
 * автоматического режима осуществляется функцией #hyscan_tvg_control_set_auto. В качестве параметров
 * этой функции должны быть переданы целевой уровень сигнала и чувствительность.
 *
 * Постоянный уровень усиления можно установить с использованием функции #hyscan_tvg_control_set_constant.
 * Диапазон допустимых значений коэффициента усиления можно узнать с помощью функции
 * #hyscan_tvg_control_get_gain_range.
 *
 * Функция #hyscan_tvg_control_set_linear_db активирует режим ВАРУ, при котором коэффициент усиления,
 * в дБ, линейно увеличивается на указанную величину каждые 100 метров. Данный режим является наиболее
 * распространённым методом управления системой ВАРУ при боковом обзоре. В этом случае, величина
 * изменения усиления устанавливается в 20 дБ на 100 метров.
 *
 * Управление усилением по логарифмическому закону включается функцией #hyscan_tvg_control_set_logarithmic.
 * Данный режим в основном используется профессиональными пользователями или в исследовательских целях.
 * При этом используется следующая формула при расчёте коэффициента усиления:
 *
 * K = gain0 + beta * log (r) + alpha * r,
 *
 * где:
 *
 * - gain0 - начальный коэффициент усиления;
 * - beta - коэффициент отражения цели;
 * - aplha - коэффициент затухания;
 * - r - расстояние.
 *
 * Систему ВАРУ, любого из источников данных, можно включить или выключить. Эта возможность полезна
 * при проведении технологических работ и экспериментов. Для этого предназначена функция
 * #hyscan_tvg_control_set_enable. Если система ВАРУ выключена, коэффициент усиления сигнала не изменяется
 * во времени и устанавливается минимально возможным.
 *
 * При изменении параметров ВАРУ, класс посылает сигнал "gains", в котором передаёт новые
 * коэффициенты усиления. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    gains_cb    (HyScanTVGControl    *control,
 *                      HyScanSourceType     source,
 *                      HyScanDataWriterTVG *tvg,
 *                      gpointer             user_data);
 *
 * \endcode
 *
 * Где:
 *
 * - source - идентификатор источника данных;
 * - tvg - параметры системы ВАРУ.
 *
 * Класс HyScanTVGControl поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_TVG_CONTROL_H__
#define __HYSCAN_TVG_CONTROL_H__

#include <hyscan-generator-control.h>

G_BEGIN_DECLS

/** \brief Режимы работы системы ВАРУ */
typedef enum
{
  HYSCAN_TVG_MODE_INVALID                      = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_TVG_MODE_AUTO                         = (1 << 0),     /**< Автоматический режим работы. */
  HYSCAN_TVG_MODE_CONSTANT                     = (1 << 1),     /**< Постоянный уровень усиления. */
  HYSCAN_TVG_MODE_LINEAR_DB                    = (1 << 2),     /**< Линейное увеличение усиления в дБ / 100 метров. */
  HYSCAN_TVG_MODE_LOGARITHMIC                  = (1 << 3)      /**< Управление усилением по логарифмическому закону. */
} HyScanTVGModeType;

#define HYSCAN_TYPE_TVG_CONTROL             (hyscan_tvg_control_get_type ())
#define HYSCAN_TVG_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TVG_CONTROL, HyScanTVGControl))
#define HYSCAN_IS_TVG_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TVG_CONTROL))
#define HYSCAN_TVG_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TVG_CONTROL, HyScanTVGControlClass))
#define HYSCAN_IS_TVG_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TVG_CONTROL))
#define HYSCAN_TVG_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TVG_CONTROL, HyScanTVGControlClass))

typedef struct _HyScanTVGControl HyScanTVGControl;
typedef struct _HyScanTVGControlPrivate HyScanTVGControlPrivate;
typedef struct _HyScanTVGControlClass HyScanTVGControlClass;

struct _HyScanTVGControl
{
  HyScanGeneratorControl parent_instance;

  HyScanTVGControlPrivate *priv;
};

struct _HyScanTVGControlClass
{
  HyScanGeneratorControlClass parent_class;
};

HYSCAN_API
GType                  hyscan_tvg_control_get_type             (void);

/**
 *
 * Функция возвращает флаги допустимых режимов работы системы ВАРУ.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных.
 *
 * \return Флаги допустимых режимов работы системы ВАРУ.
 *
 */
HYSCAN_API
HyScanTVGModeType      hyscan_tvg_control_get_capabilities      (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source);

/**
 *
 * Функция возвращает допустимые пределы диапазона регулировки усиления ВАРУ.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных;
 * \param min_gain минимальный коэффициент усиления, дБ;
 * \param max_gain максимальный коэффициент усиления, дБ.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_tvg_control_get_gain_range        (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source,
                                                                 gdouble              *min_gain,
                                                                 gdouble              *max_gain);

/**
 *
 * Функция включает автоматический режим управления системой ВАРУ.
 *
 * Если в качестве значений параметров уровня сигнала и (или) чувствительности
 * передать отрицательное число, будут установлены значения по умолчанию.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных;
 * \param level целевой уровень сигнала;
 * \param sensitivity чувствительность автомата регулировки.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_tvg_control_set_auto              (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source,
                                                                 gdouble               level,
                                                                 gdouble               sensitivity);

/**
 *
 * Функция устанавливает постоянный уровень усиления системой ВАРУ.
 *
 * Ууровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных;
 * \param gain коэффициент усиления, дБ;
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_tvg_control_set_constant          (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source,
                                                                 gdouble               gain);

/**
 *
 * Функция устанавливает линейное увеличение усиления в дБ на 100 метров.
 *
 * Начальный уровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range. Величина изменения усиления должна
 * находится в пределах от 0 до 100.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param step величина изменения усиления каждые 100 метров, дБ.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_tvg_control_set_linear_db         (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source,
                                                                 gdouble               gain0,
                                                                 gdouble               step);

/**
 *
 * Функция устанавливает логарифмический вид закона усиления системой ВАРУ.
 *
 * Начальный уровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range. Значение коэффициента отражения цели
 * должно находится в пределах от 0 до 100. Значение коэффициента затухания
 * должно находится в пределах от 0 до 1.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param beta коэффициент отражения цели, дБ;
 * \param alpha коэффициент затухания, дБ/м.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_tvg_control_set_logarithmic       (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source,
                                                                 gdouble               gain0,
                                                                 gdouble               beta,
                                                                 gdouble               alpha);

/**
 *
 * Функция включает или выключает систему ВАРУ.
 *
 * \param control указатель на интерфейс \link HyScanTVGControl \endlink;
 * \param source идентификатор источника данных;
 * \param enable включёно или выключено.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_tvg_control_set_enable            (HyScanTVGControl     *control,
                                                                 HyScanSourceType      source,
                                                                 gboolean              enable);

G_END_DECLS

#endif /* __HYSCAN_TVG_CONTROL_H__ */
