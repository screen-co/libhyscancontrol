/*
 * \file hyscan-sonar-control-async.c
 *
 * \brief Исходный файл класса управления гидролокатором
 * \author Vladimir Maximov (vmakxs@gmail.com)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 */

#include "hyscan-sonar-control-async.h"
#include "hyscan-control-marshallers.h"

#define HYSCAN_ASYNC_IDLE                       (0)
#define HYSCAN_ASYNC_BUSY                       (1)

#define HYSCAN_ASYNC_CONTINUE                   (0)
#define HYSCAN_ASYNC_SHUTDOWN                   (1)

#define HYSCAN_ASYNC_THREAD_NAME                "sonar-async-thread"

#define HYSCAN_ASYNC_COND_WAIT_TIMEOUT          (100 * G_TIME_SPAN_MILLISECOND)
#define HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS    (100)

enum
{
  SIGNAL_STARTED,
  SIGNAL_COMPLETED,
  SIGNAL_LAST
};

/* Идентификаторы сигналов. */
guint hyscan_sonar_control_async_signals[SIGNAL_LAST] = { 0 };

/* Параметры запроса установки режима синхронизации. */
typedef struct
{
  HyScanSonarSyncType        sync_type;     /* Режим синхронизации. */
} HyScanSonarSetSyncTypeParams;

/* Параметры запроса установки местоположения антенн ГЛ. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanAntennaPosition      position;      /* Местоположение. */
} HyScanSonarSetPositionParams;

/* Параметры запроса установки времени приёма. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    receive_time;  /* Время приёма данных. */
} HyScanSonarSetReceiveTimeParams;

/* Параметры запроса запуска гидролокатора. */
typedef struct
{
  gchar                     *track_name;    /* Имя галса. */
  HyScanTrackType            track_type;    /* Тип галса. */
} HyScanSonarStartParams;

/* Параметры запроса установки автоматического режима ВАРУ. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    level;         /* Целевой уровень сигнала. */
  gdouble                    sensitivity;   /* Чувствительность автомата регулировки. */
} HyScanTvgSetAutoParams;

/* Параметры запроса установки постоянного уровня усиления. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    gain;          /* Усиление. */
} HyScanTvgSetConstantParams;

/* Параметры запроса установки линейного увеличения усиления. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    gain0;         /* Начальный уровень усиления. */
  gdouble                    step;          /* Величина изменения усиления каждые 100 метров. */
} HyScanTvgSetLinearDbParams;


/* Параметры запроса установки логарифмического закона изменения усиления. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gdouble                    gain0;         /* начальный уровень усиления. */
  gdouble                    beta;          /* Коэффициент отражения цели. */
  gdouble                    alpha;         /* Коэффициент затухания. */
} HyScanTvgSetLogarithmicParams;

/* Параметры запроса включения/выключения системы ВАРУ. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gboolean                   enable;        /* Включена или выключена. */
} HyScanTvgSetEnableParams;

/* Параметры запроса установки режима работы генератора по преднастройкам. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  guint                      preset;        /* Идентификатор преднастройки. */
} HyScanGenSetPresetParams;

/* Параметры запроса установки автоматического режима генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanGeneratorSignalType  signal;        /* Тип сигнала. */
} HyScanGenSetAutoParams;

/* Параметры запроса установки упрощённого режима генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanGeneratorSignalType  signal;        /* Тип сигнала. */
  gdouble                    power;         /* Мощность. */
} HyScanGenSetSimpleParams;

/* Параметры запроса установки расширенного режима генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  HyScanGeneratorSignalType  signal;        /* Тип сигнала. */
  gdouble                    duration;      /* Длительность. */
  gdouble                    power;         /* Мощность. */
} HyScanGenSetExtendedParams;

 /* Параметры запроса включения/выключения генератора. */
typedef struct
{
  HyScanSourceType           source;        /* Источник данных. */
  gboolean                   enable;        /* Включён или выключен. */
} HyScanGenSetEnableParams;

/* Параметры запроса установки режима работы виртуального порта. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  guint                      channel;       /* Канал. */
  gint64                     time_offset;   /* Коррекция времени приёма данных. */
} HyScanSensorSetVirtualPortParamParams;

/* Параметры запроса установки режима работы UART порта. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  guint                      channel;       /* Канал. */
  gint64                     time_offset;   /* Коррекция времени приёма данных. */
  HyScanSensorProtocolType   protocol;      /* Протокол обмена данными с датчиком. */
  guint                      uart_device;   /* Идентификатор устройства. */
  guint                      uart_mode;     /* Идентификатор режима работы. */
} HyScanSensorSetUartPortParamParams;

/* Параметры запроса установки режима работы UDP/IP порта. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  guint                      channel;       /* Канал. */
  gint64                     time_offset;   /* Коррекция времени приёма данных. */
  HyScanSensorProtocolType   protocol;      /* Протокол обмена данными с датчиком. */
  guint                      ip_address;    /* IP-адрес датчика. */
  guint16                    udp_port;      /* Номер порта датчика. */
} HyScanSensorSetUdpIpPortParamParams;

/* Параметры запроса установки местоположения датчика. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  HyScanAntennaPosition      position;      /* Местоположение датчика. */
} HyScanSensorSetPositionParams;

/* Параметры запроса включения/выключения датчика. */
typedef struct
{
  gchar                     *name;          /* Название датчика. */
  gboolean                   enable;        /* Включён или выключен. */
} HyScanSensorSetEnableParams;

