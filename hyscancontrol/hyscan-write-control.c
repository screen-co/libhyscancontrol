/*
 * \file hyscan-write-control.c
 *
 * \brief Исходный файл класса управления записью данных от гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <string.h>

#include "hyscan-control-common.h"
#include "hyscan-write-control.h"
#include <hyscan-data-channel-writer.h>

enum
{
  PROP_O,
  PROP_DB
};

typedef struct
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */
  gchar                       *name;                           /* Название канала для записи данных. */
  gint32                       id;                             /* Идентификатор канала для записи данных. */
} HyScanWriteControlSensorChannel;

typedef struct
{
  gchar                       *name;                           /* Название канала для записи данных. */
  HyScanBoardType              board;                          /* Идентификатор борта. */
  gboolean                     raw;                            /* Признак "сырых" данных. */
  HyScanDataChannelWriter     *writer;                         /* Объект записи данных. */
} HyScanWriteControlDataChannel;

struct _HyScanWriteControlPrivate
{
  HyScanDB                    *db;                             /* Интерфейс системы хранения данных. */

  gboolean                     write;                          /* Признак включения записи. */

  GHashTable                  *sensor_channels;                /* Список каналов для записи данных от датчиков. */
  GHashTable                  *data_channels;                  /* Список каналов для записи гидролокационных данных. */
  GHashTable                  *signals;                        /* Список образов сигналов. */

  gchar                       *project_name;                   /* Название проекта для записи данных. */
  gchar                       *track_name;                     /* Название галса для записи данных. */

  gint32                       chunk_size;                     /* Максимальный размер файлов в галсе. */
  gint64                       save_time;                      /* Интервал времени хранения данных. */
  gint64                       save_size;                      /* Максимальный объём данных в канале. */

  GMutex                       lock;                           /* Блокировка. */
};

static void    hyscan_write_control_set_property               (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void    hyscan_write_control_object_constructed         (GObject               *object);
static void    hyscan_write_control_object_finalize            (GObject               *object);

static void    hyscan_write_control_close_sensor_channel       (gpointer               data);
static void    hyscan_write_control_close_data_channel         (gpointer               data);
static void    hyscan_write_control_free_signal                (gpointer               data);

static void    hyscan_write_control_class_stop_int             (HyScanWriteControlPrivate *priv);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanWriteControl, hyscan_write_control, G_TYPE_OBJECT)

static void
hyscan_write_control_class_init (HyScanWriteControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_write_control_set_property;

  object_class->constructed = hyscan_write_control_object_constructed;
  object_class->finalize = hyscan_write_control_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "DB", "HyScanDB interface", HYSCAN_TYPE_DB,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_write_control_init (HyScanWriteControl *control)
{
  control->priv = hyscan_write_control_get_instance_private (control);
}

static void
hyscan_write_control_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanWriteControl *control = HYSCAN_WRITE_CONTROL (object);
  HyScanWriteControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_write_control_object_constructed (GObject *object)
{
  HyScanWriteControl *control = HYSCAN_WRITE_CONTROL (object);
  HyScanWriteControlPrivate *priv = control->priv;

  g_mutex_init (&priv->lock);

  priv->write = FALSE;

  priv->chunk_size = -1;
  priv->save_time = -1;
  priv->save_size = -1;

  /* Обязательно должен быть передан указатель на HyScanDB. */
  if (priv->db == NULL)
    return;

  /* Список каналов для записи данных от датчиков. */
  priv->sensor_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                 NULL, hyscan_write_control_close_sensor_channel);

  /* Список каналов для записи гидролокационных данных. */
  priv->data_channels = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               NULL, hyscan_write_control_close_data_channel);

  /* Список образов сигналов. */
  priv->signals = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_write_control_free_signal);
}

static void
hyscan_write_control_object_finalize (GObject *object)
{
  HyScanWriteControl *control = HYSCAN_WRITE_CONTROL (object);
  HyScanWriteControlPrivate *priv = control->priv;

  g_free (priv->project_name);
  g_free (priv->track_name);

  g_clear_pointer(&priv->sensor_channels, g_hash_table_unref);
  g_clear_pointer(&priv->data_channels, g_hash_table_unref);
  g_clear_pointer(&priv->signals, g_hash_table_unref);

  g_clear_object (&priv->db);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_write_control_parent_class)->finalize (object);
}

/* Функция закрывает канал записи данных от датчиков. */
static void
hyscan_write_control_close_sensor_channel (gpointer data)
{
  HyScanWriteControlSensorChannel *channel = data;

  hyscan_db_close (channel->db, channel->id);

  g_free (channel->name);
  g_free (channel);
}

/* Функция закрывает канал записи гидролокационных данных. */
static void
hyscan_write_control_close_data_channel (gpointer data)
{
  HyScanWriteControlDataChannel *channel = data;

  g_object_unref (channel->writer);

  g_free (channel->name);
  g_free (channel);
}

