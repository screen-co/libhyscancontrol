/*
 * \file hyscan-sonar-sensor.c
 *
 * \brief Исходный файл класса управления датчиками местоположения и ориентации
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include <glib/gi18n-lib.h>

#include <hyscan-nmea-udp.h>
#include <hyscan-nmea-uart.h>
#include <hyscan-core-types.h>
#include "hyscan-sensor-control.h"
#include "hyscan-sonar-messages.h"
#include "hyscan-control-common.h"
#include "hyscan-control-marshallers.h"
#include <string.h>

enum
{
  PROP_O,
  PROP_SONAR,
  PROP_N_UART_PORTS,
  PROP_N_UDP_PORTS
};

enum
{
  SIGNAL_SENSOR_DATA,
  SIGNAL_LAST
};

/* Описание локального порта. */
typedef struct
{
  gboolean                     enable;                         /* Признак включения порта. */
  HyScanSensorPortType         type;                           /* Тип порта. */
  gint64                       time_offset;                    /* Коррекция времени. */
  guint                        channel;                        /* Номер канала данных. */

  union
  {
    struct
    {
      gchar                   *device;                         /* UART устройство. */
      HyScanNmeaUARTMode       mode;                           /* Режим работы. */
    } uart;
    struct
    {
      gchar                   *address;                        /* IP адрес. */
      guint16                  port;                           /* UDP порт. */
    } udp;
  };

  GObject                     *receiver;                       /* Приёмник NMEA данных. */
} HyScanSensorControlLocalPort;

/* Описание порта гидролокатора. */
typedef struct
{
  gchar                       *name;                           /* Название порта. */
  gchar                       *path;                           /* Путь к описанию порта в схеме. */
  HyScanSensorPortType         type;                           /* Тип порта. */
  HyScanSensorProtocolType     protocol;                       /* Протокол передачи данных. */
  gint64                       time_offset;                    /* Коррекция времени. */
  guint                        channel;                        /* Номер канала данных. */
} HyScanSensorControlPort;

struct _HyScanSensorControlPrivate
{
  HyScanParam                 *sonar;                          /* Интерфейс управления гидролокатором. */
  HyScanDataSchema            *schema;                         /* Схема параметров гидролокатора. */

  guint                        n_uart_ports;                   /* Число локальных UART портов. */
  guint                        n_udp_ports;                    /* Число локальных UDP/IP портов. */

  HyScanDataSchema            *local_schema;                   /* Таблицы параметров локальных портов. */
  GHashTable                  *uart_devices;                   /* Таблица UART устройств. */
  GHashTable                  *uart_modes;                     /* Таблица режимов работы UART устройств. */
  GHashTable                  *ip_addresses;                   /* IP адреса доступные системе. */
  GHashTable                  *local_ports;                    /* Список локальных портов. */

  GHashTable                  *ports_by_id;                    /* Список портов для подключения датчиков. */
  GHashTable                  *ports_by_name;                  /* Список портов для подключения датчиков. */

  GMutex                       lock;                           /* Блокировка. */
};

static void          hyscan_sensor_control_set_property        (GObject                      *object,
                                                                guint                         prop_id,
                                                                const GValue                 *value,
                                                                GParamSpec                   *pspec);
static void          hyscan_sensor_control_object_constructed  (GObject                      *object);
static void          hyscan_sensor_control_object_finalize     (GObject                      *object);

static HyScanSourceType hyscan_sensor_control_get_source_type  (const gchar                  *nmea_str);

static void          hyscan_sensor_control_add_uart_devices    (HyScanDataSchemaBuilder      *builder,
                                                                GHashTable                   *devices);
static void          hyscan_sensor_control_add_uart_modes      (HyScanDataSchemaBuilder      *builder,
                                                                GHashTable                   *modes);
static void          hyscan_sensor_control_add_ip_addresses    (HyScanDataSchemaBuilder      *builder,
                                                                GHashTable                   *addresses);

static void          hyscan_sensor_control_free_local_port     (gpointer                      data);
static void          hyscan_sensor_control_free_port           (gpointer                      data);

static gboolean      hyscan_sensor_control_setup_uart_port     (HyScanSensorControl          *control,
                                                                const gchar                  *name,
                                                                gboolean                      enable);
static gboolean      hyscan_sensor_control_setup_udp_port      (HyScanSensorControl          *control,
                                                                const gchar                  *name,
                                                                gboolean                      enable);

static void          hyscan_sensor_control_nmea_sender         (HyScanSensorControl          *control,
                                                                guint                         channel,
                                                                gint64                        time,
                                                                gint64                        offset,
                                                                const gchar                  *name,
                                                                guint                         size,
                                                                const gchar                  *nmea);
static void          hyscan_sensor_control_local_receiver      (HyScanSensorControl          *control,
                                                                gint64                        time,
                                                                const gchar                  *name,
                                                                guint                         size,
                                                                const gchar                  *nmea);
static void          hyscan_sensor_control_data_receiver       (HyScanSensorControl          *control,
                                                                HyScanSonarMessage           *message);

