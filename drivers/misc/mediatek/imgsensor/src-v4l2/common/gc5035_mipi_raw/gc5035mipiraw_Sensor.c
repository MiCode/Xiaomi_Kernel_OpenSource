// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "gc5035mipiraw_Sensor.h"
#include "gc5035_ana_gain_table.h"



#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8_reg8(__VA_ARGS__)
//#define read_cmos_sensor(ctx, ...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8_reg8(__VA_ARGS__)
#define write_cmos_sensor(...)  write_cmos_sensor_8(ctx,__VA_ARGS__)
//#define write_cmos_sensor(ctx, ...) subdrv_i2c_wr_u16(__VA_ARGS__)
//#define table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8(__VA_ARGS__)
//#define table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u16(__VA_ARGS__)
//static DEFINE_SPINLOCK(imgsensor_drv_lock);
/* used for shutter compensation */


#define PFX "GC5035_camera_sensor"
#define LOG_INF(format, args...)\
    pr_debug(PFX "[%s] " format, __func__, ##args)
static kal_uint32 Dgain_ratio = GC5035_SENSOR_DGAIN_BASE;


static struct gc5035_otp_t gc5035_otp_data;

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = GC5035_SENSOR_ID,
    .checksum_value = 0xf61b7b7c,

    .pre = {
        .pclk = 87600000,
        .linelength = 1460,
        .framelength = 2008,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1296,
        .grabwindow_height = 972,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 87600000,
        .max_framerate = 300,
    },
    .cap = {
        .pclk = 175200000,
        .linelength = 2920,
        .framelength = 2008,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 175200000,
        .max_framerate = 300,
    },
    .cap1 = {
        .pclk = 141600000,
        .linelength = 2920,
        .framelength = 2008,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 141600000,
        .max_framerate = 240,             /*less than 13M(include 13M)*/
    },
    .normal_video = {
        .pclk = 175200000,
        .linelength = 2920,
        .framelength = 2008,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 175200000,
        .max_framerate = 300,
    },
    .hs_video = {
        .pclk = 175200000,
        .linelength = 1896,
        .framelength = 1536,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 175200000,
        .max_framerate = 600,
    },
    .slim_video = {
        .pclk = 87600000,
        .linelength = 1460,
        .framelength = 2008,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 85,
        .mipi_pixel_rate = 87600000,
        .max_framerate = 300,
    },
    .margin = 16,
    .min_shutter = 4,
    .min_gain = BASEGAIN,
    .max_gain = BASEGAIN * 16,
    .min_gain_iso = 100,
    .exp_step = 2,
    .gain_step = 1,
    .gain_type = 0,
    .max_frame_length = 0x3fff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,    //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 5,    //support sensor mode num

    .cap_delay_frame = 2,
    .pre_delay_frame = 2,
    .video_delay_frame = 2,
    .hs_video_delay_frame = 2,
    .slim_video_delay_frame = 2,

    .isp_driving_current = ISP_DRIVING_6MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_sensor_type = MIPI_OPHY_NCSI2,
    //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if GC5035_MIRROR_FLIP_ENABLE
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
#endif
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_2_LANE,
    .i2c_addr_table = {0x7e,0xFF},
};



/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
    { 2592, 1944,   0,   0, 2592, 1944, 1296,  972, 0, 0,
        1296,  972, 0, 0, 1296,  972 },
    { 2592, 1944,   0,   0, 2592, 1944, 2592, 1944, 0, 0,
        2592, 1944, 0, 0, 2592, 1944 },
    { 2592, 1944,   0,   0, 2592, 1944, 2592, 1944, 0, 0,
        2592, 1944, 0, 0, 2592, 1944 },
    { 2592, 1944, 656, 492, 1280,  960,  640,  480, 0, 0,
        640,  480, 0, 0,  640,  480 },
    { 2592, 1944,  16, 252, 2560, 1440, 1280,  720, 0, 0,
        1280,  720, 0, 0, 1280,  720 }
};

/************************************************************
*     OTP function   start
************************************************************/
static kal_uint8 gc5035_otp_read_byte(struct subdrv_ctx *ctx,kal_uint16 addr)
{
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
    write_cmos_sensor(0x6a, addr & 0xff);
    write_cmos_sensor(0xf3, 0x20);

    return read_cmos_sensor_8(ctx,0x6c);
}

static void gc5035_otp_read_group(struct subdrv_ctx *ctx,kal_uint16 addr,
    kal_uint8 *data, kal_uint16 length)
{
    kal_uint16 i = 0;

    if ((((addr & 0x1fff) >> 3) + length) > GC5035_OTP_DATA_LENGTH) {
        LOG_INF("out of range, start addr: 0x%.4x, length = %d\n", addr & 0x1fff, length);
        return;
    }

    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
    write_cmos_sensor(0x6a, addr & 0xff);
    write_cmos_sensor(0xf3, 0x20);
    write_cmos_sensor(0xf3, 0x12);

    for (i = 0; i < length; i++)
        data[i] = read_cmos_sensor_8(ctx,0x6c);

    write_cmos_sensor(0xf3, 0x00);
}


static void gc5035_gcore_read_dpc(struct subdrv_ctx *ctx)
{
    kal_uint8 dpcFlag = 0;
    struct gc5035_dpc_t *pDPC = &gc5035_otp_data.dpc;

    dpcFlag = gc5035_otp_read_byte(ctx,GC5035_OTP_DPC_FLAG_OFFSET);
    LOG_INF("dpc flag = 0x%x\n", dpcFlag);
    switch (GC5035_OTP_GET_2BIT_FLAG(dpcFlag, 0)) {
    case GC5035_OTP_FLAG_EMPTY: {
        LOG_INF("dpc info is empty!!\n");
        pDPC->flag = GC5035_OTP_FLAG_EMPTY;
        break;
    }
    case GC5035_OTP_FLAG_VALID: {
        LOG_INF("dpc info is valid!\n");
        pDPC->total_num = gc5035_otp_read_byte(ctx,GC5035_OTP_DPC_TOTAL_NUMBER_OFFSET)
            + gc5035_otp_read_byte(ctx,GC5035_OTP_DPC_ERROR_NUMBER_OFFSET);
        pDPC->flag = GC5035_OTP_FLAG_VALID;
        LOG_INF("total_num = %d\n", pDPC->total_num);
        break;
    }
    default:
        pDPC->flag = GC5035_OTP_FLAG_INVALID;
        break;
    }
}

static void gc5035_gcore_read_reg(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0;
    kal_uint8 j = 0;
    kal_uint16 base_group = 0;
    kal_uint8 reg[GC5035_OTP_REG_DATA_SIZE];
    struct gc5035_reg_update_t *pRegs = &gc5035_otp_data.regs;

    memset(&reg, 0, GC5035_OTP_REG_DATA_SIZE);
    pRegs->flag = gc5035_otp_read_byte(ctx,GC5035_OTP_REG_FLAG_OFFSET);
    LOG_INF("register update flag = 0x%x\n", pRegs->flag);
    if (pRegs->flag == GC5035_OTP_FLAG_VALID) {
        gc5035_otp_read_group(ctx,GC5035_OTP_REG_DATA_OFFSET, &reg[0], GC5035_OTP_REG_DATA_SIZE);

        for (i = 0; i < GC5035_OTP_REG_MAX_GROUP; i++) {
            base_group = i * GC5035_OTP_REG_BYTE_PER_GROUP;
            for (j = 0; j < GC5035_OTP_REG_REG_PER_GROUP; j++)
                if (GC5035_OTP_CHECK_1BIT_FLAG(reg[base_group], (4 * j + 3))) {
                    pRegs->reg[pRegs->cnt].page =
                        (reg[base_group] >> (4 * j)) & 0x07;
                    pRegs->reg[pRegs->cnt].addr =
                        reg[base_group + j * GC5035_OTP_REG_BYTE_PER_REG + 1];
                    pRegs->reg[pRegs->cnt].value =
                        reg[base_group + j * GC5035_OTP_REG_BYTE_PER_REG + 2];
                    LOG_INF("register[%d] P%d:0x%x->0x%x\n",
                        pRegs->cnt, pRegs->reg[pRegs->cnt].page,
                        pRegs->reg[pRegs->cnt].addr, pRegs->reg[pRegs->cnt].value);
                    pRegs->cnt++;
                }
        }

    }
}

#if GC5035_OTP_FOR_CUSTOMER
static kal_uint8 gc5035_otp_read_module_info(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0;
    kal_uint8 idx = 0;
    kal_uint8 flag = 0;
    kal_uint16 check = 0;
    kal_uint16 module_start_offset = GC5035_OTP_MODULE_DATA_OFFSET;
    kal_uint8 info[GC5035_OTP_MODULE_DATA_SIZE];
    struct gc5035_module_info_t module_info = { 0 };

    memset(&info, 0, GC5035_OTP_MODULE_DATA_SIZE);
    memset(&module_info, 0, sizeof(struct gc5035_module_info_t));

    flag = gc5035_otp_read_byte(ctx,GC5035_OTP_MODULE_FLAG_OFFSET);
    LOG_INF("flag = 0x%x\n", flag);

    for (idx = 0; idx < GC5035_OTP_GROUP_CNT; idx++) {
        switch (GC5035_OTP_GET_2BIT_FLAG(flag, 2 * (1 - idx))) {
        case GC5035_OTP_FLAG_EMPTY: {
            LOG_INF("group %d is empty!\n", idx + 1);
            break;
        }
        case GC5035_OTP_FLAG_VALID: {
            LOG_INF("group %d is valid!\n", idx + 1);
            module_start_offset = GC5035_OTP_MODULE_DATA_OFFSET
                + GC5035_OTP_GET_OFFSET(idx * GC5035_OTP_MODULE_DATA_SIZE);
            gc5035_otp_read_group(ctx,module_start_offset, &info[0], GC5035_OTP_MODULE_DATA_SIZE);
            for (i = 0; i < GC5035_OTP_MODULE_DATA_SIZE - 1; i++)
                check += info[i];

            if ((check % 255 + 1) == info[GC5035_OTP_MODULE_DATA_SIZE - 1]) {
                module_info.module_id = info[0];
                module_info.lens_id = info[1];
                module_info.year = info[2];
                module_info.month = info[3];
                module_info.day = info[4];

                LOG_INF("module_id = 0x%x\n", module_info.module_id);
                LOG_INF("lens_id = 0x%x\n", module_info.lens_id);
                LOG_INF("data = %d-%d-%d\n", module_info.year, module_info.month, module_info.day);
            } else
                LOG_INF("check sum %d error! check sum = 0x%x, calculate result = 0x%x\n",
                    idx + 1, info[GC5035_OTP_MODULE_DATA_SIZE - 1], (check % 255 + 1));
            break;
        }
        case GC5035_OTP_FLAG_INVALID:
        case GC5035_OTP_FLAG_INVALID2: {
            LOG_INF("group %d is invalid!\n", idx + 1);
            break;
        }
        default:
            break;
        }
    }

    return module_info.module_id;
}