/* Функция освобождает память занятую структурой HyScanWriteSignal. */
static void
hyscan_write_control_free_signal (gpointer data)
{
  HyScanWriteSignal *signal = data;

  g_free (signal->points);
  g_free (signal);
}

/* Внутренняя функция отключения записи данных. */
static void
hyscan_write_control_class_stop_int (HyScanWriteControlPrivate *priv)
{
  if (!priv->write)
    return;

  g_hash_table_remove_all (priv->sensor_channels);
  g_hash_table_remove_all (priv->data_channels);

  priv->write = FALSE;
}

/* Функция включает запись данных. */
gboolean
hyscan_write_control_start (HyScanWriteControl *control,
                            const gchar        *project_name,
                            const gchar        *track_name)
{
  HyScanWriteControlPrivate *priv;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return TRUE;

  g_mutex_lock (&priv->lock);

  /* Останавливаем запись в текущий галс. */
  hyscan_write_control_class_stop_int (priv);

  /* Создаём галс. */
  if (!hyscan_track_create (priv->db, project_name, track_name, HYSCAN_TRACK_SURVEY))
    goto exit;

  /* Запоминаем названия проекта и галса. */
  g_clear_pointer (&priv->project_name, g_free);
  g_clear_pointer (&priv->track_name, g_free);
  priv->project_name = g_strdup (project_name);
  priv->track_name = g_strdup (track_name);

  /* Включаем запись. */
  priv->write = TRUE;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция отключает запись данных. */
void
hyscan_write_control_stop (HyScanWriteControl *control)
{
  g_return_if_fail (HYSCAN_IS_WRITE_CONTROL (control));

  if (control->priv->db == NULL)
    return;

  g_mutex_lock (&control->priv->lock);

  hyscan_write_control_class_stop_int (control->priv);
  g_hash_table_remove_all (control->priv->signals);

  g_mutex_unlock (&control->priv->lock);
}

/* Функция устанавливает максимальный размер файлов в галсе. */
gboolean
hyscan_write_control_set_chunk_size (HyScanWriteControl *control,
                                     gint32              chunk_size)
{
  HyScanWriteControlPrivate *priv;

  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->sensor_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlSensorChannel *channel = data;
      if (!hyscan_db_channel_set_chunk_size (priv->db, channel->id, chunk_size))
        goto exit;
    }

  g_hash_table_iter_init (&iter, priv->data_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlDataChannel *channel = data;
      if (!hyscan_data_channel_writer_set_chunk_size (channel->writer, chunk_size))
        goto exit;
    }

  priv->chunk_size = chunk_size;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция задаёт интервал времени, для которого сохраняются записываемые данные. */
gboolean
hyscan_write_control_set_save_time (HyScanWriteControl *control,
                                    gint64              save_time)
{
  HyScanWriteControlPrivate *priv;

  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->sensor_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlSensorChannel *channel = data;
      if (!hyscan_db_channel_set_save_time (priv->db, channel->id, save_time))
        goto exit;
    }

  g_hash_table_iter_init (&iter, priv->data_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlDataChannel *channel = data;
      if (!hyscan_data_channel_writer_set_save_time (channel->writer, save_time))
        goto exit;
    }

  priv->save_time = save_time;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция задаёт объём сохраняемых данных в канале. */
gboolean
hyscan_write_control_set_save_size (HyScanWriteControl *control,
                                    gint64              save_size)
{
  HyScanWriteControlPrivate *priv;

  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->sensor_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlSensorChannel *channel = data;
      if (!hyscan_db_channel_set_save_size (priv->db, channel->id, save_size))
        goto exit;
    }

  g_hash_table_iter_init (&iter, priv->data_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlDataChannel *channel = data;
      if (!hyscan_data_channel_writer_set_save_size (channel->writer, save_size))
        goto exit;
    }

  priv->save_size = save_size;

  status = TRUE;

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает данные от датчиков в систему хранения. */
gboolean
hyscan_write_control_sensor_add_data (HyScanWriteControl      *control,
                                      HyScanWriteData         *data,
                                      HyScanSensorChannelInfo *info)
{
  HyScanWriteControlPrivate *priv;

  HyScanWriteControlSensorChannel *channel;
  const gchar *name;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return FALSE;

  /* Название канала для записи данных. */
  name = hyscan_channel_get_name_by_types (data->source, data->raw, data->channel);
  if (name == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  if (!priv->write)
    goto exit;

  /* Ищем канал для записи данных, при необходимости содаём новый. */
  channel = g_hash_table_lookup (priv->sensor_channels, name);
  if (channel == NULL)
    {
      channel = g_new (HyScanWriteControlSensorChannel, 1);

      channel->db = priv->db;
      channel->name = g_strdup (name);
      channel->id = hyscan_channel_sensor_create (priv->db,
                                                  priv->project_name,
                                                  priv->track_name,
                                                  name, info);

      if (priv->chunk_size > 0)
        hyscan_db_channel_set_chunk_size (priv->db, channel->id, priv->chunk_size);
      if (priv->save_time > 0)
        hyscan_db_channel_set_save_time (priv->db, channel->id, priv->save_time);
      if (priv->save_size > 0)
        hyscan_db_channel_set_save_size (priv->db, channel->id, priv->save_size);

      g_hash_table_insert (priv->sensor_channels, channel->name, channel);
    }

  if (channel->id < 0)
    goto exit;

  /* Записываем данные. */
  status =  hyscan_db_channel_add_data (priv->db, channel->id, data->time, data->data, data->size, NULL);

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает гидролокационные данные. */
gboolean
hyscan_write_control_sonar_add_data (HyScanWriteControl    *control,
                                     HyScanWriteData       *data,
                                     HyScanDataChannelInfo *info)
{
  HyScanWriteControlPrivate *priv;

  HyScanWriteControlDataChannel *channel;
  const gchar *name;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return FALSE;

  /* Название канала для записи данных. */
  name = hyscan_channel_get_name_by_types (data->source, data->raw, data->channel);
  if (name == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  if (!priv->write)
    goto exit;

  /* Ищем канал для записи данных, при необходимости содаём новый. */
  channel = g_hash_table_lookup (priv->data_channels, name);
  if (channel == NULL)
    {
      HyScanWriteSignal *signal;

      channel = g_new (HyScanWriteControlDataChannel, 1);

      channel->name = g_strdup (name);
      channel->board = hyscan_control_get_board_type_by_source (data->source);
      channel->raw = data->raw;
      channel->writer = hyscan_data_channel_writer_new (priv->db,
                                                        priv->project_name,
                                                        priv->track_name,
                                                        name, info);

      if (priv->chunk_size > 0)
        hyscan_data_channel_writer_set_chunk_size (channel->writer, priv->chunk_size);
      if (priv->save_time > 0)
        hyscan_data_channel_writer_set_save_time (channel->writer, priv->save_time);
      if (priv->save_size > 0)
        hyscan_data_channel_writer_set_save_size (channel->writer, priv->save_size);

      g_hash_table_insert (priv->data_channels, channel->name, channel);

      if (channel->raw)
        {
          signal = g_hash_table_lookup (priv->signals, GINT_TO_POINTER (channel->board));

          if (signal != NULL && signal-> points != NULL)
            hyscan_data_channel_writer_add_signal_image (channel->writer,
                                                         signal->time,
                                                         signal->points,
                                                         signal->n_points);
        }
    }

  /* Записываем данные. */
  status =  hyscan_data_channel_writer_add_data (channel->writer, data->time, data->data, data->size);

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция записывает образ сигнала для свёртки. */
gboolean
hyscan_write_control_sonar_add_signal (HyScanWriteControl *control,
                                       HyScanWriteSignal  *signal)
{
  HyScanWriteControlPrivate *priv;

  HyScanWriteSignal *cur_signal;
  GHashTableIter iter;
  gpointer data;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_WRITE_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->db == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Текущий образ сигнала. */
  cur_signal = g_hash_table_lookup (priv->signals, GINT_TO_POINTER (signal->board));
  if (cur_signal == NULL)
    {
      cur_signal = g_new0 (HyScanWriteSignal, 1);
      cur_signal->board = signal->board;
    }

  /* Сохраняем новый образ сигнала. */
  cur_signal->time = signal->time;
  cur_signal->n_points = signal->n_points;
  g_clear_pointer (&cur_signal->points, g_free);

  if (signal->n_points > 0)
    {
      cur_signal->points = g_new (HyScanComplexFloat, signal->n_points);
      memcpy (cur_signal->points, signal->points, signal->n_points * sizeof (HyScanComplexFloat));
    }

  g_hash_table_iter_init (&iter, priv->data_channels);
  while (g_hash_table_iter_next (&iter, NULL, &data))
    {
      HyScanWriteControlDataChannel *channel = data;

      if (channel->board != cur_signal->board || !channel->raw)
        continue;

      if (cur_signal-> points != NULL)
        hyscan_data_channel_writer_add_signal_image (channel->writer,
                                                     cur_signal->time,
                                                     cur_signal->points,
                                                     cur_signal->n_points);
    }

  status = TRUE;

  g_mutex_unlock (&priv->lock);

  return status;
}

gboolean
hyscan_write_control_sonar_add_gain (HyScanWriteControl *control,
                                     HyScanWriteGain    *gain)
{
  #warning "Add implementation"

  return FALSE;
}