static gboolean      hyscan_sensor_control_check_nmea_crc      (const gchar                  *nmea_str);

static guint         hyscan_sensor_control_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanSensorControl, hyscan_sensor_control, HYSCAN_TYPE_DATA_WRITER)

static void
hyscan_sensor_control_class_init (HyScanSensorControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_sensor_control_set_property;

  object_class->constructed = hyscan_sensor_control_object_constructed;
  object_class->finalize = hyscan_sensor_control_object_finalize;

  g_object_class_install_property (object_class, PROP_SONAR,
    g_param_spec_object ("sonar", "Sonar", "Sonar interface", HYSCAN_TYPE_PARAM,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_UART_PORTS,
    g_param_spec_uint ("n-uart-ports", "nUARTPort", "Number of local UART ports", 0, 4, 0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_UDP_PORTS,
    g_param_spec_uint ("n-udp-ports", "nUDPPort", "Number of local UDP/IP ports", 0, 4, 0,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_sensor_control_signals[SIGNAL_SENSOR_DATA] =
    g_signal_new ("sensor-data", HYSCAN_TYPE_SENSOR_CONTROL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__STRING_INT_INT_POINTER,
                  G_TYPE_NONE,
                  4, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_POINTER);
}

static void
hyscan_sensor_control_init (HyScanSensorControl *control)
{
  control->priv = hyscan_sensor_control_get_instance_private (control);
}

static void
hyscan_sensor_control_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  switch (prop_id)
    {
    case PROP_SONAR:
      priv->sonar = g_value_dup_object (value);
      break;

    case PROP_N_UART_PORTS:
      priv->n_uart_ports = g_value_get_uint (value);
      break;

    case PROP_N_UDP_PORTS:
      priv->n_udp_ports = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_sensor_control_object_constructed (GObject *object)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  HyScanDataSchemaNode *params;
  HyScanDataSchemaNode *sensors;

  gint64 version;
  gint64 id;
  guint i, j;

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);

  /* Список доступных портов. */
  priv->local_ports = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, hyscan_sensor_control_free_local_port);
  priv->ports_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL, hyscan_sensor_control_free_port);
  priv->ports_by_name = g_hash_table_new (g_str_hash, g_str_equal);

  /* Параметры локальных портов. */
  priv->uart_devices = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
  priv->uart_modes = g_hash_table_new (g_direct_hash, g_direct_equal);
  priv->ip_addresses = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

  /* Обязательно должен быть передан указатель на интерфейс управления локатором. */
  if (priv->sonar == NULL)
    {
      g_warning ("HyScanControl: empty sonar");
      return;
    }

  /* Проверяем идентификатор и версию схемы гидролокатора. */
  if (!hyscan_param_get_integer (priv->sonar, "/schema/id", &id))
    {
      g_warning ("HyScanControl: unknown sonar schema id");
      return;
    }
  if (id != HYSCAN_SONAR_SCHEMA_ID)
    {
      g_warning ("HyScanControl: sonar schema id mismatch");
      return;
    }
  if (!hyscan_param_get_integer (priv->sonar, "/schema/version", &version))
    {
      g_warning ("HyScanControl: unknown sonar schema version");
      return;
    }
  if ((version / 100) != (HYSCAN_SONAR_SCHEMA_VERSION / 100))
    {
      g_warning ("HyScanControl: sonar schema version mismatch");
      return;
    }

  /* Параметры гидролокатора. */
  priv->schema = hyscan_param_schema (priv->sonar);
  params = hyscan_data_schema_list_nodes (priv->schema);

  /* Ветка схемы с описанием портов - "/sensors". */
  for (i = 0, sensors = NULL; i < params->n_nodes; i++)
    {
      if (g_strcmp0 (params->nodes[i]->path, "/sensors") == 0)
        {
          sensors = params->nodes[i];
          break;
        }
    }

  if (sensors != NULL)
    {
      /* Считываем описания портов. */
      for (i = 0; i < sensors->n_nodes; i++)
        {
          HyScanSensorControlPort *port;

          gchar *param_names[4];
          GVariant *param_values[4];

          gint64 id;
          gint64 type;
          gint64 protocol;

          gchar **pathv;
          gchar *name;

          gboolean status;

          param_names[0] = g_strdup_printf ("%s/id", sensors->nodes[i]->path);
          param_names[1] = g_strdup_printf ("%s/type", sensors->nodes[i]->path);
          param_names[2] = g_strdup_printf ("%s/protocol", sensors->nodes[i]->path);
          param_names[3] = NULL;

          status = hyscan_param_get (priv->sonar, (const gchar **)param_names, param_values);

          if (status)
            {
              id = g_variant_get_int64 (param_values[0]);
              type = g_variant_get_int64 (param_values[1]);
              protocol = g_variant_get_int64 (param_values[2]);

              g_variant_unref (param_values[0]);
              g_variant_unref (param_values[1]);
              g_variant_unref (param_values[2]);
            }

          g_free (param_names[0]);
          g_free (param_names[1]);
          g_free (param_names[2]);

          if (!status)
            continue;

          if (id <= 0 || id > G_MAXINT32)
            continue;

          if (type != HYSCAN_SENSOR_PORT_VIRTUAL &&
              type != HYSCAN_SENSOR_PORT_UART &&
              type != HYSCAN_SENSOR_PORT_UDP_IP)
            {
              continue;
            }

          if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS &&
              protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
            {
              continue;
            }

          pathv = g_strsplit (sensors->nodes[i]->path, "/", -1);
          name = g_strdup (pathv[2]);
          g_strfreev (pathv);

          /* Описание порта. */
          port = g_new0 (HyScanSensorControlPort, 1);
          port->name = name;
          port->path = g_strdup (sensors->nodes[i]->path);
          port->type = type;
          port->channel = 1;
          port->protocol = protocol;

          g_hash_table_insert (priv->ports_by_id, GINT_TO_POINTER (id), port);
          g_hash_table_insert (priv->ports_by_name, name, port);
        }

      /* Обработчик данных от гидролокатора. */
      g_signal_connect_swapped (priv->sonar, "data",
                                G_CALLBACK (hyscan_sensor_control_data_receiver), control);
    }

  /* Локальные порты. */
  if ((priv->n_uart_ports > 0) || (priv->n_udp_ports > 0))
    {
      HyScanDataSchemaBuilder *builder;
      gchar *schema_data;

      builder = hyscan_data_schema_builder_new ("ports");
      hyscan_sensor_control_add_uart_devices (builder, priv->uart_devices);
      hyscan_sensor_control_add_uart_modes (builder, priv->uart_modes);
      hyscan_sensor_control_add_ip_addresses (builder, priv->ip_addresses);

      schema_data = hyscan_data_schema_builder_get_data (builder);
      priv->local_schema = hyscan_data_schema_new_from_string (schema_data, "ports");

      g_object_unref (builder);
      g_free (schema_data);
    }

  /* Локальные UART порты. */
  for (i = 0, j = 1; i < priv->n_uart_ports; j++)
    {
      gchar *name = g_strdup_printf ("uart.%d", j);

      if (!g_hash_table_contains (priv->ports_by_name, name))
        {
          HyScanSensorControlLocalPort *port;

          port = g_new0 (HyScanSensorControlLocalPort, 1);
          port->type = HYSCAN_SENSOR_PORT_UART;
          g_hash_table_insert (priv->local_ports, name, port);

          i++;
        }
      else
        {
          g_free (name);
        }
    }

  /* Локальные UDP порты. */
  for (i = 0, j = 1; i < priv->n_udp_ports; j++)
    {
      gchar *name = g_strdup_printf ("udp.%d", j);

      if (!g_hash_table_contains (priv->ports_by_name, name))
        {
          HyScanSensorControlLocalPort *port;

          port = g_new0 (HyScanSensorControlLocalPort, 1);
          port->type = HYSCAN_SENSOR_PORT_UDP_IP;
          g_hash_table_insert (priv->local_ports, name, port);

          i++;
        }
      else
        {
          g_free (name);
        }
    }

  hyscan_data_schema_free_nodes (params);
}

static void
hyscan_sensor_control_object_finalize (GObject *object)
{
  HyScanSensorControl *control = HYSCAN_SENSOR_CONTROL (object);
  HyScanSensorControlPrivate *priv = control->priv;

  g_signal_handlers_disconnect_by_data (priv->sonar, control);

  g_clear_object (&priv->local_schema);
  g_clear_object (&priv->schema);
  g_clear_object (&priv->sonar);

  g_hash_table_unref (priv->ports_by_id);
  g_hash_table_unref (priv->ports_by_name);
  g_hash_table_unref (priv->local_ports);

  g_hash_table_unref (priv->uart_devices);
  g_hash_table_unref (priv->uart_modes);
  g_hash_table_unref (priv->ip_addresses);

  g_mutex_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_sensor_control_parent_class)->finalize (object);
}