static void gc5035_otp_read_wb_info(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0;
    kal_uint8 idx = 0;
    kal_uint8 flag = 0;
    kal_uint16 wb_check = 0;
    kal_uint16 golden_check = 0;
    kal_uint16 wb_start_offset = GC5035_OTP_WB_DATA_OFFSET;
    kal_uint16 golden_start_offset = GC5035_OTP_GOLDEN_DATA_OFFSET;
    kal_uint8 wb[GC5035_OTP_WB_DATA_SIZE];
    kal_uint8 golden[GC5035_OTP_GOLDEN_DATA_SIZE];
    struct gc5035_wb_t *pWB = &gc5035_otp_data.wb;
    struct gc5035_wb_t *pGolden = &gc5035_otp_data.golden;

    memset(&wb, 0, GC5035_OTP_WB_DATA_SIZE);
    memset(&golden, 0, GC5035_OTP_GOLDEN_DATA_SIZE);
    flag = gc5035_otp_read_byte(ctx,GC5035_OTP_WB_FLAG_OFFSET);
    LOG_INF("flag = 0x%x\n", flag);

    for (idx = 0; idx < GC5035_OTP_GROUP_CNT; idx++) {
        switch (GC5035_OTP_GET_2BIT_FLAG(flag, 2 * (1 - idx))) {
        case GC5035_OTP_FLAG_EMPTY: {
            LOG_INF("wb group %d is empty!\n", idx + 1);
            pWB->flag = pWB->flag | GC5035_OTP_FLAG_EMPTY;
            break;
        }
        case GC5035_OTP_FLAG_VALID: {
            LOG_INF("wb group %d is valid!\n", idx + 1);
            wb_start_offset = GC5035_OTP_WB_DATA_OFFSET
                + GC5035_OTP_GET_OFFSET(idx * GC5035_OTP_WB_DATA_SIZE);
            gc5035_otp_read_group(ctx,wb_start_offset, &wb[0], GC5035_OTP_WB_DATA_SIZE);

            for (i = 0; i < GC5035_OTP_WB_DATA_SIZE - 1; i++)
                wb_check += wb[i];

            if ((wb_check % 255 + 1) == wb[GC5035_OTP_WB_DATA_SIZE - 1]) {
                pWB->rg = (wb[0] | ((wb[1] & 0xf0) << 4));
                pWB->bg = (((wb[1] & 0x0f) << 8) | wb[2]);
                pWB->rg = pWB->rg == 0 ? GC5035_OTP_WB_RG_TYPICAL : pWB->rg;
                pWB->bg = pWB->bg == 0 ? GC5035_OTP_WB_BG_TYPICAL : pWB->bg;
                pWB->flag = pWB->flag | GC5035_OTP_FLAG_VALID;
                LOG_INF("wb r/g = 0x%x\n", pWB->rg);
                LOG_INF("wb b/g = 0x%x\n", pWB->bg);
            } else {
                pWB->flag = pWB->flag | GC5035_OTP_FLAG_INVALID;
                LOG_INF("wb check sum %d error! check sum = 0x%x, calculate result = 0x%x\n",
                    idx + 1, wb[GC5035_OTP_WB_DATA_SIZE - 1], (wb_check % 255 + 1));
            }
            break;
        }
        case GC5035_OTP_FLAG_INVALID:
        case GC5035_OTP_FLAG_INVALID2: {
            LOG_INF("wb group %d is invalid!\n", idx + 1);
            pWB->flag = pWB->flag | GC5035_OTP_FLAG_INVALID;
            break;
        }
        default:
            break;
        }

        switch (GC5035_OTP_GET_2BIT_FLAG(flag, 2 * (3 - idx))) {
        case GC5035_OTP_FLAG_EMPTY: {
            LOG_INF("golden group %d is empty!\n", idx + 1);
            pGolden->flag = pGolden->flag | GC5035_OTP_FLAG_EMPTY;
            break;
        }
        case GC5035_OTP_FLAG_VALID: {
            LOG_INF("golden group %d is valid!\n", idx + 1);
            golden_start_offset = GC5035_OTP_GOLDEN_DATA_OFFSET
                + GC5035_OTP_GET_OFFSET(idx * GC5035_OTP_GOLDEN_DATA_SIZE);
            gc5035_otp_read_group(golden_start_offset, &golden[0], GC5035_OTP_GOLDEN_DATA_SIZE);
            for (i = 0; i < GC5035_OTP_GOLDEN_DATA_SIZE - 1; i++)
                golden_check += golden[i];

            if ((golden_check % 255 + 1) == golden[GC5035_OTP_GOLDEN_DATA_SIZE - 1]) {
                pGolden->rg = (golden[0] | ((golden[1] & 0xf0) << 4));
                pGolden->bg = (((golden[1] & 0x0f) << 8) | golden[2]);
                pGolden->rg = pGolden->rg == 0 ? GC5035_OTP_WB_RG_TYPICAL : pGolden->rg;
                pGolden->bg = pGolden->bg == 0 ? GC5035_OTP_WB_BG_TYPICAL : pGolden->bg;
                pGolden->flag = pGolden->flag | GC5035_OTP_FLAG_VALID;
                LOG_INF("golden r/g = 0x%x\n", pGolden->rg);
                LOG_INF("golden b/g = 0x%x\n", pGolden->bg);
            } else {
                pGolden->flag = pGolden->flag | GC5035_OTP_FLAG_INVALID;
                LOG_INF("golden check sum %d error! check sum = 0x%x, calculate result = 0x%x\n",
                    idx + 1, golden[GC5035_OTP_WB_DATA_SIZE - 1], (golden_check % 255 + 1));
            }
            break;
        }
        case GC5035_OTP_FLAG_INVALID:
        case GC5035_OTP_FLAG_INVALID2: {
            LOG_INF("golden group %d is invalid!\n", idx + 1);
            pGolden->flag = pGolden->flag | GC5035_OTP_FLAG_INVALID;
            break;
        }
        default:
            break;
        }
    }
}
#endif

static kal_uint8 gc5035_otp_read_sensor_info(struct subdrv_ctx *ctx)
{
    kal_uint8 moduleID = 0;
#if GC5035_OTP_DEBUG
    kal_uint16 i = 0;
    kal_uint8 debug[GC5035_OTP_DATA_LENGTH];
#endif

    gc5035_gcore_read_dpc(ctx);
    gc5035_gcore_read_reg(ctx);
#if GC5035_OTP_FOR_CUSTOMER
    moduleID = gc5035_otp_read_module_info(ctx);
    gc5035_otp_read_wb_info(ctx);

#endif

#if GC5035_OTP_DEBUG
    memset(&debug[0], 0, GC5035_OTP_DATA_LENGTH);
    gc5035_otp_read_group(ctx,GC5035_OTP_START_ADDR, &debug[0], GC5035_OTP_DATA_LENGTH);
    for (i = 0; i < GC5035_OTP_DATA_LENGTH; i++)
        LOG_INF("addr = 0x%x, data = 0x%x\n", GC5035_OTP_START_ADDR + i * 8, debug[i]);
#endif

    return moduleID;
}

static void gc5035_otp_update_dd(struct subdrv_ctx *ctx)
{
    kal_uint8 state = 0;
    kal_uint8 n = 0;
    struct gc5035_dpc_t *pDPC = &gc5035_otp_data.dpc;

    if (GC5035_OTP_FLAG_VALID == pDPC->flag) {
        LOG_INF("DD auto load start!\n");
        write_cmos_sensor(0xfe, 0x02);
        write_cmos_sensor(0xbe, 0x00);
        write_cmos_sensor(0xa9, 0x01);
        write_cmos_sensor(0x09, 0x33);
        write_cmos_sensor(0x01, (pDPC->total_num >> 8) & 0x07);
        write_cmos_sensor(0x02, pDPC->total_num & 0xff);
        write_cmos_sensor(0x03, 0x00);
        write_cmos_sensor(0x04, 0x80);
        write_cmos_sensor(0x95, 0x0a);
        write_cmos_sensor(0x96, 0x30);
        write_cmos_sensor(0x97, 0x0a);
        write_cmos_sensor(0x98, 0x32);
        write_cmos_sensor(0x99, 0x07);
        write_cmos_sensor(0x9a, 0xa9);
        write_cmos_sensor(0xf3, 0x80);
        while (n < 3) {
            state = read_cmos_sensor_8(ctx,0x06);
            if ((state | 0xfe) == 0xff)
                mdelay(10);
            else
                n = 3;
            n++;
        }
        write_cmos_sensor(0xbe, 0x01);
        write_cmos_sensor(0x09, 0x00);
        write_cmos_sensor(0xfe, 0x01);
        write_cmos_sensor(0x80, 0x02);
        write_cmos_sensor(0xfe, 0x00);
    }
}

