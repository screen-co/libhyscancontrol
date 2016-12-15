
#include "hyscan-sonar-control.h"
#include "hyscan-sonar-proxy.h"
#include "hyscan-sensor-control-server.h"
#include "hyscan-generator-control-server.h"
#include "hyscan-tvg-control-server.h"
#include "hyscan-sonar-control-server.h"
#include "hyscan-control-common.h"
#include "hyscan-proxy-common.h"

#include <libxml/parser.h>
#include <string.h>
#include <math.h>

#define N_TESTS                        32

#define SONAR_N_SOURCES                3

#define SENSOR_N_PORTS                 4
#define SENSOR_N_CHANNELS              5
#define SENSOR_N_UART_DEVICES          SENSOR_N_PORTS
#define SENSOR_N_UART_MODES            8
#define SENSOR_N_IP_ADDRESSES          4

#define GENERATOR_N_PRESETS            32

typedef struct
{
  HyScanSensorPortType                 type;
  gboolean                             enable;
  HyScanSensorProtocolType             protocol;
  guint                                channel;
  gint64                               time_offset;
  HyScanAntennaPosition                position;
} VirtualPortInfo;

typedef struct
{
  HyScanSensorPortType                 type;
  gboolean                             enable;
  HyScanSensorProtocolType             protocol;
  guint                                channel;
  gint64                               time_offset;
  HyScanAntennaPosition                position;
  guint                                uart_device;
  guint                                uart_mode;
  guint                                uart_device_ids[SENSOR_N_UART_DEVICES+1];
  gchar                               *uart_device_names[SENSOR_N_UART_DEVICES+1];
  guint                                uart_mode_ids[SENSOR_N_UART_MODES+1];
  gchar                               *uart_mode_names[SENSOR_N_UART_MODES+1];
} UARTPortInfo;

typedef struct
{
  HyScanSensorPortType                 type;
  gboolean                             enable;
  HyScanSensorProtocolType             protocol;
  guint                                channel;
  gint64                               time_offset;
  HyScanAntennaPosition                position;
  guint                                ip_address;
  guint                                udp_port;
  guint                                ip_address_ids[SENSOR_N_IP_ADDRESSES+1];
  gchar                               *ip_address_names[SENSOR_N_IP_ADDRESSES+1];
} UDPIPPortInfo;

typedef struct
{
  HyScanAntennaPosition                position;
  HyScanRawDataInfo                    raw_info;

  gdouble                              max_receive_time;
  gdouble                              cur_receive_time;

  gdouble                              signal_rate;
  gdouble                              tvg_rate;

  struct
  {
    HyScanGeneratorModeType            capabilities;
    HyScanGeneratorSignalType          signals;
    gint                               preset_ids[GENERATOR_N_PRESETS+1];
    gchar                             *preset_names[GENERATOR_N_PRESETS+1];
    gdouble                            min_tone_duration;
    gdouble                            max_tone_duration;
    gdouble                            min_lfm_duration;
    gdouble                            max_lfm_duration;

    gboolean                           enable;
    HyScanGeneratorModeType            cur_mode;
    HyScanGeneratorSignalType          cur_signal;
    gint                               cur_preset;
    gdouble                            cur_power;
    gdouble                            cur_duration;
  } generator;

  struct
  {
    HyScanTVGModeType                  capabilities;
    gdouble                            min_gain;
    gdouble                            max_gain;

    gboolean                           enable;
    guint                              cur_mode;
    gdouble                            cur_level;
    gdouble                            cur_sensitivity;
    gdouble                            cur_gain;
    gdouble                            cur_gain0;
    gdouble                            cur_step;
    gdouble                            cur_alpha;
    gdouble                            cur_beta;
  } tvg;
} SourceInfo;

typedef struct
{
  gboolean                             enable;
  gint64                               sync_capabilities;
  gint64                               sync_type;
  gchar                               *project_name;
  gchar                               *track_name;
  HyScanTrackType                      track_type;
} SonarInfo;

typedef struct
{
  HyScanSensorControlServer           *sensor;
  HyScanGeneratorControlServer        *generator;
  HyScanTVGControlServer              *tvg;
  HyScanSonarControlServer            *sonar;
  gint64                              *counter;
} ServerInfo;

gint64                                 counter = 0;

GHashTable                            *ports;

SourceInfo                             starboard;
SourceInfo                             port;
SourceInfo                             echosounder;

SonarInfo                              sonar_info;

/* Функция возвращает тип источника данных по его индексу. */
HyScanSourceType
select_source_by_index (guint index)
{
  if (index == 0)
    return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;

  if (index == 1)
    return HYSCAN_SOURCE_SIDE_SCAN_PORT;

  if (index == 2)
    return HYSCAN_SOURCE_ECHOSOUNDER;

  return HYSCAN_SOURCE_INVALID;
}