/* Функция возвращает название канала, в которые необходимо записать данные. */
HyScanSourceType
hyscan_sensor_control_get_source_type (const gchar *nmea_str)
{
  HyScanSourceType source;

  if (strncmp (nmea_str + 3, "GGA", 3) == 0)
    source = HYSCAN_SOURCE_NMEA_GGA;
  else if (strncmp (nmea_str + 3, "RMC", 3) == 0)
    source = HYSCAN_SOURCE_NMEA_RMC;
  else if (strncmp (nmea_str + 3, "DPT", 3) == 0)
    source = HYSCAN_SOURCE_NMEA_DPT;
  else
    source = HYSCAN_SOURCE_NMEA_ANY;

  return source;
}

/* Функция формирует таблицу UART устройсв. */
static void
hyscan_sensor_control_add_uart_devices (HyScanDataSchemaBuilder *builder,
                                        GHashTable              *devices)
{
  HyScanNmeaUARTDevice **uarts;
  guint i;

  uarts = hyscan_nmea_uart_list_devices ();

  hyscan_data_schema_builder_enum_create (builder, "uart-devices");

  hyscan_data_schema_builder_enum_value_create (builder, "uart-devices", 0,
                                                         "Disabled", _("Disabled"));

  for (i = 0; (uarts != NULL) && (uarts[i] != NULL); i++)
    {
      hyscan_data_schema_builder_enum_value_create (builder, "uart-devices", i + 1,
                                                             uarts[i]->name, uarts[i]->name);

      g_hash_table_insert (devices, GINT_TO_POINTER (i + 1), g_strdup (uarts[i]->path));
    }

  hyscan_nmea_uart_devices_free (uarts);
}

