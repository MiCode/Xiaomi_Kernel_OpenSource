// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "cam_cal_config.h"
#include "gc5035_mtk_eeprom.h"

#define MAX_TEMP(a,b,c)  (a)>(b)?((a)>(c)?(a):(c)):((b)>(c)?(b):(c))

#define write_cmos_sensor_8(i2c_client, reg, val) \
        adaptor_i2c_wr_u8_reg8(i2c_client, \
        i2c_client->addr, reg, val)

#define read_cmos_sensor_8(i2c_client, reg) \
 ({ \
        u8 __val = 0xff; \
        adaptor_i2c_rd_u8_reg8(i2c_client, \
            i2c_client->addr, reg, &__val); \
        __val; \
})



typedef struct {
   u8  addr;
   u8  data;
} i2c_byte_addr_byte_data_pair;


i2c_byte_addr_byte_data_pair  otp_init_setting_array[] = {
    {0xfa, 0x10},
    {0xf5, 0xe9},
    {0xfe, 0x02},
    {0x67, 0xc0},
    {0x59, 0x3f},
    {0x55, 0x84},
    {0x65, 0x80},
    {0x66, 0x03},
    {0xfe, 0x00},
};

static int adaptor_i2c_wr_u8_reg8(struct i2c_client *i2c_client,
        u16 addr, u16 reg, u8 val)
{
    int ret;
    u8 buf[2];
    struct i2c_msg msg;

    buf[0] = reg & 0xff;
    buf[1] = val;

    msg.addr = addr;
    msg.flags = i2c_client->flags;
    msg.buf = buf;
    msg.len = sizeof(buf);

    ret = i2c_transfer(i2c_client->adapter, &msg, 1);
    if (ret < 0)
        dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);

    return ret;
}

static int adaptor_i2c_rd_u8_reg8(struct i2c_client *i2c_client,
        u16 addr, u16 reg, u8 *val)
{
    int ret;
    u8 buf[1];
    struct i2c_msg msg[2];

    buf[0] = reg & 0xff;

    msg[0].addr = addr;
    msg[0].flags = i2c_client->flags;
    msg[0].buf = buf;
    msg[0].len = sizeof(buf);

    msg[1].addr = addr;
    msg[1].flags = i2c_client->flags | I2C_M_RD;
    msg[1].buf = buf;
    msg[1].len = 1;

    ret = i2c_transfer(i2c_client->adapter, msg, 2);
    if (ret < 0) {
        dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
        return ret;
    }

    *val = buf[0];

    return 0;
}


static int gc5035_otp_func_init(struct i2c_client *client)
{
    int ret = 0;
    int i;
/*
    write_cmos_sensor_8(client,0xfa,0x10);
    write_cmos_sensor_8(client,0xf5,0xe9);
    write_cmos_sensor_8(client,0xfe,0x02);
    write_cmos_sensor_8(client,0x67,0xc0);
    write_cmos_sensor_8(client,0x59,0x3f);
    write_cmos_sensor_8(client,0x55,0x84);
    write_cmos_sensor_8(client,0x65,0x80);
    write_cmos_sensor_8(client,0x66,0x03);
    write_cmos_sensor_8(client,0xfe,0x00);
*/

    must_log("%s,%d sizeof otp_func array: %d, E\n",__func__,__LINE__,ARRAY_SIZE(otp_init_setting_array));
    for(i = 0; i < ARRAY_SIZE(otp_init_setting_array); i++) {
        ret = write_cmos_sensor_8(client,otp_init_setting_array[i].addr, otp_init_setting_array[i].data);
        if(ret < 0){
            must_log("[%s,%d]write i2c fail,addr:0x%x, data:0x%x\n", __func__,__LINE__,
                                                            otp_init_setting_array[i].addr,
                                                            otp_init_setting_array[i].data);
            return ret;
        }
    }
    return ret;
}

unsigned int gc5035_read_region(struct i2c_client *client, unsigned int addr,
                unsigned char *data, unsigned int size)
{
	int ret = 0;
    unsigned int i;

    if(addr == 0x0) {
        addr = 0x1000;
    }

    ret = gc5035_otp_func_init(client);
    if (ret < 0) {
        must_log("[%s,%d] otp_func_init failed ret:%d\n", __func__,__LINE__,ret);
		return 0;
    }

    must_log("[%s,%d] addr:0x%x, size:%d\n", __func__,__LINE__,addr,size);
    write_cmos_sensor_8(client,0xfe,0x02);
    write_cmos_sensor_8(client,0x69,(addr >> 8) & 0x1f);
    write_cmos_sensor_8(client,0x6a,addr & 0xff);
    write_cmos_sensor_8(client,0xf3,0x20);
    write_cmos_sensor_8(client,0xf3,0x12);

    for(i = 0; i < size;i++) {
        data[i] = read_cmos_sensor_8(client,0x6c);
    }

    write_cmos_sensor_8(client,0xf3,0x00);

    return size;
}

