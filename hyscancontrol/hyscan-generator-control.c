/*
 * \file hyscan-generator-control.c
 *
 * \brief Исходный файл класса управления генераторами сигналов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-generator-control.h"
#include "hyscan-sonar-messages.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"

enum
{
  SIGNAL_SIGNAL_IMAGE,
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
  gchar                       *path;                           /* Путь к описанию генератора в схеме. */
  HyScanGeneratorModeType      capabilities;                   /* Режимы работы. */
  HyScanGeneratorSignalType    signals;                        /* Возможные сигналы. */
} HyScanGeneratorControlGen;

struct _HyScanGeneratorControlPrivate
{
  HyScanParam                 *sonar;                          /* Интерфейс управления гидролокатором. */
  HyScanDataSchema            *schema;                         /* Схема параметров гидролокатора. */

  GHashTable                  *gens_by_id;                     /* Список генераторов гидролокатора. */
  GHashTable                  *gens_by_source;                 /* Список генераторов гидролокатора. */
};

static void    hyscan_generator_control_set_property           (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void    hyscan_generator_control_object_constructed     (GObject                   *object);
static void    hyscan_generator_control_object_finalize        (GObject                   *object);

static void    hyscan_generator_control_signal_receiver        (HyScanGeneratorControl    *control,
                                                                HyScanSonarMessage        *message);

static void    hyscan_generator_control_free_gen               (gpointer                   data);

static guint   hyscan_generator_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGeneratorControl, hyscan_generator_control, HYSCAN_TYPE_SENSOR_CONTROL)

static void
hyscan_generator_control_class_init (HyScanGeneratorControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_generator_control_set_property;

  object_class->constructed = hyscan_generator_control_object_constructed;
  object_class->finalize = hyscan_generator_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_PARAM,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_generator_control_signals[SIGNAL_SIGNAL_IMAGE] =
    g_signal_new ("signal-image", HYSCAN_TYPE_GENERATOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_control_marshal_VOID__INT_POINTER,
                  G_TYPE_NONE,
                  2, G_TYPE_INT, G_TYPE_POINTER);
}

static void
hyscan_generator_control_init (HyScanGeneratorControl *control)
{
  control->priv = hyscan_generator_control_get_instance_private (control);
}

static void
hyscan_generator_control_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGeneratorControl *control = HYSCAN_GENERATOR_CONTROL (object);
  HyScanGeneratorControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      G_OBJECT_CLASS (hyscan_generator_control_parent_class)->set_property (object, prop_id, value, pspec);
      priv->sonar = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_generator_control_object_constructed (GObject *object)
{
  HyScanGeneratorControl *control = HYSCAN_GENERATOR_CONTROL (object);
  HyScanGeneratorControlPrivate *priv = control->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sources;

  gint64 version;
  gint64 id;
  guint i;

  G_OBJECT_CLASS (hyscan_generator_control_parent_class)->constructed (object);

  /* Список генераторов. */
  priv->gens_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, hyscan_generator_control_free_gen);
  priv->gens_by_source = g_hash_table_new (g_direct_hash, g_direct_equal);

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
      /* Считываем описания генераторов. */
      for (i = 0; i < sources->n_nodes; i++)
        {
          HyScanGeneratorControlGen *generator;

          gchar *param_names[4];
          GVariant *param_values[4];

          gchar **pathv;
          HyScanSourceType source;

          gint64 id;
          gint64 capabilities;
          gint64 signals;

          gboolean status;

          /* Тип источника данных гидролокатора. */
          pathv = g_strsplit (sources->nodes[i]->path, "/", -1);
          source = hyscan_control_get_source_type (pathv[2]);
          g_strfreev (pathv);

          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          param_names[0] = g_strdup_printf ("%s/generator/id", sources->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/generator/capabilities", sources->nodes[i]->path);
          param_names[2] = g_strdup_printf ("%s/generator/signals", sources->nodes[i]->path);
          param_names[3] = NULL;

          status = hyscan_param_get (priv->sonar, (const gchar **)param_names, param_values);

          if (status)
            {
              id = g_variant_get_int64 (param_values[0]);
              capabilities = g_variant_get_int64 (param_values[1]);
              signals = g_variant_get_int64 (param_values[2]);

              g_variant_unref (param_values[0]);
              g_variant_unref (param_values[1]);
              g_variant_unref (param_values[2]);
            }

          g_free (param_names[0]);
          g_free (param_names[1]);
          g_free (param_names[2]);

          if (!status)
            continue;

          if (id <= 0 || id > G_MAXINT32)
            continue;

          /* Описание генератора. */
          generator = g_new0 (HyScanGeneratorControlGen, 1);
          generator->source = source;
          generator->path = g_strdup_printf ("%s/generator", sources->nodes[i]->path);
          generator->capabilities = capabilities;
          generator->signals = signals;

          g_hash_table_insert (priv->gens_by_id, GINT_TO_POINTER (id), generator);
          g_hash_table_insert (priv->gens_by_source, GINT_TO_POINTER (source), generator);
        }

      /* Обработчик образов сигналов от гидролокатора. */
      g_signal_connect_swapped (priv->sonar, "data",
                                G_CALLBACK (hyscan_generator_control_signal_receiver), control);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_generator_control_object_finalize (GObject *object)
{
  HyScanGeneratorControl *control = HYSCAN_GENERATOR_CONTROL (object);
  HyScanGeneratorControlPrivate *priv = control->priv;

  g_signal_handlers_disconnect_by_data (priv->sonar, control);

  g_clear_object (&priv->schema);
  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->gens_by_source);
  g_hash_table_unref (priv->gens_by_id);

  G_OBJECT_CLASS (hyscan_generator_control_parent_class)->finalize (object);
}

