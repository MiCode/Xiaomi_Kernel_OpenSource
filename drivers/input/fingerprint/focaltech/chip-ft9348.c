/**
 * ${ANDROID_BUILD_TOP}/vendor/focaltech/src/chips/ft9348.c
 *
 * Copyright (C) 2014-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
**/

#include <linux/delay.h>

#include "ff_log.h"
#include "ff_err.h"
#include "ff_spi.h"
#include "ff_chip.h"
#include <teei_fp.h>
#include <tee_client_api.h>
//#include "../fp_drv/fp_drv.h"
# undef LOG_TAG
#define LOG_TAG "focaltech:ft9348"



//liukangping@huaqin.con add fingerprint hardinfo begin 
#ifdef CONFIG_HQ_HARDWARE_INFO
#include <linux/hardware_info.h>
#define FOCAL_FINGER		"FOCALTECH"
#endif

//liukangping@huaqin.con add fingerprint hardinfo end


/*
 * Protocol commands.
 */
#define FT9348_CMD_DMA_ENTER  0x06
#define FT9348_CMD_SFR_WRITE  0x09
#define FT9348_CMD_SFR_READ   0x08
#define FT9348_CMD_SRAM_WRITE 0x05
#define FT9348_CMD_SRAM_READ  0x04
#define FT9348_CMD_INFO_WRITE 0x10
#define FT9348_CMD_INFO_READ  0x11
#define FT9348_CMD_BOOT_WRITE 0x11


#define  WorkMode_Idle_Cmd     {0xC0, 0x3F, 0x00}
#define  WorkMode_Sleep_Cmd    {0xC1, 0x3E, 0x00}
#define  WorkMode_Fdt_Cmd      {0xC2, 0x3D, 0x00}
#define  WorkMode_Img_Cmd      {0xC4, 0x3B, 0x00}
#define  WorkMode_Nav_Cmd      {0xC8, 0x37, 0x00}
#define  WorkMode_SysRst_Cmd   {0xD8, 0x27, 0x00}
#define  WorkMode_AfeRst_Cmd   {0xD1, 0x2E, 0x00}
#define  WorkMode_FdtRst_Cmd   {0xD2, 0x2D, 0x00}
#define  WorkMode_FifoRst_Cmd  {0xD4, 0x2B, 0x00}
#define  WorkMode_OscOn_Cmd    {0x5A, 0xA5, 0x00}
#define  WorkMode_OscOff_Cmd   {0xA5, 0x5A, 0x00}
#define  WorkMode_SpiWakeUp    {0x70, 0x00, 0x00}

/*read id retry 5 times*/
#define RETRY_TIMES 5

/*
 * App info offset in XRAM.
 */
#define FT9348_INFO_OFFSET (0x0500 * 2)

/*
 * SPI exchange buffer offset in XRAM.
 */
#define FT9348_XBUF_OFFSET (0x05c0 * 2)

/*
 * Boot Info data structure.
 * Note: Big-endian.
 */
typedef struct __attribute__((__packed__)) {
    uint8_t  e_WorkState;
    uint8_t  e_RstType;
    uint16_t e_ChipId;
    uint8_t  e_VendorId;
    uint8_t  e_FactoryId;
    uint8_t  e_PromVersion;
    uint8_t  e_AppValid;
    uint8_t  e_UpgradeFlg;
    uint8_t  e_FlashType;
    uint8_t  e_FwType;
    uint8_t  e_CheckAppValid;
    uint8_t  e_Dummy0;
    uint8_t  e_Dummy1;
    uint8_t  e_Reserved[147];
    uint8_t  e_WO_SoftReset;
    uint8_t  e_WO_StartApp;
    uint8_t  e_Dummy3;
    uint8_t  e_WO_EnterUpgrade;
    uint8_t  e_WO_VerifyApp;
} ft9348_boot_info_t;

/*
 * App Info data structure.
 * Note: Big-endian.
 */
