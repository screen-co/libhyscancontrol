/**
 * \file hyscan-nmea-uart.h
 *
 * \brief Заголовочный файл класса приёма NMEA данных через UART порты
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanNmeaUART HyScanNmeaUART - класс приёма NMEA данных через UART порты
 *
 * Класс предназначен для приёма NMEA данных через UART порты. Во время приёма данных
 * производится автоматическая группировка NMEA строк в блоки, по времени принятия решения
 * навигационной ситсемой. Для этих целей используются строки GGA, RMC, GLL, BWC и ZDA.
 * Одновременно с этим производится фиксация локального времени компьютера, с помощью
 * функции g_get_monotonic_time, на момент прихода первого символа первой строки блока.
 *
 * Объект HyScanNmeaUART создаётся с помощию функции #hyscan_nmea_uart_new. Порт для
 * приёма данных и его параметры задаются с помощью функции #hyscan_nmea_uart_set_device.
 *
 * Если какая-либо из NMEA строк принята с ошибкой, дальнейшая обработка этой строки
 * определяется с помощью функции #hyscan_nmea_uart_skip_broken. В автоматическом режиме
 * обрабатываются только правильные NMEA строки.
 *
 * Получить состояние приёма данных можно функцией #hyscan_nmea_uart_get_status.
 *
 * Список UART портов, доступных в системе, можно получить с помощью функции
 * #hyscan_nmea_uart_list_devices. Функция #hyscan_nmea_uart_devices_free предназначена
 * для освобождения памяти, занятой списокм устройств.
 *
 * В своей работе класс использует фоновый поток, в котором осуществляет приём данных.
 * Когда блок NMEA строк готов, посылается сигнал "nmea-data", в котором передаётся время
 * приёма данных компьютером, название порта, размер и сами строки. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    data_cb    (HyScanNmeaUART   *uart,
 *                     gint64            time,
 *                     const gchar      *name,
 *                     guint             size,
 *                     const gchar      *nmea,
 *                     gpointer          user_data);
 *
 * \endcode
 *
 */

#ifndef __HYSCAN_NMEA_UART_H__
#define __HYSCAN_NMEA_UART_H__

#include <hyscan-sensor-control.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

/** \brief Режимы работы UART порта */
typedef enum
{
  HYSCAN_NMEA_UART_MODE_DISABLED,              /**< Порт отключен. */

  HYSCAN_NMEA_UART_MODE_AUTO,                  /**< Автоматический выбор режима работы. */
  HYSCAN_NMEA_UART_MODE_4800_8N1,              /**< Скорость 4800 бод, 8N1. */
  HYSCAN_NMEA_UART_MODE_9600_8N1,              /**< Скорость 9600 бод, 8N1. */
  HYSCAN_NMEA_UART_MODE_19200_8N1,             /**< Скорость 19200 бод, 8N1. */
  HYSCAN_NMEA_UART_MODE_38400_8N1,             /**< Скорость 38400 бод, 8N1. */
  HYSCAN_NMEA_UART_MODE_57600_8N1,             /**< Скорость 57600 бод, 8N1. */
  HYSCAN_NMEA_UART_MODE_115200_8N1             /**< Скорость 1152000 бод, 8N1. */
} HyScanNmeaUARTMode;

/** \brief Описание UART порта */
typedef struct
{
  gchar               *name;                   /**< Название UART порта. */
  gchar               *path;                   /**< Путь к файлу устройства порта. */
} HyScanNmeaUARTDevice;

#define HYSCAN_TYPE_NMEA_UART            (hyscan_nmea_uart_get_type ())
#define HYSCAN_NMEA_UART(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_UART, HyScanNmeaUART))
#define HYSCAN_IS_UART(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_UART))
#define HYSCAN_NMEA_UART_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_UART, HyScanNmeaUARTClass))
#define HYSCAN_IS_UART_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_UART))
#define HYSCAN_NMEA_UART_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_UART, HyScanNmeaUARTClass))

typedef struct _HyScanNmeaUART HyScanNmeaUART;
typedef struct _HyScanNmeaUARTPrivate HyScanNmeaUARTPrivate;
typedef struct _HyScanNmeaUARTClass HyScanNmeaUARTClass;

struct _HyScanNmeaUART
{
  GObject parent_instance;

  HyScanNmeaUARTPrivate *priv;
};

struct _HyScanNmeaUARTClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_nmea_uart_get_type       (void);

/**
 *
 * Функция создаёт новый объект \link HyScanNmeaUART \endlink. При создании
 * необходимо указать название порта. Это название будет передаваться вместе
 * с данными в сигнале "nmea-data".
 *
 * \param name название порта;
 *
 * \return Указатель на объект \link HyScanNmeaUART \endlink.
 *
 */
HYSCAN_API
HyScanNmeaUART        *hyscan_nmea_uart_new            (const gchar           *name);

/**
 *
 * Функция устанавливает порт и режим его работы.
 *
 * \param uart указатель на объект \link HyScanNmeaUART \endlink;
 * \param path путь к устройству;
 * \param mode режим работы.
 *
 * \return TRUE - если режим работы установлен, FALSE в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_nmea_uart_set_device     (HyScanNmeaUART        *uart,
                                                        const gchar           *path,
                                                        HyScanNmeaUARTMode     mode);

/**
 *
 * Функция определяет пропускать "битые" NMEA строки или нет.
 *
 * \param uart указатель на объект \link HyScanNmeaUART \endlink;
 * \param skip_broken TRUE - пропускать строки, FALSE - нет.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_nmea_uart_skip_broken    (HyScanNmeaUART        *uart,
                                                        gboolean               skip_broken);

/**
 *
 * Функция возвращает текущее состояние приёма данных.
 *
 * \param uart указатель на объект \link HyScanNmeaUART \endlink.
 *
 * \return Состояние приёме данных.
 *
 */
HYSCAN_API
HyScanSensorPortStatus hyscan_nmea_uart_get_status     (HyScanNmeaUART        *uart);

/**
 *
 * Функция возвращает список доступных UART устройств. Пользователь должен освободить
 * память используемую списком функцией #hyscan_nmea_uart_devices_free.
 *
 * \return NULL терминированный список UART устройств \link HyScanNmeaUARTDevice \endlink.
 *
 */
HYSCAN_API
HyScanNmeaUARTDevice **hyscan_nmea_uart_list_devices   (void);

/**
 *
 * Функция освобождает память, занятую списком доступных UART устройств.
 *
 * \param devices NULL терминированный список UART устройств \link HyScanNmeaUARTDevice \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_nmea_uart_devices_free   (HyScanNmeaUARTDevice **devices);

G_END_DECLS

#endif /* __HYSCAN_NMEA_UART_H__ */
