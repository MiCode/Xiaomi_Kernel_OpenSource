// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/**
 * @file    gpueb_mt6983_config.c
 * @brief   GPUEB platform related config
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>
#include <mboot_params.h>

#include "gpueb_common_helper.h"
#include "gpueb_plat_config.h"

// MTK common IPI/MBOX
#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include <linux/soc/mediatek/mtk-mbox.h>

#define MT_GPUEB_BRINGUP        0
#define MT_GPUEB_LOGGER_ENABLE  0
#define MT_GPUEB_IPI_TEST       1

#define PIN_R_SIZE 20
#define PIN_S_SIZE 20
#define GPUEB_MBOX_TOTAL 4
#define GPUEB_SLOT_NUM_PER_MBOX (PIN_R_SIZE + PIN_S_SIZE)

#define CH_PLATFORM	0
#define CH_DVFS	    1
#define CH_SLEEP	2
#define CH_TIMER	3
#define GPUEB_IPI_COUNT	4
#define GPUEB_TOTAL_SEND_PIN     GPUEB_IPI_COUNT
#define GPUEB_TOTAL_RECV_PIN     GPUEB_IPI_COUNT

#define PIN_R_SIZE_PLATFORM     PIN_R_SIZE
#define PIN_R_SIZE_DVFS         PIN_R_SIZE
#define PIN_R_SIZE_SLEEP        PIN_R_SIZE
#define PIN_R_SIZE_TIMER        PIN_R_SIZE

#define PIN_S_SIZE_PLATFORM     PIN_S_SIZE
#define PIN_S_SIZE_DVFS         PIN_S_SIZE
#define PIN_S_SIZE_SLEEP        PIN_S_SIZE
#define PIN_S_SIZE_TIMER        PIN_S_SIZE

#define PIN_S_OFFSET_PLATFORM   0
#define PIN_S_OFFSET_DVFS       0
#define PIN_S_OFFSET_SLEEP      0
#define PIN_S_OFFSET_TIMER      0

#define PIN_R_OFFSET_PLATFORM   (PIN_S_OFFSET_PLATFORM  + PIN_S_SIZE_PLATFORM)
#define PIN_R_OFFSET_DVFS       (PIN_S_OFFSET_DVFS      + PIN_S_SIZE_DVFS)
#define PIN_R_OFFSET_SLEEP      (PIN_S_OFFSET_SLEEP     + PIN_S_SIZE_SLEEP)
#define PIN_R_OFFSET_TIMER      (PIN_S_OFFSET_TIMER     + PIN_S_SIZE_TIMER)

#define PIN_R_MSG_SIZE_PLATFORM 1 // 1 slot,    4 bytes
#define PIN_R_MSG_SIZE_DVFS     4 // 4 slots,   16 bytes
#define PIN_R_MSG_SIZE_SLEEP    1 // 1 slot,    4 bytes
#define PIN_R_MSG_SIZE_TIMER    1 // 1 slot,    4 bytes

#define PIN_S_MSG_SIZE_PLATFORM 4 // 4 slots,   16 bytes
#define PIN_S_MSG_SIZE_DVFS     4 // 4 slots,   16 bytes
#define PIN_S_MSG_SIZE_SLEEP    3 // 3 slots,   12 bytes
#define PIN_S_MSG_SIZE_TIMER    3 // 3 slots,   12 bytes

struct gpueb_reserve_mblock gpueb_reserve_mblock_ary[] = {
    {
        .num = GPUEB_LOGGER_MEM_ID,
        .start_phys = 0x0,
        .start_virt = 0x0,
        .size = 0x0,
    },
};

