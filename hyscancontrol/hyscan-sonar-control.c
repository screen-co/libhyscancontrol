/*
 * \file hyscan-sonar-control.c
 *
 * \brief Исходный файл класса управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control-common.h"
#include "hyscan-sonar-control.h"
#include "hyscan-marshallers.h"

enum
{
  PROP_O,
  PROP_SONAR
};

enum
{
  SIGNAL_RAW_DATA,
  SIGNAL_LAST
};

typedef struct
{
  guint32                      id;                             /* Идентификатор приёмного канала. */
  HyScanSourceType             source;                         /* Тип источника данных. */
  guint                        channel;                        /* Индекс приёмного канала. */
  gdouble                      antenna_offset;                 /* Смещение антенны в блоке. */
  gint                         adc_offset;                     /* Смещение нуля АЦП. */
  gfloat                       adc_vref;                       /* Опоное напряжение АЦП. */
  HyScanDataChannelInfo       *info;                           /* Общие параметры борта. */
} HyScanSonarControlRaw;

struct _HyScanSonarControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */
  gulong                       signal_id;                      /* Идентификатор обработчика сигнала data. */

  GHashTable                  *boards;                         /* Список бортов гидролокатора. */
  GHashTable                  *raws;                           /* Список приёмных каналов гидролокатора. */
  HyScanSonarSyncType          sync_types;                     /* Доступные методы синхронизации излучения. */

  gdouble                      alive_timeout;                  /* Интервал отправки сигнала alive. */
  GThread                     *guard;                          /* Поток для периодической отправки сигнала alive. */
  gint                         shutdown;                       /* Признак завершения работы. */

  GRWLock                      lock;                           /* Блокировка. */
};

static void    hyscan_sonar_control_set_property       (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_sonar_control_object_constructed (GObject               *object);
static void    hyscan_sonar_control_object_finalize    (GObject               *object);

static gpointer hyscan_sonar_control_quard             (gpointer               data);

static void    hyscan_sonar_control_data_receiver      (HyScanSonarControl    *control,
                                                        HyScanSonarMessage    *message);

static guint   hyscan_sonar_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarControl, hyscan_sonar_control, HYSCAN_TYPE_TVG_CONTROL)

