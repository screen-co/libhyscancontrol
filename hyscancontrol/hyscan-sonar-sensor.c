#include "hyscan-sonar-sensor.h"

G_DEFINE_INTERFACE (HyScanSonarSensor, hyscan_sonar_sensor, G_TYPE_OBJECT)

static void
hyscan_sonar_sensor_default_init (HyScanSonarSensorInterface *iface)
{
}

HyScanSonarSensorPort **
hyscan_sonar_sensor_list_ports (HyScanSonarSensor *sensor)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), NULL);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->list_ports != NULL)
    return (* iface->list_ports) (sensor);

  return NULL;
}

HyScanDataSchemaEnumValue **
hyscan_sonar_sensor_list_ip_addresses (HyScanSonarSensor *sensor)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), NULL);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->list_ip_addresses != NULL)
    return (* iface->list_ip_addresses) (sensor);

  return NULL;
}

HyScanDataSchemaEnumValue **
hyscan_sonar_sensor_list_rs232_ports (HyScanSonarSensor *sensor)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), NULL);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->list_rs232_ports != NULL)
    return (* iface->list_rs232_ports) (sensor);

  return NULL;
}

HyScanDataSchemaEnumValue **
hyscan_sonar_sensor_list_rs232_speeds (HyScanSonarSensor *sensor)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), NULL);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->list_rs232_speeds != NULL)
    return (* iface->list_rs232_speeds) (sensor);

  return NULL;
}

HyScanSonarSensorPortStatus
hyscan_sonar_sensor_get_port_status (HyScanSonarSensor *sensor,
                                     gint               port_id)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), HYSCAN_SONAR_SENSOR_PORT_STATUS_INVALID);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->get_port_status != NULL)
    return (* iface->get_port_status) (sensor, port_id);

  return HYSCAN_SONAR_SENSOR_PORT_STATUS_INVALID;
}

gboolean
hyscan_sonar_sensor_set_virtual_port_param (HyScanSonarSensor        *sensor,
                                            gint                      port_id,
                                            HyScanSonarSensorChannel  channel)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->set_virtual_port_param != NULL)
    return (* iface->set_virtual_port_param) (sensor, port_id, channel);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_get_virtual_port_param (HyScanSonarSensor        *sensor,
                                            gint                      port_id,
                                            HyScanSonarSensorChannel *channel)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->get_virtual_port_param != NULL)
    return (* iface->get_virtual_port_param) (sensor, port_id, channel);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_set_ip_port_param (HyScanSonarSensor         *sensor,
                                       gint                       port_id,
                                       HyScanSonarSensorChannel   channel,
                                       HyScanSonarSensorProtocol  protocol,
                                       gint64                     ip_address,
                                       guint16                    udp_port)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->set_ip_port_param != NULL)
    return (* iface->set_ip_port_param) (sensor, port_id, channel, protocol, ip_address, udp_port);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_get_ip_port_param (HyScanSonarSensor         *sensor,
                                       gint                       port_id,
                                       HyScanSonarSensorChannel  *channel,
                                       HyScanSonarSensorProtocol *protocol,
                                       gint64                    *ip_address,
                                       guint16                   *udp_port)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->get_ip_port_param != NULL)
    return (* iface->get_ip_port_param) (sensor, port_id, channel, protocol, ip_address, udp_port);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_set_rs232_port_param (HyScanSonarSensor         *sensor,
                                          gint                       port_id,
                                          HyScanSonarSensorChannel   channel,
                                          HyScanSonarSensorProtocol  protocol,
                                          gint64                     rs232_port,
                                          gint64                     rs232_speed)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->set_rs232_port_param != NULL)
    return (* iface->set_rs232_port_param) (sensor, port_id, channel, protocol, rs232_port, rs232_speed);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_get_rs232_port_param (HyScanSonarSensor         *sensor,
                                          gint                       port_id,
                                          HyScanSonarSensorChannel  *channel,
                                          HyScanSonarSensorProtocol *protocol,
                                          gint64                    *rs232_port,
                                          gint64                    *rs232_speed)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->get_rs232_port_param != NULL)
    return (* iface->get_rs232_port_param) (sensor, port_id, channel, protocol, rs232_port, rs232_speed);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_set_enable (HyScanSonarSensor *sensor,
                                gint               port_id,
                                gboolean           enable)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->set_enable != NULL)
    return (* iface->set_enable) (sensor, port_id, enable);

  return FALSE;
}

gboolean
hyscan_sonar_sensor_get_enable (HyScanSonarSensor *sensor,
                                gint               port_id)
{
  HyScanSonarSensorInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_SENSOR (sensor), FALSE);

  iface = HYSCAN_SONAR_SENSOR_GET_IFACE (sensor);
  if (iface->get_enable != NULL)
    return (* iface->get_enable) (sensor, port_id);

  return FALSE;
}

void
hyscan_sonar_sensor_free_ports (HyScanSonarSensorPort **ports)
{
  gint i;

  for (i = 0; ports[i] != NULL; i++)
    {
      g_free (ports[i]->name);
      g_free (ports[i]);
    }

  g_free (ports);
}
