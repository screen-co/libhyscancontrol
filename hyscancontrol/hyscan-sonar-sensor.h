/**
 * \file hyscan-sonar-sensor.h
 *
 * \brief Заголовочный файл интерфейса управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarSensor HyScanSonarSensor - интерфейс управления датчиками местоположения и ориентации
 *
 * Интерфейс предназначен для настройки портов к которым подключены датчики местоположения и ориентации.
 *
 * Интерфейс HyScanSonarSensor умеет управлять следующими типами портов:
 *
 * - HYSCAN_SONAR_SENSOR_PORT_VIRTUAL - "виртуальный" порт (например встроенный в гидролокатор датчик и т.п.);
 * - HYSCAN_SONAR_SENSOR_PORT_IP - порт приёма данных по протоколу UDP/IP;
 * - HYSCAN_SONAR_SENSOR_PORT_RS232 - порт приёма данных по физическому порту RS232.
 *
 * Список портов, доступных для работы, можно узнать с помощью функции #hyscan_sonar_sensor_list_ports.
 * Пользователь должен освободить память, занимаемую этим списком, функцией #hyscan_sonar_sensor_free_ports.
 * Для идентификации порта используется идентификатор из структуры \link HyScanSonarSensorPort \endlink.
 *
 * Для всех типов портов, можно включить или выключить приём данных с помощью функции #hyscan_sonar_sensor_set_enable.
 * Узнать текущее состояние приёма данных можно функцией #hyscan_sonar_sensor_get_enable.
 *
 * Данные, принимаемые портом, дополнительно маркируются номер приёмного канала. Это позволяет
 * принимать данные от нескольких однотипных датчиков, например двух приёмников GPS. В этом случае,
 * для корректной работы системы, необходимо указать разные номера каналов для портов. Номера каналов
 * задаются при настройке портов.
 *
 * Дальнейшая настройка портов осуществляется в зависимости от их типа.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_VIRTUAL настраиваются только по номеру канала. Параметры порта этого типа
 * задаются функцией #hyscan_sonar_sensor_set_virtual_port_param. Считать текущие параметры можно функцией
 * #hyscan_sonar_sensor_get_virtual_port_param.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_IP могут принимать данные по протоколу UDP/IP. Для их
 * корректной работы необходимо указать IP адрес и номер UDP порта, по которым порт будет
 * принимать данные. Список допустимых IP адресов можно узнать функцией #hyscan_sonar_sensor_list_ip_addresses.
 * Номер UDP порта должен выбираться в диапазоне 1024 - 65535. Параметры порта этого типа
 * задаются функцией #hyscan_sonar_sensor_set_ip_port_param. Считать текущие параметры можно функцией
 * #hyscan_sonar_sensor_get_ip_port_param.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_RS232 могут принимать данные по физическому порту RS232. Для их
 * корректной работы необходимо указать порт RS232 и скорость обмена данными. Список портов RS232
 * можно узнать функцией #hyscan_sonar_sensor_list_rs232_ports. Допустимые скорости работы можно узнать
 * функцией #hyscan_sonar_sensor_list_rs232_speeds. Параметры порта этого типа задаются функцией
 * #hyscan_sonar_sensor_set_rs232_port_param. Считать текущие параметры можно функцией
 * #hyscan_sonar_sensor_get_rs232_port_param.
 *
 * Для портов HYSCAN_SONAR_SENSOR_PORT_IP и HYSCAN_SONAR_SENSOR_PORT_RS232 необходимо указать протокол
 * обмена данными \link HyScanSonarSensorProtocol \endlink.
 *
 */

#ifndef __HYSCAN_SONAR_SENSOR_H__
#define __HYSCAN_SONAR_SENSOR_H__

#include <hyscan-types.h>
#include <hyscan-data-schema.h>
#include <hyscan-control-exports.h>

G_BEGIN_DECLS

/** \brief Типы портов датчиков. */
typedef enum
{
  HYSCAN_SONAR_SENSOR_PORT_INVALID             = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_SENSOR_PORT_VIRTUAL             = 101,          /**< Виртуальный порт.  */
  HYSCAN_SONAR_SENSOR_PORT_IP                  = 102,          /**< IP порт. */
  HYSCAN_SONAR_SENSOR_PORT_RS232               = 103           /**< RS232 порт. */
} HyScanSonarSensorPortType;

/** \brief Типы протоколов данных от датчиков. */
typedef enum
{
  HYSCAN_SONAR_SENSOR_PROTOCOL_INVALID         = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_SENSOR_PROTOCOL_SAS             = 101,          /**< Протокол САД.  */

  HYSCAN_SONAR_SENSOR_PROTOCOL_NMEA_0183       = 201           /**< Протокол NMEA 0183.  */
} HyScanSonarSensorProtocol;