/* Параметры запросов. */
typedef struct
{
  HyScanSonarSetSyncTypeParams          sonar_set_sync_type_params;             /* Параметры запроса установки режима синхронизации. */
  HyScanSonarSetPositionParams          sonar_set_position_params;              /* Параметры запроса установки местоположения антенн ГЛ. */
  HyScanSonarSetReceiveTimeParams       sonar_set_receive_type_params;          /* Параметры запроса установки времени приёма. */
  HyScanSonarStartParams                sonar_start_params;                     /* Параметры запроса запуска гидролокатора. */

  HyScanTvgSetAutoParams                tvg_set_auto_params;                    /* Параметры запроса установки автоматического режима ВАРУ. */
  HyScanTvgSetConstantParams            tvg_set_constant_params;                /* Параметры запроса установки постоянного уровня усиления. */
  HyScanTvgSetLinearDbParams            tvg_set_linear_db_params;               /* Параметры запроса установки линейного увеличения усиления. */
  HyScanTvgSetLogarithmicParams         tvg_set_logarithmic_params;             /* Параметры запроса установки логарифмического закона изменения усиления. */
  HyScanTvgSetEnableParams              tvg_set_enable_params;                  /* Параметры запроса включения/выключения системы ВАРУ. */

  HyScanGenSetPresetParams              gen_set_preset_params;                  /* Параметры запроса установки режима работы генератора по преднастройкам. */
  HyScanGenSetAutoParams                gen_set_auto_params;                    /* Параметры запроса установки автоматического режима генератора. */
  HyScanGenSetSimpleParams              gen_set_simple_params;                  /* Параметры запроса установки упрощённого режима генератора. */
  HyScanGenSetExtendedParams            gen_set_extended_params;                /* Параметры запроса установки расширенного режима генератора. */
  HyScanGenSetEnableParams              gen_set_enable_params;                  /* Параметры запроса включения/выключения генератора. */

  HyScanSensorSetVirtualPortParamParams sensor_set_virtual_port_param_params;   /* Параметры запроса установки режима работы виртуального порта. */
  HyScanSensorSetUartPortParamParams    sensor_set_uart_port_param_params;      /* Параметры запроса установки режима работы UART порта. */
  HyScanSensorSetUdpIpPortParamParams   sensor_set_udp_ip_port_param_params;    /* Параметры запроса установки режима работы UDP/IP порта. */
  HyScanSensorSetPositionParams         sensor_set_position_params;             /* Параметры запроса установки местоположения датчика. */
  HyScanSensorSetEnableParams           sensor_set_enable_params;               /* Параметры запроса включения/выключения датчика. */
} SonarControlQueryParams;

/* Информация о запросе. */
typedef struct
{
  gint           id;    /* Идентификатор запроса. */
  const gchar   *name;  /* Имя запроса. */
} SonarControlAsyncQueryInfo;

static SonarControlAsyncQueryInfo hyscan_sonar_control_async_query_info[] =
  {
    { HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_SYNC_TYPE,           "sonar-set-sync-type"           },
    { HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_POSITION,            "sonar-set-position"            },
    { HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_RECEIVE_TIME,        "sonar-set-receive-time"        },
    { HYSCAN_SONAR_CONTROL_ASYNC_SONAR_START,                   "sonar-start"                   },
    { HYSCAN_SONAR_CONTROL_ASYNC_SONAR_STOP,                    "sonar-stop"                    },
    { HYSCAN_SONAR_CONTROL_ASYNC_SONAR_PING,                    "sonar-ping"                    },
    { HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_AUTO,                  "tvg-set-auto"                  },
    { HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_CONSTANT,              "tvg-set-constant"              },
    { HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LINEAR_DB,             "tvg-set-linear_db"             },
    { HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LOGARITHMIC,           "tvg-set-logarithmic"           },
    { HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_ENABLE,                "tvg-set-enable"                },
    { HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_PRESET,          "generator-set-preset"          },
    { HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_AUTO,            "generator-set-auto"            },
    { HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_SIMPLE,          "generator-set-simple"          },
    { HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_EXTENDED,        "generator-set-extended"        },
    { HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_ENABLE,          "generator-set-enable"          },
    { HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_VIRTUAL_PORT_PARAM, "sensor-set-virtual-port-param" },
    { HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UART_PORT_PARAM,    "sensor-set-uart-port-param"    },
    { HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UDP_IP_PORT_PARAM,  "sensor-set-udp-ip-port-param"  },
    { HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_POSITION,           "sensor-set-position"           },
    { HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_ENABLE,             "sensor-set-enable"             },
    { 0,                                           NULL                            }
  };

/* Приватная секция класса. */
struct _HyScanSonarControlAsyncPrivate
{
  gint                     busy;                /* Флаг, запрещающий выполнение нового запроса, пока не закончится выполнение текущего. */
  gint                     shutdown;            /* Флаг останова потока выполнения запросов и отслеживания окончания выполнения запроса. */

  GThread                 *thread;              /* Поток выполнения запросов. */
  GMutex                   mutex;               /* Мьютекс, для установки запроса на выполнение. */
  GCond                    cond;                /* Условие приостановки потока выполнения запросов. */

  GHashTable              *signal_info_map;     /* Ассоциативный массив, содержащий информацию (detail) сигнала completed. */

  gint                     query_id;            /* Идентификатор запроса (см. HyScanSonarControlAsyncAction) */
  gboolean                 query_completed;     /* Флаг завершения запроса. */
  gboolean                 query_result;        /* Результат выполнения запроса (TRUE - успех, FALSE - ошибка). */
  SonarControlQueryParams  query_params;        /* Параметры запроса. */
};

