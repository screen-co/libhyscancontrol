/*
 * \file hyscan-sonar-control.c
 *
 * \brief Исходный файл класса управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-control.h"
#include "hyscan-sonar-messages.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"

enum
{
  PROP_O,
  PROP_SONAR
};

enum
{
  SIGNAL_RAW_DATA,
  SIGNAL_NOISE_DATA,
  SIGNAL_ACOUSTIC_DATA,
  SIGNAL_LAST
};

typedef struct
{
  HyScanSourceType             source;                         /* Тип источника данных. */
  guint                        channel;                        /* Индекс приёмного канала. */
  HyScanRawDataInfo            info;                           /* Параметры "сырых" гидролокационных данных. */
} HyScanSonarControlChannel;

typedef struct
{
  HyScanSourceType             source;                         /* Тип источника данных. */
  HyScanAcousticDataInfo       info;                           /* Параметры акустических данных. */
} HyScanSonarControlAcoustic;

struct _HyScanSonarControlPrivate
{
  HyScanParam                 *sonar;                          /* Интерфейс управления гидролокатором. */
  HyScanDataSchema            *schema;                         /* Схема параметров гидролокатора. */

  GArray                      *sources;                        /* Список источников гидролокационных данных. */
  GHashTable                  *channels;                       /* Список приёмных каналов гидролокатора. */
  GHashTable                  *noises;                         /* Список источников шумов приёмных каналов гидролокатора. */
  GHashTable                  *acoustics;                      /* Список источников акустических данных гидролокатора. */
  HyScanSonarSyncType          sync_types;                     /* Доступные методы синхронизации излучения. */

  gdouble                      alive_timeout;                  /* Интервал отправки сигнала alive. */
  GThread                     *guard;                          /* Поток для периодической отправки сигнала alive. */
  gint                         shutdown;                       /* Признак завершения работы. */

  GMutex                       lock;                           /* Блокировка. */
};

static void            hyscan_sonar_control_interface_init     (HyScanParamInterface  *iface);
static void            hyscan_sonar_control_set_property       (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void            hyscan_sonar_control_object_constructed (GObject               *object);
static void            hyscan_sonar_control_object_finalize    (GObject               *object);

static gpointer        hyscan_sonar_control_quard              (gpointer               data);

static void            hyscan_sonar_control_raw_data_receiver  (HyScanSonarControl    *control,
                                                                HyScanSonarMessage    *message);

static void        hyscan_sonar_control_acoustic_data_receiver (HyScanSonarControl    *control,
                                                                HyScanSonarMessage    *message);

static guint           hyscan_sonar_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanSonarControl, hyscan_sonar_control, HYSCAN_TYPE_TVG_CONTROL,
                         G_ADD_PRIVATE (HyScanSonarControl)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_sonar_control_interface_init))

