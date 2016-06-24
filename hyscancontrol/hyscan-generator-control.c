/*
 * \file hyscan-generator-control.c
 *
 * \brief Исходный файл класса управления генераторами сигналов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
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
  gint                         id;                             /* Идентификатор генератора. */
  HyScanBoardType              board;                          /* Тип борта гидролокатора. */
  gchar                       *path;                           /* Путь к описанию генератора в схеме. */
  HyScanGeneratorModeType      capabilities;                   /* Режимы работы. */
  HyScanGeneratorSignalType    signals;                        /* Возможные сигналы. */
} HyScanGeneratorControlGen;

struct _HyScanGeneratorControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */

  HyScanDataSchema            *schema;                         /* Схема данных гидролокатора. */
  HyScanDataSchemaNode        *params;                         /* Список параметров гидролокатора. */

  GHashTable                  *gens_by_id;                     /* Список генераторов гидролокатора. */
  GHashTable                  *gens_by_board;                  /* Список генераторов гидролокатора. */

  GMutex                       lock;                           /* Блокировка. */
};

static void    hyscan_generator_control_set_property           (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void    hyscan_generator_control_object_constructed     (GObject                   *object);
static void    hyscan_generator_control_object_finalize        (GObject                   *object);

static void    hyscan_generator_control_signal_receiver        (HyScanGeneratorControl    *control,
                                                                HyScanSonarMsgSignal      *signal_msg);

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
hyscan_generator_control_init (HyScanGeneratorControl *generator_control)
{
  generator_control->priv = hyscan_generator_control_get_instance_private (generator_control);
}

static void
hyscan_generator_control_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGeneratorControl *generator_control = HYSCAN_GENERATOR_CONTROL (object);
  HyScanGeneratorControlPrivate *priv = generator_control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
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

  HyScanDataSchemaNode *boards = NULL;
  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_generator_control_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSensor: sonar schema version mismatch");
      return;
    }

  /* Схема данных гидролокатора. */
  priv->schema = hyscan_sonar_get_schema (priv->sonar);
  priv->params = hyscan_data_schema_list_nodes (priv->schema);

  /* Список генераторов. */
  priv->gens_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, hyscan_generator_control_free_gen);
  priv->gens_by_board = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Ветка схемы с описанием бортов гидролокатора - "/boards". */
  for (i = 0; priv->params->n_nodes; i++)
    {
      if (g_strcmp0 (priv->params->nodes[i]->path, "/boards") == 0)
        {
          boards = priv->params->nodes[i];
          break;
        }
    }

  if (boards == NULL)
    return;

  /* Считываем описания генераторов. */
  for (i = 0; i < boards->n_nodes; i++)
    {
      HyScanGeneratorControlGen *generator;

      gchar *key_name;
      gboolean status;

      gint64 id;
      gint64 board;
      gint64 capabilities;
      gint64 signals;

      /* Тип борта гидролокатора. */
      key_name = g_strdup_printf ("%s/type", boards->nodes[i]->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &board);
      g_free (key_name);

      if (!status)
        continue;

      /* Идентификатор генератора. */
      key_name = g_strdup_printf ("%s/generator/id", boards->nodes[i]->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &id);
      g_free (key_name);

      if (!status || id <= 0 || id > G_MAXUINT32)
        continue;

      /* Режимы работы генератора. */
      key_name = g_strdup_printf ("%s/generator/capabilities", boards->nodes[i]->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &capabilities);
      g_free (key_name);

      if (!status)
        continue;

      /* Возможные сигналы. */
      key_name = g_strdup_printf ("%s/generator/signals", boards->nodes[i]->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &signals);
      g_free (key_name);

      if (!status)
        continue;

      /* Описание генератора. */
      generator = g_new0 (HyScanGeneratorControlGen, 1);
      generator->board = board;
      generator->path = g_strdup_printf ("%s/generator", boards->nodes[i]->path);
      generator->capabilities = capabilities & 0xF;
      generator->signals = signals & 0xF;

      if (!g_hash_table_insert (priv->gens_by_id, GINT_TO_POINTER (id), generator))
        hyscan_generator_control_free_gen (generator);

//      if (!g_hash_table_insert (priv->gens_by_source, GINT_TO_POINTER (source), generator))
//        g_hash_table_remove (priv->gens_by_id, GINT_TO_POINTER (id));
    }

  /* Обработчик образов сигналов от гидролокатора. */
  g_signal_connect_swapped (priv->sonar, "signal", G_CALLBACK (hyscan_generator_control_signal_receiver), control);
}

static void
hyscan_generator_control_object_finalize (GObject *object)
{
  HyScanGeneratorControl *generator_control = HYSCAN_GENERATOR_CONTROL (object);
  HyScanGeneratorControlPrivate *priv = generator_control->priv;

  g_clear_pointer (&priv->gens_by_board, g_hash_table_unref);
  g_clear_pointer (&priv->gens_by_id, g_hash_table_unref);

  g_clear_pointer (&priv->params, hyscan_data_schema_free_nodes);

  g_clear_object (&priv->sonar);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_generator_control_parent_class)->finalize (object);
}

