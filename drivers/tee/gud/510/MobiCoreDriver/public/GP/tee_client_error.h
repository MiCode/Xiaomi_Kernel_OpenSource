/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2013-2020 TRUSTONIC LIMITED
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

#ifndef TEE_CLIENT_ERROR_H
#define TEE_CLIENT_ERROR_H

#define TEEC_SUCCESS                      ((uint32_t)0x00000000)

/**
 * Generic error code : Generic error
 **/
#define TEEC_ERROR_GENERIC                ((uint32_t)0xFFFF0000)

/**
 * Generic error code : The underlying security system denies the access to the
 * object
 **/
#define TEEC_ERROR_ACCESS_DENIED          ((uint32_t)0xFFFF0001)

/**
 * Generic error code : The pending operation is cancelled.
 **/
#define TEEC_ERROR_CANCEL                 ((uint32_t)0xFFFF0002)

/**
 * Generic error code : The underlying system detects a conflict
 **/
#define TEEC_ERROR_ACCESS_CONFLICT        ((uint32_t)0xFFFF0003)

/**
 * Generic error code : Too much data for the operation or some data remain
 * unprocessed by the operation.
 **/
#define TEEC_ERROR_EXCESS_DATA            ((uint32_t)0xFFFF0004)

/**
 * Generic error code : Error of data format
 **/
#define TEEC_ERROR_BAD_FORMAT             ((uint32_t)0xFFFF0005)

/**
 * Generic error code : The specified parameters are invalid
 **/
#define TEEC_ERROR_BAD_PARAMETERS         ((uint32_t)0xFFFF0006)

/**
 * Generic error code : Illegal state for the operation.
 **/
#define TEEC_ERROR_BAD_STATE              ((uint32_t)0xFFFF0007)

/**
 * Generic error code : The item is not found
 **/
#define TEEC_ERROR_ITEM_NOT_FOUND         ((uint32_t)0xFFFF0008)

/**
 * Generic error code : The specified operation is not implemented
 **/
#define TEEC_ERROR_NOT_IMPLEMENTED        ((uint32_t)0xFFFF0009)

/**
 * Generic error code : The specified operation is not supported
 **/
#define TEEC_ERROR_NOT_SUPPORTED          ((uint32_t)0xFFFF000A)

/**
 * Generic error code : Insufficient data is available for the operation.
 **/
#define TEEC_ERROR_NO_DATA                ((uint32_t)0xFFFF000B)

/**
 * Generic error code : Not enough memory to perform the operation
 **/
#define TEEC_ERROR_OUT_OF_MEMORY          ((uint32_t)0xFFFF000C)

/**
 * Generic error code : The service is currently unable to handle the request;
 * try later
 **/
#define TEEC_ERROR_BUSY                   ((uint32_t)0xFFFF000D)

/**
 * Generic communication error
 **/
#define TEEC_ERROR_COMMUNICATION          ((uint32_t)0xFFFF000E)

/**
 * Generic error code : security violation
 **/
#define TEEC_ERROR_SECURITY               ((uint32_t)0xFFFF000F)

/**
 * Generic error code : the buffer is too short
 **/
#define TEEC_ERROR_SHORT_BUFFER           ((uint32_t)0xFFFF0010)

/**
 * Error of communication: The target of the connection is dead
 **/
#define TEEC_ERROR_TARGET_DEAD            ((uint32_t)0xFFFF3024)

/**
 * File system error code: not enough space to complete the operation.
 **/
#define TEEC_ERROR_STORAGE_NO_SPACE       ((uint32_t)0xFFFF3041)

#endif /* TEE_CLIENT_ERROR_H */

