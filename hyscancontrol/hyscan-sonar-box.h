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
 * Класс предназначен для реализации интерфеса \link HyScanParam \endlink классами,
 * осуществляющими непосредственное взаимодействие гидролокаторами.
 *
 * Создание класса осуществляется функцией #hyscan_sonar_box_new. Изначально объект создаётся
 * с пустой схемой параметров гидролокатора. Для задания схемы используется функция
 * #hyscan_sonar_box_set_schema.
 *
 * Класс используется для отправки данных через сигнал "data". Для этого используется функция
 * #hyscan_sonar_box_send.
 *
 * Перед изменением параметров гидролокатора объект посылает сигнал "set". В нём передаются
 * названия изменяемых параметров и их новые значения. Пользователь может обработать этот
 * сигнал и проверить валидность новых значений. Пользователь может зарегистрировать несколько
 * обработчиков сигнала "set". Если любой из обработчиков сигнала вернёт значение FALSE, новые
 * значения не будут установлены.
 *
 * Прототип обработчика сигнала:
 *
 * \code
 *
 * gboolean sonar_box_set_param_cb  (HyScanSonarBox        *sonar,
 *                                   const gchar *const    *names,
 *                                   GVariant             **values,
 *                                   gpointer               user_data);
 *
 * \endcode
 *
 */

#ifndef __HYSCAN_SONAR_BOX_H__
#define __HYSCAN_SONAR_BOX_H__

#include <hyscan-sonar-messages.h>

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
  GObject parent_instance;

  HyScanSonarBoxPrivate *priv;
};

struct _HyScanSonarBoxClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_sonar_box_get_type               (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSonarBox \endlink.
 *
 * \return Указатель на объект \link HyScanSonarBox \endlink.
 *
 */
HYSCAN_API
HyScanSonarBox        *hyscan_sonar_box_new                    (void);

/**
 *
 * Функция задаёт схему параметров гидролокатора.
 *
 * \param sonar указатель на объект \link HyScanSonarBox \endlink;
 * \param schema_data строка с описанием схемы в формате XML;
 * \param schema_id идентификатор загружаемой схемы.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_sonar_box_set_schema             (HyScanSonarBox        *sonar,
                                                                const gchar           *schema_data,
                                                                const gchar           *schema_id);

/**
 *
 * Функция передаёт данные, через отправку сигнала "data".
 *
 * \param sonar указатель на объект \link HyScanSonarBox \endlink;
 * \param message данные для отправки \link HyScanSonarMessage \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                   hyscan_sonar_box_send                   (HyScanSonarBox        *sonar,
                                                                HyScanSonarMessage    *message);



G_END_DECLS

#endif /* __HYSCAN_SONAR_BOX_H__ */