/* Функция обрабатывает сообщения с образцами сигналов от гидролокатора. */
static void
hyscan_generator_control_signal_receiver (HyScanGeneratorControl *control,
                                          HyScanSonarMessage     *message)
{
  HyScanGeneratorControlGen *generator;

  HyScanDataWriterSignal signal;

  /* Проверяем тип данных. */
  if (message->type != HYSCAN_DATA_COMPLEX_FLOAT)
    return;

  /* Ищем генератор. */
  generator = g_hash_table_lookup (control->priv->gens_by_id, GINT_TO_POINTER (message->id));
  if (generator == NULL)
    return;

  /* Образец сигнала. */
  signal.time = message->time;
  signal.rate = message->rate;
  signal.n_points = message->size / sizeof (HyScanComplexFloat);
  signal.points = message->data;

  hyscan_data_writer_raw_add_signal (HYSCAN_DATA_WRITER (control), generator->source, &signal);

  g_signal_emit (control, hyscan_generator_control_signals[SIGNAL_SIGNAL_IMAGE], 0, generator->source, &signal);
}

/* Функция освобождает память, занятую структурой HyScanGeneratorControlGen. */
static void
hyscan_generator_control_free_gen (gpointer data)
{
  HyScanGeneratorControlGen *generator = data;

  g_free (generator->path);
  g_free (generator);
}

/* Функция возвращает флаги допустимых режимов работы генератора. */
HyScanGeneratorModeType
hyscan_generator_control_get_capabilities (HyScanGeneratorControl *control,
                                           HyScanSourceType        source)
{
  HyScanGeneratorControlGen *generator;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), HYSCAN_GENERATOR_MODE_INVALID);

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return HYSCAN_GENERATOR_MODE_INVALID;

  return generator->capabilities;
}

/* Функция возвращает флаги допустимых сигналов генератора. */
HyScanGeneratorSignalType
hyscan_generator_control_get_signals (HyScanGeneratorControl *control,
                                      HyScanSourceType        source)
{
  HyScanGeneratorControlGen *generator;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), HYSCAN_GENERATOR_SIGNAL_INVALID);

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return HYSCAN_GENERATOR_SIGNAL_INVALID;

  return generator->signals;
}

/* Функция возвращает максимальную длительность сигнала. */
gboolean
hyscan_generator_control_get_duration_range (HyScanGeneratorControl    *control,
                                             HyScanSourceType           source,
                                             HyScanGeneratorSignalType  signal,
                                             gdouble                   *min_duration,
                                             gdouble                   *max_duration)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  GVariant *min_duration_value;
  GVariant *max_duration_value;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return FALSE;

  if (signal == HYSCAN_GENERATOR_SIGNAL_TONE)
    param_name = g_strdup_printf ("%s/tone/duration", generator->path);
  else if (signal != HYSCAN_GENERATOR_SIGNAL_LFM || signal != HYSCAN_GENERATOR_SIGNAL_LFMD)
    param_name = g_strdup_printf ("%s/lfm/duration", generator->path);
  else
    return FALSE;

  min_duration_value = hyscan_data_schema_key_get_minimum (control->priv->schema, param_name);
  max_duration_value = hyscan_data_schema_key_get_maximum (control->priv->schema, param_name);

  if (min_duration_value != NULL && max_duration_value != NULL)
    {
      *min_duration = g_variant_get_double (min_duration_value);
      *max_duration = g_variant_get_double (max_duration_value);

      status = TRUE;
    }

  g_clear_pointer (&min_duration_value, g_variant_unref);
  g_clear_pointer (&max_duration_value, g_variant_unref);

  g_free (param_name);

  return status;
}

