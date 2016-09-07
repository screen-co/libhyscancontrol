/**
 * \file hyscan-sensor-control.h
 *
 * \brief Заголовочный файл класса управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSensorControl HyScanSensorControl - класс управления датчиками местоположения и ориентации
 *
 * Класс предназначен для настройки портов к которым подключены датчики местоположения и ориентации.
 *
 * Класс HyScanSensorControl наследуется от класса \link HyScanWriteControl \endlink и используется
 * как базовый для классов управления локаторами.
 *
 * Класс HyScanSensorControl умеет управлять следующими типами портов:
 *
 * - HYSCAN_SONAR_SENSOR_PORT_VIRTUAL - "виртуальный" порт (например встроенный в гидролокатор датчик и т.п.);
 * - HYSCAN_SONAR_SENSOR_PORT_UART - порт приёма данных через последовательный UART порт;
 * - HYSCAN_SONAR_SENSOR_PORT_UDP_IP - порт приёма данных по протоколу UDP/IP.
 *
 * Список портов, доступных для работы, можно узнать с помощью функции #hyscan_sensor_control_list_ports.
 * Пользователь должен освободить память, занимаемую этим списком, функцией g_strfreev.
 * Для идентификации порта используется его название из этого списка. Тип порта можно узнать
 * с помощью функции #hyscan_sensor_control_get_port_type.
 *
 * Узнать текущее состояние порта можно функцией #hyscan_sensor_control_get_port_status.
 *
 * Данные, принимаемые портом, дополнительно маркируются номером приёмного канала. Это позволяет
 * принимать данные от нескольких однотипных датчиков, например двух приёмников GPS. В этом случае,
 * для корректной работы системы, необходимо указать разные номера каналов для портов. Номера каналов
 * задаются при настройке портов.
 *
 * Для каждого порта можно установить коррекцию времени приёма данных. Это позволяет нивелировать
 * ошибку, возникающую из-за задержки, требуемой датчиком для решения и передачи данных. Значение
 * указывается в микросекундах и должно быть положительным или равным нулю.
 *
 * Настройка портов осуществляется в зависимости от их типа.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_VIRTUAL настраиваются только по номеру канала. Параметры порта этого типа
 * задаются функцией #hyscan_sensor_control_set_virtual_port_param.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_UART могут принимать данные через последовательный UART порт. Для их
 * корректной работы необходимо указать UART устройство и режим его работы. Список UART устройств
 * можно узнать функцией #hyscan_sensor_control_list_uart_devices. Допустимые режимы работы можно
 * узнать функцией #hyscan_sensor_control_list_uart_modes. Параметры порта этого типа задаются функцией
 * #hyscan_sensor_control_set_uart_port_param.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_UDP_IP могут принимать данные по протоколу UDP/IP. Для их
 * корректной работы необходимо указать IP адрес и номер UDP порта, по которым порт будет
 * принимать данные. Список допустимых IP адресов можно узнать функцией #hyscan_sensor_control_list_ip_addresses.
 * Номер UDP порта должен выбираться в диапазоне 1024 - 65535. Параметры порта этого типа
 * задаются функцией #hyscan_sensor_control_set_udp_ip_port_param.
 *
 * Для портов HYSCAN_SENSOR_CONTROL_PORT_UDP_IP и HYSCAN_SENSOR_CONTROL_PORT_UART необходимо указать протокол
 * обмена данными \link HyScanSensorProtocolType \endlink.
 *
 * Для каждого из портов можно указать информацию о местоположении приёмных антенн относительно центра
 * масс судна. Для этого используется функция #hyscan_sensor_control_set_position.
 *
 * Порт можно включить или выключить с помощью функции #hyscan_sensor_control_set_enable.
 *
 * При получении данных от датчиков, класс посылает сигнал "sensor-data", в котором передаёт их
 * пользователю. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    data_cb    (HyScanSensorControl  *control,
 *                     const gchar          *name,
 *                     HyScanDataWriterData *data);
 *
 * \endcode
 *
 * Где name - название порта.
 *
 * Класс HyScanSensorControl поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_SENSOR_CONTROL_H__
#define __HYSCAN_SENSOR_CONTROL_H__

#include <hyscan-data-writer.h>
#include <hyscan-data-schema.h>
#include <hyscan-sonar.h>

G_BEGIN_DECLS

/** \brief Типы портов датчиков. */
typedef enum
{
  HYSCAN_SENSOR_PORT_INVALID                           = 0,    /**< Недопустимый тип, ошибка. */

  HYSCAN_SENSOR_PORT_VIRTUAL                           = 101,  /**< Виртуальный порт.  */
  HYSCAN_SENSOR_PORT_UART                              = 102,  /**< UART порт. */
  HYSCAN_SENSOR_PORT_UDP_IP                            = 103   /**< UDP IP порт. */
} HyScanSensorPortType;

/** \brief Типы протоколов данных от датчиков. */
typedef enum
{
  HYSCAN_SENSOR_PROTOCOL_INVALID                       = 0,    /**< Недопустимый тип, ошибка. */

  HYSCAN_SENSOR_PROTOCOL_SAS                           = 101,  /**< Протокол САД.  */
  HYSCAN_SENSOR_PROTOCOL_NMEA_0183                     = 102   /**< Протокол NMEA 0183.  */
} HyScanSensorProtocolType;

/** \brief Состояние портов датчиков. */
typedef enum
{
  HYSCAN_SENSOR_PORT_STATUS_INVALID                    = 0,    /**< Недопустимый тип, ошибка. */

  HYSCAN_SENSOR_PORT_STATUS_DISABLED                   = 101,  /**< Порт отключен. */
  HYSCAN_SENSOR_PORT_STATUS_OK                         = 102,  /**< Порт принимает данные без ошибок. */
  HYSCAN_SENSOR_PORT_STATUS_WARNING                    = 103,  /**< Порт принимает данные с ошибками. */
  HYSCAN_SENSOR_PORT_STATUS_ERROR                      = 104   /**< Нет данных. */
} HyScanSensorPortStatus;