unsigned char check_otp_sum(unsigned char *data, unsigned int length){
    char i = 0;
    int sum = 0;
    unsigned char checksum;

    for(i = 0; i < length;i++){
        sum += *(data + i);
    }
    checksum = sum % 255 + 1;
    return checksum;
}

unsigned int gc5035_do_2a_gain(struct EEPROM_DRV_FD_DATA *pdata,
         unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
    struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
                          (struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
    unsigned int err = CamCalReturnErr[pCamCalData->Command];

    unsigned short tempMax = 0;
    int read_data_size;

    struct STRUCT_GC5035_CAL_DATA_INFO CalData[GROUP_NUM];
    unsigned char AWBAFConfig = 0x3;

    unsigned short CalR = 1, CalGr = 1, CalGb = 1, CalG = 1, CalB = 1;
    unsigned short FacR = 1, FacGr = 1, FacGb = 1, FacG = 1, FacB = 1;

	unsigned int group_id = GROUP_NUM;
    int i;
#ifdef ENABLE_CHECK_SUM
    unsigned int checkSum;
	unsigned long long ptr_ofst = 0;
#endif

    must_log("[GC5035_EEPROM]start_addr:0x%x, block_size=%d sensor_id=%x\n", start_addr, block_size, pCamCalData->sensorID);
    memset((void *)&pCamCalData->Single2A, 0, sizeof(struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT));

    /* Check rule */
    if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
        err = CAM_CAL_ERR_NO_DEVICE;
        error_log("Read Failed\n");
        show_cmd_error_log(pCamCalData->Command);
        return err;
    }

    pCamCalData->Single2A.S2aVer = 0x01;
    pCamCalData->Single2A.S2aBitEn = 1;
    pCamCalData->Single2A.S2aAfBitflagEn = (0x0C & AWBAFConfig);

    /* AWB Unit Gain (5100K) */
    pCamCalData->Single2A.S2aAwb.rGainSetNum = 0;
    read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
                                start_addr,
                                GROUP_NUM * sizeof(struct STRUCT_GC5035_CAL_DATA_INFO),
                                (unsigned char *)&CalData[0]);
    if (read_data_size < 0) {
            pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
            error_log("Read CalGain Failed\n");
            show_cmd_error_log(pCamCalData->Command);
            return CamCalReturnErr[pCamCalData->Command];
    }

    for(i= 0; i < GROUP_NUM;i++) {
        if(CalData[i].flag == 0x40) {
            group_id = i;
            break;
        }
    }

    debug_log("[AWB_INFO]group_id : %d",group_id);
	if (group_id >= GROUP_NUM) {
		error_log("No Valid calibration data found\n");
       return CamCalReturnErr[pCamCalData->Command];
    }

#ifdef ENABLE_CHECK_SUM
        checkSum = CalData[group_id].checksum;
		ptr_ofst = (unsigned long long)(&CalData[i].module_info) -
			(unsigned long long)(&CalData[i].flag);

		if (checkSum !=
			check_otp_sum(((unsigned char *)&CalData[i] + ptr_ofst),
			(sizeof(struct STRUCT_GC5035_CAL_DATA_INFO)-2))) {
			must_log("[AWB_INFO]checkSum failed, checksum: %d, %d\n",
                      checkSum,
					check_otp_sum(((unsigned char *)&CalData[i] + ptr_ofst),
						(sizeof(struct STRUCT_GC5035_CAL_DATA_INFO)-2))
                    );
            return CamCalReturnErr[pCamCalData->Command];
        }
        must_log("[AWB_INFO]checkSum succeed\n");
