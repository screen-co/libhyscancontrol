/*
 * \file hyscan-nmea-udp.c
 *
 * \brief Исходный файл класса приёма NMEA данных через UDP порты
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-nmea-udp.h"
#include "hyscan-control-marshallers.h"

#include <hyscan-slice-pool.h>
#include <gio/gio.h>
#include <stdio.h>

#ifdef G_OS_UNIX
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

#define N_BUFFERS 16
#define MAX_MSG_SIZE 4000

enum
{
  PROP_O,
  PROP_NAME,
  PROP_N_BUFFERS
};

enum
{
  SIGNAL_NMEA_DATA,
  SIGNAL_LAST
};

typedef struct
{
  gint64                   time;               /* Время приёма сообщения. */
  guint                    size;               /* Размер сообщения. */
  gchar                    nmea[MAX_MSG_SIZE]; /* Данные. */
} HyScanNmeaUDPMessage;

struct _HyScanNmeaUDPPrivate
{
  gchar                       *name;           /* Название порта. */

  GThread                     *receiver;       /* Поток приёма данных. */
  GThread                     *emmiter;        /* Поток отправки данных. */

  gint                         started;        /* Признак работы потока приёма данных. */
  gint                         configure;      /* Признак режима конфигурации UART порта. */
  gint                         terminate;      /* Признак необходимости завершения работы. */

  GSocket                     *socket;         /* Сокет для приёма данных по UDP. */
  GAsyncQueue                 *queue;          /* Очередь сообщений для отправки клиенту. */

  HyScanSlicePool             *buffers;        /* Список буферов приёма данных. */
  GRWLock                      lock;           /* Блокировка доступа к списку буферов. */
};

static void            hyscan_nmea_udp_set_property            (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void            hyscan_nmea_udp_object_constructed      (GObject               *object);
static void            hyscan_nmea_udp_object_finalize         (GObject               *object);

static gboolean        hyscan_nmea_udp_check_nmea              (gchar                 *nmea,
                                                                guint                  size);

static gpointer        hyscan_nmea_udp_receiver                (gpointer               user_data);
static gpointer        hyscan_nmea_udp_emmiter                 (gpointer               user_data);

static guint           hyscan_nmea_udp_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNmeaUDP, hyscan_nmea_udp, G_TYPE_OBJECT)

static void
hyscan_nmea_udp_class_init (HyScanNmeaUDPClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nmea_udp_set_property;

  object_class->constructed = hyscan_nmea_udp_object_constructed;
  object_class->finalize = hyscan_nmea_udp_object_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Name", "UDP port name", NULL,
                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_nmea_udp_signals[SIGNAL_NMEA_DATA] =
    g_signal_new ("nmea-data", HYSCAN_TYPE_NMEA_UDP, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__INT64_STRING_UINT_STRING,
                  G_TYPE_NONE,
                  4, G_TYPE_INT64, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING);
}

static void
hyscan_nmea_udp_init (HyScanNmeaUDP *udp)
{
  udp->priv = hyscan_nmea_udp_get_instance_private (udp);
}

static void
hyscan_nmea_udp_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanNmeaUDP *udp = HYSCAN_NMEA_UDP (object);
  HyScanNmeaUDPPrivate *priv = udp->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_nmea_udp_object_constructed (GObject *object)
{
  HyScanNmeaUDP *udp = HYSCAN_NMEA_UDP (object);
  HyScanNmeaUDPPrivate *priv = udp->priv;

  guint i;

  /* Буферы приёма данных. */
  for (i = 0; i < N_BUFFERS; i++)
    hyscan_slice_pool_push (&priv->buffers, g_new (HyScanNmeaUDPMessage, 1));

  priv->started = 1;

  priv->queue = g_async_queue_new_full (g_free);
  priv->receiver = g_thread_new ("udp-receiver", hyscan_nmea_udp_receiver, priv);
  priv->emmiter = g_thread_new ("udp-emmiter", hyscan_nmea_udp_emmiter, udp);
}

static void
hyscan_nmea_udp_object_finalize (GObject *object)
{
  HyScanNmeaUDP *udp = HYSCAN_NMEA_UDP (object);
  HyScanNmeaUDPPrivate *priv = udp->priv;
  gpointer buffer;

  g_atomic_int_set (&priv->terminate, 1);
  g_thread_join (priv->receiver);
  g_thread_join (priv->emmiter);

  g_clear_object (&priv->socket);
  g_free (priv->name);

  g_async_queue_unref (priv->queue);
  while ((buffer = hyscan_slice_pool_pop (&priv->buffers)) != NULL)
    g_free (buffer);

  G_OBJECT_CLASS (hyscan_nmea_udp_parent_class)->finalize (object);
}

