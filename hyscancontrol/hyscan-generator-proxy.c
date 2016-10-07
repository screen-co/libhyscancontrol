/*
 * \file hyscan-generator-proxy.c
 *
 * \brief Исходный файл класса прокси сервера управления генераторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-generator-proxy.h"
#include "hyscan-generator-control.h"
#include "hyscan-generator-control-server.h"
#include "hyscan-control-common.h"

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_PROXY_MODE
};

struct _HyScanGeneratorProxyPrivate
{
  HyScanGeneratorControl       *control;                       /* Клиент управления проксируемыми генераторами. */
  HyScanGeneratorControlServer *server;                        /* Прокси сервер генераторов. */

  GHashTable                   *sources;                       /* Список таблиц трансляции преднастроек генераторов. */
  HyScanSonarProxyModeType      proxy_mode;                    /* Режим трансляции команд и данных. */
};

static void        hyscan_generator_proxy_set_property         (GObject                       *object,
                                                                guint                          prop_id,
                                                                const GValue                  *value,
                                                                GParamSpec                    *pspec);
static void        hyscan_generator_proxy_object_constructed   (GObject                       *object);
static void        hyscan_generator_proxy_object_finalize      (GObject                       *object);

static gboolean    hyscan_generator_proxy_set_preset           (HyScanGeneratorProxyPrivate   *priv,
                                                                HyScanSourceType               source,
                                                                guint                          preset);
static gboolean    hyscan_generator_proxy_set_auto             (HyScanGeneratorProxyPrivate   *priv,
                                                                HyScanSourceType               source,
                                                                HyScanGeneratorSignalType      signal);
static gboolean    hyscan_generator_proxy_set_simple           (HyScanGeneratorProxyPrivate   *priv,
                                                                HyScanSourceType               source,
                                                                HyScanGeneratorSignalType      signal,
                                                                gdouble                        power);
static gboolean    hyscan_generator_proxy_set_extended         (HyScanGeneratorProxyPrivate   *priv,
                                                                HyScanSourceType               source,
                                                                HyScanGeneratorSignalType      signal,
                                                                gdouble                        duration,
                                                                gdouble                        power);
static gboolean    hyscan_generator_proxy_set_enable           (HyScanGeneratorProxyPrivate   *priv,
                                                                HyScanSourceType               source,
                                                                gboolean                       enable);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGeneratorProxy, hyscan_generator_proxy, HYSCAN_TYPE_SENSOR_PROXY)

static void
hyscan_generator_proxy_class_init (HyScanGeneratorProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_generator_proxy_set_property;

  object_class->constructed = hyscan_generator_proxy_object_constructed;
  object_class->finalize = hyscan_generator_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "Generator control", HYSCAN_TYPE_GENERATOR_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROXY_MODE,
    g_param_spec_int ("proxy-mode", "ProxyMode", "Proxy mode",
                      HYSCAN_SONAR_PROXY_MODE_ALL, HYSCAN_SONAR_PROXY_FORWARD_COMPUTED,
                      HYSCAN_SONAR_PROXY_FORWARD_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_generator_proxy_init (HyScanGeneratorProxy *proxy)
{
  proxy->priv = hyscan_generator_proxy_get_instance_private (proxy);
}

static void
hyscan_generator_proxy_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanGeneratorProxy *proxy = HYSCAN_GENERATOR_PROXY (object);
  HyScanGeneratorProxyPrivate *priv = proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      G_OBJECT_CLASS (hyscan_generator_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->control = g_value_dup_object (value);
      break;

    case PROP_PROXY_MODE:
      G_OBJECT_CLASS (hyscan_generator_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->proxy_mode = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_generator_proxy_object_constructed (GObject *object)
{
  HyScanGeneratorProxy *proxy = HYSCAN_GENERATOR_PROXY (object);
  HyScanGeneratorProxyPrivate *priv = proxy->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sources;

  gint64 version;
  gint64 id;
  gint i, j, k;

  G_OBJECT_CLASS (hyscan_generator_proxy_parent_class)->constructed (object);

  /* Обязательно должен быть передан указатель на HyScanGeneratorControl. */
  if (priv->control == NULL)
    return;

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_data_box_get_integer (HYSCAN_DATA_BOX (proxy), "/schema/id", &id))
    return;
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    return;
  if (!hyscan_data_box_get_integer (HYSCAN_DATA_BOX (proxy), "/schema/version", &version))
    return;
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    return;

  /* Таблицы трансляции преднастроек генераторов. */
  priv->sources = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, (GDestroyNotify)g_hash_table_unref);

  /* Параметры гидролокатора. */
  params = hyscan_data_schema_list_nodes (HYSCAN_DATA_SCHEMA (proxy));

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
          gchar **pathv;
          HyScanSourceType source;
          HyScanGeneratorModeType capabilities;

          gchar *param_name;
          GHashTable *presets;
          HyScanDataSchemaEnumValue **sonar_values;
          HyScanDataSchemaEnumValue **proxy_values;

          /* Тип источника данных. */
          pathv = g_strsplit (sources->nodes[i]->path, "/", -1);
          source = hyscan_control_get_source_type (pathv[2]);
          g_strfreev (pathv);

          if (source == HYSCAN_SOURCE_INVALID)
            continue;

          /* Проверка преднастроек у генератора. */
          capabilities = hyscan_generator_control_get_capabilities (priv->control, source);
          if (!(capabilities & HYSCAN_GENERATOR_MODE_PRESET))
            continue;

          /* Таблица трансляции идентификаторов преднастроек. */
          presets = g_hash_table_new (g_direct_hash, g_direct_equal);
          g_hash_table_insert (priv->sources, GINT_TO_POINTER (source), presets);

          param_name = g_strdup_printf ("%s/generator/preset/id", sources->nodes[i]->path);
          sonar_values = hyscan_generator_control_list_presets (priv->control, source);
          proxy_values = hyscan_data_schema_key_get_enum_values (HYSCAN_DATA_SCHEMA (proxy), param_name);

          for (j = 0; proxy_values != NULL && proxy_values[j] != NULL; j++)
            {
              for (k = 0; sonar_values != NULL && sonar_values[k] != NULL; k++)
                {
                  if (g_strcmp0 (proxy_values[j]->name, sonar_values[k]->name) == 0)
                    {
                      guint proxy_id = proxy_values[j]->value;
                      guint sonar_id = sonar_values[k]->value;

                      g_hash_table_insert (presets, GINT_TO_POINTER (proxy_id), GINT_TO_POINTER (sonar_id));
                      break;
                    }
                }
            }

          g_clear_pointer (&sonar_values, hyscan_data_schema_free_enum_values);
          g_clear_pointer (&proxy_values, hyscan_data_schema_free_enum_values);
          g_free (param_name);
        }
    }

  hyscan_data_schema_free_nodes (params);

  /* Прокси сервер. */
  priv->server = hyscan_generator_control_server_new (HYSCAN_SONAR_BOX (proxy));

  /* Обработчики команд. */
  g_signal_connect_swapped (priv->server, "generator-set-preset",
                            G_CALLBACK (hyscan_generator_proxy_set_preset), priv);
  g_signal_connect_swapped (priv->server, "generator-set-auto",
                            G_CALLBACK (hyscan_generator_proxy_set_auto), priv);
  g_signal_connect_swapped (priv->server, "generator-set-simple",
                            G_CALLBACK (hyscan_generator_proxy_set_simple), priv);
  g_signal_connect_swapped (priv->server, "generator-set-extended",
                            G_CALLBACK (hyscan_generator_proxy_set_extended), priv);
  g_signal_connect_swapped (priv->server, "generator-set-enable",
                            G_CALLBACK (hyscan_generator_proxy_set_enable), priv);

  /* Обработчик образов сигналов. */
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      g_signal_connect_swapped (priv->control, "signal-image",
                                G_CALLBACK (hyscan_generator_control_server_send_signal), priv->server);
    }
}