static void
hyscan_sonar_control_class_init (HyScanSonarControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_control_set_property;

  object_class->constructed = hyscan_sonar_control_object_constructed;
  object_class->finalize = hyscan_sonar_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_control_signals[SIGNAL_RAW_DATA] =
    g_signal_new ("raw-data", HYSCAN_TYPE_SONAR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__POINTER_POINTER,
                  G_TYPE_NONE,
                  2, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
hyscan_sonar_control_init (HyScanSonarControl *control)
{
  control->priv = hyscan_sonar_control_get_instance_private (control);
}

static void
hyscan_sonar_control_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->set_property (object, prop_id, value, pspec);
      priv->sonar = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_control_object_constructed (GObject *object)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *boards;

  gint64 sync_types;
  gint64 version;
  gint64 id;
  gint i, j;

  G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->constructed (object);

  g_rw_lock_init (&priv->lock);

  /* Список доступных бортов и приёмных каналов. */
  priv->boards = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->raws = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: sonar schema version mismatch");
      return;
    }

  /* Доступные методы синхронизации излучения. */
  if (hyscan_sonar_get_integer (priv->sonar, "/sync/capabilities", &sync_types))
    priv->sync_types = sync_types;

  /* Поток отправки сигнала alive. */
  if (hyscan_sonar_get_double (priv->sonar, "/info/alive-timeout", &priv->alive_timeout))
    priv->guard = g_thread_new ("sonar-control-alive", hyscan_sonar_control_quard, priv);

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (priv->sonar));

  /* Ветка схемы с описанием бортов гидролокатора - "/boards". */
  for (i = 0, boards = NULL; params->n_nodes; i++)
    {
      if (g_strcmp0 (params->nodes[i]->path, "/boards") == 0)
        {
          boards = params->nodes[i];
          break;
        }
    }

  if (boards != NULL)
    {
      /* Считываем описания бортов. */
      for (i = 0; i < boards->n_nodes; i++)
        {
          HyScanDataChannelInfo *board_info;

          gchar *param_names[5];
          GVariant *param_values[5];

          gchar **pathv;
          guint board;
          guint source;

          gdouble vertical_pattern;
          gdouble horizontal_pattern;

          gboolean status;

          /* Тип борта гидролокатора. */
          pathv = g_strsplit (boards->nodes[i]->path, "/", -1);
          board = hyscan_control_get_board_type (pathv[2]);
          g_strfreev (pathv);

          if (board == HYSCAN_BOARD_INVALID)
            continue;

          /* Тип источника "сырых" данных для борта. */
          source = hyscan_control_get_raw_source_type (board);
          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          param_names[0] = g_strdup_printf ("%s/info/antenna-pattern/vertical", boards->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/info/antenna-pattern/horizontal", boards->nodes[i]->path);
          param_names[2] = NULL;

          status = hyscan_sonar_get (priv->sonar, (const gchar **)param_names, param_values);

          if (status)
            {
              vertical_pattern = g_variant_get_double (param_values[0]);
              horizontal_pattern = g_variant_get_double (param_values[1]);

              g_variant_unref (param_values[0]);
              g_variant_unref (param_values[1]);
            }

          g_free (param_names[0]);
          g_free (param_names[1]);

          if (!status)
            continue;

          /* Параметры борта. */
          board_info = g_new0 (HyScanDataChannelInfo, 1);
          board_info->vertical_pattern = vertical_pattern;
          board_info->horizontal_pattern = horizontal_pattern;

          g_hash_table_insert (priv->boards, GINT_TO_POINTER (board), board_info);

          /* Приёмные каналы. */
          for (j = 0; TRUE; j++)
            {
              HyScanSonarControlRaw *raw;

              gchar *key_id;
              gboolean has_raw;

              gint64 id;
              gdouble antenna_offset;
              gint adc_offset;
              gdouble adc_vref;

              key_id = g_strdup_printf ("%s/sources/raw.%d/id", boards->nodes[i]->path, j);
              has_raw = hyscan_data_schema_has_key (HYSCAN_DATA_SCHEMA (priv->sonar), key_id);
              g_free (key_id);

              if (!has_raw)
                break;

              param_names[0] = g_strdup_printf ("%s/sources/raw.%d/id", boards->nodes[i]->path, j);
              param_names[1] = g_strdup_printf ("%s/sources/raw.%d/antenna/offset", boards->nodes[i]->path, j);
              param_names[2] = g_strdup_printf ("%s/sources/raw.%d/adc/offset", boards->nodes[i]->path, j);
              param_names[3] = g_strdup_printf ("%s/sources/raw.%d/adc/vref", boards->nodes[i]->path, j);
              param_names[4] = NULL;

              status = hyscan_sonar_get (priv->sonar, (const gchar **)param_names, param_values);

              if (status)
                {
                  id = g_variant_get_int64 (param_values[0]);
                  antenna_offset = g_variant_get_double (param_values[1]);
                  adc_offset = g_variant_get_int64 (param_values[2]);
                  adc_vref = g_variant_get_double (param_values[3]);

                  g_variant_unref (param_values[0]);
                  g_variant_unref (param_values[1]);
                  g_variant_unref (param_values[2]);
                  g_variant_unref (param_values[3]);
                }

              g_free (param_names[0]);
              g_free (param_names[1]);
              g_free (param_names[2]);
              g_free (param_names[3]);

              if (!status)
                continue;

              if (id <= 0 || id > G_MAXUINT32)
                continue;

              raw = g_new0 (HyScanSonarControlRaw, 1);
              raw->id = id;
              raw->source = source;
              raw->channel = j;
              raw->antenna_offset = antenna_offset;
              raw->adc_offset = adc_offset;
              raw->adc_vref = adc_vref;
              raw->info = board_info;

              g_hash_table_insert (priv->raws, GINT_TO_POINTER (raw->id), raw);
            }
        }

      /* Обработчик данных от приёмных каналов гидролокатора. */
      priv->signal_id = g_signal_connect_swapped (priv->sonar,
                                                  "data",
                                                  G_CALLBACK (hyscan_sonar_control_data_receiver),
                                                  control);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sonar_control_object_finalize (GObject *object)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  if (priv->guard != NULL)
    {
      g_atomic_int_set (&priv->shutdown, 1);
      g_thread_join (priv->guard);
    }

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->sonar, priv->signal_id);

  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->raws);
  g_hash_table_unref (priv->boards);

  g_rw_lock_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->finalize (object);
}

/* Поток отправки сигнала alive. */
static gpointer
hyscan_sonar_control_quard (gpointer data)
{
  HyScanSonarControlPrivate *priv = data;
  GTimer *timer = g_timer_new ();

  while (!g_atomic_int_get (&priv->shutdown))
    {
      if (g_timer_elapsed (timer, NULL) >= (priv->alive_timeout / 2.0))
        {
          hyscan_sonar_set_boolean (priv->sonar, "/control/alive", TRUE);

          g_timer_start (timer);
        }

      g_usleep (100000);
    }

  g_timer_destroy (timer);

  return NULL;
}

