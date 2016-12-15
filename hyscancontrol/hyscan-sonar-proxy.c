/*
 * \file hyscan-sonar-proxy.c
 *
 * \brief Исходный файл класса прокси сервера управления гидролокатором
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-proxy.h"
#include "hyscan-sonar-control.h"
#include "hyscan-sonar-control-server.h"
#include "hyscan-control-common.h"
#include "hyscan-proxy-common.h"

#include <string.h>

enum
{
  PROP_O,
  PROP_CONTROL,
  PROP_SONAR_PROXY_MODE,
};

typedef struct
{
  gint64                       time;                           /* Метка времени. */
  gfloat                      *data;                           /* Буфер для данных от гидролокатора. */
  guint32                      max_points;                     /* Размер буфера, в точках. */
  guint32                      n_points;                       /* Размер данных в буфере, в точках. */
  guint                        n_lines;                        /* Число строк данных. */
} HyScanSonarProxyAcousticData;

struct _HyScanSonarProxyPrivate
{
  HyScanSonarControl          *control;                        /* Клиент управления проксируемым гидролокатором. */
  HyScanSonarControlServer    *server;                         /* Прокси сервер гидролокатора. */

  HyScanSonarProxyModeType     proxy_mode;                     /* Режим трансляции команд и данных. */

  GHashTable                  *buffers;                        /* Буферы для данных от гидролокатора по каналам. */

  gfloat                      *input_data;                     /* Буфер для входных данных. */
  guint32                      input_max_points;               /* Размер буфера для входных данных в точках. */

  guint                        side_scale;                     /* Коэффициент масштабирования по наклонной дальности. */
  guint                        track_scale;                    /* Коэффициент масштабирования по оси движения. */
};

static void        hyscan_sonar_proxy_set_property             (GObject                     *object,
                                                                guint                        prop_id,
                                                                const GValue                *value,
                                                                GParamSpec                  *pspec);
static void        hyscan_sonar_proxy_object_constructed       (GObject                     *object);
static void        hyscan_sonar_proxy_object_finalize          (GObject                     *object);

static void        hyscan_sonar_proxy_free_acoustic_data       (gpointer                     data);

static gboolean    hyscan_sonar_proxy_set_sync_type            (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSonarSyncType          sync_type);
static gboolean    hyscan_sonar_proxy_set_position             (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                HyScanAntennaPosition       *position);
static gboolean    hyscan_sonar_proxy_set_receive_time         (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                gdouble                      receive_time);
static gboolean    hyscan_sonar_proxy_start                    (HyScanSonarProxyPrivate     *priv,
                                                                const gchar                 *project_name,
                                                                const gchar                 *track_name,
                                                                HyScanTrackType              track_type);
static gboolean    hyscan_sonar_proxy_stop                     (HyScanSonarProxyPrivate     *priv);
static gboolean    hyscan_sonar_proxy_ping                     (HyScanSonarProxyPrivate     *priv);

static void        hyscan_sonar_proxy_raw_data_forwarder       (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                guint                        channel,
                                                                HyScanRawDataInfo           *info,
                                                                HyScanDataWriterData        *data);
static void        hyscan_sonar_proxy_noise_data_forwarder     (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                guint                        channel,
                                                                HyScanRawDataInfo           *info,
                                                                HyScanDataWriterData        *data);
static void        hyscan_sonar_proxy_acoustic_forwarder       (HyScanSonarProxyPrivate     *priv,
                                                                HyScanSourceType             source,
                                                                HyScanAcousticDataInfo      *info,
                                                                HyScanDataWriterData        *data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarProxy, hyscan_sonar_proxy, HYSCAN_TYPE_TVG_PROXY)

