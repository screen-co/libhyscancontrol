/*
 * \file hyscan-sonar-control-server.c
 *
 * \brief Исходный файл класса сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-control-server.h"
#include "hyscan-sonar-control.h"
#include "hyscan-control-common.h"
#include "hyscan-marshallers.h"
#include "hyscan-sonar-box.h"

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
  SIGNAL_SONAR_ALIVE_TIMEOUT,
  SIGNAL_LAST
};

typedef struct _HyScanSonarControlServerCtl HyScanSonarControlServerCtl;

/* Прототип функции выполнения операции. */
typedef gboolean (*hyscan_sonar_control_server_operation)              (HyScanSonarControlServer    *server,
                                                                        HyScanSonarControlServerCtl *ctl,
                                                                        const gchar *const          *names,
                                                                        GVariant                   **values);

/* Операция над источниками данных. */
struct _HyScanSonarControlServerCtl
{
  HyScanSourceType                         source;                     /* Тип источника данных. */
  const gchar                             *name;                       /* Название источника данных. */
  hyscan_sonar_control_server_operation    func;                       /* Функция выполнения операции. */
};

struct _HyScanSonarControlServerPrivate
{
  HyScanDataBox                           *params;                     /* Параметры гидролокатора. */
  gulong                                   set_signal_id;              /* Идентификатор обработчика сигнала set. */
  gulong                                   changed_signal_id;          /* Идентификатор обработчика сигнала changed. */

  GHashTable                              *operations;                 /* Таблица возможных запросов. */
  GHashTable                              *paths;                      /* Таблица названий параметров запросов. */
  GHashTable                              *channels;                   /* Идентификаторы приёмных каналов. */

  gdouble                                  alive_timeout;              /* Интервал отправки сигнала alive. */
  GTimer                                  *alive_timer;                /* Таймер проверки таймаута сигнала alive. */
  GThread                                 *guard;                      /* Поток проверки таймаута сигнала alive. */
  gint                                     shutdown;                   /* Признак завершения работы. */
};

static void        hyscan_sonar_control_server_set_property            (GObject                     *object,
                                                                        guint                        prop_id,
                                                                        const GValue                *value,
                                                                        GParamSpec                  *pspec);
static void        hyscan_sonar_control_server_object_constructed      (GObject                     *object);
static void        hyscan_sonar_control_server_object_finalize         (GObject                     *object);

static gpointer    hyscan_sonar_control_server_uniq_channel            (HyScanSourceType             type,
                                                                        guint                        channel);

static gpointer    hyscan_sonar_control_server_quard                   (gpointer                     data);

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
    g_param_spec_object ("params", "SonarParams", "Sonar parameters via HyScanSonarBox", HYSCAN_TYPE_SONAR_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_SET_SYNC_TYPE] =
    g_signal_new ("sonar-set-sync-type", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  hyscan_control_boolean_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN,
                  1, G_TYPE_INT);

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

  hyscan_sonar_control_server_signals[SIGNAL_SONAR_ALIVE_TIMEOUT] =
    g_signal_new ("sonar-alive-timeout", HYSCAN_TYPE_SONAR_CONTROL_SERVER, G_SIGNAL_RUN_LAST, 0,
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
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
  HyScanDataSchemaNode *sources;

  gint64 version;
  gint64 id;
  gint i, j;

  G_OBJECT_CLASS (hyscan_sonar_control_server_parent_class)->constructed (object);

  priv->operations = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  priv->channels = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Обязательно должны быть переданы параметры гидролокатора. */
  if (priv->params == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (priv->params, "/schema/id", &id))
    {
      g_clear_object (&priv->params);
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->params);
      return;
    }
  if (!hyscan_data_box_get_integer (priv->params, "/schema/version", &version))
    {
      g_clear_object (&priv->params);
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->params);
      return;
    }

  if (hyscan_data_box_get_double (priv->params, "/info/alive-timeout", &priv->alive_timeout))
    {
      priv->alive_timer = g_timer_new ();
      priv->guard = g_thread_new ("sonar-control-server-alive", hyscan_sonar_control_server_quard, server);
    }

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
      HyScanSonarControlServerCtl *operation;
      gchar *operation_path;

      /* Считываем описания источников данных. */
      for (i = 0; i < sources->n_nodes; i++)
        {
          gchar **pathv;
          HyScanSourceType source;
          HyScanDataSchemaNode *channels;

          /* Тип источника данных. */
          pathv = g_strsplit (sources->nodes[i]->path, "/", -1);
          source = hyscan_control_get_source_type (pathv[2]);
          g_strfreev (pathv);

          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          /* Считываем описание приёмных каналов. */
          for (j = 0, channels = NULL; j < sources->nodes[i]->n_nodes; j++)
            {
              if (g_str_has_suffix (sources->nodes[i]->nodes[j]->path, "/channels"))
                {
                  channels = sources->nodes[i]->nodes[j];
                  break;
                }
            }

          if (channels != NULL)
            {
              gpointer uniq;
              gchar *param_id;
              guint channel_id;
              guint channel;

              /* Считываем индентификаторы каналов. */
              for (j = 0; j < channels->n_nodes; j++)
                {
                  pathv = g_strsplit (channels->nodes[j]->path, "/", -1);
                  channel = g_ascii_strtoull (pathv[4], NULL, 10);
                  g_strfreev (pathv);

                  param_id = g_strdup_printf ("%s/id", channels->nodes[j]->path);
                  if (hyscan_data_box_get_integer (priv->params, param_id, &id))
                    {
                      channel_id = id;
                      uniq = hyscan_sonar_control_server_uniq_channel (source, channel);
                      g_hash_table_insert (priv->channels, uniq, GINT_TO_POINTER (channel_id));
                    }
                  else
                    {
                      g_warning ("HyScanSonarControlServer: can't get channel %d id", channel);
                    }

                  g_free (param_id);
                }
            }

          /* Команда - hyscan_sonar_control_set_receive_time. */
          operation = g_new0 (HyScanSonarControlServerCtl, 1);
          operation->source = source;
          operation->name = hyscan_control_get_source_name (source);
          operation->func = hyscan_sonar_control_server_set_receive_time;
          g_hash_table_insert (priv->operations, operation, operation);

          operation_path = g_strdup_printf ("/sources/%s/control/receive-time", operation->name);
          g_hash_table_insert (priv->paths, operation_path, operation);
        }

      /* Команда - hyscan_sonar_control_set_sync_type. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->source = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_set_sync_type;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/sync/type");
      g_hash_table_insert (priv->paths, operation_path, operation);

      /* Команда - hyscan_sonar_control_enable_raw_data. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->source = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_enable_raw_data;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/control/raw-data");
      g_hash_table_insert (priv->paths, operation_path, operation);

      /* Команды - hyscan_sonar_control_start / hyscan_sonar_control_stop. */
      operation = g_new0 (HyScanSonarControlServerCtl, 1);
      operation->source = 0;
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
      operation->source = 0;
      operation->name = NULL;
      operation->func = hyscan_sonar_control_server_ping;
      g_hash_table_insert (priv->operations, operation, operation);

      operation_path = g_strdup ("/sync/ping");
      g_hash_table_insert (priv->paths, operation_path, operation);

      priv->set_signal_id = g_signal_connect (priv->params,
                                              "set",
                                              G_CALLBACK (hyscan_sonar_control_server_set_cb),
                                              server);
    }

  priv->changed_signal_id = g_signal_connect_swapped (priv->params,
                                                      "changed",
                                                      G_CALLBACK (g_timer_start),
                                                      priv->alive_timer);

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sonar_control_server_object_finalize (GObject *object)
{
  HyScanSonarControlServer *server = HYSCAN_SONAR_CONTROL_SERVER (object);
  HyScanSonarControlServerPrivate *priv = server->priv;

  if (priv->guard != NULL)
    {
      g_atomic_int_set (&priv->shutdown, 1);
      g_thread_join (priv->guard);
      g_timer_destroy (priv->alive_timer);
    }

  if (priv->set_signal_id > 0)
    g_signal_handler_disconnect (priv->params, priv->set_signal_id);

  if (priv->changed_signal_id > 0)
    g_signal_handler_disconnect (priv->params, priv->changed_signal_id);

  g_hash_table_unref (priv->paths);
  g_hash_table_unref (priv->operations);
  g_hash_table_unref (priv->channels);

  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_sonar_control_server_parent_class)->finalize (object);
}

