/**
 * \file hyscan-nmea-udp.h
 *
 * \brief Заголовочный файл класса приёма NMEA данных через UDP порты
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanNmeaUDP HyScanNmeaUDP - класс приёма NMEA данных через UDP порты
 *
 * Класс предназначен для приёма NMEA данных через UDP порты. Во время приёма данных
 * производится фиксация локального времени компьютера, с помощью функции g_get_monotonic_time.
 *
 * Объект HyScanNmeaUDP создаётся с помощию функции #hyscan_nmea_udp_new. IP адрес и UDP порт для
 * приёма данных задаются с помощью функции #hyscan_nmea_udp_set_address.
 *
 * Получить состояние приёма данных можно функцией #hyscan_nmea_uart_get_status.
 *
 * Список IP адресов доступных в системе можно узнать с помощью функции #hyscan_nmea_udp_list_addresses.
 *
 * В своей работе класс использует фоновый поток, в котором осуществляет приём данных.
 * Когда блок NMEA строк готов, посылается сигнал "nmea-data", в котором передаётся время
 * приёма данных компьютером, название порта, размер и сами строки. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    data_cb    (HyScanNmeaUDP    *udp,
 *                     gint64            time,
 *                     const gchar      *name,
 *                     guint             size,
 *                     const gchar      *nmea,
 *                     gpointer          user_data);
 *
 * \endcode
 *
 */

#ifndef __HYSCAN_NMEA_UDP_H__
#define __HYSCAN_NMEA_UDP_H__

#include <hyscan-sensor-control.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NMEA_UDP             (hyscan_nmea_udp_get_type ())
#define HYSCAN_NMEA_UDP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NMEA_UDP, HyScanNmeaUDP))
#define HYSCAN_IS_NMEA_UDP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NMEA_UDP))
#define HYSCAN_NMEA_UDP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NMEA_UDP, HyScanNmeaUDPClass))
#define HYSCAN_IS_NMEA_UDP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NMEA_UDP))
#define HYSCAN_NMEA_UDP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NMEA_UDP, HyScanNmeaUDPClass))

typedef struct _HyScanNmeaUDP HyScanNmeaUDP;
typedef struct _HyScanNmeaUDPPrivate HyScanNmeaUDPPrivate;
typedef struct _HyScanNmeaUDPClass HyScanNmeaUDPClass;

struct _HyScanNmeaUDP
{
  GObject parent_instance;

  HyScanNmeaUDPPrivate *priv;
};

struct _HyScanNmeaUDPClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_nmea_udp_get_type        (void);

/**
 *
 * Функция создаёт новый объект \link HyScanNmeaUDP \endlink. При создании
 * необходимо указать название порта. Это название будет передаваться вместе
 * с данными в сигнале "nmea-data".
 *
 * \param name название порта;
 *
 * \return Указатель на объект \link HyScanNmeaUDP \endlink.
 *
 */
HYSCAN_API
HyScanNmeaUDP         *hyscan_nmea_udp_new             (const gchar           *name);

/**
 *
 * Функция устанавливает IP адрес и номер UDP порта для приёма данных.
 * Номер порта должен быть больше или равным 1024.
 *
 * \param udp указатель на объект \link HyScanNmeaUDP \endlink;
 * \param ip IP адрес;
 * \param port UDP порт.
 *
 * \return TRUE - если режим работы установлен, FALSE в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_nmea_udp_set_address     (HyScanNmeaUDP         *udp,
                                                        const gchar           *ip,
                                                        guint16                port);

/**
 *
 * Функция возвращает текущее состояние приёма данных.
 *
 * \param udp указатель на объект \link HyScanNmeaUDP \endlink.
 *
 * \return Состояние приёме данных.
 *
 */
HYSCAN_API
HyScanSensorPortStatus hyscan_nmea_udp_get_status      (HyScanNmeaUDP         *udp);

/**
 *
 * Функция возвращает список IP адресов достeпных в системе.
 *
 * Функция возвращает NULL терминированный массив строк с IP адресами.
 * Пользователь должен освободить память используемую списком функцией g_strfreev.
 *
 * \return NULL терминированный список IP адресов.
 *
 */
HYSCAN_API
gchar                **hyscan_nmea_udp_list_addresses  (void);

G_END_DECLS

#endif /* __HYSCAN_NMEA_UDP_H__ */
