/*
 * Данный класс представляет реализацию интерфейса HyScanSonar для "примитивного гидролокатора
 * бокового обзора". Он используется для проверки возможностей классов HyScanSensor и HyScanSSSE.
 *
 * Схема данных класса определена в ресурсе "/org/hyscan/schemas/hyscan-sonar-dummy-schema.xml",
 * который в свою очередь находится в файле hyscan-sonar-dummy-schema.xml каталога tests.
 *
 */

#ifndef __HYSCAN_SONAR_DUMMY_SSSE_H__
#define __HYSCAN_SONAR_DUMMY_SSSE_H__

#include <hyscan-sonar.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_DUMMY_SSSE             (hyscan_sonar_dummy_ssse_get_type ())
#define HYSCAN_SONAR_DUMMY_SSSE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_DUMMY_SSSE, HyScanSonarDummySSSE))
#define HYSCAN_IS_SONAR_DUMMY_SSSE(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_DUMMY_SSSE))
#define HYSCAN_SONAR_DUMMY_SSSE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_DUMMY_SSSE, HyScanSonarDummySSSEClass))
#define HYSCAN_IS_SONAR_DUMMY_SSSE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_DUMMY_SSSE))
#define HYSCAN_SONAR_DUMMY_SSSE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_DUMMY_SSSE, HyScanSonarDummySSSEClass))

typedef struct _HyScanSonarDummySSSE HyScanSonarDummySSSE;
typedef struct _HyScanSonarDummySSSEPrivate HyScanSonarDummySSSEPrivate;
typedef struct _HyScanSonarDummySSSEClass HyScanSonarDummySSSEClass;

struct _HyScanSonarDummySSSE
{
  GObject parent_instance;

  HyScanSonarDummySSSEPrivate *priv;
};

struct _HyScanSonarDummySSSEClass
{
  GObjectClass parent_class;
};

GType                  hyscan_sonar_dummy_ssse_get_type     (void);

HyScanSonar           *hyscan_sonar_dummy_ssse_new          (void);

G_END_DECLS

#endif /* __HYSCAN_SONAR_DUMMY_SSSE_H__ */
