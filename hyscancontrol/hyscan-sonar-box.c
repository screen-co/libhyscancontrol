/*
 * \file hyscan-sonar-box.c
 *
 * \brief Исходный файл базового класса для реализации интерфейса HyScanSonar
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-box.h"

enum
{
  SIGNAL_DATA,
  SIGNAL_LAST
};

static void    hyscan_sonar_box_interface_init         (HyScanSonarInterface *iface);

static guint   hyscan_sonar_box_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanSonarBox, hyscan_sonar_box, HYSCAN_TYPE_DATA_BOX,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_sonar_box_interface_init))

static void
hyscan_sonar_box_class_init (HyScanSonarBoxClass *klass)
{
  hyscan_sonar_box_signals[SIGNAL_DATA] =
    g_signal_new ("data", HYSCAN_TYPE_SONAR_BOX, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
hyscan_sonar_box_init (HyScanSonarBox *sonar)
{
}

static gboolean
hyscan_sonar_box_set (HyScanSonar         *sonar,
                      const gchar *const  *names,
                      GVariant           **values)
{
  return hyscan_data_box_set (HYSCAN_DATA_BOX (sonar), names, values);
}

static gboolean
hyscan_sonar_box_get (HyScanSonar         *sonar,
                      const gchar *const  *names,
                      GVariant           **values)
{
  return hyscan_data_box_get (HYSCAN_DATA_BOX (sonar), names, values);
}

/* Функция создаёт новый объект HyScanSonarBox. */
HyScanSonarBox *
hyscan_sonar_box_new (const gchar *schema_data,
                      const gchar *schema_id)
{
  return g_object_new (HYSCAN_TYPE_SONAR_BOX,
                       "schema-data", schema_data,
                       "schema-id", schema_id,
                       NULL);
}

/* Функция передаёт данные. */
void
hyscan_sonar_box_send (HyScanSonarBox *sonar,
                       gint64          time,
                       guint32         id,
                       guint32         type,
                       gfloat          rate,
                       guint32         size,
                       gpointer        data)
{
  HyScanSonarMessage message;

  message.time = time;
  message.id = id;
  message.type = type;
  message.rate = rate;
  message.size = size;
  message.data = data;

  g_signal_emit (sonar, hyscan_sonar_box_signals[SIGNAL_DATA], 0, &message);
}

static void
hyscan_sonar_box_interface_init (HyScanSonarInterface *iface)
{
  iface->set = hyscan_sonar_box_set;
  iface->get = hyscan_sonar_box_get;
}
