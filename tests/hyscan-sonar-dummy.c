#include "hyscan-sonar-dummy.h"
#include "hyscan-sonar-messages.h"

#include <hyscan-data-schema-builder.h>
#include <hyscan-data-box.h>
#include <gio/gio.h>
#include <string.h>

#define MSG_DATA_MIN_SOURCES           1
#define MSG_DATA_MAX_SOURCES           16
#define MSG_DATA_DEFAULT_SOURCES       4

#define MSG_DATA_MIN_PERIOD            0.001
#define MSG_DATA_MAX_PERIOD            10.0
#define MSG_DATA_DEFAULT_PERIOD        1.0

#define MSG_DATA_MIN_POINTS            1
#define MSG_DATA_MAX_POINTS            262144
#define MSG_DATA_DEFAULT_POINTS        8192

enum
{
  SIGNAL_DATA,
  SIGNAL_LAST
};

struct _HyScanSonarDummyPrivate
{
  HyScanDataBox               *data;
  HyScanDataSchema            *schema;
  gint                         started;
  gint                         shutdown;
  GThread                     *worker;

  gboolean                     is_data;

  GTimer                      *guard;
};

static void            hyscan_sonar_dummy_interface_init       (HyScanParamInterface  *iface);
static void            hyscan_sonar_dummy_object_constructed   (GObject               *object);
static void            hyscan_sonar_dummy_object_finalize      (GObject               *object);

static gboolean        hyscan_sonar_dummy_check_enable_params  (HyScanDataBox         *data,
                                                                const gchar *const    *names,
                                                                GVariant             **values,
                                                                gpointer               user_data);
static gboolean        hyscan_sonar_dummy_check_data_params    (HyScanDataBox         *data,
                                                                const gchar *const    *names,
                                                                GVariant             **values,
                                                                gpointer               user_data);

static gpointer        hyscan_sonar_dummy_worker               (gpointer               user_data);

static guint           hyscan_sonar_dummy_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanSonarDummy, hyscan_sonar_dummy, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarDummy)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_sonar_dummy_interface_init))

static void
hyscan_sonar_dummy_class_init( HyScanSonarDummyClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->constructed = hyscan_sonar_dummy_object_constructed;
  object_class->finalize = hyscan_sonar_dummy_object_finalize;

  hyscan_sonar_dummy_signals[SIGNAL_DATA] =
    g_signal_new( "data", HYSCAN_TYPE_SONAR_DUMMY, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER );
}

static void
hyscan_sonar_dummy_init (HyScanSonarDummy *dummy_sonar)
{
  dummy_sonar->priv = hyscan_sonar_dummy_get_instance_private (dummy_sonar);
}

static void
hyscan_sonar_dummy_object_constructed (GObject *object)
{
  HyScanSonarDummy *dummy_sonar = HYSCAN_SONAR_DUMMY (object);
  HyScanSonarDummyPrivate *priv = dummy_sonar->priv;

  HyScanDataSchemaBuilder *builder;
  gchar *schema_data;

  builder = hyscan_data_schema_builder_new ("sonar");

  /* Активность. */
  hyscan_data_schema_builder_key_boolean_create (builder, "/alive", "alive",
                                                 "Sonar breaker", FALSE);

  /* Главный "рубильник". */
  hyscan_data_schema_builder_key_boolean_create (builder, "/enable", "Enable",
                                                 "Enable emulator", FALSE);

  /* Параметры имитатора данных. */
  hyscan_data_schema_builder_key_integer_create (builder, "/data/sources", "Number of sources",
                                                 "Number of sources", MSG_DATA_DEFAULT_SOURCES);
  hyscan_data_schema_builder_key_integer_range (builder, "/data/sources",
                                                MSG_DATA_MIN_SOURCES, MSG_DATA_MAX_SOURCES, 1);

  hyscan_data_schema_builder_key_double_create (builder, "/data/period", "Data period",
                                                "Data period", MSG_DATA_DEFAULT_PERIOD);
  hyscan_data_schema_builder_key_double_range (builder, "/data/period",
                                               MSG_DATA_MIN_PERIOD, MSG_DATA_MAX_PERIOD, 0.1);

  hyscan_data_schema_builder_key_integer_create (builder, "/data/size", "Data size",
                                                 "Data size", MSG_DATA_DEFAULT_POINTS);
  hyscan_data_schema_builder_key_integer_range (builder, "/data/size",
                                                MSG_DATA_MIN_POINTS, MSG_DATA_MAX_POINTS, 1);

  schema_data = hyscan_data_schema_builder_get_data (builder);
  priv->data = hyscan_data_box_new_from_string (schema_data, "sonar");
  g_free (schema_data);

  g_object_unref (builder);
  priv->schema = hyscan_param_schema (HYSCAN_PARAM (priv->data));

  priv->guard = g_timer_new ();

  g_signal_connect (priv->data, "set", G_CALLBACK (hyscan_sonar_dummy_check_data_params), priv);
  g_signal_connect (priv->data, "set", G_CALLBACK (hyscan_sonar_dummy_check_enable_params), priv);

  priv->worker = g_thread_new ("dummy-sonar-worker", hyscan_sonar_dummy_worker, dummy_sonar);
  while (g_atomic_int_get (&priv->started) != 1)
    g_usleep (1000);
}

static void
hyscan_sonar_dummy_object_finalize (GObject *object)
{
  HyScanSonarDummy *dummy_sonar = HYSCAN_SONAR_DUMMY (object);
  HyScanSonarDummyPrivate *priv = dummy_sonar->priv;

  g_atomic_int_set (&priv->shutdown, 1);
  g_thread_join (priv->worker);

  g_timer_destroy (priv->guard);
  g_object_unref (priv->schema);
  g_object_unref (priv->data);

  G_OBJECT_CLASS (hyscan_sonar_dummy_parent_class)->finalize (object);
}

