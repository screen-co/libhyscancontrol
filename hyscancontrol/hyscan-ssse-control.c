/*
 * \file hyscan-ssse-control.c
 *
 * \brief Исходный файл класса управления ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-ssse-control.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"

enum
{
  PROP_O,
  PROP_SONAR
};

enum
{
  SIGNAL_ACOUSTIC_DATA,
  SIGNAL_LAST
};

typedef struct
{
  guint32                      id;                             /* Идентификатор источника акустических данных. */
  HyScanSourceType             source;                         /* Тип источника данных. */
  HyScanAcousticDataInfo       info;                           /* Параметры акустических данных. */
} HyScanSSSEControlAcoustic;

struct _HyScanSSSEControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */
  gulong                       signal_id;                      /* Идентификатор обработчика сигнала data. */

  gboolean                     has_starboard;                  /* Признак наличия правого борта, стандартное разрешение. */
  gboolean                     has_port;                       /* Признак наличия левого борта, стандартное разрешение. */
  gboolean                     has_starboard_hi;               /* Признак наличия правого борта, повышенное разрешение. */
  gboolean                     has_port_hi;                    /* Признак наличия левого борта, повышенное разрешение. */
  gboolean                     has_echosounder;                /* Признак наличия эхолота. */

  HyScanSSSEControlAcoustic   *starboard;                      /* Правый борт, стандартное разрешение. */
  HyScanSSSEControlAcoustic   *port;                           /* Левый борт, стандартное разрешение. */
  HyScanSSSEControlAcoustic   *starboard_hi;                   /* Правый борт, повышенное разрешение. */
  HyScanSSSEControlAcoustic   *port_hi;                        /* Левый борт, повышенное разрешение. */
  HyScanSSSEControlAcoustic   *echosounder;                    /* Эхолот, стандартное разрешение. */
};

static void    hyscan_ssse_control_set_property                (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void    hyscan_ssse_control_object_constructed          (GObject               *object);
static void    hyscan_ssse_control_object_finalize             (GObject               *object);

static gboolean
               hyscan_ssse_control_is_source                   (HyScanDataSchema      *schema,
                                                                HyScanSourceType       source);
static HyScanSSSEControlAcoustic *
               hyscan_ssse_control_get_acoustic_info           (HyScanSonar           *sonar,
                                                                HyScanSourceType       source);


static void    hyscan_ssse_control_data_receiver               (HyScanSSSEControl     *control,
                                                                HyScanSonarMessage    *message);

static guint   hyscan_ssse_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSSSEControl, hyscan_ssse_control, HYSCAN_TYPE_SONAR_CONTROL)

