
#include "hyscan-sonar-control-server.h"
#include "hyscan-sonar-control.h"
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
  SIGNAL_SONAR_SET_SYNC_TYPE,
  SIGNAL_SONAR_ENABLE_RAW_DATA,
  SIGNAL_SONAR_SET_RECEIVE_TIME,
  SIGNAL_SONAR_START,
  SIGNAL_SONAR_STOP,
  SIGNAL_SONAR_PING,
  SIGNAL_LAST
};

typedef struct _HyScanSonarControlServerCtl HyScanSonarControlServerCtl;

/* Прототип функции выполнения операции. */
typedef gboolean (*hyscan_sonar_control_server_operation)              (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);

/* Операция над бортами гидролокатора. */
struct _HyScanSonarControlServerCtl
{
  gint                                     board;                      /* Тип борта. */
  const gchar                             *name;                       /* Название борта. */
  hyscan_sonar_control_server_operation    func;                       /* Функция выполнения операции. */
};

struct _HyScanSonarControlServerPrivate
{
  HyScanDataBox                           *params;                     /* Параметры гидролокатора. */
  gulong                                   signal_id;                  /* Идентификатор обработчика сигнала set. */

  GHashTable                              *operations;                 /* Таблица возможных запросов. */
  GHashTable                              *paths;                      /* Таблица названий параметров запросов. */
};

static void        hyscan_sonar_control_server_set_property            (GObject                     *object,
                                                                        guint                        prop_id,
                                                                        const GValue                *value,
                                                                        GParamSpec                  *pspec);
static void        hyscan_sonar_control_server_object_constructed      (GObject                     *object);
static void        hyscan_sonar_control_server_object_finalize         (GObject                     *object);

static gboolean    hyscan_sonar_control_server_set_cb                  (HyScanDataBox               *params,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values,
                                                                        HyScanSonarControlServer    *server);

static gboolean    hyscan_sonar_control_server_set_sync_type           (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_sonar_control_server_enable_raw_data         (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_sonar_control_server_set_receive_time        (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_sonar_control_server_start_stop              (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);
static gboolean    hyscan_sonar_control_server_ping                    (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);

static guint       hyscan_sonar_control_server_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarControlServer, hyscan_sonar_control_server, HYSCAN_TYPE_TVG_CONTROL_SERVER)

static void
hyscan_sonar_control_server_class_init (HyScanSonarControlServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_control_server_set_property;

  object_class->constructed = hyscan_sonar_control_server_object_constructed;
  object_class->finalize = hyscan_sonar_control_server_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_object ("params", "SonarParams", "Sonar parameters via HyScanDataBox", HYSCAN_TYPE_DATA_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_SET_SYNC_TYPE] =
    g_signal_new ("sonar-set-sync-type", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT64,
                  G_TYPE_BOOLEAN,
                  1, G_TYPE_INT64);

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_ENABLE_RAW_DATA] =
    g_signal_new ("sonar-enable-raw-data", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__BOOLEAN,
                  G_TYPE_BOOLEAN,
                  1, G_TYPE_BOOLEAN);

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_SET_RECEIVE_TIME] =
    g_signal_new ("sonar-set-receive-time", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT_DOUBLE,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_INT, G_TYPE_DOUBLE);

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_START] =
    g_signal_new ("sonar-start", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__STRING_STRING,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_STRING, G_TYPE_STRING);

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_STOP] =
    g_signal_new ("sonar-stop", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_PING] =
    g_signal_new ("sonar-ping", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__VOID,
                  G_TYPE_BOOLEAN, 0);
}

static void
hyscan_sonar_control_server_init (HyScanSonarControlServer *server)
{
  server->priv = hyscan_sonar_control_server_get_instance_private (server);
}

