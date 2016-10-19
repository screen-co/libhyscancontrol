/**
 * \file hyscan-ssse-control-server.h
 *
 * \brief Заголовочный файл класса сервера управления ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSSSEControlServer HyScanSSSEControlServer - класс сервера управления ГБОЭ
 *
 * Класс предназначен для серверной реализации управления ГБОЭ, через интерфейс
 * \link HyScanSonar \endlink. Создание класса осуществляется функцией
 * #hyscan_ssse_control_server_new.
 *
 * Класс содержит функцию отправки акустических данных от ГБОЭ -
 * #hyscan_ssse_control_server_send_acoustic_data.
 *
 */

#ifndef __HYSCAN_SSSE_CONTROL_SERVER_H__
#define __HYSCAN_SSSE_CONTROL_SERVER_H__

#include <hyscan-data-writer.h>
#include <hyscan-sonar-box.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SSSE_CONTROL_SERVER             (hyscan_ssse_control_server_get_type ())
#define HYSCAN_SSSE_CONTROL_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SSSE_CONTROL_SERVER, HyScanSSSEControlServer))
#define HYSCAN_IS_SSSE_CONTROL_SERVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SSSE_CONTROL_SERVER))
#define HYSCAN_SSSE_CONTROL_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SSSE_CONTROL_SERVER, HyScanSSSEControlServerClass))
#define HYSCAN_IS_SSSE_CONTROL_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SSSE_CONTROL_SERVER))
#define HYSCAN_SSSE_CONTROL_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SSSE_CONTROL_SERVER, HyScanSSSEControlServerClass))

typedef struct _HyScanSSSEControlServer HyScanSSSEControlServer;
typedef struct _HyScanSSSEControlServerPrivate HyScanSSSEControlServerPrivate;
typedef struct _HyScanSSSEControlServerClass HyScanSSSEControlServerClass;

struct _HyScanSSSEControlServer
{
  GObject parent_instance;

  HyScanSSSEControlServerPrivate *priv;
};

struct _HyScanSSSEControlServerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_ssse_control_server_get_type             (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSSSEControlServer \endlink.
 * Функция не создаёт дополнительной ссылки на бъект с параметрами гидролокатора,
 * этот объект должен существовать всё время работы сервера.
 *
 * \param sonar указатель на базовый класс \link HyScanSonarBox \endlink.
 *
 * \return Указатель на объект \link HyScanSSSEControlServer \endlink.
 *
 */
HYSCAN_API
HyScanSSSEControlServer       *hyscan_ssse_control_server_new                  (HyScanSonarBox              *sonar);

/**
 *
 * Функция передаёт акустические данные от гидролокатора, через отправку сигнала
 * "data" интерфейса \link HyScanSonar \endlink.
 *
 * \param server указатель на интерфейс \link HyScanTVGControlServer \endlink;
 * \param source идентификатор источника данных;
 * \param type тип данных, \link HyScanDataType \endlink;
 * \param rate частота дискретизации данных, Гц;
 * \param data данные от гидролокатора.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_ssse_control_server_send_acoustic_data   (HyScanSSSEControlServer     *server,
                                                                                HyScanSourceType             source,
                                                                                HyScanDataType               type,
                                                                                gdouble                      rate,
                                                                                HyScanDataWriterData        *data);

G_END_DECLS

#endif /* __HYSCAN_SSSE_CONTROL_SERVER_H__ */
