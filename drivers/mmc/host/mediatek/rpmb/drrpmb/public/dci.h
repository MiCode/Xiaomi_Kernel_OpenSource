/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
struct dciCommandHeader_t {
	dciCommandId_t commandId; /**< Command ID */
};

/**
 * DCI response header.
 */
struct dciResponseHeader_t {
	/**< Response ID (must be command ID | RSP_ID_MASK )*/
	dciResponseId_t     responseId;
	dciReturnCode_t     returnCode; /**< Return code of command */
};

#endif
