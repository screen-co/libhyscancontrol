#include <hyscan-nmea-uart.h>
#include <stdio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

void
data_cb (HyScanNmeaUART *uart,
         gint64          time,
         const gchar    *name,
         guint           size,
         const gchar    *nmea,
         gpointer        user_data)
{
  g_print ("%s time: %" G_GINT64_FORMAT ", size %d\n%s\n", name, time, size, nmea);
}

int
main (int    argc,
      char **argv)
{
  HyScanNmeaUARTDevice **devices;
  HyScanNmeaUART **uarts;
  guint n_uarts;
  guint i;

#ifdef G_OS_WIN32
  timeBeginPeriod (1);
#endif

  devices = hyscan_nmea_uart_list_devices ();

  for (n_uarts = 0; devices != NULL && devices[n_uarts] != NULL; n_uarts++);
  uarts = g_malloc (n_uarts * sizeof (HyScanNmeaUART*));

  for (i = 0; i < n_uarts; i++)
    {
      g_message ("%s: %s", devices[i]->name, devices[i]->path);

      uarts[i] = hyscan_nmea_uart_new (devices[i]->name);
      g_signal_connect (uarts[i], "nmea-data", G_CALLBACK (data_cb), NULL);
      hyscan_nmea_uart_set_device (uarts[i], devices[i]->path, HYSCAN_NMEA_UART_MODE_AUTO);
    }

  g_message ("Press [Enter] to terminate test...");
  getchar ();

  for (i = 0; i < n_uarts; i++)
    g_object_unref (uarts[i]);

  hyscan_nmea_uart_devices_free (devices);
  g_free (uarts);

  return 0;
}
