/*
 * \file hyscan-tvg-control.c
 *
 * \brief Исходный файл класса управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-tvg-control.h"
#include "hyscan-sonar-messages.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"

enum
{
  SIGNAL_GAINS,
  SIGNAL_LAST
};

enum
{
  PROP_O,
  PROP_SONAR
};

typedef struct
{
  HyScanSourceType             source;                         /* Тип источника данных. */
  gchar                       *path;                           /* Путь к описанию источника данных в схеме. */
  HyScanTVGModeType            capabilities;                   /* Режимы работы системы ВАРУ. */
} HyScanTVGControlTVG;

typedef struct
{
  HyScanSourceType             source;                         /* Тип источника данных. */
  guint                        channel;                        /* Индекс приёмного канала. */
} HyScanTVGControlChannelTVG;

struct _HyScanTVGControlPrivate
{
  HyScanParam                 *sonar;                          /* Интерфейс управления гидролокатором. */
  HyScanDataSchema            *schema;                         /* Схема параметров гидролокатора. */

  GHashTable                  *tvgs_by_id;                     /* Список систем ВАРУ. */
  GHashTable                  *tvgs_by_source;                 /* Список систем ВАРУ. */
};

static void    hyscan_tvg_control_set_property         (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_tvg_control_object_constructed   (GObject               *object);
static void    hyscan_tvg_control_object_finalize      (GObject               *object);

static void    hyscan_tvg_control_tvg_receiver         (HyScanTVGControl      *control,
                                                        HyScanSonarMessage    *message);

static void    hyscan_tvg_control_free_tvg             (gpointer               data);

static guint   hyscan_tvg_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTVGControl, hyscan_tvg_control, HYSCAN_TYPE_GENERATOR_CONTROL)

static void
hyscan_tvg_control_class_init (HyScanTVGControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_tvg_control_set_property;

  object_class->constructed = hyscan_tvg_control_object_constructed;
  object_class->finalize = hyscan_tvg_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_PARAM,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_tvg_control_signals[SIGNAL_GAINS] =
    g_signal_new ("gains", HYSCAN_TYPE_GENERATOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_control_marshal_VOID__INT_INT_POINTER,
                  G_TYPE_NONE,
                  3, G_TYPE_INT, G_TYPE_INT, G_TYPE_POINTER);
}

static void
hyscan_tvg_control_init (HyScanTVGControl *control)
{
  control->priv = hyscan_tvg_control_get_instance_private (control);
}

