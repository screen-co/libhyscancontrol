/*
 * \file hyscan-proxy-common.c
 *
 * \brief Исходный файл общих функций схем прокси гидролокаторов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-proxy-common.h"
#include "hyscan-control-common.h"

/* Функция создаёт новый объект для определения схемы прокси гидролокатора. */
HyScanSonarSchema *
hyscan_proxy_schema_new (HyScanSonarSyncType sync_capabilities,
                         gdouble             timeout)
{
  HyScanSonarSchema *proxy_schema;

  /* Схема прокси гидролокатора. */
  proxy_schema = hyscan_sonar_schema_new (timeout);

  /* Режимы синхронизации излучения. */
  if (!hyscan_sonar_schema_sync_add (proxy_schema, sync_capabilities))
    {
      g_warning ("HyScanSonarProxy: can't set sync capabilities");
      g_object_unref (proxy_schema);
      return NULL;
    }

  return proxy_schema;
}

/* Функция определяет один виртуальный nmea порт прокси гидролокатора. */
gboolean
hyscan_proxy_schema_sensor_virtual (HyScanSonarSchema *schema)
{
  if (hyscan_sonar_schema_sensor_add (schema, HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME,
                                      HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183) < 0)
    {
      g_warning ("HyScanSonarProxy: can't setup NMEA port");
      return FALSE;
    }

  return TRUE;
}

