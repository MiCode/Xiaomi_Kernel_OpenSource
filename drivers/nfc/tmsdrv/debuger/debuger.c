// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#include <linux/slab.h>
#include "tms_debuger.h"
#include "logger.h"
#include "checker.h"

/*********** PART0: Global Variables Area ***********/
#define DEBUGER_DEVICE_INFO_SIZE 256
#define DEBUGER_DUMP_CHUNK_SIZE 512

uint8_t tms_log_level = LOG_LEVEL_INFO;
/*********** PART1: Function Area ***********/
void tms_buffer_dump(const char *tag, const uint8_t *src, int16_t len)
{
    uint16_t buf_len = (len > DEBUGER_DUMP_CHUNK_SIZE) ? DEBUGER_DUMP_CHUNK_SIZE : len;
    uint16_t index = 0;
    uint16_t i;
    char buf[DEBUGER_DUMP_CHUNK_SIZE * 2 + 1];

    do {
        memset(buf, 0, sizeof(buf));

        for (i = 0; i < buf_len; i++) {
            snprintf(&buf[i * 2], 3, "%02X", src[index++]);
        }

        TMS_LOG("%s[%d] %s\n", tag, buf_len, buf);
        len = len - buf_len;
        buf_len = (len > DEBUGER_DUMP_CHUNK_SIZE) ? DEBUGER_DUMP_CHUNK_SIZE : len;
    } while (len > 0);
}

/*********** PART3: TMS Debuger Init Area ***********/
int tms_debuger_proc_create(struct proc_dir_entry *prEntry)
{
    int ret;

    ret = checker_proc_create(prEntry);
    if (ret) {
        return ret;
    }

    ret = logger_proc_create(prEntry);
    if (ret) {
        return ret;
    }

    return 0;
}

int tms_debuger_init(void)
{
    int ret = 0;

#ifdef TMS_DEBUGER_CHECKER
    ret = checker_init();
    if (ret) {
        return ret;
    }
#endif
#ifdef TMS_DEBUGER_LOGGER
    ret = logger_init();
#endif
    return ret;
}

void tms_debuger_deinit(void)
{
#ifdef TMS_DEBUGER_CHECKER
    checker_deinit();
#endif
#ifdef TMS_DEBUGER_LOGGER
    logger_deinit();
#endif
}