static void
hyscan_sonar_proxy_class_init (HyScanSonarProxyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sonar_proxy_set_property;

  object_class->constructed = hyscan_sonar_proxy_object_constructed;
  object_class->finalize = hyscan_sonar_proxy_object_finalize;

  g_object_class_install_property (object_class, PROP_CONTROL,
    g_param_spec_object ("control", "Control", "Sonar control", HYSCAN_TYPE_SONAR_CONTROL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SONAR_PROXY_MODE,
    g_param_spec_int ("sonar-proxy-mode", "SonarProxyMode", "Sonar proxy mode",
                      HYSCAN_SONAR_PROXY_MODE_ALL, HYSCAN_SONAR_PROXY_MODE_COMPUTED,
                      HYSCAN_SONAR_PROXY_MODE_COMPUTED, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_sonar_proxy_init (HyScanSonarProxy *proxy)
{
  proxy->priv = hyscan_sonar_proxy_get_instance_private (proxy);
}

static void
hyscan_sonar_proxy_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  HyScanSonarProxy *proxy = HYSCAN_SONAR_PROXY (object);
  HyScanSonarProxyPrivate *priv = proxy->priv;

  switch (prop_id)
    {
    case PROP_CONTROL:
      G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->control = g_value_dup_object (value);
      break;

    case PROP_SONAR_PROXY_MODE:
      G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->set_property (object, prop_id, value, pspec);
      priv->proxy_mode = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sonar_proxy_object_constructed (GObject *object)
{
  HyScanSonarProxy *proxy = HYSCAN_SONAR_PROXY (object);
  HyScanSonarProxyPrivate *priv = proxy->priv;

  gint64 version;
  gint64 id;

  /* Буферы для данных от гидролокатора. */
  priv->buffers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                         NULL, hyscan_sonar_proxy_free_acoustic_data);

  priv->side_scale = 1;
  priv->track_scale = 1;

  /* Обязательно должен быть передан указатель на HyScanSonarControl. */
  if (priv->control != NULL)
    {
      gchar *schema_data = NULL;

      if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
        {
          HyScanDataSchema *schema = hyscan_param_schema (HYSCAN_PARAM (priv->control));
          schema_data = hyscan_data_schema_get_data (schema, NULL, NULL);
          g_object_unref (schema);
        }
      else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_COMPUTED)
        {
          HyScanDataSchema *schema;
          HyScanSonarSchema *proxy_schema;

          schema = hyscan_param_schema (HYSCAN_PARAM (priv->control));
          proxy_schema = hyscan_proxy_schema_new (schema, 10.0);
          hyscan_data_schema_builder_schema_join (HYSCAN_DATA_SCHEMA_BUILDER (proxy_schema), "/info",
                                                  schema, "/info");

          hyscan_proxy_schema_sensor_virtual (proxy_schema);
          hyscan_proxy_schema_ssse_acoustic (proxy_schema, priv->control);

          schema_data = hyscan_data_schema_builder_get_data (HYSCAN_DATA_SCHEMA_BUILDER (proxy_schema));

          g_object_unref (proxy_schema);
          g_object_unref (schema);
        }

      /* Устанавливаем схему параметров гидролокатора. */
      hyscan_sonar_box_set_schema (HYSCAN_SONAR_BOX (proxy), schema_data, "sonar");
      g_free (schema_data);

      G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->constructed (object);
    }
  else
    {
      return;
    }

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_param_get_integer (HYSCAN_PARAM (proxy), "/schema/id", &id))
    return;
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    return;
  if (!hyscan_param_get_integer (HYSCAN_PARAM (proxy), "/schema/version", &version))
    return;
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    return;

  /* Прокси сервер. */
  priv->server = hyscan_sonar_control_server_new (HYSCAN_SONAR_BOX (proxy));

  /* Обработчики команд. */
  g_signal_connect_swapped (priv->server, "sonar-set-sync-type",
                            G_CALLBACK (hyscan_sonar_proxy_set_sync_type), priv);
  g_signal_connect_swapped (priv->server, "sonar-set-position",
                            G_CALLBACK (hyscan_sonar_proxy_set_position), priv);
  g_signal_connect_swapped (priv->server, "sonar-set-receive-time",
                            G_CALLBACK (hyscan_sonar_proxy_set_receive_time), priv);
  g_signal_connect_swapped (priv->server, "sonar-start",
                            G_CALLBACK (hyscan_sonar_proxy_start), priv);
  g_signal_connect_swapped (priv->server, "sonar-stop",
                            G_CALLBACK (hyscan_sonar_proxy_stop), priv);
  g_signal_connect_swapped (priv->server, "sonar-ping",
                            G_CALLBACK (hyscan_sonar_proxy_ping), priv);

  /* Обработчики "сырых" данных от гидролокатора. */
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      g_signal_connect_swapped (priv->control, "raw-data",
                                G_CALLBACK (hyscan_sonar_proxy_raw_data_forwarder), priv);
      g_signal_connect_swapped (priv->control, "noise-data",
                                G_CALLBACK (hyscan_sonar_proxy_noise_data_forwarder), priv);
    }

  /* Обработчик акустических данных от гидролокатора. */
    g_signal_connect_swapped (priv->control, "acoustic-data",
                              G_CALLBACK (hyscan_sonar_proxy_acoustic_forwarder), priv);
}

static void
hyscan_sonar_proxy_object_finalize (GObject *object)
{
  HyScanSonarProxy *proxy = HYSCAN_SONAR_PROXY (object);
  HyScanSonarProxyPrivate *priv = proxy->priv;

  g_signal_handlers_disconnect_by_data (priv->control, priv);

  g_clear_object (&priv->server);
  g_clear_object (&priv->control);

  g_free (priv->input_data);
  g_clear_pointer (&priv->buffers, g_hash_table_unref);

  G_OBJECT_CLASS (hyscan_sonar_proxy_parent_class)->finalize (object);
}

/* Функция освобождает память, занятую структурой HyScanAcousticProxyData. */
static void
hyscan_sonar_proxy_free_acoustic_data (gpointer data)
{
  HyScanSonarProxyAcousticData *buffer = data;

  g_free (buffer->data);
  g_free (buffer);
}

/* Команда - hyscan_sonar_control_set_sync_type. */
static gboolean
hyscan_sonar_proxy_set_sync_type (HyScanSonarProxyPrivate *priv,
                                  HyScanSonarSyncType      sync_type)
{
  return hyscan_sonar_control_set_sync_type (priv->control, sync_type);
}

/* Команда - hyscan_sonar_control_set_position. */
static gboolean
hyscan_sonar_proxy_set_position (HyScanSonarProxyPrivate *priv,
                                 HyScanSourceType         source,
                                 HyScanAntennaPosition   *position)
{
  return hyscan_sonar_control_set_position (priv->control, source, position);
}

/* Команда - hyscan_sonar_control_receive_time. */
static gboolean
hyscan_sonar_proxy_set_receive_time (HyScanSonarProxyPrivate *priv,
                                     HyScanSourceType         source,
                                     gdouble                  receive_time)
{
  return hyscan_sonar_control_set_receive_time (priv->control, source, receive_time);
}

/* Команда - hyscan_sonar_control_start. */
static gboolean
hyscan_sonar_proxy_start (HyScanSonarProxyPrivate *priv,
                          const gchar             *project_name,
                          const gchar             *track_name,
                          HyScanTrackType          track_type)
{
  return hyscan_sonar_control_start (priv->control, project_name, track_name, track_type);
}

/* Команда - hyscan_sonar_control_stop. */
static gboolean
hyscan_sonar_proxy_stop (HyScanSonarProxyPrivate *priv)
{
  return hyscan_sonar_control_stop (priv->control);
}

/* Команда - hyscan_sonar_control_ping. */
static gboolean
hyscan_sonar_proxy_ping (HyScanSonarProxyPrivate *priv)
{
  return hyscan_sonar_control_ping (priv->control);
}

/* Функция перенаправляет "сырые" данные от гидролокатора. */
static void
hyscan_sonar_proxy_raw_data_forwarder (HyScanSonarProxyPrivate *priv,
                                       HyScanSourceType         source,
                                       guint                    channel,
                                       HyScanRawDataInfo       *info,
                                       HyScanDataWriterData    *data)
{
  hyscan_sonar_control_server_send_raw_data (priv->server, source, channel, info->data.type, info->data.rate, data);
}

/* Функция перенаправляет данные по шумам от гидролокатора. */
static void
hyscan_sonar_proxy_noise_data_forwarder (HyScanSonarProxyPrivate *priv,
                                         HyScanSourceType         source,
                                         guint                    channel,
                                         HyScanRawDataInfo       *info,
                                         HyScanDataWriterData    *data)
{
  hyscan_sonar_control_server_send_noise_data (priv->server, source, channel, info->data.type, info->data.rate, data);
}

/* Функция перенаправляет акустические данные от гидролокатора. */
static void
hyscan_sonar_proxy_acoustic_forwarder (HyScanSonarProxyPrivate *priv,
                                       HyScanSourceType         source,
                                       HyScanAcousticDataInfo  *info,
                                       HyScanDataWriterData    *data)
{
  /* Перенаправляем данные без обработки. */
  if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      hyscan_sonar_control_server_send_acoustic_data (priv->server, source, info->data.type, info->data.rate, data);
    }

  /* Масштабируем данные. */
  else if (priv->proxy_mode == HYSCAN_SONAR_PROXY_MODE_COMPUTED)
    {
      guint32 input_n_points;
      guint32 output_n_points;

      gfloat accumulator = 0.0;
      guint side_scale = priv->side_scale;
      guint track_scale = priv->track_scale;

      HyScanSonarProxyAcousticData *buffer;

      guint32 i, j;

      /* Масштабирование отключено. */
      if ((track_scale == 1) && (side_scale == 1))
        {
          hyscan_sonar_control_server_send_acoustic_data (priv->server, source, info->data.type, info->data.rate, data);
          return;
        }

      if (hyscan_data_get_point_size (info->data.type) <= 0)
        {
          g_warning ("HyScanSonarProxy:Proxy: unknown data format");
          return;
        }

      buffer = g_hash_table_lookup (priv->buffers, GINT_TO_POINTER (source));
      if (buffer == NULL)
        {
          buffer = g_new0 (HyScanSonarProxyAcousticData, 1);
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
          g_warning ("HyScanSonarProxy: unsupported data format");
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
          hyscan_sonar_control_server_send_acoustic_data (priv->server, source, HYSCAN_DATA_FLOAT, info->data.rate / side_scale, &data);

          memset (buffer->data, 0, buffer->n_points * sizeof (gfloat));
          buffer->n_points = 0;
          buffer->n_lines = 0;
          buffer->time = 0;
        }
    }
}