/* Функция возвращает список преднастроек генератора. */
HyScanDataSchemaEnumValue **
hyscan_generator_control_list_presets (HyScanGeneratorControl *control,
                                       HyScanSourceType        source)
{
  HyScanGeneratorControlGen *generator;

  const gchar *param_values_id;
  HyScanDataSchemaEnumValue **param_values = NULL;
  gchar *params_name;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return NULL;

  params_name = g_strdup_printf ("%s/preset/id", generator->path);
  param_values_id = hyscan_data_schema_key_get_enum_id (control->priv->schema, params_name);
  if (param_values_id != NULL)
    param_values = hyscan_data_schema_key_get_enum_values (control->priv->schema, param_values_id);
  g_free (params_name);

  return param_values;
}

/* Функция включает преднастроенный режим работы генератора. */
gboolean
hyscan_generator_control_set_preset (HyScanGeneratorControl *control,
                                     HyScanSourceType        source,
                                     guint                   preset)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/preset/id", generator->path);
  status = hyscan_param_set_enum (control->priv->sonar, param_name, preset);
  g_free (param_name);

  return status;
}

/* Функция включает автоматический режим работы генератора. */
gboolean
hyscan_generator_control_set_auto (HyScanGeneratorControl    *control,
                                   HyScanSourceType           source,
                                   HyScanGeneratorSignalType  signal)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/auto/signal", generator->path);
  status = hyscan_param_set_enum (control->priv->sonar, param_name, signal);
  g_free (param_name);

  return status;
}

/* Функция включает упрощённый режим работы генератора. */
gboolean
hyscan_generator_control_set_simple (HyScanGeneratorControl    *control,
                                     HyScanSourceType           source,
                                     HyScanGeneratorSignalType  signal,
                                     gdouble                    power)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_names[3];
  GVariant *param_values[3];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/simple/signal", generator->path);
  param_names[1] = g_strdup_printf ("%s/simple/power", generator->path);
  param_names[2] = NULL;

  param_values[0] = g_variant_new_int64 (signal);
  param_values[1] = g_variant_new_double (power);
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

/* Функция включает расширенный режим работы генератора. */
gboolean
hyscan_generator_control_set_extended (HyScanGeneratorControl    *control,
                                       HyScanSourceType           source,
                                       HyScanGeneratorSignalType  signal,
                                       gdouble                    duration,
                                       gdouble                    power)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return FALSE;

  if (signal == HYSCAN_GENERATOR_SIGNAL_TONE)
    {
      param_names[0] = g_strdup_printf ("%s/tone/duration", generator->path);
      param_names[1] = g_strdup_printf ("%s/tone/power", generator->path);
      param_names[2] = NULL;
      param_names[3] = NULL;

      param_values[0] = g_variant_new_double (duration);
      param_values[1] = g_variant_new_double (power);
      param_values[2] = NULL;
      param_values[3] = NULL;
    }
  else if (signal == HYSCAN_GENERATOR_SIGNAL_LFM || signal == HYSCAN_GENERATOR_SIGNAL_LFMD)
    {
      gboolean decreasing;

      decreasing = (signal == HYSCAN_GENERATOR_SIGNAL_LFMD) ? TRUE : FALSE;

      param_names[0] = g_strdup_printf ("%s/lfm/decreasing", generator->path);
      param_names[1] = g_strdup_printf ("%s/lfm/duration", generator->path);
      param_names[2] = g_strdup_printf ("%s/lfm/power", generator->path);
      param_names[3] = NULL;

      param_values[0] = g_variant_new_boolean (decreasing);
      param_values[1] = g_variant_new_double (duration);
      param_values[2] = g_variant_new_double (power);
      param_values[3] = NULL;
    }
  else
    {
      return FALSE;
    }

  status = hyscan_param_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_clear_pointer (&param_values[0], g_variant_unref);
      g_clear_pointer (&param_values[1], g_variant_unref);
      g_clear_pointer (&param_values[2], g_variant_unref);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);

  return status;
}

/* Функция включает или выключает формирование сигнала генератором. */
gboolean
hyscan_generator_control_set_enable (HyScanGeneratorControl *control,
                                     HyScanSourceType        source,
                                     gboolean                enable)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_source, GINT_TO_POINTER (source));
  if (generator == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_param_set_boolean (control->priv->sonar, param_name, enable);
  g_free (param_name);

  return status;
}
