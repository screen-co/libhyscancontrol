/**
 * \file hyscan-sonar-control.h
 *
 * \brief Заголовочный файл класса управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarControl HyScanSonarControl - класс управления гидролокатором
 *
 * Класс реализует основные команды управления гидролокатором.
 *
 * Класс HyScanSonarControl наследуется от класса \link HyScanTVGControl \endlink и
 * реализует управление и приём данных гидролокаторами.
 *
 * Создание объекта HyScanSonarControl производится функцией #hyscan_sonar_control_new.
 *
 * Список источников гидролокационных данных можно получить с помощью функции
 * #hyscan_sonar_control_source_list.
 *
 * Излучение зондирующего импульса гидролокатором управляется системой синхронизации излучения.
 * Узнать её характеристики можно при помощи функции #hyscan_sonar_control_get_sync_capabilities.
 *
 * Установить тип синхронизации излучения можно функцией # hyscan_sonar_control_set_sync_type.
 *
 * Время приёма эхосигналов гидролокатором управляется функцией #hyscan_sonar_control_set_receive_time.
 * Максимально возможное время приёма эхосигналов можно узнать с помощью функции
 * #hyscan_sonar_control_get_max_receive_time. Возможность автоматического управления временем
 * приёма можно определить с помощью функции #hyscan_sonar_control_get_auto_receive_time.
 *
 * Включить гидролокатор в рабочий режим, в соответствии с установленными параметрами, можно
 * при помощи функции #hyscan_sonar_control_start, остановить #hyscan_sonar_control_stop.
 *
 * Функция #hyscan_sonar_control_ping используется для программного управления зондированием.
 *
 * При получении "сырых" данных от гидролокатора, класс посылает сигнал "raw-data". Некоторые модели
 * гидролокаторов имеют режим, в котором, через некоторые промежутки времени, осуществляется приём
 * данных с отключенным излучением - шумов. При получении "шумов", класс посулыает сигнал "noise-data".
 * Прототип обработчиков сигналов:
 *
 * \code
 *
 * void    data_cb    (HyScanSonarControl     *control,
 *                     HyScanSourceType        source,
 *                     guint                   channel,
 *                     HyScanRawDataInfo      *info,
 *                     HyScanDataWriterData   *data,
 *                     gpointer                user_data);
 *
 * \endcode
 *
 * Где:
 *
 * - source - идентификатор источника данных;
 * - channel - индекс канала данных;
 * - info - параметры "сырых" гидролокационных данных;
 * - data - "сырые" гидролокационные данные.
 *
 * При получении обработанных акустических данных от гидролокатора, класс посылает сигнал
 * "acoustic-data", в котором передаёт их пользователю. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    data_cb    (HyScanSonarControl     *control,
 *                     HyScanSourceType        source,
 *                     HyScanAcousticDataInfo *info,
 *                     HyScanDataWriterData   *data,
 *                     gpointer                user_data);
 *
 * \endcode
 *
 * Где:
 *
 * - source - идентификатор источника данных;
 * - info - параметры акустических данных;
 * - data - акустические данные.
 *
 * Класс HyScanSonarControl поддерживает работу в многопоточном режиме.
 *
 * Класс HyScanSonarControl реализует интерфейс \link HyScanParam \endlink для доступа к
 * параметрам гидролокатора.
 *
 */

#ifndef __HYSCAN_SONAR_CONTROL_H__
#define __HYSCAN_SONAR_CONTROL_H__

#include <hyscan-tvg-control.h>

G_BEGIN_DECLS

/** \brief Типы синхронизации излучения */
typedef enum
{
  HYSCAN_SONAR_SYNC_INVALID                    = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_SYNC_INTERNAL                   = (1 << 0),     /**< Внутренняя синхронизация. */
  HYSCAN_SONAR_SYNC_EXTERNAL                   = (1 << 1),     /**< Внешняя синхронизация. */
  HYSCAN_SONAR_SYNC_SOFTWARE                   = (1 << 2)      /**< Программная синхронизация. */
} HyScanSonarSyncType;

#define HYSCAN_TYPE_SONAR_CONTROL             (hyscan_sonar_control_get_type ())
#define HYSCAN_SONAR_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_CONTROL, HyScanSonarControl))
#define HYSCAN_IS_SONAR_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_CONTROL))
#define HYSCAN_SONAR_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_CONTROL, HyScanSonarControlClass))
#define HYSCAN_IS_SONAR_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_CONTROL))
#define HYSCAN_SONAR_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_CONTROL, HyScanSonarControlClass))

