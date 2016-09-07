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

  HYSCAN_SONAR_SSSE                            = 101,          /**< Гидролокатор бокового обзора. */
} HyScanSonarType;

/**
 *
 * Функция возвращает тип гидролокатора.
 *
 * \param sonar указатель на интерфейс \link HyScanSonar e\ndlink.
 *
 * \return Тип гидролокатора - \link HyScanSonarType \endlink.
 *
 */
HYSCAN_API
HyScanSonarType                hyscan_control_sonar_probe      (HyScanSonar           *sonar);

#endif /* __HYSCAN_CONTROL_H__ */