/** \brief Состояние портов датчиков. */
typedef enum
{
  HYSCAN_SONAR_SENSOR_PORT_STATUS_INVALID      = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_SENSOR_PORT_STATUS_DISABLED     = 101,          /**< Порт отключен. */
  HYSCAN_SONAR_SENSOR_PORT_STATUS_OK           = 102,          /**< Порт принимает данные без ошибок. */
  HYSCAN_SONAR_SENSOR_PORT_STATUS_WARNING      = 103,          /**< Порт принимает данные с ошибками. */
  HYSCAN_SONAR_SENSOR_PORT_STATUS_ERROR        = 104           /**< Нет данных. */
} HyScanSonarSensorPortStatus;

/** \brief Описание порта для подключения датчиков позиционирования и ориентации. */
typedef struct
{
  gint                                         id;             /**< Идентификатор порта. */
  gchar                                       *name;           /**< Название порта. */
  HyScanSonarSensorPortType                    type;           /**< Тип порта. */
} HyScanSonarSensorPort;

#define HYSCAN_TYPE_SONAR_SENSOR            (hyscan_sonar_sensor_get_type ())
#define HYSCAN_SONAR_SENSOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_SENSOR, HyScanSonarSensor))
#define HYSCAN_IS_SONAR_SENSOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_SENSOR))
#define HYSCAN_SONAR_SENSOR_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SONAR_SENSOR, HyScanSonarSensorInterface))

typedef struct _HyScanSonarSensor HyScanSonarSensor;
typedef struct _HyScanSonarSensorInterface HyScanSonarSensorInterface;

struct _HyScanSonarSensorInterface
{
  GTypeInterface               g_iface;

  HyScanSonarSensorPort      **(*list_ports)                (HyScanSonarSensor              *sensor);

  HyScanDataSchemaEnumValue  **(*list_ip_addresses)         (HyScanSonarSensor              *sensor);

  HyScanDataSchemaEnumValue  **(*list_rs232_ports)          (HyScanSonarSensor              *sensor);

  HyScanDataSchemaEnumValue  **(*list_rs232_speeds)         (HyScanSonarSensor              *sensor);

  HyScanSonarSensorPortStatus  (*get_port_status)           (HyScanSonarSensor              *sensor,
                                                             gint                            port_id);

  gboolean                     (*set_virtual_port_param)    (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             HyScanSonarSensorChannel        channel);

  gboolean                     (*get_virtual_port_param)    (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             HyScanSonarSensorChannel       *channel);

  gboolean                     (*set_ip_port_param)         (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             HyScanSonarSensorChannel        channel,
                                                             HyScanSonarSensorProtocol       protocol,
                                                             gint64                          ip_address,
                                                             guint16                         udp_port);

  gboolean                     (*get_ip_port_param)         (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             HyScanSonarSensorChannel       *channel,
                                                             HyScanSonarSensorProtocol      *protocol,
                                                             gint64                         *ip_address,
                                                             guint16                        *udp_port);

  gboolean                     (*set_rs232_port_param)      (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             HyScanSonarSensorChannel        channel,
                                                             HyScanSonarSensorProtocol       protocol,
                                                             gint64                          rs232_port,
                                                             gint64                          rs232_speed);

  gboolean                     (*get_rs232_port_param)      (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             HyScanSonarSensorChannel       *channel,
                                                             HyScanSonarSensorProtocol      *protocol,
                                                             gint64                         *rs232_port,
                                                             gint64                         *rs232_speed);

  gboolean                     (*set_enable)                (HyScanSonarSensor              *sensor,
                                                             gint                            port_id,
                                                             gboolean                        enable);

  gboolean                     (*get_enable)                (HyScanSonarSensor              *sensor,
                                                             gint                            port_id);
};

HYSCAN_CONTROL_EXPORT
GType                        hyscan_sonar_sensor_get_type                (void);