#define HYSCAN_TYPE_SENSOR_CONTROL             (hyscan_sensor_control_get_type ())
#define HYSCAN_SENSOR_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SENSOR_CONTROL, HyScanSensorControl))
#define HYSCAN_IS_SENSOR_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SENSOR_CONTROL))
#define HYSCAN_SENSOR_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SENSOR_CONTROL, HyScanSensorControlClass))
#define HYSCAN_IS_SENSOR_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SENSOR_CONTROL))
#define HYSCAN_SENSOR_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SENSOR_CONTROL, HyScanSensorControlClass))

typedef struct _HyScanSensorControl HyScanSensorControl;
typedef struct _HyScanSensorControlPrivate HyScanSensorControlPrivate;
typedef struct _HyScanSensorControlClass HyScanSensorControlClass;

struct _HyScanSensorControl
{
  HyScanDataWriter parent_instance;

  HyScanSensorControlPrivate *priv;
};

struct _HyScanSensorControlClass
{
  HyScanDataWriterClass parent_class;
};

HYSCAN_API
GType                          hyscan_sensor_control_get_type                  (void);

/**
 *
 * Функция возвращает список портов, к которым могут быть подключены датчики. Пользователь
 * должен освободить память, занимаемую списком, функцией g_strfreev.
 *
 * \param control указатель на объект \link HyScanSensorControl \endlink.
 *
 * \return Список портов или NULL.
 *
 */
HYSCAN_API
gchar                        **hyscan_sensor_control_list_ports                (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает список физических устройств UART, присутствующих в системе.
 * Пользователь должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink.
 *
 * \return Список физических устройств UART или NULL.
 *
 */
HYSCAN_API
HyScanDataSchemaEnumValue    **hyscan_sensor_control_list_uart_devices         (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает список допустимых режимов работы UART устройства.
 * Пользователь должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink.
 *
 * \return Список режимов обмена данными или NULL.
 *
 */
HYSCAN_API
HyScanDataSchemaEnumValue    **hyscan_sensor_control_list_uart_modes           (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает список допустимых IP адресов, для которых можно включить приём данных
 * от датчиков. Пользователь должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink.
 *
 * \return Список IP адресов или NULL.
 *
 */
HYSCAN_API
HyScanDataSchemaEnumValue    **hyscan_sensor_control_list_ip_addresses         (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает тип порта.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название  порта.
 *
 * \return Тип порта.
 *
 */
HYSCAN_API
HyScanSensorPortType           hyscan_sensor_control_get_port_type             (HyScanSensorControl       *control,
                                                                                const gchar               *name);

/**
 *
 * Функция возвращает текущее состояние порта.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название  порта.
 *
 * \return Состояние порта.
 *
 */
HYSCAN_API
HyScanSensorPortStatus         hyscan_sensor_control_get_port_status           (HyScanSensorControl       *control,
                                                                                const gchar               *name);

/**
 *
 * Функция устанавливает номер канала для порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sensor_control_set_virtual_port_param    (HyScanSensorControl       *control,
                                                                                const gchar               *name,
                                                                                guint                      channel,
                                                                                gint64                     time_offset);

/**
 *
 * Функция устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART.
 *
 * В эту функцию передаются идентификаторы устойства UART и режима его работы
 * из списка допустимых значений (см. #hyscan_sensor_control_list_uart_devices и
 * #hyscan_sensor_control_list_uart_modes).
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param uart_device идентификатор устройства UART;
 * \param uart_mode идентификатор режима работы устройства UART.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sensor_control_set_uart_port_param       (HyScanSensorControl       *control,
                                                                                const gchar               *name,
                                                                                guint                      channel,
                                                                                gint64                     time_offset,
                                                                                HyScanSensorProtocolType   protocol,
                                                                                gint64                     uart_device,
                                                                                gint64                     uart_mode);

/**
 *
 * Функция устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP.
 *
 * В эту функцию передаётся идентфикатор IP адреса из списка допустимых значений
 * (см. #hyscan_sensor_control_list_ip_addresses).
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param ip_address идентификатор IP адреса, по которому принимать данные;
 * \param udp_port номер UDP порта, по которому принимать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sensor_control_set_udp_ip_port_param     (HyScanSensorControl       *control,
                                                                                const gchar               *name,
                                                                                guint                      channel,
                                                                                gint64                     time_offset,
                                                                                HyScanSensorProtocolType   protocol,
                                                                                gint64                     ip_address,
                                                                                guint16                    udp_port);

/**
 *
 * Функция устанавливает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название порта;
 * \param x смещение антенны по оси X, метры;
 * \param y смещение антенны по оси Y, метры;
 * \param z смещение антенны по оси Z, метры;
 * \param psi угол поворота антенны по курсу, радианы;
 * \param gamma угол поворота антенны по крену, радианы;
 * \param theta угол поворота антенны по дифференту, радианы.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sensor_control_set_position              (HyScanSensorControl       *control,
                                                                                const gchar               *name,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                gdouble                    z,
                                                                                gdouble                    psi,
                                                                                gdouble                    gamma,
                                                                                gdouble                    theta);

/**
 *
 * Функция включает или выключает приём данных на указанном порту.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param name название порта;
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sensor_control_set_enable                (HyScanSensorControl       *control,
                                                                                const gchar               *name,
                                                                                gboolean                   enable);

G_END_DECLS

#endif /* __HYSCAN_SENSOR_CONTROL_H__ */
