/**
 * \file hyscan-generator-proxy.h
 *
 * \brief Заголовочный файл класса прокси сервера управления генераторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanGeneratorProxy HyScanGeneratorProxy - класс прокси сервера управления генераторами
 *
 * Класс реализует интерфейс \link HyScanParam \endlink и предназначен для трансляции
 * команд управления генераторами в проксируемый гидролокатор и трансляции образов
 * сигнала от проксируемого гидролокатора.
 *
 * Класс HyScanGeneratorProxy наследуется от класса \link HyScanSensorProxy \endlink и используется
 * как базовый для классов прокси серверов управления локаторами.
 *
 * Класс HyScanGeneratorProxy поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_GENERATOR_PROXY_H__
#define __HYSCAN_GENERATOR_PROXY_H__

#include <hyscan-sensor-proxy.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GENERATOR_PROXY             (hyscan_generator_proxy_get_type ())
#define HYSCAN_GENERATOR_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GENERATOR_PROXY, HyScanGeneratorProxy))
#define HYSCAN_IS_GENERATOR_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GENERATOR_PROXY))
#define HYSCAN_GENERATOR_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GENERATOR_PROXY, HyScanGeneratorProxyClass))
#define HYSCAN_IS_GENERATOR_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GENERATOR_PROXY))
#define HYSCAN_GENERATOR_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GENERATOR_PROXY, HyScanGeneratorProxyClass))

typedef struct _HyScanGeneratorProxy HyScanGeneratorProxy;
typedef struct _HyScanGeneratorProxyPrivate HyScanGeneratorProxyPrivate;
typedef struct _HyScanGeneratorProxyClass HyScanGeneratorProxyClass;

struct _HyScanGeneratorProxy
{
  HyScanSensorProxy parent_instance;

  HyScanGeneratorProxyPrivate *priv;
};

struct _HyScanGeneratorProxyClass
{
  HyScanSensorProxyClass parent_class;
};

HYSCAN_API
GType                  hyscan_generator_proxy_get_type         (void);

G_END_DECLS

#endif /* __HYSCAN_GENERATOR_PROXY_H__ */
