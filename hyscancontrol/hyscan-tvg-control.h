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
 *
 *
 *
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
  HYSCAN_TVG_MODE_MANUAL                       = (1 << 1),     /**< Ручное управление усилением. */
  HYSCAN_TVG_MODE_CONSTANT                     = (1 << 2),     /**< Постоянный уровень усиления. */
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

HYSCAN_CONTROL_EXPORT
GType                  hyscan_tvg_control_get_type             (void);

HYSCAN_CONTROL_EXPORT
HyScanTVGModeType      hyscan_tvg_control_get_capabilities      (HyScanTVGControl     *control,
                                                                 HyScanBoardType       board);

HYSCAN_CONTROL_EXPORT
gboolean               hyscan_tvg_control_get_range             (HyScanTVGControl     *control,
                                                                 HyScanBoardType       board,
                                                                 gdouble               min_gain,
                                                                 gdouble               max_gain);

HYSCAN_CONTROL_EXPORT
gboolean               hyscan_tvg_control_set_auto              (HyScanTVGControl     *control,
                                                                 HyScanBoardType       board,
                                                                 gdouble               level,
                                                                 gdouble               sensitivity);

HYSCAN_CONTROL_EXPORT
gboolean               hyscan_tvg_control_set_constant          (HyScanTVGControl     *control,
                                                                 HyScanBoardType       board,
                                                                 gint                  channel,
                                                                 gdouble               gain);

HYSCAN_CONTROL_EXPORT
gboolean               hyscan_tvg_control_set_logarithmic       (HyScanTVGControl     *control,
                                                                 HyScanBoardType       board,
                                                                 gint                  channel,
                                                                 gdouble               alpha,
                                                                 gdouble               betta);

G_END_DECLS

#endif /* __HYSCAN_TVG_CONTROL_H__ */