/* Функция обрабатывает сообщения с образцами сигналов от гидролокатора. */
static void
hyscan_generator_control_signal_receiver (HyScanGeneratorControl *control,
                                          HyScanSonarMsgSignal   *signal_msg)
{
  HyScanGeneratorControlPrivate *priv = control->priv;
  HyScanGeneratorControlGen *generator;

  HyScanWriteSignal signal;

  g_mutex_lock (&priv->lock);

  /* Ищем генератор. */
  generator = g_hash_table_lookup (priv->gens_by_id, GINT_TO_POINTER (signal_msg->id));
  if (generator == NULL)
    goto exit;

  /* Образец сигнала. */
  signal.board = generator->board;
  signal.time = signal_msg->time;
  signal.n_points = signal_msg->n_points;
  signal.points = signal_msg->points;

  #warning "write signal image to writer"

  g_signal_emit (control, hyscan_generator_control_signals[SIGNAL_SIGNAL], 0, &signal);

exit:
  g_mutex_unlock (&priv->lock);
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

/* Функция возвращает список преднастроек генератора. */
HyScanDataSchemaEnumValue **
hyscan_generator_control_list_presets (HyScanGeneratorControl *control,
                                       HyScanBoardType         board)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;
  HyScanDataSchemaEnumValue **values;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  generator = g_hash_table_lookup (priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return NULL;

  key_name = g_strdup_printf ("%s/preset/id", generator->path);
  values = hyscan_data_schema_key_get_enum_values (priv->schema, key_name);
  g_free (key_name);

  return values;
}

/* Функция включает преднастроенный режим работы генератора. */
gboolean
hyscan_generator_control_set_preset (HyScanGeneratorControl *control,
                                     HyScanBoardType         board,
                                     gint64                  preset)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gint64 ivalue;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  /* Временно отключаем генератор и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_generator_control_set_enable (control, board, FALSE))
    goto exit;

  key_name = g_strdup_printf ("%s/preset/id", generator->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, preset);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || ivalue != preset)
    goto exit;

  /* Включаем генератор. */
  if (enable)
    {
      if (hyscan_generator_control_set_enable (control, board, enable))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
}

/* Функция включает автоматический режим работы генератора. */
gboolean
hyscan_generator_control_set_auto (HyScanGeneratorControl    *control,
                                   HyScanBoardType            board,
                                   HyScanGeneratorSignalType  signal)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gint64 ivalue;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (control->priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;
  g_mutex_lock (&priv->lock);

  /* Временно отключаем генератор и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_generator_control_set_enable (control, board, FALSE))
    goto exit;

  key_name = g_strdup_printf ("%s/auto/signal", generator->path);
  hyscan_sonar_set_enum (priv->sonar, key_name, signal);
  status = hyscan_sonar_get_enum (priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || ivalue != signal)
    goto exit;

  /* Включаем генератор. */
  if (enable)
    {
      if (hyscan_generator_control_set_enable (control, board, enable))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
}

/* Функция включает упрощённый режим работы генератора. */
gboolean
hyscan_generator_control_set_simple (HyScanGeneratorControl    *control,
                                     HyScanBoardType            board,
                                     HyScanGeneratorSignalType  type,
                                     gdouble                    power)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gdouble dvalue;
  gint64 ivalue;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  /* Временно отключаем генератор и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_generator_control_set_enable (control, board, FALSE))
    goto exit;

  key_name = g_strdup_printf ("%s/simple/type", generator->path);
  hyscan_sonar_set_enum (control->priv->sonar, key_name, type);
  status = hyscan_sonar_get_enum (control->priv->sonar, key_name, &ivalue);
  g_free (key_name);

  if (!status || ivalue != type)
    return FALSE;

  if (power < 0.0)
    power = -1.0;

  key_name = g_strdup_printf ("%s/simple/power", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, power);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки энергии сигнала - 1%. */
  dvalue -= power;
  if (!status || ABS (dvalue) > 1.0)
    goto exit;

  /* Включаем генератор. */
  if (enable)
    {
      if (hyscan_generator_control_set_enable (control, board, enable))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
}

/* Функция возвращает максимальную длительность сигнала. */
gdouble
hyscan_generator_control_get_max_duration (HyScanGeneratorControl    *control,
                                           HyScanBoardType            board,
                                           HyScanGeneratorSignalType  signal)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;
  gdouble max_duration = -1.0;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), -1.0);

  priv = control->priv;

  if (priv->sonar == NULL)
    return -1.0;

  generator = g_hash_table_lookup (priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return -1.0;

  if (signal == HYSCAN_GENERATOR_SIGNAL_TONE)
    key_name = g_strdup_printf ("%s/tone/duration", generator->path);
  else if (signal != HYSCAN_GENERATOR_SIGNAL_LFM || signal != HYSCAN_GENERATOR_SIGNAL_LFMD)
    key_name = g_strdup_printf ("%s/lfm/duration", generator->path);
  else
    key_name = NULL;

  if (key_name != NULL)
    {
      if (!hyscan_data_schema_key_get_default_double (priv->schema, key_name, &max_duration))
        max_duration = -1.0;

      g_free (key_name);
    }

  return max_duration;
}

