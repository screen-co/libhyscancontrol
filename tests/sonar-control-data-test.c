
#include "hyscan-sonar-schema.h"
#include "hyscan-ssse-control.h"
#include "hyscan-sensor-control-server.h"
#include "hyscan-generator-control-server.h"
#include "hyscan-tvg-control-server.h"
#include "hyscan-sonar-control-server.h"
#include "hyscan-ssse-control-server.h"
#include "hyscan-ssse-proxy.h"

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define PROJECT_NAME                   "test"

#define FLOAT_EPSILON                  1e-5

#define N_TESTS                        16
#define SONAR_N_SOURCES                5
#define SENSOR_N_PORTS                 4

#define DATA_N_POINTS                  4096
#define SIGNAL_N_POINTS                1024
#define TVG_N_GAINS                    512

#define SIDE_SCALE                     4
#define TRACK_SCALE                    4

typedef struct
{
  HyScanAntennaPosition                position;
  gint64                               time_offset;
} PortInfo;

typedef struct
{
  HyScanAntennaPosition                position;
  HyScanRawDataInfo                    raw_info;
  HyScanAcousticDataInfo               acoustic_info;
  gdouble                              tvg_rate;
} SourceInfo;

typedef struct
{
  HyScanSensorControlServer           *sensor;
  HyScanGeneratorControlServer        *generator;
  HyScanTVGControlServer              *tvg;
  HyScanSonarControlServer            *sonar;
  HyScanSSSEControlServer             *ssse;
} ServerInfo;

PortInfo                               ports[SENSOR_N_PORTS];
SourceInfo                             sources[SONAR_N_SOURCES];

/* Функция возвращает тип источника данных по его индексу. */
HyScanSourceType
select_source_by_index (guint index)
{
  if (index == 0)
    return HYSCAN_SOURCE_ECHOSOUNDER;

  if (index == 1)
    return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;

  if (index == 2)
    return HYSCAN_SOURCE_SIDE_SCAN_PORT;

  if (index == 3)
    return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI;

  if (index == 4)
    return HYSCAN_SOURCE_SIDE_SCAN_PORT_HI;

  return HYSCAN_SOURCE_INVALID;
}

/* Функция возвращает информацию об источнике данных по его индексу. */
SourceInfo *
source_info_by_index (guint index)
{
  if (index < SONAR_N_SOURCES)
    return &sources[index];

  return NULL;
}

/* Функция возвращает название порта по его индексу. */
const gchar *
select_port_by_index (guint index)
{
  const gchar *names[SENSOR_N_PORTS] = {"nmea", "port.1", "port.2", "port.3" };

  if (index < SENSOR_N_PORTS)
    return names[index];

  return NULL;
}

/* функция возвращает информацию о порте по его индексу. */
PortInfo *
port_info_by_index (guint index)
{
  if (index < SENSOR_N_PORTS)
    return &ports[index];

  return NULL;
}

