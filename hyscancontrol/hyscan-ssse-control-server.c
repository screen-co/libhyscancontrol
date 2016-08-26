/*
 * \file hyscan-ssse-control-server.c
 *
 * \brief Исходный файл класса сервера управления ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-ssse-control-server.h"
#include "hyscan-control-common.h"

enum
{
  PROP_O,
  PROP_PARAMS
};

struct _HyScanSSSEControlServerPrivate
{
  HyScanDataBox               *params;                         /* Параметры гидролокатора. */

  guint32                      starboard_id;                   /* Источника акустических данных правого борта. */
  guint32                      port_id;                        /* Источника акустических данных левого борта. */
  guint32                      starboard_hi_id;                /* Источника акустических данных правого борта. */
  guint32                      port_hi_id;                     /* Источника акустических данных левого борта. */
  guint32                      echosounder_id;                 /* Источника акустических данных эхолота. */
};

static void    hyscan_ssse_control_server_set_property         (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void    hyscan_ssse_control_server_object_constructed   (GObject               *object);
static void    hyscan_ssse_control_server_object_finalize      (GObject               *object);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSSSEControlServer, hyscan_ssse_control_server, HYSCAN_TYPE_SONAR_CONTROL_SERVER)

static void
hyscan_ssse_control_server_class_init (HyScanSSSEControlServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_ssse_control_server_set_property;

  object_class->constructed = hyscan_ssse_control_server_object_constructed;
  object_class->finalize = hyscan_ssse_control_server_object_finalize;

  g_object_class_install_property (object_class, PROP_PARAMS,
    g_param_spec_object ("params", "SonarParams", "Sonar parameters via HyScanSonarBox", HYSCAN_TYPE_SONAR_BOX,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_ssse_control_server_init (HyScanSSSEControlServer *server)
{
  server->priv = hyscan_ssse_control_server_get_instance_private (server);
}

static void
hyscan_ssse_control_server_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanSSSEControlServer *server = HYSCAN_SSSE_CONTROL_SERVER (object);
  HyScanSSSEControlServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_PARAMS:
      G_OBJECT_CLASS (hyscan_ssse_control_server_parent_class)->set_property (object, prop_id, value, pspec);
      priv->params = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_ssse_control_server_object_constructed (GObject *object)
{
  HyScanSSSEControlServer *server = HYSCAN_SSSE_CONTROL_SERVER (object);
  HyScanSSSEControlServerPrivate *priv = server->priv;

  gchar *param_name;

  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_ssse_control_server_parent_class)->constructed (object);

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

  /* Идентификаторы источников акустических данных. */
  param_name = g_strdup_printf ("/sources/%s/acoustic/id",
                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD));
  if (hyscan_data_box_get_integer (priv->params, param_name, &id))
    if (id >= 1 && id <= G_MAXUINT32)
      priv->starboard_id = id;
  g_free (param_name);

  param_name = g_strdup_printf ("/sources/%s/acoustic/id",
                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_PORT));
  if (hyscan_data_box_get_integer (priv->params, param_name, &id))
    if (id >= 1 && id <= G_MAXUINT32)
      priv->port_id = id;
  g_free (param_name);

  param_name = g_strdup_printf ("/sources/%s/acoustic/id",
                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI));
  if (hyscan_data_box_get_integer (priv->params, param_name, &id))
    if (id >= 1 && id <= G_MAXUINT32)
      priv->starboard_hi_id = id;
  g_free (param_name);

  param_name = g_strdup_printf ("/sources/%s/acoustic/id",
                                hyscan_control_get_source_name (HYSCAN_SOURCE_SIDE_SCAN_PORT_HI));
  if (hyscan_data_box_get_integer (priv->params, param_name, &id))
    if (id >= 1 && id <= G_MAXUINT32)
      priv->port_hi_id = id;
  g_free (param_name);

  param_name = g_strdup_printf ("/sources/%s/acoustic/id",
                                hyscan_control_get_source_name (HYSCAN_SOURCE_ECHOSOUNDER));
  if (hyscan_data_box_get_integer (priv->params, param_name, &id))
    if (id >= 1 && id <= G_MAXUINT32)
      priv->echosounder_id = id;
  g_free (param_name);
}

static void
hyscan_ssse_control_server_object_finalize (GObject *object)
{
  HyScanSSSEControlServer *server = HYSCAN_SSSE_CONTROL_SERVER (object);
  HyScanSSSEControlServerPrivate *priv = server->priv;

  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_ssse_control_server_parent_class)->finalize (object);
}

/* Функция создаёт новый объект HyScanSSSEControlServer. */
HyScanSSSEControlServer *
hyscan_ssse_control_server_new (HyScanSonarBox *params)
{
  return g_object_new (HYSCAN_TYPE_SSSE_CONTROL_SERVER,
                       "params",
                       params,
                       NULL);
}

/* Функция передаёт акустические данные от гидролокатора */
void
hyscan_ssse_control_server_send_acoustic_data (HyScanSSSEControlServer *server,
                                               HyScanSourceType         source,
                                               HyScanDataType           type,
                                               gdouble                  rate,
                                               HyScanDataWriterData    *data)
{
  HyScanSSSEControlServerPrivate *priv;

  HyScanSonarMessage message;
  guint32 id = 0;

  g_return_if_fail (HYSCAN_IS_SSSE_CONTROL_SERVER (server));

  priv = server->priv;

  if (priv->params == NULL)
    return;

  switch (source)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
      id = priv->starboard_id;
      break;

    case HYSCAN_SOURCE_SIDE_SCAN_PORT:
      id = priv->port_id;
      break;

    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI:
      id = priv->starboard_hi_id;
      break;

    case HYSCAN_SOURCE_SIDE_SCAN_PORT_HI:
      id = priv->port_hi_id;
      break;

    case HYSCAN_SOURCE_ECHOSOUNDER:
      id = priv->echosounder_id;
      break;

    default:
      return;
    }

  if (id == 0)
    return;

  message.time = data->time;
  message.id   = id;
  message.type = type;
  message.rate = rate;
  message.size = data->size;
  message.data = data->data;

  hyscan_sonar_box_send (HYSCAN_SONAR_BOX (server->priv->params), &message);
}
