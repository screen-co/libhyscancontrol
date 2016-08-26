/**
 * \file hyscan-generator-control-server.h
 *
 * \brief Заголовочный файл класса сервера управления генераторами сигналов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanGeneratorControlServer HyScanGeneratorControlServer - класс сервера управления генераторами сигналов
 *
 * Класс предназначен для серверной реализации управления генераторами сигналов,
 * через интерфейс \link HyScanSonar \endlink.
 *
 * Класс обрабатывает запросы от \link HyScanGeneratorControl \endlink по управлению генераторами сигналов.
 * При получении такого запроса происходит предварительная проверка валидности данных по схеме
 * гидролокатора и по составу параметров запроса. Затем класс посылает сигнал с параметрами запроса.
 * Пользователь должен установить свои обработчики сигналов, чтобы реагировать на эти запросы.
 * Класс посылает следующие сигналы:
 *
 * - "generator-set-preset" - при выборе режима работы генератора по преднастройкам;
 * - "generator-set-auto" - при выборе автоматического режима работы генератора;
 * - "generator-set-simple" - при выборе упрощённого режима работы генератора;
 * - "generator-set-extended" - при выборе расширенного режима работы генератора;
 * - "generator-set-enable" - при включении или отключении генератора.
 *
 * Прототипы обработчиков сигналов:
 *
 * \code
 *
 * gboolean generator_set_preset_cb     (HyScanGeneratorControlServer  *server,
 *                                       HyScanSourceType               source,
 *                                       guint                          preset,
 *                                       gpointer                       user_data);
 *
 * gboolean generator_set_auto_cb       (HyScanGeneratorControlServer  *server,
 *                                       HyScanSourceType               source,
 *                                       HyScanGeneratorSignalType      signal,
 *                                       gpointer                       user_data);
 *
 * gboolean generator_set_simple_cb     (HyScanGeneratorControlServer  *server,
 *                                       HyScanSourceType               source,
 *                                       HyScanGeneratorSignalType      signal,
 *                                       gdouble                        power,
 *                                       gpointer                       user_data);
 *
 * gboolean generator_set_extended_cb   (HyScanGeneratorControlServer  *server,
 *                                       HyScanSourceType               source,
 *                                       HyScanGeneratorSignalType      signal,
 *                                       gdouble                        duration,
 *                                       gdouble                        power,
 *                                       gpointer                       user_data);
 *
 * gboolean generator_set_enable_cb     (HyScanGeneratorControlServer  *server,
 *                                       HyScanSourceType               source,
 *                                       gboolean                       enable,
 *                                       gpointer                       user_data);
 *
 * \endcode
 *
 * Описание параметров сигналов аналогично параметрам функций \link hyscan_generator_control_set_preset \endlink,
 * \link hyscan_generator_control_set_auto \endlink, \link hyscan_generator_control_set_simple \endlink,
 * \link hyscan_generator_control_set_extended \endlink, \link hyscan_generator_control_set_enable \endlink,
 * класса \link HyScanGeneratorControl \endlink.
 *
 * Обработчик сигнала должен вернуть значение TRUE - если команда успешно выполнена,
 * FALSE - в случае ошибки.
 *
 * Функция #hyscan_generator_control_server_send_signal предназаначена для отправки образа
 * излучаемого сигнала.
 *
 */

#ifndef __HYSCAN_GENERATOR_CONTROL_SERVER_H__
#define __HYSCAN_GENERATOR_CONTROL_SERVER_H__

#include <hyscan-sensor-control-server.h>
#include <hyscan-core-types.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GENERATOR_CONTROL_SERVER             (hyscan_generator_control_server_get_type ())
#define HYSCAN_GENERATOR_CONTROL_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, HyScanGeneratorControlServer))
#define HYSCAN_IS_GENERATOR_CONTROL_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GENERATOR_CONTROL_SERVER))
#define HYSCAN_GENERATOR_CONTROL_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, HyScanGeneratorControlServerClass))
#define HYSCAN_IS_GENERATOR_CONTROL_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GENERATOR_CONTROL_SERVER))
#define HYSCAN_GENERATOR_CONTROL_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, HyScanGeneratorControlServerClass))

typedef struct _HyScanGeneratorControlServer HyScanGeneratorControlServer;
typedef struct _HyScanGeneratorControlServerPrivate HyScanGeneratorControlServerPrivate;
typedef struct _HyScanGeneratorControlServerClass HyScanGeneratorControlServerClass;

struct _HyScanGeneratorControlServer
{
  HyScanSensorControlServer parent_instance;

  HyScanGeneratorControlServerPrivate *priv;
};

struct _HyScanGeneratorControlServerClass
{
  HyScanSensorControlServerClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_generator_control_server_get_type        (void);

/**
 *
 * Функция передаёт образы излучаемых сигналов, через отправку сигнала "data" интерфейса \link HyScanSonar \endlink.
 *
 * \param server указатель на интерфейс \link HyScanGeneratorControlServer \endlink;
 * \param source идентификатор источника данных;
 * \param signal образ сигнала.
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void                   hyscan_generator_control_server_send_signal     (HyScanGeneratorControlServer  *server,
                                                                        HyScanSourceType               source,
                                                                        HyScanDataWriterSignal        *signal);

G_END_DECLS

#endif /* __HYSCAN_GENERATOR_CONTROL_SERVER_H__ */
