/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * Copyright (C) 2018 XiaoMi, Inc.
 * All rights reserved
 *
 * The present software is the confidential and proprietary information of
 * TRUSTONIC LIMITED. You shall not disclose the present software and shall
 * use it only in accordance with the terms of the license agreement you
 * entered into with TRUSTONIC LIMITED. This software may be subject to
 * export or import laws in certain countries.
 */

#ifndef TLTEST_API_H_
#define TLTEST_API_H_

#include "tci.h"

/*
 * Command ID's for communication Trustlet Connector -> Trustlet.
 */
#define TLTEST_CMD_1        0x50
#define TLTEST_CMD_2        0x51
#define TLTEST_CMD_3        0x52
#define TLTEST_CMD_4        0x53

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
} tl_cmd_t;

/*
 * Response structure Trustlet -> Trustlet Connector.
 */
typedef struct {
    tciResponseHeader_t header;     /**< Response header */
    uint32_t            len;
} tl_rsp_t;

/*
 * TCI message data.
 */
typedef struct {
    union {
      tl_cmd_t     cmd_test;
      tl_rsp_t     rsp_test;
    };

    uint32_t tx_dma; //dma_addr_t
    uint32_t rx_dma; //dma_addr_t

} tciTestMessage_t;

/*
 * Trustlet UUID.
 */
#define TL_TEST_UUID {{ 0x05, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}

#endif // TLTEST_API_H_
