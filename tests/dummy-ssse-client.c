#include <hyscan-db.h>
#include <hyscan-sonar.h>
#include <hyscan-sensor-control.h>

#include "hyscan-sonar-dummy-ssse.h"

int
main (int    argc,
      char **argv)
{
  HyScanDB *db;
  HyScanSonar *sonar;

  HyScanWriteControl *writer;
  HyScanSensorControl *sensor;

  HyScanSensorPort **ports;

  gint i;

  db = hyscan_db_new ("file://db");
  sonar = hyscan_sonar_dummy_ssse_new ();

  sensor = hyscan_sensor_control_new (sonar, db);
  writer = HYSCAN_WRITE_CONTROL (sensor);

  hyscan_sonar_lock (sonar, NULL, 0);

  ports = hyscan_sensor_control_list_ports (sensor);
  for (i = 0; ports[i] != NULL; i++)
    {
      g_message ("port%d: %s", ports[i]->id, ports[i]->name);
      hyscan_sensor_control_set_enable (sensor, ports[i]->id, TRUE);
    }

  hyscan_db_project_create (db, "test", NULL);

  hyscan_sensor_control_start (sensor, "test", "test");
  g_usleep (5000000);
  hyscan_sensor_control_start (sensor, "test", "test2");
  g_usleep (5000000);
  hyscan_sensor_control_stop (sensor);

  g_usleep (2000000);

  g_object_unref (sensor);
  g_object_unref (sonar);
  g_object_unref (db);

  return 0;
}