typedef struct _HyScanSonarControl HyScanSonarControl;
typedef struct _HyScanSonarControlPrivate HyScanSonarControlPrivate;
typedef struct _HyScanSonarControlClass HyScanSonarControlClass;

struct _HyScanSonarControl
{
  HyScanTVGControl parent_instance;

  HyScanSonarControlPrivate *priv;
};

struct _HyScanSonarControlClass
{
  HyScanTVGControlClass parent_class;
};

HYSCAN_API
GType                  hyscan_sonar_control_get_type                   (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarControl \endlink и возвращает
 * указатель на него. Если тип гидролокатора не поддерживается функция возвращает NULL.
 *
 * \param sonar указатель на интерфейс \link HyScanParam \endlink;
 * \param n_uart_ports число локальных UART портов;
 * \param n_udp_ports число локальных UDP/IP портов;
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return Указатель на объект \link HyScanSonarControl \endlink или NULL.
 *
 */
HYSCAN_API
HyScanSonarControl    *hyscan_sonar_control_new                        (HyScanParam           *sonar,
                                                                        guint                  n_uart_ports,
                                                                        guint                  n_udp_ports,
                                                                        HyScanDB              *db);

/**
 *
 * Функция возвращает список доступных источников гидролокационных данных. Список
 * возвращается в виде массива элементов типа gint. Конец списка обозначен как
 * значение \link HYSCAN_SOURCE_INVALID \endlink.
 *
 * После использования, необходимо освободить память функцией g_free.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink.
 *
 * \return Список источников данных или NULL.
 *
 */
HYSCAN_API
HyScanSourceType      *hyscan_sonar_control_source_list                (HyScanSonarControl    *control);

/**
 *
 * Функция возвращает маску доступных типов синхронизации излучения.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink.
 *
 * \return Маска доступных типов синхронизации излучения.
 *
 */
HYSCAN_API
HyScanSonarSyncType    hyscan_sonar_control_get_sync_capabilities      (HyScanSonarControl    *control);

/**
 *
 * Функция возвращает максимально возможное время приёма эхосигнала.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink;
 * \param source идентификатор источника данных.
 *
 * \return Максимально возможное время приёма эхосигнала.
 *
 */
HYSCAN_API
gdouble                hyscan_sonar_control_get_max_receive_time       (HyScanSonarControl    *control,
                                                                        HyScanSourceType       source);

/**
 *
 * Функция возвращает возможность автоматического управления временем приёма.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink;
 * \param source идентификатор источника данных.
 *
 * \return TRUE - если имеется возможность автоматического управления, FALSE - если нет.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_get_auto_receive_time      (HyScanSonarControl    *control,
                                                                        HyScanSourceType       source);

/**
 *
 * Функция устанавливает тип синхронизации излучения.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink;
 * \param sync_type тип синхронизации излучения.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_set_sync_type              (HyScanSonarControl    *control,
                                                                        HyScanSonarSyncType    sync_type);

/**
 *
 * Функция устанавливает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink;
 * \param source идентификатор источника данных;
 * \param position параметры местоположения приёмной антенны.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_set_position               (HyScanSonarControl    *control,
                                                                        HyScanSourceType       source,
                                                                        HyScanAntennaPosition *position);

/**
 *
 * Функция задаёт время приёма эхосигнала источником данных. Если время
 * установленно нулевым, приём данных этим источником отключается. Если время
 * приёма отрицательное (<= -1.0), будет использоваться автоматическое управление.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink;
 * \param source идентификатор источника данных;
 * \param receive_time время приёма эхосигнала, секунды.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_set_receive_time           (HyScanSonarControl    *control,
                                                                        HyScanSourceType       source,
                                                                        gdouble                receive_time);

/**
 *
 * Функция переводит гидролокатор в рабочий режим и включает запись данных.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink;
 * \param track_name название галса, в который записывать данные;
 * \param track_type тип галса.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_start                      (HyScanSonarControl    *control,
                                                                        const gchar           *track_name,
                                                                        HyScanTrackType        track_type);

/**
 *
 * Функция переводит гидролокатор в ждущий режим и отключает запись данных.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_stop                       (HyScanSonarControl    *control);

/**
 *
 * Функция выполняет один цикл зондирования и приёма данных. Для использования
 * этой функции гидролокатор должен поддерживать программное управление синхронизацией
 * излучения (#HYSCAN_SONAR_SYNC_SOFTWARE) и этот тип синхронизации должен быть включён
 * функцией #hyscan_sonar_control_set_sync_type.
 *
 * \param control указатель на класс \link HyScanSonarControl \endlink.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sonar_control_ping                       (HyScanSonarControl    *control);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CONTROL_H__ */
