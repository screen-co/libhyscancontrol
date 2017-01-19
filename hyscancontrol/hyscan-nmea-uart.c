/*
 * \file hyscan-nmea-uart-posix.c
 *
 * \brief Исходный файл класса приёма NMEA данных через UART порты для POSIX систем
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-nmea-uart.h"
#include "hyscan-control-marshallers.h"

#include <hyscan-slice-pool.h>
#include <stdio.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#define HANDLE gint
#define INVALID_HANDLE_VALUE -1
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <tchar.h>

#define sscanf sscanf_s
#endif

#define N_BUFFERS 16
#define MAX_MSG_SIZE 4000

enum
{
  PROP_O,
  PROP_NAME,
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
} HyScanNmeaUARTMessage;

struct _HyScanNmeaUARTPrivate
{
  gchar                       *name;           /* Название порта. */

  GThread                     *receiver;       /* Поток приёма данных. */
  GThread                     *emmiter;        /* Поток отправки данных. */

  gint                         started;        /* Признак работы потока приёма данных. */
  gint                         configure;      /* Признак режима конфигурации UART порта. */
  gint                         terminate;      /* Признак необходимости завершения работы. */
  gboolean                     skip_broken;    /* Признак необходимости пропуска "плохих" NMEA строк. */

  gchar                       *path;           /* Путь к файлу устройства. */
  HANDLE                       fd;             /* Дескриптор открытого порта. */
  HyScanNmeaUARTMode           mode;           /* Текущий режим работы порта. */
  GAsyncQueue                 *queue;          /* Очередь сообщений для отправки клиенту. */

  HyScanSlicePool             *buffers;        /* Список буферов приёма данных. */
  GRWLock                      lock;           /* Блокировка доступа к списку буферов. */
};

static void            hyscan_nmea_uart_set_property           (GObject               *object,
                                                                guint                  prop_id,
                                                                const GValue          *value,
                                                                GParamSpec            *pspec);
static void            hyscan_nmea_uart_object_constructed     (GObject               *object);
static void            hyscan_nmea_uart_object_finalize        (GObject               *object);

static HANDLE          hyscan_nmea_uart_open                   (const gchar           *path);
static void            hyscan_nmea_uart_close                  (HANDLE                 fd);

static gboolean        hyscan_nmea_uart_set_mode               (HANDLE                 fd,
                                                                HyScanNmeaUARTMode     mode);

static gchar           hyscan_nmea_uart_read                   (HANDLE                 fd);

static gpointer        hyscan_nmea_uart_receiver               (gpointer               user_data);
static gpointer        hyscan_nmea_uart_emmiter                (gpointer               user_data);

static guint           hyscan_nmea_uart_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanNmeaUART, hyscan_nmea_uart, G_TYPE_OBJECT)

static void
hyscan_nmea_uart_class_init (HyScanNmeaUARTClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_nmea_uart_set_property;

  object_class->constructed = hyscan_nmea_uart_object_constructed;
  object_class->finalize = hyscan_nmea_uart_object_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Name", "UART port name", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  hyscan_nmea_uart_signals[SIGNAL_NMEA_DATA] =
    g_signal_new ("nmea-data", HYSCAN_TYPE_NMEA_UART, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_user_marshal_VOID__INT64_STRING_UINT_STRING,
                  G_TYPE_NONE,
                  4, G_TYPE_INT64, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING);
}

static void
hyscan_nmea_uart_init (HyScanNmeaUART *uart)
{
  uart->priv = hyscan_nmea_uart_get_instance_private (uart);
}

static void
hyscan_nmea_uart_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  HyScanNmeaUART *uart = HYSCAN_NMEA_UART (object);
  HyScanNmeaUARTPrivate *priv = uart->priv;

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
hyscan_nmea_uart_object_constructed (GObject *object)
{
  HyScanNmeaUART *uart = HYSCAN_NMEA_UART (object);
  HyScanNmeaUARTPrivate *priv = uart->priv;

  guint i;

  g_rw_lock_init (&priv->lock);

  /* Буферы приёма данных. */
  for (i = 0; i < N_BUFFERS; i++)
    hyscan_slice_pool_push (&priv->buffers, g_new (HyScanNmeaUARTMessage, 1));

  priv->started = 1;
  priv->fd = INVALID_HANDLE_VALUE;

  priv->queue = g_async_queue_new_full (g_free);
  priv->receiver = g_thread_new ("uart-receiver", hyscan_nmea_uart_receiver, uart);
  priv->emmiter = g_thread_new ("uart-emmiter", hyscan_nmea_uart_emmiter, uart);
}