static void
hyscan_sonar_control_class_init (HyScanSonarControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_control_set_property;

  object_class->constructed = hyscan_sonar_control_object_constructed;
  object_class->finalize = hyscan_sonar_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_PARAM,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_control_signals[SIGNAL_RAW_DATA] =
    g_signal_new ("raw-data", HYSCAN_TYPE_SONAR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_control_marshal_VOID__INT_UINT_POINTER_POINTER,
                  G_TYPE_NONE,
                  4, G_TYPE_INT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_POINTER);

  hyscan_sonar_control_signals[SIGNAL_NOISE_DATA] =
    g_signal_new ("noise-data", HYSCAN_TYPE_SONAR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_control_marshal_VOID__INT_UINT_POINTER_POINTER,
                  G_TYPE_NONE,
                  4, G_TYPE_INT, G_TYPE_UINT, G_TYPE_POINTER, G_TYPE_POINTER);

  hyscan_sonar_control_signals[SIGNAL_ACOUSTIC_DATA] =
    g_signal_new ("acoustic-data", HYSCAN_TYPE_SONAR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_control_marshal_VOID__INT_POINTER_POINTER,
                  G_TYPE_NONE,
                  3, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER);
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
  HyScanDataSchemaNode *sources;
  gchar *info;

  gint64 sync_types;
  gint64 version;
  gint64 id;
  guint i, j;

  G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);

  /* Список источников данных. */
  priv->sources = g_array_new (TRUE, TRUE, sizeof (gint));

  /* Список приёмных каналов. */
  priv->channels = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->noises = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
  priv->acoustics = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  /* Обязательно должен быть передан указатель на интерфейс управления локатором. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_param_get_integer (priv->sonar, "/schema/id", &id))
    return;
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    return;
  if (!hyscan_param_get_integer (priv->sonar, "/schema/version", &version))
    return;
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    return;

  /* Доступные методы синхронизации излучения. */
  if (hyscan_param_get_integer (priv->sonar, "/sync/capabilities", &sync_types))
    priv->sync_types = sync_types;

  /* Поток отправки сигнала alive. */
  if (hyscan_param_get_double (priv->sonar, "/control/timeout", &priv->alive_timeout))
    priv->guard = g_thread_new ("sonar-control-alive", hyscan_sonar_control_quard, priv);

  /* Параметры гидролокатора. */
  priv->schema = hyscan_param_schema (priv->sonar);
  params = hyscan_data_schema_list_nodes (priv->schema);

  /* Ветка схемы с описанием источников данных - "/sources". */
  for (i = 0, sources = NULL; i < params->n_nodes; i++)
    {
      if (g_strcmp0 (params->nodes[i]->path, "/sources") == 0)
        {
          sources = params->nodes[i];
          break;
        }
    }

  if (sources != NULL)
    {
      HyScanSourceType source;

      /* Считываем описания источников "сырых" данных. */
      for (i = 0; i < sources->n_nodes; i++)
        {
          gchar *param_names[7];
          GVariant *param_values[7];

          gchar **pathv;
          gchar *param_id;

          gdouble antenna_vpattern;
          gdouble antenna_hpattern;
          gdouble antenna_frequency;
          gdouble antenna_bandwidth;
          gint64 acoustic_id;

          gboolean status;

          /* Тип источника данных. */
          pathv = g_strsplit (sources->nodes[i]->path, "/", -1);
          source = hyscan_control_get_source_type (pathv[2]);
          g_strfreev (pathv);

          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          param_names[0] = g_strdup_printf ("%s/antenna/pattern/vertical", sources->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/antenna/pattern/horizontal", sources->nodes[i]->path);
          param_names[2] = g_strdup_printf ("%s/antenna/frequency", sources->nodes[i]->path);
          param_names[3] = g_strdup_printf ("%s/antenna/bandwidth", sources->nodes[i]->path);
          param_names[4] = NULL;

          status = hyscan_param_get (priv->sonar, (const gchar **)param_names, param_values);

          if (status)
            {
              antenna_vpattern = g_variant_get_double (param_values[0]);
              antenna_hpattern = g_variant_get_double (param_values[1]);
              antenna_frequency = g_variant_get_double (param_values[2]);
              antenna_bandwidth = g_variant_get_double (param_values[3]);

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

          g_array_append_val (priv->sources, source);

          /* Приёмные каналы. */
          for (j = 1; TRUE; j++)
            {
              HyScanSonarControlChannel *channel;

              gboolean has_channel;
              gint64 data_id;
              gint64 noise_id;
              gdouble antenna_voffset;
              gdouble antenna_hoffset;
              gint adc_offset;
              gdouble adc_vref;

              param_id = g_strdup_printf ("%s/channels/%d/id", sources->nodes[i]->path, j);
              has_channel = hyscan_data_schema_has_key (priv->schema, param_id);
              g_free (param_id);

              if (!has_channel)
                break;

              param_names[0] = g_strdup_printf ("%s/channels/%d/id", sources->nodes[i]->path, j);
              param_names[1] = g_strdup_printf ("%s/channels/%d/noise/id", sources->nodes[i]->path, j);
              param_names[2] = g_strdup_printf ("%s/channels/%d/antenna/offset/vertical", sources->nodes[i]->path, j);
              param_names[3] = g_strdup_printf ("%s/channels/%d/antenna/offset/horizontal", sources->nodes[i]->path, j);
              param_names[4] = g_strdup_printf ("%s/channels/%d/adc/offset", sources->nodes[i]->path, j);
              param_names[5] = g_strdup_printf ("%s/channels/%d/adc/vref", sources->nodes[i]->path, j);
              param_names[6] = NULL;

              status = hyscan_param_get (priv->sonar, (const gchar **)param_names, param_values);

              if (status)
                {
                  data_id = g_variant_get_int64 (param_values[0]);
                  noise_id = g_variant_get_int64 (param_values[1]);
                  antenna_voffset = g_variant_get_double (param_values[2]);
                  antenna_hoffset = g_variant_get_double (param_values[3]);
                  adc_offset = g_variant_get_int64 (param_values[4]);
                  adc_vref = g_variant_get_double (param_values[5]);

                  g_variant_unref (param_values[0]);
                  g_variant_unref (param_values[1]);
                  g_variant_unref (param_values[2]);
                  g_variant_unref (param_values[3]);
                  g_variant_unref (param_values[4]);
                  g_variant_unref (param_values[5]);
                }

              g_free (param_names[0]);
              g_free (param_names[1]);
              g_free (param_names[2]);
              g_free (param_names[3]);
              g_free (param_names[4]);
              g_free (param_names[5]);

              if (!status)
                continue;

              if (data_id <= 0 || data_id > G_MAXINT32)
                continue;

              if (noise_id <= 0 || noise_id > G_MAXINT32)
                continue;

              channel = g_new0 (HyScanSonarControlChannel, 1);
              channel->source = source;
              channel->channel = j;
              channel->info.adc.offset = adc_offset;
              channel->info.adc.vref = adc_vref;
              channel->info.antenna.offset.vertical = antenna_voffset;
              channel->info.antenna.offset.horizontal = antenna_hoffset;
              channel->info.antenna.pattern.vertical = antenna_vpattern;
              channel->info.antenna.pattern.horizontal = antenna_hpattern;
              channel->info.antenna.frequency = antenna_frequency;
              channel->info.antenna.bandwidth = antenna_bandwidth;

              g_hash_table_insert (priv->channels, GINT_TO_POINTER (data_id), channel);
              g_hash_table_insert (priv->noises, GINT_TO_POINTER (noise_id), channel);
            }

          /* Акустические данные. */
          param_id = g_strdup_printf ("%s/acoustic/id", sources->nodes[i]->path);
          if (hyscan_param_get_integer (priv->sonar, param_id, &acoustic_id) &&
              (acoustic_id > 0) && (acoustic_id <= G_MAXINT32))
            {
              HyScanSonarControlAcoustic *acoustic;

              acoustic = g_new0 (HyScanSonarControlAcoustic, 1);
              acoustic->source = source;
              acoustic->info.antenna.pattern.vertical = antenna_vpattern;
              acoustic->info.antenna.pattern.horizontal = antenna_hpattern;

              g_hash_table_insert (priv->acoustics, GINT_TO_POINTER (acoustic_id), acoustic);
            }
          g_free (param_id);
        }

      source = HYSCAN_SOURCE_INVALID;
      g_array_append_val (priv->sources, source);

      /* Обработчик данных от гидролокатора. */
      g_signal_connect_swapped (priv->sonar, "data",
                                G_CALLBACK (hyscan_sonar_control_raw_data_receiver), control);
      g_signal_connect_swapped (priv->sonar, "data",
                                G_CALLBACK (hyscan_sonar_control_acoustic_data_receiver), control);
    }

  /* Информация о гидролокаторе. */
  info = hyscan_data_schema_get_data (priv->schema, "/info", "info");
  if (info != NULL)
    {
      hyscan_data_writer_set_sonar_info (HYSCAN_DATA_WRITER (control), info);

      g_free (info);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sonar_control_object_finalize (GObject *object)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  g_signal_handlers_disconnect_by_data (priv->sonar, control);

  if (priv->guard != NULL)
    {
      g_atomic_int_set (&priv->shutdown, 1);
      g_thread_join (priv->guard);
    }

  g_clear_object (&priv->schema);
  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->channels);
  g_hash_table_unref (priv->noises);
  g_hash_table_unref (priv->acoustics);
  g_array_free (priv->sources, TRUE);

  g_mutex_clear (&priv->lock);

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
          hyscan_param_set_boolean (priv->sonar, "/control/alive", TRUE);

          g_timer_start (timer);
        }

      g_usleep (100000);
    }

  g_timer_destroy (timer);

  return NULL;
}