static void
hyscan_sonar_control_server_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  HyScanSonarControlServer *server = HYSCAN_SONAR_CONTROL_SERVER (object);
  HyScanSonarControlServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_PARAMS:
      G_OBJECT_CLASS (hyscan_sonar_control_server_parent_class)->set_property (object, prop_id, value, pspec);
      priv->params = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_control_server_object_constructed (GObject *object)
{
  HyScanSonarControlServer *server = HYSCAN_SONAR_CONTROL_SERVER (object);
  HyScanSonarControlServerPrivate *priv = server->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *boards;

  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_sonar_control_server_parent_class)->constructed (object);

  priv->operations = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (priv->params, "/schema/id", &id))
    {
      g_warning ("HyScanSonarControlServer: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_warning ("HyScanSonarControlServer: sonar schema id mismatch");
      return;
    }
  if (!hyscan_data_box_get_integer (priv->params, "/schema/version", &version))
    {
      g_warning ("HyScanSonarControlServer: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_warning ("HyScanSonarControlServer: sonar schema version mismatch");
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
      HyScanSonarControlServerCtl *operation;
      gchar *operation_path;

      /* Считываем описания бортов. */
      for (i = 0; i < boards->n_nodes; i++)
        {
          gchar **pathv;
          gint board;

          /* Тип борта гидролокатора. */
          pathv = g_strsplit (boards->nodes[i]->path, "/", -1);
          board = hyscan_control_get_board_type (pathv[2]);
          g_strfreev (pathv);

          if (board == HYSCAN_BOARD_INVALID)
            continue;

          /* Команда - hyscan_sonar_control_set_receive_time. */
          operation = g_new0 (HyScanSonarControlServerCtl, 1);
          operation->board = board;
          operation->name = hyscan_control_get_board_name (board);
          operation->func = hyscan_sonar_control_server_set_receive_time;
          g_hash_table_insert (priv->operations, operation, operation);

          operation_path = g_strdup_printf ("/boards/%s/control/receive-time", operation->name);
          g_hash_table_insert (priv->paths, operation_path, operation);
        }

      /* Команда - hyscan_sonar_control_set_sync_type. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->board = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_set_sync_type;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/sync/type");
      g_hash_table_insert (priv->paths, operation_path, operation);

      /* Команда - hyscan_sonar_control_enable_raw_data. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->board = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_enable_raw_data;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/control/raw-data");
      g_hash_table_insert (priv->paths, operation_path, operation);

      /* Команды - hyscan_sonar_control_start / hyscan_sonar_control_stop. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->board = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_start_stop;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/control/enable");
      g_hash_table_insert (priv->paths, operation_path, operation);
      operation_path = g_strdup ("/control/project-name");
      g_hash_table_insert (priv->paths, operation_path, operation);
      operation_path = g_strdup ("/control/track-name");
      g_hash_table_insert (priv->paths, operation_path, operation);

      /* Команда - hyscan_sonar_control_ping. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->board = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_ping;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/sync/ping");
      g_hash_table_insert (priv->paths, operation_path, operation);

      priv->signal_id = g_signal_connect (priv->params,
                                          "set",
                                          G_CALLBACK (hyscan_sonar_control_server_set_cb),
                                          server);
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sonar_control_server_object_finalize (GObject *object)
{
  HyScanSonarControlServer *server = HYSCAN_SONAR_CONTROL_SERVER (object);
  HyScanSonarControlServerPrivate *priv = server->priv;

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->params, priv->signal_id);

  g_hash_table_unref (priv->paths);
  g_hash_table_unref (priv->operations);

  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_sonar_control_server_parent_class)->finalize (object);
}

/* Функция - обработчик параметров. */
static gboolean
hyscan_sonar_control_server_set_cb (HyScanDataBox             *params,
                                    const gchar *const        *names,
                                    GVariant                 **values,
                                    HyScanSonarControlServer  *server)
{
  HyScanSonarControlServerCtl *ctl0;
  HyScanSonarControlServerCtl *ctln;
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  if (n_names == 0)
    return FALSE;

  ctl0 = g_hash_table_lookup (server->priv->paths, names[0]);
  if (ctl0 == NULL)
    return TRUE;

  /* Все параметры, относящиеся к одному запросу, должны быть
   * в таблице и указывать на одну структуру с описанием операции.
   * Параметры должны относиться только к одному запроса. */
  for (i = 1; i < n_names; i++)
    {
      ctln = g_hash_table_lookup (server->priv->paths, names[i]);

      if (ctl0 != ctln)
        return FALSE;

      if (ctl0->board != ctln->board)
        return FALSE;
    }

  return ctl0->func (server, ctl0, names, values);
}

/* Команда - hyscan_sonar_control_set_sync_type. */
static gboolean
hyscan_sonar_control_server_set_sync_type (HyScanSonarControlServer     *server,
                                           HyScanSonarControlServerCtl  *ctl,
                                           const gchar *const           *names,
                                           GVariant                    **values)
{
  gboolean cancel;
  gint64 sync_type;

  if (!hyscan_control_find_integer_param ("/sync/type", names, values, &sync_type))
    return FALSE;

  g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_SET_SYNC_TYPE], 0,
                 sync_type, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_sonar_control_enable_raw_data. */
static gboolean
hyscan_sonar_control_server_enable_raw_data (HyScanSonarControlServer     *server,
                                             HyScanSonarControlServerCtl  *ctl,
                                             const gchar *const           *names,
                                             GVariant                    **values)
{
  gboolean cancel;
  gboolean enable;

  if (!hyscan_control_find_boolean_param ("/control/raw-data", names, values, &enable))
    return FALSE;

  g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_ENABLE_RAW_DATA], 0,
                 enable, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_sonar_control_set_receive_time. */
static gboolean
hyscan_sonar_control_server_set_receive_time (HyScanSonarControlServer     *server,
                                              HyScanSonarControlServerCtl  *ctl,
                                              const gchar *const           *names,
                                              GVariant                    **values)
{
  gchar *name = NULL;
  gboolean status;
  gboolean cancel;

  gdouble receive_time;

  name = g_strdup_printf ("/boards/%s/control/receive-time", ctl->name);
  status = hyscan_control_find_double_param (name, names, values, &receive_time);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_SET_RECEIVE_TIME], 0,
                 receive_time, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команды - hyscan_sonar_control_start / hyscan_sonar_control_stop. */
static gboolean
hyscan_sonar_control_server_start_stop (HyScanSonarControlServer     *server,
                                        HyScanSonarControlServerCtl  *ctl,
                                        const gchar *const           *names,
                                        GVariant                    **values)
{
  gboolean cancel;
  gboolean enable;
  const gchar *project_name;
  const gchar *track_name;

  if (!hyscan_control_find_boolean_param ("/control/enable", names, values, &enable))
    return FALSE;

  if (enable)
    {
      project_name = hyscan_control_find_string_param ("/control/project-name", names, values);
      track_name = hyscan_control_find_string_param ("/control/track-name", names, values);

      g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_START], 0,
                     project_name, track_name, &cancel);
    }
  else
    {
      g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_STOP], 0, &cancel);
    }

  if (cancel)
    return FALSE;

  return TRUE;
}

/* Команда - hyscan_sonar_control_ping. */
static gboolean
hyscan_sonar_control_server_ping (HyScanSonarControlServer     *server,
                                  HyScanSonarControlServerCtl  *ctl,
                                  const gchar *const           *names,
                                  GVariant                    **values)
{
  gboolean cancel;
  gboolean ping;

  if (!hyscan_control_find_boolean_param ("/sync/ping", names, values, &ping))
    return FALSE;

  if (ping)
    g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_PING], 0, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

#warning "Add implementation"
void
hyscan_sonar_control_server_send_raw_data (HyScanSonarControlServer *server,
                                           gint64                    time,
                                           HyScanBoardType           board,
                                           gint                      channel,
                                           guint32                   size,
                                           gpointer                  data)
{

}