static void
hyscan_generator_proxy_object_finalize (GObject *object)
{
  HyScanGeneratorProxy *proxy = HYSCAN_GENERATOR_PROXY (object);
  HyScanGeneratorProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv->server);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  g_clear_pointer (&priv->sources, g_hash_table_unref);

  G_OBJECT_CLASS (hyscan_generator_proxy_parent_class)->finalize (object);
}

/* Команда - hyscan_generator_control_set_preset. */
static gboolean
hyscan_generator_proxy_set_preset (HyScanGeneratorProxyPrivate *priv,
                                   HyScanSourceType             source,
                                   guint                        preset)
{
  GHashTable *presets;
  guint sonar_preset;

  presets = g_hash_table_lookup (priv->sources, GINT_TO_POINTER (source));
  if (presets == NULL)
    return FALSE;

  sonar_preset = GPOINTER_TO_INT (g_hash_table_lookup (presets, GINT_TO_POINTER (preset)));

  return hyscan_generator_control_set_preset (priv->control, source, sonar_preset);
}

/* Команда - hyscan_generator_control_set_auto. */
static gboolean
hyscan_generator_proxy_set_auto (HyScanGeneratorProxyPrivate *priv,
                                 HyScanSourceType             source,
                                 HyScanGeneratorSignalType    signal)
{
  return hyscan_generator_control_set_auto (priv->control, source, signal);
}

/* Команда - hyscan_generator_control_set_simple. */
static gboolean
hyscan_generator_proxy_set_simple (HyScanGeneratorProxyPrivate *priv,
                                   HyScanSourceType             source,
                                   HyScanGeneratorSignalType    signal,
                                   gdouble                      power)
{
  return hyscan_generator_control_set_simple (priv->control, source, signal, power);
}

/* Команда - hyscan_generator_control_set_extanded. */
static gboolean
hyscan_generator_proxy_set_extended (HyScanGeneratorProxyPrivate *priv,
                                     HyScanSourceType             source,
                                     HyScanGeneratorSignalType    signal,
                                     gdouble                      duration,
                                     gdouble                      power)
{
  return hyscan_generator_control_set_extended (priv->control, source, signal, duration, power);
}

/* Команда - hyscan_generator_control_set_enable. */
static gboolean
hyscan_generator_proxy_set_enable (HyScanGeneratorProxyPrivate *priv,
                                   HyScanSourceType             source,
                                   gboolean                     enable)
{
  return hyscan_generator_control_set_enable (priv->control, source, enable);
}