#if GC5035_OTP_FOR_CUSTOMER
static void gc5035_otp_update_wb(struct subdrv_ctx *ctx)
{
    kal_uint16 r_gain = GC5035_OTP_WB_GAIN_BASE;
    kal_uint16 g_gain = GC5035_OTP_WB_GAIN_BASE;
    kal_uint16 b_gain = GC5035_OTP_WB_GAIN_BASE;
    kal_uint16 base_gain = GC5035_OTP_WB_CAL_BASE;
    kal_uint16 r_gain_curr = GC5035_OTP_WB_CAL_BASE;
    kal_uint16 g_gain_curr = GC5035_OTP_WB_CAL_BASE;
    kal_uint16 b_gain_curr = GC5035_OTP_WB_CAL_BASE;
    kal_uint16 rg_typical = GC5035_OTP_WB_RG_TYPICAL;
    kal_uint16 bg_typical = GC5035_OTP_WB_BG_TYPICAL;
    struct gc5035_wb_t *pWB = &gc5035_otp_data.wb;
    struct gc5035_wb_t *pGolden = &gc5035_otp_data.golden;

    if (GC5035_OTP_CHECK_1BIT_FLAG(pGolden->flag, 0)) {
        rg_typical = pGolden->rg;
        bg_typical = pGolden->bg;
    } else {
        rg_typical = GC5035_OTP_WB_RG_TYPICAL;
        bg_typical = GC5035_OTP_WB_BG_TYPICAL;
    }
    LOG_INF("typical rg = 0x%x, bg = 0x%x\n", rg_typical, bg_typical);

    if (GC5035_OTP_CHECK_1BIT_FLAG(pWB->flag, 0)) {
        r_gain_curr = GC5035_OTP_WB_CAL_BASE * rg_typical / pWB->rg;
        b_gain_curr = GC5035_OTP_WB_CAL_BASE * bg_typical / pWB->bg;
        g_gain_curr = GC5035_OTP_WB_CAL_BASE;

        base_gain = (r_gain_curr < b_gain_curr) ? r_gain_curr : b_gain_curr;
        base_gain = (base_gain < g_gain_curr) ? base_gain : g_gain_curr;

        r_gain = GC5035_OTP_WB_GAIN_BASE * r_gain_curr / base_gain;
        g_gain = GC5035_OTP_WB_GAIN_BASE * g_gain_curr / base_gain;
        b_gain = GC5035_OTP_WB_GAIN_BASE * b_gain_curr / base_gain;
        LOG_INF("channel gain r = 0x%x, g = 0x%x, b = 0x%x\n", r_gain, g_gain, b_gain);

        write_cmos_sensor(0xfe, 0x04);
        write_cmos_sensor(0x40, g_gain & 0xff);
        write_cmos_sensor(0x41, r_gain & 0xff);
        write_cmos_sensor(0x42, b_gain & 0xff);
        write_cmos_sensor(0x43, g_gain & 0xff);
        write_cmos_sensor(0x44, g_gain & 0xff);
        write_cmos_sensor(0x45, r_gain & 0xff);
        write_cmos_sensor(0x46, b_gain & 0xff);
        write_cmos_sensor(0x47, g_gain & 0xff);
        write_cmos_sensor(0x48, (g_gain >> 8) & 0x07);
        write_cmos_sensor(0x49, (r_gain >> 8) & 0x07);
        write_cmos_sensor(0x4a, (b_gain >> 8) & 0x07);
        write_cmos_sensor(0x4b, (g_gain >> 8) & 0x07);
        write_cmos_sensor(0x4c, (g_gain >> 8) & 0x07);
        write_cmos_sensor(0x4d, (r_gain >> 8) & 0x07);
        write_cmos_sensor(0x4e, (b_gain >> 8) & 0x07);
        write_cmos_sensor(0x4f, (g_gain >> 8) & 0x07);
        write_cmos_sensor(0xfe, 0x00);
    }
}
#endif

static void gc5035_otp_update_reg(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0;

    LOG_INF("reg count = %d\n", gc5035_otp_data.regs.cnt);

    if (GC5035_OTP_CHECK_1BIT_FLAG(gc5035_otp_data.regs.flag, 0))
        for (i = 0; i < gc5035_otp_data.regs.cnt; i++) {
            write_cmos_sensor(0xfe, gc5035_otp_data.regs.reg[i].page);
            write_cmos_sensor(gc5035_otp_data.regs.reg[i].addr, gc5035_otp_data.regs.reg[i].value);
            LOG_INF("reg[%d] P%d:0x%x -> 0x%x\n", i, gc5035_otp_data.regs.reg[i].page,
                gc5035_otp_data.regs.reg[i].addr, gc5035_otp_data.regs.reg[i].value);
        }
}

static void gc5035_otp_update(struct subdrv_ctx *ctx)
{
    gc5035_otp_update_dd(ctx);
#if GC5035_OTP_FOR_CUSTOMER
    gc5035_otp_update_wb(ctx);
#endif
    gc5035_otp_update_reg(ctx);
}

static void gc5035_otp_function(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0, flag = 0;
    kal_uint8 otp_id[GC5035_OTP_ID_SIZE];

    memset(&otp_id, 0, GC5035_OTP_ID_SIZE);

    write_cmos_sensor(0xfa, 0x10);
    write_cmos_sensor(0xf5, 0xe9);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x67, 0xc0);
    write_cmos_sensor(0x59, 0x3f);
    write_cmos_sensor(0x55, 0x84);
    write_cmos_sensor(0x65, 0x80);
    write_cmos_sensor(0x66, 0x03);
    write_cmos_sensor(0xfe, 0x00);

    gc5035_otp_read_group(ctx,GC5035_OTP_ID_DATA_OFFSET, &otp_id[0], GC5035_OTP_ID_SIZE);
    for (i = 0; i < GC5035_OTP_ID_SIZE; i++)
        if (otp_id[i] != gc5035_otp_data.otp_id[i]) {
            flag = 1;
            break;
        }

    if (flag == 1) {
        LOG_INF("otp id mismatch, read again");
        memset(&gc5035_otp_data, 0, sizeof(gc5035_otp_data));
        for (i = 0; i < GC5035_OTP_ID_SIZE; i++)
            gc5035_otp_data.otp_id[i] = otp_id[i];
        gc5035_otp_read_sensor_info(ctx);
    }

    gc5035_otp_update(ctx);

    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x67, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfa, 0x00);
}

/*****************************************************************/


static void set_dummy(struct subdrv_ctx *ctx)
{
    kal_uint32 frame_length = ctx->frame_length >>2;
    LOG_INF("dummyline = %d, dummypixels = %d ctx->frame_length %d\n",
        ctx->dummy_line, ctx->dummy_pixel, ctx->frame_length);
    frame_length = frame_length << 2;
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x41, (frame_length >> 8) & 0x3f);
    write_cmos_sensor(0x42, frame_length & 0xff);

}   /*  set_dummy  */


static void set_max_framerate(struct subdrv_ctx *ctx, UINT16 framerate, kal_bool min_framelength_en)
{

    kal_uint32 frame_length = ctx->frame_length;
    //
    LOG_INF("framerate = %d, min framelength should enable = %d\n",
            framerate, min_framelength_en);

    frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
    if (frame_length >= ctx->min_frame_length)
        ctx->frame_length = frame_length;
    else
        ctx->frame_length = ctx->min_frame_length;
    ctx->dummy_line =
        ctx->frame_length - ctx->min_frame_length;

    if (ctx->frame_length > imgsensor_info.max_frame_length) {
        ctx->frame_length = imgsensor_info.max_frame_length;
        ctx->dummy_line =
            ctx->frame_length - ctx->min_frame_length;
    }
    if (min_framelength_en)
        ctx->min_frame_length = ctx->frame_length;
    set_dummy(ctx);
}   /*  set_max_framerate  */


static void write_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
    //unsigned long flags;
    kal_uint16 realtime_fps = 0, cal_shutter = 0;

    //spin_lock_irqsave(&imgsensor_drv_lock, flags);
    ctx->shutter = shutter;
    //spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    /*if shutter bigger than framelength, should extend frame length first*/
    //spin_lock(&imgsensor_drv_lock);
    if (shutter > ctx->min_frame_length - imgsensor_info.margin)
        ctx->frame_length = shutter + imgsensor_info.margin;
    else
        ctx->frame_length =ctx->min_frame_length;

    if (ctx->frame_length > imgsensor_info.max_frame_length)
        ctx->frame_length = imgsensor_info.max_frame_length;
    //spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ?
        imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length -
        imgsensor_info.margin)) ?
        (imgsensor_info.max_frame_length -
         imgsensor_info.margin) : shutter;

    realtime_fps = ctx->pclk / ctx->line_length * 10 /
        ctx->frame_length;

    if (ctx->autoflicker_en) {
        if (realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(ctx,296, 0);
        else if (realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(ctx,146, 0);
        else
            set_max_framerate(ctx,realtime_fps, 0);
    } else
        set_max_framerate(ctx,realtime_fps, 0);

    cal_shutter = shutter >> 2;
    cal_shutter = cal_shutter << 2;
    Dgain_ratio = 256 * shutter / cal_shutter;

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x03, (cal_shutter >> 8) & 0x3F);
    write_cmos_sensor(0x04, cal_shutter & 0xFF);

    LOG_INF("Exit! shutter = %d, framelength = %d\n",
            shutter, ctx->frame_length);
    LOG_INF("Exit! cal_shutter = %d, ", cal_shutter);

}   /*  write_shutter  */



