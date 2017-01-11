/*
 * \file hyscan-sonar-rpc.h
 *
 * \brief Файл констант и функций для поддержки RPC в HyScanSonar
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2016
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#ifndef __HYSCAN_SONAR_RPC_H__
#define __HYSCAN_SONAR_RPC_H__

#include <glib.h>
#include <urpc-types.h>

#define HYSCAN_SONAR_RPC_MAGIC                 0x4E53534B
#define HYSCAN_SONAR_RPC_VERSION               20160100
#define HYSCAN_SONAR_RPC_STATUS_OK             1
#define HYSCAN_SONAR_RPC_STATUS_FAIL           0

#define HYSCAN_SONAR_RPC_MIN_PORT              10000
#define HYSCAN_SONAR_RPC_MAX_PORT              50000

#define HYSCAN_SONAR_RPC_MAX_PARAMS            1024

#define HYSCAN_SONAR_RPC_TYPE_NULL             0
#define HYSCAN_SONAR_RPC_TYPE_BOOLEAN          1
#define HYSCAN_SONAR_RPC_TYPE_INT64            2
#define HYSCAN_SONAR_RPC_TYPE_DOUBLE           3
#define HYSCAN_SONAR_RPC_TYPE_STRING           4

#define HYSCAN_SONAR_MSG_MAX_SIZE              sizeof (HyScanSonarRpcPacket)
#define HYSCAN_SONAR_MSG_DATA_PART_SIZE        32000

/* UDP сообщение HyScanSonarMessage. */
typedef struct
{
  guint32              magic;
  guint32              version;
  guint32              index;
  guint32              crc32;
  gint64               time;
  guint32              id;
  guint32              type;
  gfloat               rate;
  guint32              size;
  guint32              part_size;
  guint32              offset;
  guint8               data[HYSCAN_SONAR_MSG_DATA_PART_SIZE];
} HyScanSonarRpcPacket;

enum
{
  HYSCAN_SONAR_RPC_PROC_VERSION = URPC_PROC_USER,
  HYSCAN_SONAR_RPC_PROC_GET_SCHEMA,
  HYSCAN_SONAR_RPC_PROC_SET_MASTER,
  HYSCAN_SONAR_RPC_PROC_SET,
  HYSCAN_SONAR_RPC_PROC_GET
};

enum
{
  HYSCAN_SONAR_RPC_PARAM_VERSION = URPC_PARAM_USER,
  HYSCAN_SONAR_RPC_PARAM_MAGIC,
  HYSCAN_SONAR_RPC_PARAM_STATUS,
  HYSCAN_SONAR_RPC_PARAM_SCHEMA_DATA,
  HYSCAN_SONAR_RPC_PARAM_SCHEMA_SIZE,
  HYSCAN_SONAR_RPC_PARAM_SCHEMA_MD5,
  HYSCAN_SONAR_RPC_PARAM_SCHEMA_ID,
  HYSCAN_SONAR_RPC_PARAM_MASTER_HOST,
  HYSCAN_SONAR_RPC_PARAM_MASTER_PORT,
  HYSCAN_SONAR_RPC_PARAM_NAME0,
  HYSCAN_SONAR_RPC_PARAM_NAME1 = HYSCAN_SONAR_RPC_PARAM_NAME0 + HYSCAN_SONAR_RPC_MAX_PARAMS,
  HYSCAN_SONAR_RPC_PARAM_TYPE0,
  HYSCAN_SONAR_RPC_PARAM_TYPE1 = HYSCAN_SONAR_RPC_PARAM_TYPE0 + HYSCAN_SONAR_RPC_MAX_PARAMS,
  HYSCAN_SONAR_RPC_PARAM_VALUE0,
  HYSCAN_SONAR_RPC_PARAM_VALUE1 = HYSCAN_SONAR_RPC_PARAM_VALUE0 + HYSCAN_SONAR_RPC_MAX_PARAMS
};

/* Функция преобразовывает значение float из LE в машинный формат. */
gfloat         hyscan_sonar_rpc_float_from_le  (gfloat         value);

/* Функция преобразовывает значение float из машинного формата в LE. */
gfloat         hyscan_sonar_rpc_float_to_le    (gfloat         value);

#endif /* __HYSCAN_SONAR_RPC_H__ */
