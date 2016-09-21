/**
 * \file hyscan-sensor-proxy.h
 *
 * \brief Заголовочный файл класса прокси сервера управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSensorProxy HyScanSensorProxy - класс прокси сервера управления датчиками местоположения и ориентации
 *
 * Класс реализует интерфейс \link HyScanSonar \endlink и предназначен для трансляции
 * команд управления датчиками в проксируемый гидролокатор и трансляции данных от
 * проксируемого гидролокатора.
 *
 * Класс HyScanSensorProxy наследуется от класса \link HyScanSonarBox \endlink и используется
 * как базовый для классов прокси серверов управления локаторами.
 *
 * Работа с проксируемым гидролокатором осуществляется через \link HyScanSensorControl \endlink.
 * Класс реализует два режимы трансляции портов:
 *
 * - трансляция данных от всех портов один к одному;
 * - трансляция одного из портов в виртуальный NMEA порт.
 *
 * Выбор порта, данные которого будут передаваться в виртуальный NMEA порт,
 * осуществляется функцией #hyscan_sensor_proxy_set_source.
 *
 * Класс HyScanSensorProxy поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_SENSOR_PROXY_H__
#define __HYSCAN_SENSOR_PROXY_H__

#include <hyscan-sonar-box.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SENSOR_PROXY             (hyscan_sensor_proxy_get_type ())
#define HYSCAN_SENSOR_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SENSOR_PROXY, HyScanSensorProxy))
#define HYSCAN_IS_SENSOR_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SENSOR_PROXY))
#define HYSCAN_SENSOR_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SENSOR_PROXY, HyScanSensorProxyClass))
#define HYSCAN_IS_SENSOR_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SENSOR_PROXY))
#define HYSCAN_SENSOR_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SENSOR_PROXY, HyScanSensorProxyClass))

typedef struct _HyScanSensorProxy HyScanSensorProxy;
typedef struct _HyScanSensorProxyPrivate HyScanSensorProxyPrivate;
typedef struct _HyScanSensorProxyClass HyScanSensorProxyClass;

struct _HyScanSensorProxy
{
  HyScanSonarBox parent_instance;

  HyScanSensorProxyPrivate *priv;
};

struct _HyScanSensorProxyClass
{
  HyScanSonarBoxClass parent_class;
};

HYSCAN_API
GType                  hyscan_sensor_proxy_get_type            (void);

/**
 *
 * Функция выбирает порт, используемый как источник данных
 * для виртуального NMEA порта.
 *
 * \param proxy указатель на интерфейс \link HyScanSensorProxy \endlink;
 * \param name название  порта.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_sensor_proxy_set_source          (HyScanSensorProxy     *proxy,
                                                                const gchar           *name);

G_END_DECLS

#endif /* __HYSCAN_SENSOR_PROXY_H__ */