/*************************************************************************
 * FUNCTION
 * set_shutter
 *
 * DESCRIPTION
 * This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 * iShutter : exposured lines
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(struct subdrv_ctx *ctx, kal_uint16 shutter)
{
    ctx->shutter = shutter;

    write_shutter(ctx, shutter);
} /* set_shutter */

static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint32 gain)
{
    kal_uint16 reg_gain = gain << 2;

    if (reg_gain < GC5035_SENSOR_GAIN_BASE)
        reg_gain = GC5035_SENSOR_GAIN_BASE;
    else if (reg_gain > GC5035_SENSOR_GAIN_MAX)
        reg_gain = GC5035_SENSOR_GAIN_MAX;


    return (kal_uint16) reg_gain;
}

/*************************************************************************
 * FUNCTION
 * set_gain
 *
 * DESCRIPTION
 * This function is to set global gain to sensor.
 *
 * PARAMETERS
 * iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 * the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
__attribute__((unused)) static kal_uint32 set_gain(struct subdrv_ctx *ctx, kal_uint32 gain)
{
    kal_uint16 reg_gain;
    kal_uint32 temp_gain;
    kal_int16 gain_index;
    kal_uint16 GC5035_AGC_Param[GC5035_SENSOR_GAIN_MAP_SIZE][2] = {
        {  256,  0 },
        {  302,  1 },
        {  358,  2 },
        {  425,  3 },
        {  502,  8 },
        {  599,  9 },
        {  717, 10 },
        {  845, 11 },
        {  998, 12 },
        { 1203, 13 },
        { 1434, 14 },
        { 1710, 15 },
        { 1997, 16 },
        { 2355, 17 },
        { 2816, 18 },
        { 3318, 19 },
        { 3994, 20 },
    };

    reg_gain = gain2reg(ctx,gain);

    for (gain_index = GC5035_SENSOR_GAIN_MAX_VALID_INDEX - 1;
            gain_index > 0; gain_index--)
        if (reg_gain >= GC5035_AGC_Param[gain_index][0])
            break;

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xb6, GC5035_AGC_Param[gain_index][1]);
    temp_gain = reg_gain * Dgain_ratio / GC5035_AGC_Param[gain_index][0];
    write_cmos_sensor(0xb1, (temp_gain >> 8) & 0x0f);
    write_cmos_sensor(0xb2, temp_gain & 0xfc);
    LOG_INF("Exit! GC5035_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, reg_gain = %d\n",
        GC5035_AGC_Param[gain_index][1], temp_gain, reg_gain);

    return gain;
}   /*  set_gain  */




/*************************************************************************
 * FUNCTION
 * night_mode
 *
 * DESCRIPTION
 * This function night mode of sensor.
 *
 * PARAMETERS
 * bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
/*No Need to implement this function*/
} /* night_mode */

static void sensor_init(struct subdrv_ctx *ctx)
{
    LOG_INF("E\n");
    //init setting
    /* SYSTEM */
    write_cmos_sensor(0xfc, 0x01);
    write_cmos_sensor(0xf4, 0x40);
    write_cmos_sensor(0xf5, 0xe9);
    write_cmos_sensor(0xf6, 0x14);
    write_cmos_sensor(0xf8, 0x49);
    write_cmos_sensor(0xf9, 0x82);
    write_cmos_sensor(0xfa, 0x00);
    write_cmos_sensor(0xfc, 0x81);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0xd3, 0x87);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x33, 0x00);
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x01, 0xe7);
    write_cmos_sensor(0xf7, 0x01);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xee, 0x30);
    write_cmos_sensor(0x87, 0x18);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x90);
    write_cmos_sensor(0xfe, 0x00);

    /* Analog & CISCTL */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x05, 0x02);
    write_cmos_sensor(0x06, 0xda);
    write_cmos_sensor(0x9d, 0x0c);
    write_cmos_sensor(0x09, 0x00);
    write_cmos_sensor(0x0a, 0x04);
    write_cmos_sensor(0x0b, 0x00);
    write_cmos_sensor(0x0c, 0x03);
    write_cmos_sensor(0x0d, 0x07);
    write_cmos_sensor(0x0e, 0xa8);
    write_cmos_sensor(0x0f, 0x0a);
    write_cmos_sensor(0x10, 0x30);
    write_cmos_sensor(0x11, 0x02);
    write_cmos_sensor(0x17, GC5035_MIRROR);
    write_cmos_sensor(0x19, 0x05);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x30, 0x03);
    write_cmos_sensor(0x31, 0x03);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xd9, 0xc0);
    write_cmos_sensor(0x1b, 0x20);
    write_cmos_sensor(0x21, 0x48);
    write_cmos_sensor(0x28, 0x22);
    write_cmos_sensor(0x29, 0x58);
    write_cmos_sensor(0x44, 0x20);
    write_cmos_sensor(0x4b, 0x10);
    write_cmos_sensor(0x4e, 0x1a);
    write_cmos_sensor(0x50, 0x11);
    write_cmos_sensor(0x52, 0x33);
    write_cmos_sensor(0x53, 0x44);
    write_cmos_sensor(0x55, 0x10);
    write_cmos_sensor(0x5b, 0x11);
    write_cmos_sensor(0xc5, 0x02);
    write_cmos_sensor(0x8c, 0x1a);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x33, 0x05);
    write_cmos_sensor(0x32, 0x38);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x91, 0x80);
    write_cmos_sensor(0x92, 0x28);
    write_cmos_sensor(0x93, 0x20);
    write_cmos_sensor(0x95, 0xa0);
    write_cmos_sensor(0x96, 0xe0);
    write_cmos_sensor(0xd5, 0xfc);
    write_cmos_sensor(0x97, 0x28);
    write_cmos_sensor(0x16, 0x0c);
    write_cmos_sensor(0x1a, 0x1a);
    write_cmos_sensor(0x1f, 0x11);
    write_cmos_sensor(0x20, 0x10);
    write_cmos_sensor(0x46, 0xe3);
    write_cmos_sensor(0x4a, 0x04);
    write_cmos_sensor(0x54, GC5035_RSTDUMMY1);
    write_cmos_sensor(0x62, 0x00);
    write_cmos_sensor(0x72, 0xcf);
    write_cmos_sensor(0x73, 0xc9);
    write_cmos_sensor(0x7a, 0x05);
    write_cmos_sensor(0x7d, 0xcc);
    write_cmos_sensor(0x90, 0x00);
    write_cmos_sensor(0xce, 0x98);
    write_cmos_sensor(0xd0, 0xb2);
    write_cmos_sensor(0xd2, 0x40);
    write_cmos_sensor(0xe6, 0xe0);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x12, 0x01);
    write_cmos_sensor(0x13, 0x01);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x15, 0x02);
    write_cmos_sensor(0x22, GC5035_RSTDUMMY2);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x00);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);

    /* Gain */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xb0, 0x6e);
    write_cmos_sensor(0xb1, 0x01);
    write_cmos_sensor(0xb2, 0x00);
    write_cmos_sensor(0xb3, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb6, 0x00);

    /* ISP */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x53, 0x00);
    write_cmos_sensor(0x89, 0x03);
    write_cmos_sensor(0x60, 0x40);

    /* BLK */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x42, 0x21);
    write_cmos_sensor(0x49, 0x03);
    write_cmos_sensor(0x4a, 0xff);
    write_cmos_sensor(0x4b, 0xc0);
    write_cmos_sensor(0x55, 0x00);

    /* Anti_blooming */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x41, 0x28);
    write_cmos_sensor(0x4c, 0x00);
    write_cmos_sensor(0x4d, 0x00);
    write_cmos_sensor(0x4e, 0x3c);
    write_cmos_sensor(0x44, 0x08);
    write_cmos_sensor(0x48, 0x01);

    /* Crop */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x08);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x07);
    write_cmos_sensor(0x95, 0x07);
    write_cmos_sensor(0x96, 0x98);
    write_cmos_sensor(0x97, 0x0a);
    write_cmos_sensor(0x98, 0x20);
    write_cmos_sensor(0x99, 0x00);

    /* MIPI */
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x02, 0x57);
    write_cmos_sensor(0x03, 0xb7);
    write_cmos_sensor(0x15, 0x14);
    write_cmos_sensor(0x18, 0x0f);
    write_cmos_sensor(0x21, 0x22);
    write_cmos_sensor(0x22, 0x06);
    write_cmos_sensor(0x23, 0x48);
    write_cmos_sensor(0x24, 0x12);
    write_cmos_sensor(0x25, 0x28);
    write_cmos_sensor(0x26, 0x08);
    write_cmos_sensor(0x29, 0x06);
    write_cmos_sensor(0x2a, 0x58);
    write_cmos_sensor(0x2b, 0x08);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x10);

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x01);
} /* sensor_init */


