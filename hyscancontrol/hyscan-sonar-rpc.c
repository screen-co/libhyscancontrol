/*
 * \file hyscan-sonar-rpc.c
 *
 * \brief Файл констант и функций для поддержки RPC в HyScanSonar
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-sonar-rpc.h"
#include <string.h>

/* Функция преобразовывает значение float из LE в машинный формат. */
gfloat
hyscan_sonar_rpc_float_from_le (gfloat value)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
  guint32 buffer;
  gfloat output;

  memcpy (&buffer, &value, sizeof (guint32));
  buffer = GUINT32_FROM_LE (buffer);
  memcpy (&output, &buffer, sizeof (guint32));

  return output;
#else
  return value;
#endif
}

/* Функция преобразовывает значение float из машинного формата в LE. */
gfloat
hyscan_sonar_rpc_float_to_le (gfloat value)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
  guint32 buffer;
  gfloat output;

  memcpy (&buffer, &value, sizeof (guint32));
  buffer = GUINT32_TO_LE (buffer);
  memcpy (&output, &buffer, sizeof (guint32));

  return output;
#else
  return value;
#endif
}