/* Функция включает тональный сигнал для излучения генератором. */
gboolean
hyscan_generator_control_set_tone (HyScanGeneratorControl *control,
                                   HyScanBoardType         board,
                                   gdouble                 frequency,
                                   gdouble                 duration,
                                   gdouble                 power)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gdouble dvalue;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  /* Временно отключаем генератор и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_generator_control_set_enable (control, board, FALSE))
    goto exit;

  key_name = g_strdup_printf ("%s/tone/frequency", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, frequency);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки частоты сигнала - 1Гц. */
  dvalue -= frequency;
  if (!status || ABS (dvalue) > 1.0)
    goto exit;

  key_name = g_strdup_printf ("%s/tone/duration", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, duration);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки длительности сигнала - 1мкс. */
  dvalue -= duration;
  if (!status || ABS (dvalue) > 0.000001)
    goto exit;

  key_name = g_strdup_printf ("%s/tone/power", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, power);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки энергии сигнала - 1%. */
  dvalue -= power;
  if (!status || ABS (dvalue) > 1.0)
    goto exit;

  /* Включаем генератор. */
  if (enable)
    {
      if (hyscan_generator_control_set_enable (control, board, enable))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
}

/* Функция включает линейно-частотно модулированный сигнал для излучения генератором. */
gboolean
hyscan_generator_control_set_lfm (HyScanGeneratorControl *control,
                                  HyScanBoardType         board,
                                  gboolean                decreasing,
                                  gdouble                 low_frequency,
                                  gdouble                 high_frequency,
                                  gdouble                 duration,
                                  gdouble                 power)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;

  gboolean rstatus = FALSE;
  gboolean status;
  gboolean enable;
  gchar *key_name;
  gboolean bvalue;
  gdouble dvalue;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  /* Временно отключаем генератор и устанавливаем его параметры. */
  key_name = g_strdup_printf ("%s/enable", generator->path);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &enable);
  g_free (key_name);

  if (!status)
    goto exit;

  if (enable && !hyscan_generator_control_set_enable (control, board, FALSE))
    goto exit;

  key_name = g_strdup_printf ("%s/lfm/decreasing", generator->path);
  hyscan_sonar_set_boolean (priv->sonar, key_name, decreasing);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &bvalue);
  g_free (key_name);

  if (!status || bvalue != decreasing)
    goto exit;

  key_name = g_strdup_printf ("%s/lfm/low-frequency", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, low_frequency);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки частоты сигнала - 1Гц. */
  dvalue -= low_frequency;
  if (!status || ABS (dvalue) > 1.0)
    goto exit;

  key_name = g_strdup_printf ("%s/lfm/high-frequency", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, high_frequency);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки частоты сигнала - 1Гц. */
  dvalue -= high_frequency;
  if (!status || ABS (dvalue) > 1.0)
    goto exit;

  key_name = g_strdup_printf ("%s/lfm/duration", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, duration);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки длительности сигнала - 1мкс. */
  dvalue -= duration;
  if (!status || ABS (dvalue) > 0.000001)
    goto exit;

  key_name = g_strdup_printf ("%s/lfm/power", generator->path);
  hyscan_sonar_set_double (priv->sonar, key_name, power);
  status = hyscan_sonar_get_double (priv->sonar, key_name, &dvalue);
  g_free (key_name);

  /* Точность установки энергии сигнала - 1%. */
  dvalue -= power;
  if (!status || ABS (dvalue) > 1.0)
    goto exit;

  /* Включаем генератор. */
  if (enable)
    {
      if (hyscan_generator_control_set_enable (control, board, enable))
        rstatus = TRUE;
    }
  else
    {
      rstatus = TRUE;
    }

exit:
  g_mutex_unlock (&priv->lock);

  return rstatus;
}

/* Функция включает или выключает формирование сигнала генератором. */
gboolean
hyscan_generator_control_set_enable (HyScanGeneratorControl *control,
                                     HyScanBoardType         board,
                                     gboolean                enable)
{
  HyScanGeneratorControlPrivate *priv;

  HyScanGeneratorControlGen *generator;
  gboolean is_enabled;
  gboolean status;
  gchar *key_name;

  g_return_val_if_fail (HYSCAN_IS_GENERATOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  generator = g_hash_table_lookup (priv->gens_by_board, GINT_TO_POINTER (board));
  if (generator == NULL)
    return FALSE;

  key_name = g_strdup_printf ("%s/enable", generator->path);
  hyscan_sonar_set_boolean (priv->sonar, key_name, enable);
  status = hyscan_sonar_get_boolean (priv->sonar, key_name, &is_enabled);
  g_free (key_name);

  if (!status || is_enabled != enable)
    return FALSE;

  return TRUE;
}