/* Функция обрабатывает сообщения с "сырыми" данными от приёмных каналов гидролокатора. */
static void
hyscan_sonar_control_raw_data_receiver (HyScanSonarControl *control,
                                        HyScanSonarMessage *message)
{
  HyScanSonarControlChannel *channel;
  HyScanRawDataInfo info;
  HyScanDataWriterData data;
  gboolean noise = FALSE;

  /* Ищем приёмный канал. */
  channel = g_hash_table_lookup (control->priv->channels, GINT_TO_POINTER (message->id));
  if (channel == NULL)
    {
      channel = g_hash_table_lookup (control->priv->noises, GINT_TO_POINTER (message->id));
      if (channel == NULL)
        return;
      noise = TRUE;
    }

  /* Данные. */
  info = channel->info;
  info.data.type = message->type;
  info.data.rate = message->rate;
  data.time = message->time;
  data.size = message->size;
  data.data = message->data;

  if (!noise)
    {
      hyscan_data_writer_raw_add_data (HYSCAN_DATA_WRITER (control),
                                       channel->source, channel->channel,
                                       &info, &data);

      g_signal_emit (control, hyscan_sonar_control_signals[SIGNAL_RAW_DATA], 0,
                     channel->source, channel->channel, &info, &data);
    }
  else
    {
      hyscan_data_writer_raw_add_noise (HYSCAN_DATA_WRITER (control),
                                        channel->source, channel->channel,
                                        &info, &data);

      g_signal_emit (control, hyscan_sonar_control_signals[SIGNAL_NOISE_DATA], 0,
                     channel->source, channel->channel, &info, &data);
    }
}

