/**
 * \file hyscan-sonar-control.h
 *
 * \brief Заголовочный файл класса управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarControl HyScanSonarControl - класс управления гидролокатором
 *
 * Класс реализует основные команды управления гидролокатором.
 *
 * Класс HyScanSonarControl наследуется от класса \link HyScanTVGControl \endlink и используется
 * как базовый для классов управления локаторами.
 *
 * Излучение зондирующего импульса гидролокатором управляется системой синхронизации излучения.
 * Узнать её характеристики можно при помощи функции #hyscan_sonar_control_get_sync_capabilities.
 *
 * Установить тип синхронизации излучения можно функцией # hyscan_sonar_control_set_sync_type.
 *
 * Некоторые модели гидролокаторов могут выдавать данные в "сыром", т.е. не обработанном виде.
 * Включить или выключить приём и запись таких данных можно функцией #hyscan_sonar_control_enable_raw_data.
 *
 * Время приёма эхосигналов гидролокатором управляется функцией #hyscan_sonar_control_set_receive_time.
 *
 * Включить гидролокатор в рабочий режим, в соответствии с установленными параметрами, можно
 * при помощи функции #hyscan_sonar_control_start, остановить #hyscan_sonar_control_stop.
 *
 * Функция #hyscan_sonar_control_ping используется для программного управления зондированием.
 *
 * Класс HyScanSonarControl поддерживает работу в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_SONAR_CONTROL_H__
#define __HYSCAN_SONAR_CONTROL_H__

#include <hyscan-tvg-control.h>

G_BEGIN_DECLS

/** \brief Типы синхронизации излучения */
typedef enum {
  HYSCAN_SONAR_SYNC_INVALID                    = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_SYNC_INTERNAL                   = (1 << 0),     /**< Внутренняя синхронизация. */
  HYSCAN_SONAR_SYNC_EXTERNAL                   = (1 << 1),     /**< Внешняя синхронизация. */
  HYSCAN_SONAR_SYNC_SOFTWARE                   = (1 << 2)      /**< Программная синхронизация. */
} HyScanSonarSyncType;

#define HYSCAN_TYPE_SONAR_CONTROL             (hyscan_sonar_control_get_type ())
#define HYSCAN_SONAR_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_CONTROL, HyScanSonarControl))
#define HYSCAN_IS_SONAR_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_CONTROL))
#define HYSCAN_SONAR_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_CONTROL, HyScanSonarControlClass))
#define HYSCAN_IS_SONAR_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_CONTROL))
#define HYSCAN_SONAR_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_CONTROL, HyScanSonarControlClass))

typedef struct _HyScanSonarControl HyScanSonarControl;
typedef struct _HyScanSonarControlPrivate HyScanSonarControlPrivate;
typedef struct _HyScanSonarControlClass HyScanSonarControlClass;

struct _HyScanSonarControl
{
  HyScanTVGControl parent_instance;

  HyScanSonarControlPrivate *priv;
};

struct _HyScanSonarControlClass
{
  HyScanTVGControlClass parent_class;
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_sonar_control_get_type                   (void);

/**
 *
 * Функция возвращает маску доступных типов синхронизации излучения.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink.
 *
 * \return Маска доступных типов синхронизации излучения.
 *
 */
HYSCAN_CONTROL_EXPORT
HyScanSonarSyncType    hyscan_sonar_control_get_sync_capabilities      (HyScanSonarControl    *control);

/**
 *
 * Функция устанавливает тип синхронизации излучения.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink;
 * \param sync_type тип синхронизации излучения.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_set_sync_type              (HyScanSonarControl    *control,
                                                                        HyScanSonarSyncType    sync_type);

/**
 *
 * Функция включает или выключает выдачу "сырых" данных гидролокатором.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink;
 * \param enable включёно или выключено.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_enable_raw_data            (HyScanSonarControl    *control,
                                                                        gboolean               enable);

/**
 *
 * Функция устанавливает информацию о местоположении приёмных антенн относительно центра масс судна.
 * Подробное описание параметров приводится в \link HyScanCoreTypes \endlink.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink;
 * \param board идентификатор борта гидролокатора;
 * \param x смещение антенны по оси X, метры;
 * \param y смещение антенны по оси Y, метры;
 * \param z смещение антенны по оси Z, метры;
 * \param psi угол поворота антенны по курсу, радианы;
 * \param gamma угол поворота антенны по крену, радианы;
 * \param theta угол поворота антенны по дифференту, радианы.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_set_position               (HyScanSonarControl    *control,
                                                                        HyScanBoardType        board,
                                                                        gdouble                x,
                                                                        gdouble                y,
                                                                        gdouble                z,
                                                                        gdouble                psi,
                                                                        gdouble                gamma,
                                                                        gdouble                theta);

/**
 *
 * Функция задаёт время приёма эхосигнала бортом гидролокатора.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink;
 * \param board идентификатор борта гидролокатора;
 * \param receive_time время приёма эхосигнала, секунды.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_set_receive_time           (HyScanSonarControl    *control,
                                                                        HyScanBoardType        board,
                                                                        gdouble                receive_time);

/**
 *
 * Функция переводит гидролокатор в рабочий режим и включает запись данных.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink;
 * \param project_name название проекта, в который записывать данные;
 * \param track_name название галса, в который записывать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_start                      (HyScanSonarControl    *control,
                                                                        const gchar           *project_name,
                                                                        const gchar           *track_name);

/**
 *
 * Функция переводит гидролокатор в ждущий режим и отключает запись данных.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_stop                       (HyScanSonarControl    *control);

/**
 *
 * Функция выполняет один цикл зондирования и приёма данных. Для использования
 * этой функции гидролокатор должен поддерживать программное управление синхронизацией
 * излучения (#HYSCAN_SONAR_SYNC_SOFTWARE) и этот тип синхронизации должен быть включён
 * функцией #hyscan_sonar_control_set_sync_type.
 *
 * \param control указатель на интерфейс \link HyScanSonarControl \endlink.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_sonar_control_ping                       (HyScanSonarControl    *control);

G_END_DECLS

#endif /* __HYSCAN_SONAR_CONTROL_H__ */
