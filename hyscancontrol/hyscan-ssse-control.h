#ifndef __HYSCAN_SSSE_CONTROL_H__
#define __HYSCAN_SSSE_CONTROL_H__

#include <hyscan-sensor-control.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SSSE_CONTROL             (hyscan_ssse_control_get_type ())
#define HYSCAN_SSSE_CONTROL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SSSE_CONTROL, HyScanSSSEControl))
#define HYSCAN_IS_SSSE_CONTROL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SSSE_CONTROL))
#define HYSCAN_SSSE_CONTROL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SSSE_CONTROL, HyScanSSSEControlClass))
#define HYSCAN_IS_SSSE_CONTROL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SSSE_CONTROL))
#define HYSCAN_SSSE_CONTROL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SSSE_CONTROL, HyScanSSSEControlClass))

typedef struct _HyScanSSSEControl HyScanSSSEControl;
typedef struct _HyScanSSSEControlPrivate HyScanSSSEControlPrivate;
typedef struct _HyScanSSSEControlClass HyScanSSSEControlClass;

struct _HyScanSSSEControl
{
  HyScanSensorControl parent_instance;

  HyScanSSSEControlPrivate *priv;
};

struct _HyScanSSSEControlClass
{
  HyScanSensorControlClass parent_class;
};

GType                  hyscan_ssse_control_get_type         (void);

void                   hyscan_ssse_control_set_a            (HyScanSSSEControl *ssse_control,
                                                        gint           a);

gint                   hyscan_ssse_control_get_a            (HyScanSSSEControl *ssse_control);

G_END_DECLS

#endif /* __HYSCAN_SSSE_CONTROL_H__ */
