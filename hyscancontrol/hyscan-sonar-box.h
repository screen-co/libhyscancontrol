/**
 * \file hyscan-sonar-box.h
 *
 * \brief Заголовочный файл базового класса для реализации интерфейса HyScanSonar
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarBox HyScanSonarBox - базовый класс для реализации интерфейса HyScanSonar
 *
 * Класс предназначен для реализации интерфеса \link HyScanSonar \endlink классами,
 * осуществляющими непосредственное взаимодействие гидролокаторами. Класс HyScanSonarBox
 * наследуется от класса \link HyScanDataBox \endlink.
 *
 * Создание класса осуществляется функцией #hyscan_sonar_box_new.
 *
 * Класс используется для отправки данных через сигнал  "data",
 * интерфейса \link HyScanSonar \endlink. Для этого используется функция
 * #hyscan_sonar_box_send.
 *
 */

#ifndef __HYSCAN_SONAR_BOX_H__
#define __HYSCAN_SONAR_BOX_H__

#include <hyscan-sonar.h>
#include <hyscan-data-box.h>
#include <hyscan-control-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_BOX             (hyscan_sonar_box_get_type ())
#define HYSCAN_SONAR_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_BOX, HyScanSonarBox))
#define HYSCAN_IS_SONAR_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_BOX))
#define HYSCAN_SONAR_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_BOX, HyScanSonarBoxClass))
#define HYSCAN_IS_SONAR_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_BOX))
#define HYSCAN_SONAR_BOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_BOX, HyScanSonarBoxClass))

typedef struct _HyScanSonarBox HyScanSonarBox;
typedef struct _HyScanSonarBoxPrivate HyScanSonarBoxPrivate;
typedef struct _HyScanSonarBoxClass HyScanSonarBoxClass;

struct _HyScanSonarBox
{
  HyScanDataBox parent_instance;

  HyScanSonarBoxPrivate *priv;
};

struct _HyScanSonarBoxClass
{
  HyScanDataBoxClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_sonar_box_get_type               (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarBox \endlink.
 *
 * \param schema_data строка с описанием схемы в формате XML;
 * \param schema_id идентификатор загружаемой схемы.
 *
 * \return Указатель на объект \link HyScanSonarBox \endlink.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSonarBox        *hyscan_sonar_box_new                    (const gchar           *schema_data,
                                                                const gchar           *schema_id);

/**
 *
 * Функция передаёт данные, через отправку сигнала "data" интерфейса \link HyScanSonar \endlink.
 *
 * \param sonar указатель на объект \link HyScanSonarBox \endlink;
 * \param message данные для отправки \link HyScanSonarMessage \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void                   hyscan_sonar_box_send                   (HyScanSonarBox        *sonar,
                                                                HyScanSonarMessage    *message);



G_END_DECLS

#endif /* __HYSCAN_SONAR_BOX_H__ */