/* Функция возвращает информацию об источнике данных по его типу. */
SourceInfo *
source_info (HyScanSourceType type)
{
  if (type == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
    return &starboard;

  if (type == HYSCAN_SOURCE_SIDE_SCAN_PORT)
    return &port;

  if (type == HYSCAN_SOURCE_ECHOSOUNDER)
    return &echosounder;

  return NULL;
}

/* Функция изменяет параметры VIRTUAL порта. */
gboolean
sensor_virtual_port_param_cb (ServerInfo  *server,
                              const gchar *name,
                              guint        channel,
                              gint64       time_offset)
{
  UARTPortInfo *port = g_hash_table_lookup (ports, name);

  if (port == NULL || port->type != HYSCAN_SENSOR_PORT_VIRTUAL)
    return FALSE;

  port->channel = channel;
  port->time_offset = time_offset;

  *server->counter += 1;

  return TRUE;
}

/* Функция изменяет параметры UART порта. */
gboolean
sensor_uart_port_param_cb (ServerInfo                *server,
                           const gchar               *name,
                           guint                      channel,
                           gint64                     time_offset,
                           HyScanSensorProtocolType   protocol,
                           guint                      uart_device,
                           guint                      uart_mode)
{
  UARTPortInfo *port = g_hash_table_lookup (ports, name);
  guint i;

  if (port == NULL || port->type != HYSCAN_SENSOR_PORT_UART)
    return FALSE;

  /* Проверяем индентификатор UART устройства. */
  for (i = 0; i <= SENSOR_N_UART_DEVICES; i++)
    if (uart_device == port->uart_device_ids[i])
      break;
  if (i > SENSOR_N_UART_DEVICES)
    return FALSE;

  /* Проверяем индентификатор режима работы UART устройства. */
  for (i = 0; i <= SENSOR_N_UART_MODES; i++)
    if (uart_mode == port->uart_mode_ids[i])
      break;
  if (i > SENSOR_N_UART_MODES)
    return FALSE;

  port->channel = channel;
  port->time_offset = time_offset;
  port->protocol = protocol;
  port->uart_device = uart_device;
  port->uart_mode = uart_mode;

  *server->counter += 1;

  return TRUE;
}

/* Функция изменяет параметры UDP/IP порта. */
gboolean
sensor_udp_ip_port_param_cb (ServerInfo                *server,
                             const gchar               *name,
                             guint                      channel,
                             gint64                     time_offset,
                             HyScanSensorProtocolType   protocol,
                             guint                      ip_address,
                             guint                      udp_port)
{
  UDPIPPortInfo *port = g_hash_table_lookup (ports, name);
  guint i;

  if (port == NULL || port->type != HYSCAN_SENSOR_PORT_UDP_IP)
    return FALSE;

  /* Проверяем индентификатор IP адреса. */
  for (i = 0; i <= SENSOR_N_IP_ADDRESSES; i++)
    if ((ip_address == 0) || (ip_address == port->ip_address_ids[i]))
      break;
  if (i > SENSOR_N_IP_ADDRESSES)
    return FALSE;

  port->channel = channel;
  port->time_offset = time_offset;
  port->protocol = protocol;
  port->ip_address = ip_address;
  port->udp_port = udp_port;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает местоположение приёмной антенны датчика. */
gboolean
sensor_set_position_cb (ServerInfo            *server,
                        const gchar           *name,
                        HyScanAntennaPosition *position)
{
  VirtualPortInfo *port = g_hash_table_lookup (ports, name);

  if (port == NULL)
    return FALSE;

  port->position = *position;

  *server->counter += 1;

  return TRUE;
}

/* Функция включает и выключает датчик. */
gboolean
sensor_set_enable_cb (ServerInfo  *server,
                      const gchar *name,
                      gboolean     enable)
{
  VirtualPortInfo *port = g_hash_table_lookup (ports, name);

  if (port == NULL)
    return FALSE;

  port->enable = enable;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает режим работы генератора по преднастройкам. */
gboolean
generator_set_preset_cb (ServerInfo       *server,
                         HyScanSourceType  source,
                         gint              preset)
{
  SourceInfo *info = source_info (source);
  guint i;

  if (info == NULL)
    return FALSE;

  /* Проверяем идентификатор преднастройки. */
  for (i = 0; i <= GENERATOR_N_PRESETS; i++)
    if ((preset == 0) || (preset == info->generator.preset_ids[i]))
      break;
  if (i > GENERATOR_N_PRESETS)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_PRESET;
  info->generator.cur_preset = preset;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает автоматический режим работы генератора. */
gboolean
generator_set_auto_cb (ServerInfo                *server,
                       HyScanSourceType           source,
                       HyScanGeneratorSignalType  signal)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_AUTO;
  info->generator.cur_signal = signal;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает упрощённый режим работы генератора. */
gboolean
generator_set_simple_cb (ServerInfo                *server,
                         HyScanSourceType           source,
                         HyScanGeneratorSignalType  signal,
                         gdouble                    power)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_SIMPLE;
  info->generator.cur_signal = signal;
  info->generator.cur_power = power;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает расширенный режим работы генератора. */
gboolean
generator_set_extended_cb (ServerInfo                *server,
                           HyScanSourceType           source,
                           HyScanGeneratorSignalType  signal,
                           gdouble                    duration,
                           gdouble                    power)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->generator.cur_mode = HYSCAN_GENERATOR_MODE_EXTENDED;
  info->generator.cur_signal = signal;
  info->generator.cur_duration = duration;
  info->generator.cur_power = power;

  *server->counter += 1;

  return TRUE;
}

/* Функция включает или отключает генератор. */
gboolean
generator_set_enable_cb (ServerInfo       *server,
                         HyScanSourceType  source,
                         gboolean          enable)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->generator.enable = enable;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает автоматический режим управления ВАРУ. */
gboolean
tvg_set_auto_cb (ServerInfo       *server,
                 HyScanSourceType  source,
                 gdouble           level,
                 gdouble           sensitivity)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_AUTO;
  info->tvg.cur_level = level;
  info->tvg.cur_sensitivity = sensitivity;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает постоянный уровень усиления ВАРУ. */
gboolean
tvg_set_constant_cb (ServerInfo       *server,
                     HyScanSourceType  source,
                     gdouble           gain)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_CONSTANT;
  info->tvg.cur_gain = gain;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает линейный закон усиления ВАРУ. */
gboolean
tvg_set_linear_db_cb (ServerInfo       *server,
                      HyScanSourceType  source,
                      gdouble           gain0,
                      gdouble           step)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_LINEAR_DB;
  info->tvg.cur_gain0 = gain0;
  info->tvg.cur_step = step;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает логарифмический закон усиления ВАРУ. */
gboolean
tvg_set_logarithmic_cb (ServerInfo       *server,
                        HyScanSourceType  source,
                        gdouble           gain0,
                        gdouble           beta,
                        gdouble           alpha)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->tvg.cur_mode = HYSCAN_TVG_MODE_LOGARITHMIC;
  info->tvg.cur_gain0 = gain0;
  info->tvg.cur_beta = beta;
  info->tvg.cur_alpha = alpha;

  *server->counter += 1;

  return TRUE;
}

/* Функция включает или отключает ВАРУ. */
gboolean
tvg_set_enable_cb (ServerInfo       *server,
                   HyScanSourceType  source,
                   gboolean          enable)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->tvg.enable = enable;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает тип синхронизации излучения. */
gboolean
sonar_set_sync_type_cb (ServerInfo          *server,
                        HyScanSonarSyncType  sync_type)
{
  sonar_info.sync_type = sync_type;
  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает местоположение приёмной антенны гидролокатора. */
gboolean
sonar_set_position_cb (ServerInfo            *server,
                       HyScanSourceType       source,
                       HyScanAntennaPosition *position)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->position = *position;

  *server->counter += 1;

  return TRUE;
}

/* Функция устанавливает время приёма эхосигнала бортом. */
gboolean
sonar_set_receive_time_cb (ServerInfo       *server,
                           HyScanSourceType  source,
                           gdouble           receive_time)
{
  SourceInfo *info = source_info (source);

  if (info == NULL)
    return FALSE;

  info->cur_receive_time = receive_time;
  *server->counter += 1;

  return TRUE;
}

/* Функция включает гидролокатор в работу. */
gboolean
sonar_start_cb (ServerInfo      *server,
                const gchar     *project_name,
                const gchar     *track_name,
                HyScanTrackType  track_type)
{
  g_clear_pointer (&sonar_info.project_name, g_free);
  g_clear_pointer (&sonar_info.track_name, g_free);

  sonar_info.project_name = g_strdup (project_name);
  sonar_info.track_name = g_strdup (track_name);
  sonar_info.track_type = track_type;
  sonar_info.enable = TRUE;
  *server->counter += 1;

  return TRUE;
}

/* Функция останавливает работу гидролокатора. */
gboolean
sonar_stop_cb (ServerInfo *server)
{
  g_clear_pointer (&sonar_info.project_name, g_free);
  g_clear_pointer (&sonar_info.track_name, g_free);

  sonar_info.enable = FALSE;
  *server->counter += 1;

  return TRUE;
}

/* Функция выполняет цикл зондирования. */
gboolean
sonar_ping_cb (ServerInfo *server)
{
  *server->counter += 1;

  return TRUE;
}

/* Функция проверяет управление портами для подключения датчиков. */
void
check_sensor_control (HyScanSensorControl      *control,
                      HyScanSonarProxyModeType  proxy_mode)
{
  HyScanDataSchemaEnumValue **uart_devices;
  HyScanDataSchemaEnumValue **uart_modes;
  HyScanDataSchemaEnumValue **ip_addresses;
  gchar **names;

  gint64 prev_counter;
  guint i, j, k, n;

  /* Список портов. */
  names = hyscan_sensor_control_list_ports (control);
  if (names == NULL)
    g_error ("can't list sensor ports");

  /* Проверка числа портов. */
  if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      if (g_strv_length (names) != 3 * SENSOR_N_PORTS)
        g_error ("wrong number of sensor ports");
    }
  else
    {
      if (g_strv_length (names) != 1)
        g_error ("wrong number of sensor ports");
    }

  for (i = 0; names[i] != NULL; i++)
    {
      VirtualPortInfo *port;
      UARTPortInfo *uart_port;
      UDPIPPortInfo *udp_port;
      HyScanAntennaPosition position;

      gint64 time_offset;
      gboolean status;

      /* Проверяем наличие порта. */
      port = g_hash_table_lookup (ports, names[i]);
      if (port == NULL)
        g_error ("can't find sensor port %s", names[i]);

      if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
        {
          /* Позиция приёмной антенны. */
          position.x = g_random_double ();
          position.y = g_random_double ();
          position.z = g_random_double ();
          position.psi = g_random_double ();
          position.gamma = g_random_double ();
          position.theta = g_random_double ();

          prev_counter = counter;
          status = hyscan_sensor_control_set_position (control, names[i], &position);
          if (!status || (prev_counter + 1 != counter) ||
              (port->position.x != position.x) ||
              (port->position.y != position.y) ||
              (port->position.z != position.z) ||
              (port->position.psi != position.psi) ||
              (port->position.gamma != position.gamma) ||
              (port->position.theta != position.theta))
            {
              g_error ("sensor.%s: can't set position", names[i]);
            }

          /* Выключаем порт. */
          prev_counter = counter;
          status = hyscan_sensor_control_set_enable (control, names[i], FALSE);
          if (!status || (prev_counter + 1 != counter) ||
              (port->enable != FALSE))
            {
              g_error ("sensor.%s: can't disable", names[i]);
            }
        }

      /* Настраиваем порт типа HYSCAN_SENSOR_PORT_VIRTUAL для теста данных. */
      if (port->type == HYSCAN_SENSOR_PORT_VIRTUAL)
        {
          for (j = SENSOR_N_CHANNELS; j >= 1; j--)
            {
              time_offset = g_random_int_range (1, 1000);

              prev_counter = counter;
              status = hyscan_sensor_control_set_virtual_port_param (control, names[i], j, time_offset);
              if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
                {
                  if (!status || (prev_counter + 1 != counter) ||
                      (port->channel != j) ||
                      (port->time_offset != time_offset))
                    {
                      g_error ("sensorv.%s: can't set param", names[i]);
                    }
                }
              else
                {
                  if (!status)
                    g_error ("sensor.%s: can't set param", names[i]);
                }
            }
        }

      /* Проверка порта типа HYSCAN_SENSOR_PORT_UART. */
      uart_port = g_hash_table_lookup (ports, names[i]);
      if (uart_port->type == HYSCAN_SENSOR_PORT_UART)
        {
          /* Список UART устройств. */
          uart_devices = hyscan_sensor_control_list_uart_devices (control, names[i]);
          if (uart_devices == NULL)
            g_error ("sensor.uart-devices: can't get");

          if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
            {
              for (j = 0; j <= SENSOR_N_UART_DEVICES; j++)
                {
                  gboolean has_uart_device = FALSE;

                  for (k = 0; uart_devices[k] != NULL; k++)
                    if (g_strcmp0 (uart_port->uart_device_names[j], uart_devices[k]->name) == 0)
                      has_uart_device = TRUE;

                  if (!has_uart_device)
                    g_error ("sensor.uart-devices: can't find device %s", uart_port->uart_device_names[j]);
                }
            }

          /* Список UART режимов. */
          uart_modes = hyscan_sensor_control_list_uart_modes (control, names[i]);
          if (uart_modes == NULL)
            g_error ("sensor.uart-modes: can't get");

          if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
            {
              for (j = 0; j <= SENSOR_N_UART_MODES; j++)
                {
                  gboolean has_uart_mode = FALSE;

                  for (k = 0; uart_modes[k] != NULL; k++)
                    if (g_strcmp0 (uart_port->uart_mode_names[j], uart_modes[k]->name) == 0)
                      has_uart_mode = TRUE;

                  if (!has_uart_mode)
                    g_error ("sensor.uart-modes: can't find mode %s", uart_port->uart_mode_names[j]);
              }
            }


          for (j = 0; j <= SENSOR_N_UART_DEVICES; j++)
            for (k = 0; k <= SENSOR_N_UART_MODES; k++)
              for (n = SENSOR_N_CHANNELS; n >= 2; n--)
                {
                  HyScanSensorProtocolType protocol;
                  guint uart_device = uart_devices[j]->value;
                  guint uart_mode = uart_modes[k]->value;

                  protocol = (n % 2) ? HYSCAN_SENSOR_PROTOCOL_SAS : HYSCAN_SENSOR_PROTOCOL_NMEA_0183;
                  time_offset = g_random_int_range (1, 1000);

                  prev_counter = counter;
                  status = hyscan_sensor_control_set_uart_port_param (control, names[i], n, time_offset,
                                                                      protocol, uart_device, uart_mode);

                  if (!status || (prev_counter + 1 != counter) ||
                      (uart_port->channel != n) ||
                      (uart_port->time_offset != time_offset) ||
                      (uart_port->protocol != protocol) ||
                      (uart_port->uart_device != uart_port->uart_device_ids[j]) ||
                      (uart_port->uart_mode != uart_port->uart_mode_ids[k]))
                    {
                      g_error ("sensor.%s: can't set param", names[i]);
                    }
                }

          hyscan_data_schema_free_enum_values (uart_devices);
          hyscan_data_schema_free_enum_values (uart_modes);
        }

      /* Проверка порта типа HYSCAN_SENSOR_PORT_UDP_IP. */
      udp_port = g_hash_table_lookup (ports, names[i]);
      if (udp_port->type == HYSCAN_SENSOR_PORT_UDP_IP)
        {
          /* Список IP адресов. */
          ip_addresses = hyscan_sensor_control_list_ip_addresses (control, names[i]);
          if (ip_addresses == NULL)
            g_error ("sensor.ip-addresses: can't get");

          if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
            {
              for (j = 0; j < SENSOR_N_IP_ADDRESSES; j++)
                {
                  gboolean has_ip_address = FALSE;

                  for (k = 0; ip_addresses[k] != NULL; k++)
                    if (g_strcmp0 (udp_port->ip_address_names[j], ip_addresses[k]->name) == 0)
                      has_ip_address = TRUE;

                  if (!has_ip_address)
                    g_error ("sensor.ip-addresses: can't find ip address %s", udp_port->ip_address_names[j]);
                }
            }

          for (j = 0; j < SENSOR_N_IP_ADDRESSES; j++)
            for (k = SENSOR_N_CHANNELS; k >= 3; k--)
                {
                  HyScanSensorProtocolType protocol;
                  guint ip_address = ip_addresses[j]->value;
                  guint16 udp_port_n = g_random_int_range (1024, 65535);

                  protocol = (k % 2) ? HYSCAN_SENSOR_PROTOCOL_NMEA_0183 : HYSCAN_SENSOR_PROTOCOL_SAS;
                  time_offset = g_random_int_range (1, 1000);

                  prev_counter = counter;
                  status = hyscan_sensor_control_set_udp_ip_port_param (control, names[i], k, time_offset,
                                                                        protocol, ip_address, udp_port_n);

                  if (!status || (prev_counter + 1 != counter) ||
                      (udp_port->channel != k) ||
                      (udp_port->time_offset != time_offset) ||
                      (udp_port->protocol != protocol) ||
                      (udp_port->ip_address != udp_port->ip_address_ids[j]) ||
                      (udp_port->udp_port != udp_port_n))
                    {
                      g_error ("sensor.%s: can't set param", names[i]);
                    }
                }

          hyscan_data_schema_free_enum_values (ip_addresses);
        }

      if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
        {
          /* Включаем порт. */
          prev_counter = counter;
          if (!hyscan_sensor_control_set_enable (control, names[i], TRUE) ||
              (port->enable != TRUE) ||
              (prev_counter + 1 != counter))
            {
              g_error ("sensor.%s.enable: can't enable", names[i]);
            }
        }
    }

  g_strfreev (names);
}

/* Функция проверяет управление генератором. */
void
check_generator_control (HyScanGeneratorControl *control,
                         HyScanSourceType        source)
{
  SourceInfo *info = source_info (source);
  const gchar *name = hyscan_channel_get_name_by_types (source, FALSE, 1);

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
    g_error ("generator.%s.capabilities: mismatch", name);

  /* Допустимые сигналы. */
  signals = hyscan_generator_control_get_signals (control, source);
  if (signals != info->generator.signals)
    g_error ("generator.%s.signals: mismatch", name);

  /* Диапазон длительностей тонального сигнала. */
  if (!hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_TONE, &min_duration, &max_duration))
    g_error ("generator.%s.duration_range (tone): can't get", name);

  if (min_duration != info->generator.min_tone_duration || max_duration != info->generator.max_tone_duration)
    g_error ("generator.%s.duration_range (tone): mismatch", name);

  /* Диапазон длительностей ЛЧМ сигнала. */
  if (!hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_LFM, &min_duration, &max_duration))
    g_error ("generator.%s.duration_range (lfm): can't get", name);

  if (min_duration != info->generator.min_lfm_duration || max_duration != info->generator.max_lfm_duration)
    g_error ("generator.%s.duration_range (lfm): mismatch", name);

  if (!hyscan_generator_control_get_duration_range (control, source, HYSCAN_GENERATOR_SIGNAL_LFMD, &min_duration, &max_duration))
    g_error ("generator.%s.duration_range (lfmd): can't get", name);

  if (min_duration != info->generator.min_lfm_duration || max_duration != info->generator.max_lfm_duration)
    g_error ("generator.%s.duration_range (lfmd): mismatch", name);

  /* Преднастройки генератора. */
  presets = hyscan_generator_control_list_presets (control, source);
  if (presets == NULL)
    g_error ("generator.%s.presets: can't get", name);

  for (i = 0; i <= GENERATOR_N_PRESETS; i++)
    {
      gboolean has_preset = FALSE;

      for (j = 0; presets[j] != NULL; j++)
        if (g_strcmp0 (presets[j]->name, info->generator.preset_names[i]) == 0)
          has_preset = TRUE;

      if (!has_preset)
        g_error ("generator.%s.presets: can't find preset %s", name, info->generator.preset_names[i]);
    }

  for (i = 0; presets[i] != NULL; i++)
    {
      prev_counter = counter;
      if (!hyscan_generator_control_set_preset (control, source, presets[i]->value) ||
          (info->generator.cur_mode != HYSCAN_GENERATOR_MODE_PRESET) ||
          (info->generator.cur_preset != info->generator.preset_ids[i]) ||
          (prev_counter + 1 != counter))
        {
          g_error ("generator.%s.presets: can't set preset %s", name, presets[i]->name);
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
          g_error ("generator.%s.auto: can't set mode", name);
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
          g_error ("generator.%s.simple: can't set mode", name);
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
          g_error ("generator.%s.extended: can't set mode", name);
        }
    }

  /* Включение / выключение. */
  prev_counter = counter;
  if (!hyscan_generator_control_set_enable (control, source, TRUE) ||
      (info->generator.enable != TRUE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("generator.%s.enable: can't enable", name);
    }
  prev_counter = counter;
  if (!hyscan_generator_control_set_enable (control, source, FALSE) ||
      (info->generator.enable != FALSE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("generator.%s.enable: can't disable", name);
    }
}

/* Функция проверяет управление системой ВАРУ. */
void
check_tvg_control (HyScanTVGControl *control,
                   HyScanSourceType  source)
{
  SourceInfo *info = source_info (source);
  const gchar *name = hyscan_channel_get_name_by_types (source, FALSE, 1);

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
    g_error ("tvg.%s.capabilities: mismatch", name);

  /* Диапазон значений коэффициентов усилений ВАРУ. */
  if (!hyscan_tvg_control_get_gain_range (control, source, &min_gain, &max_gain))
    g_error ("tvg.%s.gain_range: can't get", name);

  if (min_gain != info->tvg.min_gain || max_gain != info->tvg.max_gain)
    g_error ("tvg.%s.gain_range: mismatch", name);

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
          g_error ("tvg.%s.auto: can't set mode", name);
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
          g_error ("tvg.%s.constant: can't set mode", name);
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
          g_error ("tvg.%s.linear_db: can't set mode", name);
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
          g_error ("tvg.%s.logarithmic: can't set mode", name);
        }
    }

  /* Включение / выключение. */
  prev_counter = counter;
  if (!hyscan_tvg_control_set_enable (control, source, TRUE) ||
      (info->tvg.enable != TRUE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("tvg.%s.enable: can't enable", name);
    }
  prev_counter = counter;
  if (!hyscan_tvg_control_set_enable (control, source, FALSE) ||
      (info->tvg.enable != FALSE) ||
      (prev_counter + 1 != counter))
    {
      g_error ("tvg.%s.enable: can't disable", name);
    }
}