/* Функция формирует таблицу режимов работы UART устройств. */
static void
hyscan_sensor_control_add_uart_modes (HyScanDataSchemaBuilder *builder,
                                      GHashTable              *modes)
{
  hyscan_data_schema_builder_enum_create (builder, "uart-modes");

  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_DISABLED,
                                                         "Disabled", _("Disabled"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_AUTO,
                                                         "Auto", _("Auto"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_4800_8N1,
                                                         "4800-8N1", _("4800 8N1"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_9600_8N1,
                                                         "9600-8N1", _("9600 8N1"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_19200_8N1,
                                                         "19200-8N1", _("19200 8N1"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_38400_8N1,
                                                         "38400-8N1", _("38400 8N1"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_57600_8N1,
                                                         "57600-8N1", _("57600 8N1"));
  hyscan_data_schema_builder_enum_value_create (builder, "uart-modes", HYSCAN_NMEA_UART_MODE_115200_8N1,
                                                         "115200-8N1", _("115200 8N1"));

  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_AUTO),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_AUTO));
  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_4800_8N1),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_4800_8N1));
  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_9600_8N1),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_9600_8N1));
  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_19200_8N1),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_19200_8N1));
  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_38400_8N1),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_38400_8N1));
  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_57600_8N1),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_57600_8N1));
  g_hash_table_insert (modes, GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_115200_8N1),
                              GINT_TO_POINTER (HYSCAN_NMEA_UART_MODE_115200_8N1));
}

/* Функция формирует таблицу IP адресов. */
static void
hyscan_sensor_control_add_ip_addresses (HyScanDataSchemaBuilder *builder,
                                        GHashTable              *addresses)
{
  gchar **ips;
  guint i;

  ips = hyscan_nmea_udp_list_addresses ();

  hyscan_data_schema_builder_enum_create (builder, "ip-addresses");

  hyscan_data_schema_builder_enum_value_create (builder, "ip-addresses", 0,
                                                         "Disabled", _("Disabled"));

  for (i = 0; (ips != NULL) && (ips[i] != NULL); i++)
    {
      hyscan_data_schema_builder_enum_value_create (builder, "ip-addresses", i + 1,
                                                             ips[i], ips[i]);

      g_hash_table_insert (addresses, GINT_TO_POINTER (i + 1), g_strdup (ips[i]));
    }

  g_strfreev (ips);
}

/* Функция освобождает память, занятую структурой HyScanSensorControlLocalPort. */
static void
hyscan_sensor_control_free_local_port (gpointer data)
{
  HyScanSensorControlLocalPort *port = data;

  g_clear_object (&port->receiver);
  g_free (port);
}

/* Функция освобождает память, занятую структурой HyScanSensorControlPort. */
static void
hyscan_sensor_control_free_port (gpointer data)
{
  HyScanSensorControlPort *port = data;

  g_free (port->path);
  g_free (port->name);
  g_free (port);
}

/* Функция устанавливает параметры работы локального UART порта. */
static gboolean
hyscan_sensor_control_setup_uart_port (HyScanSensorControl *control,
                                       const gchar         *name,
                                       gboolean             enable)
{
  HyScanSensorControlLocalPort *local_port;

  local_port = g_hash_table_lookup (control->priv->local_ports, name);
  if ((local_port == NULL) || (local_port->type != HYSCAN_SENSOR_PORT_UART))
    return FALSE;

  local_port->enable = enable;

  if (!local_port->enable)
    {
      g_clear_object (&local_port->receiver);
      return TRUE;
    }

  if (local_port->receiver == NULL)
    {
      local_port->receiver = G_OBJECT (hyscan_nmea_uart_new (name));

      g_signal_connect_swapped (local_port->receiver, "nmea-data",
                                G_CALLBACK (hyscan_sensor_control_local_receiver), control);
    }

  return hyscan_nmea_uart_set_device (HYSCAN_NMEA_UART (local_port->receiver),
                                      local_port->uart.device, local_port->uart.mode);
}

/* Функция устанавливает параметры работы локального UDP порта. */
static gboolean
hyscan_sensor_control_setup_udp_port (HyScanSensorControl *control,
                                      const gchar         *name,
                                      gboolean             enable)
{
  HyScanSensorControlLocalPort *local_port;

  local_port = g_hash_table_lookup (control->priv->local_ports, name);
  if ((local_port == NULL) || (local_port->type != HYSCAN_SENSOR_PORT_UDP_IP))
    return FALSE;

  local_port->enable = enable;

  if (!local_port->enable)
    {
      g_clear_object (&local_port->receiver);
      return TRUE;
    }

  if (local_port->receiver == NULL)
    {
      local_port->receiver = G_OBJECT (hyscan_nmea_udp_new (name));

      g_signal_connect_swapped (local_port->receiver, "nmea-data",
                                G_CALLBACK (hyscan_sensor_control_local_receiver), control);
    }

  return hyscan_nmea_udp_set_address (HYSCAN_NMEA_UDP (local_port->receiver),
                                      local_port->udp.address, local_port->udp.port);
}