#endif

    debug_log("======================MODULE_INFO==================\n");
    debug_log("[MODULE_INFO]flag: 0x%x", CalData[group_id].flag);
    debug_log("[MODULE_INFO]module_id: 0x%x", CalData[group_id].module_info.module_id);
    debug_log("[MODULE_INFO]PartNum = 0x%x 0x%x 0x%x\n",
              CalData[group_id].module_info.part_number[0],
              CalData[group_id].module_info.part_number[1],
              CalData[group_id].module_info.part_number[2]
            );
    debug_log("[MODULE_INFO]sensor_id:0x%x, lens_id:0x%x, vcm_id:0x%x,"
              "phase:0x%x, mirror_flip_status:0x%x, ir_fliter_id:0x%x, date:%d-%d",
               CalData[group_id].module_info.sensor_id,
               CalData[group_id].module_info.lens_id,
               CalData[group_id].module_info.vcm_id,
               CalData[group_id].module_info.phase,
               CalData[group_id].module_info.mirror_flip_status,
               CalData[group_id].module_info.ir_filter_id,
               CalData[group_id].module_info.year,
               CalData[group_id].module_info.month);
    debug_log("======================MODULE_INFO==================\n");

    /* AWB Gain (5100K) */
    CalR  = (CalData[group_id].awb_info.awb_r_h << 8) | CalData[group_id].awb_info.awb_r_l;
    CalGr = ( CalData[group_id].awb_info.awb_gr_h << 8) | CalData[group_id].awb_info.awb_gr_l;
    CalGb = ( CalData[group_id].awb_info.awb_gb_h << 8) |  CalData[group_id].awb_info.awb_gb_l;
    CalG  = (CalGr + CalGb + 1) >> 1;
    CalB  =( CalData[group_id].awb_info.awb_b_h << 8) | CalData[group_id].awb_info.awb_b_l;
    tempMax = MAX_TEMP(CalR,CalG,CalB);
    pCamCalData->Single2A.S2aAwb.rUnitGainu4R =
                (unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
    pCamCalData->Single2A.S2aAwb.rUnitGainu4G =
                (unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
    pCamCalData->Single2A.S2aAwb.rUnitGainu4B =
                (unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);

    /* AWB Golden Gain (5100K) */
    FacR  = (CalData[group_id].awb_info.golden_awb_r_h << 8) | CalData[group_id].awb_info.golden_awb_r_l;
    FacGr = (CalData[group_id].awb_info.golden_awb_gr_h << 8) | CalData[group_id].awb_info.golden_awb_gr_l;
    FacGb = (CalData[group_id].awb_info.golden_awb_gb_h << 8) | CalData[group_id].awb_info.golden_awb_gb_l;
    FacG  = ((FacGr + FacGb) + 1) >> 1;
    FacB  = (CalData[group_id].awb_info.golden_awb_b_h << 8) | CalData[group_id].awb_info.golden_awb_b_l;

    tempMax = MAX_TEMP(FacR,FacB,FacG);
    debug_log("[AWB_INFO]GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",FacR, FacG, FacB, tempMax);


    if (FacR == 0 || FacG == 0 || FacB == 0) {
         error_log("There are something wrong on EEPROM, plz contact module vendor!!\n");
         return CamCalReturnErr[pCamCalData->Command];
    }

    pCamCalData->Single2A.S2aAwb.rGoldGainu4R =
                (unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
    pCamCalData->Single2A.S2aAwb.rGoldGainu4G =
            (unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
    pCamCalData->Single2A.S2aAwb.rGoldGainu4B =
                (unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);

     /* Set AWB to 3A Layer */
    pCamCalData->Single2A.S2aAwb.rValueR    = CalR;
    pCamCalData->Single2A.S2aAwb.rValueGr   = CalGr;
    pCamCalData->Single2A.S2aAwb.rValueGb   = CalGb;
    pCamCalData->Single2A.S2aAwb.rValueB    = CalB;
    pCamCalData->Single2A.S2aAwb.rGoldenR   = FacR;
    pCamCalData->Single2A.S2aAwb.rGoldenGr  = FacGr;
    pCamCalData->Single2A.S2aAwb.rGoldenGb  = FacGb;
    pCamCalData->Single2A.S2aAwb.rGoldenB   = FacB;
    debug_log("======================AWB CAM_CAL==================\n");
    must_log("[rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R);
    must_log("[rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G);
    must_log("[rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B);

    must_log("[rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R);
    must_log("[rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G);
    must_log("[rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B);
    debug_log("======================AWB CAM_CAL==================\n");

    return CAM_CAL_ERR_NO_ERR;
}

static struct STRUCT_CALIBRATION_LAYOUT_STRUCT cal_layout_table = {
    0x00000001, 0x010b00ff, CAM_CAL_SINGLE_EEPROM_DATA,
    {
        {0x00000000, 0x00000000, 0x00000000, NULL},
        {0x00000000, 0x00000000, 0x00000000, NULL},
        {0x00000000, 0x00000000, 0x00000000, NULL},
        {0x00000001, 0x00001000, 0x0000007E, gc5035_do_2a_gain},
        {0x00000000, 0x00000000, 0x00000000, NULL},
        {0x00000000, 0x00000000, 0x00000000, NULL},
        {0x00000000, 0x00000000, 0x00000000, NULL},
    }
};

struct STRUCT_CAM_CAL_CONFIG_STRUCT gc5035_mtk_eeprom = {
    .name = "gc5035_mtk_eeprom",
    .check_layout_function = custom_layout_check,
    .read_function = gc5035_read_region,
    .layout = &cal_layout_table,
    .sensor_id = GC5035_SENSOR_ID,
    .i2c_write_id = 0x7e,
    .max_size = 0x2000, //bigger than start_addr + block_size (0x1000 + 0x7e)
    .enable_preload = 1,
    .preload_size = 126,
    .has_stored_data = 1,
};

