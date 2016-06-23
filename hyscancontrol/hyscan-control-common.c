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

/* Типы источников данных и их названия. */
typedef struct
{
  GQuark                       quark;
  const gchar                 *name;

  HyScanSourceType             id;
} HyScanSourceTypeInfo;

static HyScanSourceTypeInfo hyscan_source_type_info[] =
{
  { 0, "echosounder",     HYSCAN_SOURCE_ECHOSOUNDER },
  { 0, "ss-starboard",    HYSCAN_SOURCE_SS_STARBOARD },
  { 0, "ss-port",         HYSCAN_SOURCE_SS_PORT },
  { 0, "ss-starboard-hi", HYSCAN_SOURCE_SS_STARBOARD_HI },
  { 0, "ss-port-hi",      HYSCAN_SOURCE_SS_PORT_HI },
  { 0, "iss-starboard",   HYSCAN_SOURCE_ISS_STARBOARD },
  { 0, "iss-port",        HYSCAN_SOURCE_ISS_PORT },
  { 0, "la-starboard",    HYSCAN_SOURCE_LA_STARBOARD },
  { 0, "la-port",         HYSCAN_SOURCE_LA_PORT },
  { 0, "profiler",        HYSCAN_SOURCE_PROFILER },
  { 0, "fl",              HYSCAN_SOURCE_FL },

  { 0, NULL,              HYSCAN_SOURCE_INVALID }
};

/* Функция инициализации статических данных. */
static void
hyscan_control_types_initialize (void)
{
  static gboolean hyscan_control_types_initialized = FALSE;
  gint i;

  if (hyscan_control_types_initialized)
    return;

  for (i = 0; hyscan_source_type_info[i].name != NULL; i++)
    hyscan_source_type_info[i].quark = g_quark_from_static_string (hyscan_source_type_info[i].name);

  hyscan_control_types_initialized = TRUE;
}


/* Функция возвращает название источника данных по его идентификатору. */
const gchar *
hyscan_control_get_source_name (HyScanSourceType id)
{
  gint i;

  /* Инициализация статических данных. */
  hyscan_control_types_initialize ();

  /* Ищем название типа. */
  for (i = 0; hyscan_source_type_info[i].quark != 0; i++)
    {
      if (hyscan_source_type_info[i].id != id)
        continue;
      return hyscan_source_type_info[i].name;
    }

  return NULL;
}

/* Функция возвращает идентификатор источника по его названию. */
HyScanSourceType
hyscan_control_get_source_id (const gchar *name)
{
  GQuark quark;
  gint i;

  /* Инициализация статических данных. */
  hyscan_control_types_initialize ();

  /* Ищем тип по названию. */
  quark = g_quark_try_string (name);
  for (i = 0; hyscan_source_type_info[i].quark != 0; i++)
    if (hyscan_source_type_info[i].quark == quark)
      return hyscan_source_type_info[i].id;

  return HYSCAN_SOURCE_INVALID;
}