/* Функция определяет акустические источники прокси гидролокатора. */
gboolean
hyscan_proxy_schema_ssse_acoustic (HyScanSonarSchema  *schema,
                                   HyScanSonarControl *control)
{
  HyScanParam *sonar;
  gboolean status = FALSE;
  gint *sources;
  gint i, j;

  sonar = HYSCAN_PARAM (control);
  sources = hyscan_sonar_control_source_list (control);
  if (sources == NULL)
    goto exit;

  /* Проброс источников акустических данных. */
  for (i = 0; sources[i] != HYSCAN_SOURCE_INVALID; i++)
    {
      const gchar *source_name;

      gchar *param_names[3];
      GVariant *param_values[3];

      gdouble antenna_vpattern;
      gdouble antenna_hpattern;
      gdouble max_receive_time;

      HyScanGeneratorModeType generator_capabilities;
      HyScanGeneratorSignalType generator_signals;
      HyScanTVGModeType tvg_capabilities;

      gdouble min_tone_duration = 0.0;
      gdouble max_tone_duration = 0.0;
      gdouble min_lfm_duration = 0.0;
      gdouble max_lfm_duration = 0.0;
      gdouble min_gain = 0.0;
      gdouble max_gain = 0.0;

      gboolean status;

      /* Обрабатываем только источники акустических данных. */
      if (!hyscan_source_is_acoustic (sources[i]))
        continue;

      source_name = hyscan_control_get_source_name (sources[i]);

      /* Параметры источника данных. */
      param_names[0] = g_strdup_printf ("/sources/%s/antenna/pattern/vertical", source_name);
      param_names[1] = g_strdup_printf ("/sources/%s/antenna/pattern/horizontal", source_name);
      param_names[2] = NULL;

      status = hyscan_param_get (sonar, (const gchar **)param_names, param_values);

      g_free (param_names[0]);
      g_free (param_names[1]);

      if (!status)
        {
          g_warning ("HyScanSonarProxy: can't get '%s' source info", source_name);
          goto exit;
        }

      antenna_vpattern = g_variant_get_double (param_values[0]);
      antenna_hpattern = g_variant_get_double (param_values[1]);

      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);

      max_receive_time = hyscan_sonar_control_get_max_receive_time (HYSCAN_SONAR_CONTROL (control), sources[i]);

      /* Проброс источника акустических данных. */
      if ((hyscan_sonar_schema_source_add (schema, sources[i], antenna_vpattern, antenna_hpattern, max_receive_time) < 0) ||
          (hyscan_sonar_schema_source_add_acuostic (schema, sources[i]) < 0))
        {
          g_warning ("HyScanSonarProxy: can't forward '%s' source", source_name);
          goto exit;
        }

      /* Параметры генератора. */
      generator_capabilities = hyscan_generator_control_get_capabilities (HYSCAN_GENERATOR_CONTROL (control),
                                                                          sources[i]);
      generator_signals = hyscan_generator_control_get_signals (HYSCAN_GENERATOR_CONTROL (control),
                                                                sources[i]);

      /* Параметры тонального сигнала. */
      if (generator_signals & HYSCAN_GENERATOR_SIGNAL_TONE)
        {
          status = hyscan_generator_control_get_duration_range (HYSCAN_GENERATOR_CONTROL (control),
                                                                sources[i],
                                                                HYSCAN_GENERATOR_SIGNAL_TONE,
                                                                &min_tone_duration,
                                                                &max_tone_duration);
          if (!status)
            {
              g_warning ("HyScanSonarProxy: can't get tone signal parameters for '%s' source", source_name);
              goto exit;
            }
        }

      /* Параметры ЛЧМ сигнала. */
      if ((generator_signals & HYSCAN_GENERATOR_SIGNAL_LFM) ||
          (generator_signals & HYSCAN_GENERATOR_SIGNAL_LFMD))
        {
          status = hyscan_generator_control_get_duration_range (HYSCAN_GENERATOR_CONTROL (control),
                                                                sources[i],
                                                                HYSCAN_GENERATOR_SIGNAL_LFM,
                                                                &min_lfm_duration,
                                                                &max_lfm_duration);
          if (!status)
            {
              g_warning ("HyScanSonarProxy: can't get lfm signal parameters for '%s' source", source_name);
              goto exit;
            }
        }

      /* Описание генератора. */
      if (hyscan_sonar_schema_generator_add (schema, sources[i],
                                             generator_capabilities, generator_signals,
                                             min_tone_duration, max_tone_duration,
                                             min_lfm_duration, max_lfm_duration) < 0)
        {
          g_warning ("HyScanSonarProxy: can't setup generator parameters for '%s' source", source_name);
          goto exit;
        }

      /* Преднастройки генератора. */
      if (generator_capabilities & HYSCAN_GENERATOR_MODE_PRESET)
        {
          HyScanDataSchemaEnumValue **presets;

          presets = hyscan_generator_control_list_presets (HYSCAN_GENERATOR_CONTROL (control), sources[i]);
          for (j = 0; presets != NULL && presets[j] != NULL; j++)
            {
              if (presets[j]->value == 0)
                continue;

              if (hyscan_sonar_schema_generator_add_preset (schema, sources[i], presets[j]->name) < 0)
                {
                  g_warning ("HyScanSonarProxy: can't setup '%s' generator '%s' preset", source_name, presets[j]->name);
                  goto exit;
                }
            }

          hyscan_data_schema_free_enum_values (presets);
        }

      /* Параметры ВАРУ. */
      tvg_capabilities = hyscan_tvg_control_get_capabilities (HYSCAN_TVG_CONTROL (control), sources[i]);
      if ((tvg_capabilities ^ HYSCAN_TVG_MODE_AUTO))
        {
          status = hyscan_tvg_control_get_gain_range (HYSCAN_TVG_CONTROL (control), sources[i], &min_gain, &max_gain);
          if (!status)
            {
              g_warning ("HyScanSonarProxy: can't get tvg parameters for '%s' source", source_name);
              goto exit;
            }

          if (hyscan_sonar_schema_tvg_add (schema, sources[i], tvg_capabilities, min_gain, max_gain) < 0)
            {
              g_warning ("HyScanSonarProxy: can't setup tvg parameters for '%s' source", source_name);
              goto exit;
            }
        }
    }

  g_free (sources);

  status = TRUE;

exit:
  return status;
}
