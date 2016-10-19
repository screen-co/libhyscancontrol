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
#include "hyscan-control-marshallers.h"
#include <hyscan-data-box.h>

enum
{
  SIGNAL_SET,
  SIGNAL_DATA,
  SIGNAL_LAST
};

struct _HyScanSonarBoxPrivate
{
  HyScanDataBox               *data;                   /* Параметры гидролокатора. */
};

static void     hyscan_sonar_box_interface_init        (HyScanSonarInterface  *iface);
static void     hyscan_sonar_box_object_finalize       (GObject               *object);

static gboolean hyscan_sonar_box_set_cb                (HyScanSonarBox        *sonar,
                                                        const gchar *const    *names,
                                                        GVariant             **values);

static gboolean hyscan_sonar_box_signal_accumulator    (GSignalInvocationHint *ihint,
                                                        GValue                *return_accu,
                                                        const GValue          *handler_return,
                                                        gpointer               data);

static guint   hyscan_sonar_box_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_CODE (HyScanSonarBox, hyscan_sonar_box, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanSonarBox)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_SONAR, hyscan_sonar_box_interface_init))

static void
hyscan_sonar_box_class_init (HyScanSonarBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->finalize = hyscan_sonar_box_object_finalize;

  hyscan_sonar_box_signals[SIGNAL_SET] =
    g_signal_new ("set", HYSCAN_TYPE_SONAR_BOX, G_SIGNAL_RUN_LAST, 0,
                  hyscan_sonar_box_signal_accumulator, NULL,
                  g_cclosure_user_marshal_BOOLEAN__POINTER_POINTER,
                  G_TYPE_BOOLEAN,
                  2, G_TYPE_POINTER, G_TYPE_POINTER);

  hyscan_sonar_box_signals[SIGNAL_DATA] =
    g_signal_new ("data", HYSCAN_TYPE_SONAR_BOX, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
hyscan_sonar_box_init (HyScanSonarBox *sonar)
{
  sonar->priv = hyscan_sonar_box_get_instance_private (sonar);
}

static void
hyscan_sonar_box_object_finalize (GObject *object)
{
  HyScanSonarBox *sonar = HYSCAN_SONAR_BOX (object);
  HyScanSonarBoxPrivate *priv = sonar->priv;

  g_clear_object (&priv->data);

  G_OBJECT_CLASS (hyscan_sonar_box_parent_class)->finalize (object);
}

/* Функция вызывается при установке параметров гидролокатора и
 * посылает сигнал "set". */
static gboolean
hyscan_sonar_box_set_cb (HyScanSonarBox  *sonar,
                         const gchar     *const *names,
                         GVariant       **values)
{
  gboolean cancel = FALSE;

  g_signal_emit (sonar, hyscan_sonar_box_signals[SIGNAL_SET], 0, names, values, &cancel);
  if (cancel)
    return FALSE;

  return TRUE;
}

/* Функция аккумулирует ответы всех callback'ов сигнала set.
 * Здесь действует обратная логика, если какой-либо из callback'ов
 * вернёт FALSE, аккумулятор вернёт TRUE. Это будет сигналом
 * прекратить обработку запроса установки параметров. */
static gboolean
hyscan_sonar_box_signal_accumulator (GSignalInvocationHint *ihint,
                                     GValue                *return_accu,
                                     const GValue          *handler_return,
                                     gpointer               data)
{
  if (!g_value_get_boolean (handler_return))
    {
      g_value_set_boolean (return_accu, TRUE);
      return FALSE;
    }

  return TRUE;
}

static HyScanDataSchema *
hyscan_sonar_box_schema (HyScanSonar *sonar)
{
  HyScanSonarBox *sonar_box = HYSCAN_SONAR_BOX (sonar);
  HyScanSonarBoxPrivate *priv = sonar_box->priv;

  if (priv->data == NULL)
    return NULL;

  return g_object_ref (priv->data);
}

static gboolean
hyscan_sonar_box_set (HyScanSonar         *sonar,
                      const gchar *const  *names,
                      GVariant           **values)
{
  HyScanSonarBox *sonar_box = HYSCAN_SONAR_BOX (sonar);
  HyScanSonarBoxPrivate *priv = sonar_box->priv;

  if (priv->data == NULL)
    return FALSE;

  return hyscan_data_box_set (priv->data, names, values);
}

static gboolean
hyscan_sonar_box_get (HyScanSonar         *sonar,
                      const gchar *const  *names,
                      GVariant           **values)
{
  HyScanSonarBox *sonar_box = HYSCAN_SONAR_BOX (sonar);
  HyScanSonarBoxPrivate *priv = sonar_box->priv;

  if (priv->data == NULL)
    return FALSE;

  return hyscan_data_box_get (priv->data, names, values);
}

/* Функция создаёт новый объект HyScanSonarBox. */
HyScanSonarBox *
hyscan_sonar_box_new (void)
{
  return g_object_new (HYSCAN_TYPE_SONAR_BOX, NULL);
}

/* Функция задаёт схему параметров гидролокатора. */
void
hyscan_sonar_box_set_schema (HyScanSonarBox  *sonar,
                             const gchar     *schema_data,
                             const gchar     *schema_id)
{
  g_return_if_fail (HYSCAN_IS_SONAR_BOX (sonar));

  g_clear_object (&sonar->priv->data);

  sonar->priv->data = hyscan_data_box_new_from_string (schema_data, schema_id);
  g_signal_connect_swapped (sonar->priv->data, "set", G_CALLBACK (hyscan_sonar_box_set_cb), sonar);
}

/* Функция передаёт данные. */
void
hyscan_sonar_box_send (HyScanSonarBox     *sonar,
                       HyScanSonarMessage *message)
{
  g_return_if_fail (HYSCAN_IS_SONAR_BOX (sonar));

  g_signal_emit (sonar, hyscan_sonar_box_signals[SIGNAL_DATA], 0, message);
}

static void
hyscan_sonar_box_interface_init (HyScanSonarInterface *iface)
{
  iface->get_schema = hyscan_sonar_box_schema;
  iface->set = hyscan_sonar_box_set;
  iface->get = hyscan_sonar_box_get;
}
