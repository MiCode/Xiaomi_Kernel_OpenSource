// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPUEB_PLAT_IPI_TEST_H__
#define __GPUEB_PLAT_IPI_TEST_H__

int gpueb_plat_ipi_send_testing(void);

#define PLT_INIT           0x504C5401
#define PLT_LOG_ENABLE     0x504C5402

struct plat_ipi_send_data {
    unsigned int cmd;
    union {
        struct {
            unsigned int phys;
            unsigned int size;
        } ctrl;
        struct {
            unsigned int enable;
        } logger;
    } u;
};

#endif /* __GPUEB_PLAT_IPI_TEST_H__ */