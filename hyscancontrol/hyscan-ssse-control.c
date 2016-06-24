
#include "hyscan-ssse-control.h"
#include "hyscan-marshallers.h"

enum
{
  PROP_O,
  PROP_SONAR
};

enum
{
  SIGNAL_DATA,
  SIGNAL_LAST
};

struct _HyScanSSSEControlPrivate
{
  HyScanSonar                 *sonar;                          /* Интерфейс управления гидролокатором. */




};

static void            hyscan_ssse_control_set_property        (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void            hyscan_ssse_control_object_constructed  (GObject               *object);
static void            hyscan_ssse_control_object_finalize     (GObject               *object);

static guint           hyscan_ssse_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSSSEControl, hyscan_ssse_control, HYSCAN_TYPE_SENSOR_CONTROL)

static void
hyscan_ssse_control_class_init (HyScanSSSEControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_ssse_control_set_property;

  object_class->constructed = hyscan_ssse_control_object_constructed;
  object_class->finalize = hyscan_ssse_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_SONAR,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_ssse_control_signals[SIGNAL_DATA] =
    g_signal_new ("sensor-data", HYSCAN_TYPE_SENSOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__POINTER_POINTER,
                  G_TYPE_NONE,
                  2, G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
hyscan_ssse_control_init (HyScanSSSEControl *ssse_control)
{
  ssse_control->priv = hyscan_ssse_control_get_instance_private (ssse_control);
}

static void
hyscan_ssse_control_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanSSSEControl *ssse_control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = ssse_control->priv;

  switch (prop_id)
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_ssse_control_object_constructed (GObject *object)
{
  HyScanSSSEControl *ssse_control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = ssse_control->priv;

  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->constructed (object);




}

static void
hyscan_ssse_control_object_finalize (GObject *object)
{
  HyScanSSSEControl *ssse_control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = ssse_control->priv;


  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->finalize (object);
}

void
hyscan_ssse_control_set_a (HyScanSSSEControl *ssse_control,
                      gint           a)
{
  HyScanSSSEControlPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SSSE_CONTROL (ssse_control));

  priv = ssse_control->priv;

}
