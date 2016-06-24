/*
 * \file hyscan-tvg-control.c
 *
 * \brief Исходный файл класса управления системой ВАРУ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-control-common.h"
#include "hyscan-tvg-control.h"

enum
{
  SIGNAL_GAIN,
  SIGNAL_LAST
};

enum
{
  PROP_O,
  PROP_SONAR
};

typedef struct
{
  gint                         id;                             /* Идентификатор источника данных. */
  HyScanSourceType             source;                         /* Тип источника данных. */
  gchar                       *path;                           /* Путь к описанию источника данных в схеме. */
  HyScanTVGModeType            capabilities;                   /* Режимы работы системы ВАРУ. */
} HyScanTVGControlTVG;

struct _HyScanTVGControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */

  HyScanDataSchema            *schema;                         /* Схема данных гидролокатора. */
  HyScanDataSchemaNode        *params;                         /* Список параметров гидролокатора. */

  GHashTable                  *tvgs_by_id;                     /* Список систем ВАРУ. */
  GHashTable                  *tvgs_by_source;                 /* Список систем ВАРУ. */

  GMutex                       lock;                           /* Блокировка. */
};

static void    hyscan_tvg_control_set_property         (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_tvg_control_object_constructed   (GObject               *object);
static void    hyscan_tvg_control_object_finalize      (GObject               *object);

static void    hyscan_tvg_control_free_tvg             (gpointer               data);

static guint   hyscan_tvg_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanTVGControl, hyscan_tvg_control, HYSCAN_TYPE_GENERATOR_CONTROL)

static void
hyscan_tvg_control_class_init (HyScanTVGControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_tvg_control_set_property;

  object_class->constructed = hyscan_tvg_control_object_constructed;
  object_class->finalize = hyscan_tvg_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_tvg_control_signals[SIGNAL_GAIN] =
    g_signal_new ("gain", HYSCAN_TYPE_GENERATOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1, G_TYPE_POINTER);
}

static void
hyscan_tvg_control_init (HyScanTVGControl *tvg_control)
{
  tvg_control->priv = hyscan_tvg_control_get_instance_private (tvg_control);
}

static void
hyscan_tvg_control_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (object);
  HyScanTVGControlPrivate *priv = tvg_control->priv;

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
hyscan_tvg_control_object_constructed (GObject *object)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (object);
  HyScanTVGControlPrivate *priv = tvg_control->priv;

  HyScanDataSchemaNode *sources = NULL;
  gint64 version;
  gint64 id;
  gint i;

  G_OBJECT_CLASS (hyscan_tvg_control_parent_class)->constructed (object);

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

  /* Список систем ВАРУ. */
  priv->tvgs_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, hyscan_tvg_control_free_tvg);
  priv->tvgs_by_source = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Ветка схемы с описанием источников данных - "/sources". */
  for (i = 0; priv->params->n_nodes; i++)
    {
      if (g_strcmp0 (priv->params->nodes[i]->path, "/sources") == 0)
        {
          sources = priv->params->nodes[i];
          break;
        }
    }

  if (sources == NULL)
    return;

  /* Считываем описания генераторов. */
  for (i = 0; i < sources->n_nodes; i++)
    {
      HyScanTVGControlTVG *tvg;

      gchar *key_name;
      gboolean status;

      gint64 id;
      gint64 source;
      gint64 capabilities;

      /* Идентификатор порта. */
      key_name = g_strdup_printf ("%s/id", sources->nodes[i]->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &id);
      g_free (key_name);

      if (!status || id <= 0 || id > G_MAXUINT32)
        continue;

      /* Тип источника данных. */
      key_name = g_strdup_printf ("%s/source", sources->nodes[i]->path);
      status = hyscan_sonar_get_enum (priv->sonar, key_name, &source);
      g_free (key_name);

      if (!status)
        continue;

      /* Режимы работы системы ВАРУ. */
      key_name = g_strdup_printf ("%s/tvg/capabilities", sources->nodes[i]->path);
      status = hyscan_sonar_get_integer (priv->sonar, key_name, &capabilities);
      g_free (key_name);

      if (!status)
        continue;

    }





}

static void
hyscan_tvg_control_object_finalize (GObject *object)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (object);
  HyScanTVGControlPrivate *priv = tvg_control->priv;

  g_clear_pointer (&priv->tvgs_by_source, g_hash_table_unref);
  g_clear_pointer (&priv->tvgs_by_id, g_hash_table_unref);

  g_clear_pointer (&priv->params, hyscan_data_schema_free_nodes);

  g_clear_object (&priv->sonar);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_tvg_control_parent_class)->finalize (object);
}

/* Функция освобождает память, занятую структурой HyScanTVGControlTVG. */
static void
hyscan_tvg_control_free_tvg (gpointer data)
{
  HyScanTVGControlTVG *tvg = data;

  g_free (tvg->path);
  g_free (tvg);
}

void
hyscan_tvg_control_set_a (HyScanTVGControl *tvg_control,
                      gint           a)
{
  HyScanTVGControlPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TVG_CONTROL (tvg_control));

  priv = tvg_control->priv;

//  priv->prop_a = a;
}

gint
hyscan_tvg_control_get_a (HyScanTVGControl *tvg_control)
{
  g_return_val_if_fail (HYSCAN_IS_TVG_CONTROL (tvg_control), -1);

//  return tvg_control->priv->prop_a;
}
