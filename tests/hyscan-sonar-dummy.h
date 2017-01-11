/*
 * Данный класс представляет примитивную реализацию драйвера "гидролокатора".
 * Он используется для проверки правильности работы клиент/серверной реализации управления
 * гидролокатором.
 *
 * Схема данных содержит следующие параметры, управляющие поведением класса:
 *
 * - /enable - признак включения имитатора данных.
 * - /data/sources - число источников данных;
 * - /data/period - период выдачи сообщений HyScanSonarMsgData, секунды;
 * - /data/size - размер сообщений HyScanSonarMsgData, uint32 числа;
 * - /alive - подтверждение активности.
 *
 * Включение "гидролокатора" должно осуществляться только после установки всех параметров.
 * Параметры /data/ * должны устанавливаться одновременно.
 *
 */

#ifndef __HYSCAN_SONAR_DUMMY_H__
#define __HYSCAN_SONAR_DUMMY_H__

#include <hyscan-param.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_SONAR_DUMMY             (hyscan_sonar_dummy_get_type ())
#define HYSCAN_SONAR_DUMMY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_DUMMY, HyScanSonarDummy))
#define HYSCAN_IS_SONAR_DUMMY(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_DUMMY))
#define HYSCAN_SONAR_DUMMY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_SONAR_DUMMY, HyScanSonarDummyClass))
#define HYSCAN_IS_SONAR_DUMMY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_SONAR_DUMMY))
#define HYSCAN_SONAR_DUMMY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_SONAR_DUMMY, HyScanSonarDummyClass))

typedef struct _HyScanSonarDummy HyScanSonarDummy;
typedef struct _HyScanSonarDummyPrivate HyScanSonarDummyPrivate;
typedef struct _HyScanSonarDummyClass HyScanSonarDummyClass;

struct _HyScanSonarDummy
{
  GObject parent_instance;

  HyScanSonarDummyPrivate *priv;
};

struct _HyScanSonarDummyClass
{
  GObjectClass parent_class;
};

GType                  hyscan_sonar_dummy_get_type     (void);

/* Функция создаёт объект HyScanSonarDummy. */
HyScanSonarDummy      *hyscan_sonar_dummy_new          (void);

G_END_DECLS

#endif /* __HYSCAN_SONAR_DUMMY_H__ */