struct mtk_mbox_info gpueb_plat_mbox_table[GPUEB_MBOX_TOTAL] = {
    {
        0, // mbdev  :mbox device
        0, // irq_num:identity of mbox irq
        0, // id     :mbox id
        GPUEB_SLOT_NUM_PER_MBOX,// slot   :how many slots that mbox used, up to 1GB
        1, // opt    :option for mbox or share memory, 0:mbox, 1:share memory
        1, // enable :mbox status, 0:disable, 1: enable
        0, // is64d  :mbox is64d status, 0:32d, 1: 64d
        0, // base   :mbox base address
        0, // set_irq_reg  : mbox set irq register
        0, // clr_irq_reg  : mbox clear irq register
        0, // init_base_reg: mbox initialize register
        0, // send_status_reg
        0, // recv_status_reg
        { { { { 0 } } } }, // mbox lock : lock of mbox
        {0, 0, 0} // mtk_mbox_record: mbox record information
    },
    {
        0, // mbdev  :mbox device
        0, // irq_num:identity of mbox irq
        1, // id     :mbox id
        GPUEB_SLOT_NUM_PER_MBOX,// slot   :how many slots that mbox used, up to 1GB
        1, // opt    :option for mbox or share memory, 0:mbox, 1:share memory
        1, // enable :mbox status, 0:disable, 1: enable
        0, // is64d  :mbox is64d status, 0:32d, 1: 64d
        0, // base   :mbox base address
        0, // set_irq_reg  : mbox set irq register
        0, // clr_irq_reg  : mbox clear irq register
        0, // init_base_reg: mbox initialize register
        0, // send_status_reg
        0, // recv_status_reg
        { { { { 0 } } } }, // mbox lock    : lock of mbox
        {0, 0, 0} // mtk_mbox_record: mbox record information
    },
    {
        0, // mbdev  :mbox device
        0, // irq_num:identity of mbox irq
        2, // id     :mbox id
        GPUEB_SLOT_NUM_PER_MBOX,// slot   :how many slots that mbox used, up to 1GB
        1, // opt    :option for mbox or share memory, 0:mbox, 1:share memory
        1, // enable :mbox status, 0:disable, 1: enable
        0, // is64d  :mbox is64d status, 0:32d, 1: 64d
        0, // base   :mbox base address
        0, // set_irq_reg  : mbox set irq register
        0, // clr_irq_reg  : mbox clear irq register
        0, // init_base_reg: mbox initialize register
        0, // send_status_reg
        0, // recv_status_reg
        { { { { 0 } } } }, // mbox lock    : lock of mbox
        {0, 0, 0} // mtk_mbox_record: mbox record information
    },
    {
        0, // mbdev  :mbox device
        0, // irq_num:identity of mbox irq
        3, // id     :mbox id
        GPUEB_SLOT_NUM_PER_MBOX,// slot   :how many slots that mbox used, up to 1GB
        1, // opt    :option for mbox or share memory, 0:mbox, 1:share memory
        1, // enable :mbox status, 0:disable, 1: enable
        0, // is64d  :mbox is64d status, 0:32d, 1: 64d
        0, // base   :mbox base address
        0, // set_irq_reg  : mbox set irq register
        0, // clr_irq_reg  : mbox clear irq register
        0, // init_base_reg: mbox initialize register
        0, // send_status_reg
        0, // recv_status_reg
        { { { { 0 } } } }, // mbox lock    : lock of mbox
        {0, 0, 0} // mtk_mbox_record: mbox record information
    },
};

struct mtk_mbox_pin_send gpueb_plat_mbox_pin_send[GPUEB_IPI_COUNT] = {
    {
        0, // mbox number
        PIN_S_OFFSET_PLATFORM, // msg offset in share memory
        1, // send opt, 0:send, 1: send for response
        0, // polling lock 0:unuse, 1:used
        PIN_S_MSG_SIZE_PLATFORM, // message size in words, 4 bytes alignment
        0, // pin index in the mbox
        CH_PLATFORM, // ipi channel id
        { { 0 } }, // mutex for remote response
        { 0 }, // completion for remote response
        { { { { 0 } } } } // lock of the pin
    },
        {
        1, // mbox number
        PIN_S_OFFSET_DVFS, // msg offset in share memory
        1, // send opt, 0:send, 1: send for response
        0, // polling lock 0:unuse, 1:used
        PIN_S_MSG_SIZE_DVFS, // message size in words, 4 bytes alignment
        1, // pin index in the mbox
        CH_DVFS, // ipi channel id
        { { 0 } }, // mutex for remote response
        { 0 }, // completion for remote response
        { { { { 0 } } } } // lock of the pin
    },
        {
        2, // mbox number
        PIN_S_OFFSET_SLEEP, // msg offset in share memory
        1, // send opt, 0:send, 1: send for response
        0, // polling lock 0:unuse, 1:used
        PIN_S_MSG_SIZE_SLEEP, // message size in words, 4 bytes alignment
        2, // pin index in the mbox
        CH_SLEEP, // ipi channel id
        { { 0 } }, // mutex for remote response
        { 0 }, // completion for remote response
        { { { { 0 } } } } // lock of the pin
    },
        {
        3, // mbox number
        PIN_S_OFFSET_TIMER, // msg offset in share memory
        1, // send opt, 0:send, 1: send for response
        0, // polling lock 0:unuse, 1:used
        PIN_S_MSG_SIZE_TIMER, // message size in words, 4 bytes alignment
        3, // pin index in the mbox
        CH_SLEEP, // ipi channel id
        { { 0 } }, // mutex for remote response
        { 0 },  // completion for remote response
        { { { { 0 } } } } // lock of the pin
    },
};

