#include <hyscan-nmea-udp.h>
#include <stdio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

void
data_cb (HyScanNmeaUDP *udp,
         gint64         time,
         const gchar   *name,
         guint          size,
         const gchar   *nmea,
         gpointer       user_data)
{
  g_print ("%s time: %" G_GINT64_FORMAT ", size %d\n%s\n", name, time, size, nmea);
}

int
main (int    argc,
      char **argv)
{
  HyScanNmeaUDP *udp;
  gboolean list = FALSE;
  gchar *host = NULL;
  gint port = 0;

#ifdef G_OS_WIN32
  WSADATA wsa_data;
  WSAStartup (MAKEWORD (2, 0), &wsa_data);
#endif

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        {"list", 'l', 0, G_OPTION_ARG_NONE, &list, "List available ip addresses", NULL},
        {"host", 'h', 0, G_OPTION_ARG_STRING, &host, "Bind ip address", NULL},
        {"port", 'p', 0, G_OPTION_ARG_INT, &port, "Bind udp port", NULL},
        {NULL}
      };

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

    if ((!list) && ((host == NULL) || (port < 1024) || (port > 65535)))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  if (list)
    {
      gchar **addresses;
      guint i;

      addresses = hyscan_nmea_udp_list_addresses ();
      g_print ("Local ip addresses: \n");

      for (i = 0; addresses != NULL && addresses[i] != NULL; i++)
        g_print ("  %s\n", addresses[i]);

      g_strfreev (addresses);

      return 0;
    }

#ifdef G_OS_WIN32
  timeBeginPeriod (1);
#endif

  udp = hyscan_nmea_udp_new ("udp");
  hyscan_nmea_udp_set_address (udp, host, port);
  g_signal_connect (udp, "nmea-data", G_CALLBACK (data_cb), NULL);

  g_message ("Press [Enter] to terminate test...");
  getchar ();

  g_object_unref (udp);

  return 0;
}
