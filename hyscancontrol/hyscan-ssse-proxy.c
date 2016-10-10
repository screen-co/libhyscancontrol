/*
 * \file hyscan-ssse-proxy.c
 *
 * \brief Исходный файл класса прокси сервера ГБОЭ
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control.h"
#include "hyscan-ssse-proxy.h"
#include "hyscan-ssse-control.h"
#include "hyscan-ssse-control-server.h"
#include "hyscan-control-common.h"
#include "hyscan-proxy-common.h"

#include <string.h>

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_PROXY_MODE,
  PROP_SIDE_SCALE,
  PROP_TRACK_SCALE
};

typedef struct
{
  gint64                       time;                           /* Метка времени. */
  gfloat                      *data;                           /* Буфер для данных от гидролокатора. */
  guint32                      max_points;                     /* Размер буфера, в точках. */
  guint32                      n_points;                       /* Размер данных в буфере, в точках. */
  guint                        n_lines;                        /* Число строк данных. */
} HyScanSSSEProxyData;

struct _HyScanSSSEProxyPrivate
{
  HyScanSSSEControl           *control;                        /* Клиент управления проксируемым гидролокатором. */
  HyScanSSSEControlServer     *server;                         /* Сервер прокси гидролокатора. */

  HyScanSonarProxyModeType     proxy_mode;                     /* Режим трансляции команд и данных. */

  GHashTable                  *buffers;                        /* Буферы для данных от гидролокатора по каналам. */

  gfloat                      *input_data;                     /* Буфер для входных данных. */
  gint32                       input_max_points;               /* Размер буфера для входных данных в точках. */

  guint                        side_scale;                     /* Коэффициент масштабирования по наклонной дальности. */
  guint                        track_scale;                    /* Коэффициент масштабирования по оси движения. */
};

static void        hyscan_ssse_proxy_set_property              (GObject                   *object,
                                                                guint                      prop_id,
                                                                const GValue              *value,
                                                                GParamSpec                *pspec);
static void        hyscan_ssse_proxy_object_constructed        (GObject                   *object);
static void        hyscan_ssse_proxy_object_finalize           (GObject                   *object);

static void        hyscan_ssse_proxy_free_data                 (gpointer                   data);

static void        hyscan_ssse_proxy_data_forwarder            (HyScanSSSEProxyPrivate    *priv,
                                                                HyScanSourceType           source,
                                                                HyScanAcousticDataInfo    *info,
                                                                HyScanDataWriterData      *data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSSSEProxy, hyscan_ssse_proxy, HYSCAN_TYPE_SONAR_PROXY)

