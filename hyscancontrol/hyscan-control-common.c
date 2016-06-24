/*
 * \file hyscan-control-common.c
 *
 * \brief Исходный файл общих функций библиотеки управления гидролокаторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-control-common.h"

/* Типы бортов гидролокаторов и их названия. */
typedef struct
{
  GQuark                       quark;
  const gchar                 *name;

  HyScanBoardType              board;
} HyScanBoardTypeInfo;

/* Типы источников данных и их названия. */
typedef struct
{
  GQuark                       quark;
  const gchar                 *name;

  HyScanSourceType             source;
} HyScanSourceTypeInfo;

static HyScanBoardTypeInfo hyscan_board_type_info[] =
{
  { 0, "starboard",            HYSCAN_BOARD_STARBOARD },
  { 0, "port",                 HYSCAN_BOARD_PORT },
  { 0, "starboard-hi",         HYSCAN_BOARD_STARBOARD_HI },
  { 0, "port-hi",              HYSCAN_BOARD_PORT_HI },
  { 0, "echosounder",          HYSCAN_BOARD_ECHOSOUNDER },
  { 0, "profiler",             HYSCAN_BOARD_PROFILER },
  { 0, "look-around",          HYSCAN_BOARD_LOOK_AROUND },
  { 0, "forward-look",         HYSCAN_BOARD_FORWARD_LOOK },

  { 0, NULL,                   HYSCAN_SOURCE_INVALID }
};

static HyScanSourceTypeInfo hyscan_source_type_info[] =
{
  { 0, "ss-starboard",         HYSCAN_SOURCE_SIDE_SCAN_STARBOARD },
  { 0, "ss-port",              HYSCAN_SOURCE_SIDE_SCAN_PORT },
  { 0, "ss-starboard-hi",      HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI },
  { 0, "ss-port-hi",           HYSCAN_SOURCE_SIDE_SCAN_PORT_HI },
  { 0, "bathy-starboard",      HYSCAN_SOURCE_BATHYMETRY_STARBOARD },
  { 0, "bathy-port",           HYSCAN_SOURCE_BATHYMETRY_PORT },
  { 0, "echosounder",          HYSCAN_SOURCE_ECHOSOUNDER },
  { 0, "profiler",             HYSCAN_SOURCE_PROFILER },
  { 0, "look-around",          HYSCAN_SOURCE_LOOK_AROUND },
  { 0, "forward-look",         HYSCAN_SOURCE_FORWARD_LOOK },

  { 0, NULL,                   HYSCAN_SOURCE_INVALID }
};

/* Функция инициализации статических данных. */
static void
hyscan_control_types_initialize (void)
{
  static gboolean hyscan_control_types_initialized = FALSE;
  gint i;

  if (hyscan_control_types_initialized)
    return;

  for (i = 0; hyscan_board_type_info[i].name != NULL; i++)
    hyscan_board_type_info[i].quark = g_quark_from_static_string (hyscan_board_type_info[i].name);

  for (i = 0; hyscan_source_type_info[i].name != NULL; i++)
    hyscan_source_type_info[i].quark = g_quark_from_static_string (hyscan_source_type_info[i].name);

  hyscan_control_types_initialized = TRUE;
}

/* Функция возвращает название борта гидролокатора по его идентификатору. */
const gchar *
hyscan_control_get_board_name (HyScanBoardType board)
{
  gint i;

  /* Инициализация статических данных. */
  hyscan_control_types_initialize ();

  /* Ищем название типа. */
  for (i = 0; hyscan_board_type_info[i].quark != 0; i++)
    {
      if (hyscan_board_type_info[i].board != board)
        continue;
      return hyscan_board_type_info[i].name;
    }

  return NULL;
}

/* Функция возвращает идентификатор борта гидролокатора по его названию. */
HyScanBoardType
hyscan_control_get_board_type (const gchar *name)
{
  GQuark quark;
  gint i;

  /* Инициализация статических данных. */
  hyscan_control_types_initialize ();

  /* Ищем тип по названию. */
  quark = g_quark_try_string (name);
  for (i = 0; hyscan_board_type_info[i].quark != 0; i++)
    if (hyscan_board_type_info[i].quark == quark)
      return hyscan_board_type_info[i].board;

  return HYSCAN_BOARD_INVALID;
}

/* Функция возвращает название источника данных по его идентификатору. */
const gchar *
hyscan_control_get_source_name (HyScanSourceType source)
{
  gint i;

  /* Инициализация статических данных. */
  hyscan_control_types_initialize ();

  /* Ищем название типа. */
  for (i = 0; hyscan_source_type_info[i].quark != 0; i++)
    {
      if (hyscan_source_type_info[i].source != source)
        continue;
      return hyscan_source_type_info[i].name;
    }

  return NULL;
}

/* Функция возвращает идентификатор источника по его названию. */
HyScanSourceType
hyscan_control_get_source_type (const gchar *name)
{
  GQuark quark;
  gint i;

  /* Инициализация статических данных. */
  hyscan_control_types_initialize ();

  /* Ищем тип по названию. */
  quark = g_quark_try_string (name);
  for (i = 0; hyscan_source_type_info[i].quark != 0; i++)
    if (hyscan_source_type_info[i].quark == quark)
      return hyscan_source_type_info[i].source;

  return HYSCAN_SOURCE_INVALID;
}