static void
hyscan_tvg_control_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanTVGControl *control = HYSCAN_TVG_CONTROL (object);
  HyScanTVGControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      G_OBJECT_CLASS (hyscan_tvg_control_parent_class)->set_property (object, prop_id, value, pspec);
      priv->sonar = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_tvg_control_object_constructed (GObject *object)
{
  HyScanTVGControl *control = HYSCAN_TVG_CONTROL (object);
  HyScanTVGControlPrivate *priv = control->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sources;

  gint64 version;
  gint64 id;
  guint i, j;

  G_OBJECT_CLASS (hyscan_tvg_control_parent_class)->constructed (object);

  /* Список систем ВАРУ. */
  priv->tvgs_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->tvgs_by_source = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                NULL, hyscan_tvg_control_free_tvg);

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
      /* Считываем описания систем ВАРУ. */
      for (i = 0; i < sources->n_nodes; i++)
        {
          HyScanTVGControlTVG *tvg;

          gchar *param_names[3];
          GVariant *param_values[3];

          gchar **pathv;
          HyScanSourceType source;

          gint64 id;
          gint64 capabilities;

          gboolean status;

          /* Тип борта гидролокатора. */
          pathv = g_strsplit (sources->nodes[i]->path, "/", -1);
          source = hyscan_control_get_source_type (pathv[2]);
          g_strfreev (pathv);

          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          param_names[0] = g_strdup_printf ("%s/tvg/id", sources->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/tvg/capabilities", sources->nodes[i]->path);
          param_names[2] = NULL;

          status = hyscan_param_get (priv->sonar, (const gchar **)param_names, param_values);

          if (status)
            {
              id = g_variant_get_int64 (param_values[0]);
              capabilities = g_variant_get_int64 (param_values[1]);

              g_variant_unref (param_values[0]);
              g_variant_unref (param_values[1]);
            }

          g_free (param_names[0]);
          g_free (param_names[1]);

          if (!status)
            continue;

          if (id <= 0 || id > G_MAXINT32)
            continue;

          /* Описание системы ВАРУ. */
          tvg = g_new0 (HyScanTVGControlTVG, 1);
          tvg->source = source;
          tvg->path = g_strdup_printf ("%s/tvg", sources->nodes[i]->path);
          tvg->capabilities = capabilities;

          g_hash_table_insert (priv->tvgs_by_source, GINT_TO_POINTER (source), tvg);

          /* ВАРУ приёмных каналов. */
          for (j = 1; TRUE; j++)
            {
              HyScanTVGControlChannelTVG *channel_tvg;

              gchar *key_id;
              gboolean has_channel;

              gint64 tvg_id;

              key_id = g_strdup_printf ("%s/channels/%d/id", sources->nodes[i]->path, j);
              has_channel = hyscan_data_schema_has_key (priv->schema, key_id);
              g_free (key_id);

              if (!has_channel)
                break;

              /* Идентификатор ВАРУ канала. */
              key_id = g_strdup_printf ("%s/channels/%d/tvg/id", sources->nodes[i]->path, j);
              if (hyscan_param_get_integer (priv->sonar, key_id, &tvg_id))
                {
                  channel_tvg = g_new0 (HyScanTVGControlChannelTVG, 1);
                  channel_tvg->source = source;
                  channel_tvg->channel = j;
                  g_hash_table_insert (priv->tvgs_by_id, GINT_TO_POINTER (tvg_id), channel_tvg);
                }
              g_free (key_id);
            }
        }

      /* Обработчик параметров системы ВАРУ от гидролокатора. */
      g_signal_connect_swapped (priv->sonar, "data",
                                G_CALLBACK (hyscan_tvg_control_tvg_receiver), control);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_tvg_control_object_finalize (GObject *object)
{
  HyScanTVGControl *control = HYSCAN_TVG_CONTROL (object);
  HyScanTVGControlPrivate *priv = control->priv;

  g_signal_handlers_disconnect_by_data (priv->sonar, control);

  g_clear_object (&priv->schema);
  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->tvgs_by_source);
  g_hash_table_unref (priv->tvgs_by_id);

  G_OBJECT_CLASS (hyscan_tvg_control_parent_class)->finalize (object);
}

/* Функция обрабатывает сообщения системы ВАРУ гидролокатора. */
static void
hyscan_tvg_control_tvg_receiver (HyScanTVGControl   *control,
                                 HyScanSonarMessage *message)
{
  HyScanTVGControlChannelTVG *tvg;

  HyScanDataWriterTVG gain;

  /* Проверяем тип данных. */
  if (message->type != HYSCAN_DATA_FLOAT)
    return;

  /* Ищем систему ВАРУ. */
  tvg = g_hash_table_lookup (control->priv->tvgs_by_id, GINT_TO_POINTER (message->id));
  if (tvg == NULL)
    return;

  /* Параметры ВАРУ. */
  gain.time = message->time;
  gain.rate = message->rate;
  gain.n_gains = message->size / sizeof (gfloat);
  gain.gains = message->data;

  hyscan_data_writer_raw_add_tvg (HYSCAN_DATA_WRITER (control), tvg->source, tvg->channel, &gain);

  g_signal_emit (control, hyscan_tvg_control_signals[SIGNAL_GAINS], 0, tvg->source, tvg->channel, &gain);
}

/* Функция освобождает память, занятую структурой HyScanTVGControlTVG. */
static void
hyscan_tvg_control_free_tvg (gpointer data)
{
  HyScanTVGControlTVG *tvg = data;

  g_free (tvg->path);
  g_free (tvg);
}

/* Функция возвращает флаги допустимых режимов работы системы ВАРУ. */
HyScanTVGModeType
hyscan_tvg_control_get_capabilities (HyScanTVGControl *control,
                                     HyScanSourceType  source)
{
  HyScanTVGControlTVG *tvg;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), HYSCAN_TVG_MODE_INVALID);

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return HYSCAN_GENERATOR_MODE_INVALID;

  return tvg->capabilities;
}

