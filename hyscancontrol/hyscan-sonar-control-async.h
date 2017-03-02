/**
 * \file hyscan-sonar-control-async.h
 *
 * \brief Заголовочный файл класса асинхронного управления гидролокатором.
 * \author Vladimir Maximov (vmakxs@gmail.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarControlAsync HyScanSonarControlAsync - класс асинхронного управления гидролокаторами, ВАРУ, генераторами и датчиками.
 *
 * Класс HyScanSonarControlAsync наследуется от класса \link HyScanSonarControl \endlink и
 * реализует асинхронное управление гидролокатором, системой ВАРУ, генератором и датчиками.
 *
 * При управлении гидролокатором функции, устанавливающие параметры гидролокатора и датчиков,
 * выполняются в главном потоке и могу занять продолжительное время. Вызов этих функций из GUI-приложения,
 * будет приводить к остановке главного потока на всё время выполнения функции, что приведёт к "подвисаниям"
 * приложения. Класс асинхронного управления гидролокатором призван устранить этот недостаток.
 *
 * Принцип работы асинхронных функций основан на вызове функций управления в отдельном потоке и отслеживании
 * результата выполнения в функции проверки результата, которая вызывается из Главного Цикла (см. GMainLoop).
 * Однако, такой подход приводит к необходимости использования данного класса только вместе с Главным Циклом.
 *
 * Создание объекта HyScanSonarControlAsync производится функцией #hyscan_sonar_control_async_new.
 *
 * При успешном запуске асинхронной команды, объект посылает сигнал "started".
 * Прототип обработчиков сигналов "started":
 * \code
 * void     started_cb    (HyScanSonarControlAsync  *async,
 *                         gint                      action,
 *                         gpointer                  user_data);
 *
 * \endcode
 * где:
 * - action - идентификатор операции (см. \link HyScanSonarControlAsyncAction \endlink).
 *
 * После выполнения, запущенной асинхронно, команды, объект испускает детализированный сигнал "completed".
 * Детализированные сигналы испускаются в результате выполнения соответсвующих функций:
 * - completed::sensor-set-virtual-port-param - в результате выполнения #hyscan_sonar_control_async_sensor_set_virtual_port_param;
 * - completed::sensor-set-uart-port-param - в результате выполнения #hyscan_sonar_control_async_sensor_set_uart_port_param;
 * - completed::sensor-set-udp-ip-port-param - в результате выполнения #hyscan_sonar_control_async_sensor_set_udp_ip_port_param;
 * - completed::sensor-set-position  - в результате выполнения #hyscan_sonar_control_async_sensor_set_position;
 * - completed::sensor-set-enable - в результате выполнения #hyscan_sonar_control_async_sensor_set_enable;
 * - completed::generator-set-preset - в результате выполнения #hyscan_sonar_control_async_generator_set_preset;
 * - completed::generator-set-auto - в результате выполнения #hyscan_sonar_control_async_generator_set_auto;
 * - completed::generator-set-simple - в результате выполнения #hyscan_sonar_control_async_generator_set_simple;
 * - completed::generator-set-extended - в результате выполнения #hyscan_sonar_control_async_generator_set_extended;
 * - completed::generator-set-enable - в результате выполнения #hyscan_sonar_control_async_generator_set_enable;
 * - completed::tvg-set-auto - в результате выполнения #hyscan_sonar_control_async_tvg_set_auto;
 * - completed::tvg-set-constant - в результате выполнения #hyscan_sonar_control_async_tvg_set_constant;
 * - completed::tvg-set-linear_db - в результате выполнения #hyscan_sonar_control_async_tvg_set_linear_db;
 * - completed::tvg-set-logarithmic - в результате выполнения #hyscan_sonar_control_async_tvg_set_logarithmic;
 * - completed::tvg-set-enable - в результате выполнения #hyscan_sonar_control_async_tvg_set_enable;
 * - completed::sonar-set-sync-type - в результате выполнения #hyscan_sonar_control_async_sonar_set_sync_type;
 * - completed::sonar-set-position - в результате выполнения #hyscan_sonar_control_async_sonar_set_position;
 * - completed::sonar-set-receive-time - в результате выполнения #hyscan_sonar_control_async_sonar_set_receive_time;
 * - completed::sonar-start - в результате выполнения #hyscan_sonar_control_async_sonar_start;
 * - completed::sonar-stop - в результате выполнения #hyscan_sonar_control_async_sonar_stop;
 * - completed::sonar-ping - в результате выполнения #hyscan_sonar_control_async_sonar_ping;
 *
 * Прототип обработчиков сигналов "completed":
 * \code
 * void     completed_cb    (HyScanSonarControlAsync  *async,
 *                           gint                      action,
 *                           gboolean                  result,
 *                           gpointer                  user_data);
 *
 * \endcode
 * где:
 * - action - идентификатор операции (см. \link HyScanSonarControlAsyncAction \endlink).
 * - result - результат операции.
 */

