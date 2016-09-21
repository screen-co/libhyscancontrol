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
 * реализует управление и приём данных гидролокаторами бокового обзора с эхолотом или без.
 *
 * Создание объекта HyScanSSSEControl производится функцией #hyscan_ssse_control_new.
 *
 * Определить наличие бортов со стандартным или повышенным разрешением можно функциями
 * #hyscan_ssse_control_has_starboard, #hyscan_ssse_control_has_port,
 * #hyscan_ssse_control_has_starboard_hi и #hyscan_ssse_control_has_port_hi.
 *
 * Определить наличие эхолота можно с помощью функции #hyscan_ssse_control_has_echosounder.
 *
 * При получении обработанных акустических данных от гидролокатора, класс посылает сигнал
 * "acoustic-data", в котором передаёт их пользователю. Прототип обработчика сигнала:
 *
 * \code
 *
 * void    data_cb    (HyScanSSSEControl      *control,
 *                     HyScanSourceType        source,
 *                     HyScanAcousticDataInfo *info,
 *                     HyScanDataWriterData   *data,
 *                     gpointer                user_data);
 *
 * \endcode
 *
 * Где:
 *
 * - source - идентификатор источника данных;
 * - info - параметры акустических данных;
 * - data - акустические данные.
 *
 * Класс HyScanSSSEControl поддерживает работу в многопоточном режиме.
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

HYSCAN_API
GType                  hyscan_ssse_control_get_type            (void);

/**
 *
 * Функция создаёт новый объект \link HyScanSSSEControl \endlink и возвращает
 * указатель на него. Если тип гидролокатора не совпадает с \link HYSCAN_SONAR_SSSE \endlink,
 * функция возвращает NULL.
 *
 * \param sonar указатель на интерфейс \link HyScanSonar \endlink;
 * \param db указатель на интерфейс \link HyScanDB \endlink.
 *
 * \return Указатель на объект \link HyScanSSSEControl \endlink или NULL.
 *
 */
HYSCAN_API
HyScanSSSEControl     *hyscan_ssse_control_new                 (HyScanSonar           *sonar,
                                                                HyScanDB              *db);

/**
 *
 * Функция определяет наличие правого борта со стандартным разрешением -
 * \link HYSCAN_SOURCE_SIDE_SCAN_STARBOARD \endlink.
 *
 * \param control указатель на объект \link HyScanSSSEControl \endlink.
 *
 * \return TRUE - если борт присутствует, иначе - FALSE.
 *
 */
HYSCAN_API
gboolean               hyscan_ssse_control_has_starboard       (HyScanSSSEControl     *control);

/**
 *
 * Функция определяет наличие левого борта со стандартным разрешением -
 * \link HYSCAN_SOURCE_SIDE_SCAN_PORT \endlink.
 *
 * \param control указатель на объект \link HyScanSSSEControl \endlink.
 *
 * \return TRUE - если борт присутствует, иначе - FALSE.
 *
 */
HYSCAN_API
gboolean               hyscan_ssse_control_has_port            (HyScanSSSEControl     *control);

/**
 *
 * Функция определяет наличие правого борта с повышенным разрешением -
 * \link HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI \endlink.
 *
 * \param control указатель на объект \link HyScanSSSEControl \endlink.
 *
 * \return TRUE - если борт присутствует, иначе - FALSE.
 *
 */
HYSCAN_API
gboolean               hyscan_ssse_control_has_starboard_hi    (HyScanSSSEControl     *control);

/**
 *
 * Функция определяет наличие левого борта с повышенным разрешением -
 * \link HYSCAN_SOURCE_SIDE_SCAN_PORT_HI \endlink.
 *
 * \param control указатель на объект \link HyScanSSSEControl \endlink.
 *
 * \return TRUE - если борт присутствует, иначе - FALSE.
 *
 */
HYSCAN_API
gboolean               hyscan_ssse_control_has_port_hi         (HyScanSSSEControl     *control);

/**
 *
 * Функция определяет наличие эхолота - \link HYSCAN_SOURCE_ECHOSOUNDER \endlink.
 *
 * \param control указатель на объект \link HyScanSSSEControl \endlink.
 *
 * \return TRUE - если эхолот присутствует, иначе - FALSE.
 *
 */
HYSCAN_API
gboolean               hyscan_ssse_control_has_echosounder     (HyScanSSSEControl     *control);

G_END_DECLS

#endif /* __HYSCAN_SSSE_CONTROL_H__ */
