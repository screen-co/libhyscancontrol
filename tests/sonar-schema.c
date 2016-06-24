#include <hyscan-sonar-schema.h>


//#ifdef G_OS_UNIX
//  {
//    struct ifaddrs *ifaddrs, *ifa;
//
//    if (getifaddrs (&ifaddrs) != 0)
//      return;
//
//    for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next)
//      {
//        char ip[1024];
//
//        if (ifa->ifa_addr == NULL)
//          continue;
//
//        if (ifa->ifa_addr->sa_family != AF_INET)
//          continue;
//
//        if (getnameinfo (ifa->ifa_addr, sizeof (struct sockaddr_in), ip, sizeof (ip), NULL, 0, NI_NUMERICHOST) != 0)
//          continue;
//
//        address = g_new (HyScanSonarSchemaIPAddress, 1);
//        address->address = g_inet_address_new_from_string (ip);
//        address->id = id++;
//
//        g_hash_table_insert (ip_addresses, &address->id, address);
//        hyscan_data_schema_builder_enum_value_create (builder, "ip-address",
//                                                      address->id,
//                                                      ip, NULL);
//      }
//
//    freeifaddrs (ifaddrs);
//  }
//#endif
//
//#ifdef __linux__
//  {
//    GDir *dir;
//    const gchar *tty_dev;
//
//    dir = g_dir_open ("/dev", 0, NULL);
//
//    while ((tty_dev = g_dir_read_name (dir)) != NULL)
//      {
//        struct serial_struct dev_info;
//        gchar *dev_name = NULL;
//        gchar *dev_path;
//        gint dev_fd;
//
//        gboolean is_pci;
//        gboolean is_usb;
//
//        /* Проверяем только стандартные порты и USB-RS232 преобразователи. */
//        is_pci = g_str_has_prefix (tty_dev, "ttyS");
//        is_usb = g_str_has_prefix (tty_dev, "ttyUSB");
//
//        if (!is_pci && !is_usb)
//          continue;
//
//        /* Проверяем что устройство существует. */
//        dev_path = g_build_filename ("/dev", tty_dev, NULL);
//        dev_fd = open (dev_path, O_RDWR | O_NOCTTY | O_NDELAY);
//        g_free (dev_path);
//        if (dev_fd < 0)
//          continue;
//
//        /* Проверяем что устройство является последовательным портом. */
//        if (ioctl (dev_fd, TIOCMGET, &dev_info) < 0)
//          {
//            is_pci = FALSE;
//            is_usb = FALSE;
//          }
//
//        close (dev_fd);
//
//        if (is_pci)
//          {
//            gint index = g_ascii_strtoll (tty_dev + 4, NULL, 10);
//            dev_name = g_strdup_printf ("COM%d", index);
//          }
//        else if (is_usb)
//          {
//            gint index = g_ascii_strtoll (tty_dev + 6, NULL, 10);
//            dev_name = g_strdup_printf ("USB COM%d", index);
//          }
//        else
//          {
//            continue;
//          }
//
//        rs232_dev = g_new (HyScanSonarSchemaRS232Dev, 1);
//        rs232_dev->dev_path = g_build_filename ("/dev", tty_dev, NULL);;
//        rs232_dev->id = id++;
//
//        g_hash_table_insert (rs232_devs, &rs232_dev->id, rs232_dev);
//        hyscan_data_schema_builder_enum_value_create (builder, "rs232-dev",
//                                                      rs232_dev->id,
//                                                      dev_name, NULL);
//      }
//
//    g_dir_close (dir);
//  }
//#endif



int
main (int    argc,
      char **argv)
{
  HyScanSonarSchema *schema;

  schema  = hyscan_sonar_schema_new ();

  hyscan_sonar_schema_sensor_add_uart_device (schema, "COM1");
  hyscan_sonar_schema_sensor_add_uart_device (schema, "COM2");
  hyscan_sonar_schema_sensor_add_uart_device (schema, "COM3");
  hyscan_sonar_schema_sensor_add_uart_device (schema, "COM4");

  hyscan_sonar_schema_sensor_add_uart_mode (schema, "4800/8N1");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "9600/8N1");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "19200/8N1");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "38400/8N1");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "57600/8N1");
  hyscan_sonar_schema_sensor_add_uart_mode (schema, "115200/8N1");

  hyscan_sonar_schema_sensor_add (schema, "virtual.1", HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_SAS);
  hyscan_sonar_schema_sensor_add (schema, "virtual.2", HYSCAN_SENSOR_PORT_VIRTUAL, HYSCAN_SENSOR_PROTOCOL_SAS);

  hyscan_sonar_schema_sensor_add (schema, "serial.1", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "serial.2", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "serial.3", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "serial.4", HYSCAN_SENSOR_PORT_UART, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

  hyscan_sonar_schema_sensor_add (schema, "ip.1", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "ip.2", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "ip.3", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);
  hyscan_sonar_schema_sensor_add (schema, "ip.4", HYSCAN_SENSOR_PORT_UDP_IP, HYSCAN_SENSOR_PROTOCOL_NMEA_0183);

  hyscan_sonar_schema_generator_add (schema, HYSCAN_BOARD_STARBOARD, 0xf, 0xf, 200000.0, 240000.0, 0.100);
  hyscan_sonar_schema_generator_add (schema, HYSCAN_BOARD_PORT, 0xf, 0xf, 250000.0, 290000.0, 0.100);
  hyscan_sonar_schema_generator_add (schema, HYSCAN_BOARD_ECHOSOUNDER, 0xf, 0xf, 150000.0, 200000.0, 0.100);

  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_STARBOARD, "Tone 50us");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_STARBOARD, "Tone 100us");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_STARBOARD, "LFM 3ms");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_STARBOARD, "LFM 6ms");

  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_PORT, "Tone 50us");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_PORT, "Tone 100us");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_PORT, "LFM 3ms");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_PORT, "LFM 6ms");

  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_ECHOSOUNDER, "Tone 50us");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_ECHOSOUNDER, "Tone 100us");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_ECHOSOUNDER, "LFM 3ms");
  hyscan_sonar_schema_generator_add_preset (schema, HYSCAN_BOARD_ECHOSOUNDER, "LFM 6ms");

  gchar *schema_data = hyscan_data_schema_builder_get_data (HYSCAN_DATA_SCHEMA_BUILDER (schema));
  g_print ("%s", schema_data);

  g_object_unref (schema);

  return 0;
}
