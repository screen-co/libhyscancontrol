/*
 * \file hyscan-tvg-control-server.c
 *
 * \brief Исходный файл класса сервера управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-tvg-control-server.h"
#include "hyscan-tvg-control.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"
#include "hyscan-sonar-box.h"

enum
{
  PROP_0,
  PROP_PARAMS
};

enum
{
  SIGNAL_TVG_SET_AUTO,
  SIGNAL_TVG_SET_CONSTANT,
  SIGNAL_TVG_SET_LINEAR_DB,
  SIGNAL_TVG_SET_LOGARITHMIC,
  SIGNAL_TVG_SET_ENABLE,
  SIGNAL_LAST
};

typedef struct _HyScanTVGControlServerTVG HyScanTVGControlServerTVG;

/* Прототип функции выполнения операции. */
typedef gboolean (*hyscan_tvg_control_server_operation)                (HyScanTVGControlServer    *server,
                                                                        HyScanTVGControlServerTVG *tvg,
                                                                        const gchar *const        *names,
                                                                        GVariant                 **values);

/* Операция над системой ВАРУ. */
struct _HyScanTVGControlServerTVG
{
  HyScanSourceType                         source;                     /* Тип источника данных. */
  const gchar                             *name;                       /* Название источника данных. */
  hyscan_tvg_control_server_operation      func;                       /* Функция выполнения операции. */
};

struct _HyScanTVGControlServerPrivate
{
  HyScanDataBox                           *params;                     /* Параметры гидролокатора. */

  GHashTable                              *operations;                 /* Таблица возможных запросов. */
  GHashTable                              *paths;                      /* Таблица названий параметров запросов. */
  GHashTable                              *ids;                        /* Идентификаторы ВАРУ. */
};

static void        hyscan_tvg_control_server_set_property              (GObject                     *object,
                                                                        guint                        prop_id,
                                                                        const GValue                *value,
                                                                        GParamSpec                  *pspec);
static void        hyscan_tvg_control_server_object_constructed        (GObject                     *object);
static void        hyscan_tvg_control_server_object_finalize           (GObject                     *object);

static gboolean    hyscan_tvg_control_server_set_cb                    (HyScanDataBox               *params,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values,
                                                                        HyScanTVGControlServer      *server);

