/*
 * Программа представляет собой простой сервер управления "гидролокатором". В качестве
 * "гидролокатора" используется класс HyScanSonarDummy. Сервер принимает в качестве
 * параметров IP адрес и UDP порт, по которому производится подключение клиента.
 *
 * Кроме этого можно задать целевую скорость отправки данных клиенту.
 *
 */

#include "hyscan-sonar-dummy.h"
#include "hyscan-sonar-server.h"

#include <libxml/parser.h>
#include <stdio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

int
main (int    argc,
      char **argv)
{
  HyScanSonarServer *server;
  HyScanSonarDummy *dummy;
  HyScanParam *sonar;

  gchar *sonar_address = NULL;
  gint sonar_port = 12345;
  gchar *target_speed = NULL;
  HyScanSonarServerTargetSpeed target_speed_id;

#ifdef G_OS_WIN32
  timeBeginPeriod (1);
#endif

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "sonar-address", 's', 0, G_OPTION_ARG_STRING, &sonar_address, "Sonar address", NULL },
        { "target-speed", 'e', 0, G_OPTION_ARG_STRING, &target_speed, "Target speed (local, 10M, 100M, 1G, 10G)", NULL },
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
        g_print ("%s\n", error->message);
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

  if ((target_speed == NULL) || (g_strcmp0 (target_speed, "local") == 0))
    target_speed_id = HYSCAN_SONAR_SERVER_TARGET_SPEED_LOCAL;
  else if (g_strcmp0 (target_speed, "10M") == 0)
    target_speed_id = HYSCAN_SONAR_SERVER_TARGET_SPEED_10M;
  else if (g_strcmp0 (target_speed, "100M") == 0)
    target_speed_id = HYSCAN_SONAR_SERVER_TARGET_SPEED_100M;
  else if (g_strcmp0 (target_speed, "1G") == 0)
    target_speed_id = HYSCAN_SONAR_SERVER_TARGET_SPEED_1G;
  else if (g_strcmp0 (target_speed, "10G") == 0)
    target_speed_id = HYSCAN_SONAR_SERVER_TARGET_SPEED_10G;
  else
    g_error ("Unknown target speed %s", target_speed);

  dummy = hyscan_sonar_dummy_new ();
  sonar = HYSCAN_PARAM (dummy);
  server = hyscan_sonar_server_new (sonar, sonar_address);

  if (!hyscan_sonar_server_set_target_speed (server, target_speed_id))
    g_error ("can't set target speed");

  if (!hyscan_sonar_server_start (server, HYSCAN_SONAR_SERVER_DEFAULT_TIMEOUT))
    g_error ("can't start sonar server");

  g_message ("Press [Enter] to terminate server...");
  getchar ();

  g_object_unref (server);
  g_object_unref (sonar);

  xmlCleanupParser ();

  return 0;
}
