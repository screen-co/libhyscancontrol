/**
 * \file hyscan-control.h
 *
 * \brief Заголовочный файл библиотеки высокоуровневого управления гидролокаторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanControl HyScanControl - библиотека высокоуровневого управления гидролокаторами
 *
 * Для определения типа гидролокатора предназначена функция #hyscan_control_sonar_probe.
 *
 */

#ifndef __HYSCAN_CONTROL_H__
#define __HYSCAN_CONTROL_H__

#include <hyscan-sonar.h>

/** \brief Типы гидролокаторов */
typedef enum
{
  HYSCAN_SONAR_INVALID                         = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_ECHO                            = 101,          /**< Однолучевой эхолот. */
  HYSCAN_SONAR_SSS                             = 102,          /**< Гидролокатор бокового обзора. */
  HYSCAN_SONAR_SSSE                            = 103,          /**< Гидролокатор бокового обзора с эхолотом. */
  HYSCAN_SONAR_DSSS                            = 104,          /**< Двухчастотный гидролокатор бокового обзора. */
  HYSCAN_SONAR_DSSSE                           = 105           /**< Двухчастотный гидролокатор бокового обзора с эхолотом. */
} HyScanSonarType;

/**
 *
 * Функция возвращает тип гидролокатора.
 *
 * \param sonar указатель на интерфейс \link HyScanSonar \endlink.
 *
 * \return Тип гидролокатора - \link HyScanSonarType \endlink.
 *
 */
HYSCAN_API
HyScanSonarType                hyscan_control_sonar_probe      (HyScanSonar           *sonar);

#endif /* __HYSCAN_CONTROL_H__ */
