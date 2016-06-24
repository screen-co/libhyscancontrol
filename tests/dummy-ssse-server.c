/*
 * Программа представляет собой простой сервер управления "гидролокатором". В качестве
 * "гидролокатора" используется класс HyScanSonarDummySSSE. Сервер принимает в качестве
 * параметров IP адрес и UDP порт, по которому производится подключение клиента.
 *
 */

#include "hyscan-sonar-dummy-ssse.h"
#include "hyscan-sonar-server.h"

#include <stdio.h>

int
main (int    argc,
      char **argv)
{
  HyScanSonar *sonar;
  HyScanSonarServer *server;

  gchar *sonar_address = NULL;
  gint sonar_port = 12345;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "sonar-address", 's', 0, G_OPTION_ARG_STRING, &sonar_address, "Sonar IP address", NULL },
        { "sonar-port", 'p', 0, G_OPTION_ARG_INT, &sonar_port, "Sonar UDP port", NULL },
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

    if (sonar_address == NULL)
      {
        g_print ("%s", g_option_context_get_help (context, TRUE, NULL));
        return 0;
      }

    if (sonar_port <= 0 || sonar_port > 65535)
      {
        g_warning ("Sonar UDP port '%d' out of range", sonar_port);
        return -1;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  sonar = hyscan_sonar_dummy_ssse_new (100000.0, 200000.0, 300000.0, 20.0);
  server = hyscan_sonar_server_new (sonar, sonar_address, sonar_port);

  if (!hyscan_sonar_server_start (server))
    g_error ("can't start sonar server");

  g_message ("Press [Enter] to terminate server...");
  getchar ();

  g_object_unref (server);
  g_object_unref (sonar);

  return 0;
}
