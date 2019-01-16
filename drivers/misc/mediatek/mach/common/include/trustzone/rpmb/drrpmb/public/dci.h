/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All rights reserved
 * 
 * The present software is the confidential and proprietary information of
 * TRUSTONIC LIMITED. You shall not disclose the present software and shall
 * use it only in accordance with the terms of the license agreement you
 * entered into with TRUSTONIC LIMITED. This software may be subject to
 * export or import laws in certain countries.
 */

/**
 * @file   dci.h
 * @brief  Contains DCI (Driver Control
 * Interface) definitions and data structures
 *
 */

#ifndef __DCI_H__
#define __DCI_H__


typedef uint32_t dciCommandId_t;
typedef uint32_t dciResponseId_t;
typedef uint32_t dciReturnCode_t;

/**< Responses have bit 31 set */
#define RSP_ID_MASK (1U << 31)
#define RSP_ID(cmdId) (((uint32_t)(cmdId)) | RSP_ID_MASK)
#define IS_CMD(cmdId) ((((uint32_t)(cmdId)) & RSP_ID_MASK) == 0)
#define IS_RSP(cmdId) ((((uint32_t)(cmdId)) & RSP_ID_MASK) == RSP_ID_MASK)

/**
 * Return codes of driver commands.
 */
#define RET_OK                    0
#define RET_ERR_UNKNOWN_CMD       1
#define RET_ERR_NOT_SUPPORTED     2
#define RET_ERR_INTERNAL_ERROR    3
/* ... add more error codes when needed */

/**
 * DCI command header.
 */
typedef struct{
    dciCommandId_t commandId; /**< Command ID */
} dciCommandHeader_t;

/**
 * DCI response header.
 */
typedef struct{
    dciResponseId_t     responseId; /**< Response ID (must be command ID | RSP_ID_MASK )*/
    dciReturnCode_t     returnCode; /**< Return code of command */
} dciResponseHeader_t;

#endif // __DCI_H__