static void         hyscan_sonar_control_async_object_constructed                     (GObject                      *object);
static void         hyscan_sonar_control_async_object_finalize                        (GObject                      *object);
static void         hyscan_sonar_control_async_init_query_thread                      (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_shutdown                               (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_init_signal_info_map                   (HyScanSonarControlAsync      *self);
static GQuark       hyscan_sonar_control_async_get_current_query_detail               (HyScanSonarControlAsync      *self);

static void         hyscan_sonar_control_async_execute_query                          (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_set_query                              (HyScanSonarControlAsync      *self,
                                                                                       gint                          query);
static void         hyscan_sonar_control_async_sonar_set_sync_type_query              (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sonar_set_position_query               (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sonar_set_receive_time_query           (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sonar_start_query                      (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sonar_stop_query                       (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sonar_ping_query                       (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_tvg_set_auto_query                     (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_tvg_set_constant_query                 (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_tvg_set_linear_db_query                (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_tvg_set_logarithmic_query              (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_tvg_set_enable_query                   (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_generator_set_preset_query             (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_generator_set_auto_query               (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_generator_set_simple_query             (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_generator_set_extended_query           (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_generator_set_enable_query             (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sensor_set_virtual_port_param_query    (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sensor_set_uart_port_param_query       (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sensor_set_udp_ip_port_param_query     (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sensor_set_position_query              (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_sensor_set_enable_query                (HyScanSonarControlAsync      *self);

static gboolean     hyscan_sonar_control_async_set_busy                               (HyScanSonarControlAsync      *self);
static void         hyscan_sonar_control_async_set_idle                               (HyScanSonarControlAsync      *self);
static gpointer     hyscan_sonar_control_async_thread_func                            (gpointer                      self);
static gboolean     hyscan_sonar_control_async_result_func                            (gpointer                      self);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSonarControlAsync, hyscan_sonar_control_async, HYSCAN_TYPE_SONAR_CONTROL)

static void
hyscan_sonar_control_async_class_init (HyScanSonarControlAsyncClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->constructed = hyscan_sonar_control_async_object_constructed;
  obj_class->finalize = hyscan_sonar_control_async_object_finalize;

  hyscan_sonar_control_async_signals[SIGNAL_STARTED] =
    g_signal_new ("started", HYSCAN_TYPE_GENERATOR_CONTROL,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  hyscan_sonar_control_async_signals[SIGNAL_COMPLETED] =
    g_signal_new ("completed", HYSCAN_TYPE_GENERATOR_CONTROL,
                  G_SIGNAL_DETAILED | G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__INT_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_BOOLEAN);
}

static void
hyscan_sonar_control_async_init (HyScanSonarControlAsync *self)
{
  HyScanSonarControlAsyncPrivate *priv;

  self->priv = hyscan_sonar_control_async_get_instance_private (self);
  priv = self->priv;

  priv->thread = NULL;
  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);

  priv->busy = HYSCAN_ASYNC_BUSY;
  priv->query_id = HYSCAN_SONAR_CONTROL_ASYNC_EMPTY;
  priv->query_completed = FALSE;

  hyscan_sonar_control_async_init_signal_info_map (self);
}

static void
hyscan_sonar_control_async_object_constructed (GObject *object)
{
  HyScanSonarControlAsync *self = HYSCAN_SONAR_CONTROL_ASYNC (object);

  G_OBJECT_CLASS (hyscan_sonar_control_async_parent_class)->constructed (object);

  hyscan_sonar_control_async_init_query_thread (self);
}

static void
hyscan_sonar_control_async_object_finalize (GObject *object)
{
  HyScanSonarControlAsync *self = HYSCAN_SONAR_CONTROL_ASYNC (object);
  HyScanSonarControlAsyncPrivate *priv = self->priv;

  hyscan_sonar_control_async_shutdown (self);

  g_mutex_clear (&priv->mutex);
  g_cond_clear (&priv->cond);

  g_hash_table_unref (priv->signal_info_map);

  G_OBJECT_CLASS (hyscan_sonar_control_async_parent_class)->finalize (object);
}

/* Инициализация потока отправки запросов. */
static void
hyscan_sonar_control_async_init_query_thread (HyScanSonarControlAsync *self)
{
  HyScanSonarControlAsyncPrivate *priv = self->priv;

  priv->shutdown = HYSCAN_ASYNC_CONTINUE;
  priv->thread = g_thread_new (HYSCAN_ASYNC_THREAD_NAME, hyscan_sonar_control_async_thread_func, self);
  hyscan_sonar_control_async_set_idle (self);
}

/* Завершает поток отправки и выключает таймер опроса статуса. */
static void
hyscan_sonar_control_async_shutdown (HyScanSonarControlAsync *self)
{
  HyScanSonarControlAsyncPrivate *priv = self->priv;

  g_atomic_int_set (&priv->shutdown, HYSCAN_ASYNC_SHUTDOWN);
  g_thread_join (priv->thread);
}

/* Инициализация ассоциативного массива с информацией о запросах для
 * сигнала о завершении выполнения запроса. */
static void
hyscan_sonar_control_async_init_signal_info_map (HyScanSonarControlAsync *self)
{
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  SonarControlAsyncQueryInfo query_info;
  gint i;

  priv->signal_info_map = g_hash_table_new (NULL, NULL);

  for (i = 0; ; ++i)
    {
      GQuark detail;

      query_info = hyscan_sonar_control_async_query_info[i];
      if (query_info.id == 0 && query_info.name == NULL)
        break;

      detail = g_quark_from_static_string (query_info.name);
      g_hash_table_insert (priv->signal_info_map, GINT_TO_POINTER (query_info.id), GINT_TO_POINTER (detail));
    }
}

/* Возвращает детализацию для конкретоного типа запроса. Детализация (detail)
 * используется в сигнале о завершении выполнения запроса. */
static GQuark
hyscan_sonar_control_async_get_current_query_detail (HyScanSonarControlAsync *self)
{
  gpointer value = g_hash_table_lookup (self->priv->signal_info_map, GINT_TO_POINTER (self->priv->query_id));
  return (GQuark) GPOINTER_TO_INT (value);
}

/* Вызывает запрос, соответствующий типу запроса. */
static void
hyscan_sonar_control_async_execute_query (HyScanSonarControlAsync *self)
{
  switch (self->priv->query_id)
    {
    case HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_SYNC_TYPE:
      hyscan_sonar_control_async_sonar_set_sync_type_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_POSITION:
      hyscan_sonar_control_async_sonar_set_position_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_RECEIVE_TIME:
      hyscan_sonar_control_async_sonar_set_receive_time_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SONAR_START:
      hyscan_sonar_control_async_sonar_start_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SONAR_STOP:
      hyscan_sonar_control_async_sonar_stop_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SONAR_PING:
      hyscan_sonar_control_async_sonar_ping_query (self);
      break;

    case HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_AUTO:
      hyscan_sonar_control_async_tvg_set_auto_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_CONSTANT:
      hyscan_sonar_control_async_tvg_set_constant_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LINEAR_DB:
      hyscan_sonar_control_async_tvg_set_linear_db_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LOGARITHMIC:
      hyscan_sonar_control_async_tvg_set_logarithmic_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_ENABLE:
      hyscan_sonar_control_async_tvg_set_enable_query (self);
      break;

    case HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_PRESET:
      hyscan_sonar_control_async_generator_set_preset_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_AUTO:
      hyscan_sonar_control_async_generator_set_auto_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_SIMPLE:
      hyscan_sonar_control_async_generator_set_simple_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_EXTENDED:
      hyscan_sonar_control_async_generator_set_extended_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_ENABLE:
      hyscan_sonar_control_async_generator_set_enable_query (self);
      break;

    case HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_VIRTUAL_PORT_PARAM:
      hyscan_sonar_control_async_sensor_set_virtual_port_param_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UART_PORT_PARAM:
      hyscan_sonar_control_async_sensor_set_uart_port_param_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UDP_IP_PORT_PARAM:
      hyscan_sonar_control_async_sensor_set_udp_ip_port_param_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_POSITION:
      hyscan_sonar_control_async_sensor_set_position_query (self);
      break;
    case HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_ENABLE:
      hyscan_sonar_control_async_sensor_set_enable_query (self);
      break;
    default:
      break;
    }
}

/* Задаёт тип запроса, сбрасывает результат запроса и включает периодический
 * опрос статуса выполнения запроса. */
static void
hyscan_sonar_control_async_set_query (HyScanSonarControlAsync *self,
                                      gint                     query)
{
  HyScanSonarControlAsyncPrivate *priv = self->priv;

  g_mutex_lock (&priv->mutex);

  priv->query_id = query;
  priv->query_completed = FALSE;

  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);

  g_timeout_add (HYSCAN_ASYNC_RESULT_CHECK_TIMEOUT_MS, hyscan_sonar_control_async_result_func, self);
}

/* Запрос: установка типа синхронизации. */
static void
hyscan_sonar_control_async_sonar_set_sync_type_query (HyScanSonarControlAsync *self)
{
  HyScanSonarControl *sonar_control = HYSCAN_SONAR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSonarSetSyncTypeParams *prm = &priv->query_params.sonar_set_sync_type_params;

  priv->query_result = hyscan_sonar_control_set_sync_type (sonar_control, prm->sync_type);
}

/* Запрос: установка позиции антенны ГЛ. */
static void
hyscan_sonar_control_async_sonar_set_position_query (HyScanSonarControlAsync *self)
{
  HyScanSonarControl *sonar_control = HYSCAN_SONAR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSonarSetPositionParams *prm = &priv->query_params.sonar_set_position_params;

  priv->query_result = hyscan_sonar_control_set_position (sonar_control, prm->source, &prm->position);
}

/* Запрос: установка времени приёма. */
static void
hyscan_sonar_control_async_sonar_set_receive_time_query (HyScanSonarControlAsync *self)
{
  HyScanSonarControl *sonar_control = HYSCAN_SONAR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSonarSetReceiveTimeParams *prm = &priv->query_params.sonar_set_receive_type_params;

  priv->query_result = hyscan_sonar_control_set_receive_time (sonar_control, prm->source, prm->receive_time);
}

/* Запрос: запуск ГЛ. */
static void
hyscan_sonar_control_async_sonar_start_query (HyScanSonarControlAsync *self)
{
  HyScanSonarControl *sonar_control = HYSCAN_SONAR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSonarStartParams *prm = &priv->query_params.sonar_start_params;

  priv->query_result = hyscan_sonar_control_start (sonar_control, prm->track_name, prm->track_type);

  g_free (prm->track_name);
}

/* Запрос: останов ГЛ. */
static void
hyscan_sonar_control_async_sonar_stop_query (HyScanSonarControlAsync *self)
{
  HyScanSonarControl *sonar_control = HYSCAN_SONAR_CONTROL (self);

  self->priv->query_result = hyscan_sonar_control_stop (sonar_control);
}

/* Запрос: одиночное зондирование. */
static void
hyscan_sonar_control_async_sonar_ping_query (HyScanSonarControlAsync *self)
{
  HyScanSonarControl *sonar_control = HYSCAN_SONAR_CONTROL (self);

  self->priv->query_result = hyscan_sonar_control_ping (sonar_control);
}

/* Запрос: установка автоматического режима ВАРУ. */
static void
hyscan_sonar_control_async_tvg_set_auto_query (HyScanSonarControlAsync *self)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanTvgSetAutoParams *prm = &priv->query_params.tvg_set_auto_params;

  priv->query_result = hyscan_tvg_control_set_auto (tvg_control, prm->source, prm->level, prm->sensitivity);
}

/* Запрос: установка постоянного режима ВАРУ. */
static void
hyscan_sonar_control_async_tvg_set_constant_query (HyScanSonarControlAsync *self)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanTvgSetConstantParams *prm = &priv->query_params.tvg_set_constant_params;

  priv->query_result = hyscan_tvg_control_set_constant (tvg_control, prm->source, prm->gain);
}

/* Запрос: установка линейного увеличения усиления. */
static void
hyscan_sonar_control_async_tvg_set_linear_db_query (HyScanSonarControlAsync *self)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanTvgSetLinearDbParams *prm = &priv->query_params.tvg_set_linear_db_params;

  priv->query_result = hyscan_tvg_control_set_linear_db (tvg_control, prm->source, prm->gain0, prm->step);
}

/* Запрос: установка изменения усиления по логарифмическому закону. */
static void
hyscan_sonar_control_async_tvg_set_logarithmic_query (HyScanSonarControlAsync *self)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanTvgSetLogarithmicParams *prm = &priv->query_params.tvg_set_logarithmic_params;

  priv->query_result = hyscan_tvg_control_set_logarithmic (tvg_control, prm->source, prm->gain0, prm->beta, prm->alpha);
}

/* Запрос: включение/выключение системы ВАРУ. */
static void
hyscan_sonar_control_async_tvg_set_enable_query (HyScanSonarControlAsync *self)
{
  HyScanTVGControl *tvg_control = HYSCAN_TVG_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanTvgSetEnableParams *prm = &priv->query_params.tvg_set_enable_params;

  priv->query_result = hyscan_tvg_control_set_enable (tvg_control, prm->source, prm->enable);
}

/* Запрос: установка режима работы генератора по преднастройкам. */
static void
hyscan_sonar_control_async_generator_set_preset_query (HyScanSonarControlAsync *self)
{
  HyScanGeneratorControl *gen_control = HYSCAN_GENERATOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanGenSetPresetParams *prm = &priv->query_params.gen_set_preset_params;

  priv->query_result = hyscan_generator_control_set_preset (gen_control, prm->source, prm->preset);
}

/* Запрос: установка работы генератора в автоматическом режиме. */
static void
hyscan_sonar_control_async_generator_set_auto_query (HyScanSonarControlAsync *self)
{
  HyScanGeneratorControl *gen_control = HYSCAN_GENERATOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanGenSetAutoParams *prm = &priv->query_params.gen_set_auto_params;

  priv->query_result = hyscan_generator_control_set_auto (gen_control, prm->source, prm->signal);
}

/* Запрос: установка упрощённого режима работы генератора. */
static void
hyscan_sonar_control_async_generator_set_simple_query (HyScanSonarControlAsync *self)
{
  HyScanGeneratorControl *gen_control = HYSCAN_GENERATOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanGenSetSimpleParams *prm = &priv->query_params.gen_set_simple_params;

  priv->query_result = hyscan_generator_control_set_simple (gen_control, prm->source, prm->signal, prm->power);
}

/* Запрос: установка расширенного режима работы генератора. */
static void
hyscan_sonar_control_async_generator_set_extended_query (HyScanSonarControlAsync *self)
{
  HyScanGeneratorControl *gen_control = HYSCAN_GENERATOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanGenSetExtendedParams *prm = &priv->query_params.gen_set_extended_params;

  priv->query_result = hyscan_generator_control_set_extended (gen_control, prm->source, prm->signal, prm->duration, prm->power);
}

/* Запрос: включение/выключение генератора. */
static void
hyscan_sonar_control_async_generator_set_enable_query (HyScanSonarControlAsync *self)
{
  HyScanGeneratorControl *gen_control = HYSCAN_GENERATOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanGenSetEnableParams *prm = &priv->query_params.gen_set_enable_params;

  priv->query_result = hyscan_generator_control_set_enable (gen_control, prm->source, prm->enable);
}

/* Запрос: установка параметров виртуального порта датчика. */
static void
hyscan_sonar_control_async_sensor_set_virtual_port_param_query (HyScanSonarControlAsync *self)
{
  HyScanSensorControl *sensor_control = HYSCAN_SENSOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSensorSetVirtualPortParamParams *prm = &priv->query_params.sensor_set_virtual_port_param_params;

  priv->query_result = hyscan_sensor_control_set_virtual_port_param (sensor_control, prm->name, prm->channel, prm->time_offset);

  g_free (prm->name);
}

/* Запрос: установка параметров UART порта датчика. */
static void
hyscan_sonar_control_async_sensor_set_uart_port_param_query (HyScanSonarControlAsync *self)
{
  HyScanSensorControl *sensor_control = HYSCAN_SENSOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSensorSetUartPortParamParams *prm = &priv->query_params.sensor_set_uart_port_param_params;

  priv->query_result = hyscan_sensor_control_set_uart_port_param (sensor_control, prm->name, prm->channel, prm->time_offset,
                                                                  prm->protocol, prm->uart_device, prm->uart_mode);

  g_free (prm->name);
}

/* Запрос: установка параметров UDP/IP порта датчика. */
static void
hyscan_sonar_control_async_sensor_set_udp_ip_port_param_query (HyScanSonarControlAsync *self)
{
  HyScanSensorControl *sensor_control = HYSCAN_SENSOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSensorSetUdpIpPortParamParams *prm = &priv->query_params.sensor_set_udp_ip_port_param_params;

  priv->query_result = hyscan_sensor_control_set_udp_ip_port_param (sensor_control, prm->name, prm->channel, prm->time_offset,
                                                                    prm->protocol, prm->ip_address, prm->udp_port);

  g_free (prm->name);
}

/* Запрос: установка местоположения датчика. */
static void
hyscan_sonar_control_async_sensor_set_position_query (HyScanSonarControlAsync *self)
{
  HyScanSensorControl *sensor_control = HYSCAN_SENSOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSensorSetPositionParams *prm = &priv->query_params.sensor_set_position_params;

  priv->query_result = hyscan_sensor_control_set_position (sensor_control, prm->name, &prm->position);

  g_free (prm->name);
}

/* Запрос: включение/выключение датчика. */
static void
hyscan_sonar_control_async_sensor_set_enable_query (HyScanSonarControlAsync *self)
{
  HyScanSensorControl *sensor_control = HYSCAN_SENSOR_CONTROL (self);
  HyScanSonarControlAsyncPrivate *priv = self->priv;
  HyScanSensorSetEnableParams *prm = &priv->query_params.sensor_set_enable_params;

  priv->query_result = hyscan_sensor_control_set_enable (sensor_control, prm->name, prm->enable);

  g_free (prm->name);
}

/* Перевод в режим выполнения запроса. */
static gboolean
hyscan_sonar_control_async_set_busy (HyScanSonarControlAsync *self)
{
  HyScanSonarControlAsyncPrivate *priv;
  gboolean res;

  priv = self->priv;
  res = g_atomic_int_compare_and_exchange (&self->priv->busy, HYSCAN_ASYNC_IDLE, HYSCAN_ASYNC_BUSY);

  if (res)
    g_signal_emit (self, hyscan_sonar_control_async_signals[SIGNAL_STARTED], 0, priv->query_id);

  return res;
}

/* Перевод в режим ожидания запроса. */
static void
hyscan_sonar_control_async_set_idle (HyScanSonarControlAsync *self)
{
  g_atomic_int_set (&self->priv->busy, HYSCAN_ASYNC_IDLE);
}

/* Поток выполнения запросов. */
static gpointer
hyscan_sonar_control_async_thread_func (gpointer object)
{
  HyScanSonarControlAsync *self;
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (object), GINT_TO_POINTER (-1));

  self = HYSCAN_SONAR_CONTROL_ASYNC (object);
  priv = self->priv;

  while (g_atomic_int_get (&priv->shutdown) == HYSCAN_ASYNC_CONTINUE)
    {
      g_mutex_lock (&priv->mutex);

      if (priv->query_completed || priv->query_id == HYSCAN_SONAR_CONTROL_ASYNC_EMPTY)
        {
          gint64 end_time = g_get_monotonic_time () + HYSCAN_ASYNC_COND_WAIT_TIMEOUT;
          g_cond_wait_until (&priv->cond, &priv->mutex, end_time);
        }
      else
        {
          hyscan_sonar_control_async_execute_query (self);
          priv->query_completed = TRUE;
        }

      g_mutex_unlock (&priv->mutex);
    }

  return GINT_TO_POINTER (0);
}

/* Callback-функция опроса состояния запроса. */
static gboolean
hyscan_sonar_control_async_result_func (gpointer data)
{
  HyScanSonarControlAsync *self = data;
  HyScanSonarControlAsyncPrivate *priv = self->priv;

  if (g_atomic_int_get (&priv->shutdown) == HYSCAN_ASYNC_SHUTDOWN)
    return G_SOURCE_REMOVE;

  if (g_mutex_trylock (&priv->mutex))
    {
      gboolean retval;

      if (priv->query_completed)
        {
          guint signal_id = hyscan_sonar_control_async_signals[SIGNAL_COMPLETED];
          GQuark detail = hyscan_sonar_control_async_get_current_query_detail (self);

          hyscan_sonar_control_async_set_idle (self);
          g_signal_emit (self, signal_id, detail, priv->query_id, priv->query_result);

          priv->query_id = HYSCAN_SONAR_CONTROL_ASYNC_EMPTY;
          priv->query_completed = FALSE;

          retval = G_SOURCE_REMOVE;
        }
      else
        {
          retval = G_SOURCE_CONTINUE;
        }

      g_mutex_unlock (&priv->mutex);
      return retval;
    }

  return G_SOURCE_CONTINUE;
}

/* Создаёт новый объект HyScanSonarControlAsync. */
HyScanSonarControlAsync *
hyscan_sonar_control_async_new (HyScanParam  *sonar,
                                guint         n_uart_ports,
                                guint         n_udp_ports,
                                HyScanDB     *db)
{
  return g_object_new (HYSCAN_TYPE_SONAR_CONTROL_ASYNC,
                       "sonar", sonar,
                       "n-uart-ports", n_uart_ports,
                       "n-udp-ports", n_udp_ports,
                       "db", db,
                       NULL);
}

/* Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_VIRTUAL. */
gboolean
hyscan_sonar_control_async_sensor_set_virtual_port_param (HyScanSonarControlAsync *async,
                                                          const gchar             *name,
                                                          guint                    channel,
                                                          gint64                   time_offset)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sensor_set_virtual_port_param_params.name = g_strdup (name);
  priv->query_params.sensor_set_virtual_port_param_params.channel = channel;
  priv->query_params.sensor_set_virtual_port_param_params.time_offset = time_offset;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_VIRTUAL_PORT_PARAM);

  return TRUE;
}

/* Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UART. */
gboolean
hyscan_sonar_control_async_sensor_set_uart_port_param (HyScanSonarControlAsync  *async,
                                                       const gchar              *name,
                                                       guint                     channel,
                                                       gint64                    time_offset,
                                                       HyScanSensorProtocolType  protocol,
                                                       guint                     uart_device,
                                                       guint                     uart_mode)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sensor_set_uart_port_param_params.name = g_strdup (name);
  priv->query_params.sensor_set_uart_port_param_params.channel = channel;
  priv->query_params.sensor_set_uart_port_param_params.time_offset = time_offset;
  priv->query_params.sensor_set_uart_port_param_params.protocol = protocol;
  priv->query_params.sensor_set_uart_port_param_params.uart_device = uart_device;
  priv->query_params.sensor_set_uart_port_param_params.uart_mode = uart_mode;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UART_PORT_PARAM);

  return TRUE;
}

/* Функция асинхронно устанавливает режим работы порта типа HYSCAN_SENSOR_CONTROL_PORT_UDP_IP. */
gboolean
hyscan_sonar_control_async_sensor_set_udp_ip_port_param (HyScanSonarControlAsync  *async,
                                                         const gchar              *name,
                                                         guint                     channel,
                                                         gint64                    time_offset,
                                                         HyScanSensorProtocolType  protocol,
                                                         guint                     ip_address,
                                                         guint16                   udp_port)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sensor_set_udp_ip_port_param_params.name = g_strdup (name);
  priv->query_params.sensor_set_udp_ip_port_param_params.channel = channel;
  priv->query_params.sensor_set_udp_ip_port_param_params.time_offset = time_offset;
  priv->query_params.sensor_set_udp_ip_port_param_params.protocol = protocol;
  priv->query_params.sensor_set_udp_ip_port_param_params.ip_address = ip_address;
  priv->query_params.sensor_set_udp_ip_port_param_params.udp_port = udp_port;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_UDP_IP_PORT_PARAM);

  return TRUE;
}

/* Функция асинхронно устанавливает информацию о местоположении приёмных антенн относительно центра масс судна. */
gboolean
hyscan_sonar_control_async_sensor_set_position (HyScanSonarControlAsync *async,
                                                const gchar             *name,
                                                HyScanAntennaPosition   *position)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sensor_set_position_params.name = g_strdup (name);
  priv->query_params.sensor_set_position_params.position = *position;
  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_POSITION);

  return TRUE;
}

/* Функция асинхронно включает или выключает приём данных на указанном порту. */
gboolean
hyscan_sonar_control_async_sensor_set_enable (HyScanSonarControlAsync *async,
                                              const gchar             *name,
                                              gboolean                 enable)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sensor_set_enable_params.name = g_strdup (name);
  priv->query_params.sensor_set_enable_params.enable = enable;
  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SENSOR_SET_ENABLE);

  return TRUE;
}

/* Функция асинхронно включает преднастроенный режим работы генератора. */
gboolean
hyscan_sonar_control_async_generator_set_preset (HyScanSonarControlAsync *async,
                                                 HyScanSourceType         source,
                                                 guint                    preset)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.gen_set_preset_params.source = source;
  priv->query_params.gen_set_preset_params.preset = preset;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_PRESET);

  return TRUE;
}

/* Функция асинхронно включает автоматический режим работы генератора. */
gboolean
hyscan_sonar_control_async_generator_set_auto (HyScanSonarControlAsync    *async,
                                               HyScanSourceType            source,
                                               HyScanGeneratorSignalType   signal)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.gen_set_auto_params.source = source;
  priv->query_params.gen_set_auto_params.signal = signal;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_AUTO);

  return TRUE;
}

/* Функция асинхронно включает упрощённый режим работы генератора. */
gboolean
hyscan_sonar_control_async_generator_set_simple (HyScanSonarControlAsync    *async,
                                                 HyScanSourceType            source,
                                                 HyScanGeneratorSignalType   signal,
                                                 gdouble                     power)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.gen_set_simple_params.source = source;
  priv->query_params.gen_set_simple_params.signal = signal;
  priv->query_params.gen_set_simple_params.power = power;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_SIMPLE);

  return TRUE;
}

