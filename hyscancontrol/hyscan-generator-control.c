/*
 * \file hyscan-generator-control.c
 *
 * \brief Исходный файл класса управления генераторами сигналов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control-common.h"
#include "hyscan-generator-control.h"

enum
{
  SIGNAL_SIGNAL,
  SIGNAL_LAST
};

enum
{
  PROP_O,
  PROP_SONAR
};

typedef struct
{
  guint32                      id;                             /* Идентификатор генератора. */
  HyScanBoardType              board;                          /* Тип борта гидролокатора. */
  gchar                       *path;                           /* Путь к описанию генератора в схеме. */
  HyScanGeneratorModeType      capabilities;                   /* Режимы работы. */
  HyScanGeneratorSignalType    signals;                        /* Возможные сигналы. */
} HyScanGeneratorControlGen;

struct _HyScanGeneratorControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */
  gulong                       signal_id;                      /* Идентификатор обработчика сигнала data. */

  GHashTable                  *gens_by_id;                     /* Список генераторов гидролокатора. */
  GHashTable                  *gens_by_board;                  /* Список генераторов гидролокатора. */
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
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_generator_control_signals[SIGNAL_SIGNAL] =
    g_signal_new ("signal", HYSCAN_TYPE_GENERATOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1, G_TYPE_POINTER);
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
  HyScanDataSchemaNode *boards;

  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_generator_control_parent_class)->constructed (object);

  /* Список генераторов. */
  priv->gens_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, hyscan_generator_control_free_gen);
  priv->gens_by_board = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanGeneratorControl: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanGeneratorControl: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanGeneratorControl: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanGeneratorControl: sonar schema version mismatch");
      return;
    }

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
      /* Считываем описания генераторов. */
      for (i = 0; i < boards->n_nodes; i++)
        {
          HyScanGeneratorControlGen *generator;

          gchar *param_names[4];
          GVariant *param_values[4];

          gchar **pathv;
          guint board;

          gint64 id;
          gint64 capabilities;
          gint64 signals;

          gboolean status;

          /* Тип борта гидролокатора. */
          pathv = g_strsplit (boards->nodes[i]->path, "/", -1);
          board = hyscan_control_get_board_type (pathv[2]);
          g_strfreev (pathv);

          if (board == HYSCAN_BOARD_INVALID)
            continue;

          param_names[0] = g_strdup_printf ("%s/generator/id", boards->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/generator/capabilities", boards->nodes[i]->path);
          param_names[2] = g_strdup_printf ("%s/generator/signals", boards->nodes[i]->path);
          param_names[3] = NULL;

          status = hyscan_sonar_get (priv->sonar, (const gchar **)param_names, param_values);

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

          if (id <= 0 || id > G_MAXUINT32)
            continue;

          /* Описание генератора. */
          generator = g_new0 (HyScanGeneratorControlGen, 1);
          generator->id = id;
          generator->board = board;
          generator->path = g_strdup_printf ("%s/generator", boards->nodes[i]->path);
          generator->capabilities = capabilities;
          generator->signals = signals;

          g_hash_table_insert (priv->gens_by_id, GINT_TO_POINTER (generator->id), generator);
          g_hash_table_insert (priv->gens_by_board, GINT_TO_POINTER (board), generator);
        }

      /* Обработчик образов сигналов от гидролокатора. */
      priv->signal_id = g_signal_connect_swapped (priv->sonar,
                                                  "data",
                                                  G_CALLBACK (hyscan_generator_control_signal_receiver),
                                                  control);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_generator_control_object_finalize (GObject *object)
{
  HyScanGeneratorControl *control = HYSCAN_GENERATOR_CONTROL (object);
  HyScanGeneratorControlPrivate *priv = control->priv;

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->sonar, priv->signal_id);

  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->gens_by_board);
  g_hash_table_unref (priv->gens_by_id);

  G_OBJECT_CLASS (hyscan_generator_control_parent_class)->finalize (object);
}

