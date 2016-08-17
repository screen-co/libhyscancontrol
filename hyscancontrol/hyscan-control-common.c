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

/* Функция возвращает идентификатор борта гидролокатора по типу источника данных. */
HyScanBoardType
hyscan_control_get_board_type_by_source (HyScanSourceType source)
{
  switch (source)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
    case HYSCAN_SOURCE_BATHYMETRY_STARBOARD:
      return HYSCAN_BOARD_STARBOARD;

    case HYSCAN_SOURCE_SIDE_SCAN_PORT:
    case HYSCAN_SOURCE_BATHYMETRY_PORT:
      return HYSCAN_BOARD_PORT;

    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI:
      return HYSCAN_BOARD_STARBOARD_HI;

    case HYSCAN_SOURCE_SIDE_SCAN_PORT_HI:
      return HYSCAN_BOARD_PORT_HI;

    case HYSCAN_SOURCE_ECHOSOUNDER:
      return HYSCAN_BOARD_ECHOSOUNDER;

    case HYSCAN_SOURCE_PROFILER:
      return HYSCAN_BOARD_PROFILER;

    case HYSCAN_SOURCE_LOOK_AROUND:
      return HYSCAN_BOARD_LOOK_AROUND;

    case HYSCAN_SOURCE_FORWARD_LOOK:
      return HYSCAN_BOARD_FORWARD_LOOK;

    default:
      break;
    }

  return HYSCAN_BOARD_INVALID;
}

/* Функция возвращает идентификатор источника "сырых" данных для борта. */
HyScanSourceType
hyscan_control_get_raw_source_type (HyScanBoardType board)
{
  switch (board)
    {
    case HYSCAN_BOARD_STARBOARD:
      return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;

    case HYSCAN_BOARD_PORT:
      return HYSCAN_SOURCE_SIDE_SCAN_PORT;

    case HYSCAN_BOARD_STARBOARD_HI:
      return HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI;

    case HYSCAN_BOARD_PORT_HI:
      return HYSCAN_SOURCE_SIDE_SCAN_PORT_HI;

    case HYSCAN_BOARD_ECHOSOUNDER:
      return HYSCAN_SOURCE_ECHOSOUNDER;

    case HYSCAN_BOARD_PROFILER:
      return HYSCAN_SOURCE_PROFILER;

    case HYSCAN_BOARD_LOOK_AROUND:
      return HYSCAN_SOURCE_LOOK_AROUND;

    case HYSCAN_BOARD_FORWARD_LOOK:
      return HYSCAN_SOURCE_FORWARD_LOOK;

    default:
      break;
    }

  return HYSCAN_SOURCE_INVALID;
}

/* Функция аккумулирует boolean ответы всех callback'ов .
 * Здесь действует обратная логика, если какой-либо из callback'ов
 * вернёт FALSE, аккумулятор вернёт TRUE. Это будет сигналом
 * прекратить обработку запроса. */
gboolean
hyscan_control_boolean_accumulator (GSignalInvocationHint *ihint,
                                    GValue                *return_accu,
                                    const GValue          *handler_return,
                                    gpointer              data)
{
  if (!g_value_get_boolean (handler_return))
    {
      g_value_set_boolean (return_accu, TRUE);
      return FALSE;
    }

  return TRUE;
}

/* Функция ищет boolean параметр в списке и считывает его значение. */
gboolean
hyscan_control_find_boolean_param (const gchar         *name,
                                   const gchar *const  *names,
                                   GVariant           **values,
                                   gboolean            *value)
{
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  for (i = 0; i < n_names; i++)
    {
      if (g_strcmp0 (names[i], name) == 0)
        {
          if (values[i] == NULL)
            return FALSE;

          if (g_variant_classify (values[i]) != G_VARIANT_CLASS_BOOLEAN)
            return FALSE;

          *value = g_variant_get_boolean (values[i]);

          return TRUE;
        }

    }

  return FALSE;
}

/* Функция ищет integer параметр в списке и считывает его значение. */
gboolean
hyscan_control_find_integer_param (const gchar         *name,
                                   const gchar *const  *names,
                                   GVariant           **values,
                                   gint64              *value)
{
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  for (i = 0; i < n_names; i++)
    {
      if (g_strcmp0 (names[i], name) == 0)
        {
          if (values[i] == NULL)
            return FALSE;

          if (g_variant_classify (values[i]) != G_VARIANT_CLASS_INT64)
            return FALSE;

          *value = g_variant_get_int64 (values[i]);

          return TRUE;
        }

    }

  return FALSE;
}

/* Функция ищет double параметр в списке и считывает его значение. */
gboolean
hyscan_control_find_double_param (const gchar         *name,
                                  const gchar *const  *names,
                                  GVariant           **values,
                                  gdouble             *value)
{
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  for (i = 0; i < n_names; i++)
    {
      if (g_strcmp0 (names[i], name) == 0)
        {
          if (values[i] == NULL)
            return FALSE;

          if (g_variant_classify (values[i]) != G_VARIANT_CLASS_DOUBLE)
            return FALSE;

          *value = g_variant_get_double (values[i]);

          return TRUE;
        }

    }

  return FALSE;
}

/* Функция ищет string параметр в списке и считывает его значение. */
const gchar *
hyscan_control_find_string_param (const gchar         *name,
                                  const gchar *const  *names,
                                  GVariant           **values)
{
  guint n_names;
  guint i;

  n_names = g_strv_length ((gchar**)names);
  for (i = 0; i < n_names; i++)
    {
      if (g_strcmp0 (names[i], name) == 0)
        {
          if (values[i] == NULL)
            return NULL;

          if (g_variant_classify (values[i]) != G_VARIANT_CLASS_STRING)
            return NULL;

          return g_variant_get_string (values[i], NULL);
        }

    }

  return NULL;
}
