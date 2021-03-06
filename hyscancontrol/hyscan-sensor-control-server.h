/**
 * \file hyscan-sensor-control-server.h
 *
 * \brief Заголовочный файл класса сервера управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSensorControlServer HyScanSensorControlServer - класс сервера управления датчиками местоположения и ориентации
 *
 * Класс предназначен для серверной реализации управления датчиками местоположения и ориентации,
 * через интерфейс \link HyScanParam \endlink. Создание класса осуществляется функцией
 * #hyscan_sensor_control_server_new.
 *
 * Класс обрабатывает запросы от \link HyScanSensorControl \endlink по управлению портами,
 * для подключения датчиков. При получении такого запроса происходит предварительная проверка
 * валидности данных по схеме гидролокатора и по составу параметров запроса. Затем класс
 * посылает сигнал с параметрами запроса. Пользователь должен установить свои обработчики
 * сигналов, чтобы реагировать на эти запросы. Класс посылает следующие сигналы:
 *
 * - "sensor-virtual-port-param" - при изменении параметров работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL;
 * - "sensor-uart-port-param" - при изменении параметров работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART;
 * - "sensor-udp-ip-port-param" - при изменении параметров работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP;
 * - "sensor-set-position" - при установке информации о местоположении приёмных антенн;
 * - "sensor-set-enable" - при включении или отключении датчика.
 *
 * Прототипы обработчиков сигналов:
 *
 * \code
 *
 * gboolean sensor_virtual_port_param_cb  (HyScanSensorControlServer   *server,
 *                                         const gchar                 *name,
 *                                         guint                        channel,
 *                                         gint64                       time_offset,
 *                                         gpointer                     user_data);
 *
 * gboolean sensor_uart_port_param_cb     (HyScanSensorControlServer   *server,
 *                                         const gchar                 *name,
 *                                         guint                        channel,
 *                                         gint64                       time_offset,
 *                                         HyScanSensorProtocolType     protocol,
 *                                         guint                        uart_device,
 *                                         guint                        uart_mode,
 *                                         gpointer                     user_data);
 *
 * gboolean sensor_udp_ip_port_param_cb   (HyScanSensorControlServer   *server,
 *                                         const gchar                 *name,
 *                                         guint                        channel,
 *                                         gint64                       time_offset,
 *                                         HyScanSensorProtocolType     protocol,
 *                                         guint                        ip_address,
 *                                         guint                        udp_port,
 *                                         gpointer                     user_data);
 *
 * gboolean sensor_set_position_cb        (HyScanSensorControlServer   *server,
 *                                         const gchar                 *name,
 *                                         HyScanAntennaPosition        position,
 *                                         gpointer                     user_data);
 *
 * gboolean sensor_set_enable_cb          (HyScanSensorControlServer   *server,
 *                                         const gchar                 *name,
 *                                         gboolean                     enable,
 *                                         gpointer                     user_data);
 *
 * \endcode
 *
 * Описание параметров сигналов аналогично параметрам функций \link hyscan_sensor_control_set_virtual_port_param \endlink,
 * \link hyscan_sensor_control_set_uart_port_param \endlink, \link hyscan_sensor_control_set_udp_ip_port_param \endlink,
 * \link hyscan_sensor_control_set_position \endlink и \link hyscan_sensor_control_set_enable \endlink,
 * класса \link HyScanSensorControl \endlink.
 *
 * Обработчик сигнала должен вернуть значение TRUE - если команда успешно выполнена,
 * FALSE - в случае ошибки.
 *
 * Функция #hyscan_sensor_control_server_send_data предназаначена для отправки данных датчиков.
 *
 */

#ifndef __HYSCAN_SENSOR_CONTROL_SERVER_H__
#define __HYSCAN_SENSOR_CONTROL_SERVER_H__

#include <hyscan-data-writer.h>
#include <hyscan-sonar-box.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SENSOR_CONTROL_SERVER             (hyscan_sensor_control_server_get_type ())
#define HYSCAN_SENSOR_CONTROL_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SENSOR_CONTROL_SERVER, HyScanSensorControlServer))
#define HYSCAN_IS_SENSOR_CONTROL_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SENSOR_CONTROL_SERVER))
#define HYSCAN_SENSOR_CONTROL_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SENSOR_CONTROL_SERVER, HyScanSensorControlServerClass))
#define HYSCAN_IS_SENSOR_CONTROL_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SENSOR_CONTROL_SERVER))
#define HYSCAN_SENSOR_CONTROL_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SENSOR_CONTROL_SERVER, HyScanSensorControlServerClass))

typedef struct _HyScanSensorControlServer HyScanSensorControlServer;
typedef struct _HyScanSensorControlServerPrivate HyScanSensorControlServerPrivate;
typedef struct _HyScanSensorControlServerClass HyScanSensorControlServerClass;

struct _HyScanSensorControlServer
{
  GObject parent_instance;

  HyScanSensorControlServerPrivate *priv;
};

struct _HyScanSensorControlServerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_sensor_control_server_get_type           (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSensorControlServer \endlink.
 * Функция не создаёт дополнительной ссылки на бъект с параметрами гидролокатора,
 * этот объект должен существовать всё время работы сервера.
 *
 * \param sonar указатель на базовый класс \link HyScanSonarBox \endlink.
 *
 * \return Указатель на объект \link HyScanSensorControlServer \endlink.
 *
 */
HYSCAN_API
HyScanSensorControlServer     *hyscan_sensor_control_server_new                (HyScanSonarBox              *sonar);

/**
 *
 * Функция передаёт данные датчиков, через отправку сигнала "data".
 *
 * \param server указатель на интерфейс \link HyScanSensorControlServer \endlink;
 * \param name название порта;
 * \param type тип данных, \link HyScanDataType \endlink;
 * \param data данные датчиков.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_sensor_control_server_send_data          (HyScanSensorControlServer   *server,
                                                                                const gchar                 *name,
                                                                                HyScanDataType               type,
                                                                                HyScanDataWriterData        *data);

G_END_DECLS

#endif /* __HYSCAN_SENSOR_CONTROL_SERVER_H__ */