/* Функция возвращает допустимые пределы диапазона регулировки усиления ВАРУ. */
gboolean
hyscan_tvg_control_get_gain_range (HyScanTVGControl     *control,
                                   HyScanSourceType      source,
                                   gdouble              *min_gain,
                                   gdouble              *max_gain)
{
  HyScanTVGControlTVG *tvg;

  gchar *param_name;
  GVariant *min_gain_value;
  GVariant *max_gain_value;

  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), FALSE);

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/constant/gain", tvg->path);
  min_gain_value = hyscan_data_schema_key_get_minimum (control->priv->schema, param_name);
  max_gain_value = hyscan_data_schema_key_get_maximum (control->priv->schema, param_name);
  g_free (param_name);

  if (min_gain_value != NULL && max_gain_value != NULL)
    {
      *min_gain = g_variant_get_double (min_gain_value);
      *max_gain = g_variant_get_double (max_gain_value);
      status = TRUE;
    }
  else
    {
      status = FALSE;
    }

  g_clear_pointer (&min_gain_value, g_variant_unref);
  g_clear_pointer (&max_gain_value, g_variant_unref);

  return status;
}

/* Функция включает автоматический режим управления системой ВАРУ. */
gboolean
hyscan_tvg_control_set_auto (HyScanTVGControl *control,
                             HyScanSourceType  source,
                             gdouble           level,
                             gdouble           sensitivity)
{
  HyScanTVGControlTVG *tvg;

  gchar *param_names[3];
  GVariant *param_values[3];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/auto/level", tvg->path);
  param_names[1] = g_strdup_printf ("%s/auto/sensitivity", tvg->path);
  param_names[2] = NULL;

  if (level < 0.0)
    param_values[0] = NULL;
  else
    param_values[0] = g_variant_new_double (level);

  if (sensitivity < 0.0)
    param_values[1] = NULL;
  else
    param_values[1] = g_variant_new_double (sensitivity);

  param_values[2] = NULL;

  status = hyscan_param_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_clear_pointer (&param_values[0], g_variant_unref);
      g_clear_pointer (&param_values[1], g_variant_unref);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);

  return status;
}

/* Функция устанавливает постоянный уровень усиления системой ВАРУ. */
gboolean
hyscan_tvg_control_set_constant (HyScanTVGControl *control,
                                 HyScanSourceType  source,
                                 gdouble           gain)
{
  HyScanTVGControlTVG *tvg;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/constant/gain", tvg->path);
  status = hyscan_param_set_double (control->priv->sonar, param_name, gain);
  g_free (param_name);

  return status;
}

/* Функция устанавливает линейное увеличение усиления в дБ на 100 метров. */
gboolean
hyscan_tvg_control_set_linear_db (HyScanTVGControl *control,
                                  HyScanSourceType  source,
                                  gdouble           gain0,
                                  gdouble           step)
{
  HyScanTVGControlTVG *tvg;

  gchar *param_names[3];
  GVariant *param_values[3];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/linear-db/gain0", tvg->path);
  param_names[1] = g_strdup_printf ("%s/linear-db/step", tvg->path);
  param_names[2] = NULL;

  param_values[0] = g_variant_new_double (gain0);
  param_values[1] = g_variant_new_double (step);
  param_values[2] = NULL;

  status = hyscan_param_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);

  return status;
}

/* Функция устанавливает логарифмический вид закона усиления системой ВАРУ. */
gboolean
hyscan_tvg_control_set_logarithmic (HyScanTVGControl *control,
                                    HyScanSourceType  source,
                                    gdouble           gain0,
                                    gdouble           beta,
                                    gdouble           alpha)
{
  HyScanTVGControlTVG *tvg;

  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/logarithmic/gain0", tvg->path);
  param_names[1] = g_strdup_printf ("%s/logarithmic/beta", tvg->path);
  param_names[2] = g_strdup_printf ("%s/logarithmic/alpha", tvg->path);
  param_names[3] = NULL;

  param_values[0] = g_variant_new_double (gain0);
  param_values[1] = g_variant_new_double (beta);
  param_values[2] = g_variant_new_double (alpha);
  param_values[3] = NULL;

  status = hyscan_param_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);

  return status;
}

/* Функция включает или выключает систему ВАРУ. */
gboolean
hyscan_tvg_control_set_enable (HyScanTVGControl *control,
                               HyScanSourceType  source,
                               gboolean          enable)
{
  HyScanTVGControlTVG *tvg;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  tvg = g_hash_table_lookup (control->priv->tvgs_by_source, GINT_TO_POINTER (source));
  if (tvg == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/enable", tvg->path);
  status = hyscan_param_set_boolean (control->priv->sonar, param_name, enable);
  g_free (param_name);

  return status;
}
