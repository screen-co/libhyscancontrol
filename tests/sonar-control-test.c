
#include "hyscan-sonar-schema.h"
#include "hyscan-ssse-control-server.h"
#include "hyscan-ssse-control.h"

#include <string.h>

#define N_TESTS                                10

#define SENSOR_N_PORTS                         4

#define SENSOR_N_UART_DEVICES                  SENSOR_N_PORTS
#define SENSOR_N_UART_MODES                    8
#define SENSOR_N_IP_ADDRESSES                  4

#define GENERATOR_MIN_VERTICAL_PATTERN         30.0
#define GENERATOR_MAX_VERTICAL_PATTERN         50.0
#define GENERATOR_MIN_HORIZONTAL_PATTERN       0.1
#define GENERATOR_MAX_HORIZONTAL_PATTERN       2.0
#define GENERATOR_MIN_RECEIVE_TIME             0.1
#define GENERATOR_MAX_RECEIVE_TIME             2.0

#define GENERATOR_MIN_MIN_TONE_DURATION        1.0
#define GENERATOR_MIN_MAX_TONE_DURATION        2.0
#define GENERATOR_MAX_MIN_TONE_DURATION        2.0
#define GENERATOR_MAX_MAX_TONE_DURATION        3.0
#define GENERATOR_MIN_MIN_LFM_DURATION         10.0
#define GENERATOR_MIN_MAX_LFM_DURATION         20.0
#define GENERATOR_MAX_MIN_LFM_DURATION         20.0
#define GENERATOR_MAX_MAX_LFM_DURATION         30.0

#define GENERATOR_MIN_N_PRESETS                8
#define GENERATOR_MAX_N_PRESETS                32

#define GENERATOR_SIGNAL_N_POINTS              1024

#define TVG_MIN_MIN_GAIN                       0.0
#define TVG_MIN_MAX_GAIN                       10.0
#define TVG_MAX_MIN_GAIN                       10.0
#define TVG_MAX_MAX_GAIN                       90.0

#define TVG_N_GAINS                            512

typedef struct
{
  guint                type;
  guint                protocol;
  gboolean             enable;
  guint                status;
} VirtualPortInfo;

typedef struct
{
  guint                type;
  guint                protocol;
  gboolean             enable;
  guint                status;
  guint                uart_device;
  guint                uart_mode;
} UARTPortInfo;

typedef struct
{
  guint                type;
  guint                protocol;
  gboolean             enable;
  guint                status;
  guint                ip_address;
  guint                udp_port;
} UDPIPPortInfo;

