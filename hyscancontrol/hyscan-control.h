/**
 * \file hyscan-control.h
 *
 * \brief Заголовочный файл библиотеки высокоуровневого управления гидролокаторами
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_CONTROL_H__
#define __HYSCAN_CONTROL_H__

#include <hyscan-sonar.h>
#include <hyscan-control-exports.h>

/** \brief Типы гидролокаторов */
typedef enum
{
  HYSCAN_SONAR_INVALID                         = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_SONAR_SSSE                            = 101,          /**< Гидролокатор бокового обзора. */
} HyScanSonarType;

/** \brief Типы бортов гидролокаторов */
typedef enum
{
  HYSCAN_BOARD_INVALID                         = 0,            /**< Недопустимый тип, ошибка. */

  HYSCAN_BOARD_STARBOARD                       = 101,          /**< Правый борт. */
  HYSCAN_BOARD_PORT                            = 102,          /**< Левый борт. */
  HYSCAN_BOARD_STARBOARD_HI                    = 103,          /**< Правый борт, высокое разрашение. */
  HYSCAN_BOARD_PORT_HI                         = 104,          /**< Левый борт, высокое разрашение. */
  HYSCAN_BOARD_ECHOSOUNDER                     = 105,          /**< Эхолот. */
  HYSCAN_BOARD_PROFILER                        = 106,          /**< Профилограф. */
  HYSCAN_BOARD_LOOK_AROUND                     = 107,          /**< Круговой обзор. */
  HYSCAN_BOARD_FORWARD_LOOK                    = 108           /**< Вперёдсмотрящий гидролокатор. */
} HyScanBoardType;

HYSCAN_CONTROL_EXPORT
HyScanSonarType                hyscan_control_sonar_probe      (HyScanSonar           *sonar);

#endif /* __HYSCAN_CONTROL_H__ */