#ifndef __HYSCAN_SONAR_CONTROL_ASYNC_H__
#define __HYSCAN_SONAR_CONTROL_ASYNC_H__

#include "hyscan-sonar-control.h"

G_BEGIN_DECLS

/** \brief Типы асинохронных операций. */
typedef enum
{
  HYSCAN_SONAR_CONTROL_ASYNC_EMPTY,                          /**< Пустой запрос. */
  HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_VIRTUAL_PORT_PARAM,  /**< Установка параметров виртуального порта. */
  HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UART_PORT_PARAM,     /**< Установка параметров UART порта. */
  HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UDP_IP_PORT_PARAM,   /**< Установка параметров UDP/IP порта. */
  HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_POSITION,            /**< Установка местоположения датчика. */
  HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_ENABLE,               /**< Включение/выключение датчика. */
  HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_PRESET,           /**< Установка работы генератора по преднастройкам. */
  HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_AUTO,             /**< Установка автоматического режима генератора. */
  HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_SIMPLE,           /**< Установка упрощённого режима генератора. */
  HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_EXTENDED,         /**< Установка расширенного режима генератора. */
  HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_ENABLE,           /**< Включение/выключение генератора. */
  HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_AUTO,                   /**< Установка автоматического режима системы ВАРУ. */
  HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_CONSTANT,               /**< Установка постоянного уровения усиления системы ВАРУ. */
  HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LINEAR_DB,              /**< Установка работы системы ВАРУ по линейного закону. */
  HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LOGARITHMIC,            /**< Установка работы системы ВАРУ по логирифмическому закону. */
  HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_ENABLE,                 /**< Включение/выключение системы ВАРУ. */
  HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_SYNC_TYPE,            /**< Установка режима синхронизации. */
  HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_POSITION,             /**< Установка местоположения антенн ГЛ. */
  HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_RECEIVE_TIME,         /**< Установка времени приёма. */
  HYSCAN_SONAR_CONTROL_ASYNC_SONAR_START,                    /**< Запуск гидролокатора. */
  HYSCAN_SONAR_CONTROL_ASYNC_SONAR_STOP,                     /**< Останов гидролокатора. */
  HYSCAN_SONAR_CONTROL_ASYNC_SONAR_PING,                     /**< Выполнение одиночного зондирования. */
} HyScanSonarControlAsyncAction;

#define HYSCAN_TYPE_SONAR_CONTROL_ASYNC             (hyscan_sonar_control_async_get_type ())
#define HYSCAN_SONAR_CONTROL_ASYNC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_CONTROL_ASYNC, HyScanSonarControlAsync))
#define HYSCAN_IS_SONAR_CONTROL_ASYNC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_CONTROL_ASYNC))
#define HYSCAN_SONAR_CONTROL_ASYNC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_CONTROL_ASYNC, HyScanSonarControlAsyncClass))
#define HYSCAN_IS_SONAR_CONTROL_ASYNC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_CONTROL_ASYNC))
#define HYSCAN_SONAR_CONTROL_ASYNC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_CONTROL_ASYNC, HyScanSonarControlAsyncClass))

typedef struct _HyScanSonarControlAsync HyScanSonarControlAsync;
typedef struct _HyScanSonarControlAsyncPrivate HyScanSonarControlAsyncPrivate;
typedef struct _HyScanSonarControlAsyncClass HyScanSonarControlAsyncClass;

struct _HyScanSonarControlAsync
{
  HyScanSonarControl                 parent_instance;
  HyScanSonarControlAsyncPrivate    *priv;
};

