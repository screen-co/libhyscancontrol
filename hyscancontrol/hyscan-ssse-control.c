#include "hyscan-ssse-control.h"

enum
{
  PROP_O,
  PROP_A
};

struct _HyScanSSSEControlPrivate
{
  gint                         prop_a;
};

static void    hyscan_ssse_control_set_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_ssse_control_get_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        GValue                *value,
                                                        GParamSpec            *pspec);
static void    hyscan_ssse_control_object_constructed       (GObject               *object);
static void    hyscan_ssse_control_object_finalize          (GObject               *object);

/* !!! Change G_TYPE_OBJECT to type of the base class. !!! */
G_DEFINE_TYPE_WITH_PRIVATE (HyScanSSSEControl, hyscan_ssse_control, G_TYPE_OBJECT)

static void
hyscan_ssse_control_class_init (HyScanSSSEControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_ssse_control_set_property;
  object_class->get_property = hyscan_ssse_control_get_property;

  object_class->constructed = hyscan_ssse_control_object_constructed;
  object_class->finalize = hyscan_ssse_control_object_finalize;

  g_object_class_install_property (object_class, PROP_A,
    g_param_spec_int ("param-a", "ParamA", "Parameter A", G_MININT32, G_MAXINT32, 0,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
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
    case PROP_A:
      priv->prop_a = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_ssse_control_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  HyScanSSSEControl *ssse_control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = ssse_control->priv;

  switch ( prop_id )
    {
    case PROP_A:
      g_value_set_int (value, priv->prop_a);
      break;

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

  /* Remove this call then class is derived from GObject.
     This call is strongly needed then class is derived from GtkWidget. */
  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->constructed (object);

  priv->prop_a = 1;
}

static void
hyscan_ssse_control_object_finalize (GObject *object)
{
  HyScanSSSEControl *ssse_control = HYSCAN_SSSE_CONTROL (object);
  HyScanSSSEControlPrivate *priv = ssse_control->priv;

  priv->prop_a = 0;

  G_OBJECT_CLASS (hyscan_ssse_control_parent_class)->finalize (object);
}

void
hyscan_ssse_control_set_a (HyScanSSSEControl *ssse_control,
                      gint           a)
{
  HyScanSSSEControlPrivate *priv;

  g_return_if_fail (HYSCAN_IS_SSSE_CONTROL (ssse_control));

  priv = ssse_control->priv;

  priv->prop_a = a;
}

gint
hyscan_ssse_control_get_a (HyScanSSSEControl *ssse_control)
{
  g_return_val_if_fail (HYSCAN_IS_SSSE_CONTROL (ssse_control), -1);

  return ssse_control->priv->prop_a;
}