/* Функция обрабатывает сообщения с обработанными акустическимим данными от гидролокатора. */
static void
hyscan_sonar_control_acoustic_data_receiver (HyScanSonarControl *control,
                                             HyScanSonarMessage *message)
{
  HyScanSonarControlAcoustic *acoustic;
  HyScanAcousticDataInfo info;
  HyScanDataWriterData data;

  /* Ищем источник акустических данных. */
  acoustic = g_hash_table_lookup (control->priv->acoustics, GINT_TO_POINTER (message->id));
  if (acoustic == NULL)
    return;

  /* Данные. */
  info = acoustic->info;
  info.data.type = message->type;
  info.data.rate = message->rate;
  data.time = message->time;
  data.size = message->size;
  data.data = message->data;
  hyscan_data_writer_acoustic_add_data (HYSCAN_DATA_WRITER (control), acoustic->source, &info, &data);

  g_signal_emit (control, hyscan_sonar_control_signals[SIGNAL_ACOUSTIC_DATA], 0,
                 acoustic->source, &info, &data);
}

/* Функция возвращает схему гидролокатора. */
static HyScanDataSchema *
hyscan_sonar_control_schema (HyScanParam *sonar)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (sonar);

  if (control->priv->sonar == NULL)
    return NULL;

  return g_object_ref (control->priv->schema);
}