/* Функция асинхронно включает расширенный режим работы генератора. */
gboolean
hyscan_sonar_control_async_generator_set_extended (HyScanSonarControlAsync   *async,
                                                   HyScanSourceType           source,
                                                   HyScanGeneratorSignalType  signal,
                                                   gdouble                    duration,
                                                   gdouble                    power)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.gen_set_extended_params.source = source;
  priv->query_params.gen_set_extended_params.signal = signal;
  priv->query_params.gen_set_extended_params.duration = duration;
  priv->query_params.gen_set_extended_params.power = power;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_EXTENDED);

  return TRUE;
}


/* Функция асинхронно включает или выключает формирование сигнала генератором. */
gboolean
hyscan_sonar_control_async_generator_set_enable (HyScanSonarControlAsync *async,
                                                 HyScanSourceType         source,
                                                 gboolean                 enable)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.gen_set_enable_params.source = source;
  priv->query_params.gen_set_enable_params.enable = enable;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_GENERATOR_SET_ENABLE);

  return TRUE;
}


/* Функция асинхронно включает автоматический режим управления системой ВАРУ. */
gboolean
hyscan_sonar_control_async_tvg_set_auto (HyScanSonarControlAsync *async,
                                         HyScanSourceType         source,
                                         gdouble                  level,
                                         gdouble                  sensitivity)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.tvg_set_auto_params.source = source;
  priv->query_params.tvg_set_auto_params.level = level;
  priv->query_params.tvg_set_auto_params.sensitivity = sensitivity;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_AUTO);

  return TRUE;
}