/* Функция проверяет блок NMEA строк. */
static gboolean
hyscan_nmea_udp_check_nmea (gchar    *nmea,
                            guint     size)
{
  gboolean is_nmea = FALSE;

  gboolean bad_crc;
  gchar *string_end;
  guint string_length;
  guint offset = 0;
  guchar nmea_crc1;
  guint nmea_crc2;
  guint i;

  while (TRUE)
    {
      /* Ищем контрольную сумму в строке. */
      string_end = g_strstr_len (nmea + offset, size - offset, "*");
      if (string_end == NULL)
        break;

      /* Считаем контрольную сумму строки. */
      string_length = string_end - nmea - offset;

      for (nmea_crc1 = 0, i = 1; i < string_length; i++)
        nmea_crc1 ^= nmea[offset + i];

      /* Проверяем контрольную сумму NMEA строки. */
      bad_crc = FALSE;
      if ((sscanf (string_end, "*%02X", &nmea_crc2) != 1) || (nmea_crc1 != nmea_crc2))
        bad_crc = TRUE;

      /* Нашли хотя бы одну валидную NMEA строку. */
      if (!bad_crc)
        is_nmea = TRUE;

      /* Переход на начало следующей строки. */
      offset += string_length + 5;
      if (offset >= size)
        break;
    }

  return is_nmea;
}

/* Поток приёма данных. */
static gpointer
hyscan_nmea_udp_receiver (gpointer user_data)
{
  HyScanNmeaUDPPrivate *priv = user_data;

  HyScanNmeaUDPMessage *buffer;
  gint64 data_time;
  gssize data_size;

  gchar null[1024];

  while (g_atomic_int_get (&priv->terminate) == 0)
    {
      /* Режим конфигурации. */
      if (g_atomic_int_get (&priv->configure) == 1)
        {
          g_clear_object (&priv->socket);

          /* Ждём завершения конфигурации. */
          g_atomic_int_set (&priv->started, 0);
          g_usleep (100000);
          continue;
        }

      /* Конфигурация завершена. */
      else
        {
          /* Адрес не установлен. */
          if (priv->socket == NULL)
            {
              g_usleep (100000);
              continue;
            }

          /* Ожидаем данные. */
          if (!g_socket_condition_timed_wait (priv->socket, G_IO_IN, 100000, NULL, NULL))
            continue;

          data_time = g_get_monotonic_time ();

          /* Выбор буфера приёма. */
          g_rw_lock_writer_lock (&priv->lock);
          buffer = hyscan_slice_pool_pop (&priv->buffers);
          g_rw_lock_writer_unlock (&priv->lock);

          if (buffer == NULL)
            {
              g_socket_receive (priv->socket, null, sizeof (null), NULL, NULL);
              continue;
            }

          /* Приём данных. */
          data_size = g_socket_receive (priv->socket, buffer->nmea,
                                        sizeof (buffer->nmea) - 1, NULL, NULL);
          if (data_size <= 0)
            {
              g_rw_lock_writer_lock (&priv->lock);
              hyscan_slice_pool_push (&priv->buffers, buffer);
              g_rw_lock_writer_unlock (&priv->lock);
              continue;
            }

          buffer->time = data_time;
          buffer->size = data_size;
          buffer->nmea[data_size] = 0;

          /* Если есть валидные NMEA строки - помещяем в очередь отправки клиенту. */
          if (hyscan_nmea_udp_check_nmea (buffer->nmea, data_size))
            {
              g_async_queue_push (priv->queue, buffer);
            }
          else
            {
              g_rw_lock_writer_lock (&priv->lock);
              hyscan_slice_pool_push (&priv->buffers, buffer);
              g_rw_lock_writer_unlock (&priv->lock);
            }
        }
    }

  return NULL;
}

/* Поток отправки данных клиенту. */
static gpointer
hyscan_nmea_udp_emmiter (gpointer user_data)
{
  HyScanNmeaUDP *udp = user_data;
  HyScanNmeaUDPPrivate *priv = udp->priv;
  HyScanNmeaUDPMessage *buffer;

  while (g_atomic_int_get (&priv->terminate) == 0)
    {
      buffer = g_async_queue_timeout_pop (priv->queue, 100000);
      if (buffer == NULL)
        continue;

      g_signal_emit (udp, hyscan_nmea_udp_signals[SIGNAL_NMEA_DATA], 0,
                     buffer->time, priv->name, buffer->size, buffer->nmea);

      g_rw_lock_writer_lock (&priv->lock);
      hyscan_slice_pool_push (&priv->buffers, buffer);
      g_rw_lock_writer_unlock (&priv->lock);
    }

  return NULL;
}