/* Функция выполняет цикл зондирования. */
gboolean
sonar_ping_cb (ServerInfo *server)
{
  gfloat *values = g_new (gfloat, DATA_N_POINTS);
  HyScanComplexFloat *signal_points = g_new (HyScanComplexFloat, SIGNAL_N_POINTS);
  gfloat *tvg_gains = g_new (gfloat, TVG_N_GAINS);

  static gint n_track = 0;
  gint i, j, k;

  /* Имитация данных. */
  for (i = 0; i < N_TESTS; i++)
    {
      HyScanDataWriterData data;
      HyScanDataWriterSignal signal;
      HyScanDataWriterTVG tvg;

      gchar *nmea;
      gchar *nmea_rmc;
      gchar *nmea_gga;
      gchar *nmea_dpt;
      guchar nmea_crc;
      gsize nmea_len;

      gint64 time = 1000 * (i + 1);

      /* Имитация гидролокационных данных. */
      for (j = 0; j < SONAR_N_SOURCES; j++)
        {
          HyScanSourceType source = select_source_by_index (j);
          SourceInfo *info = source_info_by_index (j);

          /* Гидролокационные данные. */
          for (k = 0; k < DATA_N_POINTS; k++)
            values[k] = (i / TRACK_SCALE) + j + (k / SIDE_SCALE) + n_track;

          data.time = time;
          data.size = DATA_N_POINTS * sizeof (gfloat);
          data.data = values;

          hyscan_sonar_control_server_send_raw_data (server->sonar, source, 1, HYSCAN_DATA_FLOAT,
                                                     info->raw_info.data.rate, &data);

          hyscan_sonar_control_server_send_noise_data (server->sonar, source, 1, HYSCAN_DATA_FLOAT,
                                                       info->raw_info.data.rate, &data);

          hyscan_ssse_control_server_send_acoustic_data (HYSCAN_SSSE_CONTROL_SERVER (server->ssse),
                                                         source, HYSCAN_DATA_FLOAT,
                                                         info->acoustic_info.data.rate, &data);

          /* Сигналы и ВАРУ. */
          if ((i > 0) && (i == n_track))
            {
              /* Образ сигнала. */
              for (k = 0; k < SIGNAL_N_POINTS; k++)
                {
                  signal_points[k].re = j + k + n_track;
                  signal_points[k].im = -signal_points[k].re;
                }

              signal.time = time;
              signal.rate = info->raw_info.data.rate;
              signal.n_points = SIGNAL_N_POINTS;
              signal.points = signal_points;
              hyscan_generator_control_server_send_signal (HYSCAN_GENERATOR_CONTROL_SERVER (server->generator), source, &signal);

              /* Параметры ВАРУ. */
              for (k = 0; k < TVG_N_GAINS; k++)
                  tvg_gains[k] = j + k + n_track;

              tvg.time = time;
              tvg.rate = info->tvg_rate;
              tvg.n_gains = TVG_N_GAINS;
              tvg.gains = tvg_gains;
              hyscan_tvg_control_server_send_gains (HYSCAN_TVG_CONTROL_SERVER (server->tvg), source, &tvg);
            }
        }

      /* Имитация данных датчиков. */
      for (j = 0; j < SENSOR_N_PORTS; j++)
        {
          const gchar *name = select_port_by_index (j);

          nmea_gga = g_strdup_printf ("$GPGGA,PORT.%d DUMMY DATA %d,*00", j, i + n_track + 1);
          nmea_rmc = g_strdup_printf ("$GPRMC,PORT.%d DUMMY DATA %d,*00", j, i + n_track + 1);
          nmea_dpt = g_strdup_printf ("$GPDPT,PORT.%d DUMMY DATA %d,*00", j, i + n_track + 1);

          nmea_crc = 0;
          nmea_len = strlen (nmea_gga);
          for (k = 1; k < nmea_len - 3; k++)
            nmea_crc ^= nmea_gga[k];
          g_snprintf (nmea_gga + nmea_len - 2, 3, "%02X", nmea_crc);

          nmea_crc = 0;
          nmea_len = strlen (nmea_rmc);
          for (k = 1; k < nmea_len - 3; k++)
            nmea_crc ^= nmea_rmc[k];
          g_snprintf (nmea_rmc + nmea_len - 2, 3, "%02X", nmea_crc);

          nmea_crc = 0;
          nmea_len = strlen (nmea_dpt);
          for (k = 1; k < nmea_len - 3; k++)
            nmea_crc ^= nmea_dpt[k];
          g_snprintf (nmea_dpt + nmea_len - 2, 3, "%02X", nmea_crc);

          nmea = g_strdup_printf ("%s\r\n%s\r\n%s\r\n", nmea_gga, nmea_rmc, nmea_dpt);

          data.time = time;
          data.data = nmea;
          data.size = strlen (nmea);
          hyscan_sensor_control_server_send_data (HYSCAN_SENSOR_CONTROL_SERVER (server->sensor),
                                                  name, HYSCAN_DATA_STRING, &data);

          g_free (nmea);
          g_free (nmea_gga);
          g_free (nmea_rmc);
          g_free (nmea_dpt);
        }
    }

  g_free (signal_points);
  g_free (tvg_gains);
  g_free (values);

  n_track += 1;

  return TRUE;
}