static void
hyscan_nmea_uart_object_finalize (GObject *object)
{
  HyScanNmeaUART *uart = HYSCAN_NMEA_UART (object);
  HyScanNmeaUARTPrivate *priv = uart->priv;
  gpointer buffer;

  g_atomic_int_set (&priv->terminate, 1);
  g_thread_join (priv->receiver);
  g_thread_join (priv->emmiter);

  if (priv->fd != INVALID_HANDLE_VALUE)
    hyscan_nmea_uart_close (priv->fd);

  g_free (priv->path);
  g_free (priv->name);

  g_async_queue_unref (priv->queue);
  while ((buffer = hyscan_slice_pool_pop (&priv->buffers)) != NULL)
    g_free (buffer);

  g_rw_lock_clear (&priv->lock);

  G_OBJECT_CLASS (hyscan_nmea_uart_parent_class)->finalize (object);
}

/* UNIX версии функций работы с uart портами. */
#ifdef G_OS_UNIX

/* Функция открывает uart порт. */
static HANDLE
hyscan_nmea_uart_open (const gchar *path)
{
  return open (path, O_RDONLY | O_NOCTTY | O_NDELAY | O_NONBLOCK);
}

/* Функция закрывает uart порт. */
static void
hyscan_nmea_uart_close (HANDLE fd)
{
  close (fd);
}

/* Функция устанавливает режим работы UART порта - unix версия. */
static gboolean
hyscan_nmea_uart_set_mode (HANDLE             fd,
                           HyScanNmeaUARTMode mode)
{
  struct termios options = {0};

  /* Выбор режима работы. */
  switch (mode)
    {
    case HYSCAN_NMEA_UART_MODE_4800_8N1:
      options.c_cflag = B4800;
      break;

    case HYSCAN_NMEA_UART_MODE_9600_8N1:
      options.c_cflag = B9600;
      break;

    case HYSCAN_NMEA_UART_MODE_19200_8N1:
      options.c_cflag = B19200;
      break;

    case HYSCAN_NMEA_UART_MODE_38400_8N1:
      options.c_cflag = B38400;
      break;

    case HYSCAN_NMEA_UART_MODE_57600_8N1:
      options.c_cflag = B57600;
      break;

    case HYSCAN_NMEA_UART_MODE_115200_8N1:
      options.c_cflag = B115200;
      break;

    default:
      return FALSE;
    }

  /* Устанавливаем параметры устройства. */
  cfmakeraw (&options);
  if ((tcflush (fd, TCIFLUSH) != 0) || (tcsetattr (fd, TCSANOW, &options) != 0))
    return FALSE;

  return TRUE;
}

/* Функция считывает доступные данные из uart порта. */
static gchar
hyscan_nmea_uart_read (HANDLE fd)
{
  fd_set set;
  struct timeval tv;
  gchar data;

  /* Ожидаем новые данные в течение 100 мс. */
  FD_ZERO (&set);
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  FD_SET (fd, &set);
  if (select (fd + 1, &set, NULL, NULL, &tv) <= 0)
    return  0;

  /* Считываем данные. */
  if (read (fd, &data, 1) <= 0)
    return 0;

  return data;
}
#endif