/* Функция проверяет управление гидролокатором. */
void
check_sonar_control (HyScanSonarControl       *control,
                     HyScanSonarProxyModeType  proxy_mode)
{
  HyScanSonarSyncType capabilities;

  gint64 prev_counter;
  gboolean status;
  guint i, j;

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

  /* Время приёма эхосигналов. */
  for (i = 0; i < N_TESTS; i++)
    {
      for (j = 0; j < SONAR_N_SOURCES; j++)
        {
          HyScanSourceType source = select_source_by_index (j);
          SourceInfo *info = source_info (source);
          gdouble receive_time;

          receive_time = g_random_double_range (0.001, info->max_receive_time);
          prev_counter = counter;
          if (!hyscan_sonar_control_set_receive_time (control, source, receive_time) ||
              (info->cur_receive_time != receive_time) ||
              (prev_counter + 1 != counter))
            {
              g_error ("sonar.%s.receive_time: can't set", hyscan_channel_get_name_by_types (source, FALSE, 1));
            }
        }
    }

  /* Местоположение приёмных антенн. */
  for (i = 0; i < N_TESTS; i++)
    {
      for (j = 0; j < SONAR_N_SOURCES; j++)
        {
          HyScanSourceType source = select_source_by_index (j);
          SourceInfo *info = source_info (source);
          HyScanAntennaPosition position;

          position.x = g_random_double ();
          position.y = g_random_double ();
          position.z = g_random_double ();
          position.psi = g_random_double ();
          position.gamma = g_random_double ();
          position.theta = g_random_double ();

          prev_counter = counter;
          status = hyscan_sonar_control_set_position (control, source, &position);
          if (!status || (prev_counter + 1 != counter) ||
              (info->position.x != position.x) ||
              (info->position.y != position.y) ||
              (info->position.z != position.z) ||
              (info->position.psi != position.psi) ||
              (info->position.gamma != position.gamma) ||
              (info->position.theta != position.theta))
            {
              g_error ("sonar.%s: can't set position", hyscan_channel_get_name_by_types (source, FALSE, 1));
            }
        }
    }

  /* Включаем гидролокатор в работу. */
  for (i = 0; i < N_TESTS; i++)
    {
      gchar *project_name = g_strdup_printf ("test-project-%d", i);
      gchar *track_name = g_strdup_printf ("test-track-%d", i);
      HyScanTrackType track_type = HYSCAN_TRACK_SURVEY + (i % 2);

      prev_counter = counter;
      if (!hyscan_sonar_control_start (control, project_name, track_name, track_type) ||
          (g_strcmp0 (sonar_info.project_name, project_name) != 0) ||
          (g_strcmp0 (sonar_info.track_name, track_name) != 0) ||
          (sonar_info.track_type != track_type) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar: can't start");
        }

      prev_counter = counter;
      if( !hyscan_sonar_control_ping (control) ||
          (prev_counter + 1 != counter))
        {
          g_error ("sonar: can't ping");
        }

      g_free (project_name);
      g_free (track_name);
    }

  /* Выключаем гидролокатор. */
  prev_counter = counter;
  if (!hyscan_sonar_control_stop (control) ||
      (sonar_info.project_name != NULL) ||
      (sonar_info.track_name != NULL) ||
      (prev_counter + 1 != counter))
    {
      g_error ("sonar: can't stop");
    }
}