/* Функция проверяет управление гидролокатором. */
void
generate_data (HyScanSSSEControl        *control,
               const gchar              *project_name,
               HyScanSonarProxyModeType  proxy_mode)
{
  guint i;
  guint sensor_n_ports = (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL) ? SENSOR_N_PORTS : 1;

  /* Местоположение приёмных антенн датчиков. */
  for (i = 0; i < sensor_n_ports; i++)
    {
      PortInfo *port = port_info_by_index (i);
      const gchar *name = select_port_by_index (i);

      hyscan_sensor_control_set_position (HYSCAN_SENSOR_CONTROL (control), name, &port->position);
      hyscan_sensor_control_set_virtual_port_param (HYSCAN_SENSOR_CONTROL (control), name, i + 1, port->time_offset);
      hyscan_sensor_control_set_enable (HYSCAN_SENSOR_CONTROL (control), name, TRUE);
    }

  /* Местоположение приёмных антенн гидролокатора. */
  for (i = 0; i < SONAR_N_SOURCES; i++)
    {
      HyScanSourceType source = select_source_by_index (i);
      SourceInfo *info = source_info_by_index (i);

      hyscan_sonar_control_set_position (HYSCAN_SONAR_CONTROL (control), source, &info->position);
    }

  /* Включаем гидролокатор в работу. */
  hyscan_sonar_control_set_data_mode (HYSCAN_SONAR_CONTROL (control), HYSCAN_SONAR_DATA_BOTH);

  for (i = 0; i < N_TESTS; i++)
    {
      gchar *track_name = g_strdup_printf ("test-track-%d", i);
      HyScanTrackType track_type = HYSCAN_TRACK_SURVEY + (i % 3);

      hyscan_sonar_control_start (HYSCAN_SONAR_CONTROL (control), project_name, track_name, track_type);
      hyscan_sonar_control_ping (HYSCAN_SONAR_CONTROL (control));

      g_free (track_name);
    }

  /* Выключаем гидролокатор. */
  hyscan_sonar_control_stop (HYSCAN_SONAR_CONTROL (control));
}

