/**
 * \file hyscan-sonar-discover.h
 *
 * \brief Заголовочный файл интерфейса обнаружения гидролокаторов
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanSonarDiscover HyScanSonarDiscover - интерфейс обнаружения гидролокаторов
 *
 * Интерфейс предназначен для обнаружения гидролокаторов подключенных к компьютеру.
 * Реализация данного интерфейса зависит от модели гидролокатора и должна размещаться
 * в драйвере гидролокатора.
 *
 * Интерфейс содержит функции управления процессом обнаружения гидролокаторов:
 *
 * - #hyscan_sonar_discover_begin - инициирование обнаружения гидролокаторов;
 * - #hyscan_sonar_discover_stop - прерывание обнаружения гидролокаторов;
 * - #hyscan_sonar_discover_progress - прогресс обнаружения гидролокаторов.
 *
 * Список обнаруженных гидролокаторов можно получить функцией #hyscan_sonar_discover_list.
 * Память, используемая списоком, должена быть освобождена функцией #hyscan_sonar_discover_free.
 *
 * Для подключения к гидролокатору используется функция #hyscan_sonar_discover_connect.
 *
 * Параметры драйвера можно узнать с помощью функции #hyscan_sonar_discover_config.
 *
 */

#ifndef __HYSCAN_SONAR_DISCOVER_H__
#define __HYSCAN_SONAR_DISCOVER_H__

#include <hyscan-data-box.h>

G_BEGIN_DECLS

/** \brief Общая информация о гидролокаторе. */
typedef struct
{
  gchar                       *model;         /**< Модель гидролокатора. */
  gchar                       *uri;           /**< Путь для подключения к гидролокатору. */
} HyScanSonarDiscoverInfo;

#define HYSCAN_TYPE_SONAR_DISCOVER            (hyscan_sonar_discover_get_type ())
#define HYSCAN_SONAR_DISCOVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_SONAR_DISCOVER, HyScanSonarDiscover))
#define HYSCAN_IS_SONAR_DISCOVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_SONAR_DISCOVER))
#define HYSCAN_SONAR_DISCOVER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_SONAR_DISCOVER, HyScanSonarDiscoverInterface))

typedef struct _HyScanSonarDiscover HyScanSonarDiscover;
typedef struct _HyScanSonarDiscoverInterface HyScanSonarDiscoverInterface;

struct _HyScanSonarDiscoverInterface
{
  GTypeInterface               g_iface;

  gboolean                     (*begin)                        (HyScanSonarDiscover           *discover);

  gboolean                     (*stop)                         (HyScanSonarDiscover           *discover);

  guint                        (*progress)                     (HyScanSonarDiscover           *discover);

  HyScanSonarDiscoverInfo    **(*list)                         (HyScanSonarDiscover           *discover);

  HyScanParam                 *(*connect)                      (HyScanSonarDiscover           *discover,
                                                                const gchar                   *uri,
                                                                const gchar                   *config);

  HyScanDataBox               *(*config)                       (HyScanSonarDiscover           *discover,
                                                                const gchar                   *uri);
};

HYSCAN_API
GType                          hyscan_sonar_discover_get_type  (void);

/**
 *
 * Функция инициирует процесс обнаружения гидролокаторов.
 *
 * \param discover указатель на интерфейс \link HyScanSonarDsicover \endlink.
 *
 * \return TRUE - если процесс запущен, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sonar_discover_begin     (HyScanSonarDiscover           *discover);

/**
 *
 * Функция останавливает процесс обнаружения гидролокаторов.
 *
 * \param discover указатель на интерфейс \link HyScanSonarDsicover \endlink.
 *
 * \return TRUE - если процесс остановлен, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean                       hyscan_sonar_discover_stop      (HyScanSonarDiscover           *discover);

/**
 *
 * Функция возвращает прогресс обнаружения гидролокаторов.
 *
 * \param discover указатель на интерфейс \link HyScanSonarDsicover \endlink.
 *
 * \return процент выполнения поиска от 0 до 100 включительно.
 *
 */
HYSCAN_API
guint                          hyscan_sonar_discover_progress  (HyScanSonarDiscover           *discover);

/**
 *
 * Функция возвращает список с общей информацией об обнаруженных гидролокаторах.
 * Память выделенная под список должна быть освобождена после использования
 * функцией #hyscan_sonar_discover_free.
 *
 * \param discover указатель на интерфейс \link HyScanSonarDsicover \endlink.
 *
 * \return NULL терминированный список или NULL - если нет обнаруженных гидролокаторов.
 *
 */
HYSCAN_API
HyScanSonarDiscoverInfo      **hyscan_sonar_discover_list      (HyScanSonarDiscover           *discover);

/**
 *
 * Функция производит подключение к гидролокатору.
 *
 * \param discover указатель на интерфейс \link HyScanSonarDsicover \endlink;
 * \param uri путь для подключения к гидролокатору;
 * \param config параметры драйвера или NULL.
 *
 * \return Указатель на интерфейс \link HyScanParam \endlink или NULL.
 *
 */
HYSCAN_API
HyScanParam                   *hyscan_sonar_discover_connect   (HyScanSonarDiscover           *discover,
                                                                const gchar                   *uri,
                                                                const gchar                   *config);

/**
 *
 * Функция возвращает объект с параметрами драйвера гидролокатора. Эти параметры,
 * в сериализованном виде (см. \link hyscan_data_box_serialize \endlink), можно
 * передать в функцию #hyscan_sonar_discover_connect.
 *
 * Если драйвер не содержит настраиваемых параметров, функция вернёт NULL.
 *
 * \param discover указатель на интерфейс \link HyScanSonarDsicover \endlink;
 * \param uri путь для подключения к гидролокатору.
 *
 * \return Указатель на объект \link HyScanDataBox \endlink или NULL.
 *
 */
HYSCAN_API
HyScanDataBox                 *hyscan_sonar_discover_config    (HyScanSonarDiscover           *discover,
                                                                const gchar                   *uri);

/**
 *
 * Функция освобождает память занятую списоком гидролокаторов.
 *
 * \param list список гидролокаторов \link HyScanSonarDiscoverInfo \endlink.
 *
 * \return Нет.
 *
 */
HYSCAN_API
void                           hyscan_sonar_discover_free      (HyScanSonarDiscoverInfo      **list);

G_END_DECLS

#endif /* __HYSCAN_SONAR_DISCOVER_H__ */