/* Функция устанавливает параметры гидролокатора. */
static gboolean
hyscan_sonar_control_set (HyScanParam         *sonar,
                          const gchar *const  *names,
                          GVariant           **values)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (sonar);

  if (control->priv->sonar == NULL)
    return FALSE;

  return hyscan_param_set (control->priv->sonar, names, values);
}

/* Функция устанавливает параметры гидролокатора. */
static gboolean
hyscan_sonar_control_get (HyScanParam         *sonar,
                          const gchar *const  *names,
                          GVariant           **values)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (sonar);

  if (control->priv->sonar == NULL)
    return FALSE;

  return hyscan_param_get (control->priv->sonar, names, values);
}

/* Функция создаёт новый объект HyScanSonarControl. */
HyScanSonarControl *
hyscan_sonar_control_new (HyScanParam *sonar,
                          guint        n_uart_ports,
                          guint        n_udp_ports,
                          HyScanDB    *db)
{
  HyScanSonarControl *control;

  control = g_object_new (HYSCAN_TYPE_SONAR_CONTROL,
                          "sonar", sonar,
                          "n-uart-ports", n_uart_ports,
                          "n-udp-ports", n_udp_ports,
                          "db", db,
                          NULL);

  if (control->priv->sources->len <= 1)
    g_clear_object (&control);

  return control;
}

/* Функция возвращает список доступных источников гидролокационных данных. */
gint *
hyscan_sonar_control_source_list (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), NULL);

  if (control->priv->sources->len <= 1)
    return NULL;

  return g_memdup (control->priv->sources->data, control->priv->sources->len * sizeof (gint));
}

/* Функция возвращает маску доступных типов синхронизации излучения. */
HyScanSonarSyncType
hyscan_sonar_control_get_sync_capabilities (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), 0);

  return control->priv->sync_types;
}

/* Функция возвращает максимально возможное время приёма эхосигнала. */
gdouble
hyscan_sonar_control_get_max_receive_time (HyScanSonarControl *control,
                                           HyScanSourceType    source)
{
  gchar *param_name;
  gdouble max_receive_time;
  GVariant *value;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), -1.0);

  param_name = g_strdup_printf ("/sources/%s/control/receive-time",
                                hyscan_control_get_source_name (source));
  value = hyscan_data_schema_key_get_maximum (control->priv->schema, param_name);
  g_free (param_name);

  if (value == NULL)
    return -1.0;

  max_receive_time = g_variant_get_double (value);
  g_variant_unref (value);

  return max_receive_time;
}

/* Функция возвращает возможность автоматического управления временем приёма. */
gboolean
hyscan_sonar_control_get_auto_receive_time (HyScanSonarControl *control,
                                            HyScanSourceType    source)
{
  gchar *param_name;
  gdouble min_receive_time;
  GVariant *value;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  param_name = g_strdup_printf ("/sources/%s/control/receive-time",
                                hyscan_control_get_source_name (source));
  value = hyscan_data_schema_key_get_minimum (control->priv->schema, param_name);
  g_free (param_name);

  if (value == NULL)
    return FALSE;

  min_receive_time = g_variant_get_double (value);
  g_variant_unref (value);

  if (min_receive_time < -1.0)
    return TRUE;

  return FALSE;
}

/* Функция устанавливает тип синхронизации излучения. */
gboolean
hyscan_sonar_control_set_sync_type (HyScanSonarControl  *control,
                                    HyScanSonarSyncType  sync_type)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_param_set_enum (control->priv->sonar, "/sync/type", sync_type);
}