/* Функция асинхронно устанавливает постоянный уровень усиления системой ВАРУ. */
gboolean
hyscan_sonar_control_async_tvg_set_constant (HyScanSonarControlAsync *async,
                                             HyScanSourceType         source,
                                             gdouble                  gain)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.tvg_set_constant_params.source = source;
  priv->query_params.tvg_set_constant_params.gain = gain;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_CONSTANT);

  return TRUE;
}

/* Функция асинхронно устанавливает линейное увеличение усиления в дБ на 100 метров. */
gboolean
hyscan_sonar_control_async_tvg_set_linear_db (HyScanSonarControlAsync *async,
                                              HyScanSourceType         source,
                                              gdouble                  gain0,
                                              gdouble                  step)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.tvg_set_linear_db_params.source = source;
  priv->query_params.tvg_set_linear_db_params.gain0 = gain0;
  priv->query_params.tvg_set_linear_db_params.step = step;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LINEAR_DB);

  return TRUE;
}

/* Функция асинхронно устанавливает логарифмический вид закона усиления системой ВАРУ. */
gboolean
hyscan_sonar_control_async_tvg_set_logarithmic (HyScanSonarControlAsync *async,
                                                HyScanSourceType         source,
                                                gdouble                  gain0,
                                                gdouble                  beta,
                                                gdouble                  alpha)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.tvg_set_logarithmic_params.source = source;
  priv->query_params.tvg_set_logarithmic_params.gain0 = gain0;
  priv->query_params.tvg_set_logarithmic_params.beta = beta;
  priv->query_params.tvg_set_logarithmic_params.alpha = alpha;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_LOGARITHMIC);

  return TRUE;
}