typedef struct __attribute__((__packed__)) {
    uint8_t  e_WorkState; // 0x00
    uint8_t  e_ImageBitsWide;
    uint8_t  e_LockAgc;
    uint8_t  e_Agc1;
    uint8_t  e_Agc2;
    uint8_t  e_Agc3;
    uint8_t  e_Agc4;
    uint16_t e_DAC1;
    uint16_t e_DAC2;
    uint16_t e_DAC_Offset;
    uint32_t e_SM_Threshold;
    uint8_t  e_NormalScanTimes;
    uint8_t  e_SM_ScanTimems;
    uint8_t  e_SM_TrigerBlkNum;
    uint8_t  e_SensorX; // 0x14
    uint8_t  e_SensorY;
    uint16_t e_ChipId;  // 0x16
    uint8_t  e_VendorId;
    uint8_t  e_FactoryId;
    uint8_t  e_FirmwareVersion; // 0x1a
    uint8_t  e_HighVoltageFlag;
    uint8_t  e_MpIntSetLowTime;
    uint8_t  e_FingerOnSensorFlag; // 0x1d
    uint8_t  e_AutoPowerAdjust;    // 0x1e
    uint8_t  e_PlatformFeatureEn;  // 0x1f
    uint16_t e_McuFreeFlag;        // 0x20
    uint8_t  e_AutoPowerTimeWdsD;
    uint8_t  e_AutoPowerTimeWdsU;
    uint8_t  e_FastSfrAddr;
    uint8_t  e_FastSfrValue;
    uint8_t  e_FastSfrRWFlag;
    uint8_t  e_OtpAddrR;
    uint8_t  e_OtpNumR;
    uint8_t  e_OtpReadEn;
    uint8_t  e_OtpBuff[10];
    uint8_t  e_InternalIoVcc;
    uint8_t  e_InternalIoVccLock;
    uint8_t  e_SMThresholdUpdateEn;
    uint8_t  e_AutoSetTeeFuntion;
    uint8_t  e_SMThresholdVirtTouch;
    uint8_t  e_SMThresholdVirtTouchDef;
    uint16_t e_GestureStatus; // 0x3a
    uint8_t  e_AgcVersion;    // 0x3c
    uint8_t  e_AutoPowerFastEn;
    uint8_t  e_FactoryIdleStopMode;
    uint8_t  e_ImageFingerFuntEn;
    uint8_t  e_TeeWorkMode;   // 0x40
    uint8_t  e_SMScanRate;
    uint8_t  e_SmartSmStable; // 0x42
    uint8_t  e_GestureSupport;
    uint8_t  e_ImageEccEn;
    uint16_t e_ImageEcc;
    uint8_t  e_SmSmartAreaBlkNum;
    uint8_t  e_SmSmartAreaContinueCnt;
    uint8_t  e_SmSmartAreaActionCnt;
    uint8_t  e_SmTriggerNum[2];
    uint8_t  e_SmTotalValue[8];
    uint8_t  e_FastEnterAPA; // 0x54
    uint8_t  e_Reserved[11];
    uint16_t e_AppInfoAddr;  // 0x60
    uint16_t e_BadPixelThrUpper;
    uint16_t e_BadPixelThrLower;
    uint8_t  e_BadPixelNum;
    uint8_t  e_RepairBadPixelEn;
    uint8_t  e_PollingScanEn;
    uint8_t  e_SpecialAldoSetEn;
    uint8_t  e_BadPixelAddrX[6];
    uint8_t  e_BadPixelAddrY[6];
    uint8_t  e_Dummy0;
    uint8_t  e_Dummy1;
    uint8_t  e_Dummy0_H;
    uint8_t  e_Dummy0_L;
    uint8_t  e_Dummy1_H;
    uint8_t  e_Dummy1_L;
} ft9348_app_info_t;

/*
 * The singleton instance of 'ft9348_context_t'.
 */
typedef struct {
    ff_device_t device;
    ft9348_boot_info_t boot_info;
    ft9348_app_info_t app_info;
    ff_device_mode_t work_mode;
} ft9348_context_t;
static ft9348_context_t ft9348_context = {
        .device.info.chip_id = 0x95a8,
        .work_mode = FF_DEVICE_MODE_IDLE,
}, *g_context = &ft9348_context;