/* Функция записывает данные в систему хранения и отправляет сигнал. */
static void
hyscan_sensor_control_nmea_sender (HyScanSensorControl *control,
                                   guint                channel,
                                   gint64               time,
                                   gint64               offset,
                                   const gchar         *name,
                                   guint                size,
                                   const gchar         *nmea)
{
  HyScanDataWriterData data;
  gchar **nmeas;
  guint i;


  /* Запись NMEA строк в разные каналы по типу строки. */
  nmeas = g_strsplit (nmea, "\r\n", -1);
  for (i = 0; i < g_strv_length (nmeas); i++)
    {
      HyScanSourceType nmea_type;

      if (!hyscan_sensor_control_check_nmea_crc (nmeas[i]))
        continue;

      nmea_type = hyscan_sensor_control_get_source_type (nmeas[i]);
      if ((nmea_type != HYSCAN_SOURCE_NMEA_GGA) &&
          (nmea_type != HYSCAN_SOURCE_NMEA_RMC) &&
          (nmea_type != HYSCAN_SOURCE_NMEA_DPT))
        {
          continue;
        }

      data.time = time - offset;
      data.data = nmeas[i];
      data.size = strlen (nmeas[i]);
      hyscan_data_writer_sensor_add_data (HYSCAN_DATA_WRITER (control),
                                          name, nmea_type, channel, &data);
    }
  g_strfreev (nmeas);

  data.time = time - offset;
  data.data = nmea;
  data.size = size;
  hyscan_data_writer_sensor_add_data (HYSCAN_DATA_WRITER (control),
                                      name, HYSCAN_SOURCE_NMEA_ANY, channel, &data);

  data.time = time;
  data.data = nmea;
  data.size = size;
  g_signal_emit (control, hyscan_sensor_control_signals[SIGNAL_SENSOR_DATA], 0,
                 name, HYSCAN_SENSOR_PROTOCOL_NMEA_0183, HYSCAN_DATA_STRING, &data);
}

/* Функция обрабатывает принятые NMEA данные от локальных портов. */
static void
hyscan_sensor_control_local_receiver (HyScanSensorControl *control,
                                      gint64               time,
                                      const gchar         *name,
                                      guint                size,
                                      const gchar         *nmea)
{
  HyScanSensorControlLocalPort *local_port;

  local_port = g_hash_table_lookup (control->priv->local_ports, name);
  if (local_port == NULL)
    return;

  g_mutex_lock (&control->priv->lock);

  hyscan_sensor_control_nmea_sender (control, local_port->channel,
                                     time, local_port->time_offset,
                                     name, size, nmea);

  g_mutex_unlock (&control->priv->lock);
}

/* Функция обрабатывает сообщения с данными от гидролокатора. */
static void
hyscan_sensor_control_data_receiver (HyScanSensorControl *control,
                                     HyScanSonarMessage  *message)
{
  HyScanSensorControlPort *port;

  port = g_hash_table_lookup (control->priv->ports_by_id, GINT_TO_POINTER (message->id));
  if (port == NULL)
    return;

  g_mutex_lock (&control->priv->lock);

  /* Обработка только данных NMEA 0183. */
  if ((port->protocol == HYSCAN_SENSOR_PROTOCOL_NMEA_0183) &&
      (message->type == HYSCAN_DATA_STRING))
    {
      hyscan_sensor_control_nmea_sender (control, port->channel,
                                         message->time, port->time_offset,
                                         port->name, message->size, message->data);
    }

  g_mutex_unlock (&control->priv->lock);

}

/* Функция проверяет контрольную сумму NMEA сообщения. */
static gboolean
hyscan_sensor_control_check_nmea_crc (const gchar *nmea_str)
{
  guint32 nmea_crc;
  guchar crc = 0;
  gsize nmea_len = strlen (nmea_str);
  gsize i;

  /* NMEA строка не может быть короче 10 символов. */
  if (nmea_len < 10)
    return FALSE;

  nmea_crc = g_ascii_strtoull (nmea_str + nmea_len - 2, NULL, 16);
  for (i = 1; i < nmea_len - 3; i++)
    crc ^= nmea_str[i];

  if (nmea_crc != crc)
    return FALSE;

  return TRUE;
}

