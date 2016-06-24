#include <hyscan-db.h>
#include <hyscan-sonar.h>
#include <hyscan-generator-control.h>

#include "hyscan-sonar-dummy-ssse.h"

#define UART_N_DEVICES         5       /* 4 + 1 Disabled. */
#define UART_N_MODES           5       /* 4 + 1 Disabled. */
#define IP_N_ADDRESSES         5       /* 4 + 1 Disabled. */
#define N_PORTS                4       /* 4 порта каждого типа. */

/* Функция сверяет два списка строк в произвольном порядке. */
void
check_list (gchar  *error_prefix,
            gchar **orig,
            gchar **list)
{
  gint64 sum = 0;
  guint n;

  /* Hash сумма всех строк контрольного списка. */
  for (n = 0; n < g_strv_length (orig); n++)
    sum += g_str_hash (orig[n]) % 1024;

  /* Сверяем число строк в списках. */
  if (g_strv_length (orig) != g_strv_length (list))
    g_error ("%s: wrong number of elements", error_prefix);

  /* Hash сумма всех строк проверяемого списка должна совпадать
     с hash суммой всех строк контрольного списка. */
  for (n = 0; n < g_strv_length (list); n++)
    sum -= g_str_hash (list[n]) % 1024;

  if (sum != 0)
    g_error ("%s: wrong list", error_prefix);
}

/* Функция проверки списка UART устройств. */
void
check_uart_devices_list (HyScanSensorControl *sensor)
{
  HyScanDataSchemaEnumValue **uart_devices;

  gchar **list;
  gchar **orig;
  gint i;

  uart_devices = hyscan_sensor_control_list_uart_devices (sensor);
  if (uart_devices == NULL)
    g_error ("can't get uart devices list");

  i = 0;
  while (uart_devices != NULL && uart_devices[i] != NULL)
    i++;

  if (i != UART_N_DEVICES)
    g_error ("wrong number of uart devices");

  list = g_malloc0 (UART_N_DEVICES * sizeof (gchar*));
  orig = g_malloc0 (UART_N_DEVICES * sizeof (gchar*));

  for (i = 0; i < UART_N_DEVICES - 1; i++)
    {
      list[i] = g_strdup (uart_devices[i + 1]->name);
      orig[i] = g_strdup_printf ("UART%d", i + 1);
    }

  check_list ("uart devices", orig, list);

  g_strfreev (list);
  g_strfreev (orig);

  hyscan_data_schema_free_enum_values (uart_devices);
}

/* Функция проверки списка режимов работы UART устройства. */
void
check_uart_modes_list (HyScanSensorControl *sensor)
{
  HyScanDataSchemaEnumValue **uart_modes;

  gchar **list;
  gchar **orig;
  gint i;

  uart_modes = hyscan_sensor_control_list_uart_modes (sensor);
  if (uart_modes == NULL)
    g_error ("can't get uart modes list");

  i = 0;
  while (uart_modes != NULL && uart_modes[i] != NULL)
    i++;

  if (i != UART_N_MODES)
    g_error ("wrong number of uart modes");

  list = g_malloc0 (UART_N_MODES * sizeof (gchar*));
  orig = g_malloc0 (UART_N_MODES * sizeof (gchar*));

  for (i = 0; i < UART_N_MODES - 1; i++)
    {
      list[i] = g_strdup (uart_modes[i + 1]->name);
      orig[i] = g_strdup_printf ("MODE%d", i + 1);
    }

  check_list ("uart modes", orig, list);

  g_strfreev (list);
  g_strfreev (orig);

  hyscan_data_schema_free_enum_values (uart_modes);
}

/* Функция проверки списка ip адресов. */
void
check_ip_addresses_list (HyScanSensorControl *sensor)
{
  HyScanDataSchemaEnumValue **ip_addresses;

  gchar **list;
  gchar **orig;
  gint i;

  ip_addresses = hyscan_sensor_control_list_ip_addresses (sensor);
  if (ip_addresses == NULL)
    g_error ("can't get ip addresses list");

  i = 0;
  while (ip_addresses != NULL && ip_addresses[i] != NULL)
    i++;

  if (i != IP_N_ADDRESSES)
    g_error ("wrong number of uart devices");

  list = g_malloc0 (UART_N_MODES * sizeof (gchar*));
  orig = g_malloc0 (UART_N_MODES * sizeof (gchar*));

  for (i = 0; i < IP_N_ADDRESSES - 1; i++)
    {
      list[i] = g_strdup (ip_addresses[i + 1]->name);
      orig[i] = g_strdup_printf ("IP%d", i + 1);
    }

  check_list ("ip addresses", orig, list);

  g_strfreev (list);
  g_strfreev (orig);

  hyscan_data_schema_free_enum_values (ip_addresses);
}

