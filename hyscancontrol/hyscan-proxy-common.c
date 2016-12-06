/*
 * \file hyscan-proxy-common.c
 *
 * \brief Исходный файл общих функций схем прокси гидролокаторов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control-common.h"
#include "hyscan-proxy-common.h"

/* Функция создаёт новый объект для определения схемы прокси гидролокатора. */
HyScanSonarSchema *
hyscan_proxy_schema_new (HyScanDataSchema *schema,
                         gdouble           timeout)
{
  HyScanSonarSchema *proxy_schema;

  GVariant *value;
  HyScanSonarSyncType sync_capabilities = 0;

  value = hyscan_data_schema_key_get_default (schema, "/sync/capabilities");
  if (value != NULL)
    {
      sync_capabilities = g_variant_get_int64 (value);
      g_variant_unref (value);
    }
  else
    {
      g_warning ("HyScanSonarProxy: can't get sync capabilities");
      return NULL;
    }

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
hyscan_proxy_schema_ssse_acoustic (HyScanSonarSchema *schema,
                                   HyScanParam       *sonar,
                                   HyScanSSSEControl *control)
{
  gboolean status = FALSE;

  HyScanSourceType sources[] = { HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                 HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                 HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI,
                                 HYSCAN_SOURCE_SIDE_SCAN_PORT_HI,
                                 HYSCAN_SOURCE_ECHOSOUNDER,
                                 HYSCAN_SOURCE_INVALID};

  gint i, j;

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

      /* Проверка наличия источника данных. */
      if (sources[i] == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
        {
          if (!hyscan_ssse_control_has_starboard (control))
            continue;
        }
      else if(sources[i] == HYSCAN_SOURCE_SIDE_SCAN_PORT)
        {
          if (!hyscan_ssse_control_has_port (control))
            continue;
        }
      else if(sources[i] == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI)
        {
          if (!hyscan_ssse_control_has_starboard_hi (control))
            continue;
        }
      else if(sources[i] == HYSCAN_SOURCE_SIDE_SCAN_PORT_HI)
        {
          if (!hyscan_ssse_control_has_port_hi (control))
            continue;
        }
      else if(sources[i] == HYSCAN_SOURCE_ECHOSOUNDER)
        {
          if (!hyscan_ssse_control_has_echosounder (control))
            continue;
        }
      else
        {
          continue;
        }

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
          g_warning ("HyScanSSSEProxy: can't get '%s' source info", source_name);
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
          g_warning ("HyScanSSSEProxy: can't forward '%s' source", source_name);
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
              g_warning ("HyScanSSSEProxy: can't get tone signal parameters for '%s' source", source_name);
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
              g_warning ("HyScanSSSEProxy: can't get lfm signal parameters for '%s' source", source_name);
              goto exit;
            }
        }

      /* Описание генератора. */
      if (hyscan_sonar_schema_generator_add (schema, sources[i],
                                             generator_capabilities, generator_signals,
                                             min_tone_duration, max_tone_duration,
                                             min_lfm_duration, max_lfm_duration) < 0)
        {
          g_warning ("HyScanSSSEProxy: can't setup generator parameters for '%s' source", source_name);
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
                  g_warning ("HyScanSSSEProxy: can't setup '%s' generator '%s' preset", source_name, presets[j]->name);
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
              g_warning ("HyScanSSSEProxy: can't get tvg parameters for '%s' source", source_name);
              goto exit;
            }

          if (hyscan_sonar_schema_tvg_add (schema, sources[i], tvg_capabilities, min_gain, max_gain) < 0)
            {
              g_warning ("HyScanSSSEProxy: can't setup tvg parameters for '%s' source", source_name);
              goto exit;
            }
        }
    }

  status = TRUE;

exit:
  return status;
}