/* Функция проверяет возможность включения "гидролокатора". */
static gboolean
hyscan_sonar_dummy_check_enable_params (HyScanDataBox       *data,
                                        const gchar *const  *names,
                                        GVariant           **values,
                                        gpointer             user_data)
{
  HyScanSonarDummyPrivate *priv = user_data;
  gint i;

  gboolean enable = FALSE;

  for (i = 0; names[i] != NULL; i++)
    {
      if (g_strcmp0 (names[i], "/enable") == 0)
        enable = TRUE;
    }

  if (enable)
    if (!priv->is_data)
      return FALSE;

  return TRUE;
}

/* Функция проверяет параметры подсистемы имитации данных. */
static gboolean
hyscan_sonar_dummy_check_data_params (HyScanDataBox       *data,
                                      const gchar *const  *names,
                                      GVariant           **values,
                                      gpointer             user_data)
{
  HyScanSonarDummyPrivate *priv = user_data;
  gint i;

  gboolean sources = FALSE;
  gboolean data_period = FALSE;
  gboolean data_size = FALSE;

  for (i = 0; names[i] != NULL; i++)
    {
      if (g_strcmp0 (names[i], "/data/sources") == 0)
        sources = TRUE;
      if (g_strcmp0 (names[i], "/data/period") == 0)
        data_period = TRUE;
      if (g_strcmp0 (names[i], "/data/size") == 0)
        data_size = TRUE;
    }

  if (sources || data_period || data_size)
    {
      if (!sources || !data_period || !data_size)
        return FALSE;

      priv->is_data = TRUE;
    }

  return TRUE;
}

static gpointer
hyscan_sonar_dummy_worker (gpointer user_data)
{
  HyScanSonarDummy *dummy_sonar = user_data;
  HyScanSonarDummyPrivate *priv = dummy_sonar->priv;
  HyScanParam *params = HYSCAN_PARAM (priv->data);

  gboolean start;

  HyScanSonarMessage message;

  GTimer *data_timer;
  guint32 *data;
  gint64 sources;
  gdouble data_period;
  gint64 data_size;

  guint32 indexes[MSG_DATA_MAX_SOURCES];
  gint i, j;

  data_timer = g_timer_new ();
  data = g_malloc (MSG_DATA_MAX_POINTS * sizeof (guint32));

  g_atomic_int_set (&priv->started, 1);

  while (g_atomic_int_get (&priv->shutdown) != 1)
    {
      hyscan_param_get_boolean (params, "/enable", &start);
      if (!start)
        {
          for (i = 0; i < MSG_DATA_MAX_SOURCES; i++)
            indexes[i] = 0;

          g_usleep (1000);
          continue;
        }

      if (g_timer_elapsed (priv->guard, NULL) > 5.0)
        {
          g_warning ("DummySonar: stopping sonar");
          hyscan_param_set_boolean (params, "/enable", FALSE);
          continue;
        }

      hyscan_param_get_integer (params, "/data/sources", &sources);
      hyscan_param_get_double (params, "/data/period", &data_period);
      hyscan_param_get_integer (params, "/data/size", &data_size);
      if (data_period < 0.0 || data_size <= 0 || data_size > MSG_DATA_MAX_POINTS)
        g_timer_reset (data_timer);

      /* Данные. */
      if (g_timer_elapsed (data_timer, NULL) >= data_period)
        {
          for (i = 0; i < sources; i++)
            {
              data[0] = indexes[i]++;
              for (j = 1; j < data_size; j++)
                data[j] = data[0] + i * data_size + j;

              message.time = g_get_monotonic_time ();
              message.id = i + 1;
              message.size = data_size * sizeof (guint32);
              message.data = data;

              g_signal_emit (dummy_sonar, hyscan_sonar_dummy_signals[SIGNAL_DATA], 0, &message);
            }
          g_timer_reset (data_timer);
        }

      g_usleep (1000);
    }

  g_timer_destroy (data_timer);
  g_free (data);

  return NULL;
}

/* Функция создаёт объект HyScanSonarDummy. */
HyScanSonarDummy *
hyscan_sonar_dummy_new (void)
{
  return g_object_new (HYSCAN_TYPE_SONAR_DUMMY, NULL);
}

static HyScanDataSchema *
hyscan_sonar_dummy_schema (HyScanParam *sonar)
{
  HyScanSonarDummy *dummy_sonar = HYSCAN_SONAR_DUMMY (sonar);

  return g_object_ref (dummy_sonar->priv->schema);
}


static gboolean
hyscan_sonar_dummy_set (HyScanParam         *sonar,
                        const gchar *const  *names,
                        GVariant           **values)
{
  HyScanSonarDummy *dummy_sonar = HYSCAN_SONAR_DUMMY (sonar);

  g_timer_reset (dummy_sonar->priv->guard);

  return hyscan_param_set (HYSCAN_PARAM (dummy_sonar->priv->data), names, values);
}

static gboolean
hyscan_sonar_dummy_get (HyScanParam         *sonar,
                        const gchar *const  *names,
                        GVariant           **values)
{
  HyScanSonarDummy *dummy_sonar = HYSCAN_SONAR_DUMMY (sonar);

  g_timer_reset (dummy_sonar->priv->guard);

  return hyscan_param_get (HYSCAN_PARAM (dummy_sonar->priv->data), names, values);
}

static void
hyscan_sonar_dummy_interface_init (HyScanParamInterface *iface)
{
  iface->schema = hyscan_sonar_dummy_schema;
  iface->set = hyscan_sonar_dummy_set;
  iface->get = hyscan_sonar_dummy_get;
}