/* Функция создаёт новый объект HyScanSonarProxy. */
HyScanSonarProxy *
hyscan_sonar_proxy_new (HyScanSonarControl       *control,
                        HyScanSonarProxyModeType  sensor_proxy_mode,
                        HyScanSonarProxyModeType  sonar_proxy_mode)
{
  HyScanSonarProxy *proxy = NULL;

  /* Прокси сервер. */
  proxy = g_object_new (HYSCAN_TYPE_SONAR_PROXY,
                        "control", control,
                        "sensor-proxy-mode", sensor_proxy_mode,
                        "sonar-proxy-mode", sonar_proxy_mode,
                        NULL);

  if (proxy->priv->server == NULL)
    g_clear_object (&proxy);

  return proxy;
}

/* Функция устанавливает коэффициенты масштабирования данных. */
void
hyscan_sonar_proxy_set_scale (HyScanSonarProxy *proxy,
                              guint             side_scale,
                              guint             track_scale)
{
  g_return_if_fail (HYSCAN_IS_SONAR_PROXY (proxy));

  if ((side_scale < 1) || (side_scale > 1024))
    return;

  if ((track_scale < 1) || (track_scale > 1024))
    return;

  proxy->priv->side_scale = side_scale;
  proxy->priv->track_scale = track_scale;
}