/* Функция асинхронно включает или выключает систему ВАРУ. */
gboolean
hyscan_sonar_control_async_tvg_set_enable (HyScanSonarControlAsync *async,
                                           HyScanSourceType         source,
                                           gboolean                 enable)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.tvg_set_enable_params.source = source;
  priv->query_params.tvg_set_enable_params.enable = enable;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_TVG_SET_ENABLE);

  return TRUE;
}

/* Функция асинхронно устанавливает тип синхронизации излучения. */
gboolean
hyscan_sonar_control_async_sonar_set_sync_type (HyScanSonarControlAsync *async,
                                                HyScanSonarSyncType      sync_type)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sonar_set_sync_type_params.sync_type = sync_type;
  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_SYNC_TYPE);

  return TRUE;
}

/* Функция асинхронно устанавливает информацию о местоположении приёмных антенн
 * относительно центра масс судна. */
gboolean
hyscan_sonar_control_async_sonar_set_position (HyScanSonarControlAsync *async,
                                               HyScanSourceType         source,
                                               HyScanAntennaPosition   *position)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sonar_set_position_params.source = source;
  priv->query_params.sonar_set_position_params.position = *position;
  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_POSITION);

  return TRUE;
}

/* Функция асинхронно задаёт время приёма эхосигнала источником данных. */
gboolean
hyscan_sonar_control_async_sonar_set_receive_time (HyScanSonarControlAsync *async,
                                                   HyScanSourceType         source,
                                                   gdouble                  receive_time)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sonar_set_receive_type_params.source = source;
  priv->query_params.sonar_set_receive_type_params.receive_time = receive_time;
  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SONAR_SET_RECEIVE_TIME);

  return TRUE;
}

