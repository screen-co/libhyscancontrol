/**
 * \file hyscan-tvg-control-server.h
 *
 * \brief Заголовочный файл класса сервера управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTVGControlServer HyScanTVGControlServer - класс сервера управления системой ВАРУ
 *
 * Класс предназначен для серверной реализации управления системой ВАРУ,
 * через интерфейс \link HyScanSonar \endlink.
 *
 * Класс обрабатывает запросы от \link HyScanTVGControl \endlink по управлению системой ВАРУ.
 * При получении такого запроса происходит предварительная проверка валидности данных по схеме
 * гидролокатора и по составу параметров запроса. Затем класс посылает сигнал с параметрами запроса.
 * Пользователь должен установить свои обработчики сигналов, чтобы реагировать на эти запросы.
 * Класс посылает следующие сигналы:
 *
 * - "tvg-set-auto" - при выборе автоматического режима работы ВАРУ;
 * - "tvg-set-constant" - при выборе постоянного уровня усиления ВАРУ;
 * - "tvg-set-linear-db" - при выборе линейного увеличение усиления ВАРУ;
 * - "tvg-set-logarithmic" - при выборе логарифмического вида закона усиления ВАРУ;
 * - "tvg-set-enable" - при включении или отключении ВАРУ.
 *
 * Прототипы обработчиков сигналов:
 *
 * \code
 *
 * gboolean tvg_set_auto_cb          (HyScanTVGControlServer    *server,
 *                                    HyScanSourceType           source,
 *                                    gdouble                    level,
 *                                    gdouble                    sensitivity,
 *                                    gpointer                   user_data);
 *
 * gboolean tvg_set_constant_cb      (HyScanTVGControlServer    *server,
 *                                    HyScanSourceType           source,
 *                                    gdouble                    gain,
 *                                    gpointer                   user_data);
 *
 * gboolean tvg_set_linear_db_cb     (HyScanTVGControlServer    *server,
 *                                    HyScanSourceType           source,
 *                                    gdouble                    gain0,
 *                                    gdouble                    step,
 *                                    gpointer                   user_data);
 *
 * gboolean tvg_set_logarithmic_cb   (HyScanTVGControlServer    *server,
 *                                    HyScanSourceType           source,
 *                                    gdouble                    gain0,
 *                                    gdouble                    beta,
 *                                    gdouble                    alpha,
 *                                    gpointer                   user_data);
 *
 * gboolean tvg_set_enable_cb        (HyScanTVGControlServer    *server,
 *                                    HyScanSourceType           source,
 *                                    gboolean                   enable,
 *                                    gpointer                   user_data);
 *
 * \endcode
 *
 * Описание параметров сигналов аналогично параметрам функций \link hyscan_tvg_control_set_auto \endlink,
 * \link hyscan_tvg_control_set_constant \endlink, \link hyscan_tvg_control_set_linear_db \endlink,
 * \link hyscan_tvg_control_set_logarithmic \endlink, класса \link HyScanTVGControl \endlink.
 *
 * Обработчик сигнала должен вернуть значение TRUE - если команда успешно выполнена,
 * FALSE - в случае ошибки.
 *
 * Функция #hyscan_tvg_control_server_send_tvg предназаначена для отправки параметров
 * системы ВАРУ.
 *
 */

#ifndef __HYSCAN_TVG_CONTROL_SERVER_H__
#define __HYSCAN_TVG_CONTROL_SERVER_H__

#include <hyscan-generator-control-server.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TVG_CONTROL_SERVER             (hyscan_tvg_control_server_get_type ())
#define HYSCAN_TVG_CONTROL_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TVG_CONTROL_SERVER, HyScanTVGControlServer))
#define HYSCAN_IS_TVG_CONTROL_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TVG_CONTROL_SERVER))
#define HYSCAN_TVG_CONTROL_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TVG_CONTROL_SERVER, HyScanTVGControlServerClass))
#define HYSCAN_IS_TVG_CONTROL_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TVG_CONTROL_SERVER))
#define HYSCAN_TVG_CONTROL_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TVG_CONTROL_SERVER, HyScanTVGControlServerClass))

typedef struct _HyScanTVGControlServer HyScanTVGControlServer;
typedef struct _HyScanTVGControlServerPrivate HyScanTVGControlServerPrivate;
typedef struct _HyScanTVGControlServerClass HyScanTVGControlServerClass;

struct _HyScanTVGControlServer
{
  HyScanGeneratorControlServer parent_instance;

  HyScanTVGControlServerPrivate *priv;
};

struct _HyScanTVGControlServerClass
{
  HyScanGeneratorControlServerClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_tvg_control_server_get_type              (void);

/**
 *
 * Функция передаёт параметры ВАРУ, через отправку сигнала "data" интерфейса \link HyScanSonar \endlink.
 *
 * \param server указатель на интерфейс \link HyScanTVGControlServer \endlink;
 * \param source идентификатор источника данных;
 * \param tvg параметры ВАРУ.
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void                   hyscan_tvg_control_server_send_tvg              (HyScanTVGControlServer    *server,
                                                                        HyScanSourceType           source,
                                                                        HyScanDataWriterTVG       *tvg);

G_END_DECLS

#endif /* __HYSCAN_TVG_CONTROL_SERVER_H__ */
