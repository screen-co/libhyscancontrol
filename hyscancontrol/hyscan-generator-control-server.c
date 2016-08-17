/*
 * \file hyscan-generator-control-server.c
 *
 * \brief Исходный файл класса сервера управления генераторами сигналов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-generator-control-server.h"
#include "hyscan-generator-control.h"
#include "hyscan-control-common.h"
#include "hyscan-marshallers.h"
#include <hyscan-sonar-box.h>

enum
{
  PROP_0,
  PROP_PARAMS
};

enum
{
  SIGNAL_GENERATOR_SET_PRESET,
  SIGNAL_GENERATOR_SET_AUTO,
  SIGNAL_GENERATOR_SET_SIMPLE,
  SIGNAL_GENERATOR_SET_EXTENDED,
  SIGNAL_GENERATOR_SET_ENABLE,
  SIGNAL_LAST
};

typedef struct _HyScanGeneratorControlServerGen HyScanGeneratorControlServerGen;

/* Прототип функции выполнения операции. */
typedef gboolean (*hyscan_generator_control_server_operation)            (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *port,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);

/* Операция над генератором. */
struct _HyScanGeneratorControlServerGen
{
  gint                                      board;                       /* Тип борта. */
  const gchar                              *name;                        /* Название борта. */
  hyscan_generator_control_server_operation func;                        /* Функция выполнения операции. */
};

struct _HyScanGeneratorControlServerPrivate
{
  HyScanDataBox                            *params;                      /* Параметры гидролокатора. */
  gulong                                    signal_id;                   /* Идентификатор обработчика сигнала set. */

  GHashTable                               *operations;                  /* Таблица возможных запросов. */
  GHashTable                               *paths;                       /* Таблица названий параметров запросов. */
  GHashTable                               *ids;                         /* Идентификаторы генераторов. */
};

static void        hyscan_generator_control_server_set_property          (GObject                         *object,
                                                                          guint                            prop_id,
                                                                          const GValue                    *value,
                                                                          GParamSpec                      *pspec);
static void        hyscan_generator_control_server_object_constructed    (GObject                         *object);
static void        hyscan_generator_control_server_object_finalize       (GObject                         *object);

static gboolean    hyscan_generator_control_server_set_cb                (HyScanDataBox                   *params,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values,
                                                                          HyScanGeneratorControlServer    *server);

static gboolean    hyscan_generator_control_server_set_preset            (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *gen,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);
static gboolean    hyscan_generator_control_server_set_auto              (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *gen,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);
static gboolean    hyscan_generator_control_server_set_simple            (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *gen,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);
static gboolean    hyscan_generator_control_server_set_tone              (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *gen,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);
static gboolean    hyscan_generator_control_server_set_lfm               (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *gen,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);
static gboolean    hyscan_generator_control_server_set_enable            (HyScanGeneratorControlServer    *server,
                                                                          HyScanGeneratorControlServerGen *gen,
                                                                          const gchar *const              *names,
                                                                          GVariant                       **values);

static guint       hyscan_generator_control_server_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGeneratorControlServer, hyscan_generator_control_server, HYSCAN_TYPE_SENSOR_CONTROL_SERVER)

static void
hyscan_generator_control_server_class_init (HyScanGeneratorControlServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_generator_control_server_set_property;

  object_class->constructed = hyscan_generator_control_server_object_constructed;
  object_class->finalize = hyscan_generator_control_server_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_object ("params", "SonarParams", "Sonar parameters via HyScanDataBox", HYSCAN_TYPE_DATA_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_PRESET] =
    g_signal_new ("generator-set-preset", HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_INT64,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_INT, G_TYPE_INT64);

  hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_AUTO] =
    g_signal_new ("generator-set-auto", HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_INT64,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_INT, G_TYPE_INT64);

  hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_SIMPLE] =
    g_signal_new ("generator-set-simple", HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_INT64_DOUBLE,
                  G_TYPE_BOOLEAN,
                  3, G_TYPE_INT, G_TYPE_INT64, G_TYPE_DOUBLE);

  hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_EXTENDED] =
    g_signal_new ("generator-set-extended", HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_INT64_DOUBLE_DOUBLE,
                  G_TYPE_BOOLEAN,
                  4, G_TYPE_INT, G_TYPE_INT64, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_ENABLE] =
    g_signal_new ("generator-set-enable", HYSCAN_TYPE_GENERATOR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_INT, G_TYPE_BOOLEAN);
}

