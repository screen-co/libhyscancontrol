/**
 * \file hyscan-sonar-control-server.h
 *
 * \brief Заголовочный файл класса сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarControlServer HyScanSonarControlServer - класс сервера управления гидролокатором
 *
 * Класс предназначен для серверной реализации управления гидролокатором,
 * через интерфейс \link HyScanSonar \endlink.
 *
 * Класс обрабатывает запросы от \link HyScanSonarControl \endlink по управлению гидролокатором.
 * При получении такого запроса происходит предварительная проверка валидности данных по схеме
 * гидролокатора и по составу параметров запроса. Затем класс посылает сигнал с параметрами запроса.
 * Пользователь должен установить свои обработчики сигналов, чтобы реагировать на эти запросы.
 * Класс посылает следующие сигналы:
 *
 * - "sonar-set-sync-type" - при изменении типа синхронизации излучения;
 * - "sonar-enable-raw-data" - при изменении выдачи "сырых" данных гидролокатором;
 * - "sonar-set-receive-time" - при установке времни приёма эхосигнала гидролокатором;
 * - "sonar-start" - при включении гидролокатора в рабочий режим;
 * - "sonar-stop" - при выключении рабочего режима гидролокатора;
 * - "sonar-ping" - при программном запуске цикла зондирования.
 *
 * Прототипы обработчиков сигналов:
 *
 * \code
 *
 * gboolean sonar_set_sync_type_cb    (HyScanSonarControlServer   *server,
 *                                     gint64                      sync_type,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_enable_raw_data_cb  (HyScanSonarControlServer   *server,
 *                                     gboolean                    enable,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_set_receive_time_cb (HyScanSonarControlServer   *server,
 *                                     gint                        board,
 *                                     gdouble                     receive_time,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_start_cb            (HyScanSonarControlServer   *server,
 *                                     const gchar                *project_name,
 *                                     const gchar                *track_name,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_stop_cb             (HyScanSonarControlServer   *server.
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_ping_cb             (HyScanSonarControlServer   *server,
 *                                     gpointer                    user_data);
 *
 * \endcode
 *
 * Описание параметров сигналов аналогично параметрам функций \link hyscan_sonar_control_set_sync_type \endlink,
 * \link hyscan_sonar_control_enable_raw_data \endlink, \link hyscan_sonar_control_set_receive_time \endlink,
 * \link hyscan_sonar_control_start \endlink, \link hyscan_sonar_control_stop \endlink,
 * \link hyscan_sonar_control_ping \endlink, класса \link HyScanSonarControl \endlink.
 *
 * Обработчик сигнала должен вернуть значение TRUE - если команда успешно выполнена,
 * FALSE - в случае ошибки.
 *
 * Функция #hyscan_sonar_control_server_send_raw_data предназаначена для отправки "сырых"
 * данных от гидролокатора.
 *
 */

#ifndef __HYSCAN_SONAR_CONTROL_SERVER_H__
#define __HYSCAN_SONAR_CONTROL_SERVER_H__

#include <hyscan-tvg-control-server.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_CONTROL_SERVER             (hyscan_sonar_control_server_get_type ())
#define HYSCAN_SONAR_CONTROL_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_CONTROL_SERVER, HyScanSonarControlServer))
#define HYSCAN_IS_SONAR_CONTROL_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_CONTROL_SERVER))
#define HYSCAN_SONAR_CONTROL_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_CONTROL_SERVER, HyScanSonarControlServerClass))
#define HYSCAN_IS_SONAR_CONTROL_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_CONTROL_SERVER))
#define HYSCAN_SONAR_CONTROL_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_CONTROL_SERVER, HyScanSonarControlServerClass))

typedef struct _HyScanSonarControlServer HyScanSonarControlServer;
typedef struct _HyScanSonarControlServerPrivate HyScanSonarControlServerPrivate;
typedef struct _HyScanSonarControlServerClass HyScanSonarControlServerClass;

struct _HyScanSonarControlServer
{
  HyScanTVGControlServer parent_instance;

  HyScanSonarControlServerPrivate *priv;
};

struct _HyScanSonarControlServerClass
{
  HyScanTVGControlServerClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_sonar_control_server_get_type            (void);

/**
 *
 * Функция передаёт "сырые" данные от гидролокатора, через отправку сигнала
 * "data" интерфейса \link HyScanSonar \endlink.
 *
 * \param server указатель на интерфейс \link HyScanTVGControlServer \endlink;
 * \param time время начала действия ВАРУ, мкс;
 * \param board идентификатор борта;
 * \param channel номер приёмного канала;
 * \param size размер данных, байт;
 * \param data данные.
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void                   hyscan_sonar_control_server_send_raw_data       (HyScanSonarControlServer    *server,
                                                                        gint64                       time,
                                                                        HyScanBoardType              board,
                                                                        gint                         channel,
                                                                        guint32                      size,
                                                                        gpointer                     data);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CONTROL_SERVER_H__ */