/* Функция возвращает уникальный идентификатор для пары: источник данных, индекс канала. */
static gpointer
hyscan_sonar_control_server_uniq_channel (HyScanSourceType type,
                                          guint            channel)
{
  gint uniq = 1000 * type + channel;
  return GINT_TO_POINTER (uniq);
}

/* Поток проверки таймаута сигнала alive. */
static gpointer
hyscan_sonar_control_server_quard (gpointer data)
{
  HyScanSonarControlServer *server = data;
  HyScanSonarControlServerPrivate *priv = server->priv;

  while (!g_atomic_int_get (&priv->shutdown))
    {
      if (g_timer_elapsed (priv->alive_timer, NULL) >= priv->alive_timeout)
        {
          g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_ALIVE_TIMEOUT], 0);

          g_timer_start (priv->alive_timer);
        }

      g_usleep (100000);
    }

  return NULL;
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

      if (ctl0->source != ctln->source)
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

  gint64 value;
  HyScanSonarSyncType sync_type = 0;

  if (!hyscan_control_find_integer_param ("/sync/type", names, values, &value))
    return FALSE;

  sync_type = value;
  if (sync_type == 0)
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

  name = g_strdup_printf ("/sources/%s/control/receive-time", ctl->name);
  status = hyscan_control_find_double_param (name, names, values, &receive_time);
  g_free (name);

  if (!status)
    return FALSE;

  g_signal_emit (server, hyscan_sonar_control_server_signals[SIGNAL_SONAR_SET_RECEIVE_TIME], 0,
                 ctl->source, receive_time, &cancel);
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

void
hyscan_sonar_control_server_send_raw_data (HyScanSonarControlServer *server,
                                           HyScanSourceType          source,
                                           gint                      channel,
                                           HyScanDataType            type,
                                           gdouble                   rate,
                                           HyScanDataWriterData     *data)
{
  HyScanSonarMessage message;
  gpointer uniq;
  gpointer id;

  g_return_if_fail (HYSCAN_IS_SONAR_CONTROL_SERVER (server));

  if (server->priv->params == NULL)
    return;

  uniq = hyscan_sonar_control_server_uniq_channel (source, channel);
  id = g_hash_table_lookup (server->priv->channels, uniq);
  if (id == NULL)
    return;

  message.time = data->time;
  message.id   = GPOINTER_TO_INT (id);
  message.type = type;
  message.rate = rate;
  message.size = data->size;
  message.data = data->data;

  hyscan_sonar_box_send (HYSCAN_SONAR_BOX (server->priv->params), &message);
}