static void
hyscan_ssse_proxy_class_init (HyScanSSSEProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_ssse_proxy_set_property;

  object_class->constructed = hyscan_ssse_proxy_object_constructed;
  object_class->finalize = hyscan_ssse_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "SSSE control", HYSCAN_TYPE_SSSE_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_PROXY_MODE,
    g_param_spec_int ("proxy-mode", "ProxyMode", "Proxy mode",
                      HYSCAN_SONAR_PROXY_MODE_ALL, HYSCAN_SONAR_PROXY_FORWARD_COMPUTED,
                      HYSCAN_SONAR_PROXY_FORWARD_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SIDE_SCALE,
    g_param_spec_uint ("side-scale", "SideScale", "Side scale",
                      1, 1024, 1, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_TRACK_SCALE,
    g_param_spec_uint ("track-scale", "TrackScale", "Track scale",
                      1, 1024, 1, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_ssse_proxy_init (HyScanSSSEProxy *proxy)
{
  proxy->priv = hyscan_ssse_proxy_get_instance_private (proxy);
}

static void
hyscan_ssse_proxy_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  HyScanSSSEProxy *ssse_proxy = HYSCAN_SSSE_PROXY (object);
  HyScanSSSEProxyPrivate *priv = ssse_proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->control = g_value_dup_object (value);
      break;

    case PROP_PROXY_MODE:
      G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->proxy_mode = g_value_get_int (value);
      break;

    case PROP_SIDE_SCALE:
      priv->side_scale = g_value_get_uint (value);
      break;

    case PROP_TRACK_SCALE:
      priv->track_scale = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_ssse_proxy_object_constructed (GObject *object)
{
  HyScanSSSEProxy *proxy = HYSCAN_SSSE_PROXY (object);
  HyScanSSSEProxyPrivate *priv = proxy->priv;

  gint64 version;
  gint64 id;

  G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->constructed (object);

  /* Буферы для данных от гидролокатора. */
  priv->buffers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_ssse_proxy_free_data);

  /* Обязательно должен быть передан указатель на HyScanSSSEControl. */
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

  /* Прокси сервер. */
  priv->server = hyscan_ssse_control_server_new (HYSCAN_SONAR_BOX (proxy));

  g_signal_connect_swapped (priv->control, "acoustic-data",
                            G_CALLBACK (hyscan_ssse_proxy_data_forwarder), priv);
}

static void
hyscan_ssse_proxy_object_finalize (GObject *object)
{
  HyScanSSSEProxy *proxy = HYSCAN_SSSE_PROXY (object);
  HyScanSSSEProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  g_hash_table_unref (priv->buffers);
  g_free (priv->input_data);

  G_OBJECT_CLASS (hyscan_ssse_proxy_parent_class)->finalize (object);
}

/* Функция освобождает память, занятую структурой HyScanSSSEProxyData. */
static void
hyscan_ssse_proxy_free_data (gpointer data)
{
  HyScanSSSEProxyData *buffer = data;

  g_free (buffer->data);
  g_free (buffer);
}

/* Функция перенаправляет акустические данные от гидролокатора. */
static void
hyscan_ssse_proxy_data_forwarder (HyScanSSSEProxyPrivate *priv,
                                  HyScanSourceType        source,
                                  HyScanAcousticDataInfo *info,
                                  HyScanDataWriterData   *data)
{
  /* Перенаправляем данные без обработки. */
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      hyscan_ssse_control_server_send_acoustic_data (priv->server, source, info->data.type, info->data.rate, data);
    }

  /* Масштабируем данные. */
  else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_COMPUTED)
    {
      gint32 input_n_points;
      gint32 output_n_points;

      gfloat accumulator = 0.0;
      guint side_scale = priv->side_scale;
      guint track_scale = priv->track_scale;

      HyScanSSSEProxyData *buffer;

      gint32 i, j;

      /* Масштабирование отключено. */
      if ((track_scale == 1) && (side_scale == 1))
        {
          hyscan_ssse_control_server_send_acoustic_data (priv->server, source, info->data.type, info->data.rate, data);
          return;
        }

      if (hyscan_data_get_point_size (info->data.type) <= 0)
        {
          g_warning ("HyScanSSSEProxy: unknown data format");
          return;
        }

      buffer = g_hash_table_lookup (priv->buffers, GINT_TO_POINTER (source));
      if (buffer == NULL)
        {
          buffer = g_new0 (HyScanSSSEProxyData, 1);
          g_hash_table_insert (priv->buffers, GINT_TO_POINTER (source), buffer);
        }

      /* Буферы для обработки данных. */
      input_n_points = data->size / hyscan_data_get_point_size (info->data.type);
      output_n_points = input_n_points / side_scale;
      if (priv->input_max_points < input_n_points)
        {
          priv->input_max_points = input_n_points;
          priv->input_data = g_realloc (priv->input_data, input_n_points * sizeof (gfloat));
        }
      if (buffer->max_points < output_n_points)
        {
          buffer->max_points = output_n_points;
          buffer->data = g_realloc (buffer->data, output_n_points * sizeof (gfloat));
          memset (buffer->data + buffer->n_points, 0, (buffer->max_points - buffer->n_points) * sizeof (gfloat));
        }

      /* Данные для обработки. */
      if (!hyscan_data_import_float (info->data.type, data->data, data->size, priv->input_data, &input_n_points))
        {
          g_warning ("HyScanSSSEProxy: unsupported data format");
          return;
        }

      buffer->time += data->time;
      buffer->n_lines += 1;
      buffer->n_points = MAX (buffer->n_points, output_n_points);

      /* Масштабируем данные по наклонной дальности. */
      for (i = 0, j = 0; i <= input_n_points; i++)
        {
          if ((i % side_scale) == 0)
            {
              if (i > 0)
                {
                  accumulator /= side_scale;
                  buffer->data[j] += accumulator;
                  j += 1;
                }

              if (i == input_n_points)
                break;

              accumulator = 0.0;
            }
          accumulator += priv->input_data[i];
        }

      /* Если накопили нужное число строк, отправляем их среднее значение. */
      if (buffer->n_lines == track_scale)
        {
          HyScanDataWriterData data;

          if (track_scale > 1)
            for (i = 0; i < buffer->n_points; i++)
              buffer->data[i] /= track_scale;

          data.time = buffer->time / track_scale;
          data.size = buffer->n_points * sizeof (gfloat);
          data.data = buffer->data;
          hyscan_ssse_control_server_send_acoustic_data (priv->server, source, HYSCAN_DATA_FLOAT, info->data.rate / side_scale, &data);

          memset (buffer->data, 0, buffer->n_points * sizeof (gfloat));
          buffer->n_points = 0;
          buffer->n_lines = 0;
          buffer->time = 0;
        }
    }
}

/* Функция создаёт новый объект HyScanSSSEProxy. */
HyScanSSSEProxy *
hyscan_ssse_proxy_new (HyScanSonar                *sonar,
                       HyScanSonarProxyModeType    proxy_mode,
                       guint                       side_scale,
                       guint                       track_scale,
                       HyScanDB                   *db)
{
  HyScanSSSEProxy *proxy = NULL;
  HyScanSSSEControl *control;
  gchar *schema_data = NULL;

  /* Проверяем тип гидролокатора. */
  if ((sonar == NULL) || (hyscan_control_sonar_probe (sonar) != HYSCAN_SONAR_SSSE))
    return NULL;

  /* Объект управления гидролокатором. */
  control = hyscan_ssse_control_new (sonar, db);

  /* Трансляция 1:1. */
  if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      schema_data = hyscan_data_schema_get_data (HYSCAN_DATA_SCHEMA (sonar));
    }
  else if (proxy_mode == HYSCAN_SONAR_PROXY_FORWARD_COMPUTED)
    {
      HyScanSonarSchema *schema;

      schema = hyscan_proxy_schema_new (sonar, 10.0);
      hyscan_proxy_schema_sensor_virtual (schema);
      hyscan_proxy_schema_ssse_acoustic (schema, sonar, control);
      schema_data = hyscan_data_schema_builder_get_data (HYSCAN_DATA_SCHEMA_BUILDER (schema));
      g_object_unref (schema);
    }
  else
    {
      return NULL;
    }

  proxy = g_object_new (HYSCAN_TYPE_SSSE_PROXY,
                        "control", control,
                        "schema-data", schema_data,
                        "schema-id", "sonar",
                        "proxy-mode", proxy_mode,
                        "side-scale", side_scale,
                        "track-scale", track_scale,
                        NULL);

  g_object_unref (control);
  g_free (schema_data);

  if (proxy->priv->server == NULL)
    g_clear_object (&proxy);

  return proxy;
}

HyScanSSSEControl *
hyscan_ssse_proxy_get_control (HyScanSSSEProxy *proxy)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_PROXY (proxy), NULL);

  return proxy->priv->control;
}