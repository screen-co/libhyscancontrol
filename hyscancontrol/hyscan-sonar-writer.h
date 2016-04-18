/**
 * \file hyscan-sonar-writer.h
 *
 * \brief Заголовочный файл интерфейса управления записью данных в систему хранения
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarWriter HyScanSonarWriter - интерфейс управления записью данных в систему хранения
 *
 * Интерфейс предназначен для управления записью данных от датчиков и гидролокатора. Интерфейс содержит две
 * основные функции:
 *
 * - #hyscan_sonar_writer_start - включения записи;
 * - #hyscan_sonar_writer_stop - выключения записи.
 *
 * Функцию #hyscan_sonar_writer_start можно вызывать даже если запись данных уже осуществляется.
 * В этом случае произойдёт переключение записываемого галса.
 *
 * Имеется возможность управлять параметрами записи, для этого предназначены функции:
 *
 * - #hyscan_sonar_writer_set_chunk_size - устанавливает максимальный размер файлов в галсе;
 * - #hyscan_sonar_writer_set_save_time - интервал времени, для которого сохраняются записываемые данные;
 * - #hyscan_sonar_writer_set_save_size - задаёт объём сохраняемых данных в канале.
 *
 * Значения, установленные этими функциями, применяются к текущему записываемому галсу и ко всем последующим.
 * Для того, что бы отменить действие установленных значений необходимо установить новые значения или
 * передеать параметр -1. В этом случае будет установленно значение по умолчанию.
 *
 */

#ifndef __HYSCAN_SONAR_WRITER_H__
#define __HYSCAN_SONAR_WRITER_H__

#include <glib-object.h>
#include <hyscan-control-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_WRITER            (hyscan_sonar_writer_get_type ())
#define HYSCAN_SONAR_WRITER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_WRITER, HyScanSonarWriter))
#define HYSCAN_IS_SONAR_WRITER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_WRITER))
#define HYSCAN_SONAR_WRITER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SONAR_WRITER, HyScanSonarWriterInterface))

typedef struct _HyScanSonarWriter HyScanSonarWriter;
typedef struct _HyScanSonarWriterInterface HyScanSonarWriterInterface;

struct _HyScanSonarWriterInterface
{
  GTypeInterface       g_iface;

  gboolean             (*start)                (HyScanSonarWriter         *writer,
                                                const gchar               *project_name,
                                                const gchar               *track_name);

  void                 (*stop)                 (HyScanSonarWriter         *writer);

  gboolean             (*set_chunk_size)       (HyScanSonarWriter         *writer,
                                                gint32                     chunk_size);

  gboolean             (*set_save_time)        (HyScanSonarWriter         *writer,
                                                gint64                     save_time);

  gboolean             (*set_save_size)        (HyScanSonarWriter         *writer,
                                                gint64                     save_size);
};

HYSCAN_CONTROL_EXPORT
GType          hyscan_sonar_writer_get_type            (void);

/**
 *
 * Функция включает запись данных от гидролокатора.
 *
 * \param writer указатель на интерфейс \link HyScanSonarWriter \endlink;
 * \param project_name название проекта, в который записывать данные;
 * \param track_name название галса, в который записывать данные.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean       hyscan_sonar_writer_start               (HyScanSonarWriter         *writer,
                                                        const gchar               *project_name,
                                                        const gchar               *track_name);

/**
 *
 * Функция останавливает запись данных от гидролокатора.
 *
 * \param writer указатель на интерфейс \link HyScanSonarWriter \endlink;
 *
 * \return Нет.
 *
 */
HYSCAN_CONTROL_EXPORT
void           hyscan_sonar_writer_stop                (HyScanSonarWriter         *writer);

/**
 *
 * Функция устанавливает максимальный размер файлов в галсе.
 *
 * \param writer указатель на интерфейс \link HyScanSonarWriter \endlink;
 * \param chunk_size максимальный размер файлов в байтах или -1.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean       hyscan_sonar_writer_set_chunk_size      (HyScanSonarWriter         *writer,
                                                        gint32                     chunk_size);

/**
 *
 * Функция задаёт интервал времени, для которого сохраняются записываемые данные. Если данные
 * были записаны ранее "текущего времени" - "интервал хранения" они удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param writer указатель на интерфейс \link HyScanSonarWriter \endlink;
 * \param save_time время хранения данных в микросекундах или -1.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean       hyscan_sonar_writer_set_save_time       (HyScanSonarWriter         *writer,
                                                        gint64                     save_time);

/**
 *
 * Функция задаёт объём сохраняемых данных в канале. Если объём данных превышает этот предел,
 * старые данные удаляются.
 *
 * Подробнее об этом можно прочитать в описании интерфейса \link HyScanDB \endlink.
 *
 * \param writer указатель на интерфейс \link HyScanSonarWriter \endlink;
 * \param save_size объём сохраняемых данных в байтах или -1.
 *
 * \return TRUE - если команда выполнена успешно, FALSE - в случае ошибки.
 *
 */
HYSCAN_CONTROL_EXPORT
gboolean       hyscan_sonar_writer_set_save_size       (HyScanSonarWriter         *writer,
                                                        gint64                     save_size);

G_END_DECLS

#endif /* __HYSCAN_SONAR_WRITER_H__ */