static void
hyscan_ssse_control_class_init (HyScanSSSEControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_ssse_control_set_property;

  object_class->constructed = hyscan_ssse_control_object_constructed;
  object_class->finalize = hyscan_ssse_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_ssse_control_signals[SIGNAL_ACOUSTIC_DATA] =
    g_signal_new ("acoustic-data", HYSCAN_TYPE_SSSE_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__INT_POINTER_POINTER,
                  G_TYPE_NONE,
                  3, G_TYPE_INT, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
hyscan_ssse_control_init (HyScanSSSEControl *control)
{
  control->priv = hyscan_ssse_control_get_instance_private (control);
}

static void
hyscan_ssse_control_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanSSSEControl *control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->set_property (object, prop_id, value, pspec);
      priv->sonar = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_ssse_control_object_constructed (GObject *object)
{
  HyScanSSSEControl *control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = control->priv;

  HyScanDataSchema *schema;
  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      return;
    }

  /* Проверка наличия источников данных гидролокатора. */
  schema = HYSCAN_DATA_SCHEMA (priv->sonar);

  priv->has_starboard    = hyscan_ssse_control_is_source (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  priv->has_port         = hyscan_ssse_control_is_source (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  priv->has_starboard_hi = hyscan_ssse_control_is_source (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI);
  priv->has_port_hi      = hyscan_ssse_control_is_source (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT_HI);
  priv->has_echosounder  = hyscan_ssse_control_is_source (schema, HYSCAN_SOURCE_ECHOSOUNDER);

  /* Загружаем параметры акустических источников. */
  priv->starboard    = hyscan_ssse_control_get_acoustic_info (priv->sonar, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  priv->port         = hyscan_ssse_control_get_acoustic_info (priv->sonar, HYSCAN_SOURCE_SIDE_SCAN_PORT);
  priv->starboard_hi = hyscan_ssse_control_get_acoustic_info (priv->sonar, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI);
  priv->port_hi      = hyscan_ssse_control_get_acoustic_info (priv->sonar, HYSCAN_SOURCE_SIDE_SCAN_PORT_HI);
  priv->echosounder  = hyscan_ssse_control_get_acoustic_info (priv->sonar, HYSCAN_SOURCE_ECHOSOUNDER);

  /* Если нет акустических источников данных, принудительно включаем приём "сырых" данных. */
  if ((priv->has_starboard && priv->starboard == NULL) ||
      (priv->has_port && priv->port == NULL) ||
      (priv->has_starboard_hi && priv->starboard_hi == NULL) ||
      (priv->has_port_hi && priv->port_hi == NULL) ||
      (priv->has_echosounder && priv->echosounder == NULL))
    {
      hyscan_sonar_control_enable_raw_data (HYSCAN_SONAR_CONTROL (control), TRUE);
    }

  /* Обработчик данных от приёмных каналов гидролокатора. */
  priv->signal_id = g_signal_connect_swapped (priv->sonar,
                                              "data",
                                              G_CALLBACK (hyscan_ssse_control_data_receiver),
                                              control);
}

static void
hyscan_ssse_control_object_finalize (GObject *object)
{
  HyScanSSSEControl *control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = control->priv;

  if (priv->signal_id > 0)
    g_signal_handler_disconnect (priv->sonar, priv->signal_id);

  g_clear_pointer (&priv->starboard, g_free);
  g_clear_pointer (&priv->port, g_free);
  g_clear_pointer (&priv->starboard_hi, g_free);
  g_clear_pointer (&priv->port_hi, g_free);
  g_clear_pointer (&priv->echosounder, g_free);

  g_clear_object (&priv->sonar);

  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->finalize (object);
}

/* Функция проверяет наличие источников данных гидролокатора. */
static gboolean
hyscan_ssse_control_is_source (HyScanDataSchema *schema,
                               HyScanSourceType  source)
{
  const gchar *source_name = hyscan_control_get_source_name (source);
  gchar *param_name = g_strdup_printf ("/sources/%s/control/receive-time", source_name);
  gboolean is_source = hyscan_data_schema_has_key (schema, param_name);

  g_free (param_name);

  return is_source;
}

/* Функция считывает параметры источника акустических данных из схемы гидролокатора. */
static HyScanSSSEControlAcoustic *
hyscan_ssse_control_get_acoustic_info (HyScanSonar      *sonar,
                                       HyScanSourceType  source)
{
  HyScanSSSEControlAcoustic *acoustic = NULL;
  const gchar *source_name;

  gchar *param_names[4];
  GVariant *param_values[4];

  gint64 id;
  gdouble antenna_vpattern;
  gdouble antenna_hpattern;

  source_name = hyscan_control_get_source_name (source);

  param_names[0] = g_strdup_printf ("/sources/%s/acoustic/id", source_name);
  param_names[1] = g_strdup_printf ("/sources/%s/antenna/pattern/vertical", source_name);
  param_names[2] = g_strdup_printf ("/sources/%s/antenna/pattern/horizontal", source_name);
  param_names[3] = NULL;

  if (hyscan_sonar_get (sonar, (const gchar **)param_names, param_values))
    {
      id = g_variant_get_int64 (param_values[0]);
      antenna_vpattern = g_variant_get_double (param_values[1]);
      antenna_hpattern = g_variant_get_double (param_values[2]);

      if (id >= 1 && id <= G_MAXINT32)
        {
          acoustic = g_new0 (HyScanSSSEControlAcoustic, 1);
          acoustic->id = id;
          acoustic->source = source;
          acoustic->info.antenna.pattern.vertical = antenna_vpattern;
          acoustic->info.antenna.pattern.horizontal = antenna_hpattern;
        }

      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);

  return acoustic;
}

/* Функция обрабатывает сообщения с обработанными акустическимим данными от гидролокатора. */
static void
hyscan_ssse_control_data_receiver (HyScanSSSEControl  *control,
                                   HyScanSonarMessage *message)
{
  HyScanSSSEControlPrivate *priv = control->priv;
  HyScanSSSEControlAcoustic *acoustic;
  HyScanAcousticDataInfo info;
  HyScanDataWriterData data;

  /* Ищем источник данных. */
  if (priv->starboard != NULL && priv->starboard->id == message->id)
    acoustic = priv->starboard;
  else if (priv->port != NULL && priv->port->id == message->id)
    acoustic = priv->port;
  else if (priv->starboard_hi != NULL && priv->starboard_hi->id == message->id)
    acoustic = priv->starboard_hi;
  else if (priv->port_hi != NULL && priv->port_hi->id == message->id)
    acoustic = priv->port_hi;
  else if (priv->echosounder != NULL && priv->echosounder->id == message->id)
    acoustic = priv->echosounder;
  else
    return;

  /* Данные. */
  info = acoustic->info;
  info.data.type = message->type;
  info.data.rate = message->rate;
  data.time = message->time;
  data.size = message->size;
  data.data = message->data;
  hyscan_data_writer_acoustic_add_data (HYSCAN_DATA_WRITER (control), acoustic->source, &info, &data);

  g_signal_emit (control, hyscan_ssse_control_signals[SIGNAL_ACOUSTIC_DATA], 0,
                 acoustic->source, &info, &data);
}

/* Функция создаёт новый объект HyScanSSSEControl. */
HyScanSSSEControl *
hyscan_ssse_control_new (HyScanSonar *sonar,
                         HyScanDB    *db)
{
  if (hyscan_control_sonar_probe (sonar) != HYSCAN_SONAR_SSSE)
    return NULL;

  return g_object_new (HYSCAN_TYPE_SSSE_CONTROL,
                       "sonar", sonar,
                       "db", db,
                       NULL);
}

gboolean
hyscan_ssse_control_has_starboard (HyScanSSSEControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (control), FALSE);

  return control->priv->has_starboard;
}

gboolean
hyscan_ssse_control_has_port (HyScanSSSEControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (control), FALSE);

  return control->priv->has_port;
}

gboolean
hyscan_ssse_control_has_starboard_hi (HyScanSSSEControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (control), FALSE);

  return control->priv->has_starboard_hi;
}

gboolean
hyscan_ssse_control_has_port_hi (HyScanSSSEControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (control), FALSE);

  return control->priv->has_port_hi;
}

/* Функция определяет наличие эхолота. */
gboolean
hyscan_ssse_control_has_echosounder (HyScanSSSEControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (control), FALSE);

  return control->priv->has_echosounder;
}