/* Функция создаёт новый объект HyScanNmeaUDP. */
HyScanNmeaUDP *
hyscan_nmea_udp_new (const gchar *name)
{
  return g_object_new (HYSCAN_TYPE_NMEA_UDP, "name", name, NULL);
}

/* Функция устанавливает IP адрес и номер UDP порта для приёма данных. */
gboolean
hyscan_nmea_udp_set_address (HyScanNmeaUDP *udp,
                             const gchar   *ip,
                             guint16        port)
{
  HyScanNmeaUDPPrivate *priv;

  GSocketAddress *address = NULL;
  gboolean status = FALSE;

  g_return_val_if_fail (HYSCAN_IS_NMEA_UDP (udp), FALSE);

  priv = udp->priv;

  /* Переходим в режим конфигурации. */
  while (!g_atomic_int_compare_and_exchange (&priv->configure, 0, 1))
    g_usleep (10000);
  while (g_atomic_int_get (&priv->started) == 1)
    g_usleep (10000);

  /* Устройство отключено. */
  if (ip == NULL || port < 1024)
    {
      status = TRUE;
      goto exit;
    }

  /* Закрываем предыдущий сокет приёма данных. */
  g_clear_object (&priv->socket);

  /* Адрес подключения. */
  address = g_inet_socket_address_new_from_string (ip, port);
  if (address == NULL)
    goto exit;

  /* Сокет приёма данных. */
  priv->socket = g_socket_new (g_socket_address_get_family (address),
                               G_SOCKET_TYPE_DATAGRAM,
                               G_SOCKET_PROTOCOL_DEFAULT,
                               NULL);
  if (priv->socket == NULL)
    goto exit;

  status = g_socket_bind (priv->socket, address, FALSE, NULL);

exit:
  g_clear_object (&address);

  /* Завершаем конфигурацию. */
  g_atomic_int_set (&priv->started, 1);
  g_atomic_int_set (&priv->configure, 0);

  return status;
}

/* Функция возвращает текущее состояние приёма данных. */
HyScanSensorPortStatus
hyscan_nmea_udp_get_status (HyScanNmeaUDP *udp)
{
  return HYSCAN_SENSOR_PORT_STATUS_INVALID;
}

/* Функция возвращает список IP адресов доступных в системе. */
gchar **
hyscan_nmea_udp_list_addresses (void)
{
#ifdef G_OS_UNIX
  struct ifaddrs *ifap, *ifa;
  GPtrArray *list;

  if (getifaddrs (&ifap) != 0)
    return NULL;

  list = g_ptr_array_new ();
  for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr == NULL)
        continue;

      if (ifa->ifa_addr->sa_family == AF_INET)
        {
          struct sockaddr_in *sa;
          sa = (struct sockaddr_in *) ifa->ifa_addr;
          g_ptr_array_add (list, g_strdup (inet_ntoa (sa->sin_addr)));
        }
    }
  freeifaddrs(ifap);

  if (list->len > 0)
    {
      gchar **addresses;

      g_ptr_array_add (list, NULL);
      addresses = (gpointer)list->pdata;
      g_ptr_array_free (list, FALSE);

      return addresses;
    }

  return NULL;
#endif

#ifdef G_OS_WIN32
  DWORD ret, paa_size;
  PIP_ADAPTER_ADDRESSES paa, caa;
  GPtrArray *list;

  paa_size = 65536;
  paa = g_malloc (paa_size);

  do
    {
      if (paa == NULL)
        paa = g_malloc (paa_size);

      ret = GetAdaptersAddresses (AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, paa, &paa_size);

      if (ret == ERROR_BUFFER_OVERFLOW)
        g_clear_pointer (&paa, g_free);
    }
  while (ret == ERROR_BUFFER_OVERFLOW);

  if (ret != NO_ERROR)
    return NULL;

  list = g_ptr_array_new ();
  for (caa = paa; caa != NULL; caa = caa->Next)
    {
      PIP_ADAPTER_UNICAST_ADDRESS ua;

      for (ua = caa->FirstUnicastAddress; ua != NULL; ua = ua->Next)
        {
          gchar address [BUFSIZ] = {0};

          if (getnameinfo (ua->Address.lpSockaddr, ua->Address.iSockaddrLength,
                           address, BUFSIZ, NULL, 0, NI_NUMERICHOST) == 0)
            {
              g_ptr_array_add (list, g_strdup (address));
            }
        }
    }
  g_free (paa);

  if (list->len > 0)
    {
      gchar **addresses;

      g_ptr_array_add (list, NULL);
      addresses = (gpointer)list->pdata;
      g_ptr_array_free (list, FALSE);

      return addresses;
    }

  g_ptr_array_free (list, TRUE);

  return NULL;
#endif
}
