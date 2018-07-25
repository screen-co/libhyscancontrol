#include <hyscan-nmea-udp.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

gint terminate = 0;

/* Обработчик сигналов TERM и INT. */
void shutdown_handler (gint signum)
{
  g_atomic_int_set (&terminate, 1);
}

void
data_cb (HyScanNmeaUDP *udp,
         gint64         time,
         const gchar   *name,
         guint          size,
         const gchar   *nmea,
         FILE          *fout)
{
  fprintf (fout, "%s", nmea);
  fflush (fout);
}

int
main (int    argc,
      char **argv)
{
  gboolean daemonize = FALSE;
  gboolean list = FALSE;
  gchar *host = NULL;
  gint port = 0;
  gchar *output = NULL;

  HyScanNmeaUDP *udp;
  FILE *fout;

  /* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "daemon", 'd', 0, G_OPTION_ARG_NONE, &daemonize, "Run as daemon", NULL },
        { "list", 'l', 0, G_OPTION_ARG_NONE, &list, "List available ip addresses", NULL },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &host, "Bind ip address", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &port, "Bind udp port", NULL },
        { "output", 'o', 0, G_OPTION_ARG_STRING, &output, "Output file", NULL },
        { NULL }
      };

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((!list) && ((host == NULL) || (port < 1024) || (port > 65535)))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
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

  if (daemonize)
    {
      struct sigaction term_signal;

      sigemptyset (&term_signal.sa_mask);
      term_signal.sa_handler = shutdown_handler;
      term_signal.sa_flags = SA_RESTART;

      if ((sigaction (SIGTERM, &term_signal, NULL) != 0) ||
          (sigaction (SIGINT, &term_signal, NULL) != 0))
        {
          g_message ("can't setup signals handler: %s", strerror (errno));
          return -1;
        }

      if (daemon (0, 0) != 0)
        {
          g_message ("can't daemonize: %s", strerror (errno));
          return -1;
        }
    }

  udp = hyscan_nmea_udp_new ("udp");
  hyscan_nmea_udp_set_address (udp, host, port);

  fout = fopen (output, "a");
  if (fout == NULL)
    {
      g_message ("can't create output file: %s", strerror (errno));
      return -1;
    }

  g_signal_connect (udp, "nmea-data", G_CALLBACK (data_cb), fout);

  if (daemonize)
    {
      while (g_atomic_int_get (&terminate) == 0)
        g_usleep (100000);
    }
  else
    {
      g_message ("Press [Enter] to terminate test...");
      getchar ();
    }

  g_object_unref (udp);
  fclose (fout);

  return 0;
}
