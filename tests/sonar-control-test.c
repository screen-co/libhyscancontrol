
#include "hyscan-sonar-schema.h"
#include "hyscan-ssse-control-server.h"
#include "hyscan-ssse-control.h"

#include <string.h>
#include <math.h>

#define FLOAT_EPSILON                          1e-5

#define N_TESTS                                10

#define SENSOR_N_PORTS                         4
#define SENSOR_N_UART_DEVICES                  SENSOR_N_PORTS
#define SENSOR_N_UART_MODES                    8
#define SENSOR_N_IP_ADDRESSES                  4

#define GENERATOR_N_PRESETS                    32

#define DATA_N_POINTS                          2048
#define SIGNAL_N_POINTS                        1024
#define TVG_N_GAINS                            512

typedef struct
{
  HyScanSensorPortType         type;
  HyScanSensorProtocolType     protocol;
  gboolean                     enable;
  gint64                       time_offset;
  HyScanAntennaPosition        position;
} VirtualPortInfo;

typedef struct
{
  HyScanSensorPortType         type;
  HyScanSensorProtocolType     protocol;
  gboolean                     enable;
  gint64                       time_offset;
  HyScanAntennaPosition        position;
  guint                        uart_device;
  guint                        uart_mode;
} UARTPortInfo;

typedef struct
{
  HyScanSensorPortType         type;
  HyScanSensorProtocolType     protocol;
  gboolean                     enable;
  gint64                       time_offset;
  HyScanAntennaPosition        position;
  guint                        ip_address;
  guint                        udp_port;
} UDPIPPortInfo;

typedef struct
{
  HyScanAntennaPosition        position;
  HyScanRawDataInfo            raw_info;

  gdouble                      max_receive_time;
  gdouble                      cur_receive_time;

  gdouble                      signal_rate;
  gdouble                      tvg_rate;

  struct
  {
    HyScanGeneratorModeType    capabilities;
    HyScanGeneratorSignalType  signals;
    guint                     *preset_ids;
    gchar                    **preset_names;
    gdouble                    min_tone_duration;
    gdouble                    max_tone_duration;
    gdouble                    min_lfm_duration;
    gdouble                    max_lfm_duration;

    gboolean                   enable;
    HyScanGeneratorModeType    cur_mode;
    HyScanGeneratorSignalType  cur_signal;
    guint                      cur_preset;
    gdouble                    cur_power;
    gdouble                    cur_duration;
  } generator;

  struct
  {
    HyScanTVGModeType          capabilities;
    gdouble                    min_gain;
    gdouble                    max_gain;

    gboolean                   enable;
    guint                      cur_mode;
    gdouble                    cur_level;
    gdouble                    cur_sensitivity;
    gdouble                    cur_gain;
    gdouble                    cur_gain0;
    gdouble                    cur_step;
    gdouble                    cur_alpha;
    gdouble                    cur_beta;
  } tvg;
} SourceInfo;

typedef struct
{
  gboolean             enable;
  gint64               sync_capabilities;
  gint64               sync_type;
  gboolean             enable_raw_data;
  gchar               *project_name;
  gchar               *track_name;
} SonarInfo;

gint64                 counter = 0;

guint                  uart_device_ids[SENSOR_N_UART_DEVICES];
gchar                 *uart_device_names[SENSOR_N_UART_DEVICES];
guint                  uart_mode_ids[SENSOR_N_UART_MODES];
gchar                 *uart_mode_names[SENSOR_N_UART_MODES];
guint                  ip_address_ids[SENSOR_N_IP_ADDRESSES];
gchar                 *ip_address_names[SENSOR_N_IP_ADDRESSES];

GHashTable            *ports;

SourceInfo             starboard;
SourceInfo             port;
SourceInfo             echosounder;

SonarInfo              sonar_info;

