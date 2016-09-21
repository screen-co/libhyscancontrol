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
 * через интерфейс \link HyScanSonar \endlink. Создание класса осуществляется функцией
 * #hyscan_sonar_control_server_new.
 *
 * Класс обрабатывает запросы от \link HyScanSonarControl \endlink по управлению гидролокатором.
 * При получении такого запроса происходит предварительная проверка валидности данных по схеме
 * гидролокатора и по составу параметров запроса. Затем класс посылает сигнал с параметрами запроса.
 * Пользователь должен установить свои обработчики сигналов, чтобы реагировать на эти запросы.
 * Класс посылает следующие сигналы:
 *
 * - "sonar-set-sync-type" - при изменении типа синхронизации излучения;
 * - "sonar-enable-raw-data" - при изменении выдачи "сырых" данных гидролокатором;
 * - "sonar-set-position" - при установке информации о местоположении приёмных антенн;
 * - "sonar-set-receive-time" - при установке времни приёма эхосигнала гидролокатором;
 * - "sonar-start" - при включении гидролокатора в рабочий режим;
 * - "sonar-stop" - при выключении рабочего режима гидролокатора;
 * - "sonar-ping" - при программном запуске цикла зондирования;
 * - "sonar-alive-timeout" - при превышении времени ожидания команды alive от клиента.
 *
 * Прототипы обработчиков сигналов:
 *
 * \code
 *
 * gboolean sonar_set_sync_type_cb    (HyScanSonarControlServer   *server,
 *                                     HyScanSonarSyncType         sync_type,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_enable_raw_data_cb  (HyScanSonarControlServer   *server,
 *                                     gboolean                    enable,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_set_position_cb     (HyScanSonarControlServer   *server,
 *                                     HyScanSourceType            source,
 *                                     HyScanAntennaPosition       position,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_set_receive_time_cb (HyScanSonarControlServer   *server,
 *                                     HyScanSourceType            source,
 *                                     gdouble                     receive_time,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_start_cb            (HyScanSonarControlServer   *server,
 *                                     const gchar                *project_name,
 *                                     const gchar                *track_name,
 *                                     HyScanTrackType             track_type,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_stop_cb             (HyScanSonarControlServer   *server,
 *                                     gpointer                    user_data);
 *
 * gboolean sonar_ping_cb             (HyScanSonarControlServer   *server,
 *                                     gpointer                    user_data);
 *
 * void sonar_alive_timeout_cb        (HyScanSonarControlServer   *server,
 *                                     gpointer                    user_data);
 *
 * \endcode
 *
 * Описание параметров сигналов аналогично параметрам функций \link hyscan_sonar_control_set_sync_type \endlink,
 * \link hyscan_sonar_control_enable_raw_data \endlink, \link hyscan_sonar_control_set_position \endlink,
 * \link hyscan_sonar_control_set_receive_time \endlink, \link hyscan_sonar_control_start \endlink,
 * \link hyscan_sonar_control_stop \endlink и \link hyscan_sonar_control_ping \endlink,
 * класса \link HyScanSonarControl \endlink.
 *
 * Обработчик сигнала должен вернуть значение TRUE - если команда успешно выполнена,
 * FALSE - в случае ошибки.
 *
 * Функция #hyscan_sonar_control_server_send_raw_data предназаначена для отправки "сырых"
 * данных от гидролокатора.
 *
 * Функция #hyscan_sonar_control_server_send_noise_data предназаначена для отправки "сырых"
 * данных от гидролокатора принятых при отключенном излучении.
 *
 */

#ifndef __HYSCAN_SONAR_CONTROL_SERVER_H__
#define __HYSCAN_SONAR_CONTROL_SERVER_H__

#include <hyscan-data-writer.h>
#include <hyscan-sonar-box.h>

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
  GObject parent_instance;

  HyScanSonarControlServerPrivate *priv;
};

struct _HyScanSonarControlServerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_sonar_control_server_get_type            (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarControlServer \endlink.
 * Функция не создаёт дополнительной ссылки на бъект с параметрами гидролокатора,
 * этот объект должен существовать всё время работы сервера.
 *
 * \param params указатель на параметры гидролокатора \link HyScanSonarBox \endlink.
 *
 * \return Указатель на объект \link HyScanSonarControlServer \endlink.
 *
 */
HYSCAN_API
HyScanSonarControlServer      *hyscan_sonar_control_server_new                 (HyScanSonarBox              *params);

/**
 *
 * Функция передаёт "сырые" данные от гидролокатора, через отправку сигнала
 * "data" интерфейса \link HyScanSonar \endlink.
 *
 * \param server указатель на интерфейс \link HyScanTVGControlServer \endlink;
 * \param source идентификатор источника данных;
 * \param channel номер приёмного канала;
 * \param type тип данных, \link HyScanDataType \endlink;
 * \param rate частота дискретизации данных, Гц;
 * \param data данные от гидролокатора.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_sonar_control_server_send_raw_data       (HyScanSonarControlServer    *server,
                                                                                HyScanSourceType             source,
                                                                                gint                         channel,
                                                                                HyScanDataType               type,
                                                                                gdouble                      rate,
                                                                                HyScanDataWriterData        *data);

/**
 *
 * Функция передаёт "сырые" данные от гидролокатора принятые при отключенном излучении - шумов.
 *
 * \param server указатель на интерфейс \link HyScanTVGControlServer \endlink;
 * \param source идентификатор источника данных;
 * \param channel номер приёмного канала;
 * \param type тип данных, \link HyScanDataType \endlink;
 * \param rate частота дискретизации данных, Гц;
 * \param data данные от гидролокатора.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_sonar_control_server_send_noise_data     (HyScanSonarControlServer    *server,
                                                                                HyScanSourceType             source,
                                                                                gint                         channel,
                                                                                HyScanDataType               type,
                                                                                gdouble                      rate,
                                                                                HyScanDataWriterData        *data);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CONTROL_SERVER_H__ */