/* Функция возвращает список портов, к которым могут быть подключены датчики. */
gchar **
hyscan_sensor_control_list_ports (HyScanSensorControl *control)
{
  HyScanSensorControlPrivate *priv;
  gchar **list = NULL;

  GHashTableIter iter;
  gpointer name;
  guint n_local_ports;
  guint n_ports;
  guint i = 0;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  n_local_ports = g_hash_table_size (priv->local_ports);
  n_ports = g_hash_table_size (priv->ports_by_id);
  if ((n_local_ports == 0) && (n_ports == 0))
    return NULL;

  list = g_malloc0 (sizeof (gchar*) * (n_local_ports + n_ports + 1));

  g_hash_table_iter_init (&iter, priv->local_ports);
  while (g_hash_table_iter_next (&iter, &name, NULL))
    list[i++] = g_strdup (name);

  g_hash_table_iter_init (&iter, priv->ports_by_name);
  while (g_hash_table_iter_next (&iter, &name, NULL))
    list[i++] = g_strdup (name);

  return list;
}

/* Функция возвращает список физических устройств UART. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_uart_devices (HyScanSensorControl *control,
                                         const gchar         *name)
{
  HyScanSensorControlPrivate *priv;
  HyScanDataSchemaEnumValue **param_values = NULL;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;
  const gchar *values_id;
  gchar *param_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if ((local_port != NULL) && (local_port->type == HYSCAN_SENSOR_PORT_UART))
    return hyscan_data_schema_key_get_enum_values (priv->local_schema, "uart-devices");

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if ((port == NULL) || (port->type != HYSCAN_SENSOR_PORT_UART))
    return NULL;

  param_name = g_strdup_printf ("%s/uart-device", port->path);
  values_id = hyscan_data_schema_key_get_enum_id (priv->schema, param_name);
  if (values_id != NULL)
    param_values = hyscan_data_schema_key_get_enum_values (priv->schema, values_id);
  g_free (param_name);

  return param_values;
}

/* Функция возвращает список допустимых режимов обмена данными через UART устройство. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_uart_modes (HyScanSensorControl *control,
                                       const gchar         *name)
{
  HyScanSensorControlPrivate *priv;
  HyScanDataSchemaEnumValue **param_values = NULL;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;
  const gchar *values_id;
  gchar *param_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if ((local_port != NULL) && (local_port->type == HYSCAN_SENSOR_PORT_UART))
    return hyscan_data_schema_key_get_enum_values (priv->local_schema, "uart-modes");

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if ((port == NULL) || (port->type != HYSCAN_SENSOR_PORT_UART))
    return NULL;

  param_name = g_strdup_printf ("%s/uart-mode", port->path);
  values_id = hyscan_data_schema_key_get_enum_id (priv->schema, param_name);
  if (values_id != NULL)
    param_values = hyscan_data_schema_key_get_enum_values (priv->schema, values_id);
  g_free (param_name);

  return param_values;
}

/* Функция возвращает список допустимых IP адресов для портов типа IP. */
HyScanDataSchemaEnumValue **
hyscan_sensor_control_list_ip_addresses (HyScanSensorControl *control,
                                         const gchar         *name)
{
  HyScanSensorControlPrivate *priv;
  HyScanDataSchemaEnumValue **param_values = NULL;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;
  const gchar *values_id;
  gchar *param_name;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), NULL);

  priv = control->priv;

  if (priv->sonar == NULL)
    return NULL;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if ((local_port != NULL) && (local_port->type == HYSCAN_SENSOR_PORT_UDP_IP))
    return hyscan_data_schema_key_get_enum_values (priv->local_schema, "ip-addresses");

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if ((port == NULL) || (port->type != HYSCAN_SENSOR_PORT_UDP_IP))
    return NULL;

  param_name = g_strdup_printf ("%s/ip-address", port->path);
  values_id = hyscan_data_schema_key_get_enum_id (priv->schema, param_name);
  if (values_id != NULL)
    param_values = hyscan_data_schema_key_get_enum_values (priv->schema, values_id);
  g_free (param_name);

  return param_values;
}

/* Функция возвращает тип порта. */
HyScanSensorPortType
hyscan_sensor_control_get_port_type (HyScanSensorControl *control,
                                     const gchar         *name)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), HYSCAN_SENSOR_PORT_INVALID);

  priv = control->priv;

  if (priv->sonar == NULL)
    return HYSCAN_SENSOR_PORT_INVALID;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if (local_port != NULL)
    return local_port->type;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port != NULL)
    return port->type;

  return HYSCAN_SENSOR_PORT_INVALID;
}

/* Функция возвращает признак локального порта. */
gboolean
hyscan_sensor_control_is_port_local (HyScanSensorControl *control,
                                     const gchar         *name)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlLocalPort *local_port;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if (local_port != NULL)
    return TRUE;

  return FALSE;
}