/* Функция проверяет записанные данные. */
void
check_data (HyScanDB                 *db,
            gint32                    project_id,
            HyScanSonarProxyModeType  proxy_mode)
{
  gint32 track_id;

  gfloat *values;
  HyScanComplexFloat *signal_points;
  gfloat *tvg_gains;

  gpointer buffer;
  gint32 buffer_size;
  gint32 data_size;
  gint64 time;

  guint sensor_n_ports = (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL) ? SENSOR_N_PORTS : 1;
  guint sonar_n_sub_types = (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL) ? 3 : 1;
  guint side_scale = (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL) ? 1 : SIDE_SCALE;
  guint track_scale = (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL) ? 1 : TRACK_SCALE;
  gint n_track;
  gint i, j, k;

  buffer_size = DATA_N_POINTS * sizeof(HyScanComplexFloat);
  buffer = g_malloc (buffer_size);

  /* Проверяем данные по галсам. */
  for (n_track = 0; n_track < N_TESTS; n_track++)
    {
      gchar *track_name = g_strdup_printf ("test-track-%d", n_track);

      track_id = hyscan_db_track_open (db, project_id, track_name);
      if (track_id < 0)
        g_error ("can't open %s", track_name);

      /* Проверяем данные от датчиков. */
      for (j = 0; j < sensor_n_ports; j++)
        {
          PortInfo *port = port_info_by_index (j);
          gint n_source;

          /* Проверяем данные по типам. */
          for (n_source = 0; n_source < 4; n_source++)
            {
              HyScanSourceType source;
              const gchar *channel_name;
              gint32 channel_id;
              gint32 param_id;

              gdouble double_value;

              if (n_source == 0)
                source = HYSCAN_SOURCE_NMEA_ANY;
              else if (n_source == 1)
                source = HYSCAN_SOURCE_NMEA_GGA;
              else if (n_source == 2)
                source = HYSCAN_SOURCE_NMEA_RMC;
              else if (n_source == 3)
                source = HYSCAN_SOURCE_NMEA_DPT;
              else
                break;

              channel_name = hyscan_channel_get_name_by_types (source, TRUE, j + 1);

              channel_id = hyscan_db_channel_open (db, track_id, channel_name);
              if (channel_id < 0)
                g_error ("can't open channel %s.%s", track_name, channel_name);

              param_id = hyscan_db_channel_param_open (db, channel_id);
              if (param_id < 0)
                g_error ("can't open channel %s.%s parameters", track_name, channel_name);

              /* Местоположение антенн. */
              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/x", &double_value) ||
                  fabs (double_value - port->position.x) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/x' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/y", &double_value) ||
                  fabs (double_value - port->position.y) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/y' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/z", &double_value) ||
                  fabs (double_value - port->position.z) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/z' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/psi", &double_value) ||
                  fabs (double_value - port->position.psi) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/psi' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/gamma", &double_value) ||
                  fabs (double_value - port->position.gamma) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/gamma' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/theta", &double_value) ||
                  fabs (double_value - port->position.theta) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/theta' error", track_name, channel_name);
                }

              /* Проверяем данные. */
              for (i = 0; i < N_TESTS; i++)
                {
                  gchar *nmea;
                  gchar *nmea_rmc;
                  gchar *nmea_gga;
                  gchar *nmea_dpt;
                  guchar nmea_crc;
                  gsize nmea_len;
                  const gchar *cur_nmea = NULL;

                  nmea_gga = g_strdup_printf ("$GPGGA,PORT.%d DUMMY DATA %d,*00", j, i + n_track + 1);
                  nmea_rmc = g_strdup_printf ("$GPRMC,PORT.%d DUMMY DATA %d,*00", j, i + n_track + 1);
                  nmea_dpt = g_strdup_printf ("$GPDPT,PORT.%d DUMMY DATA %d,*00", j, i + n_track + 1);

                  nmea_crc = 0;
                  nmea_len = strlen (nmea_gga);
                  for (k = 1; k < nmea_len - 3; k++)
                    nmea_crc ^= nmea_gga[k];
                  g_snprintf (nmea_gga + nmea_len - 2, 3, "%02X", nmea_crc);

                  nmea_crc = 0;
                  nmea_len = strlen (nmea_rmc);
                  for (k = 1; k < nmea_len - 3; k++)
                    nmea_crc ^= nmea_rmc[k];
                  g_snprintf (nmea_rmc + nmea_len - 2, 3, "%02X", nmea_crc);

                  nmea_crc = 0;
                  nmea_len = strlen (nmea_dpt);
                  for (k = 1; k < nmea_len - 3; k++)
                    nmea_crc ^= nmea_dpt[k];
                  g_snprintf (nmea_dpt + nmea_len - 2, 3, "%02X", nmea_crc);

                  nmea = g_strdup_printf ("%s\r\n%s\r\n%s", nmea_gga, nmea_rmc, nmea_dpt);

                  if (source == HYSCAN_SOURCE_NMEA_ANY)
                    cur_nmea = nmea;
                  else if (source == HYSCAN_SOURCE_NMEA_GGA)
                    cur_nmea = nmea_gga;
                  else if (source == HYSCAN_SOURCE_NMEA_RMC)
                    cur_nmea = nmea_rmc;
                  else if (source == HYSCAN_SOURCE_NMEA_DPT)
                    cur_nmea = nmea_dpt;

                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, i, buffer, &data_size, &time) ||
                      (time - port->time_offset != (i + 1) * 1000) ||
                      (strncmp (buffer, cur_nmea, strlen (cur_nmea)) != 0))
                    {
                      g_error ("%s.%s: can't get data or data error", track_name, channel_name);
                    }

                  g_free (nmea);
                  g_free (nmea_gga);
                  g_free (nmea_rmc);
                  g_free (nmea_dpt);
                }

              hyscan_db_close (db, param_id);
              hyscan_db_close (db, channel_id);
            }
        }

      /* Проверяем гидролокационные источники данных. */
      for (j = 0; j < SONAR_N_SOURCES; j++)
        {
          HyScanSourceType source = select_source_by_index (j);
          SourceInfo *info = source_info_by_index (j);

          gint32 channel_id;
          gint32 param_id;

          gdouble double_value;
          gint64 integer_value;
          gchar *string_value;

          gint sub_type;
          gint n_signal;

          for (sub_type = 0; sub_type < sonar_n_sub_types; sub_type++)
            {
              gchar *channel_name;
              gboolean raw = (sub_type > 0) ? TRUE : FALSE;
              gdouble signal_rate = (sub_type > 0) ? info->raw_info.data.rate : info->acoustic_info.data.rate;

              if (sub_type < 2)
                channel_name = g_strdup (hyscan_channel_get_name_by_types (source, raw, 1));
              else
                channel_name = g_strdup_printf ("%s-noise", hyscan_channel_get_name_by_types (source, raw, 1));

              channel_id = hyscan_db_channel_open (db, track_id, channel_name);
              if (channel_id < 0)
                g_error ("can't open channel %s.%s", track_name, channel_name);

              param_id = hyscan_db_channel_param_open (db, channel_id);
              if (param_id < 0)
                g_error ("can't open channel %s.%s parameters", track_name, channel_name);

              /* Местоположение антенн. */
              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/x", &double_value) ||
                  fabs (double_value - info->position.x) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/x' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/y", &double_value) ||
                  fabs (double_value - info->position.y) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/y' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/z", &double_value) ||
                  fabs (double_value - info->position.z) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/z' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/psi", &double_value) ||
                  fabs (double_value - info->position.psi) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/psi' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/gamma", &double_value) ||
                  fabs (double_value - info->position.gamma) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/gamma' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/position/theta", &double_value) ||
                  fabs (double_value - info->position.theta) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/position/theta' error", track_name, channel_name);
                }

              /* Параметры данных. */
              string_value = hyscan_db_param_get_string (db, param_id, NULL, "/data/type");
              if (g_strcmp0 (string_value, hyscan_data_get_type_name (HYSCAN_DATA_FLOAT)) != 0)
                g_error ("%s.%s: '/data/type' error", track_name, channel_name);
              g_free (string_value);

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/data/rate", &double_value) ||
                  fabs ((side_scale * double_value) - signal_rate) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/data/rate' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/antenna/pattern/vertical", &double_value) ||
                  fabs (double_value - info->raw_info.antenna.pattern.vertical) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/antenna/pattern/vertical' error", track_name, channel_name);
                }

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/antenna/pattern/horizontal", &double_value) ||
                  fabs (double_value - info->raw_info.antenna.pattern.horizontal) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/antenna/pattern/horizontal' error", track_name, channel_name);
                }

              if (raw)
                {
                  if (!hyscan_db_param_get_double (db, param_id, NULL, "/antenna/offset/vertical", &double_value) ||
                      fabs (double_value - info->raw_info.antenna.offset.vertical) > FLOAT_EPSILON)
                    {
                      g_error ("%s.%s: '/antenna/offset/vertical' error", track_name, channel_name);
                    }

                  if (!hyscan_db_param_get_double (db, param_id, NULL, "/antenna/offset/horizontal", &double_value) ||
                      fabs (double_value - info->raw_info.antenna.offset.horizontal) > FLOAT_EPSILON)
                    {
                      g_error ("%s.%s: '/antenna/offset/horizontal' error", track_name, channel_name);
                    }

                  if (!hyscan_db_param_get_double (db, param_id, NULL, "/adc/vref", &double_value) ||
                      fabs (double_value - info->raw_info.adc.vref) > FLOAT_EPSILON)
                    {
                      g_error ("%s.%s: '/adc/vref' error", track_name, channel_name);
                    }

                  if (!hyscan_db_param_get_integer (db, param_id, NULL, "/adc/offset", &integer_value) ||
                      integer_value != info->raw_info.adc.offset)
                    {
                      g_error ("%s.%s: '/adc/offset' error", track_name, channel_name);
                    }
                }

              /* Гидролокационные данные. */
              values = buffer;
              for (i = 0; i < (N_TESTS / track_scale); i++)
                {
                  gint64 ref_time;

                  if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
                    ref_time = 1000 * (i + 1);
                  else
                    ref_time = (1000 * (i * TRACK_SCALE + 1)) + (1000 * (TRACK_SCALE - 1) / 2);

                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, i, buffer, &data_size, &time) ||
                      data_size != (gint)((DATA_N_POINTS / side_scale)  * sizeof (gfloat)) ||
                      time != ref_time)
                    {
                      g_error ("%s.%s: can't get data", track_name, channel_name);
                    }

                  for (k = 0; k < (DATA_N_POINTS / side_scale); k++)
                    {
                      gfloat ref_value;

                      if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
                        ref_value = (i / TRACK_SCALE) + j + (k / SIDE_SCALE) + n_track;
                      else
                        ref_value = i + j + k + n_track;

                      if (values[k] != ref_value)
                        g_error ("%s.%s: data error", track_name, channel_name);
                    }
                }

              hyscan_db_close (db, param_id);
              hyscan_db_close (db, channel_id);
              g_free (channel_name);
            }

          /* Образы сигналов и ВАРУ. */
          if ((n_track > 0) && (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL))
            {
              gchar *signal_name;
              gchar *tvg_name;

              signal_name = g_strdup_printf ("%s-signal", hyscan_channel_get_name_by_types (source, TRUE, 1));

              channel_id = hyscan_db_channel_open (db, track_id, signal_name);
              if (channel_id < 0)
                g_error ("can't open channel %s.%s", track_name, signal_name);

              param_id = hyscan_db_channel_param_open (db, channel_id);
              if (param_id < 0)
                g_error ("can't open channel %s.%s parameters", track_name, signal_name);

              string_value = hyscan_db_param_get_string (db, param_id, NULL, "/data/type");
              if (g_strcmp0 (string_value, hyscan_data_get_type_name (HYSCAN_DATA_COMPLEX_FLOAT)) != 0)
                g_error ("%s.%s: '/data/type' error", track_name, signal_name);
              g_free (string_value);

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/data/rate", &double_value) ||
                  fabs (double_value - info->raw_info.data.rate) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/data/rate' error", track_name, signal_name);
                }

              /* Образ сигнала.
               *
               * В галсе с номером 0 сигналов нет.
               * В галсе с номером 1 один сигнал.
               * В остальных галсах по два сигнала, один от предыдущего галса, другой текущий.
               *
               */
              signal_points = buffer;
              for (n_signal = 0; n_signal < ((n_track == 1) ? 1 : 2); n_signal++)
                {
                  gint x = (n_track == 1) ? 1 : 0;

                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, n_signal, buffer, &data_size, &time) ||
                      data_size != SIGNAL_N_POINTS * sizeof (HyScanComplexFloat) ||
                      time != 1000 * (n_track + n_signal + x))
                    {
                      g_error ("%s.%s: can't get data", track_name, signal_name);
                    }

                  for (k = 0; k < SIGNAL_N_POINTS; k++)
                    {
                      gdouble signal_value = j + k + n_track + (n_signal - 1) + x;

                      if (fabs (signal_points[k].re - signal_value) > FLOAT_EPSILON ||
                          fabs (signal_points[k].im + signal_value) > FLOAT_EPSILON)
                        {
                          g_error ("%s.%s: data error %d %f %f %f", track_name, signal_name, n_signal, signal_points[k].re, signal_points[k].im, signal_value);
                        }
                    }
                }

              g_free (signal_name);
              hyscan_db_close (db, param_id);
              hyscan_db_close (db, channel_id);

              tvg_name = g_strdup_printf ("%s-tvg", hyscan_channel_get_name_by_types (source, TRUE, 1));

              channel_id = hyscan_db_channel_open (db, track_id, tvg_name);
              if (channel_id < 0)
                g_error ("can't open channel %s.%s", track_name, tvg_name);

              param_id = hyscan_db_channel_param_open (db, channel_id);
              if (param_id < 0)
                g_error ("can't open channel %s.%s parameters", track_name, tvg_name);

              string_value = hyscan_db_param_get_string (db, param_id, NULL, "/data/type");
              if (g_strcmp0 (string_value, hyscan_data_get_type_name (HYSCAN_DATA_FLOAT)) != 0)
                g_error ("%s.%s: '/data/type' error", track_name, tvg_name);
              g_free (string_value);

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/data/rate", &double_value) ||
                  fabs (double_value - info->tvg_rate) > FLOAT_EPSILON)
                {
                  g_error ("%s.%s: '/data/rate' error", track_name, tvg_name);
                }

              /* Параметры ВАРУ.
               *
               * В галсе с номером 0 параметров ВАРУ нет.
               * В галсе с номером 1 одна запись с параметрами ВАРУ.
               * В остальных галсах по две записи, одна от предыдущего галса, другая текущая.
               *
               */
              tvg_gains = buffer;
              for (n_signal = 0; n_signal < ((n_track == 1) ? 1 : 2); n_signal++)
                {
                  gint x = (n_track == 1) ? 1 : 0;

                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, n_signal, buffer, &data_size, &time) ||
                      data_size != TVG_N_GAINS * sizeof (gfloat) ||
                      time != 1000 * (n_track + n_signal + x))
                    {
                      g_error ("%s.%s: can't get data", track_name, tvg_name);
                    }

                  for (k = 0; k < TVG_N_GAINS; k++)
                    {
                      gdouble tvg_gain = j + k + n_track + (n_signal - 1) + x;

                      if (fabs (tvg_gains[k] - tvg_gain) > FLOAT_EPSILON)
                        {
                          g_error ("%s.%s: data error", track_name, tvg_name);
                        }
                    }
                }

              g_free (tvg_name);
              hyscan_db_close (db, param_id);
              hyscan_db_close (db, channel_id);
            }
        }

      hyscan_db_close (db, track_id);
      g_free (track_name);
    }

  g_free (buffer);
}

