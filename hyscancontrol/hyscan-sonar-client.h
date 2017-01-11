/**
 * \file hyscan-sonar-client.h
 *
 * \brief Заголовочный файл клиента управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarClient HyScanSonarClient - клиент управления гидролокатором
 *
 * HyScanSonarClient предназначен для удалённого управления гидролокатором через протокол
 * UDP библиотеки uRPC. HyScanSonarClient взаимодействует с \link HyScanSonarServer \endlink.
 * HyScanSonarClient реализует интерфейс \link HyScanParam \endlink и может использоваться
 * классом \link HyScanSonarControl \endlink.
 *
 * Создать клиента управления гидролокатором можно функциями #hyscan_sonar_client_new или
 * #hyscan_sonar_client_new_full.
 *
 * По умолчанию таймаут ожидания ответа сервера составляет #HYSCAN_SONAR_CLIENT_DEFAULT_TIMEOUT
 * секунд. Если в течение этого времени ответа от сервера не было получено, производится
 * повторный запрос выполнения команды. По умочанию клиент осуществляет
 * #HYSCAN_SONAR_CLIENT_DEFAULT_EXEC  попыток выполнить команду.
 *
 * Данные, принимаемые от сервера предварительно кэшируются во внутреннем буфере. По умолчанию
 * размер внутреннего буфера составляет #HYSCAN_SONAR_CLIENT_DEFAULT_N_BUFFERS x 32Кб.
 *
 * Эти параметры можно изменить при подключении к гидролокатору функцией
 * #hyscan_sonar_client_new_full.
 *
 * Подключение к гидролокатору производится в пассивном режиме. В этом случае нет возможности
 * принимать данные от гидролокатора. Этот режим удобен для инспекции внутренего состояния
 * гидролокатора, без прерывания рабочей сессии.
 *
 * Перевести подключение в активный режим (для приёма данных) можно функцией
 * #hyscan_sonar_client_set_master. Перевести подключение в активный режим можно только
 * если нет других активных подключений к гидролокатору.
 *
 */

#ifndef __HYSCAN_SONAR_CLIENT_H__
#define __HYSCAN_SONAR_CLIENT_H__

#include <hyscan-param.h>

G_BEGIN_DECLS

#define HYSCAN_SONAR_CLIENT_MIN_TIMEOUT        1.0     /**< Минимальное время ожидания ответа
                                                        *   серврера - 1.0 секунда. */
#define HYSCAN_SONAR_CLIENT_MAX_TIMEOUT        5.0     /**< Максимальное время ожидания ответа
                                                        *   серврера - 5.0 секунд. */
#define HYSCAN_SONAR_CLIENT_DEFAULT_TIMEOUT    2.0     /**< Время ожидания ответа серврера по
                                                        *   умолчанию - 2.0 секунды. */

#define HYSCAN_SONAR_CLIENT_MIN_EXEC           1       /**< Минимальное число попыток выполнения
                                                        *   запросов - 1. */
#define HYSCAN_SONAR_CLIENT_MAX_EXEC           10      /**< Максимальное число попыток выполнения
                                                        *   запросов - 10. */
#define HYSCAN_SONAR_CLIENT_DEFAULT_EXEC       5       /**< Число попыток выполнения запросов по
                                                        *   умолчанию - 5. */

#define HYSCAN_SONAR_CLIENT_MIN_N_BUFFERS      32      /**< Минимальное число буферов для кэширования
                                                        *   данных - 32. */
#define HYSCAN_SONAR_CLIENT_MAX_N_BUFFERS      1024    /**< Максимальное число буферов для кэширования
                                                        *   данных - 1024. */
#define HYSCAN_SONAR_CLIENT_DEFAULT_N_BUFFERS  256     /**< Число буферов для кэширования данных по
                                                        *   умолчанию - 256. */

#define HYSCAN_TYPE_SONAR_CLIENT             (hyscan_sonar_client_get_type ())
#define HYSCAN_SONAR_CLIENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_CLIENT, HyScanSonarClient))
#define HYSCAN_IS_SONAR_CLIENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_CLIENT))
#define HYSCAN_SONAR_CLIENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_CLIENT, HyScanSonarClientClass))
#define HYSCAN_IS_SONAR_CLIENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_CLIENT))
#define HYSCAN_SONAR_CLIENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_CLIENT, HyScanSonarClientClass))

typedef struct _HyScanSonarClient HyScanSonarClient;
typedef struct _HyScanSonarClientPrivate HyScanSonarClientPrivate;
typedef struct _HyScanSonarClientClass HyScanSonarClientClass;

struct _HyScanSonarClient
{
  GObject parent_instance;

  HyScanSonarClientPrivate *priv;
};

struct _HyScanSonarClientClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_sonar_client_get_type    (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarClient \endlink и производит подключение
 * к удалённому гидролокатору.
 *
 * \param host IP адрес или DNS имя гидролокатора;
 * \param port UDP порт гидролокатора.
 *
 * \return Указатель на объект \link HyScanSonarClient \endlink.
 *
 */
HYSCAN_API
HyScanSonarClient     *hyscan_sonar_client_new         (const gchar           *host,
                                                        guint16                port);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarClient \endlink и производит подключение
 * к удалённому гидролокатору. Данная функция позволяет указать дополнительные параметры,
 * определяющие характеристики подключения к гидролокатору.
 *
 * \param host IP адрес или DNS имя гидролокатора;
 * \param port UDP порт гидролокатора;
 * \param timeout таймаут ожидания выполнения RPC запроса, секунды;
 * \param n_exec число попыток выполнения RPC запроса;
 * \param n_buffers число буферов для кэширования данных.
 *
 * \return Указатель на объект \link HyScanSonarClient \endlink.
 *
 */
HYSCAN_API
HyScanSonarClient     *hyscan_sonar_client_new_full    (const gchar           *host,
                                                        guint16                port,
                                                        gdouble                timeout,
                                                        guint                  n_exec,
                                                        guint                  n_buffers);

/**
 *
 * Функция переводит подключение к гидролокатору в активный режим.
 *
 * \param client указатель на объект \link HyScanSonarClient \endlink.
 *
 * \return TRUE - если подключение переведено в активный режим, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_client_set_master  (HyScanSonarClient     *client);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CLIENT_H__ */