/* Функция возвращает информацию об источнике данных по его типу. */
SourceInfo *
select_source (HyScanSourceType type)
{
  if (type == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
    return &starboard;

  if (type == HYSCAN_SOURCE_SIDE_SCAN_PORT)
    return &port;

  if (type == HYSCAN_SOURCE_ECHOSOUNDER)
    return &echosounder;

  return NULL;
}

const gchar *
source_name (HyScanSourceType type)
{
  if (type == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
    return "starboard";

  if (type == HYSCAN_SOURCE_SIDE_SCAN_PORT)
    return "port";

  if (type == HYSCAN_SOURCE_ECHOSOUNDER)
    return "echosounder";

  return NULL;
}

/* Функция изменяет параметры UART порта. */
gboolean
sensor_uart_port_param_cb (HyScanSensorControlServer *server,
                           const gchar               *name,
                           HyScanSensorProtocolType   protocol,
                           guint                      uart_device,
                           guint                      uart_mode,
                           gpointer                   user_data)
{
  gint64 *counter = user_data;
  UARTPortInfo *port = g_hash_table_lookup (ports, name);

  if (port == NULL || port->type != HYSCAN_SENSOR_PORT_UART)
    return FALSE;

  port->protocol = protocol;
  port->uart_device = uart_device;
  port->uart_mode = uart_mode;

  *counter += 1;

  return TRUE;
}

gboolean
sensor_udp_ip_port_param_cb (HyScanSensorControlServer *server,
                             const gchar               *name,
                             HyScanSensorProtocolType   protocol,
                             guint                      ip_address,
                             guint                      udp_port,
                             gpointer                   user_data)
{
  gint64 *counter = user_data;
  UDPIPPortInfo *port = g_hash_table_lookup (ports, name);

  if (port == NULL || port->type != HYSCAN_SENSOR_PORT_UDP_IP)
    return FALSE;

  port->protocol = protocol;
  port->ip_address = ip_address;
  port->udp_port = udp_port;

  *counter += 1;

  return TRUE;
}

gboolean
sensor_set_enable_cb (HyScanSensorControlServer *server,
                      const gchar               *name,
                      gboolean                   enable,
                      gpointer                   user_data)
{
  gint64 *counter = user_data;
  VirtualPortInfo *port = g_hash_table_lookup (ports, name);

  if (port == NULL)
    return FALSE;

  port->enable = enable;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает режим работы генератора по преднастройкам. */
gboolean
generator_set_preset_cb (HyScanGeneratorControlServer *server,
                         HyScanSourceType              source,
                         guint                         preset,
                         gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_PRESET;
  info->generator.cur_preset = preset;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает автоматический режим работы генератора. */
gboolean
generator_set_auto_cb (HyScanGeneratorControlServer *server,
                       HyScanSourceType              source,
                       HyScanGeneratorSignalType     signal,
                       gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
  info->generator.cur_signal = signal;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает упрощённый режим работы генератора. */
gboolean
generator_set_simple_cb (HyScanGeneratorControlServer *server,
                         HyScanSourceType              source,
                         HyScanGeneratorSignalType     signal,
                         gdouble                       power,
                         gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_SIMPLE;
  info->generator.cur_signal = signal;
  info->generator.cur_power = power;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает расширенный режим работы генератора. */
gboolean
generator_set_extended_cb (HyScanGeneratorControlServer *server,
                           HyScanSourceType              source,
                           HyScanGeneratorSignalType     signal,
                           gdouble                       duration,
                           gdouble                       power,
                           gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_EXTENDED;
  info->generator.cur_signal = signal;
  info->generator.cur_duration = duration;
  info->generator.cur_power = power;

  *counter += 1;

  return TRUE;
}

/* Функция включает или отключает генератор. */
gboolean
generator_set_enable_cb (HyScanGeneratorControlServer *server,
                         HyScanSourceType              source,
                         gboolean                      enable,
                         gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->generator.enable = enable;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает автоматический режим управления ВАРУ. */
gboolean
tvg_set_auto_cb (HyScanTVGControlServer *server,
                 HyScanSourceType        source,
                 gdouble                 level,
                 gdouble                 sensitivity,
                 gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
  info->tvg.cur_level = level;
  info->tvg.cur_sensitivity = sensitivity;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает постоянный уровень усиления ВАРУ. */
gboolean
tvg_set_constant_cb (HyScanTVGControlServer *server,
                     HyScanSourceType        source,
                     gdouble                 gain,
                     gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_CONSTANT;
  info->tvg.cur_gain = gain;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает линейный закон усиления ВАРУ. */
gboolean
tvg_set_linear_db_cb (HyScanTVGControlServer *server,
                      HyScanSourceType        source,
                      gdouble                 gain0,
                      gdouble                 step,
                      gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_LINEAR_DB;
  info->tvg.cur_gain0 = gain0;
  info->tvg.cur_step = step;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает логарифмический закон усиления ВАРУ. */
gboolean
tvg_set_logarithmic_cb (HyScanTVGControlServer *server,
                        HyScanSourceType        source,
                        gdouble                 gain0,
                        gdouble                 beta,
                        gdouble                 alpha,
                        gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_LOGARITHMIC;
  info->tvg.cur_gain0 = gain0;
  info->tvg.cur_beta = beta;
  info->tvg.cur_alpha = alpha;

  *counter += 1;

  return TRUE;
}

/* Функция включает или отключает ВАРУ. */
gboolean
tvg_set_enable_cb (HyScanTVGControlServer *server,
                   HyScanSourceType        source,
                   gboolean                enable,
                   gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (source);

  if (info == NULL)
    return FALSE;

  info->tvg.enable = enable;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает тип синхронизации излучения. */
gboolean
sonar_set_sync_type_cb (HyScanSonarControlServer *server,
                        gint64                    sync_type,
                        gpointer                  user_data)
{
  gint64 *counter = user_data;

  sonar_info.sync_type = sync_type;
  *counter += 1;

  return TRUE;
}

/* Функция управляет отправкой "сырых" данных. */
gboolean
sonar_enable_raw_data_cb (HyScanSonarControlServer *server,
                          gboolean                  enable,
                          gpointer                  user_data)
{
  gint64 *counter = user_data;

  sonar_info.enable_raw_data = enable;
  *counter += 1;

  return TRUE;
}

/* Функция устанавливает время приёма эхосигнала бортом. */
gboolean
sonar_set_receive_time_cb (HyScanSonarControlServer *server,
                           gint                      board,
                           gdouble                   receive_time,
                           gpointer                  user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_source (board);

  if (info == NULL)
    return FALSE;

  info->cur_receive_time = receive_time;
  *counter += 1;

  return TRUE;
}

/* Функция включает гидролокатор в работу. */
gboolean
sonar_start_cb (HyScanSonarControlServer   *server,
                const gchar                *project_name,
                const gchar                *track_name,
                gpointer                    user_data)
{
  gint64 *counter = user_data;

  g_clear_pointer (&sonar_info.project_name, g_free);
  g_clear_pointer (&sonar_info.track_name, g_free);

  sonar_info.project_name = g_strdup (project_name);
  sonar_info.track_name = g_strdup (track_name);
  sonar_info.enable = TRUE;
  *counter += 1;

  return TRUE;
}

/* Функция останавливает работу гидролокатора. */
gboolean
sonar_stop_cb (HyScanSonarControlServer *server,
               gpointer                  user_data)
{
  gint64 *counter = user_data;

  g_clear_pointer (&sonar_info.project_name, g_free);
  g_clear_pointer (&sonar_info.track_name, g_free);

  sonar_info.enable = FALSE;
  *counter += 1;

  return TRUE;
}

/* Функция выполняет цикл зондирования. */
gboolean
sonar_ping_cb (HyScanSonarControlServer *server,
               gpointer                  user_data)
{
  gint64 *counter = user_data;

  static gint n_ping = 0;

  HyScanSourceType source;
  SourceInfo *info;

  HyScanComplexFloat *signal_points;
  gfloat *tvg_gains;
  guint16 *values;
  gint i, j, k;

  signal_points = g_new (HyScanComplexFloat, SIGNAL_N_POINTS);
  values = g_malloc (DATA_N_POINTS * sizeof (guint16));
  tvg_gains = g_new (gfloat, TVG_N_GAINS);

  /* Имитация гидролокационных данных. */
  for (i = 1; i <= 3; i++)
    {
      if (i == 1)
        source = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
      else if (i == 2)
        source = HYSCAN_SOURCE_SIDE_SCAN_PORT;
      else if (i == 3)
        source = HYSCAN_SOURCE_ECHOSOUNDER;
      else
        break;

      info = select_source (source);

      for (j = 1; j <= N_TESTS; j++)
        {
          HyScanDataWriterSignal signal;
          HyScanDataWriterTVG tvg;
          HyScanDataWriterData data;
          gint64 time = 1000 * j;

          /* Гидролокационные данные. */
          for (k = 0; k < DATA_N_POINTS; k++)
            values[k] = source + j + k;

          data.time = time;
          data.size = DATA_N_POINTS * sizeof (guint16);
          data.data = values;

          hyscan_sonar_control_server_send_raw_data (server, source, 1, HYSCAN_DATA_ADC_16LE,
                                                     info->signal_rate, &data);

          hyscan_sonar_control_server_send_noise_data (server, source, 1, HYSCAN_DATA_ADC_16LE,
                                                       info->signal_rate, &data);

          hyscan_ssse_control_server_send_acoustic_data (HYSCAN_SSSE_CONTROL_SERVER (server),
                                                         source, HYSCAN_DATA_ADC_16LE,
                                                         info->signal_rate, &data);

          if (j == n_ping)
            {
              /* Образ сигнала. */
              for (k = 0; k < SIGNAL_N_POINTS; k++)
                {
                  signal_points[k].re = (info->generator.cur_power * k + i + n_ping) / SIGNAL_N_POINTS;
                  signal_points[k].im = -signal_points[k].re;
                }

              signal.time = time;
              signal.rate = info->signal_rate;
              signal.n_points = SIGNAL_N_POINTS;
              signal.points = signal_points;
              hyscan_generator_control_server_send_signal (HYSCAN_GENERATOR_CONTROL_SERVER (server), source, &signal);

              /* Параметры ВАРУ. */
              for (k = 0; k < TVG_N_GAINS; k++)
                  tvg_gains[k] = (((k % 2) ? 1.0 : -1.0) * info->tvg.cur_level * k + i + n_ping) / TVG_N_GAINS;

              tvg.time = time;
              tvg.rate = info->tvg_rate;
              tvg.n_gains = TVG_N_GAINS;
              tvg.gains = tvg_gains;
              hyscan_tvg_control_server_send_tvg (HYSCAN_TVG_CONTROL_SERVER (server), source, &tvg);
            }
        }
    }

  /* Имитация данных датчиков. */
  for (i = 1; i <= N_TESTS; i++)
    {
      gchar *nmea;
      gchar *nmea_rmc;
      gchar *nmea_gga;
      gchar *nmea_dpt;
      guchar nmea_crc;
      gsize nmea_len;

      HyScanDataWriterData data;
      gint64 time = 1000 * i;

      nmea_gga = g_strdup_printf ("$GPGGA,DUMMY DATA %d,*00", i + n_ping);
      nmea_rmc = g_strdup_printf ("$GPRMC,DUMMY DATA %d,*00", i + n_ping);
      nmea_dpt = g_strdup_printf ("$GPDPT,DUMMY DATA %d,*00", i + n_ping);

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

      data.time = time;
      data.data = nmea;
      data.size = strlen (nmea);
      hyscan_sensor_control_server_send_data (HYSCAN_SENSOR_CONTROL_SERVER (server),
                                              "virtual.1", HYSCAN_DATA_STRING, &data);

      data.time = time;
      data.data = nmea;
      data.size = strlen (nmea);
      hyscan_sensor_control_server_send_data (HYSCAN_SENSOR_CONTROL_SERVER (server),
                                              "uart.1", HYSCAN_DATA_STRING, &data);

      data.time = time;
      data.data = nmea;
      data.size = strlen (nmea);
      hyscan_sensor_control_server_send_data (HYSCAN_SENSOR_CONTROL_SERVER (server),
                                              "udp.1", HYSCAN_DATA_STRING, &data);

      g_free (nmea);
      g_free (nmea_gga);
      g_free (nmea_rmc);
      g_free (nmea_dpt);
    }

  g_free (signal_points);
  g_free (tvg_gains);
  g_free (values);

  *counter += 1;
  n_ping += 1;

  return TRUE;
}

/* Функция проверяет управления портами для подключения датчиков. */
void
check_sensor_control (HyScanSensorControl *control)
{
  HyScanDataSchemaEnumValue **uart_devices;
  HyScanDataSchemaEnumValue **uart_modes;
  HyScanDataSchemaEnumValue **ip_addresses;
  gchar **names;

  gint64 prev_counter;
  guint i, j;

  /* Список портов. */
  names = hyscan_sensor_control_list_ports (control);
  if (names == NULL)
    g_error ("can't list sensor ports");

  /* Проверка числа портов. */
  if (g_strv_length (names) != 3 * SENSOR_N_PORTS)
    g_error ("wrong number of sensor ports");

  /* Список UART устройств. */
  uart_devices = hyscan_sensor_control_list_uart_devices (control);
  if (uart_devices == NULL)
    g_error ("sensor.uart-devices: can't get");

  for (i = 0; i < SENSOR_N_UART_DEVICES; i++)
    {
      gboolean has_uart_device = FALSE;

      for (j = 0; uart_devices[j] != NULL; j++)
        if ((uart_devices[j]->value == uart_device_ids[i]) &&
            (g_strcmp0 (uart_devices[j]->name, uart_device_names[i]) == 0))
          {
            has_uart_device = TRUE;
          }

      if (!has_uart_device)
        g_error ("sensor.uart-devices: can't find device %s", uart_device_names[i]);
    }

  hyscan_data_schema_free_enum_values (uart_devices);

  /* Список UART режимов. */
  uart_modes = hyscan_sensor_control_list_uart_modes (control);
  if (uart_modes == NULL)
    g_error ("sensor.uart-modes: can't get");

  for (i = 0; i < SENSOR_N_UART_MODES; i++)
    {
      gboolean has_uart_mode = FALSE;

      for (j = 0; uart_modes[j] != NULL; j++)
        if ((uart_modes[j]->value == uart_mode_ids[i]) &&
            (g_strcmp0 (uart_modes[j]->name, uart_mode_names[i]) == 0))
          {
            has_uart_mode = TRUE;
          }

      if (!has_uart_mode)
        g_error ("sensor.uart-modes: can't find mode %s", uart_mode_names[i]);
    }

  hyscan_data_schema_free_enum_values (uart_modes);

  /* Список IP адресов. */
  ip_addresses = hyscan_sensor_control_list_ip_addresses (control);
  if (ip_addresses == NULL)
    g_error ("sensor.ip-addresses: can't get");

  for (i = 0; i < SENSOR_N_IP_ADDRESSES; i++)
    {
      gboolean has_ip_address = FALSE;

      for (j = 0; ip_addresses[j] != NULL; j++)
        if ((ip_addresses[j]->value == ip_address_ids[i]) &&
            (g_strcmp0 (ip_addresses[j]->name, ip_address_names[i]) == 0))
          {
            has_ip_address = TRUE;
          }

      if (!has_ip_address)
        g_error ("sensor.ip-addresses: can't find ip address %s", ip_address_names[i]);
    }

  hyscan_data_schema_free_enum_values (ip_addresses);

  for (i = 0; names[i] != NULL; i++)
    {
      VirtualPortInfo *port;
      UARTPortInfo *uart_port;
      UDPIPPortInfo *udp_port;

      /* Проверяем наличие порта. */
      port = g_hash_table_lookup (ports, names[i]);
      if (port == NULL)
        g_error ("can't find sensor port %s", names[i]);

      /* Позиция приёмной антенны. */
      hyscan_sensor_control_set_position (control, names[i],
                                          port->position.x, port->position.y, port->position.z,
                                          port->position.psi, port->position.gamma, port->position.theta);

      /* Выключаем порт. */
      prev_counter = counter;
      if (!hyscan_sensor_control_set_enable (control, names[i], FALSE) ||
          (port->enable != FALSE) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.sensor.disable: can't disable", names[i]);
        }

      /* Настраиваем порт типа HYSCAN_SENSOR_PORT_VIRTUAL для теста данных. */
      if (port->type == HYSCAN_SENSOR_PORT_VIRTUAL)
        {
          hyscan_sensor_control_set_virtual_port_param (control, names[i], 1, port->time_offset);
        }

      /* Проверка порта типа HYSCAN_SENSOR_PORT_UART. */
      uart_port = g_hash_table_lookup (ports, names[i]);
      if (uart_port->type == HYSCAN_SENSOR_PORT_UART)
        {
          guint j, k, n;
          for (j = 0; j < SENSOR_N_UART_DEVICES; j++)
            for (k = 0; k < SENSOR_N_UART_MODES; k++)
              for (n = 1; n < SENSOR_N_PORTS; n++)
                {
                  HyScanSensorProtocolType protocol;
                  gint64 uart_device = uart_device_ids[j];
                  gint64 uart_mode = uart_mode_ids[k];
                  gboolean status;

                  protocol = (n % 2) ? HYSCAN_SENSOR_PROTOCOL_SAS : HYSCAN_SENSOR_PROTOCOL_NMEA_0183;

                  prev_counter = counter;
                  status = hyscan_sensor_control_set_uart_port_param (control, names[i], n, 0,
                                                                      protocol, uart_device, uart_mode);

                  if (!status || (prev_counter + 1 != counter) ||
                      (uart_port->protocol != protocol) ||
                      (uart_port->uart_device != uart_device) ||
                      (uart_port->uart_mode != uart_mode))
                    {
                      g_error ("%s.sensor.uart: can't set param", names[i]);
                    }
                }

          /* Настраиваем порт для теста данных. */
          hyscan_sensor_control_set_uart_port_param (control, names[i], 2, uart_port->time_offset,
                                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                              uart_device_ids[0],
                                                              uart_mode_ids[0]);
        }

      /* Проверка порта типа HYSCAN_SENSOR_PORT_UDP_IP. */
      udp_port = g_hash_table_lookup (ports, names[i]);
      if (udp_port->type == HYSCAN_SENSOR_PORT_UDP_IP)
        {
          guint j, k;
          for (j = 0; j < SENSOR_N_IP_ADDRESSES; j++)
            for (k = 1; k < SENSOR_N_PORTS; k++)
                {
                  HyScanSensorProtocolType protocol;
                  gint64 ip_address = ip_address_ids[j];
                  guint16 udp_port_n = g_random_int_range (1024, 65535);
                  gboolean status;

                  protocol = (k % 2) ? HYSCAN_SENSOR_PROTOCOL_SAS : HYSCAN_SENSOR_PROTOCOL_NMEA_0183;

                  prev_counter = counter;
                  status = hyscan_sensor_control_set_udp_ip_port_param (control, names[i], k, 0,
                                                                                 protocol,
                                                                                 ip_address,
                                                                                 udp_port_n);

                  if (!status || (prev_counter + 1 != counter) ||
                      (udp_port->protocol != protocol) ||
                      (udp_port->ip_address != ip_address) ||
                      (udp_port->udp_port != udp_port_n))
                    {
                      g_error ("%s.sensor.udpip: can't set param", names[i]);
                    }
                }

          /* Настраиваем порт для теста данных. */
          hyscan_sensor_control_set_udp_ip_port_param (control, names[i], 3, udp_port->time_offset,
                                                                HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                                ip_address_ids[0], 10000);
        }

      /* Включаем порт. */
      prev_counter = counter;
      if (!hyscan_sensor_control_set_enable (control, names[i], TRUE) ||
          (port->enable != TRUE) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.sensor.enable: can't enable", names[i]);
        }
    }

  g_strfreev (names);
}

/* Функция проверяет управление генератором. */
void
check_generator_control (HyScanGeneratorControl *control,
                         HyScanSourceType        source)
{
  SourceInfo *info = select_source (source);
  const gchar *name = source_name (source);

  HyScanGeneratorModeType capabilities;
  HyScanGeneratorSignalType signals;
  HyScanDataSchemaEnumValue **presets;

  gdouble min_duration;
  gdouble max_duration;

  gint64 prev_counter;
  guint i, j;

  if (info == NULL)
    g_error ("unknown board type %d", source);

  /* Возможности генератора. */
  capabilities = hyscan_generator_control_get_capabilities (control, source);
  if (capabilities != info->generator.capabilities)
    g_error ("%s.generator.capabilities: mismatch", name);

  /* Допустимые сигналы. */
  signals = hyscan_generator_control_get_signals (control, source);
  if (signals != info->generator.signals)
    g_error ("%s.generator.signals: mismatch", name);

  /* Диапазон длительностей тонального сигнала. */
  if (!hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_TONE, &min_duration, &max_duration))
    g_error ("%s.generator.duration_range (tone): can't get", name);

  if (min_duration != info->generator.min_tone_duration || max_duration != info->generator.max_tone_duration)
    g_error ("%s.generator.duration_range (tone): mismatch", name);

  /* Диапазон длительностей ЛЧМ сигнала. */
  if (!hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_LFM, &min_duration, &max_duration))
    g_error ("%s.generator.duration_range (lfm): can't get", name);

  if (min_duration != info->generator.min_lfm_duration || max_duration != info->generator.max_lfm_duration)
    g_error ("%s.generator.duration_range (lfm): mismatch", name);

  if (!hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_LFMD, &min_duration, &max_duration))
    g_error ("%s.generator.duration_range (lfmd): can't get", name);

  if (min_duration != info->generator.min_lfm_duration || max_duration != info->generator.max_lfm_duration)
    g_error ("%s.generator.duration_range (lfmd): mismatch", name);

  /* Преднастройки генератора. */
  presets = hyscan_generator_control_list_presets (control, source);
  if (presets == NULL)
    g_error ("%s.generator.presets: can't get", name);

  for (i = 0; i < GENERATOR_N_PRESETS; i++)
    {
      gboolean has_preset = FALSE;

      for (j = 0; presets[j] != NULL; j++)
        if ((presets[j]->value == info->generator.preset_ids[i]) &&
            (g_strcmp0 (presets[j]->name, info->generator.preset_names[i]) == 0))
          {
            has_preset = TRUE;
          }

      if (!has_preset)
        g_error ("%s.generator.presets: can't find preset %s", name, info->generator.preset_names[i]);
    }

  for (i = 0; presets[i] != NULL; i++)
    {
      prev_counter = counter;
      if (!hyscan_generator_control_set_preset (control, source, presets[i]->value) ||
          (info->generator.cur_mode != HYSCAN_GENERATOR_MODE_PRESET) ||
          (info->generator.cur_preset != presets[i]->value) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.generator.presets: can't set preset %s", name, presets[i]->name);
        }
    }

  hyscan_data_schema_free_enum_values (presets);

  /* Автоматический режим. */
  for (i = 0; signals & (1 << i); i++)
    {
      HyScanGeneratorSignalType signal = 1 << i;

      prev_counter = counter;
      if (!hyscan_generator_control_set_auto (control, source, signal) ||
          (info->generator.cur_mode != HYSCAN_GENERATOR_MODE_AUTO) ||
          (info->generator.cur_signal != signal) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.generator.auto: can't set mode", name);
        }
    }

  /* Простой режим. */
  for (i = 0; signals & (1 << i); i++)
    {
      HyScanGeneratorSignalType signal = 1 << i;
      gdouble power = g_random_double_range (0.0, 100.0);

      prev_counter = counter;
      if (!hyscan_generator_control_set_simple (control, source, signal, power) ||
          (info->generator.cur_mode != HYSCAN_GENERATOR_MODE_SIMPLE) ||
          (info->generator.cur_signal != signal) ||
          (info->generator.cur_power != power) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.generator.simple: can't set mode", name);
        }
    }

  /* Расширенный режим. */
  for (i = 1; signals & (1 << i); i++)
    {
      HyScanGeneratorSignalType signal = 1 << i;
      gdouble power = g_random_double_range (0.0, 100.0);
      gdouble duration;

      if (signal == HYSCAN_GENERATOR_SIGNAL_TONE)
        hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_TONE, &min_duration, &max_duration);

      if (signal == HYSCAN_GENERATOR_SIGNAL_LFM || signal == HYSCAN_GENERATOR_SIGNAL_LFMD)
        hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_LFM, &min_duration, &max_duration);

      duration = g_random_double_range (min_duration, max_duration);

      prev_counter = counter;
      if (!hyscan_generator_control_set_extended (control, source, signal, duration, power) ||
          (info->generator.cur_mode != HYSCAN_GENERATOR_MODE_EXTENDED) ||
          (info->generator.cur_signal != signal) ||
          (info->generator.cur_duration != duration) ||
          (info->generator.cur_power != power) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.generator.extended: can't set mode", name);
        }
    }

  /* Включение / выключение. */
  prev_counter = counter;
  if (!hyscan_generator_control_set_enable (control, source, TRUE) ||
      (info->generator.enable != TRUE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("%s.generator.enable: can't enable", name);
    }
  prev_counter = counter;
  if (!hyscan_generator_control_set_enable (control, source, FALSE) ||
      (info->generator.enable != FALSE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("%s.generator.enable: can't disable", name);
    }
}

/* Функция проверяет управление системой ВАРУ. */
void
check_tvg_control (HyScanTVGControl *control,
                   HyScanSourceType  source)
{
  SourceInfo *info = select_source (source);
  const gchar *name = source_name (source);

  HyScanTVGModeType capabilities;

  gdouble min_gain;
  gdouble max_gain;

  gint64 prev_counter;
  guint i;

  if (info == NULL)
    g_error ("unknown board type %d", source);

  /* Возможности ВАРУ. */
  capabilities = hyscan_tvg_control_get_capabilities (control, source);
  if (capabilities != info->tvg.capabilities)
    g_error ("%s.tvg.capabilities: mismatch", name);

  /* Диапазон значений коэффициентов усилений ВАРУ. */
  if (!hyscan_tvg_control_get_gain_range (control, source, &min_gain, &max_gain))
    g_error ("%s.tvg.gain_range: can't get", name);

  if (min_gain != info->tvg.min_gain || max_gain != info->tvg.max_gain)
    g_error ("%s.tvg.gain_range: mismatch", name);

  /* Автоматический режим. */
  for (i = 0; i < N_TESTS; i++)
    {
      gdouble level = g_random_double_range (0.0, 1.0);
      gdouble sensitivity = g_random_double_range (0.0, 1.0);

      prev_counter = counter;
      if (!hyscan_tvg_control_set_auto (control, source, level, sensitivity) ||
          (info->tvg.cur_mode != HYSCAN_TVG_MODE_AUTO) ||
          (info->tvg.cur_level != level) ||
          (info->tvg.cur_sensitivity != sensitivity) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.tvg.auto: can't set mode", name);
        }
    }

  /* Постоянное усиление. */
  for (i = 0; i < N_TESTS; i++)
    {
      gdouble gain = g_random_double_range (min_gain, max_gain);

      prev_counter = counter;
      if (!hyscan_tvg_control_set_constant (control, source, gain) ||
          (info->tvg.cur_mode != HYSCAN_TVG_MODE_CONSTANT) ||
          (info->tvg.cur_gain != gain) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.tvg.constant: can't set mode", name);
        }
    }

  /* Линейное усиление. */
  for (i = 0; i < N_TESTS; i++)
    {
      gdouble gain0 = g_random_double_range (min_gain, max_gain);
      gdouble step = g_random_double_range (0.0, 100.0);

      prev_counter = counter;
      if (!hyscan_tvg_control_set_linear_db (control, source, gain0, step) ||
          (info->tvg.cur_mode != HYSCAN_TVG_MODE_LINEAR_DB) ||
          (info->tvg.cur_gain0 != gain0) ||
          (info->tvg.cur_step != step) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.tvg.linear_db: can't set mode", name);
        }
    }

  /* Логарифимическое усиление. */
  for (i = 0; i < N_TESTS; i++)
    {
      gdouble gain0 = g_random_double_range (min_gain, max_gain);
      gdouble beta = g_random_double_range (0.0, 100.0);
      gdouble alpha = g_random_double_range (0.0, 1.0);

      prev_counter = counter;
      if (!hyscan_tvg_control_set_logarithmic (control, source, gain0, beta, alpha) ||
          (info->tvg.cur_mode != HYSCAN_TVG_MODE_LOGARITHMIC) ||
          (info->tvg.cur_gain0 != gain0) ||
          (info->tvg.cur_beta != beta) ||
          (info->tvg.cur_alpha != alpha) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.tvg.logarithmic: can't set mode", name);
        }
    }

  /* Включение / выключение. */
  prev_counter = counter;
  if (!hyscan_tvg_control_set_enable (control, source, TRUE) ||
      (info->tvg.enable != TRUE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("%s.tvg.enable: can't enable", name);
    }
  prev_counter = counter;
  if (!hyscan_tvg_control_set_enable (control, source, FALSE) ||
      (info->tvg.enable != FALSE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("%s.tvg.enable: can't disable", name);
    }
}

/* Функция проверяет управление гидролокатором. */
void
check_sonar_control (HyScanSonarControl *control,
                     const gchar        *project_name)
{
  HyScanSonarSyncType capabilities;

  gint64 prev_counter;
  guint i;

  /* Типы синхронизации излучения. */
  capabilities = hyscan_sonar_control_get_sync_capabilities (control);
  if (capabilities != sonar_info.sync_capabilities)
    g_error ("sonar.sync.capabilities: mismatch");

  for (i = 0; capabilities & (1 << i); i++)
    {
      prev_counter = counter;
      if (!hyscan_sonar_control_set_sync_type (control, (1 << i)) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar.sync.capabilities: can't set");
        }
    }

  /* Включение / выключение "сырых" данных. */
  prev_counter = counter;
  if (!hyscan_sonar_control_enable_raw_data (control, FALSE) ||
      (sonar_info.enable_raw_data != FALSE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("sonar.raw_data.enable: can't disable");
    }
  prev_counter = counter;
  if (!hyscan_sonar_control_enable_raw_data (control, TRUE) ||
      (sonar_info.enable_raw_data != TRUE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("sonar.raw_data.enable: can't enable");
    }

  /* Время приёма эхосигналов. */
  for (i = 0; i < N_TESTS; i++)
    {
      gdouble receive_time;

      receive_time = g_random_double_range (0.001, starboard.max_receive_time);
      prev_counter = counter;
      if (!hyscan_sonar_control_set_receive_time (control, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, receive_time) ||
          (starboard.cur_receive_time != receive_time) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar.starboard.receive_time: can't set");
        }

      receive_time = g_random_double_range (0.001, port.max_receive_time);
      prev_counter = counter;
      if (!hyscan_sonar_control_set_receive_time (control, HYSCAN_SOURCE_SIDE_SCAN_PORT, receive_time) ||
          (port.cur_receive_time != receive_time) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar.port.receive_time: can't set");
        }

      receive_time = g_random_double_range (0.001, echosounder.max_receive_time);
      prev_counter = counter;
      if (!hyscan_sonar_control_set_receive_time (control, HYSCAN_SOURCE_ECHOSOUNDER, receive_time) ||
          (echosounder.cur_receive_time != receive_time) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar.echosounder.receive_time: can't set");
        }
    }

  /* Местоположение приёмных антенн. */
  hyscan_sonar_control_set_position (control, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                     starboard.position.x, starboard.position.y, starboard.position.z,
                                     starboard.position.psi, starboard.position.gamma, starboard.position.theta);

  hyscan_sonar_control_set_position (control, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                     port.position.x, port.position.y, port.position.z,
                                     port.position.psi, port.position.gamma, port.position.theta);

  hyscan_sonar_control_set_position (control, HYSCAN_SOURCE_ECHOSOUNDER,
                                     echosounder.position.x, echosounder.position.y, echosounder.position.z,
                                     echosounder.position.psi, echosounder.position.gamma, echosounder.position.theta);

  /* Включаем гидролокатор в работу. */
  for (i = 0; i < N_TESTS; i++)
    {
      gchar *track_name = g_strdup_printf ("test-track-%d", i);

      prev_counter = counter;
      if (!hyscan_sonar_control_start (control, project_name, track_name, HYSCAN_TRACK_SURVEY) ||
          (g_strcmp0 (sonar_info.project_name, project_name) != 0) ||
          (g_strcmp0 (sonar_info.track_name, track_name) != 0) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar.start: can't start");
        }

      hyscan_sonar_control_ping (control);

      g_free (track_name);
    }

  /* Выключаем гидролокатор. */
  prev_counter = counter;
  if (!hyscan_sonar_control_stop (control) ||
      (sonar_info.project_name != NULL) ||
      (sonar_info.track_name != NULL) ||
      (prev_counter + 1 != counter))
    {
      g_error ("sonar.start: can't stop");
    }
}

/* Функция проверяет записанные данные. */
void
check_data (HyScanDB *db,
            gint32    project_id)
{
  gint32 track_id;

  HyScanComplexFloat *signal_points;
  gfloat *tvg_gains;
  guint16 *values;

  gpointer buffer;
  gint32 buffer_size;
  gint32 data_size;
  gint64 time;

  gint i, j, k, n, m;

  buffer_size = DATA_N_POINTS * sizeof(HyScanComplexFloat);
  buffer = g_malloc (buffer_size);

  for (i = 0; i < N_TESTS; i++)
    {
      HyScanSourceType source;
      SourceInfo *info;

      gchar *track_name = g_strdup_printf ("test-track-%d", i);

      track_id = hyscan_db_track_open (db, project_id, track_name);
      if (track_id < 0)
        g_error ("can't open %s", track_name);

      /* Проверяем данные от датчиков. */
      for (j = 1; j <= 3; j++)
        {
          VirtualPortInfo *port;

          if (j == 1)
            port = g_hash_table_lookup (ports, "virtual.1");
          else if (j == 2)
            port = g_hash_table_lookup (ports, "uart.1");
          else if (j == 2)
            port = g_hash_table_lookup (ports, "udp.1");
          else
            break;

          for (k = 1; k <= 4; k++)
            {
              HyScanSourceType source;
              const gchar *channel_name;
              gint32 channel_id;
              gint32 param_id;

              gchar *nmea;
              gchar *nmea_rmc;
              gchar *nmea_gga;
              gchar *nmea_dpt;
              guchar nmea_crc;
              gsize nmea_len;

              gdouble double_value;

              if (k == 1)
                source = HYSCAN_SOURCE_NMEA_ANY;
              else if (k == 2)
                source = HYSCAN_SOURCE_NMEA_GGA;
              else if (k == 3)
                source = HYSCAN_SOURCE_NMEA_RMC;
              else if (k == 4)
                source = HYSCAN_SOURCE_NMEA_DPT;
              else
                break;

              channel_name = hyscan_channel_get_name_by_types (source, TRUE, j);

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
              for (n = 1; n <= N_TESTS; n++)
                {
                  const gchar *cur_nmea = NULL;

                  nmea_gga = g_strdup_printf ("$GPGGA,DUMMY DATA %d,*00", i + n);
                  nmea_rmc = g_strdup_printf ("$GPRMC,DUMMY DATA %d,*00", i + n);
                  nmea_dpt = g_strdup_printf ("$GPDPT,DUMMY DATA %d,*00", i + n);

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
                  if (!hyscan_db_channel_get_data (db, channel_id, n - 1, buffer, &data_size, &time) ||
                      (time - port->time_offset != n * 1000) ||
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
      for (j = 1; j <= 3; j++)
        {
          gint32 channel_id;
          gint32 param_id;

          gdouble double_value;
          gint64 integer_value;
          gchar *string_value;

          if (j == 1)
            source = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
          else if (j == 2)
            source = HYSCAN_SOURCE_SIDE_SCAN_PORT;
          else if (j == 3)
            source = HYSCAN_SOURCE_ECHOSOUNDER;
          else
            break;

          info = select_source (source);

          /* Гидролокационные данных. */
          for (k = 0; k < 3; k++)
            {
              gchar *channel_name;
              gboolean raw = (k > 0) ? TRUE : FALSE;

              if (k < 2)
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
              if (g_strcmp0 (string_value, hyscan_data_get_type_name (HYSCAN_DATA_ADC_16LE)) != 0)
                g_error ("%s.%s: '/data/type' error", track_name, channel_name);
              g_free (string_value);

              if (!hyscan_db_param_get_double (db, param_id, NULL, "/data/rate", &double_value) ||
                  fabs (double_value - info->signal_rate) > FLOAT_EPSILON)
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
              for (n = 1; n <= N_TESTS; n++)
                {
                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, n - 1, buffer, &data_size, &time) ||
                      data_size != DATA_N_POINTS * sizeof (guint16) ||
                      time != 1000 * n)
                    {
                      g_error ("%s.%s: can't get data", track_name, channel_name);
                    }

                  for (m = 0; m < DATA_N_POINTS; m++)
                    if (values[m] != source + n + m)
                      g_error ("%s.%s: data error", track_name, channel_name);
                }

              hyscan_db_close (db, param_id);
              hyscan_db_close (db, channel_id);
              g_free (channel_name);
            }

          /* Образы сигналов и ВАРУ. */
          if (i > 0)
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
                  fabs (double_value - info->signal_rate) > FLOAT_EPSILON)
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
              for (n = 0; n < ((i == 1) ? 1 : 2); n++)
                {
                  gint x = (i == 1) ? 1 : 0;

                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, n, buffer, &data_size, &time) ||
                      data_size != SIGNAL_N_POINTS * sizeof (HyScanComplexFloat) ||
                      time != 1000 * (i + n + x - 1))
                    {
                      g_error ("%s.%s: can't get data", track_name, signal_name);
                    }

                  for (m = 0; m < SIGNAL_N_POINTS; m++)
                    {
                      gdouble signal_value = (info->generator.cur_power * m + j + i + n + x - 1) / SIGNAL_N_POINTS;

                      if (fabs (signal_points[m].re - signal_value) > FLOAT_EPSILON ||
                          fabs (signal_points[m].im + signal_value) > FLOAT_EPSILON)
                        {
                          g_error ("%s.%s: data error", track_name, signal_name);
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
              for (n = 0; n < ((i == 1) ? 1 : 2); n++)
                {
                  gint x = (i == 1) ? 1 : 0;

                  data_size = buffer_size;
                  if (!hyscan_db_channel_get_data (db, channel_id, n, buffer, &data_size, &time) ||
                      data_size != TVG_N_GAINS * sizeof (gfloat) ||
                      time != 1000 * (i + n + x - 1))
                    {
                      g_error ("%s.%s: can't get data", track_name, tvg_name);
                    }

                  for (m = 0; m < TVG_N_GAINS; m++)
                    {
                      gdouble tvg_gain = (((m % 2) ? 1.0 : -1.0) * info->tvg.cur_level * m + j + i + n + x - 1) / TVG_N_GAINS;

                      if (fabs (tvg_gains[m] - tvg_gain) > FLOAT_EPSILON)
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
  gboolean print_schema = FALSE;

  HyScanSonarSchema *schema;
  gchar *schema_data;

  HyScanSonarBox *sonar;
  HyScanSSSEControlServer *server;
  HyScanSSSEControl *control;

  gchar *db_uri = NULL;
  HyScanDB *db;
  gchar *project_name = "project";
  gint32 project_id;

  guint pow2;
  guint i, j;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "project-name", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { "print-schema", 's', 0, G_OPTION_ARG_NONE, &print_schema, "Print sonar schema and exit", NULL },
        { NULL } };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message( error->message);
        return -1;
      }

    if ((g_strv_length (args) != 2) && !print_schema)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  /* Параметры источников данных. */
  for (i = 0; i < 3; i++)
    {
      SourceInfo *info;

      if (i == 0)
        info = &starboard;
      else if (i == 1)
        info = &port;
      else if (i == 2)
        info = &echosounder;
      else
        break;

      memset (info, 0, sizeof (SourceInfo));

      info->position.x = g_random_double ();
      info->position.y = g_random_double ();
      info->position.z = g_random_double ();
      info->position.psi = g_random_double ();
      info->position.gamma = g_random_double ();
      info->position.theta = g_random_double ();

      info->raw_info.antenna.pattern.vertical = g_random_double ();
      info->raw_info.antenna.pattern.horizontal = g_random_double ();
      info->raw_info.antenna.offset.vertical = g_random_double ();
      info->raw_info.antenna.offset.horizontal = g_random_double ();
      info->raw_info.adc.vref = g_random_double ();
      info->raw_info.adc.offset = 100 * g_random_double ();

      info->max_receive_time = g_random_double () + 0.1;

      info->signal_rate = g_random_double ();
      info->tvg_rate = g_random_double ();

      info->generator.capabilities = HYSCAN_GENERATOR_MODE_PRESET | HYSCAN_GENERATOR_MODE_AUTO |
                                     HYSCAN_GENERATOR_MODE_SIMPLE | HYSCAN_GENERATOR_MODE_EXTENDED;
      info->generator.signals = HYSCAN_GENERATOR_SIGNAL_AUTO | HYSCAN_GENERATOR_SIGNAL_TONE |
                                HYSCAN_GENERATOR_SIGNAL_LFM | HYSCAN_GENERATOR_SIGNAL_LFMD;

      info->generator.min_tone_duration = g_random_double ();
      info->generator.max_tone_duration = info->generator.min_tone_duration + g_random_double ();
      info->generator.min_lfm_duration = g_random_double ();
      info->generator.max_lfm_duration = info->generator.min_lfm_duration + g_random_double ();

      info->generator.enable = FALSE;
      info->generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
      info->generator.cur_signal = HYSCAN_GENERATOR_SIGNAL_AUTO;

      info->tvg.capabilities = HYSCAN_TVG_MODE_AUTO | HYSCAN_TVG_MODE_CONSTANT |
                               HYSCAN_TVG_MODE_LINEAR_DB | HYSCAN_TVG_MODE_LOGARITHMIC;

      info->tvg.min_gain = g_random_double ();
      info->tvg.max_gain = info->tvg.min_gain + g_random_double ();

      info->tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
    }

  /* Параметры гидролокатора по умолчанию. */
  memset (&sonar_info, 0, sizeof (SonarInfo));
  sonar_info.sync_capabilities = HYSCAN_SONAR_SYNC_INTERNAL |
                                 HYSCAN_SONAR_SYNC_EXTERNAL |
                                 HYSCAN_SONAR_SYNC_SOFTWARE;

  /* Схема гидролокатора. */
  schema = hyscan_sonar_schema_new (HYSCAN_SONAR_SCHEMA_DEFAULT_TIMEOUT);

  /* UART устройства. */
  for (i = 0; i < SENSOR_N_UART_DEVICES; i++)
    {
      uart_device_names[i] = g_strdup_printf ("ttyS%d", i);
      uart_device_ids[i] =
        hyscan_sonar_schema_sensor_add_uart_device (schema, uart_device_names[i]);
    }

  /* Режимы UART устройств. */
  for (i = 0, pow2 = 1; i < SENSOR_N_UART_MODES; i += 1, pow2 *= 2)
    {
      uart_mode_names[i] = g_strdup_printf ("%d baud", 150 * pow2);
      uart_mode_ids[i] =
        hyscan_sonar_schema_sensor_add_uart_mode (schema, uart_mode_names[i]);
    }

  /* IP адреса. */
  for (i = 0; i < SENSOR_N_IP_ADDRESSES; i++)
    {
      ip_address_names[i] = g_strdup_printf ("10.10.10.%d", i + 1);
      ip_address_ids[i] =
        hyscan_sonar_schema_sensor_add_ip_address (schema, ip_address_names[i]);
    }

  /* Порты для датчиков. */
  ports = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  for (i = 0; i < SENSOR_N_PORTS; i++)
    {
      VirtualPortInfo *port = g_new0 (VirtualPortInfo, 1);
      gchar *name = g_strdup_printf ("virtual.%d", i + 1);

      port->type = HYSCAN_SENSOR_PORT_VIRTUAL;
      port->position.x = g_random_double ();
      port->position.y = g_random_double ();
      port->position.z = g_random_double ();
      port->position.psi = g_random_double ();
      port->position.gamma = g_random_double ();
      port->position.theta = g_random_double ();
      port->time_offset = g_random_int_range (1, 1000);
      g_hash_table_insert (ports, name, port);

      hyscan_sonar_schema_sensor_add (schema, name,
                                              HYSCAN_SENSOR_PORT_VIRTUAL,
                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
    }

  for (i = 0; i < SENSOR_N_PORTS; i++)
    {
      UARTPortInfo *port = g_new0 (UARTPortInfo, 1);
      gchar *name = g_strdup_printf ("uart.%d", i + 1);

      port->type = HYSCAN_SENSOR_PORT_UART;
      port->position.x = g_random_double ();
      port->position.y = g_random_double ();
      port->position.z = g_random_double ();
      port->position.psi = g_random_double ();
      port->position.gamma = g_random_double ();
      port->position.theta = g_random_double ();
      port->time_offset = g_random_int_range (1, 1000);
      g_hash_table_insert (ports, name, port);

      hyscan_sonar_schema_sensor_add (schema, name,
                                              HYSCAN_SENSOR_PORT_UART,
                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
    }

  for (i = 0; i < SENSOR_N_PORTS; i++)
    {
      UDPIPPortInfo *port = g_new0 (UDPIPPortInfo, 1);
      gchar *name = g_strdup_printf ("udp.%d", i + 1);

      port->type = HYSCAN_SENSOR_PORT_UDP_IP;
      port->position.x = g_random_double ();
      port->position.y = g_random_double ();
      port->position.z = g_random_double ();
      port->position.psi = g_random_double ();
      port->position.gamma = g_random_double ();
      port->position.theta = g_random_double ();
      port->time_offset = g_random_int_range (1, 1000);
      g_hash_table_insert (ports, name, port);

      hyscan_sonar_schema_sensor_add (schema, name,
                                              HYSCAN_SENSOR_PORT_UDP_IP,
                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
    }

  /* Типы синхронизации излучения. */
  hyscan_sonar_schema_sync_add (schema, sonar_info.sync_capabilities);

  /* Источники данных. */
  for (i = 0; i < 3; i++)
    {
      HyScanSourceType source;
      SourceInfo *info;

      if (i == 0)
        source = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
      else if (i == 1)
        source = HYSCAN_SOURCE_SIDE_SCAN_PORT;
      else if (i == 2)
        source = HYSCAN_SOURCE_ECHOSOUNDER;
      else
        break;

      info = select_source (source);

      hyscan_sonar_schema_source_add (schema, source,
                                      info->raw_info.antenna.pattern.vertical,
                                      info->raw_info.antenna.pattern.horizontal,
                                      info->max_receive_time);

      hyscan_sonar_schema_generator_add (schema, source,
                                         info->generator.capabilities,
                                         info->generator.signals,
                                         info->generator.min_tone_duration,
                                         info->generator.max_tone_duration,
                                         info->generator.min_lfm_duration,
                                         info->generator.max_lfm_duration);

      info->generator.preset_ids = g_new0 (guint, GENERATOR_N_PRESETS);
      info->generator.preset_names = g_new0 (gchar*, GENERATOR_N_PRESETS + 1);
      for (j = 0; j < GENERATOR_N_PRESETS; j++)
        {
          gchar *preset_name = g_strdup_printf ("%s.preset.%d", source_name (source), j + 1);

          info->generator.preset_ids[j] = hyscan_sonar_schema_generator_add_preset (schema, source, preset_name);
          info->generator.preset_names[j] = preset_name;
        }

      hyscan_sonar_schema_tvg_add (schema, source,
                                   info->tvg.capabilities,
                                   info->tvg.min_gain,
                                   info->tvg.max_gain);

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

  /* Только печать схемы на экране. */
  if (print_schema)
    {
      g_print ("%s", schema_data);
      g_free (schema_data);
      return 0;
    }

  /* Параметры гидролокатора - интерфейс HyScanSonar + HyScanDataBox. */
  sonar = hyscan_sonar_box_new (schema_data, "sonar");
  g_free (schema_data);

  /* Сервер управления. */
  server = hyscan_ssse_control_server_new (sonar);

  g_signal_connect (server, "sensor-uart-port-param", G_CALLBACK (sensor_uart_port_param_cb), &counter);
  g_signal_connect (server, "sensor-udp-ip-port-param", G_CALLBACK (sensor_udp_ip_port_param_cb), &counter);
  g_signal_connect (server, "sensor-set-enable", G_CALLBACK (sensor_set_enable_cb), &counter);

  g_signal_connect (server, "generator-set-preset", G_CALLBACK (generator_set_preset_cb), &counter);
  g_signal_connect (server, "generator-set-auto", G_CALLBACK (generator_set_auto_cb), &counter);
  g_signal_connect (server, "generator-set-simple", G_CALLBACK (generator_set_simple_cb), &counter);
  g_signal_connect (server, "generator-set-extended", G_CALLBACK (generator_set_extended_cb), &counter);
  g_signal_connect (server, "generator-set-enable", G_CALLBACK (generator_set_enable_cb), &counter);

  g_signal_connect (server, "tvg-set-auto", G_CALLBACK (tvg_set_auto_cb), &counter);
  g_signal_connect (server, "tvg-set-constant", G_CALLBACK (tvg_set_constant_cb), &counter);
  g_signal_connect (server, "tvg-set-linear-db", G_CALLBACK (tvg_set_linear_db_cb), &counter);
  g_signal_connect (server, "tvg-set-logarithmic", G_CALLBACK (tvg_set_logarithmic_cb), &counter);
  g_signal_connect (server, "tvg-set-enable", G_CALLBACK (tvg_set_enable_cb), &counter);

  g_signal_connect (server, "sonar-set-sync-type", G_CALLBACK (sonar_set_sync_type_cb), &counter);
  g_signal_connect (server, "sonar-enable-raw-data", G_CALLBACK (sonar_enable_raw_data_cb), &counter);
  g_signal_connect (server, "sonar-set-receive-time", G_CALLBACK (sonar_set_receive_time_cb), &counter);
  g_signal_connect (server, "sonar-start", G_CALLBACK (sonar_start_cb), &counter);
  g_signal_connect (server, "sonar-stop", G_CALLBACK (sonar_stop_cb), &counter);
  g_signal_connect (server, "sonar-ping", G_CALLBACK (sonar_ping_cb), &counter);

  /* База данных. */
  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);

  project_id = hyscan_db_project_create (db, project_name, NULL);
  if (project_id < 0)
    g_error ("can't create prject '%s'", project_name);

  /* Управление ГБОЭ. */
  control = hyscan_ssse_control_new (HYSCAN_SONAR (sonar), db);

  /* Проверяем наличие правого борта. */
  if (!hyscan_ssse_control_has_starboard (control))
    g_error ("ssse: no starboard");

  /* Проверяем наличие левого борта. */
  if (!hyscan_ssse_control_has_port (control))
    g_error ("ssse: no port");

  /* Проверяем отсутствие правого борта с повышенным разрешением. */
  if (hyscan_ssse_control_has_starboard_hi (control))
    g_error ("ssse: has starboard-hi");

  /* Проверяем отсутствие левого борта с повышенным разрешением. */
  if (hyscan_ssse_control_has_port_hi (control))
    g_error ("ssse: has port-hi");

  /* Проверяем наличие эхолота. */
  if (!hyscan_ssse_control_has_echosounder (control))
    g_error ("ssse: no echosounder");

  /* Проверка управления датчиками. */
  g_message ("Checking sensor control");
  check_sensor_control (HYSCAN_SENSOR_CONTROL (control));

  /* Проверка управления генераторами. */
  g_message ("Checking generator control");
  check_generator_control (HYSCAN_GENERATOR_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  check_generator_control (HYSCAN_GENERATOR_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_PORT);
  check_generator_control (HYSCAN_GENERATOR_CONTROL (control), HYSCAN_SOURCE_ECHOSOUNDER);

  /* Проверка управления системой ВАРУ. */
  g_message ("Checking tvg control");
  check_tvg_control (HYSCAN_TVG_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  check_tvg_control (HYSCAN_TVG_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_PORT);
  check_tvg_control (HYSCAN_TVG_CONTROL (control), HYSCAN_SOURCE_ECHOSOUNDER);

  /* Проверка управления гидролокатором. */
  g_message ("Checking sonar control");
  check_sonar_control (HYSCAN_SONAR_CONTROL (control), project_name);

  /* Проверка записанных данных. */
  g_message ("Checking data");
  check_data (db, project_id);

  /* Освобождаем память. */
  hyscan_db_close (db, project_id);
  hyscan_db_project_remove (db, project_name);

  g_hash_table_unref (ports);

  g_strfreev (starboard.generator.preset_names);
  g_strfreev (port.generator.preset_names);
  g_strfreev (echosounder.generator.preset_names);

  g_free (starboard.generator.preset_ids);
  g_free (port.generator.preset_ids);
  g_free (echosounder.generator.preset_ids);

  g_object_unref (control);
  g_object_unref (server);
  g_object_unref (sonar);
  g_object_unref (db);

  g_clear_pointer (&sonar_info.project_name, g_free);
  g_clear_pointer (&sonar_info.track_name, g_free);

  g_free (db_uri);

  g_message ("All done");

  return 0;
}
