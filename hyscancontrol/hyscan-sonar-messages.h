/*
 * \file hyscan-sonar-messages.h
 *
 * \brief Заголовочный файл структур данных от гидролокатора
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_SONAR_MESSAGES_H__
#define __HYSCAN_SONAR_MESSAGES_H__

#include <hyscan-param.h>

G_BEGIN_DECLS

/** \brief Сообщение от гидролокатора с данными */
typedef struct
{
  gint64                   time;               /**< Время приёма сообщения, мкс. */
  guint32                  id;                 /**< Идентификатор источника сообщения. */
  guint32                  type;               /**< Тип данных - \link HyScanDataType \endlink. */
  gfloat                   rate;               /**< Частота дискретизации данных, Гц. */
  guint32                  size;               /**< Размер данных, в байтах. */
  gconstpointer            data;               /**< Данные. */
} HyScanSonarMessage;

G_END_DECLS

#endif /* __HYSCAN_SONAR_MESSAGES_H__ */