static void
hyscan_generator_control_server_init (HyScanGeneratorControlServer *server)
{
  server->priv = hyscan_generator_control_server_get_instance_private (server);
}

static void
hyscan_generator_control_server_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  HyScanGeneratorControlServer *server = HYSCAN_GENERATOR_CONTROL_SERVER (object);
  HyScanGeneratorControlServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_PARAMS:
      G_OBJECT_CLASS (hyscan_generator_control_server_parent_class)->set_property (object, prop_id, value, pspec);
      priv->params = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_generator_control_server_object_constructed (GObject *object)
{
  HyScanGeneratorControlServer *server = HYSCAN_GENERATOR_CONTROL_SERVER (object);
  HyScanGeneratorControlServerPrivate *priv = server->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *boards;

  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_generator_control_server_parent_class)->constructed (object);

  priv->operations = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  priv->ids = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (priv->params, "/schema/id", &id))
    {
      g_warning ("HyScanGeneratorControlServer: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_warning ("HyScanGeneratorControlServer: sonar schema id mismatch");
      return;
    }
  if (!hyscan_data_box_get_integer (priv->params, "/schema/version", &version))
    {
      g_warning ("HyScanGeneratorControlServer: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_warning ("HyScanGeneratorControlServer: sonar schema version mismatch");
      return;
    }

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (priv->params));

  /* Ветка схемы с описанием портов - "/boards". */
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
          HyScanGeneratorControlServerGen *operation;
          gchar *operation_path;

          gchar *param_names[4];
          GVariant *param_values[4];

          gchar **pathv;
          gint board;

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

          status = hyscan_data_box_get (HYSCAN_DATA_BOX (priv->params), (const gchar **)param_names, param_values);

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

          /* Команда - hyscan_generator_control_set_preset. */
          if (capabilities & HYSCAN_GENERATOR_MODE_PRESET)
            {
              operation = g_new0 (HyScanGeneratorControlServerGen, 1);
              operation->board = board;
              operation->name = hyscan_control_get_board_name (board);
              operation->func = hyscan_generator_control_server_set_preset;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/boards/%s/generator/preset/id", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_generator_control_set_auto. */
          if (capabilities & HYSCAN_GENERATOR_MODE_AUTO)
            {
              operation = g_new0 (HyScanGeneratorControlServerGen, 1);
              operation->board = board;
              operation->name = hyscan_control_get_board_name (board);
              operation->func = hyscan_generator_control_server_set_auto;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/boards/%s/generator/auto/signal", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_generator_control_set_simple. */
          if (capabilities & HYSCAN_GENERATOR_MODE_SIMPLE)
            {
              operation = g_new0 (HyScanGeneratorControlServerGen, 1);
              operation->board = board;
              operation->name = hyscan_control_get_board_name (board);
              operation->func = hyscan_generator_control_server_set_simple;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/boards/%s/generator/simple/signal", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
              operation_path = g_strdup_printf ("/boards/%s/generator/simple/power", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          if (capabilities & HYSCAN_GENERATOR_MODE_EXTENDED)
            {
              /* Команда - hyscan_generator_control_set_tone. */
              if (signals & HYSCAN_GENERATOR_SIGNAL_TONE)
                {
                  operation = g_new0 (HyScanGeneratorControlServerGen, 1);
                  operation->board = board;
                  operation->name = hyscan_control_get_board_name (board);
                  operation->func = hyscan_generator_control_server_set_tone;
                  g_hash_table_insert (priv->operations, operation, operation);

                  operation_path = g_strdup_printf ("/boards/%s/generator/tone/duration", operation->name);
                  g_hash_table_insert (priv->paths, operation_path, operation);
                  operation_path = g_strdup_printf ("/boards/%s/generator/tone/power", operation->name);
                  g_hash_table_insert (priv->paths, operation_path, operation);
                }

              /* Команда - hyscan_generator_control_set_lfm. */
              if ((signals & HYSCAN_GENERATOR_SIGNAL_LFM) || (signals & HYSCAN_GENERATOR_SIGNAL_LFMD))
                {
                  operation = g_new0 (HyScanGeneratorControlServerGen, 1);
                  operation->board = board;
                  operation->name = hyscan_control_get_board_name (board);
                  operation->func = hyscan_generator_control_server_set_lfm;
                  g_hash_table_insert (priv->operations, operation, operation);

                  operation_path = g_strdup_printf ("/boards/%s/generator/lfm/decreasing", operation->name);
                  g_hash_table_insert (priv->paths, operation_path, operation);
                  operation_path = g_strdup_printf ("/boards/%s/generator/lfm/duration", operation->name);
                  g_hash_table_insert (priv->paths, operation_path, operation);
                  operation_path = g_strdup_printf ("/boards/%s/generator/lfm/power", operation->name);
                  g_hash_table_insert (priv->paths, operation_path, operation);
                }
            }

          if (capabilities)
            {
              /* Команда - hyscan_generator_control_set_enable. */
              operation = g_new0 (HyScanGeneratorControlServerGen, 1);
              operation->board = board;
              operation->name = hyscan_control_get_board_name (board);
              operation->func = hyscan_generator_control_server_set_enable;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/boards/%s/generator/enable", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);

              /* Идентификатор генератора. */
              g_hash_table_insert (priv->ids, GINT_TO_POINTER (board), GINT_TO_POINTER (id));
}
        }

      priv->signal_id = g_signal_connect (priv->params,
                                          "set",
                                          G_CALLBACK (hyscan_generator_control_server_set_cb),
                                          server);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_generator_control_server_object_finalize (GObject *object)
{
  HyScanGeneratorControlServer *server = HYSCAN_GENERATOR_CONTROL_SERVER (object);
  HyScanGeneratorControlServerPrivate *priv = server->priv;

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->params, priv->signal_id);

  g_hash_table_unref (priv->ids);
  g_hash_table_unref (priv->paths);
  g_hash_table_unref (priv->operations);

  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_generator_control_server_parent_class)->finalize (object);
}

/* Функция - обработчик параметров. */
static gboolean
hyscan_generator_control_server_set_cb (HyScanDataBox                 *params,
                                        const gchar *const            *names,
                                        GVariant                     **values,
                                        HyScanGeneratorControlServer  *server)
{
  HyScanGeneratorControlServerGen *gen0;
  HyScanGeneratorControlServerGen *genn;
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  if (n_names == 0)
    return FALSE;

  gen0 = g_hash_table_lookup (server->priv->paths, names[0]);
  if (gen0 == NULL)
    return TRUE;

  /* Все параметры, относящиеся к одному запросу, должны быть
   * в таблице и указывать на одну структуру с описанием операции.
   * Параметры должны относиться только к одному запроса. */
  for (i = 1; i < n_names; i++)
    {
      genn = g_hash_table_lookup (server->priv->paths, names[i]);

      if (gen0 != genn)
        return FALSE;

      if (gen0->board != genn->board)
        return FALSE;
    }

  return gen0->func (server, gen0, names, values);
}

/* Команда - hyscan_generator_control_set_preset. */
static gboolean
hyscan_generator_control_server_set_preset (HyScanGeneratorControlServer    *server,
                                            HyScanGeneratorControlServerGen *gen,
                                            const gchar *const              *names,
                                            GVariant                       **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gint64 preset;

  name = g_strdup_printf ("/boards/%s/generator/preset/id", gen->name);
  status = hyscan_control_find_integer_param (name, names, values, &preset);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_PRESET], 0,
                 gen->board, preset, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_generator_control_set_auto. */
static gboolean
hyscan_generator_control_server_set_auto (HyScanGeneratorControlServer    *server,
                                          HyScanGeneratorControlServerGen *gen,
                                          const gchar *const              *names,
                                          GVariant                       **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gint64 signal;

  name = g_strdup_printf ("/boards/%s/generator/auto/signal", gen->name);
  status = hyscan_control_find_integer_param (name, names, values, &signal);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_AUTO], 0,
                 gen->board, signal, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_generator_control_set_simple. */
static gboolean
hyscan_generator_control_server_set_simple (HyScanGeneratorControlServer    *server,
                                            HyScanGeneratorControlServerGen *gen,
                                            const gchar *const              *names,
                                            GVariant                       **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gint64 signal;
  gdouble power;

  name = g_strdup_printf ("/boards/%s/generator/simple/signal", gen->name);
  status = hyscan_control_find_integer_param (name, names, values, &signal);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/boards/%s/generator/simple/power", gen->name);
  status = hyscan_control_find_double_param (name, names, values, &power);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_SIMPLE], 0,
                 gen->board, signal, power, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_generator_control_set_tone. */
static gboolean
hyscan_generator_control_server_set_tone (HyScanGeneratorControlServer    *server,
                                          HyScanGeneratorControlServerGen *gen,
                                          const gchar *const              *names,
                                          GVariant                       **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gint signal;
  gdouble duration;
  gdouble power;

  name = g_strdup_printf ("/boards/%s/generator/tone/duration", gen->name);
  status = hyscan_control_find_double_param (name, names, values, &duration);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/boards/%s/generator/tone/power", gen->name);
  status = hyscan_control_find_double_param (name, names, values, &power);
  g_free (name);

  if (!status)
    return FALSE;

  signal = HYSCAN_GENERATOR_SIGNAL_TONE;

  g_signal_emit (server, hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_EXTENDED], 0,
                 gen->board, signal, duration, power, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_generator_control_set_lfm. */
static gboolean
hyscan_generator_control_server_set_lfm (HyScanGeneratorControlServer    *server,
                                         HyScanGeneratorControlServerGen *gen,
                                         const gchar *const              *names,
                                         GVariant                       **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gboolean decreasing;
  gint signal;
  gdouble duration;
  gdouble power;

  name = g_strdup_printf ("/boards/%s/generator/lfm/decreasing", gen->name);
  status = hyscan_control_find_boolean_param (name, names, values, &decreasing);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/boards/%s/generator/lfm/duration", gen->name);
  status = hyscan_control_find_double_param (name, names, values, &duration);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/boards/%s/generator/lfm/power", gen->name);
  status = hyscan_control_find_double_param (name, names, values, &power);
  g_free (name);

  if (!status)
    return FALSE;

  if (decreasing)
    signal = HYSCAN_GENERATOR_SIGNAL_LFMD;
  else
    signal = HYSCAN_GENERATOR_SIGNAL_LFM;

  g_signal_emit (server, hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_EXTENDED], 0,
                 gen->board, signal, duration, power, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_generator_control_set_enable. */
static gboolean
hyscan_generator_control_server_set_enable (HyScanGeneratorControlServer    *server,
                                            HyScanGeneratorControlServerGen *gen,
                                            const gchar *const              *names,
                                            GVariant                       **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gboolean enable;

  name = g_strdup_printf ("/boards/%s/generator/enable", gen->name);
  status = hyscan_control_find_boolean_param (name, names, values, &enable);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_generator_control_server_signals[SIGNAL_GENERATOR_SET_ENABLE], 0,
                 gen->board, enable, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Функция передаёт образы излучаемых сигналов. */
void
hyscan_generator_control_server_send_signal (HyScanGeneratorControlServer  *server,
                                             gint64                         time,
                                             HyScanBoardType                board,
                                             gfloat                         rate,
                                             guint32                        size,
                                             gpointer                       signal)
{
  gpointer id;

  g_return_if_fail (HYSCAN_IS_GENERATOR_CONTROL_SERVER (server));

  id = g_hash_table_lookup (server->priv->ids, GINT_TO_POINTER (board));
  if (id == NULL)
    return;

  hyscan_sonar_box_send (HYSCAN_SONAR_BOX (server->priv->params),
                         time, GPOINTER_TO_INT (id), HYSCAN_DATA_COMPLEX_FLOAT, rate, size, signal);
}
