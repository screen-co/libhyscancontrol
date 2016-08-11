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

enum
{
  PROP_O,
  PROP_SONAR
};

struct _HyScanSSSEControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */
  gulong                       signal_id;                      /* Идентификатор обработчика сигнала data. */

  gboolean                     has_echosounder;                /* Есть или нет эхолот. */
};

static void    hyscan_ssse_control_set_property                (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void    hyscan_ssse_control_object_constructed          (GObject               *object);
static void    hyscan_ssse_control_object_finalize             (GObject               *object);

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

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *boards;

  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanSonar. */
  if (priv->sonar == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSSSEControl: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSSSEControl: sonar schema id mismatch");
      return;
    }
  if (!hyscan_sonar_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSSSEControl: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_clear_object (&priv->sonar);
      g_warning ("HyScanSSSEControl: sonar schema version mismatch");
      return;
    }

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (priv->sonar));

  /* Ветка схемы с описанием бортов - "/boards". */
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
      /* Проверяем наличие бортов. */
      for (i = 0; i < boards->n_nodes; i++)
        {
          gchar **pathv;
          gint board;

          /* Тип борта гидролокатора. */
          pathv = g_strsplit (boards->nodes[i]->path, "/", -1);
          board = hyscan_control_get_board_type (pathv[2]);
          g_strfreev (pathv);

          if (board == HYSCAN_BOARD_ECHOSOUNDER)
            priv->has_echosounder = TRUE;
        }
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_ssse_control_object_finalize (GObject *object)
{
  HyScanSSSEControl *control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = control->priv;

  g_clear_object (&priv->sonar);

  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->finalize (object);
}

HyScanSSSEControl *
hyscan_ssse_control_new (HyScanSonar *sonar,
                         HyScanDB    *db)
{
  return g_object_new (HYSCAN_TYPE_SSSE_CONTROL,
                       "sonar", sonar,
                       "db", db,
                       NULL);
}

gboolean
hyscan_ssse_control_has_echosounder (HyScanSSSEControl *control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (control), FALSE);

  return control->priv->has_echosounder;
}

#warning "Save acoustic data"
