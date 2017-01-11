/**
 * \file hyscan-sonar-server.h
 *
 * \brief Заголовочный файл сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarServer HyScanSonarServer - сервер управления гидролокатором
 *
 * Сервер управления предоставляет доступ к гидролокатору через протокол UDP библиотеки uRPC.
 * При создании сервера передаётся указатель на объект с интерфейсом \link HyScanParam \endlink,
 * в который сервер перенаправляет все запросы клиентов \link HyScanSonarClient \endlink.
 *
 * Создать сервер управления гидролокатором можно функцией #hyscan_sonar_server_new.
 *
 * Скорость отправки данных автоматически подстраивается под пропускную способность
 * линии связи. Целевая скорость задаётся функцией #hyscan_sonar_server_set_target_speed.
 * По умолчанию скорость настроена для работы по интерфейсу localhost.
 *
 * После создания сервера его необходимо запустить функцией #hyscan_sonar_server_start.
 *
 */

#ifndef __HYSCAN_SONAR_SERVER_H__
#define __HYSCAN_SONAR_SERVER_H__

#include <hyscan-param.h>

G_BEGIN_DECLS

/* \brief Целевая скорость передачи данных */
typedef enum
{
  HYSCAN_SONAR_SERVER_TARGET_SPEED_LOCAL,                /**< Интерфейс localhost. */
  HYSCAN_SONAR_SERVER_TARGET_SPEED_10M,                  /**< Интерфейс 10 Мбит/с. */
  HYSCAN_SONAR_SERVER_TARGET_SPEED_100M,                 /**< Интерфейс 100 Мбит/с. */
  HYSCAN_SONAR_SERVER_TARGET_SPEED_1G,                   /**< Интерфейс 1 Гбит/с. */
  HYSCAN_SONAR_SERVER_TARGET_SPEED_10G                   /**< Интерфейс 10 Гбит/с. */
} HyScanSonarServerTargetSpeed;

#define HYSCAN_SONAR_SERVER_MIN_TIMEOUT        5.0     /**< Минимальное время неактивности
                                                        *   клиента до отключения - 5.0 секунд. */
#define HYSCAN_SONAR_SERVER_MAX_TIMEOUT        600.0   /**< Максимальное время неактивности
                                                        *   клиента до отключения - 600 секунд. */
#define HYSCAN_SONAR_SERVER_DEFAULT_TIMEOUT    10.0    /**< Время неактивности клиента до отключения
                                                        *   по умолчанию - 10.0 секунд. */

#define HYSCAN_TYPE_SONAR_SERVER             (hyscan_sonar_server_get_type ())
#define HYSCAN_SONAR_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_SERVER, HyScanSonarServer))
#define HYSCAN_IS_SONAR_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_SERVER))
#define HYSCAN_SONAR_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_SERVER, HyScanSonarServerClass))
#define HYSCAN_IS_SONAR_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_SERVER))
#define HYSCAN_SONAR_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_SERVER, HyScanSonarServerClass))

typedef struct _HyScanSonarServer HyScanSonarServer;
typedef struct _HyScanSonarServerPrivate HyScanSonarServerPrivate;
typedef struct _HyScanSonarServerClass HyScanSonarServerClass;

struct _HyScanSonarServer
{
  GObject parent_instance;

  HyScanSonarServerPrivate *priv;
};

struct _HyScanSonarServerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType hyscan_sonar_server_get_type (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarServer \endlink.
 *
 * \param sonar объект в который транслируются запросы клиентов;
 * \param host IP адрес или DNS имя сервера;
 * \param port UDP порт сервера.
 *
 * \return Указатель на объект \link HyScanSonarServer \endlink.
 *
 */
HYSCAN_API
HyScanSonarServer     *hyscan_sonar_server_new                (HyScanParam                    *sonar,
                                                               const gchar                    *host,
                                                               guint16                         port);

/**
 *
 * Функция устанавливает целевую скорость передачи данных клиенту.
 *
 * \param server указатель на объект \link HyScanSonarServer \endlink;
 * \param speed целевая скорость \link HyScanSonarServerTargetSpeed \endlink.
 *
 * \return TRUE - если скорость установлена, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_server_set_target_speed    (HyScanSonarServer             *server,
                                                                HyScanSonarServerTargetSpeed   speed);

/**
 *
 * Функция запускает сервер управления гидролокатором в работу.
 *
 * Если в течение timeout времени клиент находится в состоянии неактивности,
 * он автоматически отключается от сервера. Минимальное значение - #HYSCAN_SONAR_SERVER_MIN_TIMEOUT,
 * максимальное - #HYSCAN_SONAR_SERVER_MAX_TIMEOUT, значение по умолчанию -
 * #HYSCAN_SONAR_SERVER_DEFAULT_TIMEOUT.
 *
 * \param server указатель на объект \link HyScanSonarServer \endlink;
 * \param timeout таймаут клиента, с.
 *
 * \return TRUE - если сервер успешно запущен, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_server_start               (HyScanSonarServer             *server,
                                                                gdouble                        timeout);

G_END_DECLS

#endif /* __HYSCAN_SONAR_SERVER_H__ */
