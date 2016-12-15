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
 * команд управления в проксируемый гидролокатор и трансляции данных от него.
 *
 * Создание объекта HyScanSonarControl производится функцией #hyscan_sonar_proxy_new.
 *
 * Работа с проксируемым гидролокатором осуществляется через \link HyScanSonarControl \endlink.
 * Класс реализует режимы трансляции определённые типом \link HyScanSonarProxyModeType \endlink:
 *
 * - #HYSCAN_SONAR_PROXY_MODE_ALL - при трансляции один к одному без изменений все
 * команды передаются в проксируемый гидролокатор без изменений и все данные от
 * гидролокатора транслируются клиенту.
 *
 * - #HYSCAN_SONAR_PROXY_MODE_COMPUTED - в режиме трансляции обработанных данных,
 * клиенту будут передаваться только обработанные данные от акустических источников.
 *
 * В режиме трансляции #HYSCAN_SONAR_PROXY_MODE_COMPUTED можно задать коэффициенты
 * масштабирования данных функцией #hyscan_sonar_proxy_set_scale.
 *
 * Класс HyScanSonarProxy поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_SONAR_PROXY_H__
#define __HYSCAN_SONAR_PROXY_H__

#include <hyscan-sonar-control.h>
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
GType                  hyscan_sonar_proxy_get_type             (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarProxy \endlink и возвращает
 * указатель на него. Если гидролокатор не поддерживается, функция возвращает NULL.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink;
 * \param sensor_proxy_mode режим трансляции датчиков.
 * \param sonar_proxy_mode режим трансляции гидролокационных данных и команд.
 *
 * \return Указатель на объект \link HyScanSonarProxy \endlink или NULL.
 *
 */
HYSCAN_API
HyScanSonarProxy      *hyscan_sonar_proxy_new                  (HyScanSonarControl            *control,
                                                                HyScanSonarProxyModeType       sensor_proxy_mode,
                                                                HyScanSonarProxyModeType       sonar_proxy_mode);

/**
 *
 * Функция устанавливает коэффициенты масштабирования данных для
 * режима трансляции \link HYSCAN_SONAR_PROXY_MODE_COMPUTED \endlink.
 *
 * \param proxy указатель на объект \link HyScanSonarProxy \endlink;
 * \param side_scale коэффициент масштабирования данных по наклонной дальности (от 1 до 1024);
 * \param track_scale коэффициент масштабирования данных вдоль оси движения (от 1 до 1024).
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_sonar_proxy_set_scale            (HyScanSonarProxy              *proxy,
                                                                guint                          side_scale,
                                                                guint                          track_scale);

G_END_DECLS

#endif /* __HYSCAN_SONAR_PROXY_H__ */
