/**
 * \file hyscan-ssse-proxy.h
 *
 * \brief Заголовочный файл класса прокси сервера ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSSSEProxy HyScanSSSEProxy - класс прокси сервера ГБОЭ
 *
 * Класс реализует интерфейс \link HyScanSonar \endlink и предназначен для трансляции
 * команд управления в проксируемый гидролокатор бокового обзора и трансляции данных от
 * проксируемого гидролокатора. Класс HyScanSSSEProxy наследуется от класса
 * \link HyScanSonarProxy \endlink.
 *
 * Создание объекта HyScanSSSEControl производится функцией #hyscan_ssse_proxy_new.
 *
 * Работа с проксируемым гидролокатором осуществляется через \link HyScanSSSEControl \endlink.
 * Класс, одновременно с трансляцией, позволяет записывать все данные от гидролокатора
 * в указанную базу данных. Таким образом можно организовать полную запись данных для
 * последующей обработки, а клиенту передавать прореженные данные для предварительного
 * просмотра.
 *
 * Получить указатель на объект управления проксируемым гидролокатором можно
 * с помощью функции #hyscan_ssse_proxy_get_control.
 *
 * Класс реализует режимы трансляции определённые типом \link HyScanSonarProxyMode \endlink:
 *
 * - #HYSCAN_SONAR_PROXY_FORWARD_ALL - при трансляции один к одному без изменений все
 * команды передаются в проксируемый гидролокатор без изменений и все данные от
 * гидролокатора транслируются клиенту.
 *
 * - #HYSCAN_SONAR_PROXY_FORWARD_COMPUTED - в режиме трансляции обработанных данных,
 * клиенту будут передаваться только обработанные данные от акустических источников.
 *
 */

#ifndef __HYSCAN_SSSE_PROXY_H__
#define __HYSCAN_SSSE_PROXY_H__

#include <hyscan-sonar-proxy.h>
#include <hyscan-ssse-control.h>
#include <hyscan-core-types.h>
#include <hyscan-db.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SSSE_PROXY             (hyscan_ssse_proxy_get_type ())
#define HYSCAN_SSSE_PROXY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SSSE_PROXY, HyScanSSSEProxy))
#define HYSCAN_IS_SSSE_PROXY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SSSE_PROXY))
#define HYSCAN_SSSE_PROXY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SSSE_PROXY, HyScanSSSEProxyClass))
#define HYSCAN_IS_SSSE_PROXY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SSSE_PROXY))
#define HYSCAN_SSSE_PROXY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SSSE_PROXY, HyScanSSSEProxyClass))

typedef struct _HyScanSSSEProxy HyScanSSSEProxy;
typedef struct _HyScanSSSEProxyPrivate HyScanSSSEProxyPrivate;
typedef struct _HyScanSSSEProxyClass HyScanSSSEProxyClass;

struct _HyScanSSSEProxy
{
  HyScanSonarProxy parent_instance;

  HyScanSSSEProxyPrivate *priv;
};

struct _HyScanSSSEProxyClass
{
  HyScanSonarProxyClass parent_class;
};

HYSCAN_API
GType                  hyscan_ssse_proxy_get_type              (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSSSEProxy \endlink и возвращает
 * указатель на него. Если тип гидролокатора не совпадает с \link HYSCAN_SONAR_SSSE \endlink
 * или указаны неверные параметры, функция возвращает NULL.
 *
 * Если при создании класса указан объект работы с базой данных - db, все
 * данные от гидролокатора до трансляции будут записаны в эту базу.
 *
 * \param sonar указатель на интерфейс \link HyScanSonar \endlink;
 * \param proxy_mode режим трансляции \link HyScanSonarProxyMode \endlink;
 * \param side_scale коэффициент масштабирования данных по наклонной дальности;
 * \param track_scale коэффициент масштабирования данных вдоль оси движения;
 * \param db указатель на интерфейс \link HyScanDB \endlink или NULL.
 *
 * \return Указатель на объект \link HyScanSSSEProxy \endlink или NULL.
 *
 */
HYSCAN_API
HyScanSSSEProxy       *hyscan_ssse_proxy_new                   (HyScanSonar                   *sonar,
                                                                HyScanSonarProxyModeType       proxy_mode,
                                                                guint                          side_scale,
                                                                guint                          track_scale,
                                                                HyScanDB                      *db);

/**
 *
 * Функция возвращает указатель на объект управления проксируемым ГБОЭ - \link HyScanSSSEControl \endlink.
 * Объект управления принадлежит \link HyScanSSSEProxy \endlink и действителен только во время
 * его жизни. Функция может вернуть NULL, если интерфейс управления гидролокатором
 * \link HyScanSonar \endlink не совместим со схемой ГБОЭ.
 *
 * \param proxy указатель на объект \link HyScanSSSEProxy \endlink.
 *
 * \return Указатель на объект \link HyScanSSSEControl \endlink или NULL.
 *
 */
HYSCAN_API
HyScanSSSEControl     *hyscan_ssse_proxy_get_control           (HyScanSSSEProxy               *proxy);

G_END_DECLS

#endif /* __HYSCAN_SSSE_PROXY_H__ */
