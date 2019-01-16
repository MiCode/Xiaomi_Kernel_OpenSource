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

#ifndef _TL_SEC_API_H_
#define _TL_SEC_API_H_

#include "tci.h"

/*
 * Command ID's for communication Trustlet Connector -> Trustlet.
 */
#define CMD_DEVINFO_GET     1
#define CMD_DAPC_SET        2
#define CMD_HACC_REQUEST    3

/*
 * Termination codes
 */
#define EXIT_ERROR                  ((uint32_t)(-1))

/*
 * command message.
 *
 * @param len Lenght of the data to process.
 * @param data Data to processed (cleartext or ciphertext).
 */
typedef struct {
    tciCommandHeader_t  header;     /**< Command header */
    uint32_t            len;        /**< Length of data to process or buffer */
    uint32_t            respLen;    /**< Length of response buffer */
} dapc_cmd_t;

/*
 * Response structure Trustlet -> Trustlet Connector.
 */
typedef struct {
    tciResponseHeader_t header;     /**< Response header */
    uint32_t            len;
} dapc_rsp_t;

/*
 * TCI message data.
 */
typedef struct {
    union {
      dapc_cmd_t     cmd;
      dapc_rsp_t     rsp;
    };
    uint32_t    index;
    uint32_t    result;
    uint32_t    data_addr;
    uint32_t    data_len;
    uint32_t    seed_addr;
    uint32_t    seed_len;
    uint32_t    hacc_user;
    uint32_t    direction;
    uint32_t    reserve[2];
} dapc_tciMessage_t;

/*
 * Trustlet UUID.
 */
#define TL_SEC_UUID { { 0x5, 0x11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }

#endif // _TL_SEC_API_H_
