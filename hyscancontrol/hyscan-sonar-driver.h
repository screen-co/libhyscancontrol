/**
 * \file hyscan-sonar-driver.h
 *
 * \brief Заголовочный файл класса загрузки драйвера гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarDriver HyScanSonarDriver - класс загрузки драйвера гидролокатора
 *
 * Класс предназначен для загрузки драйвера гидролокатора и реализует интерфейс
 * \link HyScanSonarDiscover \endlink. Загрузка драйвера производится при создании
 * объекта функцией #hyscan_sonar_driver_new. Если загрузка драйвера выполнена успешно,
 * возврашается указатель на новый объект. Иначе возвращается NULL.
 *
 * Драйвер гидролокатора должен размещаться в динамически загружаемой библиотеке
 * с именем вида: hyscan-sonar-DRIVER_NAME-drv.EXT, где
 *
 * - DRIVER_NAME - название драйвера гидролокатора;
 * - EXT - расширение, зависящее от операционной системы ("dll" - для Windows, "so" - для UNIX).
 *
 * Драйвер должен содержать экспортируемую функцию с именем "hyscan_sonar_driver".
 * При вызове, функция должна возвращать указатель на объект реализующий интерфейс
 * \link HyScanSonarDiscover \endlink.
 *
 */

#ifndef __HYSCAN_SONAR_DRIVER_H__
#define __HYSCAN_SONAR_DRIVER_H__

#include <hyscan-sonar-discover.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_DRIVER             (hyscan_sonar_driver_get_type ())
#define HYSCAN_SONAR_DRIVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_DRIVER, HyScanSonarDriver))
#define HYSCAN_IS_SONAR_DRIVER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_DRIVER))
#define HYSCAN_SONAR_DRIVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_DRIVER, HyScanSonarDriverClass))
#define HYSCAN_IS_SONAR_DRIVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_DRIVER))
#define HYSCAN_SONAR_DRIVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_DRIVER, HyScanSonarDriverClass))

typedef struct _HyScanSonarDriver HyScanSonarDriver;
typedef struct _HyScanSonarDriverPrivate HyScanSonarDriverPrivate;
typedef struct _HyScanSonarDriverClass HyScanSonarDriverClass;

struct _HyScanSonarDriver
{
  GObject parent_instance;

  HyScanSonarDriverPrivate *priv;
};

struct _HyScanSonarDriverClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_sonar_driver_get_type    (void);

/**
 *
 * Функция загружает драйвер гидролокатора из указанного каталога. Имя драйвера должно
 * удовлетворять требованияем, приведённым в разделе \link HyScanSonarDriver \endlink.
 *
 * \param path путь к каталогу с драйверами;
 * \param name имя драйвера.
 *
 * \return Указатель на объект \link HyScanSonarDriver \endlink или NULL.
 *
 */
HYSCAN_API
HyScanSonarDriver     *hyscan_sonar_driver_new         (const gchar           *path,
                                                        const gchar           *name);

G_END_DECLS

#endif /* __HYSCAN_SONAR_DRIVER_H__ */