static void preview_setting(struct subdrv_ctx *ctx)
{
    //Preview 2592*1944 30fps 24M MCLK 4lane 608Mbps/lane
    // preview 30.01fps
    /* System */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x01);
    write_cmos_sensor(0xfc, 0x01);
    write_cmos_sensor(0xf4, 0x40);
    write_cmos_sensor(0xf5, 0xe4);
    write_cmos_sensor(0xf6, 0x14);
    write_cmos_sensor(0xf8, 0x49);
    write_cmos_sensor(0xf9, 0x12);
    write_cmos_sensor(0xfa, 0x01);
    write_cmos_sensor(0xfc, 0x81);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0xd3, 0x87);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x33, 0x20);
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x01, 0x87);
    write_cmos_sensor(0xf7, 0x11);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xee, 0x30);
    write_cmos_sensor(0x87, 0x18);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x90);
    write_cmos_sensor(0xfe, 0x00);

    /* Analog & CISCTL */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x05, 0x02);
    write_cmos_sensor(0x06, 0xda);
    write_cmos_sensor(0x9d, 0x0c);
    write_cmos_sensor(0x09, 0x00);
    write_cmos_sensor(0x0a, 0x04);
    write_cmos_sensor(0x0b, 0x00);
    write_cmos_sensor(0x0c, 0x03);
    write_cmos_sensor(0x0d, 0x07);
    write_cmos_sensor(0x0e, 0xa8);
    write_cmos_sensor(0x0f, 0x0a);
    write_cmos_sensor(0x10, 0x30);
    write_cmos_sensor(0x21, 0x60);
    write_cmos_sensor(0x29, 0x30);
    write_cmos_sensor(0x44, 0x18);
    write_cmos_sensor(0x4e, 0x20);
    write_cmos_sensor(0x8c, 0x20);
    write_cmos_sensor(0x91, 0x15);
    write_cmos_sensor(0x92, 0x3a);
    write_cmos_sensor(0x93, 0x20);
    write_cmos_sensor(0x95, 0x45);
    write_cmos_sensor(0x96, 0x35);
    write_cmos_sensor(0xd5, 0xf0);
    write_cmos_sensor(0x97, 0x20);
    write_cmos_sensor(0x1f, 0x19);
    write_cmos_sensor(0xce, 0x9b);
    write_cmos_sensor(0xd0, 0xb3);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x14, 0x02);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);

    /* BLK */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x49, 0x00);
    write_cmos_sensor(0x4a, 0x01);
    write_cmos_sensor(0x4b, 0xf8);

    /* Anti_blooming */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x4e, 0x06);
    write_cmos_sensor(0x44, 0x02);

    /* Crop */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x04);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x03);
    write_cmos_sensor(0x95, 0x03);
    write_cmos_sensor(0x96, 0xcc);
    write_cmos_sensor(0x97, 0x05);
    write_cmos_sensor(0x98, 0x10);
    write_cmos_sensor(0x99, 0x00);

    /* MIPI */
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x02, 0x58);
    write_cmos_sensor(0x22, 0x03);
    write_cmos_sensor(0x26, 0x06);
    write_cmos_sensor(0x29, 0x03);
    write_cmos_sensor(0x2b, 0x06);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x10);

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x91);
}   /*  preview_setting  */


static void capture_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
    LOG_INF("E! currefps:%d\n", currefps);
    // full size 29.76fps
    // capture setting 4208*3120  480MCLK 1.2Gp/lane
    /* System */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x01);
    write_cmos_sensor(0xfc, 0x01);
    write_cmos_sensor(0xf4, 0x40);
    if (currefps == 240) { /* PIP */
        write_cmos_sensor(0xf5, 0xe7);
        write_cmos_sensor(0xf6, 0x14);
        write_cmos_sensor(0xf8, 0x3b);
    } else {
        write_cmos_sensor(0xf5, 0xe9);
        write_cmos_sensor(0xf6, 0x14);
        write_cmos_sensor(0xf8, 0x49);
    }
    write_cmos_sensor(0xf9, 0x82);
    write_cmos_sensor(0xfa, 0x00);
    write_cmos_sensor(0xfc, 0x81);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0xd3, 0x87);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x33, 0x00);
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x01, 0xe7);
    write_cmos_sensor(0xf7, 0x01);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xee, 0x30);
    write_cmos_sensor(0x87, 0x18);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x90);
    write_cmos_sensor(0xfe, 0x00);

    /* Analog & CISCTL */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x05, 0x02);
    write_cmos_sensor(0x06, 0xda);
    write_cmos_sensor(0x9d, 0x0c);
    write_cmos_sensor(0x09, 0x00);
    write_cmos_sensor(0x0a, 0x04);
    write_cmos_sensor(0x0b, 0x00);
    write_cmos_sensor(0x0c, 0x03);
    write_cmos_sensor(0x0d, 0x07);
    write_cmos_sensor(0x0e, 0xa8);
    write_cmos_sensor(0x0f, 0x0a);
    write_cmos_sensor(0x10, 0x30);
    write_cmos_sensor(0x21, 0x48);
    write_cmos_sensor(0x29, 0x58);
    write_cmos_sensor(0x44, 0x20);
    write_cmos_sensor(0x4e, 0x1a);
    write_cmos_sensor(0x8c, 0x1a);
    write_cmos_sensor(0x91, 0x80);
    write_cmos_sensor(0x92, 0x28);
    write_cmos_sensor(0x93, 0x20);
    write_cmos_sensor(0x95, 0xa0);
    write_cmos_sensor(0x96, 0xe0);
    write_cmos_sensor(0xd5, 0xfc);
    write_cmos_sensor(0x97, 0x28);
    write_cmos_sensor(0x1f, 0x11);
    write_cmos_sensor(0xce, 0x98);
    write_cmos_sensor(0xd0, 0xb2);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x15, 0x02);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);

    /* BLK */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x49, 0x03);
    write_cmos_sensor(0x4a, 0xff);
    write_cmos_sensor(0x4b, 0xc0);

    /* Anti_blooming */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x4e, 0x3c);
    write_cmos_sensor(0x44, 0x08);

    /* Crop */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x08);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x07);
    write_cmos_sensor(0x95, 0x07);
    write_cmos_sensor(0x96, 0x98);
    write_cmos_sensor(0x97, 0x0a);
    write_cmos_sensor(0x98, 0x20);
    write_cmos_sensor(0x99, 0x00);

    /* MIPI */
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x02, 0x57);
    write_cmos_sensor(0x22, 0x06);
    write_cmos_sensor(0x26, 0x08);
    write_cmos_sensor(0x29, 0x06);
    write_cmos_sensor(0x2b, 0x08);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x10);

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x91);
}

static void normal_video_setting(struct subdrv_ctx *ctx, kal_uint16 currefps)
{
     LOG_INF("E! currefps:%d\n", currefps);
    /* System */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x01);
    write_cmos_sensor(0xfc, 0x01);
    write_cmos_sensor(0xf4, 0x40);
    if (currefps == 240) { /* PIP */
        write_cmos_sensor(0xf5, 0xe7);
        write_cmos_sensor(0xf6, 0x14);
        write_cmos_sensor(0xf8, 0x3b);
    } else {
        write_cmos_sensor(0xf5, 0xe9);
        write_cmos_sensor(0xf6, 0x14);
        write_cmos_sensor(0xf8, 0x49);
    }
    write_cmos_sensor(0xf9, 0x82);
    write_cmos_sensor(0xfa, 0x00);
    write_cmos_sensor(0xfc, 0x81);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0xd3, 0x87);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x33, 0x00);
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x01, 0xe7);
    write_cmos_sensor(0xf7, 0x01);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xee, 0x30);
    write_cmos_sensor(0x87, 0x18);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x90);
    write_cmos_sensor(0xfe, 0x00);

    /* Analog & CISCTL */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x05, 0x02);
    write_cmos_sensor(0x06, 0xda);
    write_cmos_sensor(0x9d, 0x0c);
    write_cmos_sensor(0x09, 0x00);
    write_cmos_sensor(0x0a, 0x04);
    write_cmos_sensor(0x0b, 0x00);
    write_cmos_sensor(0x0c, 0x03);
    write_cmos_sensor(0x0d, 0x07);
    write_cmos_sensor(0x0e, 0xa8);
    write_cmos_sensor(0x0f, 0x0a);
    write_cmos_sensor(0x10, 0x30);
    write_cmos_sensor(0x21, 0x48);
    write_cmos_sensor(0x29, 0x58);
    write_cmos_sensor(0x44, 0x20);
    write_cmos_sensor(0x4e, 0x1a);
    write_cmos_sensor(0x8c, 0x1a);
    write_cmos_sensor(0x91, 0x80);
    write_cmos_sensor(0x92, 0x28);
    write_cmos_sensor(0x93, 0x20);
    write_cmos_sensor(0x95, 0xa0);
    write_cmos_sensor(0x96, 0xe0);
    write_cmos_sensor(0xd5, 0xfc);
    write_cmos_sensor(0x97, 0x28);
    write_cmos_sensor(0x1f, 0x11);
    write_cmos_sensor(0xce, 0x98);
    write_cmos_sensor(0xd0, 0xb2);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x15, 0x02);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);

    /* BLK */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x49, 0x03);
    write_cmos_sensor(0x4a, 0xff);
    write_cmos_sensor(0x4b, 0xc0);

    /* Anti_blooming */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x4e, 0x3c);
    write_cmos_sensor(0x44, 0x08);

    /* Crop */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x08);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x07);
    write_cmos_sensor(0x95, 0x07);
    write_cmos_sensor(0x96, 0x98);
    write_cmos_sensor(0x97, 0x0a);
    write_cmos_sensor(0x98, 0x20);
    write_cmos_sensor(0x99, 0x00);

    /* MIPI */
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x02, 0x57);
    write_cmos_sensor(0x22, 0x06);
    write_cmos_sensor(0x26, 0x08);
    write_cmos_sensor(0x29, 0x06);
    write_cmos_sensor(0x2b, 0x08);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x10);

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x91);

}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
    LOG_INF("E\n");
    /* System */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x01);
    write_cmos_sensor(0xfc, 0x01);
    write_cmos_sensor(0xf4, 0x40);
    write_cmos_sensor(0xf5, 0xe9);
    write_cmos_sensor(0xf6, 0x14);
    write_cmos_sensor(0xf8, 0x49);
    write_cmos_sensor(0xf9, 0x82);
    write_cmos_sensor(0xfa, 0x00);
    write_cmos_sensor(0xfc, 0x81);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0xd3, 0x87);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x33, 0x20);
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x01, 0x87);
    write_cmos_sensor(0xf7, 0x11);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xee, 0x30);
    write_cmos_sensor(0x87, 0x18);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x90);
    write_cmos_sensor(0xfe, 0x00);

    /* Analog & CISCTL */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x05, 0x03);
    write_cmos_sensor(0x06, 0xb4);
    write_cmos_sensor(0x9d, 0x20);
    write_cmos_sensor(0x09, 0x00);
    write_cmos_sensor(0x0a, 0xf4);
    write_cmos_sensor(0x0b, 0x00);
    write_cmos_sensor(0x0c, 0x03);
    write_cmos_sensor(0x0d, 0x05);
    write_cmos_sensor(0x0e, 0xc8);
    write_cmos_sensor(0x0f, 0x0a);
    write_cmos_sensor(0x10, 0x30);
    write_cmos_sensor(0xd9, 0xf8);
    write_cmos_sensor(0x21, 0xe0);
    write_cmos_sensor(0x29, 0x40);
    write_cmos_sensor(0x44, 0x30);
    write_cmos_sensor(0x4e, 0x20);
    write_cmos_sensor(0x8c, 0x20);
    write_cmos_sensor(0x91, 0x15);
    write_cmos_sensor(0x92, 0x3a);
    write_cmos_sensor(0x93, 0x20);
    write_cmos_sensor(0x95, 0x45);
    write_cmos_sensor(0x96, 0x35);
    write_cmos_sensor(0xd5, 0xf0);
    write_cmos_sensor(0x97, 0x20);
    write_cmos_sensor(0x1f, 0x19);
    write_cmos_sensor(0xce, 0x9b);
    write_cmos_sensor(0xd0, 0xb3);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x14, 0x02);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0xf0);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);

    /* BLK */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x49, 0x00);
    write_cmos_sensor(0x4a, 0x01);
    write_cmos_sensor(0x4b, 0xf8);

    /* Anti_blooming */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x4e, 0x06);
    write_cmos_sensor(0x44, 0x02);

    /* Crop */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x0a);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x0b);
    write_cmos_sensor(0x95, 0x02);
    write_cmos_sensor(0x96, 0xd0);
    write_cmos_sensor(0x97, 0x05);
    write_cmos_sensor(0x98, 0x00);
    write_cmos_sensor(0x99, 0x00);

    /* MIPI */
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x02, 0x58);
    write_cmos_sensor(0x22, 0x03);
    write_cmos_sensor(0x26, 0x06);
    write_cmos_sensor(0x29, 0x03);
    write_cmos_sensor(0x2b, 0x06);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x10);

    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x91);
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
    LOG_INF("E\n");