struct mtk_mbox_pin_recv gpueb_plat_mbox_pin_recv[GPUEB_IPI_COUNT] = {
    {
        0, // mbox number
        PIN_R_OFFSET_PLATFORM, // msg offset
        1, // recv option 0:receive, 1: response
        0, // polling lock 0:unuse, 1:used
        1, // buffer full option 0:drop, 1:assert
        0, // callback option 0:isr, 1:process
        PIN_R_MSG_SIZE_PLATFORM, // msg used slots in the mbox
        0, // pin index in the mbox
        CH_PLATFORM, // ipi channel id
        { 0 }, // notify process
        0, // cb function
        0, // buffer pointer
        0, // private data
        { { { { 0 } } } }, // lock of the pin
        {0, 0, 0, 0, 0, 0} // record
    },
    {
        1, // mbox number
        PIN_R_OFFSET_DVFS, // msg offset
        1, // recv option 0:receive, 1: response
        0, // polling lock 0:unuse, 1:used
        1, // buffer full option 0:drop, 1:assert
        0, // callback option 0:isr, 1:process
        PIN_R_MSG_SIZE_DVFS, // msg used slots in the mbox
        1, // pin index in the mbox
        CH_DVFS, // ipi channel id
        { 0 }, // notify process
        0, // cb function
        0, // buffer pointer
        0, // private data
        { { { { 0 } } } }, // lock of the pin
        {0, 0, 0, 0, 0, 0} // record
    },
    {
        2, // mbox number
        PIN_R_OFFSET_SLEEP, // msg offset
        1, // recv option 0:receive, 1: response
        0, // polling lock 0:unuse, 1:used
        1, // buffer full option 0:drop, 1:assert
        0, // callback option 0:isr, 1:process
        PIN_R_MSG_SIZE_SLEEP, // msg used slots in the mbox
        2, // pin index in the mbox
        CH_SLEEP, // ipi channel id
        { 0 }, // notify process
        0, // cb function
        0, // buffer pointer
        0, // private data
        { { { { 0 } } } }, // lock of the pin
        {0, 0, 0, 0, 0, 0} // record
    },
    {
        3, // mbox number
        PIN_R_OFFSET_TIMER, // msg offset
        1, // recv option 0:receive, 1: response
        0, // polling lock 0:unuse, 1:used
        1, // buffer full option 0:drop, 1:assert
        0, // callback option 0:isr, 1:process
        PIN_R_MSG_SIZE_TIMER, // msg used slots in the mbox
        3, // pin index in the mbox
        CH_TIMER, // ipi channel id
        { 0 }, // notify process
        0, // cb function
        0, // buffer pointer
        0, // private data
        { { { { 0 } } } }, // lock of the pin
        {0, 0, 0, 0, 0, 0} // record
    },
};

struct mtk_mbox_device gpueb_plat_mboxdev = {
    .name = "gpueb_mboxdev",
    .pin_recv_table = &gpueb_plat_mbox_pin_recv[0],
    .pin_send_table = &gpueb_plat_mbox_pin_send[0],
    .info_table = &gpueb_plat_mbox_table[0],
    .count = GPUEB_MBOX_TOTAL,
    .recv_count = GPUEB_TOTAL_RECV_PIN,
    .send_count = GPUEB_TOTAL_SEND_PIN,
};

void gpueb_plat_ipi_timeout_cb(int ipi_id)
{
    gpueb_pr_debug("Error: possible error IPI %d \n", ipi_id);

    ipi_monitor_dump(&gpueb_plat_ipidev);
    //mtk_emidbg_dump();
    //BUG_ON(1);

    return;
}

struct mtk_ipi_device gpueb_plat_ipidev = {
    .name = "gpueb_ipidev",
    .id = IPI_DEV_GPUEB,
    .mbdev = &gpueb_plat_mboxdev,
    .timeout_handler = gpueb_plat_ipi_timeout_cb,
};

int gpueb_plat_get_channelID_by_name(char *channel_name)
{
    if (!strcmp(channel_name, "CH_PLATFORM")) 
        return CH_PLATFORM;
    else if (!strcmp(channel_name, "CH_DVFS"))
        return CH_DVFS;
    else if (!strcmp(channel_name, "CH_SLEEP"))
        return CH_SLEEP;
    else if (!strcmp(channel_name, "CH_TIMER"))
        return CH_TIMER;

    return -1;
}

bool gpueb_plat_is_bringup(void)
{
    return MT_GPUEB_BRINGUP;
}

bool gpueb_plat_is_logger_support(void)
{
    return MT_GPUEB_LOGGER_ENABLE;
}

bool gpueb_plat_is_ipi_test_support(void)
{
    return MT_GPUEB_IPI_TEST;
}