static struct TEEC_UUID vendor_uuid = {
    0x04190000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

int ft9348_write_sfr(uint8_t addr, uint8_t data)
{
    ff_sfr_buf_t tx_buf;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    tx_buf.cmd[0]  = (uint8_t)( FT9348_CMD_SFR_WRITE);
    tx_buf.cmd[1]  = (uint8_t)(~FT9348_CMD_SFR_WRITE);
    tx_buf.addr    = addr;
    tx_buf.tx_byte = data;
    FF_CHECK_ERR(ff_spi_write_buf(&tx_buf, 4));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_read_sfr(uint8_t addr, uint8_t *data)
{
    ff_sfr_buf_t tx_buf;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    tx_buf.cmd[0] = (uint8_t)( FT9348_CMD_SFR_READ);
    tx_buf.cmd[1] = (uint8_t)(~FT9348_CMD_SFR_READ);
    tx_buf.addr   = addr;
    FF_CHECK_ERR(ff_spi_write_then_read_buf(&tx_buf, 4, data, 1));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}
uint8_t const FW8064_WorkMode_Cmd[12][3] = {WorkMode_Idle_Cmd,\
                                   WorkMode_Sleep_Cmd,\
                                   WorkMode_Fdt_Cmd,\
                                   WorkMode_Img_Cmd,\
                                   WorkMode_Nav_Cmd,\
                                   WorkMode_SysRst_Cmd,\
                                   WorkMode_AfeRst_Cmd,\
                                   WorkMode_FdtRst_Cmd,\
                                   WorkMode_FifoRst_Cmd,\
                                   WorkMode_OscOn_Cmd,\
                                   WorkMode_OscOff_Cmd,\
                                   WorkMode_SpiWakeUp};
int fw8064_wm_switch(E_WORKMODE_FW workmode)
{
    int err = FF_SUCCESS;
    uint8_t ucBuffCmd[64];
    
    FF_LOGV("'%s' enter.", __func__);

    ucBuffCmd[0] = FW8064_WorkMode_Cmd[workmode][0];
    ucBuffCmd[1] = FW8064_WorkMode_Cmd[workmode][1];
    ucBuffCmd[2] = FW8064_WorkMode_Cmd[workmode][2];

    if (e_WorkMode_Max > workmode)
    {
        if (e_WorkMode_SpiWakeUp == workmode)
        {
            err = ff_spi_write_buf(ucBuffCmd, 1);
        }
        else
        {
            err = ff_spi_write_buf(ucBuffCmd, 3);
        }
    }
    else
    {
        FF_LOGI("'%s' Cmd Error.", __func__);
        return FF_ERR_INTERNAL;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

void fw8064_sram_write(uint16_t addr, uint16_t data)
{
    int tx_len,dlen;
    static uint8_t tx_buffer[MAX_XFER_BUF_SIZE] = {0, };
    ff_sram_buf_t *tx_buf = TYPE_OF(ff_sram_buf_t, tx_buffer);
    
    //FF_LOGV("'%s' enter.", __func__);

    /* Sign the package.  */
    tx_buf->cmd[0] = (uint8_t)( FT9348_CMD_SRAM_WRITE);
    tx_buf->cmd[1] = (uint8_t)(~FT9348_CMD_SRAM_WRITE);

    /* Write it repeatedly. */
    dlen = 2;

    /* Packing. */
    tx_buf->addr = u16_swap_endian(addr | 0x8000); //×îžßbitÎª1
    tx_buf->dlen = u16_swap_endian((dlen/2)?(dlen/2-1):(dlen/2));
    tx_len = sizeof(ff_sram_buf_t)/2*2 + dlen;
    tx_buf->data[0] = data >> 8; 
    tx_buf->data[1] = data & 0xff;

    /* Low-level transfer. */
    ff_spi_write_buf(tx_buf, tx_len);

    //FF_LOGV("'%s' leave.", __func__);
}
uint16_t fw8064_read_sram(uint16_t addr)
{
    int tx_len;
    int dlen; 
    uint8_t ucbuff[8];
    uint16_t ustemp;
    ff_sram_buf_t tx_buffer, *tx_buf = &tx_buffer;
    uint8_t *p_data = TYPE_OF(uint8_t, ucbuff);
    // FF_LOGV("'%s' enter.", __func__);

    /* Sign the package.  */
    tx_buf->cmd[0] = (uint8_t)( FT9348_CMD_SRAM_READ);
    tx_buf->cmd[1] = (uint8_t)(~FT9348_CMD_SRAM_READ);

    /* Read it repeatedly. */
    dlen = 2;

        /* Packing. */
    tx_buf->addr = u16_swap_endian(addr | 0x8000);
    tx_buf->dlen = u16_swap_endian((dlen/2)?(dlen/2-1):(dlen/2));
    tx_len = sizeof(ff_sram_buf_t)/2*2;

        /* Low-level transfer. */
    ff_spi_write_then_read_buf(tx_buf, tx_len, p_data, dlen);

    ustemp = p_data[0];
    ustemp = (ustemp << 8) + p_data[1];

    return ustemp;
}
uint16_t fw8064_chipid_get(void)
{
    uint16_t usAddr,usData;

    usAddr = 0x3500/2 + 0x0B;
    usData = fw8064_read_sram(usAddr);

    return usData;
}
void fw8064_int_mask_set(uint16_t usdata)
{
    uint16_t usAddr;

    usAddr = 0x3500/2 + 0x03;
    fw8064_sram_write(usAddr, usdata);
}

void fw8064_intflag_clear(uint16_t usdata)
{
    uint16_t usAddr;

    usAddr = 0x3500/2 + 0x04;
    fw8064_sram_write(usAddr, usdata);
}
int ft9348_write_sram(uint16_t addr, const void *data, uint16_t length)
{
    int err = FF_SUCCESS, remain = length, tx_len;
    int dlen, stride = MAX_XFER_BUF_SIZE - sizeof(ff_sram_buf_t);
    bool b_in_pram = (addr == 0x0000);
    uint16_t offset = addr;
    static uint8_t tx_buffer[MAX_XFER_BUF_SIZE] = {0, };
    ff_sram_buf_t *tx_buf = TYPE_OF(ff_sram_buf_t, tx_buffer);
    uint8_t *p_data = TYPE_OF(uint8_t, data);
    FF_LOGV("'%s' enter.", __func__);

    /* Sign the package.  */
    tx_buf->cmd[0] = (uint8_t)( FT9348_CMD_SRAM_WRITE);
    tx_buf->cmd[1] = (uint8_t)(~FT9348_CMD_SRAM_WRITE);

    /* Write it repeatedly. */
    while (remain > 0) {
        /* The last package? */
        if (remain < stride) {
            stride = remain;
        }

        /* HW specific protocol. */
        addr = b_in_pram ? (offset / 2) : 0x8000 | (offset / 2);
        dlen = stride / 2 - 1;
        if (dlen < 0) {
            dlen = 0;
        }

        /* Packing. */
        tx_buf->addr = u16_swap_endian(addr);
        tx_buf->dlen = u16_swap_endian(dlen);
        tx_len = sizeof(ff_sram_buf_t) + stride;
        memcpy(tx_buf->data, p_data, stride);

        /* Low-level transfer. */
        FF_CHECK_ERR(ff_spi_write_buf(tx_buf, tx_len));

        /* Next package. */
        offset += stride;
        p_data += stride;
        remain -= stride;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_read_sram(uint16_t addr, void *data, uint16_t length)
{
    int err = FF_SUCCESS, remain = length, tx_len;
    int dlen, stride = MAX_XFER_BUF_SIZE - sizeof(ff_sram_buf_t);
    bool b_in_pram = (addr == 0x0000);
    uint16_t offset = addr;
    ff_sram_buf_t tx_buffer, *tx_buf = &tx_buffer;
    uint8_t *p_data = TYPE_OF(uint8_t, data);
    FF_LOGV("'%s' enter.", __func__);

    /* Sign the package.  */
    tx_buf->cmd[0] = (uint8_t)( FT9348_CMD_SRAM_READ);
    tx_buf->cmd[1] = (uint8_t)(~FT9348_CMD_SRAM_READ);

    /* Read it repeatedly. */
    while (remain > 0) {
        /* The last package? */
        if (remain < stride) {
            stride = remain;
        }

        /* HW specific protocol. */
        addr = b_in_pram ? (offset / 2) : 0x8000 | (offset / 2);
        dlen = stride / 2 - 1;
        if (dlen < 0) {
            dlen = 0;
        }

        /* Packing. */
        tx_buf->addr = u16_swap_endian(addr);
        tx_buf->dlen = u16_swap_endian(dlen);
        tx_len = sizeof(ff_sram_buf_t);

        /* Low-level transfer. */
        FF_CHECK_ERR(ff_spi_write_then_read_buf(tx_buf, tx_len, p_data, stride));

        /* Next package. */
        offset += stride;
        p_data += stride;
        remain -= stride;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_write_inf(uint8_t addr, uint8_t data)
{
    ff_info_buf_t info;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    /* 2-1: Write the info parameters. */
    info.cmd[0] = (uint8_t)( FT9348_CMD_INFO_WRITE);
    info.cmd[1] = (uint8_t)(~FT9348_CMD_INFO_WRITE);

    info.addr = addr;
    info.dlen = data;
    err = ft9348_write_sram(FT9348_XBUF_OFFSET, &info, sizeof(ff_info_buf_t));
    FF_CHECK_ERR(err);

    /* 2-2: Triger the operation. */
    FF_CHECK_ERR(ft9348_write_sfr(0xa4, 0x01));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

static int ft9348_write_info(uint8_t addr, uint8_t data, bool in_boot)
{
    ff_info_buf_t info;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    /* 2-1: Write the info parameters. */
    if (in_boot) {
        info.cmd[0] = (uint8_t)( FT9348_CMD_BOOT_WRITE);
        info.cmd[1] = (uint8_t)(~FT9348_CMD_BOOT_WRITE);
    } else {
        info.cmd[0] = (uint8_t)( FT9348_CMD_INFO_WRITE);
        info.cmd[1] = (uint8_t)(~FT9348_CMD_INFO_WRITE);
    }
    info.addr = in_boot ? (addr | 0x80) : addr;
    info.dlen = data;
    err = ft9348_write_sram(FT9348_XBUF_OFFSET, &info, sizeof(ff_info_buf_t));
    FF_CHECK_ERR(err);

    /* 2-2: Triger the operation. */
    FF_CHECK_ERR(ft9348_write_sfr(0xa4, 0x01));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

static int ft9348_read_info_indirectly(uint8_t offset, void *data, uint8_t dlen)
{
    ff_info_buf_t info;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    /* 3-1: Write the info parameters. */
    info.cmd[0] = (uint8_t)( FT9348_CMD_INFO_READ);
    info.cmd[1] = (uint8_t)(~FT9348_CMD_INFO_READ);
    info.addr = offset;
    info.dlen = dlen;
    err = ft9348_write_sram(FT9348_XBUF_OFFSET, &info, sizeof(ff_info_buf_t));
    FF_CHECK_ERR(err);

    /* 3-2: Triger the operation. */
    FF_CHECK_ERR(ft9348_write_sfr(0xa4, 0x01));

    /* 3-3: Read out the result. */
    FF_CHECK_ERR(ft9348_read_sram(FT9348_XBUF_OFFSET, data, dlen));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

/* Note: APP space only. */
static int ft9348_read_info_directly(uint8_t offset, void *data, uint8_t dlen)
{
    uint16_t addr = FT9348_INFO_OFFSET + offset;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    // FIXME: Reading from an odd offset.
    if (addr % 2) {
        data = ((uint8_t *)data) - 1;
        dlen += 1;
    }
    FF_CHECK_ERR(ft9348_read_sram(addr, data, dlen));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

/*
 * Note: Only 1 byte can be written.
 */
#define ft9348_write_boot_info(member)                                           \
do {                                                                             \
    uint8_t offset = (uint8_t)((void *)&member - (void *)&g_context->boot_info); \
    FF_CHECK_ERR(ft9348_write_info(offset, member, true));                       \
} while (0)

/*
 * Note: Only 2 bytes can be read.
 */
#define ft9348_read_boot_info(member)                                            \
do {                                                                             \
    uint8_t offset = (uint8_t)((void *)&member - (void *)&g_context->boot_info); \
    uint8_t length = (uint8_t)sizeof(member);                                    \
    FF_CHECK_ERR(ft9348_read_info_indirectly(offset, &member, length));          \
} while (0)

#define ft9348_write_app_info(member)                                            \
do {                                                                             \
    uint8_t offset = (uint8_t)((void *)&member - (void *)&g_context->app_info);  \
    FF_CHECK_ERR(ft9348_write_info(offset, member, false));                      \
} while (0)

#define ft9348_read_app_info_indirectly(member)                                  \
do {                                                                             \
    uint8_t offset = (uint8_t)((void *)&member - (void *)&g_context->app_info);  \
    uint8_t length = (uint8_t)sizeof(member);                                    \
    FF_CHECK_ERR(ft9348_read_info_indirectly(offset, &member, length));          \
} while (0)

#define ft9348_read_app_info_directly(member)                                    \
do {                                                                             \
    uint8_t offset = (uint8_t)((void *)&member - (void *)&g_context->app_info);  \
    uint8_t length = (uint8_t)sizeof(member);                                    \
    FF_CHECK_ERR(ft9348_read_info_directly(offset, &member, length));            \
} while (0)

/*---8<-------------------------------------------------------------------------*/

int ft9348_query_event_status(void)
{
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    // TODO:
    UNUSED_VAR(err);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_query_device_status(void)
{
    ff_device_status_t stat = FF_DEVICE_STAT_IDLE;
    FF_LOGV("'%s' enter.", __func__);

    ft9348_read_app_info_directly(g_context->app_info.e_McuFreeFlag);
    if (g_context->app_info.e_McuFreeFlag != 0x5aa5) {
        stat = FF_DEVICE_STAT_BUSY;
    }

    FF_LOGV("'%s' leave. ", __func__);
    return stat;
}

int ft9348_query_finger_status(void)
{
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    // TODO:
    UNUSED_VAR(err);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_query_gesture_status(void)
{
    FF_LOGV("'%s' enter.", __func__);

    ft9348_read_app_info_directly(g_context->app_info.e_GestureStatus);
    FF_LOGD("fw gesture_code = 0x%x.", g_context->app_info.e_GestureStatus);

    FF_LOGV("'%s' leave.", __func__);
    return (int)g_context->app_info.e_GestureStatus;
}

int ft9348_check_alive(void)
{
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    ft9348_read_app_info_directly(g_context->app_info.e_AutoPowerAdjust);
    if (g_context->app_info.e_AutoPowerAdjust != 0x01) {
        return FF_ERR_DEAD_DEVICE;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

static int ft9348_init_sleep_mode(void)
{
    ff_device_status_t stat = FF_DEVICE_STAT_BUSY;
    int err = FF_SUCCESS, tries = 50;
    FF_LOGV("'%s' enter.", __func__);

    /* TODO: Algorithm has wrote these two info registers already. */
    g_context->app_info.e_PlatformFeatureEn = 0x01; // 0x1f
    ft9348_write_app_info(g_context->app_info.e_PlatformFeatureEn);
    g_context->app_info.e_TeeWorkMode = 0x01;       // 0x40
    ft9348_write_app_info(g_context->app_info.e_TeeWorkMode);

    g_context->app_info.e_AutoPowerAdjust = 0x01;   // 0x1e
    ft9348_write_app_info(g_context->app_info.e_AutoPowerAdjust);

    /* Wait for the device to be IDLE state. */
    do {
        mdelay(3);
        stat = ft9348_query_device_status();
    } while (stat != FF_DEVICE_STAT_IDLE && --tries);
    FF_LOGV("tries remainder = %d", tries);

    /* INT pin shall pull down after reading the 0x1d info. */
    ft9348_read_app_info_indirectly(g_context->app_info.e_FingerOnSensorFlag);

    /* Verify that the device has been in the sensor mode. */
    stat = ft9348_query_device_status();
    if (stat != FF_DEVICE_STAT_BUSY) {
        FF_LOGE("failed to init sensor mode.");
        err = FF_ERR_INTERNAL;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_config_power_mode(ff_power_mode_t mode)
{
    int err = FF_SUCCESS, tries = 10;
    FF_LOGV("'%s' enter.", __func__);

    switch (mode) {
    case FF_POWER_MODE_WAKEUP: {
        ff_device_status_t stat = ft9348_query_device_status();
        if (stat != FF_DEVICE_STAT_IDLE) {
            /* Try to wakeup the device. */
            do {
                ff_sfr_buf_t tx_buf;
                tx_buf.cmd[0] = 0x70;
                FF_CHECK_ERR(ff_spi_write_buf(&tx_buf, 1));
                mdelay(5);
                FF_CHECK_ERR(ff_spi_write_buf(&tx_buf, 1));
                mdelay(20);
                stat = ft9348_query_device_status();
            } while (stat != FF_DEVICE_STAT_IDLE && --tries);
            FF_LOGV("tries remainder = %d", tries);
            if (stat == FF_DEVICE_STAT_BUSY) {
                FF_LOGE("can't wake up the device.");
                FF_CHECK_ERR(FF_ERR_BUSY);
            }
        }
        break;
    }
    case FF_POWER_MODE_INIT_SLEEP: {
        FF_FAILURE_RETRY(ft9348_init_sleep_mode(), 3);
        break;
    }
    case FF_POWER_MODE_AUTO_SLEEP: {
        g_context->app_info.e_WorkState = 0x00;
        ft9348_write_app_info(g_context->app_info.e_WorkState);

        g_context->app_info.e_FastEnterAPA = 0x01;
        ft9348_write_app_info(g_context->app_info.e_FastEnterAPA);
        break;
    }
    case FF_POWER_MODE_DEEP_SLEEP:
        FF_LOGI("switch to 'FF_POWER_MODE_DEEP_SLEEP' mode.");
        err = ft9348_config_power_mode(FF_POWER_MODE_WAKEUP);
        g_context->app_info.e_WorkState = 0x73;
        ft9348_write_app_info(g_context->app_info.e_WorkState);
        break;
    case FF_POWER_MODE_LOST_POWER:
        // TODO:
        break;
    default:
        FF_LOGW("unknown power mode.");
        break;
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

int ft9348_config_device_mode(ff_device_mode_t mode)
{
    const char *hint = "switch to '%s' mode.";
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    switch (mode) {
#if 0
    case FF_DEVICE_MODE_IDLE: {
        err = ft9348_config_power_mode(FF_POWER_MODE_WAKEUP);
        /* Enter deep sleep mode instead of idle mode. */
        err = ft9348_config_power_mode(FF_POWER_MODE_DEEP_SLEEP);
        break;
    }
#endif
    case FF_DEVICE_MODE_SCAN_IMAGE: {
        FF_LOGI(hint, "FF_DEVICE_MODE_SCAN_IMAGE");
        // FIXME: There is no this mode for ft9348.
        break;
    }
    case FF_DEVICE_MODE_GESTURE: {
        // TODO:
        break;
    }
    case FF_DEVICE_MODE_WAIT_TOUCH:
    case FF_DEVICE_MODE_WAIT_LEAVE:
    case FF_DEVICE_MODE_WAIT_IMAGE:
    case FF_DEVICE_MODE_IDLE:
    default:
        FF_LOGI(hint, "FF_DEVICE_MODE_SENSOR");
        err = ft9348_config_power_mode(FF_POWER_MODE_WAKEUP);
        err = ft9348_config_power_mode(FF_POWER_MODE_AUTO_SLEEP);
        break;
    }

    g_context->work_mode = mode;
    FF_LOGV("'%s' leave.", __func__);
    return err;
}

/*---8<-----------------------------------------------------------------------*/

/*
 * The firmware filename in 'firmwares'.
 */
static uint8_t g_firmware_data[] = {
    //#include "firmwares/FW9361_Coating_V30_D21_20180605_app.i"
    //#include "firmwares/FW9361_Coating_V30_D1D_20180626_app.i"
};

/* See plat-xxxx.c for platform dependent implementation. */
extern int ff_ctl_reset_device(void);

int ft9348_hw_reset(void)
{
    ff_sfr_buf_t tx_buf;
    int err = FF_SUCCESS;
    FF_LOGV("'%s' enter.", __func__);

    /* 3-1: HW reset. */
    err = ff_ctl_reset_device();

    /* 3-2: Wait PROM ready. */
    mdelay(20);

    /* 3-3: Enter SPI DMA mode. */
    tx_buf.cmd[0] = (uint8_t)( FT9348_CMD_DMA_ENTER);
    tx_buf.cmd[1] = (uint8_t)(~FT9348_CMD_DMA_ENTER);
    tx_buf.addr   = 0x00;
    err = ff_spi_write_buf(&tx_buf, 3);
    if (err) {
        FF_LOGE("failed to enter DMA mode.");
        FF_CHECK_ERR(err);
    }

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

static int ft9348_download_firmware(void)
{
    int err = FF_SUCCESS, tries = 100;
    FF_LOGV("'%s' enter.", __func__);

    /* 10-1: Reset to PROM space. */
    FF_CHECK_ERR(ft9348_hw_reset());

    /* 10-2: Enter firmware download mode. */
    g_context->boot_info.e_WO_EnterUpgrade = 0x01;
    ft9348_write_boot_info(g_context->boot_info.e_WO_EnterUpgrade);

    /* 10-3: Read out PROM version. (Optional) */
    ft9348_read_boot_info(g_context->boot_info.e_PromVersion);
    FF_LOGD("PROM version: 0x%02x.", g_context->boot_info.e_PromVersion);

    /* 10-4: Disable write-protection of PRAM. */
    FF_CHECK_ERR(ft9348_write_sfr(0xa3, 0xa5));

    /* 10-5: Download firmware. */
    FF_LOGD("g_firmware_blob->size = %d.", sizeof(g_firmware_data));
    FF_LOGI("downloading firmware...");
    //ff_util_hexdump(g_firmware_blob->binary, g_firmware_blob->size);
    FF_CHECK_ERR(ft9348_write_sram(0, g_firmware_data, (uint16_t)sizeof(g_firmware_data)));
    FF_LOGI("done.");

    /* 10-6: Enable write-protection of PRAM. */
    FF_CHECK_ERR(ft9348_write_sfr(0xa3, 0x00));

    /* 10-7: Tell MCU to verify the firmware. */
    FF_LOGI("verifying the firmware...");
    g_context->boot_info.e_WO_VerifyApp = 0x01;
    ft9348_write_boot_info(g_context->boot_info.e_WO_VerifyApp);

    /* 10-8: Wait for the verification. */
    do {
        mdelay(5);
        ft9348_read_boot_info(g_context->boot_info.e_CheckAppValid);
    } while (g_context->boot_info.e_CheckAppValid && --tries);

    /* 10-9: Is the firmware okay ? */
    ft9348_read_boot_info(g_context->boot_info.e_AppValid);
    if (!g_context->boot_info.e_AppValid) {
        FF_LOGE("firmware verification failed.");
        FF_CHECK_ERR(FF_ERR_INTERNAL);
    }
    FF_LOGI("passed.");

    /* 10-10: Start the App in PRAM. */
    g_context->boot_info.e_WO_StartApp = 0x01;
    ft9348_write_boot_info(g_context->boot_info.e_WO_StartApp);
    mdelay(100);

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

/*---8<-----------------------------------------------------------------------*/

int ff_chip_init(void)
{
    uint16_t device_id = 0x0000;
    int err = FF_SUCCESS,i=0;
    FF_LOGV("'%s' enter.", __func__);
    for (i=0;i<RETRY_TIMES;i++) {
        err = ff_ctl_reset_device();
        FF_LOGI("ff_ctl_reset_device= : %d ", err);
        err = fw8064_wm_switch(e_WorkMode_OscOn);
        FF_LOGI("e_WorkMode_OscOn= : %d ", err);
        err = fw8064_wm_switch(e_WorkMode_Idle);
        FF_LOGI("e_WorkMode_Idle= : %d ", err);
        mdelay(5);
        device_id = fw8064_chipid_get();
        FF_LOGI("chip id is = : 0x%04x ", device_id);
        if(device_id==0x9362){
            memcpy(&uuid_fp, &vendor_uuid, sizeof(struct TEEC_UUID));
            //REGISTER_FP_DEV_INFO("focaltech_fp", "HV0001-TV0001  FT9362", NULL, NULL, NULL);
            FF_LOGI("copy device_id to uuid-fp.....");
            break;
        }
    }
    return device_id;

    /* Try another way: Reset to PROM space and read boot info. */
    FF_LOGD("try to read the chip id in bootloader...");
    FF_CHECK_ERR(ft9348_hw_reset());
    ft9348_read_boot_info(g_context->boot_info.e_ChipId);
    device_id = u16_swap_endian(g_context->boot_info.e_ChipId);
    FF_LOGI("got chip id: 0x%04x", device_id);
    if(device_id != 0x95a8){
        fw8064_int_mask_set(0xffff);
        fw8064_intflag_clear(0xffff);
        err = fw8064_wm_switch(e_WorkMode_Sleep);
        FF_LOGI("fw8064_wm_switch return : %d ", err);
        return err;
    }
    /*
     * Download/Update the latest firmware if needs.
     *
     * TODO: Read out the firmware version and compare to the embedded one's
     * to decide whether to upgrade the firmware or not.
     */
     
    FF_LOGI("checking for firmware...");
    FF_FAILURE_RETRY(ft9348_download_firmware(), 0);

    /* firmware version info. */
    ft9348_read_app_info_directly(g_context->app_info.e_FirmwareVersion);
    ft9348_read_app_info_directly(g_context->app_info.e_AgcVersion);
    FF_LOGI("firmware version: v%02X-%02X", g_context->app_info.e_FirmwareVersion, g_context->app_info.e_AgcVersion);

    /* Sensor dimensions. */
    ft9348_read_app_info_directly(g_context->app_info.e_SensorX);
    ft9348_read_app_info_directly(g_context->app_info.e_SensorY);
    FF_LOGI("sensor resolution: %d x %d.", g_context->app_info.e_SensorX, g_context->app_info.e_SensorY);

    /* Enter SensorMode. */
    FF_CHECK_ERR(ft9348_config_power_mode(FF_POWER_MODE_INIT_SLEEP));
    FF_CHECK_ERR(ft9348_config_power_mode(FF_POWER_MODE_DEEP_SLEEP));

    FF_LOGV("'%s' leave.", __func__);
    return err;
}