struct _HyScanSonarControlAsyncClass
{
  HyScanSonarControlClass   parent_class;
};

GType                        hyscan_sonar_control_async_get_type                            (void);

/**
 * Функция создаёт новый объект \link HyScanSonarControlAsync \endlink и возвращает
 * указатель на него. Если тип гидролокатора не поддерживается функция возвращает NULL.
 *
 * \param sonar указатель на интерфейс \link HyScanParam \endlink;
 * \param n_uart_ports число локальных UART портов;
 * \param n_udp_ports число локальных UDP/IP портов;
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return Указатель на объект \link HyScanSonarControlAsync \endlink или NULL.
 */
HYSCAN_API
HyScanSonarControlAsync     *hyscan_sonar_control_async_new                                 (HyScanParam                *sonar,
                                                                                             guint                       n_uart_ports,
                                                                                             guint                       n_udp_ports,
                                                                                             HyScanDB                   *db);


/**
 * Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sensor_set_virtual_port_param       (HyScanSonarControlAsync    *async,
                                                                                             const gchar                *name,
                                                                                             guint                       channel,
                                                                                             gint64                      time_offset);

/**
 * Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART.
 *
 * В эту функцию передаются идентификаторы устойства UART и режима его работы
 * из списка допустимых значений (см. #hyscan_sensor_control_list_uart_devices и
 * #hyscan_sensor_control_list_uart_modes).
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param uart_device идентификатор устройства UART;
 * \param uart_mode идентификатор режима работы устройства UART.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sensor_set_uart_port_param          (HyScanSonarControlAsync    *async,
                                                                                             const gchar                *name,
                                                                                             guint                       channel,
                                                                                             gint64                      time_offset,
                                                                                             HyScanSensorProtocolType    protocol,
                                                                                             guint                       uart_device,
                                                                                             guint                       uart_mode);

/**
 * Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP.
 *
 * В эту функцию передаётся идентфикатор IP адреса из списка допустимых значений
 * (см. #hyscan_sensor_control_list_ip_addresses).
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param name название порта;
 * \param channel номер канала;
 * \param time_offset коррекция времени приёма данных, мкс;
 * \param protocol протокол обмена данными с датчиком;
 * \param ip_address идентификатор IP адреса, по которому принимать данные;
 * \param udp_port номер UDP порта, по которому принимать данные.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sensor_set_udp_ip_port_param        (HyScanSonarControlAsync    *async,
                                                                                             const gchar                *name,
                                                                                             guint                       channel,
                                                                                             gint64                      time_offset,
                                                                                             HyScanSensorProtocolType    protocol,
                                                                                             guint                       ip_address,
                                                                                             guint16                     udp_port);

/**
 * Функция асинхронно устанавливает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param name название порта;
 * \param position параметры местоположения приёмной антенны.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sensor_set_position                 (HyScanSonarControlAsync    *async,
                                                                                             const gchar                *name,
                                                                                             HyScanAntennaPosition      *position);

/**
 * Функция асинхронно включает или выключает приём данных на указанном порту.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param name название порта;
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sensor_set_enable                   (HyScanSonarControlAsync    *async,
                                                                                             const gchar                *name,
                                                                                             gboolean                    enable);


/**
 * Функция асинхронно включает преднастроенный режим работы генератора.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param preset идентификатор преднастройки.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_generator_set_preset                (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             guint                       preset);

/**
 * Функция асинхронно включает автоматический режим работы генератора.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_generator_set_auto                  (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             HyScanGeneratorSignalType   signal);

/**
 * Функция асинхронно включает упрощённый режим работы генератора.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_generator_set_simple                (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             HyScanGeneratorSignalType   signal,
                                                                                             gdouble                     power);

/**
 * Функция асинхронно включает расширенный режим работы генератора.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param signal тип сигнала;
 * \param duration длительность сигнала, с;
 * \param power энергия сигнала, проценты.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_generator_set_extended              (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             HyScanGeneratorSignalType   signal,
                                                                                             gdouble                     duration,
                                                                                             gdouble                     power);

/**
 * Функция асинхронно включает или выключает формирование сигнала генератором.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param enable включён или выключен.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_generator_set_enable                (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gboolean                    enable);

/**
 * Функция асинхронно включает автоматический режим управления системой ВАРУ.
 *
 * Если в качестве значений параметров уровня сигнала и (или) чувствительности
 * передать отрицательное число, будут установлены значения по умолчанию.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param level целевой уровень сигнала;
 * \param sensitivity чувствительность автомата регулировки.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_tvg_set_auto                        (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gdouble                     level,
                                                                                             gdouble                     sensitivity);

/**
 * Функция асинхронно устанавливает постоянный уровень усиления системой ВАРУ.
 *
 * Ууровень усиления должен находится в пределах значений, возвращаемых
 * функцией #hyscan_tvg_control_get_gain_range.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param gain коэффициент усиления, дБ;
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_tvg_set_constant                    (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gdouble                     gain);

/**
 * Функция асинхронно устанавливает линейное увеличение усиления в дБ на 100 метров.
 *
 * Начальный уровень усиления должен находится в пределах от -10 дБ до максимального
 * значения возвращаемого функцией #hyscan_tvg_control_get_gain_range. Величина изменения
 * усиления должна находится в пределах от 0 до 100 дБ.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param step величина изменения усиления каждые 100 метров, дБ.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_tvg_set_linear_db                   (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gdouble                     gain0,
                                                                                             gdouble                     step);

/**
 * Функция асинхронно устанавливает логарифмический вид закона усиления системой ВАРУ.
 *
 * Начальный уровень усиления должен находится в пределах от -10 дБ до максимального
 * значения возвращаемого функцией #hyscan_tvg_control_get_gain_range. Значение коэффициента
 * отражения цели должно находится в пределах от 0 дБ до 100 дБ. Значение коэффициента
 * затухания должно находится в пределах от 0 дБ/м до 1 дБ/м.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param gain0 начальный уровень усиления, дБ;
 * \param beta коэффициент отражения цели, дБ;
 * \param alpha коэффициент затухания, дБ/м.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_tvg_set_logarithmic                 (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gdouble                     gain0,
                                                                                             gdouble                     beta,
                                                                                             gdouble                     alpha);

/**
 * Функция асинхронно включает или выключает систему ВАРУ.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param enable включёна или выключена.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_tvg_set_enable                      (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gboolean                    enable);


/**
 * Функция асинхронно устанавливает тип синхронизации излучения.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param sync_type тип синхронизации излучения.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sonar_set_sync_type                 (HyScanSonarControlAsync    *async,
                                                                                             HyScanSonarSyncType         sync_type);

/**
 * Функция асинхронно устанавливает информацию о местоположении приёмных антенн
 * относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param position параметры местоположения приёмной антенны.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sonar_set_position                  (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             HyScanAntennaPosition      *position);

/**
 * Функция асинхронно задаёт время приёма эхосигнала источником данных. Если время
 * установленно нулевым, приём данных этим источником отключается. Если время
 * приёма отрицательное (<= -1.0), будет использоваться автоматическое управление.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param source идентификатор источника данных;
 * \param receive_time время приёма эхосигнала, секунды.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sonar_set_receive_time              (HyScanSonarControlAsync    *async,
                                                                                             HyScanSourceType            source,
                                                                                             gdouble                     receive_time);

/**
 * Функция асинхронно переводит гидролокатор в рабочий режим и включает запись данных.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink;
 * \param track_name название галса, в который записывать данные;
 * \param track_type тип галса.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sonar_start                         (HyScanSonarControlAsync    *async,
                                                                                             const gchar                *track_name,
                                                                                             HyScanTrackType             track_type);

/**
 * Функция асинхронно переводит гидролокатор в ждущий режим и отключает запись данных.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sonar_stop                          (HyScanSonarControlAsync    *async);

/**
 *
 * Функция асинхронно выполняет один цикл зондирования и приёма данных. Для использования
 * этой функции гидролокатор должен поддерживать программное управление синхронизацией
 * излучения (#HYSCAN_SONAR_SYNC_SOFTWARE) и этот тип синхронизации должен быть включён
 * функцией #hyscan_sonar_control_set_sync_type.
 *
 * \param async указатель на класс \link HyScanSonarControlAsync \endlink.
 *
 * \return TRUE - если команда принята к выполнению, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                     hyscan_sonar_control_async_sonar_ping                          (HyScanSonarControlAsync    *async);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CONTROL_ASYNC_H__ */