/* Функция проверки списка портов для подключения датчиков. */
void
check_ports_list (HyScanSensorControl *sensor)
{
  gchar **ports;
  gchar **orig;
  gint i;

  ports = hyscan_sensor_control_list_ports (sensor);
  if (ports == NULL)
    g_error ("can't get ports list");

  i = 0;
  while (ports != NULL && ports[i] != NULL)
    i++;

  if (i != 3 * N_PORTS)
    g_error ("wrong number of sensor ports");

  orig = g_malloc0 ((3 * N_PORTS + 1) * sizeof (gchar*));

  for (i = 0; i < N_PORTS; i++)
    {
      orig[i] = g_strdup_printf ("virtual%d", i + 1);
      orig[i + N_PORTS] = g_strdup_printf ("uart%d", i + 1);
      orig[i + 2 * N_PORTS] = g_strdup_printf ("udp%d", i + 1);
    }

  check_list ("sensor ports", orig, ports);

  g_strfreev (ports);
  g_strfreev (orig);
}

/* Функция проверка установки параметров virtual портов. */
void
check_virtual_port_params (HyScanSensorControl *sensor)
{
  gint i;

  for (i = 0; i < N_PORTS; i++)
    {
      gchar *name;
      gint channel;
      gint time_offset;

      name = g_strdup_printf ("virtual%d", i + 1);

      if (hyscan_sensor_control_get_port_type (sensor, name) != HYSCAN_SENSOR_PORT_VIRTUAL)
        g_error ("wrong '%s' port type", name);

      for (channel = 1; channel <= 5; channel++)
        for (time_offset = 0; time_offset <= 1000; time_offset += 100)
          if (!hyscan_sensor_control_set_virtual_port_param (sensor, name, channel, time_offset))
            g_error ("can't set '%s' port parameters (channel = %d, time offset = %d)", name, channel, time_offset);

      g_free (name);
    }
}

/* Функция проверка установки параметров uart портов. */
void
check_uart_port_params (HyScanSensorControl *sensor)
{
  gint i;

  for (i = 0; i < N_PORTS; i++)
    {
      HyScanDataSchemaEnumValue **uart_devices;
      HyScanDataSchemaEnumValue **uart_modes;
      gchar *name;
      gint channel;
      gint time_offset;
      gint uart_device;
      gint uart_mode;

      uart_devices = hyscan_sensor_control_list_uart_devices (sensor);
      uart_modes = hyscan_sensor_control_list_uart_modes (sensor);
      name = g_strdup_printf ("uart%d", i + 1);

      if (hyscan_sensor_control_get_port_type (sensor, name) != HYSCAN_SENSOR_PORT_UART)
        g_error ("wrong '%s' port type", name);

      for (uart_device = 1; uart_device < UART_N_DEVICES; uart_device++)
        for (uart_mode = 1; uart_mode < UART_N_MODES; uart_mode++)
          for (channel = 1; channel <= 5; channel++)
            for (time_offset = 0; time_offset <= 1000; time_offset += 100)
              {
                gboolean status;

                status = hyscan_sensor_control_set_uart_port_param (sensor, name,
                                                                    channel, time_offset,
                                                                    HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                                    uart_devices[uart_device]->value,
                                                                    uart_modes[uart_mode]->value);
                if (!status)
                  {
                    g_error ("can't set '%s' port parameters (channel = %d, time offset = %d, uart device = %s, uart mode = %s)",
                             name,
                             channel, time_offset,
                             uart_devices[uart_device]->name,
                             uart_modes[uart_mode]->name);
                  }
              }

      g_free (name);
      hyscan_data_schema_free_enum_values (uart_devices);
      hyscan_data_schema_free_enum_values (uart_modes);
    }
}

/* Функция проверка установки параметров udp/ip портов. */
void
check_udp_ip_port_params (HyScanSensorControl *sensor)
{
  gint i;

  for (i = 0; i < N_PORTS; i++)
    {
      HyScanDataSchemaEnumValue **ip_addresses;
      gchar *name;
      gint channel;
      gint time_offset;
      gint ip_address;
      gint udp_port;

      ip_addresses = hyscan_sensor_control_list_ip_addresses (sensor);
      name = g_strdup_printf ("udp%d", i + 1);

      if (hyscan_sensor_control_get_port_type (sensor, name) != HYSCAN_SENSOR_PORT_UDP_IP)
        g_error ("wrong '%s' port type", name);

      for (ip_address = 1; ip_address < IP_N_ADDRESSES; ip_address++)
        for (udp_port = 10000; udp_port <= 60000; udp_port += 10000)
          for (channel = 1; channel <= 5; channel++)
            for (time_offset = 0; time_offset <= 1000; time_offset += 100)
              {
                gboolean status;

                status = hyscan_sensor_control_set_udp_ip_port_param (sensor, name,
                                                                      channel, time_offset,
                                                                      HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                                      ip_addresses[ip_address]->value,
                                                                      udp_port);
                if (!status)
                  {
                    g_error ("can't set '%s' port parameters (channel = %d, time offset = %d, ip address = %s, udp port = %d)",
                             name,
                             channel, time_offset,
                             ip_addresses[ip_address]->name,
                             udp_port);
                  }
              }

      g_free (name);
      hyscan_data_schema_free_enum_values (ip_addresses);
    }
}