/* Функция возвращает текущее состояние порта. */
HyScanSensorPortStatus
hyscan_sensor_control_get_port_status (HyScanSensorControl *control,
                                       const gchar         *name)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;

  gint64 port_status;
  gchar *param_name;
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), HYSCAN_SENSOR_PORT_STATUS_INVALID);

  priv = control->priv;

  if (priv->sonar == NULL)
    return HYSCAN_SENSOR_PORT_STATUS_INVALID;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if (local_port != NULL)
    {
      port_status = HYSCAN_SENSOR_PORT_STATUS_INVALID;

      g_mutex_lock (&priv->lock);

      if (local_port->type == HYSCAN_SENSOR_PORT_UART)
        {
          if (local_port->receiver != NULL)
            port_status = hyscan_nmea_uart_get_status (HYSCAN_NMEA_UART (local_port->receiver));
          else
            port_status = HYSCAN_SENSOR_PORT_STATUS_DISABLED;
        }

      if (local_port->type == HYSCAN_SENSOR_PORT_UDP_IP)
        {
          if (local_port->receiver != NULL)
            port_status = hyscan_nmea_udp_get_status (HYSCAN_NMEA_UDP (local_port->receiver));
          else
            port_status = HYSCAN_SENSOR_PORT_STATUS_DISABLED;
        }

      g_mutex_unlock (&priv->lock);

      return port_status;
    }

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return HYSCAN_SENSOR_PORT_STATUS_INVALID;

  param_name = g_strdup_printf ("%s/status", port->path);
  status = hyscan_param_get_enum (priv->sonar, param_name, &port_status);
  g_free (param_name);

  if (!status)
    port_status = HYSCAN_SENSOR_PORT_STATUS_INVALID;

  return port_status;
}