/* Функция асинхронно переводит гидролокатор в рабочий режим и включает запись данных. */
gboolean
hyscan_sonar_control_async_sonar_start (HyScanSonarControlAsync *async,
                                        const gchar             *track_name,
                                        HyScanTrackType          track_type)
{
  HyScanSonarControlAsyncPrivate *priv;
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);
  priv = async->priv;

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  priv->query_params.sonar_start_params.track_name = g_strdup (track_name);
  priv->query_params.sonar_start_params.track_type = track_type;
  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SONAR_START);

  return TRUE;
}

/* Функция асинхронно переводит гидролокатор в ждущий режим и отключает запись данных. */
gboolean
hyscan_sonar_control_async_sonar_stop (HyScanSonarControlAsync *async)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SONAR_STOP);

  return TRUE;
}

/* Функция асинхронно выполняет один цикл зондирования и приёма данных. */
gboolean
hyscan_sonar_control_async_sonar_ping (HyScanSonarControlAsync    *async)
{
  g_return_val_if_fail (HYSCAN_IS_SONAR_CONTROL_ASYNC (async), FALSE);

  if (!hyscan_sonar_control_async_set_busy (async))
    return FALSE;

  hyscan_sonar_control_async_set_query (async, HYSCAN_SONAR_CONTROL_ASYNC_SONAR_PING);

  return TRUE;
}