int
main (int    argc,
      char **argv)
{
  HyScanSonarSchema *schema;
  gchar *schema_data;

  ServerInfo server;
  HyScanSonarBox *sonar;
  HyScanSSSEControl *control;
  HyScanSSSEProxy *proxy;

  gchar *db_uri = NULL;
  HyScanDB *db;
  gint32 project_id;

  gchar *proxy_mode_string = NULL;
  HyScanSonarProxyModeType proxy_mode = 0;

  guint i;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "proxy", 'p', 0, G_OPTION_ARG_STRING, &proxy_mode_string, "Proxy mode (all, computed)", NULL },
        { NULL } };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message (error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    if (proxy_mode_string != NULL)
      {
        if (g_strcmp0 (proxy_mode_string, "all") == 0)
          {
            proxy_mode = HYSCAN_SONAR_PROXY_MODE_ALL;
          }
        else if (g_strcmp0 (proxy_mode_string, "computed") == 0)
          {
            proxy_mode = HYSCAN_SONAR_PROXY_FORWARD_COMPUTED;
          }
        else
          {
            g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
            return 0;
          }
      }
    else
      {
        proxy_mode = HYSCAN_SONAR_PROXY_MODE_ALL;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Схема гидролокатора. */
  schema = hyscan_sonar_schema_new (HYSCAN_SONAR_SCHEMA_DEFAULT_TIMEOUT);

  /* Порты для датчиков. */
  for (i = 0; i < SENSOR_N_PORTS; i++)
    {
      PortInfo *port = port_info_by_index (i);
      const gchar *name = select_port_by_index (i);

      port->position.x = g_random_double ();
      port->position.y = g_random_double ();
      port->position.z = g_random_double ();
      port->position.psi = g_random_double ();
      port->position.gamma = g_random_double ();
      port->position.theta = g_random_double ();
      port->time_offset = g_random_int_range (0, 1000);

      hyscan_sonar_schema_sensor_add (schema, name,
                                      HYSCAN_SENSOR_PORT_VIRTUAL,
                                      HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
    }

  /* Типы синхронизации излучения. */
  hyscan_sonar_schema_sync_add (schema, HYSCAN_SONAR_SYNC_SOFTWARE);

  /* Источники данных. */
  for (i = 0; i < SONAR_N_SOURCES; i++)
    {
      HyScanSourceType source = select_source_by_index (i);
      SourceInfo *info = source_info_by_index (i);

      info->position.x = g_random_double ();
      info->position.y = g_random_double ();
      info->position.z = g_random_double ();
      info->position.psi = g_random_double ();
      info->position.gamma = g_random_double ();
      info->position.theta = g_random_double ();

      info->raw_info.data.type = HYSCAN_DATA_ADC_16LE;
      info->raw_info.data.rate = 100000.0;
      info->raw_info.antenna.pattern.vertical = g_random_double ();
      info->raw_info.antenna.pattern.horizontal = g_random_double ();
      info->raw_info.antenna.offset.vertical = g_random_double ();
      info->raw_info.antenna.offset.horizontal = g_random_double ();
      info->raw_info.adc.vref = 1.0;
      info->raw_info.adc.offset = 0;

      info->acoustic_info.data.type = HYSCAN_DATA_ADC_16LE;
      info->acoustic_info.data.rate = 10000.0;
      info->acoustic_info.antenna.pattern.vertical = info->raw_info.antenna.pattern.vertical;
      info->acoustic_info.antenna.pattern.horizontal = info->raw_info.antenna.pattern.horizontal;

      info->tvg_rate = g_random_double ();

      hyscan_sonar_schema_source_add (schema, source,
                                      info->raw_info.antenna.pattern.vertical,
                                      info->raw_info.antenna.pattern.horizontal,
                                      0.0);

      hyscan_sonar_schema_generator_add (schema, source,
                                         HYSCAN_GENERATOR_MODE_AUTO,
                                         HYSCAN_GENERATOR_SIGNAL_AUTO,
                                         0.0, 0.0, 0.0, 0.0);

      hyscan_sonar_schema_tvg_add (schema, source, HYSCAN_TVG_MODE_AUTO, 0.0, 0.0);

      hyscan_sonar_schema_channel_add (schema, source, 1,
                                       info->raw_info.antenna.offset.vertical,
                                       info->raw_info.antenna.offset.horizontal,
                                       info->raw_info.adc.offset,
                                       info->raw_info.adc.vref);

      hyscan_sonar_schema_source_add_acuostic (schema, source);
    }

  /* Схема гидролокатора. */
  schema_data = hyscan_data_schema_builder_get_data (HYSCAN_DATA_SCHEMA_BUILDER (schema));
  g_object_unref (schema);

  /* Параметры гидролокатора - интерфейс HyScanSonar + HyScanDataBox. */
  sonar = hyscan_sonar_box_new (schema_data, "sonar");
  g_free (schema_data);

  /* Сервер управления. */
  server.sensor = hyscan_sensor_control_server_new (sonar);
  server.generator = hyscan_generator_control_server_new (sonar);
  server.tvg = hyscan_tvg_control_server_new (sonar);
  server.sonar = hyscan_sonar_control_server_new (sonar);
  server.ssse = hyscan_ssse_control_server_new (sonar);
  g_signal_connect_swapped (server.sonar, "sonar-ping", G_CALLBACK (sonar_ping_cb), &server);

  /* База данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  /* Управление ГБОЭ. */
  if (proxy_mode_string != NULL)
    {
      proxy = hyscan_ssse_proxy_new (HYSCAN_SONAR (sonar), proxy_mode, SIDE_SCALE, TRACK_SCALE, NULL);
      hyscan_sensor_proxy_set_source (HYSCAN_SENSOR_PROXY (proxy), select_port_by_index (0));
      control = hyscan_ssse_control_new (HYSCAN_SONAR (proxy), db);
    }
  else
    {
      proxy = NULL;
      control = hyscan_ssse_control_new (HYSCAN_SONAR (sonar), db);
    }

  /* Тестовый проект. */
  project_id = hyscan_db_project_create (db, PROJECT_NAME, NULL);
  if (project_id < 0)
    g_error ("can't create prject '%s'", PROJECT_NAME);

  /* Проверка управления гидролокатором. */
  g_message ("Generating data");
  generate_data (control, PROJECT_NAME, proxy_mode);

  /* Проверка записанных данных. */
  g_message ("Checking data");
  check_data (db, project_id, proxy_mode);

  /* Освобождаем память. */
  hyscan_db_close (db, project_id);
  hyscan_db_project_remove (db, PROJECT_NAME);

  g_message ("All done");

  g_object_unref (control);
  g_clear_object (&proxy);
  g_object_unref (server.sensor);
  g_object_unref (server.generator);
  g_object_unref (server.tvg);
  g_object_unref (server.sonar);
  g_object_unref (server.ssse);
  g_object_unref (sonar);
  g_object_unref (db);

  g_free (db_uri);

  xmlCleanupParser ();

  return 0;
}
