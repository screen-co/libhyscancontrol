/**
 * \file hyscan-ssse-control.h
 *
 * \brief Заголовочный файл класса управления ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSSSEControl HyScanSSSEControl - класс управления ГБОЭ
 *
 * Класс HyScanSSSEControl наследуется от класса \link HyScanSonarControl \endlink и
 * реализует управление и приём данных гидролокаторами бокового обзора с эхолотом (или без).
 *
 * Создание объекта HyScanSSSEControl производится функцией #hyscan_ssse_control_new.
 *
 * Определить наличие эхолота можно с помощью функции #hyscan_ssse_control_has_echosounder.
 *
 */

#ifndef __HYSCAN_SSSE_CONTROL_H__
#define __HYSCAN_SSSE_CONTROL_H__

#include <hyscan-sonar-control.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SSSE_CONTROL             (hyscan_ssse_control_get_type ())
#define HYSCAN_SSSE_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SSSE_CONTROL, HyScanSSSEControl))
#define HYSCAN_IS_SSSE_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SSSE_CONTROL))
#define HYSCAN_SSSE_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SSSE_CONTROL, HyScanSSSEControlClass))
#define HYSCAN_IS_SSSE_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SSSE_CONTROL))
#define HYSCAN_SSSE_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SSSE_CONTROL, HyScanSSSEControlClass))

typedef struct _HyScanSSSEControl HyScanSSSEControl;
typedef struct _HyScanSSSEControlPrivate HyScanSSSEControlPrivate;
typedef struct _HyScanSSSEControlClass HyScanSSSEControlClass;

struct _HyScanSSSEControl
{
  HyScanSonarControl parent_instance;

  HyScanSSSEControlPrivate *priv;
};

struct _HyScanSSSEControlClass
{
  HyScanSonarControlClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_ssse_control_get_type         (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSSSEControl \endlink;
 *
 * \param sonar указатель на интерфейс \link HyScanSonar \endlink;
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return Указатель на объект \link HyScanSSSEControl \endlink.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSSSEControl     *hyscan_ssse_control_new                 (HyScanSonar           *sonar,
                                                                HyScanDB              *db);

/**
 *
 * Функция определяет наличие эхолота.
 *
 * \param control указатель на объект \link HyScanSSSEControl \endlink.
 *
 * \return TRUE - если эхолот присутствует, иначе - FALSE.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_ssse_control_has_echosounder     (HyScanSSSEControl     *control);

G_END_DECLS

#endif /* __HYSCAN_SSSE_CONTROL_H__ */