static gboolean    hyscan_tvg_control_server_set_auto                  (HyScanTVGControlServer      *server,
                                                                        HyScanTVGControlServerTVG   *tvg,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_tvg_control_server_set_constant              (HyScanTVGControlServer      *server,
                                                                        HyScanTVGControlServerTVG   *tvg,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_tvg_control_server_set_linear_db             (HyScanTVGControlServer      *server,
                                                                        HyScanTVGControlServerTVG   *tvg,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_tvg_control_server_set_logarithmic           (HyScanTVGControlServer      *server,
                                                                        HyScanTVGControlServerTVG   *tvg,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_tvg_control_server_set_enable                (HyScanTVGControlServer      *server,
                                                                        HyScanTVGControlServerTVG   *tvg,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);

static guint       hyscan_tvg_control_server_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTVGControlServer, hyscan_tvg_control_server, G_TYPE_OBJECT)

static void
hyscan_tvg_control_server_class_init (HyScanTVGControlServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_tvg_control_server_set_property;

  object_class->constructed = hyscan_tvg_control_server_object_constructed;
  object_class->finalize = hyscan_tvg_control_server_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_object ("params", "SonarParams", "Sonar parameters via HyScanSonarBox", HYSCAN_TYPE_SONAR_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_AUTO] =
    g_signal_new ("tvg-set-auto", HYSCAN_TYPE_TVG_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_DOUBLE_DOUBLE,
                  G_TYPE_BOOLEAN,
                  3, G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_CONSTANT] =
    g_signal_new ("tvg-set-constant", HYSCAN_TYPE_TVG_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_DOUBLE,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_INT, G_TYPE_DOUBLE);

  hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_LINEAR_DB] =
    g_signal_new ("tvg-set-linear-db", HYSCAN_TYPE_TVG_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_DOUBLE_DOUBLE,
                  G_TYPE_BOOLEAN,
                  3, G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_LOGARITHMIC] =
    g_signal_new ("tvg-set-logarithmic", HYSCAN_TYPE_TVG_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_DOUBLE_DOUBLE_DOUBLE,
                  G_TYPE_BOOLEAN,
                  4, G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_DOUBLE, G_TYPE_DOUBLE);

  hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_ENABLE] =
    g_signal_new ("tvg-set-enable", HYSCAN_TYPE_TVG_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_INT, G_TYPE_BOOLEAN);
}

static void
hyscan_tvg_control_server_init (HyScanTVGControlServer *server)
{
  server->priv = hyscan_tvg_control_server_get_instance_private (server);
}

static void
hyscan_tvg_control_server_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanTVGControlServer *server = HYSCAN_TVG_CONTROL_SERVER (object);
  HyScanTVGControlServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_PARAMS:
      priv->params = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_tvg_control_server_object_constructed (GObject *object)
{
  HyScanTVGControlServer *server = HYSCAN_TVG_CONTROL_SERVER (object);
  HyScanTVGControlServerPrivate *priv = server->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sources;

  gint64 version;
  gint64 id;
  gint i;

  priv->operations = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  priv->ids = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);

  /* Обязательно должны быть переданы параметры гидролокатора. */
  if (priv->params == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (priv->params, "/schema/id", &id))
    return;
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    return;
  if (!hyscan_data_box_get_integer (priv->params, "/schema/version", &version))
    return;
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    return;

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (priv->params));

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
          HyScanTVGControlServerTVG *operation;
          gchar *operation_path;

          gchar *param_names[3];
          GVariant *param_values[3];

          gint64 id;
          gint64 capabilities;

          gchar **pathv;
          HyScanSourceType source;

          gboolean status;

          /* Тип источника данных. */
          pathv = g_strsplit (sources->nodes[i]->path, "/", -1);
          source = hyscan_control_get_source_type (pathv[2]);
          g_strfreev (pathv);

          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          param_names[0] = g_strdup_printf ("%s/tvg/id", sources->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/tvg/capabilities", sources->nodes[i]->path);
          param_names[2] = NULL;

          status = hyscan_data_box_get (HYSCAN_DATA_BOX (priv->params), (const gchar **)param_names, param_values);

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

          /* Команда - hyscan_tvg_control_set_auto. */
          if (capabilities & HYSCAN_TVG_MODE_AUTO)
            {
              operation = g_new0 (HyScanTVGControlServerTVG, 1);
              operation->source = source;
              operation->name = hyscan_control_get_source_name (source);
              operation->func = hyscan_tvg_control_server_set_auto;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sources/%s/tvg/auto/level", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
              operation_path = g_strdup_printf ("/sources/%s/tvg/auto/sensitivity", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_tvg_control_set_constant. */
          if (capabilities & HYSCAN_TVG_MODE_CONSTANT)
            {
              operation = g_new0 (HyScanTVGControlServerTVG, 1);
              operation->source = source;
              operation->name = hyscan_control_get_source_name (source);
              operation->func = hyscan_tvg_control_server_set_constant;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sources/%s/tvg/constant/gain", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_tvg_control_set_linear. */
          if (capabilities & HYSCAN_TVG_MODE_LINEAR_DB)
            {
              operation = g_new0 (HyScanTVGControlServerTVG, 1);
              operation->source = source;
              operation->name = hyscan_control_get_source_name (source);
              operation->func = hyscan_tvg_control_server_set_linear_db;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sources/%s/tvg/linear-db/gain0", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
              operation_path = g_strdup_printf ("/sources/%s/tvg/linear-db/step", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_tvg_control_set_logarithmic. */
          if (capabilities & HYSCAN_TVG_MODE_LOGARITHMIC)
            {
              operation = g_new0 (HyScanTVGControlServerTVG, 1);
              operation->source = source;
              operation->name = hyscan_control_get_source_name (source);
              operation->func = hyscan_tvg_control_server_set_logarithmic;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sources/%s/tvg/logarithmic/alpha", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
              operation_path = g_strdup_printf ("/sources/%s/tvg/logarithmic/beta", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
              operation_path = g_strdup_printf ("/sources/%s/tvg/logarithmic/gain0", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);
            }

          /* Команда - hyscan_tvg_control_set_enable. */
          if (capabilities)
            {
              operation = g_new0 (HyScanTVGControlServerTVG, 1);
              operation->source = source;
              operation->name = hyscan_control_get_source_name (source);
              operation->func = hyscan_tvg_control_server_set_enable;
              g_hash_table_insert (priv->operations, operation, operation);

              operation_path = g_strdup_printf ("/sources/%s/tvg/enable", operation->name);
              g_hash_table_insert (priv->paths, operation_path, operation);

              /* Идентификатор ВАРУ. */
              g_hash_table_insert (priv->ids, GINT_TO_POINTER (source), GINT_TO_POINTER (id));
            }
        }

      g_signal_connect (priv->params, "set",
                        G_CALLBACK (hyscan_tvg_control_server_set_cb), server);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_tvg_control_server_object_finalize (GObject *object)
{
  HyScanTVGControlServer *server = HYSCAN_TVG_CONTROL_SERVER (object);
  HyScanTVGControlServerPrivate *priv = server->priv;

  g_signal_handlers_disconnect_by_data (priv->params, server);

  g_hash_table_unref (priv->ids);
  g_hash_table_unref (priv->paths);
  g_hash_table_unref (priv->operations);

  G_OBJECT_CLASS (hyscan_tvg_control_server_parent_class)->finalize (object);
}

/* Функция - обработчик параметров. */
static gboolean
hyscan_tvg_control_server_set_cb (HyScanDataBox           *params,
                                  const gchar *const      *names,
                                  GVariant               **values,
                                  HyScanTVGControlServer  *server)
{
  HyScanTVGControlServerTVG *tvg0;
  HyScanTVGControlServerTVG *tvgn;
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  if (n_names == 0)
    return FALSE;

  tvg0 = g_hash_table_lookup (server->priv->paths, names[0]);
  if (tvg0 == NULL)
    return TRUE;

  /* Все параметры, относящиеся к одному запросу, должны быть
   * в таблице и указывать на одну структуру с описанием операции.
   * Параметры должны относиться только к одному запроса. */
  for (i = 1; i < n_names; i++)
    {
      tvgn = g_hash_table_lookup (server->priv->paths, names[i]);

      if (tvg0 != tvgn)
        return FALSE;

      if (tvg0->source != tvgn->source)
        return FALSE;
    }

  return tvg0->func (server, tvg0, names, values);
}

/* Команда - hyscan_tvg_control_set_auto. */
static gboolean
hyscan_tvg_control_server_set_auto (HyScanTVGControlServer      *server,
                                    HyScanTVGControlServerTVG   *tvg,
                                    const gchar *const          *names,
                                    GVariant                   **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gdouble level;
  gdouble sensitivity;

  name = g_strdup_printf ("/sources/%s/tvg/auto/level", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &level);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/sources/%s/tvg/auto/sensitivity", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &sensitivity);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_AUTO], 0,
                 tvg->source, level, sensitivity, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_tvg_control_set_constant. */
static gboolean
hyscan_tvg_control_server_set_constant (HyScanTVGControlServer      *server,
                                        HyScanTVGControlServerTVG   *tvg,
                                        const gchar *const          *names,
                                        GVariant                   **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gdouble gain;

  name = g_strdup_printf ("/sources/%s/tvg/constant/gain", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &gain);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_CONSTANT], 0,
                 tvg->source, gain, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_tvg_control_set_linear. */
static gboolean
hyscan_tvg_control_server_set_linear_db (HyScanTVGControlServer      *server,
                                         HyScanTVGControlServerTVG   *tvg,
                                         const gchar *const          *names,
                                         GVariant                   **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gdouble gain0;
  gdouble step;

  name = g_strdup_printf ("/sources/%s/tvg/linear-db/gain0", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &gain0);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/sources/%s/tvg/linear-db/step", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &step);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_LINEAR_DB], 0,
                 tvg->source, gain0, step, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_tvg_control_set_logarithmic. */
static gboolean
hyscan_tvg_control_server_set_logarithmic (HyScanTVGControlServer      *server,
                                           HyScanTVGControlServerTVG   *tvg,
                                           const gchar *const          *names,
                                           GVariant                   **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gdouble gain0;
  gdouble beta;
  gdouble alpha;

  name = g_strdup_printf ("/sources/%s/tvg/logarithmic/gain0", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &gain0);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/sources/%s/tvg/logarithmic/beta", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &beta);
  g_free (name);

  if (!status)
    return FALSE;

  name = g_strdup_printf ("/sources/%s/tvg/logarithmic/alpha", tvg->name);
  status = hyscan_control_find_double_param (name, names, values, &alpha);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_LOGARITHMIC], 0,
                 tvg->source, gain0, beta, alpha, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_tvg_control_set_enable. */
static gboolean
hyscan_tvg_control_server_set_enable (HyScanTVGControlServer      *server,
                                      HyScanTVGControlServerTVG   *tvg,
                                      const gchar *const          *names,
                                      GVariant                   **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gboolean enable;

  name = g_strdup_printf ("/sources/%s/tvg/enable", tvg->name);
  status = hyscan_control_find_boolean_param (name, names, values, &enable);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_tvg_control_server_signals[SIGNAL_TVG_SET_ENABLE], 0,
                 tvg->source, enable, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}
/* Функция создаёт новый объект HyScanTVGControlServer. */
HyScanTVGControlServer *
hyscan_tvg_control_server_new (HyScanSonarBox *params)
{
  return g_object_new (HYSCAN_TYPE_TVG_CONTROL_SERVER, "params", params, NULL);
}

/* Функция передаёт параметры системы ВАРУ. */
void
hyscan_tvg_control_server_send_gains (HyScanTVGControlServer *server,
                                      HyScanSourceType        source,
                                      HyScanDataWriterTVG    *tvg)
{
  HyScanSonarMessage message;
  gpointer id;

  g_return_if_fail (HYSCAN_IS_TVG_CONTROL_SERVER (server));

  if (server->priv->params == NULL)
    return;

  id = g_hash_table_lookup (server->priv->ids, GINT_TO_POINTER (source));
  if (id == NULL)
    return;

  message.time = tvg->time;
  message.id   = GPOINTER_TO_INT (id);
  message.type = HYSCAN_DATA_FLOAT;
  message.rate = tvg->rate;
  message.size = tvg->n_gains * hyscan_data_get_point_size (HYSCAN_DATA_FLOAT);
  message.data = tvg->gains;

  hyscan_sonar_box_send (HYSCAN_SONAR_BOX (server->priv->params), &message);
}