/* Функция обрабатывает сообщения с образцами сигналов от гидролокатора. */
static void
hyscan_generator_control_signal_receiver (HyScanGeneratorControl *control,
                                          HyScanSonarMessage     *message)
{
  HyScanGeneratorControlGen *generator;

  HyScanWriteSignal signal;

  /* Проверяем тип данных. */
  if (message->type != HYSCAN_DATA_COMPLEX_FLOAT)
    return;

  /* Ищем генератор. */
  generator = g_hash_table_lookup (control->priv->gens_by_id, GINT_TO_POINTER (message->id));
  if (generator == NULL)
    return;

  /* Образец сигнала. */
  signal.board = generator->board;
  signal.time = message->time;
  signal.rate = message->rate;
  signal.n_points = message->size / sizeof (HyScanComplexFloat);
  signal.points = message->data;

  hyscan_write_control_sonar_add_signal (HYSCAN_WRITE_CONTROL (control), &signal);

  g_signal_emit (control, hyscan_generator_control_signals[SIGNAL_SIGNAL], 0, &signal);
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
                                           HyScanBoardType         board)
{
  HyScanGeneratorControlGen *generator;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), HYSCAN_GENERATOR_MODE_INVALID);

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return HYSCAN_GENERATOR_MODE_INVALID;

  return generator->capabilities;
}

/* Функция возвращает флаги допустимых сигналов генератора. */
HyScanGeneratorSignalType
hyscan_generator_control_get_signals (HyScanGeneratorControl *control,
                                      HyScanBoardType         board)
{
  HyScanGeneratorControlGen *generator;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), HYSCAN_GENERATOR_SIGNAL_INVALID);

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return HYSCAN_GENERATOR_SIGNAL_INVALID;

  return generator->signals;
}

/* Функция возвращает максимальную длительность сигнала. */
gboolean
hyscan_generator_control_get_duration_range (HyScanGeneratorControl    *control,
                                             HyScanBoardType            board,
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

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  if (signal == HYSCAN_GENERATOR_SIGNAL_TONE)
    param_name = g_strdup_printf ("%s/tone/duration", generator->path);
  else if (signal != HYSCAN_GENERATOR_SIGNAL_LFM || signal != HYSCAN_GENERATOR_SIGNAL_LFMD)
    param_name = g_strdup_printf ("%s/lfm/duration", generator->path);
  else
    return FALSE;

  min_duration_value = hyscan_data_schema_key_get_minimum (HYSCAN_DATA_SCHEMA (control->priv->sonar), param_name);
  max_duration_value = hyscan_data_schema_key_get_maximum (HYSCAN_DATA_SCHEMA (control->priv->sonar), param_name);

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
                                       HyScanBoardType         board)
{
  HyScanGeneratorControlGen *generator;

  HyScanDataSchemaEnumValue **param_values;
  gchar *params_name;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), NULL);

  if (control->priv->sonar == NULL)
    return NULL;

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return NULL;

  params_name = g_strdup_printf ("%s/preset/id", generator->path);
  param_values = hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (control->priv->sonar), params_name);
  g_free (params_name);

  return param_values;
}

/* Функция включает преднастроенный режим работы генератора. */
gboolean
hyscan_generator_control_set_preset (HyScanGeneratorControl *control,
                                     HyScanBoardType         board,
                                     gint64                  preset)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/preset/id", generator->path);
  status = hyscan_sonar_set_enum (control->priv->sonar, param_name, preset);
  g_free (param_name);

  return status;
}

/* Функция включает автоматический режим работы генератора. */
gboolean
hyscan_generator_control_set_auto (HyScanGeneratorControl    *control,
                                   HyScanBoardType            board,
                                   HyScanGeneratorSignalType  signal)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/auto/signal", generator->path);
  status = hyscan_sonar_set_enum (control->priv->sonar, param_name, signal);
  g_free (param_name);

  return status;
}

/* Функция включает упрощённый режим работы генератора. */
gboolean
hyscan_generator_control_set_simple (HyScanGeneratorControl    *control,
                                     HyScanBoardType            board,
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

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  param_names[0] = g_strdup_printf ("%s/simple/signal", generator->path);
  param_names[1] = g_strdup_printf ("%s/simple/power", generator->path);
  param_names[2] = NULL;

  param_values[0] = g_variant_new_int64 (signal);
  param_values[1] = g_variant_new_double (power);
  param_values[2] = NULL;

  status = hyscan_sonar_set (control->priv->sonar, (const gchar **)param_names, param_values);

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
                                       HyScanBoardType            board,
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

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
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

  status = hyscan_sonar_set (control->priv->sonar, (const gchar **)param_names, param_values);

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
                                     HyScanBoardType         board,
                                     gboolean                enable)
{
  HyScanGeneratorControlGen *generator;

  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  if (control->priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  param_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_sonar_set_boolean (control->priv->sonar, param_name, enable);
  g_free (param_name);

  return status;
}