/* System */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x01);
    write_cmos_sensor(0xfc, 0x01);
    write_cmos_sensor(0xf4, 0x40);
    write_cmos_sensor(0xf5, 0xe4);
    write_cmos_sensor(0xf6, 0x14);
    write_cmos_sensor(0xf8, 0x49);
    write_cmos_sensor(0xf9, 0x12);
    write_cmos_sensor(0xfa, 0x01);
    write_cmos_sensor(0xfc, 0x81);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x36, 0x01);
    write_cmos_sensor(0xd3, 0x87);
    write_cmos_sensor(0x36, 0x00);
    write_cmos_sensor(0x33, 0x20);
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x01, 0x87);
    write_cmos_sensor(0xf7, 0x11);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8f);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xee, 0x30);
    write_cmos_sensor(0x87, 0x18);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x90);
    write_cmos_sensor(0xfe, 0x00);

    /* Analog & CISCTL */
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x05, 0x02);
    write_cmos_sensor(0x06, 0xda);
    write_cmos_sensor(0x9d, 0x0c);
    write_cmos_sensor(0x09, 0x00);
    write_cmos_sensor(0x0a, 0x04);
    write_cmos_sensor(0x0b, 0x00);
    write_cmos_sensor(0x0c, 0x03);
    write_cmos_sensor(0x0d, 0x07);
    write_cmos_sensor(0x0e, 0xa8);
    write_cmos_sensor(0x0f, 0x0a);
    write_cmos_sensor(0x10, 0x30);
    write_cmos_sensor(0x21, 0x60);
    write_cmos_sensor(0x29, 0x30);
    write_cmos_sensor(0x44, 0x18);
    write_cmos_sensor(0x4e, 0x20);
    write_cmos_sensor(0x8c, 0x20);
    write_cmos_sensor(0x91, 0x15);
    write_cmos_sensor(0x92, 0x3a);
    write_cmos_sensor(0x93, 0x20);
    write_cmos_sensor(0x95, 0x45);
    write_cmos_sensor(0x96, 0x35);
    write_cmos_sensor(0xd5, 0xf0);
    write_cmos_sensor(0x97, 0x20);
    write_cmos_sensor(0x1f, 0x19);
    write_cmos_sensor(0xce, 0x9b);
    write_cmos_sensor(0xd0, 0xb3);
    write_cmos_sensor(0xfe, 0x02);
    write_cmos_sensor(0x14, 0x02);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x88);
    write_cmos_sensor(0xfe, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0xfc, 0x8e);

    /* BLK */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x49, 0x00);
    write_cmos_sensor(0x4a, 0x01);
    write_cmos_sensor(0x4b, 0xf8);

    /* Anti_blooming */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x4e, 0x06);
    write_cmos_sensor(0x44, 0x02);

    /* Crop */
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x91, 0x00);
    write_cmos_sensor(0x92, 0x0a);
    write_cmos_sensor(0x93, 0x00);
    write_cmos_sensor(0x94, 0x0b);
    write_cmos_sensor(0x95, 0x02);
    write_cmos_sensor(0x96, 0xd0);
    write_cmos_sensor(0x97, 0x05);
    write_cmos_sensor(0x98, 0x00);
    write_cmos_sensor(0x99, 0x00);

    /* MIPI */
    write_cmos_sensor(0xfe, 0x03);
    write_cmos_sensor(0x02, 0x58);
    write_cmos_sensor(0x22, 0x03);
    write_cmos_sensor(0x26, 0x06);
    write_cmos_sensor(0x29, 0x03);
    write_cmos_sensor(0x2b, 0x06);
    write_cmos_sensor(0xfe, 0x01);
    write_cmos_sensor(0x8c, 0x10);
    write_cmos_sensor(0xfe, 0x00);
    write_cmos_sensor(0x3e, 0x91);
}

/*************************************************************************
 * FUNCTION
 * get_imgsensor_id
 *
 * DESCRIPTION
 * This function get the sensor ID
 *
 * PARAMETERS
 * *sensorID : return the sensor ID
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
    //we should detect the module used i2c address

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
        do {
            *sensor_id = ((read_cmos_sensor_8(ctx, 0xf0) << 8) |
                read_cmos_sensor_8(ctx, 0xf1));
            if (*sensor_id == imgsensor_info.sensor_id) {
                pr_debug("imgsensor GC5035 i2c write id: 0x%x, sensor id: 0x%x\n",
                    ctx->i2c_write_id, *sensor_id);
                return ERROR_NONE;
            }
            pr_debug("imgsensor GC5035 Read sensor id fail, write id:0x%x id: 0x%x\n",
                ctx->i2c_write_id, *sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        // if Sensor ID is not correct,
        // Must set *sensor_id to 0xFFFFFFFF
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }

    return ERROR_NONE;
}

/*************************************************************************
 * FUNCTION
 * open
 *
 * DESCRIPTION
 * This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 * None
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int open(struct subdrv_ctx *ctx)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint16 sensor_id = 0;

    LOG_INF("PLATFORM:MT6595,MIPI 2LANE\n");
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
    //we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
        do {
            sensor_id = ((read_cmos_sensor_8(ctx, 0x00f0) << 8) |
                read_cmos_sensor_8(ctx, 0x00f1));
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("imx214 i2c write id: 0x%x, sensor id: 0x%x\n",
                    ctx->i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("imx214 Read sensor id fail, write id:0x%x id: 0x%x\n",
                ctx->i2c_write_id, sensor_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init(ctx);

    gc5035_otp_function(ctx);


    ctx->autoflicker_en = KAL_FALSE;
    ctx->sensor_mode = IMGSENSOR_MODE_INIT;
    ctx->shutter = 0x3D0;
    ctx->gain = 0x100;
    ctx->pclk = imgsensor_info.pre.pclk;
    ctx->frame_length = imgsensor_info.pre.framelength;
    ctx->line_length = imgsensor_info.pre.linelength;
    ctx->min_frame_length = imgsensor_info.pre.framelength;
    ctx->dummy_pixel = 0;
    ctx->dummy_line = 0;
    ctx->ihdr_mode = 0;
    ctx->test_pattern = KAL_FALSE;
    ctx->current_fps = imgsensor_info.pre.max_framerate;

    return ERROR_NONE;
} /* open  */



/*************************************************************************
 * FUNCTION
 * close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 * None
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int close(struct subdrv_ctx *ctx)
{
    LOG_INF("E\n");

    /*No Need to implement this function*/

    return ERROR_NONE;
} /* close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 * This function start the sensor preview.
 *
 * PARAMETERS
 * *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
    ctx->pclk = imgsensor_info.pre.pclk;
    //ctx->video_mode = KAL_FALSE;
    ctx->line_length = imgsensor_info.pre.linelength;
    ctx->frame_length = imgsensor_info.pre.framelength;
    ctx->min_frame_length = imgsensor_info.pre.framelength;
    ctx->autoflicker_en = KAL_FALSE;

    preview_setting(ctx);
    //hs_video_setting(ctx);
    //set_mirror_flip(ctx, IMAGE_HV_MIRROR);
    return ERROR_NONE;
} /* preview */

