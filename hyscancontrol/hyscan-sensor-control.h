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
 * Класс HyScanSensorControl наследуется от класса \link HyScanWriteControl \endlink, соответственно
 * имеется возможность использовать любые функции этого класса. Класс HyScanSensorControl переопределяет
 * методы start и stop класса \link HyScanWriteControl \endlink таким образом, что их поведение становится
 * аналогичным функциям #hyscan_sensor_control_start и #hyscan_sensor_control_stop.
 *
 * Класс создаётся при помощи функции #hyscan_sensor_control_new. Если при создании класса не передавать
 * указатель на \link HyScanDB \endlink, данные от датчиков не будут записываться.
 *
 * При получении данных от датчиков объект использует сигнал "sensor-data" для передачи их пользователю,
 * помимо записи в систему хранения. Обработчик этого сигнала должен иметь следующее определение:
 *
 * - void data_cb (HyScanSensorControl *control, HyScanWriteData *data, HyScanSensorChannelInfo *info);
 *
 * Класс HyScanSensorControl умеет управлять следующими типами портов:
 *
 * - HYSCAN_SONAR_SENSOR_PORT_VIRTUAL - "виртуальный" порт (например встроенный в гидролокатор датчик и т.п.);
 * - HYSCAN_SONAR_SENSOR_PORT_IP - порт приёма данных по протоколу UDP/IP;
 * - HYSCAN_SONAR_SENSOR_PORT_RS232 - порт приёма данных по физическому порту RS232.
 *
 * Список портов, доступных для работы, можно узнать с помощью функции #hyscan_sensor_control_list_ports.
 * Пользователь должен освободить память, занимаемую этим списком, функцией #hyscan_sensor_control_free_ports.
 * Для идентификации порта используется идентификатор из структуры \link HyScanSensorPort \endlink.
 *
 * Данные, принимаемые портом, дополнительно маркируются номер приёмного канала. Это позволяет
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
 * задаются функцией #hyscan_sensor_control_set_virtual_port_param. Считать текущие параметры можно функцией
 * #hyscan_sensor_control_get_virtual_port_param.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_IP могут принимать данные по протоколу UDP/IP. Для их
 * корректной работы необходимо указать IP адрес и номер UDP порта, по которым порт будет
 * принимать данные. Список допустимых IP адресов можно узнать функцией #hyscan_sensor_control_list_ip_addresses.
 * Номер UDP порта должен выбираться в диапазоне 1024 - 65535. Параметры порта этого типа
 * задаются функцией #hyscan_sensor_control_set_ip_port_param. Считать текущие параметры можно функцией
 * #hyscan_sensor_control_get_ip_port_param.
 *
 * Порты типа HYSCAN_SONAR_SENSOR_PORT_RS232 могут принимать данные по физическому порту RS232. Для их
 * корректной работы необходимо указать порт RS232 и скорость обмена данными. Список портов RS232
 * можно узнать функцией #hyscan_sensor_control_list_rs232_ports. Допустимые скорости работы можно узнать
 * функцией #hyscan_sensor_control_list_rs232_speeds. Параметры порта этого типа задаются функцией
 * #hyscan_sensor_control_set_rs232_port_param. Считать текущие параметры можно функцией
 * #hyscan_sensor_control_get_rs232_port_param.
 *
 * Для портов HYSCAN_SENSOR_CONTROL_PORT_IP и HYSCAN_SENSOR_CONTROL_PORT_RS232 необходимо указать протокол
 * обмена данными \link HyScanSensorProtocolType \endlink.
 *
 * Для каждого из портов можно указать информацию о местоположении приёмных антенн относительно центра
 * масс судна. Для этого используется функция #hyscan_sensor_control_set_position. Получить текущие
 * параметры местоположения можно функцией #hyscan_sensor_control_get_position.
 *
 * Для всех типов портов, можно включить или выключить приём данных с помощью функции #hyscan_sensor_control_set_enable.
 * Узнать текущее состояние приёма данных можно функцией #hyscan_sensor_control_get_enable.
 *
 * Объекты класса HyScanSensorControl допускают использование в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_SENSOR_CONTROL_H__
#define __HYSCAN_SENSOR_CONTROL_H__

#include <hyscan-write-control.h>
#include <hyscan-data-schema.h>
#include <hyscan-sonar.h>