/**
 *
 * Функция возвращает список портов, к которым могут быть подключены датчики. Пользователь
 * должен освободить память, занимаемую списком, функцией #hyscan_sonar_sensor_free_ports.
 *
 * \param sensor указатель на объект \link HyScanSonarSensor \endlink.
 *
 * \return Список портов или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSonarSensorPort      **hyscan_sonar_sensor_list_ports              (HyScanSonarSensor         *sensor);

/**
 *
 * Функция возвращает список допустимых IP адресов, для которых можно включить приём данных
 * от датчиков. Пользователь должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink.
 *
 * \return Список IP адресов или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue  **hyscan_sonar_sensor_list_ip_addresses       (HyScanSonarSensor         *sensor);

/**
 *
 * Функция возвращает список физических портов RS232, присутствующих в системе.
 * Пользователь должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink.
 *
 * \return Список физических портов RS232 или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue  **hyscan_sonar_sensor_list_rs232_ports        (HyScanSonarSensor         *sensor);

/**
 *
 * Функция возвращает список допустимых скоростей работы физических портов RS232. Пользователь
 * должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink.
 *
 * \return Список скоростей порта RS232 или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue  **hyscan_sonar_sensor_list_rs232_speeds       (HyScanSonarSensor         *sensor);

/**
 *
 * Функция возвращает текущее состояние порта.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта.
 *
 * \return Состояние порта.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSonarSensorPortStatus  hyscan_sonar_sensor_get_port_status         (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id);

/**
 *
 * Функция устанавливает номер канала для порта типа HYSCAN_SONAR_SENSOR_PORT_VIRTUAL.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param channel номер канала.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_set_virtual_port_param  (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          HyScanSonarSensorChannel   channel);

/**
 *
 * Функция считывает номер канала для порта типа HYSCAN_SONAR_SENSOR_PORT_VIRTUAL.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param channel указатель на переменную для номера канала или NULL.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_get_virtual_port_param  (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          HyScanSonarSensorChannel  *channel);

/**
 *
 * Функция устанавливает режим работы порта типа HYSCAN_SONAR_SENSOR_PORT_IP.
 *
 * В эту функцию передаётся идентфикатор IP адреса из списка допустимых значений
 * (см. #hyscan_sonar_sensor_list_ip_addresses).
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param channel номер канала;
 * \param protocol протокол обмена данными с датчиком;
 * \param ip_address идентификатор IP адреса, по которому принимать данные;
 * \param udp_port номер UDP порта, по которому принимать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_set_ip_port_param       (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          HyScanSonarSensorChannel   channel,
                                                                          HyScanSonarSensorProtocol  protocol,
                                                                          gint64                     ip_address,
                                                                          guint16                    udp_port);

/**
 *
 * Функция считывает режим работы порта типа HYSCAN_SONAR_SENSOR_PORT_IP.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param channel указатель на переменную для номера канала или NULL;
 * \param protocol указатель на переменную для протокола обмена данными с датчиком или NULL;
 * \param ip_address указатель на переменную для идентификатора IP адреса или NULL;
 * \param udp_port указатель на переменную для номера UDP порта или NULL.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_get_ip_port_param       (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          HyScanSonarSensorChannel  *channel,
                                                                          HyScanSonarSensorProtocol *protocol,
                                                                          gint64                    *ip_address,
                                                                          guint16                   *udp_port);

/**
 *
 * Функция устанавливает режим работы порта типа HYSCAN_SONAR_SENSOR_PORT_RS232.
 *
 * В эту функцию передаются идентификаторы физического порта RS232 и скорости его работы
 * из списка допустимых значений (см. #hyscan_sonar_sensor_list_rs232_ports и #hyscan_sonar_sensor_list_rs232_speeds).
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param channel номер канала;
 * \param protocol протокол обмена данными с датчиком;
 * \param rs232_port идентификатор физического порта RS232, по которому принимать данные;
 * \param rs232_speed идентификатор скорости физического порта RS232, на которой принимать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_set_rs232_port_param    (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          HyScanSonarSensorChannel   channel,
                                                                          HyScanSonarSensorProtocol  protocol,
                                                                          gint64                     rs232_port,
                                                                          gint64                     rs232_speed);

/**
 *
 * Функция считывает режим работы порта типа HYSCAN_SONAR_SENSOR_PORT_RS232.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param channel указатель на переменную для номера канала или NULL;
 * \param protocol указатель на переменную для протокола обмена данными с датчиком или NULL;
 * \param rs232_port указатель на переменную для идентификатора физического порта RS232;
 * \param rs232_speed указатель на переменную для идентификатора скорости физического порта RS232.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_get_rs232_port_param    (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          HyScanSonarSensorChannel  *channel,
                                                                          HyScanSonarSensorProtocol *protocol,
                                                                          gint64                    *rs232_port,
                                                                          gint64                    *rs232_speed);

/**
 *
 * Функция включает или выключает приём данных на указанном порту.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта;
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_set_enable              (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id,
                                                                          gboolean                   enable);

/**
 *
 * Функция возвращает состояние приёма данных на указанном порту.
 *
 * \param sensor указатель на интерфейс \link HyScanSonarSensor \endlink;
 * \param port_id идентификатор порта.
 *
 * \return TRUE - если приём данных включен, иначе - FALSE.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                     hyscan_sonar_sensor_get_enable              (HyScanSonarSensor         *sensor,
                                                                          gint                       port_id);

/**
 *
 * Функция освобождает память, занятую списком портов.
 *
 * \param ports список портов.
 *
 * \return Нет.
 *
 */
void                         hyscan_sonar_sensor_free_ports              (HyScanSonarSensorPort    **ports);

G_END_DECLS

#endif /* __HYSCAN_SONAR_SENSOR_H__ */