/* Функция устанавливает информацию о местоположении приёмных антенн. */
gboolean
hyscan_sonar_control_set_position (HyScanSonarControl    *control,
                                   HyScanSourceType       source,
                                   HyScanAntennaPosition *position)
{
  const gchar *source_name;

  gchar *param_names[7];
  GVariant *param_values[7];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  g_mutex_lock (&control->priv->lock);

  source_name = hyscan_control_get_source_name (source);
  param_names[0] = g_strdup_printf ("/sources/%s/position/x", source_name);
  param_names[1] = g_strdup_printf ("/sources/%s/position/y", source_name);
  param_names[2] = g_strdup_printf ("/sources/%s/position/z", source_name);
  param_names[3] = g_strdup_printf ("/sources/%s/position/psi", source_name);
  param_names[4] = g_strdup_printf ("/sources/%s/position/gamma", source_name);
  param_names[5] = g_strdup_printf ("/sources/%s/position/theta", source_name);
  param_names[6] = NULL;

  param_values[0] = g_variant_new_double (position->x);
  param_values[1] = g_variant_new_double (position->y);
  param_values[2] = g_variant_new_double (position->z);
  param_values[3] = g_variant_new_double (position->psi);
  param_values[4] = g_variant_new_double (position->gamma);
  param_values[5] = g_variant_new_double (position->theta);
  param_values[6] = NULL;

  status = hyscan_param_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
      g_variant_unref (param_values[4]);
      g_variant_unref (param_values[5]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);
  g_free (param_names[3]);
  g_free (param_names[4]);
  g_free (param_names[5]);

  if (status)
    hyscan_data_writer_sonar_set_position (HYSCAN_DATA_WRITER (control), source, position);

  g_mutex_unlock (&control->priv->lock);

  return status;
}

/* Функция задаёт время приёма эхосигнала источником данных. */
gboolean
hyscan_sonar_control_set_receive_time (HyScanSonarControl *control,
                                       HyScanSourceType    source,
                                       gdouble             receive_time)
{
  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  param_name = g_strdup_printf ("/sources/%s/control/receive-time",
                                hyscan_control_get_source_name (source));
  status = hyscan_param_set_double (control->priv->sonar, param_name, receive_time);
  g_free (param_name);

  return status;
}

/* Функция переводит гидролокатор в рабочий режим и включает запись данных. */
gboolean
hyscan_sonar_control_start (HyScanSonarControl *control,
                            const gchar        *track_name,
                            HyScanTrackType     track_type)
{
  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  g_mutex_lock (&control->priv->lock);

  if (hyscan_data_writer_start (HYSCAN_DATA_WRITER (control), track_name, track_type))
    {
      param_names[0] = "/control/track-name";
      param_names[1] = "/control/track-type";
      param_names[2] = "/control/enable";
      param_names[3] = NULL;

      param_values[0] = g_variant_new_string (track_name);
      param_values[1] = g_variant_new_int64 (track_type);
      param_values[2] = g_variant_new_boolean (TRUE);
      param_values[3] = NULL;

      status = hyscan_param_set (control->priv->sonar, (const gchar **)param_names, param_values);

      if (!status)
        {
          g_variant_unref (param_values[0]);
          g_variant_unref (param_values[1]);
          g_variant_unref (param_values[2]);
        }
    }

  g_mutex_unlock (&control->priv->lock);

  return status;
}

/* Функция переводит гидролокатор в ждущий режим и отключает запись данных. */
gboolean
hyscan_sonar_control_stop (HyScanSonarControl *control)
{
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  g_mutex_lock (&control->priv->lock);

  status = hyscan_param_set_boolean (control->priv->sonar, "/control/enable", FALSE);
  hyscan_data_writer_stop (HYSCAN_DATA_WRITER (control));

  g_mutex_unlock (&control->priv->lock);

  return status;
}

/* Функция выполняет один цикл зондирования и приёма данных. */
gboolean
hyscan_sonar_control_ping (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_param_set_boolean (control->priv->sonar, "/sync/ping", TRUE);
}

static void
hyscan_sonar_control_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_sonar_control_schema;
  iface->set = hyscan_sonar_control_set;
  iface->get = hyscan_sonar_control_get;
}