/* Функция устанавливает режим работы порта типа VIRTUAL. */
gboolean
hyscan_sensor_control_set_virtual_port_param (HyScanSensorControl     *control,
                                              const gchar             *name,
                                              guint                    channel,
                                              gint64                   time_offset)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gchar *param_names[3];
  GVariant *param_values[3];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 1 || channel > HYSCAN_SENSOR_CONTROL_MAX_CHANNELS)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_VIRTUAL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  param_names[0] = g_strdup_printf ("%s/channel", port->path);
  param_names[1] = g_strdup_printf ("%s/time-offset", port->path);
  param_names[2] = NULL;

  param_values[0] = g_variant_new_int64 (channel);
  param_values[1] = g_variant_new_int64 (time_offset);
  param_values[2] = NULL;

  status = hyscan_param_set (priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);

  if (status)
    {
      port->channel = channel;
      port->time_offset = time_offset;
    }

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает режим работы порта типа UART. */
gboolean
hyscan_sensor_control_set_uart_port_param (HyScanSensorControl      *control,
                                           const gchar              *name,
                                           guint                     channel,
                                           gint64                    time_offset,
                                           HyScanSensorProtocolType  protocol,
                                           guint                     uart_device,
                                           guint                     uart_mode)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;

  gchar *param_names[6];
  GVariant *param_values[6];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 1 || channel > HYSCAN_SENSOR_CONTROL_MAX_CHANNELS)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS && protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if ((local_port != NULL) && (local_port->type == HYSCAN_SENSOR_PORT_UART))
    {
      if (protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
        return FALSE;

      g_mutex_lock (&priv->lock);

      local_port->channel = channel;
      local_port->time_offset = time_offset;
      local_port->uart.device = g_hash_table_lookup (priv->uart_devices, GINT_TO_POINTER (uart_device));
      local_port->uart.mode = uart_mode;
      status = hyscan_sensor_control_setup_uart_port (control, name, local_port->enable);

      g_mutex_unlock (&priv->lock);

      return status;
    }

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_UART)
    return FALSE;

  g_mutex_lock (&priv->lock);

  param_names[0] = g_strdup_printf ("%s/protocol", port->path);
  param_names[1] = g_strdup_printf ("%s/uart-device", port->path);
  param_names[2] = g_strdup_printf ("%s/uart-mode", port->path);
  param_names[3] = g_strdup_printf ("%s/channel", port->path);
  param_names[4] = g_strdup_printf ("%s/time-offset", port->path);
  param_names[5] = NULL;

  param_values[0] = g_variant_new_int64 (protocol);
  param_values[1] = g_variant_new_int64 (uart_device);
  param_values[2] = g_variant_new_int64 (uart_mode);
  param_values[3] = g_variant_new_int64 (channel);
  param_values[4] = g_variant_new_int64 (time_offset);
  param_values[5] = NULL;

  status = hyscan_param_set (priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
      g_variant_unref (param_values[4]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);
  g_free (param_names[3]);
  g_free (param_names[4]);

  if (status)
    {
      port->channel = channel;
      port->protocol = protocol;
      port->time_offset = time_offset;
    }

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает режим работы порта типа UDP/IP. */
gboolean
hyscan_sensor_control_set_udp_ip_port_param (HyScanSensorControl      *control,
                                             const gchar              *name,
                                             guint                     channel,
                                             gint64                    time_offset,
                                             HyScanSensorProtocolType  protocol,
                                             guint                     ip_address,
                                             guint16                   udp_port)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;

  gchar *param_names[6];
  GVariant *param_values[6];
  gboolean status;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  if (channel < 1 || channel > HYSCAN_SENSOR_CONTROL_MAX_CHANNELS)
    return FALSE;

  if (time_offset < 0)
    return FALSE;

  if (protocol != HYSCAN_SENSOR_PROTOCOL_SAS && protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
    return FALSE;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if ((local_port != NULL) && (local_port->type == HYSCAN_SENSOR_PORT_UDP_IP))
    {
      if (protocol != HYSCAN_SENSOR_PROTOCOL_NMEA_0183)
        return FALSE;

      g_mutex_lock (&priv->lock);

      local_port->channel = channel;
      local_port->time_offset = time_offset;
      local_port->udp.address = g_hash_table_lookup (priv->ip_addresses, GINT_TO_POINTER (ip_address));
      local_port->udp.port = udp_port;
      status = hyscan_sensor_control_setup_udp_port (control, name, local_port->enable);

      g_mutex_unlock (&priv->lock);

      return status;
    }

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    return FALSE;

  if (port->type != HYSCAN_SENSOR_PORT_UDP_IP)
    return FALSE;

  g_mutex_lock (&priv->lock);

  param_names[0] = g_strdup_printf ("%s/protocol", port->path);
  param_names[1] = g_strdup_printf ("%s/ip-address", port->path);
  param_names[2] = g_strdup_printf ("%s/udp-port", port->path);
  param_names[3] = g_strdup_printf ("%s/channel", port->path);
  param_names[4] = g_strdup_printf ("%s/time-offset", port->path);
  param_names[5] = NULL;

  param_values[0] = g_variant_new_int64 (protocol);
  param_values[1] = g_variant_new_int64 (ip_address);
  param_values[2] = g_variant_new_int64 (udp_port);
  param_values[3] = g_variant_new_int64 (channel);
  param_values[4] = g_variant_new_int64 (time_offset);
  param_values[5] = NULL;

  status = hyscan_param_set (priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
      g_variant_unref (param_values[4]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);
  g_free (param_names[3]);
  g_free (param_names[4]);

  if (status)
    {
      port->channel = channel;
      port->protocol = protocol;
      port->time_offset = time_offset;
    }

  g_mutex_unlock (&priv->lock);

  return status;
}

/* Функция устанавливает информацию о местоположении приёмных антенн. */
gboolean
hyscan_sensor_control_set_position (HyScanSensorControl   *control,
                                    const gchar           *name,
                                    HyScanAntennaPosition *position)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlLocalPort *local_port;
  HyScanSensorControlPort *port;

  gchar *param_names[7];
  GVariant *param_values[7];
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  local_port = g_hash_table_lookup (priv->local_ports, name);
  if (local_port != NULL)
    {
      status = TRUE;
      goto exit;
    }

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    goto exit;

  g_mutex_lock (&priv->lock);

  param_names[0] = g_strdup_printf ("%s/position/x", port->path);
  param_names[1] = g_strdup_printf ("%s/position/y", port->path);
  param_names[2] = g_strdup_printf ("%s/position/z", port->path);
  param_names[3] = g_strdup_printf ("%s/position/psi", port->path);
  param_names[4] = g_strdup_printf ("%s/position/gamma", port->path);
  param_names[5] = g_strdup_printf ("%s/position/theta", port->path);
  param_names[6] = NULL;

  param_values[0] = g_variant_new_double (position->x);
  param_values[1] = g_variant_new_double (position->y);
  param_values[2] = g_variant_new_double (position->z);
  param_values[3] = g_variant_new_double (position->psi);
  param_values[4] = g_variant_new_double (position->gamma);
  param_values[5] = g_variant_new_double (position->theta);
  param_values[6] = NULL;

  status = hyscan_param_set (priv->sonar, (const gchar **)param_names, param_values);

  if (!status)
    {
      g_variant_unref (param_values[0]);
      g_variant_unref (param_values[1]);
      g_variant_unref (param_values[2]);
      g_variant_unref (param_values[3]);
      g_variant_unref (param_values[4]);
      g_variant_unref (param_values[5]);
    }

  g_free (param_names[0]);
  g_free (param_names[1]);
  g_free (param_names[2]);
  g_free (param_names[3]);
  g_free (param_names[4]);
  g_free (param_names[5]);

  g_mutex_unlock (&priv->lock);

exit:
  if (status)
    hyscan_data_writer_sensor_set_position (HYSCAN_DATA_WRITER (control), name, position);

  return status;
}

/* Функция включает или выключает приём данных на указанном порту. */
gboolean
hyscan_sensor_control_set_enable (HyScanSensorControl *control,
                                  const gchar         *name,
                                  gboolean             enable)
{
  HyScanSensorControlPrivate *priv;
  HyScanSensorControlPort *port;

  gchar *param_name;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_SENSOR_CONTROL (control), FALSE);

  priv = control->priv;

  if (priv->sonar == NULL)
    return FALSE;

  g_mutex_lock (&priv->lock);

  status = hyscan_sensor_control_setup_uart_port (control, name, enable);
  if (status)
    goto exit;

  status = hyscan_sensor_control_setup_udp_port (control, name, enable);
  if (status)
    goto exit;

  port = g_hash_table_lookup (priv->ports_by_name, name);
  if (port == NULL)
    goto exit;

  param_name = g_strdup_printf ("%s/enable", port->path);
  status = hyscan_param_set_boolean (priv->sonar, param_name, enable);
  g_free (param_name);

exit:
  g_mutex_unlock (&priv->lock);

  return status;
}
