/*
 * Программа используется для проверки правильности работы клиент/серверной реализации управления
 * гидролокатором. Тестирование производится с использованием класса HyScanSonarDummy в локальном
 * режиме или через сетевой интерфейс. Во втором случае необходимо задействовать dummy-sonar-server
 * в качестве удалённого "гидролокатора".
 *
 * Выбор локального или сетевого режимов работы производится на основании установки параметров
 * IP адреса и UDP порта "гидролокатора". Если они заданы, используется сетевой режим, иначе - локальный.
 *
 * Во время работы проверяется правильность данных в сообщениях от "гидролокатора" и измерение
 * скорости передачи данных.
 *
 */

#include "hyscan-sonar-dummy.h"
#include "hyscan-sonar-client.h"
#include "hyscan-sonar-messages.h"

#include <libxml/parser.h>
#include <string.h>

#define MSG_DATA_MAX_SOURCES 16

guint cur_size = 0;
gdouble total_size = 0.0;

guint sources = 4;
gdouble data_msg_rate = 25.0;
gint data_size = 4000;

guint32 next_indexes[MSG_DATA_MAX_SOURCES];

gboolean set_data_params (HyScanParam *sonar,
                          gint         sources,
                          gdouble      period,
                          gint         size)
{
  const gchar *names[4];
  GVariant *values[4];

  names[0] = "/data/sources";
  names[1] = "/data/period";
  names[2] = "/data/size";
  names[3] = NULL;

  values[0] = g_variant_new_int64 (sources);
  values[1] = g_variant_new_double (period);
  values[2] = g_variant_new_int64 (size);

  if (hyscan_param_set (sonar, names, values))
    return TRUE;

  g_variant_unref (values[0]);
  g_variant_unref (values[1]);
  g_variant_unref (values[2]);

  return FALSE;
}

void
message_check (HyScanParam        *sonar,
               HyScanSonarMessage *message)
{
  guint n_points;
  const guint32 *points;
  guint i;

  if (message->id == 0 || message->id > sources)
    {
      g_warning ("unknown source id %d", message->id);
      return;
    }

  points = message->data;
  n_points = message->size / sizeof (guint32);

  i = message->id - 1;
  if (next_indexes[i] != 0)
    if (next_indexes[i] != points[0])
      g_warning ("source %d: index mismatch: got %d, need %d", message->id, points[0], next_indexes[i]);

  if (points[0] > 0)
    next_indexes[i] = points[0] + 1;
  else
    next_indexes[i]++;

  for (i = 1; i < n_points; i++)
    if (points[i] != (points[0] + (message->id - 1) * data_size + i))
      {
        g_warning ("source %d: data mismatch at %d: got %d, need %d", message->id, i, points[i], points[0] + message->id * data_size + i);
        break;
      }

  g_atomic_int_add (&cur_size, message->size);
}

int
main (int    argc,
      char **argv)
{
  HyScanSonarDummy *dummy;
  HyScanSonarClient *client;
  HyScanParam *sonar;

  gchar *sonar_address = NULL;
  gint sonar_port = 12345;

  GTimer *stimer;
  GTimer *dtimer;
  gdouble duration = 10.0;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "sonar-address", 's', 0, G_OPTION_ARG_STRING, &sonar_address, "Sonar IP address", NULL },
        { "sonar-port", 'p', 0, G_OPTION_ARG_INT, &sonar_port, "Sonar UDP port", NULL },
        { "duration", 't', 0, G_OPTION_ARG_DOUBLE, &duration, "Test duration, s", NULL },
        { "sources", 'n', 0, G_OPTION_ARG_INT, &sources, "Number of sources", NULL },
        { "data-rate", 'r', 0, G_OPTION_ARG_DOUBLE, &data_msg_rate, "Data message rate, msg/s (0 - 1000)", NULL },
        { "data-size", 'd', 0, G_OPTION_ARG_INT, &data_size, "Data size, points", NULL },
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

    if (sonar_port <= 0 || sonar_port > 65535)
      {
        g_warning ("Sonar UDP port '%d' out of range", sonar_port);
        return -1;
      }

    if (data_msg_rate <= 0 || data_msg_rate > 1000 )
      {
        g_warning ("Data message rate '%.3lf' out of range", data_msg_rate);
        return -1;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  dtimer = g_timer_new ();
  stimer = g_timer_new ();

  /* Локальный тест. */
  if (sonar_address == NULL)
    {
      dummy = hyscan_sonar_dummy_new ();

      sonar = HYSCAN_PARAM (dummy);
    }

  /* Сетевой тест. */
  else
    {
      client = hyscan_sonar_client_new (sonar_address, sonar_port);
      if (!hyscan_sonar_client_set_master (client))
        g_error ("can't setup master connection");

      sonar = HYSCAN_PARAM (client);
    }

  g_signal_connect (sonar, "data", G_CALLBACK (message_check), NULL);

  if (!set_data_params (sonar, sources, 1.0 / data_msg_rate, data_size))
    g_error ("can't set info data params");

  if (!hyscan_param_set_boolean (sonar, "/enable", TRUE))
    g_error ("can't enable sonar");

  while (g_timer_elapsed (dtimer, NULL) < duration)
    {
      gdouble view_time;
      gint cur_size_view;

      view_time = g_timer_elapsed (stimer, NULL);
      if (view_time >= 0.5)
        {
          cur_size_view = g_atomic_int_and (&cur_size, 0);
          total_size += cur_size_view;

          cur_size_view /= (1024.0 * view_time);
          g_message ("current data speed: %dKb/s", cur_size_view);

          g_timer_start (stimer);
        }

      if (!hyscan_param_set_boolean (sonar, "/alive", FALSE))
        g_error ("can't cheer up sonar");

      g_usleep (100000);
    }

  if (!hyscan_param_set_boolean (sonar, "/enable", FALSE))
    g_error ("can't disable sonar");

  total_size += cur_size;
  total_size /= (1024 * duration);
  g_message ("mean data speed: %.0lfKb/s", total_size);

  g_object_unref (sonar);

  g_timer_destroy (stimer);
  g_timer_destroy (dtimer);

  xmlCleanupParser ();

  return 0;
}