/* Windows версии функций работы с uart портами. */
#ifdef G_OS_WIN32
/* Функция открывает uart порт. */
static HANDLE
hyscan_nmea_uart_open (const gchar *path)
{
  HANDLE fd;
  COMMTIMEOUTS cto = { 0 };

  fd = CreateFile (path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

  cto.ReadIntervalTimeout = MAXDWORD;
  cto.ReadTotalTimeoutMultiplier = MAXDWORD;
  cto.ReadTotalTimeoutConstant = 100;
  if (!SetCommTimeouts(fd, &cto))
    {
      CloseHandle (fd);
      return INVALID_HANDLE_VALUE;
    }

  return fd;
}

/* Функция закрывает uart порт. */
static void
hyscan_nmea_uart_close (HANDLE fd)
{
  CloseHandle (fd);
}

/* Функция устанавливает режим работы UART порта. */
static gboolean
hyscan_nmea_uart_set_mode (HANDLE             fd,
                           HyScanNmeaUARTMode mode)
{
  DCB dcb = {0};

  /* Выбор режима работы. */
  switch (mode)
    {
    case HYSCAN_NMEA_UART_MODE_4800_8N1:
      BuildCommDCB ("4800,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_9600_8N1:
      BuildCommDCB ("9600,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_19200_8N1:
      BuildCommDCB ("19200,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_38400_8N1:
      BuildCommDCB ("38400,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_57600_8N1:
      BuildCommDCB ("57600,n,8,1", &dcb);
      break;

    case HYSCAN_NMEA_UART_MODE_115200_8N1:
      BuildCommDCB ("115200,n,8,1", &dcb);
      break;

    default:
      return FALSE;
    }

  if (!SetCommState (fd, &dcb))
    return FALSE;

  return TRUE;
}

/* Функция считывает доступные данные из uart порта. */
static gchar
hyscan_nmea_uart_read (HANDLE fd)
{
  gchar data;
  DWORD readed = -1;

  if (!ReadFile (fd, &data, 1, &readed, NULL) || (readed == 0))
    return 0;

  return data;
}
#endif

/* Поток приёма данных. */
static gpointer
hyscan_nmea_uart_receiver (gpointer user_data)
{
  HyScanNmeaUART *uart = user_data;
  HyScanNmeaUARTPrivate *priv = uart->priv;

  HyScanNmeaUARTMode cur_mode = HYSCAN_NMEA_UART_MODE_DISABLED;
  GTimer *timer;

  gchar rx_data;

  gchar data[MAX_MSG_SIZE];
  gint data_length = 0;
  gint64 data_time = 0;

  gchar string[MAX_MSG_SIZE / 4];
  guint string_length = 0;
  gint64 string_time = 0;

  HyScanNmeaUARTMessage *buffer;

  timer = g_timer_new ();

  while (g_atomic_int_get (&priv->terminate) == 0)
    {
      /* Режим конфигурации. */
      if (g_atomic_int_get (&priv->configure) == 1)
        {
          /* Восстанавливаем режим работы и закрываем устройство. */
          if (priv->fd != INVALID_HANDLE_VALUE)
            {
              g_clear_pointer (&priv->path, g_free);
              hyscan_nmea_uart_close (priv->fd);
              priv->fd = INVALID_HANDLE_VALUE;
            }

          /* Ждём завершения конфигурации. */
          g_atomic_int_set (&priv->started, 0);
          g_usleep (100000);
          continue;
        }

      /* Конфигурация завершена. */
      else
        {
          /* Устройство не выбрано. */
          if (priv->fd == INVALID_HANDLE_VALUE)
            {
              g_usleep (100000);
              continue;
            }

          /* В автоматическом режиме переключаем режимы работы каждые 2 секунды,
           * до тех пор пока не найдём рабочий. */
          if ((priv->mode == HYSCAN_NMEA_UART_MODE_AUTO) &&
              (g_timer_elapsed (timer, NULL) > 2.0))
            {
              if ((cur_mode == HYSCAN_NMEA_UART_MODE_DISABLED) ||
                  (cur_mode == HYSCAN_NMEA_UART_MODE_115200_8N1))
                {
                  cur_mode = HYSCAN_NMEA_UART_MODE_4800_8N1;
                }
              else
                {
                  cur_mode += 1;
                }

              hyscan_nmea_uart_set_mode (priv->fd, cur_mode);

              g_timer_start (timer);
            }
        }

      /* Пытаемся прочитать данные из порта. */
      rx_data = hyscan_nmea_uart_read (priv->fd);
      if (rx_data == 0)
        continue;

      /* Фиксируем время приёма. */
      if (data_time == 0)
        data_time = g_get_monotonic_time ();

      /* Текущая обрабатываемая строка пустая и данные не являются началом строки. */
      if (string_length == 0 && rx_data != '$')
        continue;

      /* Собираем строку до тех пор пока не встретится символ '\r'. */
      if (rx_data != '\r')
        {
          /* Если строка слишком длинная, пропускаем её. */
          if (string_length > sizeof (string) - 2)
            {
              string_length = 0;
              continue;
            }

          /* Сохраняем текущий символ. */
          string [string_length++] = rx_data;
          string [string_length] = 0;
          continue;
        }

      /* Строка собрана. */
      else
        {
          gboolean send_block = FALSE;
          gboolean bad_crc = FALSE;
          guchar nmea_crc1;
          guint nmea_crc2;
          guint i, j, k;

          /* NMEA строка не может быть короче 10 символов. */
          if (string_length < 10)
            {
              string_length = 0;
              continue;
            }

          /* Проверяем контрольную сумму NMEA строки. */
          string[string_length] = 0;
          for (nmea_crc1 = 0, i = 1; i < string_length - 3; i++)
            nmea_crc1 ^= string[i];

          /* Если контрольная сумма не совпадает, не используем время из это строки. */
          if ((sscanf (string + string_length - 3, "*%02X", &nmea_crc2) != 1) || (nmea_crc1 != nmea_crc2))
            bad_crc = TRUE;

          /* Пропускаем "плохие" NMEA строки. */
          if (priv->skip_broken && bad_crc)
            continue;

          /* Признак приёма правильной строки. */
          g_timer_start (timer);

          /* Вытаскиваем время из стандартных NMEA строк. */
          if ((g_str_has_prefix (string + 3, "GGA") ||
               g_str_has_prefix (string + 3, "RMC") ||
               g_str_has_prefix (string + 3, "GLL") ||
               g_str_has_prefix (string + 3, "BWC") ||
               g_str_has_prefix (string + 3, "ZDA")) && !bad_crc)
            {
              gint cur_string_time = 0;
              gint hour, min, sec, msec;
              gint n_fields;

              /* Смещение до поля со временем. */
              if (g_str_has_prefix (string + 3, "GLL"))
                {
                  for (i = 5, j = 0; i < string_length && j < 5; i++)
                    if (string[i] == ',')
                      j++;

                  k = i;
                }
              else
                {
                  k = 7;
                }

              /* Считываем время. */
              n_fields = sscanf (string + k,"%2d%2d%2d.%d", &hour, &min, &sec, &msec);
              if (n_fields == 3)
                cur_string_time = 1000 * (3600 * hour + 60 * min + sec);
              else if (n_fields == 4)
                cur_string_time = 1000 * (3600 * hour + 60 * min + sec) + msec;

              /* Если текущее время и время блока различаются, отправляем блок данных. */
              if ((string_time > 0) && (string_time != cur_string_time))
                send_block = TRUE;

              string_time = cur_string_time;
            }

          /* Если в блоке больше нет места, отправляем блок. */
          if (data_length + string_length + 3 > sizeof (data))
            send_block = TRUE;

          /* Если нет возможности определить время из строки - отправляем строку без объединения в блок. */
          if (string_time == 0)
            {
              string[string_length++] = '\r';
              string[string_length++] = '\n';
              string[string_length] = 0;

              g_rw_lock_writer_lock (&priv->lock);
              buffer = hyscan_slice_pool_pop (&priv->buffers);
              g_rw_lock_writer_unlock (&priv->lock);

              if (buffer != NULL)
                {
                  buffer->time = data_time;
                  buffer->size = string_length;
                  memcpy (buffer->nmea, string, string_length + 1);
                  g_async_queue_push (priv->queue, buffer);
                }

              string_length = 0;
              string_time = 0;
              data_time = 0;
              continue;
            }

          /* Отправляем блок данных. */
          if (send_block && (data_length > 0))
            {
              g_rw_lock_writer_lock (&priv->lock);
              buffer = hyscan_slice_pool_pop (&priv->buffers);
              g_rw_lock_writer_unlock (&priv->lock);

              if (buffer != NULL)
                {
                  buffer->time = data_time;
                  buffer->size = data_length;
                  memcpy (buffer->nmea, data, data_length + 1);
                  g_async_queue_push (priv->queue, buffer);
                }

              data_length = 0;
              data_time = 0;
            }

          /* Сохраняем строку в блоке. */
          memcpy (data + data_length, string, string_length);
          data_length += string_length;
          data [data_length++] = '\r';
          data [data_length++] = '\n';
          data [data_length] = 0;

          string_length = 0;
        }
    }

  g_timer_destroy (timer);

  return NULL;
}

/* Поток отправки данных клиенту. */
static gpointer
hyscan_nmea_uart_emmiter (gpointer user_data)
{
  HyScanNmeaUART *uart = user_data;
  HyScanNmeaUARTPrivate *priv = uart->priv;
  HyScanNmeaUARTMessage *buffer;

  while (g_atomic_int_get (&priv->terminate) == 0)
    {
      buffer = g_async_queue_timeout_pop (priv->queue, 100000);
      if (buffer == NULL)
        continue;

      g_signal_emit (uart, hyscan_nmea_uart_signals[SIGNAL_NMEA_DATA], 0,
                     buffer->time, priv->name, buffer->size, buffer->nmea);

      g_rw_lock_writer_lock (&priv->lock);
      hyscan_slice_pool_push (&priv->buffers, buffer);
      g_rw_lock_writer_unlock (&priv->lock);
    }

  return NULL;
}

/* Функция создаёт новый объект HyScanNmeaUART. */
HyScanNmeaUART *
hyscan_nmea_uart_new (const gchar *name)
{
  return g_object_new (HYSCAN_TYPE_NMEA_UART, "name", name, NULL);
}

/* Функция устанавливает порт и режим работы. */
gboolean
hyscan_nmea_uart_set_device (HyScanNmeaUART     *uart,
                             const gchar        *path,
                             HyScanNmeaUARTMode  mode)
{
  HyScanNmeaUARTPrivate *priv;
  gboolean status = FALSE;
  HANDLE fd;

  g_return_val_if_fail (HYSCAN_IS_UART (uart), FALSE);

  priv = uart->priv;

  /* Переходим в режим конфигурации. */
  while (!g_atomic_int_compare_and_exchange (&priv->configure, 0, 1))
    g_usleep (10000);
  while (g_atomic_int_get (&priv->started) == 1)
    g_usleep (10000);

  /* Устройство отключено. */
  if (path == NULL || mode == HYSCAN_NMEA_UART_MODE_DISABLED)
    {
      status = TRUE;
      goto exit;
    }

  /* Открываем устройство. */
  fd = hyscan_nmea_uart_open (path);
  if (fd == INVALID_HANDLE_VALUE)
    {
      g_warning ("HyScanNmeaUART: %s: can't open device", path);
      goto exit;
    }

  /* В автоматическом режиме отключается приём "плохих" строк. */
  if (mode == HYSCAN_NMEA_UART_MODE_AUTO)
    priv->skip_broken = TRUE;

  /* Устанавливаем режим работы порта. */
  if ((mode != HYSCAN_NMEA_UART_MODE_AUTO) && !hyscan_nmea_uart_set_mode (fd, priv->mode))
    {
      g_warning ("HyScanNmeaUART: %s: can't set device mode", path);
      goto exit;
    }

  priv->path = g_strdup (path);
  priv->mode = mode;
  priv->fd = fd;
  status = TRUE;

exit:
  /* Завершаем конфигурацию. */
  g_atomic_int_set (&priv->started, 1);
  g_atomic_int_set (&priv->configure, 0);

  return status;
}

/* Функция определяет пропускать "битые" NMEA строки или нет. */
void hyscan_nmea_uart_skip_broken (HyScanNmeaUART *uart,
                                   gboolean        skip_broken)
{
  g_return_if_fail (HYSCAN_IS_UART (uart));

  g_atomic_int_set (&uart->priv->skip_broken, skip_broken);
}

/* Функция возвращает текущее состояние приёма данных. */
HyScanSensorPortStatus
hyscan_nmea_uart_get_status (HyScanNmeaUART *uart)
{
  return HYSCAN_SENSOR_PORT_STATUS_INVALID;
}

#ifdef G_OS_UNIX
/* Функция возвращает список доступных UART устройств. */
HyScanNmeaUARTDevice **
hyscan_nmea_uart_list_devices (void)
{
  GDir *dir;
  const gchar *device;
  GPtrArray *list;

  /* Список всех устройств. */
  dir = g_dir_open ("/dev", 0, NULL);
  if (dir == NULL)
    return NULL;

  list = g_ptr_array_new ();

  while ((device = g_dir_read_name (dir)) != NULL)
    {
      struct termios options;
      gboolean usb_serial;
      gboolean serial;
      gchar *path;
      int fd;

      /* Пропускаем элементы "." и ".." */
      if (device[0] =='.' && device[1] == 0)
        continue;
      if (device[0] =='.' && device[1] =='.' && device[2] == 0)
        continue;

      /* Пропускаем все устройства, имена которых не начинаются на ttyS или ttyUSB. */
      serial = g_str_has_prefix (device, "ttyS");
      usb_serial = g_str_has_prefix (device, "ttyUSB");
      if (!serial && !usb_serial)
        continue;

      /* Открываем устройство и проверяем его тип. */
      path = g_strdup_printf ("/dev/%s", device);
      fd = open (path, O_RDWR | O_NONBLOCK | O_NOCTTY);
      g_free (path);

      if (fd < 0)
        {
          continue;
        }

      /* Проверяем тип устройства. */
      if (tcgetattr (fd, &options) == 0)
        {
          HyScanNmeaUARTDevice *port;
          gint index;

          if (serial)
            index = g_ascii_strtoll(device + 4, NULL, 10);
          else
            index = g_ascii_strtoll(device + 6, NULL, 10);

          port = g_new (HyScanNmeaUARTDevice, 1);
          port->name = g_strdup_printf ("%s%d", serial ? "COM" : "USBCOM", index + 1);
          port->path = g_strdup_printf ("/dev/%s", device);

          g_ptr_array_add (list, port);
        }

      close (fd);
    }

  g_dir_close (dir);

  /* Возвращаем список устройств. */
  if (list->len > 0)
    {
      HyScanNmeaUARTDevice **devices;

      g_ptr_array_add (list, NULL);
      devices = (gpointer)list->pdata;
      g_ptr_array_free (list, FALSE);

      return devices;
    }

  g_ptr_array_free (list, TRUE);

  return NULL;
}
#endif

#ifdef G_OS_WIN32
/* Функция возвращает список доступных UART устройств. */
HyScanNmeaUARTDevice **
hyscan_nmea_uart_list_devices (void)
{
  GPtrArray *list;

  HDEVINFO dev_info;
  SP_DEVINFO_DATA dev_data;
  DWORD dev_index;

  /* Список всех устройств типа UART. */
  dev_info = SetupDiGetClassDevs ((const GUID *)&GUID_DEVCLASS_PORTS, NULL, NULL, DIGCF_PRESENT);
  dev_data.cbSize = sizeof (SP_DEVINFO_DATA);
  dev_index = 0;

  list = g_ptr_array_new ();

  while (SetupDiEnumDeviceInfo(dev_info, dev_index, &dev_data))
    {
      HKEY hkey;
      LONG status;
      gchar port_path[1024] = {0};
      gchar port_name[1024] = {0};
      DWORD port_path_length = sizeof (port_path);
      DWORD port_name_length = sizeof (port_name);

      HyScanNmeaUARTDevice *port;
      gboolean is_usb = FALSE;

      dev_index += 1;

      /* Путь к порту из реестра. */
      hkey = SetupDiOpenDevRegKey (dev_info, &dev_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
      status = RegQueryValueEx (hkey, _T ("PortName"), NULL, NULL, (LPBYTE)port_path, &port_path_length);
      RegCloseKey(hkey);

      if (status != EXIT_SUCCESS)
        continue;

      /* Пропускаем LPT порты. */
      if (g_str_has_prefix (port_path, "LPT"))
        continue;

      /* Описание порта. */
      status = SetupDiGetDeviceRegistryProperty (dev_info, &dev_data,
                                                 SPDRP_FRIENDLYNAME, NULL, (PBYTE)port_name,
                                                 port_name_length, &port_name_length);
      if (!status)
        port_name_length = 0;

      if ((g_strstr_len (port_name, port_name_length, "USB") != NULL) ||
          (g_strstr_len (port_name, port_name_length, "usb") != NULL))
        is_usb = TRUE;

      port = g_new (HyScanNmeaUARTDevice, 1);
      port->path = g_strdup_printf ("%s:", port_path);

      if (is_usb)
        port->name = g_strdup_printf("USB%s", port_path);
      else
        port->name = g_strdup_printf("%s", port_path);

      g_ptr_array_add (list, port);
  }

  /* Возвращаем список устройств. */
  if (list->len > 0)
    {
      HyScanNmeaUARTDevice **devices;

      g_ptr_array_add (list, NULL);
      devices = (gpointer)list->pdata;
      g_ptr_array_free (list, FALSE);

      return devices;
    }

  g_ptr_array_free (list, TRUE);

  return NULL;
}
#endif

/* Функция освобождает память занятую списком UART устройств. */
void
hyscan_nmea_uart_devices_free (HyScanNmeaUARTDevice **devices)
{
  guint i;

  for (i = 0; devices != NULL && devices[i] != NULL; i++)
    {
      g_free (devices[i]->name);
      g_free (devices[i]->path);
      g_free (devices[i]);
    }

  g_free (devices);
}
