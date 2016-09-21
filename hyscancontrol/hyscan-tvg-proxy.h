/**
 * \file hyscan-tvg-proxy.h
 *
 * \brief Заголовочный файл класса прокси сервера управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanTVGProxy HyScanTVGProxy - класс прокси сервера управления системой ВАРУ
 *
 * Класс реализует интерфейс \link HyScanSonar \endlink и предназначен для трансляции
 * команд управления системой ВАРУ в проксируемый гидролокатор и трансляции параметров
 * ВАРУ от проксируемого гидролокатора.
 *
 * Класс HyScanTVGProxy наследуется от класса \link HyScanGeneratorProxy \endlink и используется
 * как базовый для классов прокси серверов управления локаторами.
 *
 * Работа с проксируемым гидролокатором осуществляется через \link HyScanTVGControl \endlink.
 * Класс реализует два режима трансляции:
 *
 * - трансляция всех данных без обработки;
 * - трансляция только обработанных данных.
 *
 * Класс HyScanTVGProxy поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_TVG_PROXY_H__
#define __HYSCAN_TVG_PROXY_H__

#include <hyscan-generator-proxy.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TVG_PROXY             (hyscan_tvg_proxy_get_type ())
#define HYSCAN_TVG_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TVG_PROXY, HyScanTVGProxy))
#define HYSCAN_IS_TVG_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TVG_PROXY))
#define HYSCAN_TVG_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TVG_PROXY, HyScanTVGProxyClass))
#define HYSCAN_IS_TVG_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TVG_PROXY))
#define HYSCAN_TVG_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TVG_PROXY, HyScanTVGProxyClass))

typedef struct _HyScanTVGProxy HyScanTVGProxy;
typedef struct _HyScanTVGProxyPrivate HyScanTVGProxyPrivate;
typedef struct _HyScanTVGProxyClass HyScanTVGProxyClass;

struct _HyScanTVGProxy
{
  HyScanGeneratorProxy parent_instance;

  HyScanTVGProxyPrivate *priv;
};

struct _HyScanTVGProxyClass
{
  HyScanGeneratorProxyClass parent_class;
};

HYSCAN_API
GType                  hyscan_tvg_proxy_get_type         (void);

G_END_DECLS

#endif /* __HYSCAN_TVG_PROXY_H__ */
