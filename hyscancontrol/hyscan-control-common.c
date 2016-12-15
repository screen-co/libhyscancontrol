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

/* Функция возвращает название борта гидролокатора по его идентификатору. */
const gchar *
hyscan_control_get_source_name (HyScanSourceType source)
{
  return hyscan_channel_get_name_by_types (source, FALSE, 1);
}

/* Функция возвращает идентификатор борта гидролокатора по его названию. */
HyScanSourceType
hyscan_control_get_source_type (const gchar *name)
{
  HyScanSourceType source = HYSCAN_SOURCE_INVALID;

  hyscan_channel_get_types_by_name (name, &source, NULL, NULL);

  return source;
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
