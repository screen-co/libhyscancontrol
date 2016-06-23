/**
 * \file hyscan-write-control.h
 *
 * \brief Заголовочный файл класса управления записью данных от гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanWriteControl HyScanWriteControl - класс управления записью данных
 *
 * Класс предназначен для управления записью данных от гидролокатора, установки параметров
 * записи и непосредственно записи данных. Класс HyScanWriteControl используется как базовый
 * для классов управления локаторами.
 *
 * Для управления записью предназначены функции:
 *
 * - #hyscan_write_control_start - включает запись данных;
 * - #hyscan_write_control_stop - останавливает запись данных.
 *
 * При вызове функции #hyscan_write_control_start создаётся новый галс, в который ничаниется запись
 * данных. Эту функцию можно вызывать если запись уже включена. В этом случае произойдёт переключение
 * записываемого галса.
 *
 * Для установки параметров записи предназначены функции:
 *
 * - #hyscan_write_control_set_chunk_size - устанавливает максимальный размер файлов в галсе;
 * - #hyscan_write_control_set_save_time - интервал времени, для которого сохраняются записываемые данные;
 * - #hyscan_write_control_set_save_size - задаёт объём сохраняемых данных в канале.
 *
 * Значения, установленные этими функциями, применяются к текущему записываемому галсу и ко всем последующим.
 * Для того, что бы отменить действие установленных значений необходимо установить новые значения или
 * передеать отрицательное число. В этом случае будет установленно значение по умолчанию.
 *
 * Для записи данных предназначены функции:
 *
 * - #hyscan_write_control_add_sensor_data - записывает данные от датчиков;
 * - #hyscan_write_control_add_acoustic_data - записывает гидролокационные данные.
 *
 * Объекты класса HyScanWriteControl допускают использование в многопоточном режиме.
 *
 */

#ifndef __HYSCAN_WRITE_CONTROL_H__
#define __HYSCAN_WRITE_CONTROL_H__

#include <hyscan-db.h>
#include <hyscan-core-types.h>
#include <hyscan-control-exports.h>

G_BEGIN_DECLS

/** \brief Данные для записи. */
typedef struct
{
  const gchar                 *name;                           /**< Название целевого канала данных. */
  gint64                       time;                           /**< Время приёма данных. */
  guint32                      size;                           /**< Размер данных. */
  gconstpointer                data;                           /**< Данные. */
} HyScanWriteData;

/** \brief Образ излучаемого сигнала для свёртки. */
typedef struct
{
  HyScanSourceType             source;                         /**< Идентификатор генератора. */
  gint64                       time;                           /**< Время начала действия сигнала. */
  guint32                      n_points;                       /**< Число точек образа. */
  HyScanComplexFloat          *points;                         /**< Образ сигнала для свёртки. */
} HyScanWriteSignal;

/** \brief Значения коэффициентов усиления системы ВАРУ. */
typedef struct
{
  const gchar                 *name;                           /**< Название целевого канала данных. */
  gint64                       time;                           /**< Время начала действия сигнала. */
  gfloat                       dtime;                          /**< Шаг времени изменения коэффициента передачи, с. */
  guint32                      n_gains;                        /**< Число коэффициентов передачи. */
  gfloat                      *gains;                          /**< Коэффициенты передачи приёмного тракта, дБ. */
} HyScanWriteTVG;

#define HYSCAN_TYPE_WRITE_CONTROL             (hyscan_write_control_get_type ())
#define HYSCAN_WRITE_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_WRITE_CONTROL, HyScanWriteControl))
#define HYSCAN_IS_WRITE_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_WRITE_CONTROL))
#define HYSCAN_WRITE_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_WRITE_CONTROL, HyScanWriteControlClass))
#define HYSCAN_IS_WRITE_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_WRITE_CONTROL))
#define HYSCAN_WRITE_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_WRITE_CONTROL, HyScanWriteControlClass))

typedef struct _HyScanWriteControl HyScanWriteControl;
typedef struct _HyScanWriteControlPrivate HyScanWriteControlPrivate;
typedef struct _HyScanWriteControlClass HyScanWriteControlClass;

struct _HyScanWriteControl
{
  GObject parent_instance;

  HyScanWriteControlPrivate *priv;
};

struct _HyScanWriteControlClass
{
  GObjectClass parent_class;

  gboolean           (*start)                                  (HyScanWriteControl            *control,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name);

  void               (*stop)                                   (HyScanWriteControl            *control);
};

HYSCAN_CONTROL_EXPORT
GType                  hyscan_write_control_get_type           (void);

/**
 *
 * Функция включает запись данных.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink;
 * \param project_name название проекта, в который записывать данные;
 * \param track_name название галса, в который записывать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_write_control_start              (HyScanWriteControl            *control,
                                                                const gchar                   *project_name,
                                                                const gchar                   *track_name);

/**
 *
 * Функция отключает запись данных.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void                   hyscan_write_control_stop               (HyScanWriteControl            *control);

/**
 *
 * Функция устанавливает максимальный размер файлов в галсе. Подробнее
 * об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink;
 * \param chunk_size максимальный размер файлов в байтах.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_write_control_set_chunk_size     (HyScanWriteControl            *control,
                                                                gint32                         chunk_size);

/**
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые данные. Если данные
 * были записаны ранее "текущего времени" - "интервал хранения" они удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink;
 * \param save_time время хранения данных в микросекундах.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_write_control_set_save_time      (HyScanWriteControl            *control,
                                                                gint64                         save_time);

/**
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём данных превышает этот предел,
 * старые данные удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink;
 * \param save_size объём сохраняемых данных в байтах.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_write_control_set_save_size      (HyScanWriteControl            *control,
                                                                gint64                         save_size);

/**
 *
 * Функция записывает данные от датчиков.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink;
 * \param data данные для записи;
 * \param info параметры канала с данными датчиков.
 *
 * \return TRUE - если данные успешно записаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_write_control_add_sensor_data    (HyScanWriteControl            *control,
                                                                HyScanWriteData               *data,
                                                                HyScanSensorChannelInfo       *info);

/**
 *
 * Функция записывает гидролокационные данные.
 *
 * \param control указатель на интерфейс \link HyScanWriteControl \endlink;
 * \param data данные для записи;
 * \param info параметры канала с акустическими данными.
 *
 * \return TRUE - если данные успешно записаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean               hyscan_write_control_add_acoustic_data  (HyScanWriteControl            *control,
                                                                HyScanWriteData               *data,
                                                                HyScanDataChannelInfo         *info);

G_END_DECLS

#endif /* __HYSCAN_WRITE_CONTROL_H__ */