/*************************************************************************
 * FUNCTION
 * capture
 *
 * DESCRIPTION
 * This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 * None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (ctx->current_fps == imgsensor_info.cap1.max_framerate) {
        //PIP capture: 24fps for less than 13M,
        //20fps for 16M,15fps for 20M
        LOG_INF("enter Cap1");
        ctx->pclk = imgsensor_info.cap1.pclk;
        ctx->line_length = imgsensor_info.cap1.linelength;
        ctx->frame_length = imgsensor_info.cap1.framelength;
        ctx->min_frame_length = imgsensor_info.cap1.framelength;
        ctx->autoflicker_en = KAL_FALSE;
    }  else if (ctx->current_fps ==
            imgsensor_info.cap2.max_framerate) {
        LOG_INF("enter cap2");
        if (ctx->current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF(
            "Warning: current_fps %d fps is not support,so use cap1's setting: %d fps!\n",
                ctx->current_fps,
                imgsensor_info.cap1.max_framerate/10);
        ctx->pclk = imgsensor_info.cap2.pclk;
        ctx->line_length = imgsensor_info.cap2.linelength;
        ctx->frame_length = imgsensor_info.cap2.framelength;
        ctx->min_frame_length = imgsensor_info.cap2.framelength;
        ctx->autoflicker_en = KAL_FALSE;
    } else {
        if (ctx->current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF(
            "Warning: current_fps %d fps is not support,so use cap1's setting: %d fps!\n",
                ctx->current_fps,
                imgsensor_info.cap1.max_framerate/10);
        LOG_INF("enter cap");
        ctx->pclk = imgsensor_info.cap.pclk;
        ctx->line_length = imgsensor_info.cap.linelength;
        ctx->frame_length = imgsensor_info.cap.framelength;
        ctx->min_frame_length = imgsensor_info.cap.framelength;
        ctx->autoflicker_en = KAL_FALSE;
    }
    capture_setting(ctx, ctx->current_fps);
    return ERROR_NONE;
}   /* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
        MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
    ctx->pclk = imgsensor_info.normal_video.pclk;
    ctx->line_length = imgsensor_info.normal_video.linelength;
    ctx->frame_length = imgsensor_info.normal_video.framelength;
    ctx->min_frame_length = imgsensor_info.normal_video.framelength;
    //ctx->current_fps = 300;
    ctx->autoflicker_en = KAL_FALSE;
    normal_video_setting(ctx, ctx->current_fps);
    //set_mirror_flip(ctx, IMAGE_HV_MIRROR);
    return ERROR_NONE;
} /* normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    ctx->pclk = imgsensor_info.hs_video.pclk;
    //ctx->video_mode = KAL_TRUE;
    ctx->line_length = imgsensor_info.hs_video.linelength;
    ctx->frame_length = imgsensor_info.hs_video.framelength;
    ctx->min_frame_length = imgsensor_info.hs_video.framelength;
    ctx->dummy_line = 0;
    ctx->dummy_pixel = 0;
    //ctx->current_fps = 300;
    ctx->autoflicker_en = KAL_FALSE;
    hs_video_setting(ctx);
    //set_mirror_flip(ctx, IMAGE_HV_MIRROR);
    return ERROR_NONE;
} /* hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
            MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
            MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    ctx->pclk = imgsensor_info.slim_video.pclk;
    //ctx->video_mode = KAL_TRUE;
    ctx->line_length = imgsensor_info.slim_video.linelength;
    ctx->frame_length = imgsensor_info.slim_video.framelength;
    ctx->min_frame_length = imgsensor_info.slim_video.framelength;
    ctx->dummy_line = 0;
    ctx->dummy_pixel = 0;
    //ctx->current_fps = 300;
    ctx->autoflicker_en = KAL_FALSE;
    slim_video_setting(ctx);
    //set_mirror_flip(ctx, IMAGE_HV_MIRROR);
    return ERROR_NONE;
} /* slim_video  */



static int get_resolution(struct subdrv_ctx *ctx,
        MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    int i = 0;

    for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
        if (i < imgsensor_info.sensor_mode_num) {
            sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
            sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
        } else {
            sensor_resolution->SensorWidth[i] = 0;
            sensor_resolution->SensorHeight[i] = 0;
        }
    }

    return ERROR_NONE;
} /* get_resolution */

static int get_info(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
    MSDK_SENSOR_INFO_STRUCT *sensor_info,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
     /* not use */
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorHsyncPolarity =
        SENSOR_CLOCK_POLARITY_LOW;// inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    //sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat =
        imgsensor_info.sensor_output_dataformat;

    sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
        imgsensor_info.pre_delay_frame;
    sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
        imgsensor_info.cap_delay_frame;
    sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
        imgsensor_info.video_delay_frame;
    sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
        imgsensor_info.hs_video_delay_frame;
    sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
        imgsensor_info.slim_video_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    /* The frame of setting shutter default 0 for TG int */
    sensor_info->AEShutDelayFrame =
        imgsensor_info.ae_shut_delay_frame;
    /* The frame of setting sensor gain */
    sensor_info->AESensorGainDelayFrame =
        imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame =
        imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0; // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    return ERROR_NONE;
}   /*  get_info  */

static int control(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
    MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
    MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    ctx->current_scenario_id = scenario_id;
    switch (scenario_id) {
    case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        preview(ctx, image_window, sensor_config_data);
        break;
    case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        capture(ctx, image_window, sensor_config_data);
        break;
    case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        normal_video(ctx, image_window, sensor_config_data);
        break;
    case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        hs_video(ctx, image_window, sensor_config_data);
        break;
    case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        slim_video(ctx, image_window, sensor_config_data);
        break;
    default:
        LOG_INF("Error ScenarioId setting");
        preview(ctx, image_window, sensor_config_data);
        return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
} /* control(ctx) */



static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
        ctx->current_fps = 296;
    else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
        ctx->current_fps = 146;
    else
        ctx->current_fps = framerate;
    set_max_framerate(ctx, ctx->current_fps, 1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx, kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    if (enable) //enable auto flicker
        ctx->autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
        ctx->autoflicker_en = KAL_FALSE;
    return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n",
        scenario_id, framerate);

    switch (scenario_id) {
    case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        frame_length = imgsensor_info.pre.pclk /
            framerate * 10 / imgsensor_info.pre.linelength;
        ctx->dummy_line = (frame_length >
            imgsensor_info.pre.framelength) ?
            (frame_length - imgsensor_info.pre.framelength) : 0;
        ctx->frame_length =
            imgsensor_info.pre.framelength + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        //set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        if (framerate == 0)
            return ERROR_NONE;
        frame_length =
            imgsensor_info.normal_video.pclk /
            framerate * 10 / imgsensor_info.normal_video.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.normal_video.framelength)
            ? (frame_length -
            imgsensor_info.normal_video.framelength) : 0;
        ctx->frame_length =
            imgsensor_info.normal_video.framelength +
            ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        //set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        if (ctx->current_fps ==
                imgsensor_info.cap1.max_framerate) {
            frame_length = imgsensor_info.cap1.pclk /
                framerate * 10 / imgsensor_info.cap1.linelength;
                ctx->dummy_line =
                    (frame_length >
                     imgsensor_info.cap1.framelength)
                    ? (frame_length -
                    imgsensor_info.cap1.framelength)
                    : 0;
                ctx->frame_length =
                    imgsensor_info.cap1.framelength
                    + ctx->dummy_line;
                ctx->min_frame_length =
                    ctx->frame_length;
        } else {
            if (ctx->current_fps !=
                imgsensor_info.cap.max_framerate)
                LOG_INF(
            "Warning: current_fps %d fps is not support,so use cap's setting: %d fps!\n",
                    framerate,
                    imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk /
                    framerate * 10 /
                    imgsensor_info.cap.linelength;
                ctx->dummy_line =
                    (frame_length >
                     imgsensor_info.cap.framelength)
                    ? (frame_length -
                    imgsensor_info.cap.framelength)
                    : 0;
                ctx->frame_length =
                    imgsensor_info.cap.framelength
                    + ctx->dummy_line;
                ctx->min_frame_length =
                    ctx->frame_length;
        }
        //set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk /
            framerate * 10 / imgsensor_info.hs_video.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.hs_video.framelength)
            ? (frame_length - imgsensor_info.hs_video.framelength)
            : 0;
        ctx->frame_length =
            imgsensor_info.hs_video.framelength
            + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        //set_dummy(ctx);
        break;
    case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk /
            framerate * 10 / imgsensor_info.slim_video.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.slim_video.framelength)
            ? (frame_length -
            imgsensor_info.slim_video.framelength) : 0;
        ctx->frame_length =
            imgsensor_info.slim_video.framelength
            + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        //set_dummy(ctx);
        break;
    default:  //coding with  preview scenario by default
        frame_length = imgsensor_info.pre.pclk / framerate * 10
            / imgsensor_info.pre.linelength;
        ctx->dummy_line =
            (frame_length > imgsensor_info.pre.framelength)
            ? (frame_length - imgsensor_info.pre.framelength) : 0;
        ctx->frame_length =
            imgsensor_info.pre.framelength + ctx->dummy_line;
        ctx->min_frame_length = ctx->frame_length;
        //set_dummy(ctx);
        LOG_INF("error scenario_id = %d, we use preview scenario\n",
            scenario_id);
        break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
        enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

    switch (scenario_id) {
    case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        *framerate = imgsensor_info.pre.max_framerate;
        break;
    case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        *framerate = imgsensor_info.normal_video.max_framerate;
        break;
    case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        *framerate = imgsensor_info.cap.max_framerate;
        break;
    case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        *framerate = imgsensor_info.hs_video.max_framerate;
        break;
    case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        *framerate = imgsensor_info.slim_video.max_framerate;
        break;
    default:
        break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    write_cmos_sensor(0xfe, 0x01);
    if (enable)
        write_cmos_sensor(0x8c, 0x11);
    else
        write_cmos_sensor(0x8c, 0x10);
    write_cmos_sensor(0xfe, 0x00);

    ctx->test_pattern = enable;
    return ERROR_NONE;
}

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
    LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n",
        enable);
    if (enable) {
        /* MIPI */
        write_cmos_sensor(0xfe, 0x00);
        write_cmos_sensor(0x3e, 0x91); /*Stream on */
    } else {
        /* MIPI */
        write_cmos_sensor(0xfe, 0x00);
        write_cmos_sensor(0x3e, 0x01); /*Stream off */
    }

    mdelay(10);
    return ERROR_NONE;
}