/* Функция обрабатывает сообщения с данными от приёмных каналов гидролокатора. */
static void
hyscan_sonar_control_data_receiver (HyScanSonarControl *control,
                                    HyScanSonarMessage *message)
{
  HyScanSonarControlPrivate *priv;
  HyScanSonarControlRaw *raw;

  HyScanWriteData data;
  HyScanDataChannelInfo info;

  priv = control->priv;

  /* Ищем приёмный канал. */
  raw = g_hash_table_lookup (control->priv->raws, GINT_TO_POINTER (message->id));
  if (raw == NULL)
    return;

  /* Данные. */
  data.source = raw->source;
  data.channel = raw->channel;
  data.raw = TRUE;
  data.time = message->time;
  data.size = message->size;
  data.data = message->data;

  g_rw_lock_reader_lock (&priv->lock);
  info = *raw->info;
  g_rw_lock_reader_unlock (&priv->lock);

  info.discretization_type = message->type;
  info.discretization_frequency = message->rate;

  #warning "Add raw data parameters: antenna offset, adc offset, adc vref ..."

  hyscan_write_control_sonar_add_data (HYSCAN_WRITE_CONTROL (control), &data, &info);

  g_signal_emit (control, hyscan_sonar_control_signals[SIGNAL_RAW_DATA], 0, &data, &info);
}

/* Функция возвращает маску доступных типов синхронизации излучения. */
HyScanSonarSyncType
hyscan_sonar_control_get_sync_capabilities (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), 0);

  return control->priv->sync_types;
}

/* Функция устанавливает тип синхронизации излучения. */
gboolean
hyscan_sonar_control_set_sync_type (HyScanSonarControl  *control,
                                    HyScanSonarSyncType  sync_type)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_sonar_set_enum (control->priv->sonar, "/sync/type", sync_type);
}

/* Функция включает или выключает выдачу "сырых" данных от гидролокатора. */
gboolean
hyscan_sonar_control_enable_raw_data (HyScanSonarControl *control,
                                      gboolean            enable)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_sonar_set_boolean (control->priv->sonar, "/control/raw-data", enable);
}

/* Функция устанавливает информацию о местоположении приёмных антенн. */
gboolean
hyscan_sonar_control_set_position (HyScanSonarControl *control,
                                   HyScanBoardType     board,
                                   gdouble             x,
                                   gdouble             y,
                                   gdouble             z,
                                   gdouble             psi,
                                   gdouble             gamma,
                                   gdouble             theta)
{
  HyScanSonarControlPrivate *priv;
  HyScanDataChannelInfo *position;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  position = g_hash_table_lookup (priv->boards, GINT_TO_POINTER (board));
  if (position == NULL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);

  position->x = x;
  position->y = y;
  position->z = z;
  position->psi = psi;
  position->gamma = gamma;
  position->theta = theta;

  g_rw_lock_writer_unlock (&priv->lock);

  return TRUE;
}

/* Функция задаёт время приёма эхосигнала бортом гидролокатора. */
gboolean
hyscan_sonar_control_set_receive_time (HyScanSonarControl *control,
                                       HyScanBoardType     board,
                                       gdouble             receive_time)
{
  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  param_name = g_strdup_printf ("/boards/%s/control/receive-time",
                                hyscan_control_get_board_name (board));
  status = hyscan_sonar_set_double (control->priv->sonar, param_name, receive_time);
  g_free (param_name);

  return status;
}

/* Функция переводит гидролокатор в рабочий режим и включает запись данных. */
gboolean
hyscan_sonar_control_start (HyScanSonarControl *control,
                            const gchar        *project_name,
                            const gchar        *track_name)
{
  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  if (!hyscan_write_control_start (HYSCAN_WRITE_CONTROL (control), project_name, track_name))
    return FALSE;

  param_names[0] = "/control/project-name";
  param_names[1] = "/control/track-name";
  param_names[2] = "/control/enable";
  param_names[3] = NULL;

  param_values[0] = g_variant_new_string (project_name);
  param_values[1] = g_variant_new_string (track_name);
  param_values[2] = g_variant_new_boolean (TRUE);
  param_values[3] = NULL;

  status = hyscan_sonar_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
    }

  return status;
}

/* Функция переводит гидролокатор в ждущий режим и отключает запись данных. */
gboolean
hyscan_sonar_control_stop (HyScanSonarControl *control)
{
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  status = hyscan_sonar_set_boolean (control->priv->sonar, "/control/enable", FALSE);
  hyscan_write_control_stop (HYSCAN_WRITE_CONTROL (control));

  return status;
}

/* Функция выполняет один цикл зондирования и приёма данных. */
gboolean
hyscan_sonar_control_ping (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_sonar_set_boolean (control->priv->sonar, "/sync/ping", TRUE);
}
