#include "hyscan-sonar-writer.h"

G_DEFINE_INTERFACE (HyScanSonarWriter, hyscan_sonar_writer, G_TYPE_OBJECT)

static void
hyscan_sonar_writer_default_init (HyScanSonarWriterInterface *iface)
{
}

gboolean
hyscan_sonar_writer_start (HyScanSonarWriter *writer,
                           const gchar       *project_name,
                           const gchar       *track_name)
{
  HyScanSonarWriterInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_WRITER (writer), FALSE);

  iface = HYSCAN_SONAR_WRITER_GET_IFACE (writer);
  if (iface->start != NULL)
    return (* iface->start) (writer, project_name, track_name);

  return FALSE;
}

void
hyscan_sonar_writer_stop (HyScanSonarWriter *writer)
{
  HyScanSonarWriterInterface *iface;

  g_return_if_fail (HYSCAN_IS_SONAR_WRITER (writer));

  iface = HYSCAN_SONAR_WRITER_GET_IFACE (writer);
  if (iface->stop != NULL)
    (* iface->stop) (writer);
}

gboolean
hyscan_sonar_writer_set_chunk_size (HyScanSonarWriter *writer,
                                    gint32             chunk_size)
{
  HyScanSonarWriterInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_WRITER (writer), FALSE);

  iface = HYSCAN_SONAR_WRITER_GET_IFACE (writer);
  if (iface->set_chunk_size != NULL)
    return (* iface->set_chunk_size) (writer, chunk_size);

  return FALSE;
}

gboolean
hyscan_sonar_writer_set_save_time (HyScanSonarWriter *writer,
                                   gint64             save_time)
{
  HyScanSonarWriterInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_WRITER (writer), FALSE);

  iface = HYSCAN_SONAR_WRITER_GET_IFACE (writer);
  if (iface->set_save_time != NULL)
    return (* iface->set_save_time) (writer, save_time);

  return FALSE;
}

gboolean
hyscan_sonar_writer_set_save_size (HyScanSonarWriter *writer,
                                   gint64             save_size)
{
  HyScanSonarWriterInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_SONAR_WRITER (writer), FALSE);

  iface = HYSCAN_SONAR_WRITER_GET_IFACE (writer);
  if (iface->set_save_size != NULL)
    return (* iface->set_save_size) (writer, save_size);

  return FALSE;
}