int
main (int    argc,
      char **argv)
{
  HyScanSonarSchema *schema;
  gchar *schema_data;

  ServerInfo server;
  HyScanSonarBox *sonar;
  HyScanSonarProxy *proxy;
  HyScanSonarControl *control;

  gchar *proxy_mode_string = NULL;
  HyScanSonarProxyModeType proxy_mode = 0;
  gboolean print_schema = FALSE;

  gint *sources;

  guint pow2;
  guint i, j;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "proxy", 'p', 0, G_OPTION_ARG_STRING, &proxy_mode_string, "Proxy mode (all, computed)", NULL },
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
        g_message (error->message);
        return -1;
      }

    if (proxy_mode_string != NULL)
      {
        if (g_strcmp0 (proxy_mode_string, "all") == 0)
          {
            proxy_mode = HYSCAN_SONAR_PROXY_MODE_ALL;
          }
        else if (g_strcmp0 (proxy_mode_string, "computed") == 0)
          {
            proxy_mode = HYSCAN_SONAR_PROXY_MODE_COMPUTED;
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

  /* Порты для датчиков. */
  ports = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      for (i = 0; i < SENSOR_N_PORTS; i++)
        {
          VirtualPortInfo *port = g_new0 (VirtualPortInfo, 1);
          gchar *name = g_strdup_printf ("virtual.%d", i + 1);

          port->type = HYSCAN_SENSOR_PORT_VIRTUAL;
          g_hash_table_insert (ports, name, port);

          hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
        }

      for (i = 0; i < SENSOR_N_PORTS; i++)
        {
          UARTPortInfo *port = g_new0 (UARTPortInfo, 1);
          gchar *name = g_strdup_printf ("uart.%d", i + 1);

          port->type = HYSCAN_SENSOR_PORT_UART;
          g_hash_table_insert (ports, name, port);

          hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

          /* UART устройства. */
          port->uart_device_names[0] = g_strdup ("Disabled");
          port->uart_device_ids[0] = 0;
          for (j = 1; j <= SENSOR_N_UART_DEVICES; j++)
            {
              port->uart_device_names[j] = g_strdup_printf ("ttyS%d", j);
              port->uart_device_ids[j] = hyscan_sonar_schema_sensor_add_uart_device (schema, name, port->uart_device_names[j]);
            }

          /* Режимы UART устройств. */
          port->uart_mode_names[0] = g_strdup  ("Disabled");
          port->uart_mode_ids[0] = 0;
          for (j = 1, pow2 = 1; j <= SENSOR_N_UART_MODES; j += 1, pow2 *= 2)
            {
              port->uart_mode_names[j] = g_strdup_printf ("%d baud", 150 * pow2);
              port->uart_mode_ids[j] = hyscan_sonar_schema_sensor_add_uart_mode (schema, name, port->uart_mode_names[j]);
            }
        }

      for (i = 0; i < SENSOR_N_PORTS; i++)
        {
          UDPIPPortInfo *port = g_new0 (UDPIPPortInfo, 1);
          gchar *name = g_strdup_printf ("udp.%d", i + 1);

          port->type = HYSCAN_SENSOR_PORT_UDP_IP;
          g_hash_table_insert (ports, name, port);

          hyscan_sonar_schema_sensor_add (schema, name, HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

          /* IP адреса. */
          port->ip_address_names[0] = g_strdup  ("Disabled");
          port->ip_address_ids[0] = 0;
          for (j = 1; j <= SENSOR_N_IP_ADDRESSES; j++)
            {
              port->ip_address_names[j] = g_strdup_printf ("10.10.10.%d", j + 1);
              port->ip_address_ids[j] = hyscan_sonar_schema_sensor_add_ip_address (schema, name, port->ip_address_names[j]);
            }
        }
    }
  else
    {
      VirtualPortInfo *port = g_new0 (VirtualPortInfo, 1);
      gchar *name = g_strdup (HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME);

      port->type = HYSCAN_SENSOR_PORT_VIRTUAL;
      g_hash_table_insert (ports, name, port);

      hyscan_sonar_schema_sensor_add (schema, name,
                                              HYSCAN_SENSOR_PORT_VIRTUAL,
                                              HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
    }

  /* Типы синхронизации излучения. */
  hyscan_sonar_schema_sync_add (schema, sonar_info.sync_capabilities);

  /* Источники данных. */
  for (i = 0; i < SONAR_N_SOURCES; i++)
    {
      HyScanSourceType source = select_source_by_index (i);
      SourceInfo *info = source_info (source);

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

      info->generator.preset_ids[0] = 0;
      info->generator.preset_names[0] = g_strdup ("Disabled");
      for (j = 1; j <= GENERATOR_N_PRESETS; j++)
        {
          gchar *preset_name = g_strdup_printf ("%s.preset.%d", hyscan_channel_get_name_by_types (source, FALSE, 1), j + 1);

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

  /* Параметры гидролокатора - интерфейс. */
  sonar = hyscan_sonar_box_new ();
  hyscan_sonar_box_set_schema (sonar, schema_data, "sonar");
  g_free (schema_data);

  /* Сервер управления. */
  server.sensor = hyscan_sensor_control_server_new (sonar);
  server.generator = hyscan_generator_control_server_new (sonar);
  server.tvg = hyscan_tvg_control_server_new (sonar);
  server.sonar = hyscan_sonar_control_server_new (sonar);
  server.counter = &counter;

  g_signal_connect_swapped (server.sensor, "sensor-virtual-port-param",
                            G_CALLBACK (sensor_virtual_port_param_cb), &server);
  g_signal_connect_swapped (server.sensor, "sensor-uart-port-param",
                            G_CALLBACK (sensor_uart_port_param_cb), &server);
  g_signal_connect_swapped (server.sensor, "sensor-udp-ip-port-param",
                            G_CALLBACK (sensor_udp_ip_port_param_cb), &server);
  g_signal_connect_swapped (server.sensor, "sensor-set-position",
                            G_CALLBACK (sensor_set_position_cb), &server);
  g_signal_connect_swapped (server.sensor, "sensor-set-enable",
                            G_CALLBACK (sensor_set_enable_cb), &server);

  g_signal_connect_swapped (server.generator, "generator-set-preset",
                            G_CALLBACK (generator_set_preset_cb), &server);
  g_signal_connect_swapped (server.generator, "generator-set-auto",
                            G_CALLBACK (generator_set_auto_cb), &server);
  g_signal_connect_swapped (server.generator, "generator-set-simple",
                            G_CALLBACK (generator_set_simple_cb), &server);
  g_signal_connect_swapped (server.generator, "generator-set-extended",
                            G_CALLBACK (generator_set_extended_cb), &server);
  g_signal_connect_swapped (server.generator, "generator-set-enable",
                            G_CALLBACK (generator_set_enable_cb), &server);

  g_signal_connect_swapped (server.tvg, "tvg-set-auto",
                            G_CALLBACK (tvg_set_auto_cb), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-constant",
                            G_CALLBACK (tvg_set_constant_cb), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-linear-db",
                            G_CALLBACK (tvg_set_linear_db_cb), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-logarithmic",
                            G_CALLBACK (tvg_set_logarithmic_cb), &server);
  g_signal_connect_swapped (server.tvg, "tvg-set-enable",
                            G_CALLBACK (tvg_set_enable_cb), &server);

  g_signal_connect_swapped (server.sonar, "sonar-set-sync-type",
                            G_CALLBACK (sonar_set_sync_type_cb), &server);
  g_signal_connect_swapped (server.sonar, "sonar-set-position",
                            G_CALLBACK (sonar_set_position_cb), &server);
  g_signal_connect_swapped (server.sonar, "sonar-set-receive-time",
                            G_CALLBACK (sonar_set_receive_time_cb), &server);
  g_signal_connect_swapped (server.sonar, "sonar-start",
                            G_CALLBACK (sonar_start_cb), &server);
  g_signal_connect_swapped (server.sonar, "sonar-stop",
                            G_CALLBACK (sonar_stop_cb), &server);
  g_signal_connect_swapped (server.sonar, "sonar-ping",
                            G_CALLBACK (sonar_ping_cb), &server);

  /* Управление ГБОЭ. */
  if (proxy_mode_string != NULL)
    {
      control = hyscan_sonar_control_new (HYSCAN_PARAM (sonar), NULL);
      proxy = hyscan_sonar_proxy_new (control, proxy_mode, proxy_mode);
      g_object_unref (control);

      if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_COMPUTED)
        hyscan_sensor_proxy_set_source (HYSCAN_SENSOR_PROXY (proxy), HYSCAN_SENSOR_PROXY_VIRTUAL_PORT_NAME);

      control = hyscan_sonar_control_new (HYSCAN_PARAM (proxy), NULL);
    }
  else
    {
      proxy = NULL;
      control = hyscan_sonar_control_new (HYSCAN_PARAM (sonar), NULL);
    }

  /* Только печать схемы на экране. */
  if (print_schema)
    {
      HyScanDataSchema *schema;

      schema = hyscan_param_schema (HYSCAN_PARAM (control));
      schema_data = hyscan_data_schema_get_data (schema, NULL, NULL);
      g_print ("%s", schema_data);
      g_free (schema_data);

      g_object_unref (schema);

      goto exit;
    }

  /* Проверяем список источников гидролокационных данных. */
  sources = hyscan_sonar_control_source_list (HYSCAN_SONAR_CONTROL (control));
  if (sources == NULL)
    g_error ("sonar: can't list sources");

  for (i = 0; sources[i] != HYSCAN_SOURCE_INVALID; i++)
    {
      if (sources[i] == HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
        continue;
      if (sources[i] == HYSCAN_SOURCE_SIDE_SCAN_PORT)
        continue;
      if (sources[i] == HYSCAN_SOURCE_ECHOSOUNDER)
        continue;

      g_error ("unsupported source: %s", hyscan_channel_get_name_by_types (sources[i], FALSE, 1));
    }

  g_free (sources);

  /* Проверка управления датчиками. */
  g_message ("Checking sensor control");
  check_sensor_control (HYSCAN_SENSOR_CONTROL (control), proxy_mode);

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
  check_sonar_control (HYSCAN_SONAR_CONTROL (control), proxy_mode);

  g_message ("All done");

exit:
  if (proxy_mode == HYSCAN_SONAR_PROXY_MODE_ALL)
    {
      for (i = 0; i < SENSOR_N_PORTS; i++)
        {
          gchar *name = g_strdup_printf ("uart.%d", i + 1);
          UARTPortInfo *port = g_hash_table_lookup (ports, name);

          for (j = 0; j <= SENSOR_N_UART_DEVICES; j++)
            g_free (port->uart_device_names[j]);
          for (j = 0; j <= SENSOR_N_UART_MODES; j++)
            g_free (port->uart_mode_names[j]);

          g_free (name);
        }

      for (i = 0; i < SENSOR_N_PORTS; i++)
        {
          gchar *name = g_strdup_printf ("udp.%d", i + 1);
          UDPIPPortInfo *port = g_hash_table_lookup (ports, name);

          for (j = 0; j <= SENSOR_N_IP_ADDRESSES; j++)
            g_free (port->ip_address_names[j]);

          g_free (name);
        }
    }

  for (i = 0; i < SONAR_N_SOURCES; i++)
    {
      HyScanSourceType source = select_source_by_index (i);
      SourceInfo *info = source_info (source);

      for (j = 0; j <= GENERATOR_N_PRESETS; j++)
        g_free (info->generator.preset_names[j]);
    }

  g_hash_table_unref (ports);

  g_clear_pointer (&sonar_info.project_name, g_free);
  g_clear_pointer (&sonar_info.track_name, g_free);

  g_object_unref (control);
  g_clear_object (&proxy);
  g_object_unref (server.sensor);
  g_object_unref (server.generator);
  g_object_unref (server.tvg);
  g_object_unref (server.sonar);
  g_object_unref (sonar);

  xmlCleanupParser ();

  return 0;
}
