/**
 * \file hyscan-sonar-proxy.h
 *
 * \brief Заголовочный файл класса прокси сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarProxy HyScanSonarProxy - класс прокси сервера управления гидролокатором
 *
 * Класс реализует интерфейс \link HyScanParam \endlink и предназначен для трансляции
 * команд управления в проксируемый гидролокатор и трансляции "сырых" данных от
 * проксируемого гидролокатора.
 *
 * Класс HyScanSonarProxy наследуется от класса \link HyScanTVGProxy \endlink и используется
 * как базовый для классов прокси серверов управления локаторами.
 *
 * Работа с проксируемым гидролокатором осуществляется через \link HyScanSonarControl \endlink.
 * Класс реализует два режима трансляции:
 *
 * - трансляция всех данных без обработки;
 * - трансляция только обработанных данных.
 *
 * Класс HyScanSonarProxy поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_SONAR_PROXY_H__
#define __HYSCAN_SONAR_PROXY_H__

#include <hyscan-tvg-proxy.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_PROXY             (hyscan_sonar_proxy_get_type ())
#define HYSCAN_SONAR_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_PROXY, HyScanSonarProxy))
#define HYSCAN_IS_SONAR_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_PROXY))
#define HYSCAN_SONAR_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_PROXY, HyScanSonarProxyClass))
#define HYSCAN_IS_SONAR_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_PROXY))
#define HYSCAN_SONAR_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_PROXY, HyScanSonarProxyClass))

typedef struct _HyScanSonarProxy HyScanSonarProxy;
typedef struct _HyScanSonarProxyPrivate HyScanSonarProxyPrivate;
typedef struct _HyScanSonarProxyClass HyScanSonarProxyClass;

struct _HyScanSonarProxy
{
  HyScanTVGProxy parent_instance;

  HyScanSonarProxyPrivate *priv;
};

struct _HyScanSonarProxyClass
{
  HyScanTVGProxyClass parent_class;
};

HYSCAN_API
GType                  hyscan_sonar_proxy_get_type         (void);

G_END_DECLS

#endif /* __HYSCAN_SONAR_PROXY_H__ */