int
main (int    argc,
      char **argv)
{
  HyScanDB *db;
  HyScanSonar *sonar;

  HyScanWriteControl *writer;
  HyScanSensorControl *sensor;

  HyScanDataSchemaEnumValue **uart_devices;
  HyScanDataSchemaEnumValue **uart_modes;
  HyScanDataSchemaEnumValue **ip_addresses;

  gchar **ports;

  gint i, j;

  db = hyscan_db_new ("file://db");
  sonar = hyscan_sonar_dummy_ssse_new (100000.0, 200000.0, 300000.0, 20.0);

  sensor = g_object_new (HYSCAN_TYPE_SENSOR_CONTROL, "sonar", sonar, "db", db, NULL);
  writer = HYSCAN_WRITE_CONTROL (sensor);

  /* Блокировка доступа к локатору. */
  if (!hyscan_sonar_lock (sonar, NULL, 0))
    g_error ("can't lock sonar");

  /* Проверка списка uart устройств. */
  check_uart_devices_list (sensor);

  /* Проверка списка режимов работы uart устройств. */
  check_uart_modes_list (sensor);

  /* Проверка списка ip адресов. */
  check_ip_addresses_list (sensor);

  /* Проверка списка портов для подключения датчиков. */
  check_ports_list (sensor);

  /* Проверка установки параметров virtual портов. */
  check_virtual_port_params (sensor);

  /* Проверка установки параметров uart портов. */
  check_uart_port_params (sensor);

  /* Проверка установки параметров udp/ip портов. */
  check_udp_ip_port_params (sensor);








  return 0;


  ports = hyscan_sensor_control_list_ports (sensor);
  if (ports == NULL)
    g_error ("empty port list");

  uart_devices = hyscan_sensor_control_list_uart_devices (sensor);
  uart_modes = hyscan_sensor_control_list_uart_modes (sensor);
  ip_addresses = hyscan_sensor_control_list_ip_addresses (sensor);

  for (i = 0; ports[i] != NULL; i++)
    {
      HyScanSensorPortType type = hyscan_sensor_control_get_port_type (sensor, ports[i]);

      switch (type)
        {
        case HYSCAN_SENSOR_PORT_VIRTUAL:
          g_message ("virtual port: %s", ports[i]);
          break;

        case HYSCAN_SENSOR_PORT_UART:
          g_message ("uart port: %s", ports[i]);
          hyscan_sensor_control_set_uart_port_param (sensor, ports[i],
                                                             1, 0, HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                             uart_devices[1]->value, uart_modes[1]->value);
          break;

        case HYSCAN_SENSOR_PORT_UDP_IP:
          g_message ("udp/ip port: %s", ports[i]);
          hyscan_sensor_control_set_udp_ip_port_param (sensor, ports[i],
                                                               1, 0, HYSCAN_SENSOR_PROTOCOL_NMEA_0183,
                                                               ip_addresses[1]->value, 10000);
          break;

        default:
          g_error ("unknown port: %s", ports[i]);
          break;
        }


    }


  for (i = 0; ports[i] != NULL; i++)
    {
      HyScanSensorPortType type = hyscan_sensor_control_get_port_type (sensor, ports[i]);
      g_message ("port: %s", ports[i]);

/*      if (type == HYSCAN_SENSOR_PORT_IP)
        hyscan_sensor_control_set_ip_port_param (sensor, ports[i], 1, 0, HYSCAN_SENSOR_PROTOCOL_NMEA_0183, 1, 1000);
      if (type == HYSCAN_SENSOR_PORT_RS232)
        hyscan_sensor_control_set_rs232_port_param (sensor, ports[i], 1, 0, HYSCAN_SENSOR_PROTOCOL_NMEA_0183, 1, 115200);*/
      if (!hyscan_sensor_control_set_enable (sensor, ports[i], TRUE))
        g_error ("can't enable port %s", ports[i]);
    }

  hyscan_db_project_create (db, "test", NULL);

  hyscan_write_control_start (writer, "test", "test");
  g_usleep (5000000);
  hyscan_write_control_start (writer, "test", "test2");
  g_usleep (5000000);
  hyscan_write_control_stop (writer);

  g_usleep (2000000);

  g_object_unref (sensor);
  g_object_unref (sonar);
  g_object_unref (db);

  return 0;
}