static int feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
            UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *) feature_para;
    //unsigned long long *feature_return_para =
    //(unsigned long long *) feature_para;
    kal_uint32 rate;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    //SET_SENSOR_AWB_GAIN *pSetSensorAWB =
    //(SET_SENSOR_AWB_GAIN *)feature_para;
     MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
         (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    switch (feature_id) {
    case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
        if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
            *(feature_data + 0) =
                sizeof(gc5035_ana_gain_table);
        } else {
            memcpy((void *)(uintptr_t) (*(feature_data + 1)),
            (void *)gc5035_ana_gain_table,
            sizeof(gc5035_ana_gain_table));
        }
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_gain;
        *(feature_data + 2) = imgsensor_info.max_gain;
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        *(feature_data + 0) = imgsensor_info.min_gain_iso;
        *(feature_data + 1) = imgsensor_info.gain_step;
        *(feature_data + 2) = imgsensor_info.gain_type;
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_shutter;
        *(feature_data + 2) = imgsensor_info.exp_step;
        break;
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = ctx->line_length;
        *feature_return_para_16 = ctx->frame_length;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        *feature_return_para_32 = ctx->pclk;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
        set_shutter(ctx, *feature_data);
        break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
        night_mode(ctx, (BOOL) * feature_data);
        break;
    case SENSOR_FEATURE_SET_GAIN:
        set_gain(ctx, (UINT32) * feature_data);
        break;
    case SENSOR_FEATURE_SET_FLASHLIGHT:
        break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
        break;
    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor_8(ctx, sensor_reg_data->RegAddr,
            sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData =
            read_cmos_sensor_8(ctx, sensor_reg_data->RegAddr);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        // get the lens driver ID from EEPROM or just
        // return LENS_DRIVER_ID_DO_NOT_CARE
        // if EEPROM does not exist in camera module.
        *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
        set_video_mode(ctx, *feature_data);
        break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(ctx, feature_return_para_32);
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
        set_auto_flicker_mode(ctx, (BOOL)*feature_data_16,
            *(feature_data_16+1));
        break;
    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario(ctx,
            (enum MSDK_SCENARIO_ID_ENUM)*feature_data,
            *(feature_data+1));
        break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario(ctx,
            (enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
            (MUINT32 *)(uintptr_t)(*(feature_data+1)));
        break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode(ctx, (BOOL)*feature_data);
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        //for factory mode auto testing
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        LOG_INF("current fps :%d\n", (UINT32)*feature_data);
        ctx->current_fps = *feature_data;
        break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
            (UINT32)*feature_data);
        wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
            (uintptr_t)(*(feature_data+1));

        switch (*feature_data_32) {
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[1],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[2],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[3],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[4],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            memcpy((void *)wininfo,
                (void *)&imgsensor_winsize_info[0],
                sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
        }
        break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
        switch (*feature_data) {
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
            rate = imgsensor_info.cap.mipi_pixel_rate;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
            rate = imgsensor_info.normal_video.mipi_pixel_rate;
            break;
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
            rate = imgsensor_info.hs_video.mipi_pixel_rate;
            break;
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
            rate = imgsensor_info.slim_video.mipi_pixel_rate;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            rate = imgsensor_info.pre.mipi_pixel_rate;
            break;
        }
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
        break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
        streaming_control(ctx, KAL_FALSE);
        break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
            *feature_data);
        if (*feature_data != 0)
            set_shutter(ctx, *feature_data);
        streaming_control(ctx, KAL_TRUE);
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.cap.framelength << 16)
                + imgsensor_info.cap.linelength;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.normal_video.framelength << 16)
                + imgsensor_info.normal_video.linelength;
            break;
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.hs_video.framelength << 16)
                + imgsensor_info.hs_video.linelength;
            break;
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.slim_video.framelength << 16)
                + imgsensor_info.slim_video.linelength;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
            = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.cap.pclk;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.normal_video.pclk;
            break;
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.hs_video.pclk;
            break;
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.slim_video.pclk;
            break;
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.pclk;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
        case SENSOR_SCENARIO_ID_SLIM_VIDEO:
        case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
        case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        default:
            *feature_return_para_32 = 1; /*BINNING_AVERAGE*/
            break;
        }
        pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
        *feature_para_len = 4;

        break;
    default:
        break;
    }

    return ERROR_NONE;
} /* feature_control(ctx)  */

#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 1296,
            .vsize = 972,
        },
    }
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
    {
        .bus.csi2 = {
            .channel = 0,
            .data_type = 0x2b,
            .hsize = 2592,
            .vsize = 1944,
        },
    }
};

static int get_frame_desc(struct subdrv_ctx *ctx,
        int scenario_id, struct mtk_mbus_frame_desc *fd)
{
    LOG_INF("enter get frame %d",scenario_id);

    switch (scenario_id) {
    case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
        fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
        fd->num_entries = ARRAY_SIZE(frame_desc_prev);
        memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
        break;
    case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
    case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
        fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
        fd->num_entries = ARRAY_SIZE(frame_desc_cap);
        memcpy(fd->entry, frame_desc_cap, sizeof(frame_desc_cap));
        break;
    default:
        return -1;
    }

    return 0;
}
#endif

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
    kal_uint16 reg_gain;

    reg_gain = ctx->gain;

    LOG_INF("sof_cnt %d reg_gain = 0x%x, 0x4018 %x\n",
        sof_cnt, reg_gain, 0);

    return 0;
};

static const struct subdrv_ctx defctx = {

    .ana_gain_def = BASEGAIN * 4,
    .ana_gain_max = BASEGAIN * 16,
    .ana_gain_min = BASEGAIN,
    .ana_gain_step = 1,
    .exposure_def = 0x3D0,
    .exposure_max = 0xffff - 10,
    .exposure_min = 1,
    .exposure_step = 1,
    .max_frame_length = 0xffff,

    .mirror = IMAGE_NORMAL,//mirrorflip information
    //IMGSENSOR_MODE enum value,record current sensor mode,
    //such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .sensor_mode = IMGSENSOR_MODE_INIT,
    .shutter = 0x3D0,//current shutter
    .gain = BASEGAIN * 4,//current gain
    .dummy_pixel = 0,//current dummypixel
    .dummy_line = 0,  //current dummyline
    //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .current_fps = 300,
    //auto flicker enable: KAL_FALSE for disable auto flicker,
    //KAL_TRUE for enable auto flicker
    .autoflicker_en = KAL_FALSE,
    //test pattern mode or not. KAL_FALSE for in test pattern mode,
    //KAL_TRUE for normal output
    .test_pattern = KAL_FALSE,
    //current scenario id
    .current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
    .ihdr_mode = 0, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x34,
};

static int init_ctx(struct subdrv_ctx *ctx,
        struct i2c_client *i2c_client, u8 i2c_write_id)
{
    memcpy(ctx, &defctx, sizeof(*ctx));
    ctx->i2c_client = i2c_client;
    ctx->i2c_write_id = i2c_write_id;
    return 0;
}

static int get_csi_param(struct subdrv_ctx *ctx,
    enum SENSOR_SCENARIO_ID_ENUM scenario_id,
    struct mtk_csi_param *csi_param)
{
    csi_param->legacy_phy = 0;
    csi_param->not_fixed_trail_settle = 0;
    //csi_param->cphy_settle = 0x1b;
    csi_param->dphy_trail= 76;
    return 0;
}

static struct subdrv_ops ops = {
    .get_id = get_imgsensor_id,
    .init_ctx = init_ctx,
    .open = open,
    .get_info = get_info,
    .get_resolution = get_resolution,
    .control = control,
    .feature_control = feature_control,
    .close = close,
#ifdef IMGSENSOR_VC_ROUTING
    .get_frame_desc = get_frame_desc,
#endif
    .get_csi_param = get_csi_param,
    .vsync_notify = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
    {HW_ID_RST, 0, 10},
    {HW_ID_MCLK, 24, 0},
    {HW_ID_MCLK_DRIVING_CURRENT, 8, 1},
    {HW_ID_DOVDD, 1800000, 0},
    {HW_ID_AVDD, 2800000, 0},
    {HW_ID_DVDD, 1200000, 0},
    {HW_ID_RST, 1, 20},
};

const struct subdrv_entry gc5035_mipi_raw_entry = {
    .name = "gc5035_mipi_raw",
    .id = GC5035_SENSOR_ID,
    .pw_seq = pw_seq,
    .pw_seq_cnt = ARRAY_SIZE(pw_seq),
    .ops = &ops,
};