typedef struct
{
  gdouble              vertical_pattern;
  gdouble              horizontal_pattern;
  gdouble              max_receive_time;
  gdouble              cur_receive_time;

  struct
  {
    guint              capabilities;
    guint              signals;
    guint              n_presets;
    guint             *preset_ids;
    gchar            **preset_names;
    gdouble            min_tone_duration;
    gdouble            max_tone_duration;
    gdouble            min_lfm_duration;
    gdouble            max_lfm_duration;

    gboolean           enable;
    guint              cur_mode;
    guint              cur_signal;
    guint              cur_preset;
    gdouble            cur_power;
    gdouble            cur_duration;
  } generator;

  struct
  {
    guint              capabilities;
    gdouble            min_gain;
    gdouble            max_gain;

    gboolean           enable;
    guint              cur_mode;
    gdouble            cur_level;
    gdouble            cur_sensitivity;
    gdouble            cur_gain;
    gdouble            cur_gain0;
    gdouble            cur_step;
    gdouble            cur_alpha;
    gdouble            cur_beta;
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

/* Функция возвращает информацию о борте по его типу. */
SourceInfo *
select_board (HyScanSourceType type)
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
board_name (HyScanSourceType type)
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
                           gint64                     protocol,
                           gint64                     uart_device,
                           gint64                     uart_mode,
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
                             gint64                     protocol,
                             gint64                     ip_address,
                             gint64                     udp_port,
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
                         gint                          board,
                         gint64                        preset,
                         gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                       gint                          board,
                       gint64                        signal,
                       gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                         gint                          board,
                         gint64                        signal,
                         gdouble                       power,
                         gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                           gint                          board,
                           gint64                        signal,
                           gdouble                       duration,
                           gdouble                       power,
                           gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                         gint                          board,
                         gboolean                      enable,
                         gpointer                      user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

  if (info == NULL)
    return FALSE;

  if (enable)
    {
      HyScanDataWriterSignal signal;
      HyScanComplexFloat *signal_points = g_new (HyScanComplexFloat, GENERATOR_SIGNAL_N_POINTS);
      guint i;

      for (i = 0; i < GENERATOR_SIGNAL_N_POINTS; i++)
        {
          signal_points[i].re =  1.0 * board * i;
          signal_points[i].im = -1.0 * board * i;
        }

      #warning "Fix signal rate (discretization frequency)"
      signal.time = 0;
      signal.rate = 123.456;
      signal.n_points = GENERATOR_SIGNAL_N_POINTS;
      signal.points = signal_points;

      hyscan_generator_control_server_send_signal (server, board, &signal);

      g_free (signal_points);
    }

  info->generator.enable = enable;

  *counter += 1;

  return TRUE;
}

/* Функция устанавливает автоматический режим управления ВАРУ. */
gboolean
tvg_set_auto_cb (HyScanTVGControlServer *server,
                 gint                    board,
                 gdouble                 level,
                 gdouble                 sensitivity,
                 gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                     gint                    board,
                     gdouble                 gain,
                     gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                      gint                    board,
                      gdouble                 gain0,
                      gdouble                 step,
                      gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                        gint                    board,
                        gdouble                 gain0,
                        gdouble                 beta,
                        gdouble                 alpha,
                        gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

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
                   gint                    board,
                   gboolean                enable,
                   gpointer                user_data)
{
  gint64 *counter = user_data;
  SourceInfo *info = select_board (board);

  if (info == NULL)
    return FALSE;

  if (enable)
    {
      HyScanDataWriterTVG tvg;
      gfloat *tvg_gains = g_new (gfloat, TVG_N_GAINS);
      guint i;

      for (i = 0; i < TVG_N_GAINS; i++)
          tvg_gains[i] = ((i % 2) ? 1.0 : -1.0) * board * i;

      #warning "Fix tvg rate (discretization frequency)"
      tvg.time = 0;
      tvg.rate = 123.456;
      tvg.n_gains = TVG_N_GAINS;
      tvg.gains = tvg_gains;
      hyscan_tvg_control_server_send_tvg (server, board, &tvg);

      g_free (tvg_gains);
    }

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
  SourceInfo *info = select_board (board);

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

/* Функция выполняет одиночное зондирование. */
gboolean
sonar_ping_cb (HyScanSonarControlServer *server,
               gpointer                  user_data)
{
  gint64 *counter = user_data;

#warning "Emit sonar data here"
  *counter += 1;

  return TRUE;
}

/* Функция проверяет управления портами для подключения датчиков. */
void
check_sensor_control (HyScanSensorControl *control)
{
  gchar **names;

  gint64 prev_counter;
  guint i;

  /* Список портов. */
  names = hyscan_sensor_control_list_ports (control);
  if (names == NULL)
    g_error ("can't list sensor ports");

  /* Проверка числа портов. */
  if (g_strv_length (names) != 3 * SENSOR_N_PORTS)
    g_error ("wrong number of sensor ports");

#warning "check uart_devices, uart_modes and ip_addresses lists"

  for (i = 0; names[i] != NULL; i++)
    {
      VirtualPortInfo *port;
      UARTPortInfo *uart_port;
      UDPIPPortInfo *udp_port;

      /* Проверяем наличие порта. */
      port = g_hash_table_lookup (ports, names[i]);
      if (port == NULL)
        g_error ("can't find sensor port %s", names[i]);

      /* Выключаем порт. */
      prev_counter = counter;
      if (!hyscan_sensor_control_set_enable (control, names[i], FALSE) ||
          (port->enable != FALSE) ||
          (prev_counter + 1 != counter))
        {
          g_error ("%s.sensor.disable: can't disable", names[i]);
        }

      /* Проверка порта типа HYSCAN_SENSOR_PORT_UART. */
      uart_port = g_hash_table_lookup (ports, names[i]);
      if (uart_port->type == HYSCAN_SENSOR_PORT_UART)
        {
          guint channel;
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
                                                                               protocol,
                                                                               uart_device,
                                                                               uart_mode);

                  if (!status || (prev_counter + 1 != counter) ||
                      (uart_port->protocol != protocol) ||
                      (uart_port->uart_device != uart_device) ||
                      (uart_port->uart_mode != uart_mode))
                    {
                      g_error ("%s.sensor.disable: can't set param", names[i]);
                    }
                }

          /* Настраиваем порт для теста данных. */
          channel = g_ascii_strtoll (names[i] + sizeof ("uart."), NULL, 10);
          hyscan_sensor_control_set_uart_port_param (control, names[i],
                                                              channel,
                                                              channel * 1000,
                                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                              uart_device_ids[0],
                                                              uart_mode_ids[0]);
        }

      /* Проверка порта типа HYSCAN_SENSOR_PORT_UDP_IP. */
      udp_port = g_hash_table_lookup (ports, names[i]);
      if (udp_port->type == HYSCAN_SENSOR_PORT_UDP_IP)
        {
          guint channel;
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
                      g_error ("%s.sensor.disable: can't set param", names[i]);
                    }
                }

          /* Настраиваем порт для теста данных. */
          channel = g_ascii_strtoll (names[i] + sizeof ("udp."), NULL, 10);
          hyscan_sensor_control_set_udp_ip_port_param (control, names[i],
                                                                channel,
                                                                channel * 1000,
                                                                HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                                ip_address_ids[0],
                                                                10000 + channel);
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
  SourceInfo *info = select_board (source);
  const gchar *name = board_name (source);

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

  for (i = 0; presets[i] != NULL; i++)
    {
      gboolean has_preset = FALSE;

      for (j = 0; j < info->generator.n_presets; j++)
        {
          if (presets[i]->value == 0)
            has_preset = TRUE;
          if (g_strcmp0 (presets[i]->name, info->generator.preset_names[j]) == 0)
            has_preset = TRUE;
        }

      if (!has_preset)
        g_error ("%s.generator.presets: can't find preset %s", name, presets[i]->name);
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
  SourceInfo *info = select_board (source);
  const gchar *name = board_name (source);

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
  guint i;

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

  /* Правый борт. */
  starboard.vertical_pattern = g_random_double_range   (GENERATOR_MIN_VERTICAL_PATTERN,
                                                        GENERATOR_MAX_VERTICAL_PATTERN);
  starboard.horizontal_pattern = g_random_double_range (GENERATOR_MIN_HORIZONTAL_PATTERN,
                                                        GENERATOR_MAX_HORIZONTAL_PATTERN);
  starboard.max_receive_time = g_random_double_range   (GENERATOR_MIN_RECEIVE_TIME,
                                                        GENERATOR_MAX_RECEIVE_TIME);

  starboard.generator.capabilities = HYSCAN_GENERATOR_MODE_PRESET | HYSCAN_GENERATOR_MODE_AUTO |
                                     HYSCAN_GENERATOR_MODE_SIMPLE | HYSCAN_GENERATOR_MODE_EXTENDED;
  starboard.generator.signals =      HYSCAN_GENERATOR_SIGNAL_AUTO | HYSCAN_GENERATOR_SIGNAL_TONE |
                                     HYSCAN_GENERATOR_SIGNAL_LFM | HYSCAN_GENERATOR_SIGNAL_LFMD;
  starboard.generator.n_presets =    g_random_int_range (GENERATOR_MIN_N_PRESETS,
                                                         GENERATOR_MAX_N_PRESETS);

  starboard.generator.min_tone_duration = g_random_double_range (GENERATOR_MIN_MIN_TONE_DURATION,
                                                                 GENERATOR_MIN_MAX_TONE_DURATION);
  starboard.generator.max_tone_duration = g_random_double_range (GENERATOR_MAX_MIN_TONE_DURATION,
                                                                 GENERATOR_MAX_MAX_TONE_DURATION);
  starboard.generator.min_lfm_duration =  g_random_double_range (GENERATOR_MIN_MIN_LFM_DURATION,
                                                                 GENERATOR_MIN_MAX_LFM_DURATION);
  starboard.generator.max_lfm_duration =  g_random_double_range (GENERATOR_MAX_MIN_LFM_DURATION,
                                                                 GENERATOR_MAX_MAX_LFM_DURATION);

  starboard.generator.enable = FALSE;
  starboard.generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
  starboard.generator.cur_signal = HYSCAN_GENERATOR_SIGNAL_AUTO;
  starboard.generator.cur_preset = 0;
  starboard.generator.cur_power = 0.0;
  starboard.generator.cur_duration = 0.0;

  starboard.tvg.capabilities = HYSCAN_TVG_MODE_AUTO | HYSCAN_TVG_MODE_CONSTANT |
                               HYSCAN_TVG_MODE_LINEAR_DB | HYSCAN_TVG_MODE_LOGARITHMIC;
  starboard.tvg.min_gain     = g_random_double_range (TVG_MIN_MIN_GAIN, TVG_MIN_MAX_GAIN);
  starboard.tvg.max_gain     = g_random_double_range (TVG_MAX_MIN_GAIN, TVG_MAX_MAX_GAIN);

  starboard.tvg.enable = FALSE;
  starboard.tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
  starboard.tvg.cur_level = 0;
  starboard.tvg.cur_sensitivity = 0;
  starboard.tvg.cur_gain = 0;
  starboard.tvg.cur_gain0 = 0;
  starboard.tvg.cur_step = 0;
  starboard.tvg.cur_alpha = 0;
  starboard.tvg.cur_beta = 0;

  /* Левый борт. */
  port.vertical_pattern = g_random_double_range   (GENERATOR_MIN_VERTICAL_PATTERN,
                                                   GENERATOR_MAX_VERTICAL_PATTERN);
  port.horizontal_pattern = g_random_double_range (GENERATOR_MIN_HORIZONTAL_PATTERN,
                                                   GENERATOR_MAX_HORIZONTAL_PATTERN);
  port.max_receive_time = g_random_double_range   (GENERATOR_MIN_RECEIVE_TIME,
                                                   GENERATOR_MAX_RECEIVE_TIME);

  port.generator.capabilities = HYSCAN_GENERATOR_MODE_PRESET | HYSCAN_GENERATOR_MODE_AUTO |
                                HYSCAN_GENERATOR_MODE_SIMPLE | HYSCAN_GENERATOR_MODE_EXTENDED;
  port.generator.signals =      HYSCAN_GENERATOR_SIGNAL_AUTO | HYSCAN_GENERATOR_SIGNAL_TONE |
                                HYSCAN_GENERATOR_SIGNAL_LFM | HYSCAN_GENERATOR_SIGNAL_LFMD;
  port.generator.n_presets =    g_random_int_range (GENERATOR_MIN_N_PRESETS,
                                                    GENERATOR_MAX_N_PRESETS);

  port.generator.min_tone_duration = g_random_double_range (GENERATOR_MIN_MIN_TONE_DURATION,
                                                            GENERATOR_MIN_MAX_TONE_DURATION);
  port.generator.max_tone_duration = g_random_double_range (GENERATOR_MAX_MIN_TONE_DURATION,
                                                            GENERATOR_MAX_MAX_TONE_DURATION);
  port.generator.min_lfm_duration =  g_random_double_range (GENERATOR_MIN_MIN_LFM_DURATION,
                                                            GENERATOR_MIN_MAX_LFM_DURATION);
  port.generator.max_lfm_duration =  g_random_double_range (GENERATOR_MAX_MIN_LFM_DURATION,
                                                            GENERATOR_MAX_MAX_LFM_DURATION);

  port.generator.enable = FALSE;
  port.generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
  port.generator.cur_signal = HYSCAN_GENERATOR_SIGNAL_AUTO;
  port.generator.cur_preset = 0;
  port.generator.cur_power = 0.0;
  port.generator.cur_duration = 0.0;

  port.tvg.capabilities = HYSCAN_TVG_MODE_AUTO | HYSCAN_TVG_MODE_CONSTANT |
                          HYSCAN_TVG_MODE_LINEAR_DB | HYSCAN_TVG_MODE_LOGARITHMIC;
  port.tvg.min_gain     = g_random_double_range (TVG_MIN_MIN_GAIN, TVG_MIN_MAX_GAIN);
  port.tvg.max_gain     = g_random_double_range (TVG_MAX_MIN_GAIN, TVG_MAX_MAX_GAIN);

  port.tvg.enable = FALSE;
  port.tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
  port.tvg.cur_level = 0;
  port.tvg.cur_sensitivity = 0;
  port.tvg.cur_gain = 0;
  port.tvg.cur_gain0 = 0;
  port.tvg.cur_step = 0;
  port.tvg.cur_alpha = 0;
  port.tvg.cur_beta = 0;

  /* Эхолот. */
  echosounder.vertical_pattern = g_random_double_range   (GENERATOR_MIN_VERTICAL_PATTERN,
                                                          GENERATOR_MAX_VERTICAL_PATTERN);
  echosounder.horizontal_pattern = g_random_double_range (GENERATOR_MIN_HORIZONTAL_PATTERN,
                                                          GENERATOR_MAX_HORIZONTAL_PATTERN);
  echosounder.max_receive_time = g_random_double_range   (GENERATOR_MIN_RECEIVE_TIME,
                                                          GENERATOR_MAX_RECEIVE_TIME);

  echosounder.generator.capabilities = HYSCAN_GENERATOR_MODE_PRESET | HYSCAN_GENERATOR_MODE_AUTO |
                                       HYSCAN_GENERATOR_MODE_SIMPLE | HYSCAN_GENERATOR_MODE_EXTENDED;
  echosounder.generator.signals =      HYSCAN_GENERATOR_SIGNAL_AUTO | HYSCAN_GENERATOR_SIGNAL_TONE |
                                       HYSCAN_GENERATOR_SIGNAL_LFM | HYSCAN_GENERATOR_SIGNAL_LFMD;
  echosounder.generator.n_presets =    g_random_int_range (GENERATOR_MIN_N_PRESETS,
                                                           GENERATOR_MAX_N_PRESETS);

  echosounder.generator.min_tone_duration = g_random_double_range (GENERATOR_MIN_MIN_TONE_DURATION,
                                                                   GENERATOR_MIN_MAX_TONE_DURATION);
  echosounder.generator.max_tone_duration = g_random_double_range (GENERATOR_MAX_MIN_TONE_DURATION,
                                                                   GENERATOR_MAX_MAX_TONE_DURATION);
  echosounder.generator.min_lfm_duration =  g_random_double_range (GENERATOR_MIN_MIN_LFM_DURATION,
                                                                   GENERATOR_MIN_MAX_LFM_DURATION);
  echosounder.generator.max_lfm_duration =  g_random_double_range (GENERATOR_MAX_MIN_LFM_DURATION,
                                                                   GENERATOR_MAX_MAX_LFM_DURATION);

  echosounder.generator.enable = FALSE;
  echosounder.generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
  echosounder.generator.cur_signal = HYSCAN_GENERATOR_SIGNAL_AUTO;
  echosounder.generator.cur_preset = 0;
  echosounder.generator.cur_power = 0.0;
  echosounder.generator.cur_duration = 0.0;

  echosounder.tvg.capabilities = HYSCAN_TVG_MODE_AUTO | HYSCAN_TVG_MODE_CONSTANT |
                                 HYSCAN_TVG_MODE_LINEAR_DB | HYSCAN_TVG_MODE_LOGARITHMIC;
  echosounder.tvg.min_gain     = g_random_double_range (TVG_MIN_MIN_GAIN, TVG_MIN_MAX_GAIN);
  echosounder.tvg.max_gain     = g_random_double_range (TVG_MAX_MIN_GAIN, TVG_MAX_MAX_GAIN);

  echosounder.tvg.enable = FALSE;
  echosounder.tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
  echosounder.tvg.cur_level = 0;
  echosounder.tvg.cur_sensitivity = 0;
  echosounder.tvg.cur_gain = 0;
  echosounder.tvg.cur_gain0 = 0;
  echosounder.tvg.cur_step = 0;
  echosounder.tvg.cur_alpha = 0;
  echosounder.tvg.cur_beta = 0;

  /* Параметры гидролокатора по умолчанию. */
  sonar_info.sync_capabilities = HYSCAN_SONAR_SYNC_INTERNAL |
                                 HYSCAN_SONAR_SYNC_EXTERNAL |
                                 HYSCAN_SONAR_SYNC_SOFTWARE;
  sonar_info.sync_type = 0;
  sonar_info.enable_raw_data = FALSE;
  sonar_info.enable = FALSE;
  sonar_info.project_name = NULL;
  sonar_info.track_name = NULL;

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
      g_hash_table_insert (ports, name, port);

      hyscan_sonar_schema_sensor_add (schema, name,
                                              HYSCAN_SENSOR_PORT_UDP_IP,
                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
    }

  /* Типы синхронизации излучения. */
  hyscan_sonar_schema_sync_add (schema, sonar_info.sync_capabilities);

  /* Правый борт. */
  hyscan_sonar_schema_source_add    (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                             starboard.vertical_pattern,
                                             starboard.horizontal_pattern,
                                             starboard.max_receive_time);

  hyscan_sonar_schema_generator_add (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                             starboard.generator.capabilities,
                                             starboard.generator.signals,
                                             starboard.generator.min_tone_duration,
                                             starboard.generator.max_tone_duration,
                                             starboard.generator.min_lfm_duration,
                                             starboard.generator.max_lfm_duration);

  starboard.generator.preset_ids = g_new0 (guint, starboard.generator.n_presets);
  starboard.generator.preset_names = g_new0 (gchar*, starboard.generator.n_presets + 1);
  for (i = 0; i < starboard.generator.n_presets; i++)
    {
      gchar *preset_name = g_strdup_printf ("starboard.preset.%d", i + 1);

      starboard.generator.preset_ids[i] =
        hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, preset_name);

      starboard.generator.preset_names[i] = preset_name;
    }

  hyscan_sonar_schema_tvg_add (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                       starboard.tvg.capabilities,
                                       starboard.tvg.min_gain,
                                       starboard.tvg.max_gain);

  hyscan_sonar_schema_channel_add (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1, 0.0, 0.0, 0, 1.0);
  hyscan_sonar_schema_source_add_acuostic (schema, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);


  /* Левый борт. */
  hyscan_sonar_schema_source_add    (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                             port.vertical_pattern,
                                             port.horizontal_pattern,
                                             port.max_receive_time);

  hyscan_sonar_schema_generator_add (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                             port.generator.capabilities,
                                             port.generator.signals,
                                             port.generator.min_tone_duration,
                                             port.generator.max_tone_duration,
                                             port.generator.min_lfm_duration,
                                             port.generator.max_lfm_duration);

  port.generator.preset_ids = g_new0 (guint, port.generator.n_presets);
  port.generator.preset_names = g_new0 (gchar*, port.generator.n_presets + 1);
  for (i = 0; i < port.generator.n_presets; i++)
    {
      gchar *preset_name = g_strdup_printf ("port.preset.%d", i + 1);

      port.generator.preset_ids[i] =
        hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, preset_name);

      port.generator.preset_names[i] = preset_name;
    }

  hyscan_sonar_schema_tvg_add (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                       port.tvg.capabilities,
                                       port.tvg.min_gain,
                                       port.tvg.max_gain);

  hyscan_sonar_schema_channel_add (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT, 1, 0.0, 0.0, 0, 1.0);
  hyscan_sonar_schema_source_add_acuostic (schema, HYSCAN_SOURCE_SIDE_SCAN_PORT);

  /* Эхолот. */
  hyscan_sonar_schema_source_add    (schema, HYSCAN_SOURCE_ECHOSOUNDER,
                                             echosounder.vertical_pattern,
                                             echosounder.horizontal_pattern,
                                             echosounder.max_receive_time);

  hyscan_sonar_schema_generator_add (schema, HYSCAN_SOURCE_ECHOSOUNDER,
                                             echosounder.generator.capabilities,
                                             echosounder.generator.signals,
                                             echosounder.generator.min_tone_duration,
                                             echosounder.generator.max_tone_duration,
                                             echosounder.generator.min_lfm_duration,
                                             echosounder.generator.max_lfm_duration);

  echosounder.generator.preset_ids = g_new0 (guint, echosounder.generator.n_presets);
  echosounder.generator.preset_names = g_new0 (gchar*, echosounder.generator.n_presets + 1);
  for (i = 0; i < echosounder.generator.n_presets; i++)
    {
      gchar *preset_name = g_strdup_printf ("echosounder.preset.%d", i + 1);

      echosounder.generator.preset_ids[i] =
        hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_SOURCE_ECHOSOUNDER, preset_name);

      echosounder.generator.preset_names[i] = preset_name;
    }

  hyscan_sonar_schema_tvg_add (schema, HYSCAN_SOURCE_ECHOSOUNDER,
                                       echosounder.tvg.capabilities,
                                       echosounder.tvg.min_gain,
                                       echosounder.tvg.max_gain);

  hyscan_sonar_schema_channel_add (schema, HYSCAN_SOURCE_ECHOSOUNDER, 1, 0.0, 0.0, 0, 1.0);
  hyscan_sonar_schema_source_add_acuostic (schema, HYSCAN_SOURCE_ECHOSOUNDER);

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

  /* Проверяем наличие эхолота. */
  if (!hyscan_ssse_control_has_echosounder (control))
    g_error ("ssse: no echosounder");

  /* Проверка управления датчиками. */
  check_sensor_control (HYSCAN_SENSOR_CONTROL (control));

  /* Проверка управления генераторами. */
  check_generator_control (HYSCAN_GENERATOR_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  check_generator_control (HYSCAN_GENERATOR_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_PORT);
  check_generator_control (HYSCAN_GENERATOR_CONTROL (control), HYSCAN_SOURCE_ECHOSOUNDER);

  /* Проверка управления системой ВАРУ. */
  check_tvg_control (HYSCAN_TVG_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  check_tvg_control (HYSCAN_TVG_CONTROL (control), HYSCAN_SOURCE_SIDE_SCAN_PORT);
  check_tvg_control (HYSCAN_TVG_CONTROL (control), HYSCAN_SOURCE_ECHOSOUNDER);

  /* Проверка управления гидролокатором. */
  check_sonar_control (HYSCAN_SONAR_CONTROL (control), project_name);


  g_usleep (10 * 1000000);


  /* Включаем запись. */
//  hyscan_sonar_control_start (HYSCAN_SONAR_CONTROL (control), "project", "track");

  /* Выключаем запись. */
//  hyscan_sonar_control_stop (HYSCAN_SONAR_CONTROL (control));

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

  return 0;
}

#warning "Generate test data"
#warning "Check data in db"
#warning "Check antenna pattern and offset"
#warning "Check adc offset and vref"
