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
 
/*
 * @file   drError.h
 * @brief  Error id definitions
 *
 */

#ifndef __DRERROR_H__
#define __DRERROR_H__

#define E_DRAPI_DRV_ROT13 0x40001

/*
 * Driver fatal error codes.
 */
typedef enum {
    E_DR_OK               = 0, /**< Success */
    E_DR_IPC              = 1, /**< IPC error */
    E_DR_INTERNAL         = 2, /**< Internal error */
    /* ... add more error codes when required */
} drError_t;


#endif // __DRERROR_H__