G_BEGIN_DECLS

/** \brief Типы портов датчиков. */
typedef enum
{
  HYSCAN_SENSOR_PORT_INVALID                           = 0,    /**< Недопустимый тип, ошибка. */

  HYSCAN_SENSOR_PORT_VIRTUAL                           = 101,  /**< Виртуальный порт.  */
  HYSCAN_SENSOR_PORT_IP                                = 102,  /**< IP порт. */
  HYSCAN_SENSOR_PORT_RS232                             = 103   /**< RS232 порт. */
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

/** \brief Описание порта для подключения датчиков позиционирования и ориентации. */
typedef struct
{
  gint                         id;                             /**< Идентификатор порта. */
  gchar                       *name;                           /**< Название порта. */
  HyScanSensorPortType         type;                           /**< Тип порта. */
} HyScanSensorPort;

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
  HyScanWriteControl parent_instance;

  HyScanSensorControlPrivate *priv;
};

struct _HyScanSensorControlClass
{
  HyScanWriteControlClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                          hyscan_sensor_control_get_type                  (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSensorControl \endlink.
 *
 * \param sonar указатель на объект \link HyScanSonar \endlink;
 * \param db указатель на объект \link HyScanDB \endlink или NULL.
 *
 * \return Указатель на объект \link HyScanSensorControl \endlink.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSensorControl           *hyscan_sensor_control_new                       (HyScanSonar               *sonar,
                                                                                HyScanDB                  *db);

/**
 *
 * Функция включает запись данных от датчиков.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param project_name название проекта, в который записывать данные;
 * \param track_name название галса, в который записывать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_start                     (HyScanSensorControl       *control,
                                                                                const gchar               *project_name,
                                                                                const gchar               *track_name);

/**
 *
 * Функция отключает запись данных от датчиков.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void                           hyscan_sensor_control_stop                      (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает список портов, к которым могут быть подключены датчики. Пользователь
 * должен освободить память, занимаемую списком, функцией #hyscan_sensor_control_free_ports.
 *
 * \param control указатель на объект \link HyScanSensorControl \endlink.
 *
 * \return Список портов или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSensorPort             **hyscan_sensor_control_list_ports                (HyScanSensorControl       *control);

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
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue    **hyscan_sensor_control_list_ip_addresses         (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает список физических портов RS232, присутствующих в системе.
 * Пользователь должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink.
 *
 * \return Список физических портов RS232 или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue    **hyscan_sensor_control_list_rs232_ports          (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает список допустимых скоростей работы физических портов RS232. Пользователь
 * должен освободить память, занимаемую списком, функцией
 * \link hyscan_data_schema_free_enum_values \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink.
 *
 * \return Список скоростей порта RS232 или NULL.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanDataSchemaEnumValue    **hyscan_sensor_control_list_rs232_speeds         (HyScanSensorControl       *control);

/**
 *
 * Функция возвращает текущее состояние порта.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта.
 *
 * \return Состояние порта.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSensorPortStatus         hyscan_sensor_control_get_port_status           (HyScanSensorControl       *control,
                                                                                gint                       port_id);

/**
 *
 * Функция устанавливает номер канала для порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_set_virtual_port_param    (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                HyScanSonarChannelIndex    channel,
                                                                                gint64                     time_offset);

/**
 *
 * Функция считывает номер канала для порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param channel указатель на переменную для номера канала или NULL;
 * \param time_offset указатель на переменную для коррекция времени приёма данных или NULL.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_get_virtual_port_param    (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                HyScanSonarChannelIndex   *channel,
                                                                                gint64                    *time_offset);

/**
 *
 * Функция устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_IP.
 *
 * В эту функцию передаётся идентфикатор IP адреса из списка допустимых значений
 * (см. #hyscan_sensor_control_list_ip_addresses).
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param ip_address идентификатор IP адреса, по которому принимать данные;
 * \param udp_port номер UDP порта, по которому принимать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_set_ip_port_param         (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                HyScanSonarChannelIndex    channel,
                                                                                gint64                     time_offset,
                                                                                HyScanSensorProtocolType   protocol,
                                                                                gint64                     ip_address,
                                                                                guint16                    udp_port);

/**
 *
 * Функция считывает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_IP.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param channel указатель на переменную для номера канала или NULL;
 * \param time_offset указатель на переменную для коррекция времени приёма данных или NULL;
 * \param protocol указатель на переменную для протокола обмена данными с датчиком или NULL;
 * \param ip_address указатель на переменную для идентификатора IP адреса или NULL;
 * \param udp_port указатель на переменную для номера UDP порта или NULL.
 *
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_get_ip_port_param         (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                HyScanSonarChannelIndex   *channel,
                                                                                gint64                    *time_offset,
                                                                                HyScanSensorProtocolType  *protocol,
                                                                                gint64                    *ip_address,
                                                                                guint16                   *udp_port);

/**
 *
 * Функция устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_RS232.
 *
 * В эту функцию передаются идентификаторы физического порта RS232 и скорости его работы
 * из списка допустимых значений (см. #hyscan_sensor_control_list_rs232_ports и #hyscan_sensor_control_list_rs232_speeds).
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param rs232_port идентификатор физического порта RS232, по которому принимать данные;
 * \param rs232_speed идентификатор скорости физического порта RS232, на которой принимать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_set_rs232_port_param      (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                HyScanSonarChannelIndex    channel,
                                                                                gint64                     time_offset,
                                                                                HyScanSensorProtocolType   protocol,
                                                                                gint64                     rs232_port,
                                                                                gint64                     rs232_speed);

/**
 *
 * Функция считывает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_RS232.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param channel указатель на переменную для номера канала или NULL;
 * \param time_offset указатель на переменную для коррекция времени приёма данных или NULL;
 * \param protocol указатель на переменную для протокола обмена данными с датчиком или NULL;
 * \param rs232_port указатель на переменную для идентификатора физического порта RS232;
 * \param rs232_speed указатель на переменную для идентификатора скорости физического порта RS232.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_get_rs232_port_param      (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                HyScanSonarChannelIndex   *channel,
                                                                                gint64                    *time_offset,
                                                                                HyScanSensorProtocolType  *protocol,
                                                                                gint64                    *rs232_port,
                                                                                gint64                    *rs232_speed);

/**
 *
 * Функция устанавливает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCore \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param x - смещение антенны по оси X, метры;
 * \param y - смещение антенны по оси Y, метры;
 * \param z - смещение антенны по оси Z, метры;
 * \param psi - угол поворота антенны по курсу, радианы;
 * \param gamma - угол поворота антенны по крену, радианы;
 * \param theta - угол поворота антенны по дифференту, радианы.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_set_position              (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                gdouble                    x,
                                                                                gdouble                    y,
                                                                                gdouble                    z,
                                                                                gdouble                    psi,
                                                                                gdouble                    gamma,
                                                                                gdouble                    theta);

/**
 *
 * Функция возвращает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCore \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param x - указатель на переменную для смещения антенны по оси X или NULL;
 * \param y - указатель на переменную для смещения антенны по оси Y или NULL;
 * \param z - указатель на переменную для смещения антенны по оси Z или NULL;
 * \param psi - указатель на переменную для угла поворота антенны по курсу или NULL;
 * \param gamma - указатель на переменную для угла поворота антенны по крену или NULL;
 * \param theta - указатель на переменную для угла поворота антенны по дифференту или NULL.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_get_position              (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                gdouble                   *x,
                                                                                gdouble                   *y,
                                                                                gdouble                   *z,
                                                                                gdouble                   *psi,
                                                                                gdouble                   *gamma,
                                                                                gdouble                   *theta);

/**
 *
 * Функция включает или выключает приём данных на указанном порту.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта;
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_set_enable                (HyScanSensorControl       *control,
                                                                                gint                       port_id,
                                                                                gboolean                   enable);

/**
 *
 * Функция возвращает состояние приёма данных на указанном порту.
 *
 * \param control указатель на интерфейс \link HyScanSensorControl \endlink;
 * \param port_id идентификатор порта.
 *
 * \return TRUE - если приём данных включен, иначе - FALSE.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean                       hyscan_sensor_control_get_enable                (HyScanSensorControl       *control,
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
void                           hyscan_sensor_control_free_ports                (HyScanSensorPort         **ports);

G_END_DECLS

#endif /* __HYSCAN_SENSOR_CONTROL_H__ */
