/*
 * \file hyscan-sonar-control.c
 *
 * \brief Исходный файл класса управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control-common.h"
#include "hyscan-sonar-control.h"

enum
{
  PROP_O,
  PROP_SONAR
};

struct _HyScanSonarControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */
  gulong                       signal_id;                      /* Идентификатор обработчика сигнала data. */

  HyScanSonarSyncType          sync_types;                     /* Доступные методы синхронизации излучения. */
};

static void    hyscan_sonar_control_set_property       (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_sonar_control_object_constructed (GObject               *object);
static void    hyscan_sonar_control_object_finalize    (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarControl, hyscan_sonar_control, HYSCAN_TYPE_TVG_CONTROL)

static void
hyscan_sonar_control_class_init (HyScanSonarControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_control_set_property;

  object_class->constructed = hyscan_sonar_control_object_constructed;
  object_class->finalize = hyscan_sonar_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_control_init (HyScanSonarControl *control)
{
  control->priv = hyscan_sonar_control_get_instance_private (control);
}

static void
hyscan_sonar_control_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->set_property (object, prop_id, value, pspec);
      priv->sonar = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
hyscan_sonar_control_object_constructed (GObject *object)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  GVariant *sync_types_value;
  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSonarControl: sonar schema version mismatch");
      return;
    }

  /* Доступные методы синхронизации излучения. */
  sync_types_value = hyscan_data_schema_key_get_default (HYSCAN_DATA_SCHEMA (priv->sonar),
                                                         "/sync/capabilities");
  if (sync_types_value != NULL)
    {
      priv->sync_types = g_variant_get_int64 (sync_types_value);
      g_variant_unref (sync_types_value);
    }
}

static void
hyscan_sonar_control_object_finalize (GObject *object)
{
  HyScanSonarControl *control = HYSCAN_SONAR_CONTROL (object);
  HyScanSonarControlPrivate *priv = control->priv;

  g_clear_object (&priv->sonar);

  G_OBJECT_CLASS (hyscan_sonar_control_parent_class)->finalize (object);
}

/* Функция возвращает маску доступных типов синхронизации излучения. */
HyScanSonarSyncType
hyscan_sonar_control_get_sync_capabilities (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), 0);

  return control->priv->sync_types;
}

/* Функция устанавливает тип синхронизации излучения. */
gboolean
hyscan_sonar_control_set_sync_type (HyScanSonarControl  *control,
                                    HyScanSonarSyncType  sync_type)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_sonar_set_enum (control->priv->sonar, "/sync/type", sync_type);
}

/* Функция включает или выключает выдачу "сырых" данных от гидролокатора. */
gboolean
hyscan_sonar_control_enable_raw_data (HyScanSonarControl *control,
                                      gboolean            enable)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_sonar_set_boolean (control->priv->sonar, "/control/raw-data", enable);
}

/* Функция задаёт время приёма эхосигнала бортом гидролокатора. */
gboolean
hyscan_sonar_control_set_receive_time (HyScanSonarControl *control,
                                       HyScanBoardType     board,
                                       gdouble             receive_time)
{
  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  param_name = g_strdup_printf ("/boards/%s/control/receive-time",
                                hyscan_control_get_board_name (board));
  status = hyscan_sonar_set_double (control->priv->sonar, param_name, receive_time);
  g_free (param_name);

  return status;
}

/* Функция переводит гидролокатор в рабочий режим и включает запись данных. */
gboolean
hyscan_sonar_control_start (HyScanSonarControl *control,
                            const gchar        *project_name,
                            const gchar        *track_name)
{
  gchar *param_names[4];
  GVariant *param_values[4];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  if (!hyscan_write_control_start (HYSCAN_WRITE_CONTROL (control), project_name, track_name))
    return FALSE;

  param_names[0] = "/control/project-name";
  param_names[1] = "/control/track-name";
  param_names[2] = "/control/enable";
  param_names[3] = NULL;

  param_values[0] = g_variant_new_string (project_name);
  param_values[1] = g_variant_new_string (track_name);
  param_values[2] = g_variant_new_boolean (TRUE);
  param_values[3] = NULL;

  status = hyscan_sonar_set (control->priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
    }

  return status;
}

/* Функция переводит гидролокатор в ждущий режим и отключает запись данных. */
gboolean
hyscan_sonar_control_stop (HyScanSonarControl *control)
{
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  status = hyscan_sonar_set_boolean (control->priv->sonar, "/control/enable", FALSE);
  hyscan_write_control_stop (HYSCAN_WRITE_CONTROL (control));

  return status;
}

/* Функция выполняет один цикл зондирования и приёма данных. */
gboolean
hyscan_sonar_control_ping (HyScanSonarControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL (control), FALSE);

  return hyscan_sonar_set_boolean (control->priv->sonar, "/sync/ping", TRUE);
}

#warning "Save RAW data"
#warning "Alive signal"
