/*
 * File: miniisp_ctrl.c
 * Description: Mini ISP Ctrl sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2013/10/14; Aaron Chuang; Initial version
 *  2013/12/05; Bruce Chung; 2nd version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



/******Include File******/
#include <linux/delay.h>

#include "include/miniisp_customer_define.h"
#include "include/ispctrl_if_master.h"
#include "include/miniisp_ctrl.h"
#include "include/error/altek_state_err.h"
#include "include/miniisp_chip_base_define.h"
/******Private Constant Definition******/
#define MINI_ISP_LOG_TAG	"[miniisp_ctrl]"

/******Private Function Prototype******/

static int load_code_task(void *data);
#if (!ENABLE_LINUX_FW_LOADER)
static u16 calibration_check_sum(u8 *input_buffer_addr, u16 input_buffer_size);
#endif
/******Private Type Declaration******/


/******Private Global Variable******/

static struct memmory_dump_hdr_info mem_dum_hdr_cfg = {0};
static struct common_log_hdr_info  com_log_hdr_cfg = {0};
static bool stop_to_log;

/*Command parameter buffer*/
/*static u8 cmd_param_buf[T_SPI_CMD_LENGTH];*/
static u8 rcv_cmd_param_buf[T_SPI_CMD_LENGTH];


bool load_code_ready;

/******Public Global Variable******/


/******Public Function******/

/*************************************************************************/
/*operation cmd*/

/**
 *\brief Mini ISP open 0x4000
 *\param boot_code_file_name [In], Boot code filename
 *\param basic_code_file_name [In], Basic code filename
 *\param advanced_code_file_name [In], Advanced code filename
 *\param scenario_table_file_name [In], SC table filename
 *\param hdr_qmerge_data_file_name [In], HDR Qmerge data filename
 *\param irp0_qmerge_data_file_name [In], IRP0 Qmerge data filename
 *\param irp1_qmerge_data_file_name [In], IRP1 Qmerge data filename
 *\param pp_map_file_name [In], pp map filename
 *\return Error code
 */
errcode mini_isp_drv_open(char *pBoot_code_file_name,
				char *pBasic_code_file_name,
				char *pAdvanced_code_file_name,
				char *pScenario_table_file_name,
				char *pHdr_qmerge_data_file_name,
				char *pIrp0_qmerge_data_file_name,
				char *pIrp1_qmerge_data_file_name,
				char *pPP_map_file_name,
				char *pDepth_qmerge_file_name)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_MINIISPOPEN; /*0x4000*/
	char *cmd_param_buf[9];

	/* Command parameter buffer*/
	memset(cmd_param_buf, 0, sizeof(cmd_param_buf));
	misp_info("%s - start", __func__);
	/* Parameter 0 boot code filename*/
	cmd_param_buf[0] = pBoot_code_file_name;
	/* Parameter 1 basic code filename*/
	cmd_param_buf[1] = pBasic_code_file_name;
	/* Parameter 2 advanced code filename*/
	cmd_param_buf[2] = pAdvanced_code_file_name;
	/* Parameter 3 calibration filename(sc atable)*/
	cmd_param_buf[3] = pScenario_table_file_name;
	/* Parameter 4 hdr qmerge data filename*/
	cmd_param_buf[4] = pHdr_qmerge_data_file_name;
	/* Parameter 5 irp0 qmerge data filename*/
	cmd_param_buf[5] = pIrp0_qmerge_data_file_name;
	/* Parameter 6 irp1 qmerge data filename*/
	cmd_param_buf[6] = pIrp1_qmerge_data_file_name;
	/* Parameter 7 PP map filename*/
	cmd_param_buf[7] = pPP_map_file_name;
	/* Parameter 8 depth qmerge filename*/
	cmd_param_buf[8] = pDepth_qmerge_file_name;

	/* mini ISP open*/
	err = ispctrl_if_mast_execute_cmd(opcode, (u8 *)cmd_param_buf);

	if (err != ERR_SUCCESS)
		misp_err("%s open file failed. err: 0x%x", __func__, err);
	else
		misp_info("%s - open files success", __func__);

	return err;

}

/*************************************************************************/
/*bulk cmd*/

/**
 *\brief Mini ISP write boot code 0x2008
 *\return Error code
 */
errcode mini_isp_drv_write_boot_code(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	u16 opcode = ISPCMD_BULK_WRITE_BOOTCODE; /*0x2008*/

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);
	misp_info("%s write boot code state: %d", __func__, err);

	return err;
}

/**
 *\brief Mini ISP write boot code (short SPI Len) 0x2009
 *\return Error code
 */
errcode mini_isp_drv_write_boot_code_shortlen(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	u16 opcode = ISPCMD_BULK_WRITE_BOOTCODE_SHORT_LEN; /*0x2009*/

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);
	misp_info("%s write boot code state: %d", __func__, err);
	return err;
}
/**
 *\brief Mini ISP write basic code 0x2002
 *\return Error code
 */
errcode mini_isp_drv_write_basic_code(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	u16 opcode = ISPCMD_BULK_WRITE_BASICCODE; /*0x2002*/
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	misp_info("%s write basic code state: %d", __func__, err);

	return err;
}

/**
 *\brief Mini ISP write basic code (short SPI Len) 0x2003/0x2004/0x2005
 *\return Error code
 */
errcode mini_isp_drv_write_basic_code_shortlen(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	u16 opcode = ISPCMD_BULK_WRITE_BASICCODE_CODESUM; /*0x2005*/
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	misp_info("%s write basic code state: %d", __func__, err);
	return err;
}

/**
 *\brief MiniISP Write Calibration Data   0x210B
 *\param info_id [In],		0   :  boot data
 *					1   :  basic data
 *\return Error code
 */
errcode mini_isp_drv_write_spinor_data(u8 info_id)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u16 opcode = ISPCMD_BULK_WRITE_SPINOR_DATA; /*0x210E*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	/* Parameter 8 Info ID*/
	cmd_param_buf[8] = info_id;

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);
	misp_info("%s write calibration data state: %d", __func__, err);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_write_spinor_data);

#if ENABLE_LINUX_FW_LOADER
/**
 *\brief MiniISP Write Calibration Data   0x210B
 *\param info_id [In],		0   :  IQ data
 *				1   :  packet data
 *				2   :  scenario table
 *				3   :  qmerge hdr
 *				4   :  qmerge irp0
 *				5   :  qmerge irp1
 *				6   :  PP map
 *				7   :  blending table
 *              8   :  qmerge depth
 *              9   :  OTP data
 *\param buf_addr [In], otp/packet data buffer start address
 *\param buf_len [In], otp/packet data buffer len
 *\return Error code
 */
errcode mini_isp_drv_write_calibration_data(u8 info_id, u8 *buf_addr,
					u32 buf_len)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u16 opcode = ISPCMD_BULK_WRITE_CALIBRATION_DATA; /*0x210B*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	u8 *allocated_memmory = 0;

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	/*
	 *misp_info("%s info_id %d  buf_addr %p buf_len %d",
	 *	__func__, info_id, buf_addr, buf_len);
	 */
	/* Parameter 0 Info ID*/
	cmd_param_buf[8] = info_id;
	if (((info_id >= 2) && (info_id < 7)) || (info_id == 8)) {
		err = ispctrl_if_mast_execute_cmd(opcode,
						cmd_param_buf);
	} else {
		/*  Request memory*/
		allocated_memmory = kzalloc(buf_len+T_SPI_CMD_LENGTH,
					GFP_KERNEL);
		if (!allocated_memmory) {
			err = ~ERR_SUCCESS;
			goto allocate_memory_fail;
		}

		/* fill checksum and block size at
		 * mast_bulk_data_cmd_write_calibration_data api
		 */
		memcpy(allocated_memmory + T_SPI_CMD_LENGTH,
			buf_addr, buf_len);
		memcpy(allocated_memmory, &buf_len, sizeof(u32));
		memcpy(allocated_memmory + 8, &info_id, sizeof(u8));

		/*
		 *misp_info("%s Cal_param[0][1][2][3]:%02x %02x %02x %02x",
		 *		__func__, allocated_memmory[0],
		 *		allocated_memmory[1],
		 *		allocated_memmory[2],
		 *		allocated_memmory[3]);
		 *misp_info("%s Cal_param[4][5][6][7]:%02x %02x %02x %02x",
		 *		__func__, allocated_memmory[4],
		 *		allocated_memmory[5],
		 *		allocated_memmory[6],
		 *		allocated_memmory[7]);
		 *misp_info("%s Cal_param[8][9][10]:%02x %02x %02x",
		 *		__func__, allocated_memmory[8],
		 *		allocated_memmory[9],
		 *		allocated_memmory[10]);
		 */
		err = ispctrl_if_mast_execute_cmd(opcode,
					allocated_memmory);
		kfree(allocated_memmory);
	}
	misp_info("%s write calibration data state: %d", __func__, err);


	goto miniisp_drv_write_calibration_data_end;
allocate_memory_fail:
	misp_err("%s Allocate memory failed.", __func__);
	kfree(allocated_memmory);
miniisp_drv_write_calibration_data_end:

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_write_calibration_data);

#else
/**
 *\brief MiniISP Write Calibration Data   0x210B
 *\param info_id [In],		0   :  IQ data
 *				1   :  packet data
 *				2   :  scenario table
 *				3   :  qmerge hdr
 *				4   :  qmerge irp0
 *				5   :  qmerge irp1
 *				6   :  PP map
 *				7   :  blending table
 *              8   :  qmerge depth
 *              9   :  OTP data
 *\param buf_addr [In], otp/packet data buffer start address
 *\param buf_len [In], otp/packet data buffer len
 *\return Error code
 */
errcode mini_isp_drv_write_calibration_data(u8 info_id, u8 *buf_addr,
					u32 buf_len)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u16 opcode = ISPCMD_BULK_WRITE_CALIBRATION_DATA; /*0x210B*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	u16 chk_sum;
	u32 block_size = 384*1024;
	u8 *allocated_memmory = 0;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	if ((dev_global_variable->now_state != MINI_ISP_STANDBY) &&
		(dev_global_variable->now_state != MINI_ISP_SENSOR_MODE))
		return ERR_MINIISP_STATE_ERROR_SEQUENCE;

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	/*
	 *misp_info("%s info_id %d  buf_addr %p buf_len %d",
	 *	__func__, info_id, buf_addr, buf_len);
	 */
	/* Parameter 0 Info ID*/
	cmd_param_buf[8] = info_id;
	if (((info_id >= 2) && (info_id < 7)) || (info_id == 8)) {
		err = ispctrl_if_mast_execute_cmd(opcode,
						cmd_param_buf);
	} else {
		/*  Request memory*/
		allocated_memmory = kzalloc(buf_len+T_SPI_CMD_LENGTH,
					GFP_KERNEL);
		if (!allocated_memmory) {
			err = ~ERR_SUCCESS;
			goto allocate_memory_fail;
		}
		memcpy(allocated_memmory + T_SPI_CMD_LENGTH, buf_addr,
			buf_len);
		memcpy(allocated_memmory, &buf_len, sizeof(u32));
		memcpy(allocated_memmory + 4, &block_size, sizeof(u32));
		memcpy(allocated_memmory + 8, &info_id, sizeof(u8));
		chk_sum = calibration_check_sum(
			allocated_memmory + T_SPI_CMD_LENGTH,
			buf_len);
		memcpy(allocated_memmory+9, &chk_sum, sizeof(u16));
		/*
		 *misp_info("%s Cal_param[0][1][2][3]:%02x %02x %02x %02x",
		 *		__func__, allocated_memmory[0],
		 *		allocated_memmory[1],
		 *		allocated_memmory[2],
		 *		allocated_memmory[3]);
		 *misp_info("%s Cal_param[4][5][6][7]:%02x %02x %02x %02x",
		 *		__func__, allocated_memmory[4],
		 *		allocated_memmory[5],
		 *		allocated_memmory[6],
		 *		allocated_memmory[7]);
		 *misp_info("%s Cal_param[8][9][10]:%02x %02x %02x",
		 *		__func__, allocated_memmory[8],
		 *		allocated_memmory[9],
		 *		allocated_memmory[10]);
		 */
		err = ispctrl_if_mast_execute_cmd(opcode,
					allocated_memmory);
		kfree(allocated_memmory);
	}
	misp_info("%s write calibration data state: %d", __func__, err);


	goto miniisp_drv_write_calibration_data_end;
allocate_memory_fail:
	misp_err("%s Allocate memory failed.", __func__);
	kfree(allocated_memmory);
miniisp_drv_write_calibration_data_end:

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_write_calibration_data);
#endif

/**
 *\brief Read Calibration Data
 *\param info_param [In],
 *\return Error code
 */
errcode mini_isp_drv_read_calibration_data(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BULK_READ_CALIBRATION_DATA;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	mem_dum_hdr_cfg.start_addr = EEPROM_BUFFER_ADDRESS;
	mem_dum_hdr_cfg.total_size = EEPROM_BUFFER_SIZE;
	if (SPI_SHORT_LEN_MODE_READ_ENABLE)
		mem_dum_hdr_cfg.block_size = SPI_BLOCK_LEN;
	else
		mem_dum_hdr_cfg.block_size = SPI_TX_BULK_SIZE;
	mem_dum_hdr_cfg.dump_mode = T_MEMDUMP_CPURUN;

	/*Copy it to transmission header*/
	memcpy(&cmd_param_buf[0], (u8 *)&mem_dum_hdr_cfg,
		sizeof(struct memmory_dump_hdr_info));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_read_calibration_data);

/**
 *\brief Read memory
 *\param start_addr [In]starting address
 *\param read_size [In]TotalReadSize
 *\return Error code
 */
errcode mini_isp_drv_read_memory(u32 start_addr, u32 read_size)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BULK_READ_MEMORY;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	mem_dum_hdr_cfg.start_addr = start_addr;/*0x0;//0x9DC00;*/
	mem_dum_hdr_cfg.total_size = read_size;/*T_MEMSIZE;*/
	mem_dum_hdr_cfg.block_size = SPI_TX_BULK_SIZE;
	mem_dum_hdr_cfg.dump_mode = T_MEMDUMP_CPURUN;

	/*Copy it to transmission header*/
	memcpy(&cmd_param_buf[0], (u8 *)&mem_dum_hdr_cfg,
		sizeof(struct memmory_dump_hdr_info));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_read_memory);

/**
 *\brief Get Depth rect A, B, invrect parameter
 *\param 0 : depth rect A, B InvRect parameter structure array buffer
 *\param 1 : transfer mode. (Default set to 0)
 *\param 2 : bluk block size
 *\return Error code
 */
errcode mini_isp_drv_write_depth_rectab_invrect(
		struct depth_rectab_invrect_param *rect_param,
		u8 trans_mode,
		u32 block_size)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BULK_WRITE_DEPTH_RECTAB_INVRECT; /*0x210C*/
	u32 rect_set_num = 0;
	u32 rectinfo_total_size = 0;
	static struct isp_cmd_depth_rectab_invrect_info rect_info;

	if (trans_mode == 0)
		rect_set_num = 3;
	else
		rect_set_num = 1;

	rectinfo_total_size
		= rect_set_num*sizeof(struct depth_rectab_invrect_param);
	/* copy to local static structure */
	memcpy(&(rect_info.rect_param[0]), rect_param,
		rectinfo_total_size);

	rect_info.trans_mode = trans_mode;
	rect_info.block_size = block_size;

	err = ispctrl_if_mast_execute_cmd(opcode, (u8 *)&rect_info);
	return err;
}
EXPORT_SYMBOL(mini_isp_drv_write_depth_rectab_invrect);

/**
 *\brief Reading Common Log
 *\param stop [In], Stop to log flag
 *\return Error code
 */
errcode mini_isp_drv_read_com_log(bool stop)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BULK_READ_COMLOG;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	/* Wait semaphore*/
	/*SEMAPHORE_LW_Wait( ISPCTRLIFMASTER_SEMAPHORE_LOGDUMP,*/
	/*	SEMAPHORE_WAITFOREVER );*/

	/* Force to stop log*/
	/*To inform isp to set log level as 0 for stoping log reight away*/
	if (stop)
		mini_isp_drv_set_com_log_level(0);

	if (!stop_to_log) {
		com_log_hdr_cfg.total_size = LEVEL_LOG_BUFFER_SIZE;
		com_log_hdr_cfg.block_size = SPI_TX_BULK_SIZE;

		/*Copy it to transmission header*/
		memcpy(&cmd_param_buf[0], (u8 *)&com_log_hdr_cfg,
				sizeof(struct common_log_hdr_info));

		err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

		/* Force to stop log*/
		if (stop)
			stop_to_log = true;
	}

	/* Post semaphore*/
	/*SEMAPHORE_LW_Post( ISPCTRLIFMASTER_SEMAPHORE_LOGDUMP );*/

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_read_com_log);

/*************************************************************************/
/*camera profile cmd*/

/**
 *\brief Set Sensor Mode	0x300A
 *\param sensor_on_off [In],sensor on/off
 *\param scenario_id[In], Scenario ID
 *\param mipi_tx_skew_enable[In],  mipi tx skew on(1)/off(0)
 *\param ae_weighting_table_index[In]
 *\param merge_mode_enable[In]
 *\ bit[0:3] :
 *\ set 0 for normal mode
 *\ set 1 for merge mode, only for image samller than 640X480 case
 *\ set 2 for depth test pattern mode
 *\ bit[4] :
 *\ set 0 for turn on sensor by AP.
 *\ set 1 for turn on sensor by AL6100.
 *\return Error code
 */
errcode mini_isp_drv_set_sensor_mode(u8 sensor_on_off, u8 scenario_id,
		u8 mipi_tx_skew_enable, u8 ae_weighting_table_index,
		u8 merge_mode_enable)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	misp_info("%s no FSM", __func__);
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	/* Parameter 0 sensor on/off*/
	cmd_param_buf[0] = sensor_on_off;
	/* Parameter 1 Scenario ID*/
	cmd_param_buf[1] = scenario_id;
	/* Parameter 2 mipi tx skew on/off*/
	cmd_param_buf[2] = mipi_tx_skew_enable;
	/* Parameter 3 ae weighting table index*/
	cmd_param_buf[3] = ae_weighting_table_index;
	/* Parameter 4 merge_mode_enable*/
	cmd_param_buf[4] = merge_mode_enable;
	/* Parameter 5 reserve*/
	cmd_param_buf[5] = 0;
	/* Parameter 6 reserve*/
	cmd_param_buf[6] = 0;

	if (sensor_on_off == 0) {
		/* Set Sensor Mode*/
		err = ispctrl_if_mast_execute_cmd(ISPCMD_CAMERA_SET_SENSORMODE,
			cmd_param_buf);

		if (err != 0) {
			misp_err("%s err, error code = %x",
				 __func__, err);
			return err;
		}
	} else {
		/* Set Sensor Mode*/
		err = ispctrl_if_mast_execute_cmd(ISPCMD_CAMERA_SET_SENSORMODE,
			cmd_param_buf);
		if (err != 0) {
			misp_err("%s err, error code = %x",
				 __func__, err);
			return err;
		}
	}
	return err;

}
EXPORT_SYMBOL(mini_isp_drv_set_sensor_mode);

/*0x300B*/
errcode mini_isp_drv_get_sensor_mode(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_GET_SENSORMODE; /*0x300B*/
	err = ispctrl_if_mast_execute_cmd(opcode, rcv_cmd_param_buf);

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_get_sensor_mode);

/*
brief Set Output format		0x300D
param1[In]depth_map_setting = resolution | opereation_mode
resolution
 0: Disable depth function (Depth engine is disable)
 1: 180p
 2: 360p
 3: 720p
 4: 480p
 5: 400p
opereation_mode,
 0x00: DEPTH_BIT_DG_ONLY
 0x10: DEPTH_BIT_DP
 0x40: DEPTH_BIT_HIGH_DISTORTION_RATE
*/
/*
 param2 [In]depth_process_type_and_qlevel = process_type | quality level
 B[0:2] process_type: value 0x6 as reserve
 B[3:6] quality level: 0 ~ 15

 param3 [In]
 B[0:0] InvRect bypass: set 1 for enable InvRect bypass. set 0 disable.
		(This function is incompatible with
		mini_isp_drv_write_depth_rectab_invrect,
		 Do not configure this bit if set
		 mini_isp_drv_write_depth_rectab_invrect before)
 return Error code
*/
errcode mini_isp_drv_set_output_format(
				struct isp_cmd_set_output_format *output_format)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_OUTPUTFORMAT; /*0x300D*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, output_format,
			sizeof(struct isp_cmd_set_output_format));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_set_output_format);

/**
 *\brief Set CP mode		0x300E
 *\return Error code
 */
errcode mini_isp_drv_set_cp_mode(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	misp_info("%s no FSM", __func__);
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E))
		mini_isp_e_to_a();

	dev_global_variable->spi_low_speed_mode = 1;

	err = ispctrl_if_mast_execute_cmd(ISPCMD_CAMERA_SET_CP_MODE,
			NULL);
	if (err != 0) {
		dev_global_variable->spi_low_speed_mode = 0;
		return err;
	}

	if (dev_global_variable->intf_status & INTF_SPI_READY)
		mini_isp_a_to_e();
	/*enter code here*/
	mini_isp_cp_mode_suspend_flow();

	if (err != 0) {
		misp_err("%s err, error code = %x", __func__, err);
		return err;
	}
	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_cp_mode);

/**
 *\brief Set AE statistics		0x300F
 *\param ae_statistics [In], ae statistics
 *\return Error code
 */
errcode mini_isp_drv_set_ae_statistics(
	struct isp_cmd_ae_statistics *ae_statistics)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	misp_info("%s no FSM", __func__);
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, ae_statistics,
		sizeof(struct isp_cmd_ae_statistics));
	err = ispctrl_if_mast_execute_cmd(ISPCMD_CAMERA_SET_AE_STATISTICS,
		cmd_param_buf);

	if (err != 0)
		misp_err("%s err, error code = %x", __func__, err);

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_set_ae_statistics);

/**
 *\brief Preview stream on/off		0x3010
 *\param tx0_stream_on_off [In], Tx0 stream on/off
 *\param tx1_stream_on_off [In], Tx1 stream on/off
 *\return Error code
 */
errcode mini_isp_drv_preview_stream_on_off(u8 tx0_stream_on_off,
				u8 tx1_stream_on_off)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	misp_info("%s no FSM", __func__);
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	/* Parameter 0 Tx0 stream on/off*/
	cmd_param_buf[0] = tx0_stream_on_off;
	/* Parameter 1 Tx1 stream on/off*/
	cmd_param_buf[1] = tx1_stream_on_off;

	err = ispctrl_if_mast_execute_cmd(ISPCMD_CAMERA_PREVIEWSTREAMONOFF,
		cmd_param_buf);

	if (err != 0)
		misp_err("%s err, error code = %x", __func__, err);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_preview_stream_on_off);

/**
 *\brief Dual PD Y Calcualtion Weighting		0x3011
 *\param isp_cmd_dual_pd_y_calculation_weightings [In],
   dual PD Y calculation weightings
 *\return Error code
 */
errcode mini_isp_drv_dual_pd_y_calculation_weighting(
	struct isp_cmd_dual_pd_y_calculation_weightings *calculation_weighting)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_DUALPDYCALCULATIONWEIGHT; /*0x3010*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, calculation_weighting,
		sizeof(struct isp_cmd_dual_pd_y_calculation_weightings));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_dual_pd_y_calculation_weighting);


/**
 *\brief LED power control		0x3012
 *\param projector_control_param [In],
 *\return Error code
 */
errcode mini_isp_drv_led_power_control(
	struct isp_cmd_led_power_control *projector_control_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_LED_POWERCONTROL; /*0x3012*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, projector_control_param,
		sizeof(struct isp_cmd_led_power_control));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_led_power_control);

/**
 *\brief Active AE		0x3013
 *\param active_ae_param [In],
 *\return Error code
 */
errcode mini_isp_drv_active_ae(
	struct isp_cmd_active_ae *active_ae_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_ACTIVE_AE; /*0x3013*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, active_ae_param,
		sizeof(struct isp_cmd_active_ae));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_active_ae);


/**
 *\brief  ISP AE control mode on off		0x3014
 *\param isp_ae_control_mode_on_off [In], 0:off 1:on
 *\return Error code
 */
errcode mini_isp_drv_isp_ae_control_mode_on_off(u8 isp_ae_control_mode_on_off)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_ISP_AECONTROLONOFF; /*0x3014*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	/* Parameter 0 isp_ae_control_mode_on_off*/
	cmd_param_buf[0] = isp_ae_control_mode_on_off;

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_isp_ae_control_mode_on_off);

/**
 *\brief  Set Frame Rate Limite		0x3015
 *\param set_frame_rate_param [In],
 *\return Error code
 */
errcode mini_isp_drv_set_frame_rate_limits(
	struct isp_cmd_frame_rate_limits *set_frame_rate_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_FRAMERATELIMITS; /*0x3015*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, set_frame_rate_param,
		sizeof(struct isp_cmd_frame_rate_limits));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_frame_rate_limits);

/**
 *\brief  Set period drop frame		0x3016
 *\param set_period_drop_fram_param [In],
 *\return Error code
 */
errcode mini_isp_drv_set_period_drop_frame(
	struct isp_cmd_period_drop_frame *set_period_drop_fram_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_PERIODDROPFRAME; /*0x3016*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, set_period_drop_fram_param,
		sizeof(struct isp_cmd_period_drop_frame));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_period_drop_frame);

/**
 *\brief Leave CP Mode
 *\using set sensor mode opcode :0x300A
 *\param sensor_on_off [In],sensor on/off
 *\param scenario_id[In], Scenario ID
 *\param mipi_tx_skew_enable[In],  mipi tx skew on(1)/off(0)
 *\param ae_weighting_table_index[In]
 *\return Error code
 */
errcode mini_isp_drv_leave_cp_mode(u8 sensor_on_off, u8 scenario_id,
		u8 mipi_tx_skew_enable, u8 ae_weighting_table_index,
		u8 merge_mode_enable)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	int original_altek_spi_mode;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	original_altek_spi_mode = dev_global_variable->altek_spi_mode;

	misp_info("%s no FSM", __func__);
	/* Parameter 0 sensor on/off*/
	cmd_param_buf[0] = sensor_on_off;
	/* Parameter 1 Scenario ID*/
	cmd_param_buf[1] = scenario_id;
		/* Parameter 2 mipi tx skew on/off*/
	cmd_param_buf[2] = mipi_tx_skew_enable;
	/* Parameter 3 ae_weighting_table_index*/
	cmd_param_buf[3] = ae_weighting_table_index;
	/* Parameter 4 merge_mode_enable*/
	cmd_param_buf[4] = merge_mode_enable;
	/* Parameter 5 reserve*/
	cmd_param_buf[5] = 0;
	/* Parameter 6 reserve*/
	cmd_param_buf[6] = 0;

	if (sensor_on_off) {
		if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
			(original_altek_spi_mode == ALTEK_SPI_MODE_A))
			mini_isp_a_to_e();
		mini_isp_cp_mode_resume_flow();

		dev_global_variable->spi_low_speed_mode = 0;

		if (dev_global_variable->intf_status & INTF_SPI_READY)
			mini_isp_e_to_a();

		/* Set Sensor Mode*/
		err = ispctrl_if_mast_execute_cmd(ISPCMD_CAMERA_SET_SENSORMODE,
				cmd_param_buf);

		if (err != 0) {
			misp_err("mini_isp_drv_leave_cp_mode err, errcode = %x",
				err);
			return err;
		}
	} else {
		if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
			(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A))
			mini_isp_a_to_e();

		mini_isp_cp_mode_resume_flow();
		dev_global_variable->spi_low_speed_mode = 0;

		if (dev_global_variable->intf_status & INTF_SPI_READY)
			mini_isp_e_to_a();
	}

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_leave_cp_mode);

/*************************************************************************/
/*system cmd*/

/**
 *\brief Set ISP register	0x0100
 *\param a_udStartAddr [In], Reg start addr
 *\param reg_value [In], Reg value
 *\return Error code
 */
errcode mini_isp_drv_set_isp_register(u32 reg_start_addr, u32 reg_value)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_SET_ISPREGISTER; /*0x0100*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	/* Reg start addr*/
	memcpy(&cmd_param_buf[0], &reg_start_addr, 4);
	/* Reg count*/
	memcpy(&cmd_param_buf[4], &reg_value, 4);

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_isp_register);


/**
 *\brief Set ISP register	0x0101
 *\param a_udStartAddr [In], Reg start addr
 *\param reg_count [In], Reg count
 *\return Error code
 */
errcode mini_isp_drv_get_isp_register(u32 reg_start_addr, u32 reg_count)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_GET_ISPREGISTER; /*0x0101*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	/* Reg start addr*/
	memcpy(&cmd_param_buf[0], &reg_start_addr, 4);
	/* Reg count*/
	memcpy(&cmd_param_buf[4], &reg_count, 4);

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_get_isp_register);


errcode mini_isp_drv_set_com_log_level(u32 log_level)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_SET_COMLOGLEVEL;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, &log_level, sizeof(u32));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_com_log_level);

/*0x0015*/
errcode mini_isp_drv_get_last_exec_cmd(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_GET_STATUSOFLASTEXECUTEDCOMMAND; /*0x0015*/
	err = ispctrl_if_mast_execute_cmd(opcode, rcv_cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_get_last_exec_cmd);

/*0x0016*/
errcode mini_isp_drv_get_err_code_cmd(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_GET_ERRORCODE; /*0x0016*/
	err = ispctrl_if_mast_execute_cmd(opcode, rcv_cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_get_err_code_cmd);

/*0x0016*/
errcode mini_isp_drv_get_err_code_cmd_in_irq(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	void *devdata = (void *)get_mini_isp_intf(MINIISP_I2C_TOP);
	u8 param[64];
	u32 al6100_errcode = 0;
	/* Parameter size*/
	/*get last ten error code and error status*/
	u32 para_size = (sizeof(errcode))*10;

	misp_err("%s - enter + + + + +", __func__);

	/* Send command to slave*/
	err = ispctrl_mast_send_cmd_to_slave(
		devdata, ISPCMD_SYSTEM_GET_ERRORCODE, param, 0);
	if (err != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_error_code_command_end;

	/* Get data from slave*/
	err = ispctrl_mast_recv_response_from_slave(devdata, param,
							para_size, false);
	if (err != ERR_SUCCESS)
		goto mast_sys_manage_cmd_get_error_code_command_end;

	al6100_errcode = *(u32 *)param;
	misp_err("%s last error code %#x", __func__, al6100_errcode);

mast_sys_manage_cmd_get_error_code_command_end:

	misp_err("%s - leave - - - - -", __func__);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_get_err_code_cmd_in_irq);


/**
 *\brief Get Chip test Report	0x010A
 *\return Error code
 */
errcode mini_isp_drv_get_chip_test_report(void)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_GET_CHIPTESTREPORT; /*0x010A*/
	err = ispctrl_if_mast_execute_cmd(opcode, rcv_cmd_param_buf);

	misp_info("%s chip test report: %x %x %x %x", __func__,
		rcv_cmd_param_buf[0], rcv_cmd_param_buf[1],
		rcv_cmd_param_buf[2], rcv_cmd_param_buf[3]);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_get_chip_test_report);

/*************************************************************************/
/*basic cmd*/

/**
 *\brief Set Depth 3A Information	0x10B9
 *\param depth_3a_info [In], ISP Depth 3A parameter
 *\return Error code
 */
errcode mini_isp_drv_set_depth_3a_info(
			struct isp_cmd_depth_3a_info *depth_3a_info)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 *param;

	misp_info("%s no FSM", __func__);
	param = kzalloc(sizeof(struct isp_cmd_depth_3a_info), GFP_KERNEL);
	if (param != NULL) {
		/* Copy ISP Depth 3A Info*/
		memcpy(param, depth_3a_info,
			sizeof(struct isp_cmd_depth_3a_info));
	} else {
		misp_info("%s buf alloc fail!", __func__);
		return err;
	}

	err = ispctrl_if_mast_execute_cmd(
		ISPCMD_BASIC_SET_DEPTH_3A_INFO, param);

	kfree(param);

	if (err != 0)
		misp_err("%s err, error code = %x", __func__, err);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_depth_3a_info);


/**
 *\brief Set Depth auto interleave mode	0x10BC
 *\param depth_auto_interleave_param [In], ISP Depth auto interleave parameter
 *\return Error code
 */
errcode mini_isp_drv_set_depth_auto_interleave_mode(
	struct isp_cmd_depth_auto_interleave_param *depth_auto_interleave_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BASIC_SET_DEPTH_AUTO_INTERLEAVE_MODE; /*0x10BC*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, depth_auto_interleave_param,
		sizeof(struct isp_cmd_depth_auto_interleave_param));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_depth_auto_interleave_mode);

/**
 *\brief Set Projector Interleave Mode with Depth Type	0x10BD
 *\param projector_interleave_mode_with_depth_type [In],
 *\      0: depth active, 1: depth passive
 *\return Error code
 */
errcode mini_isp_drv_projector_interleave_mode_depth_type(
	u8 projector_interleave_mode_with_depth_type)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BASIC_SET_INTERLEAVE_MODE_DEPTH_TYPE; /*0x10BD*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	cmd_param_buf[0] = projector_interleave_mode_with_depth_type;

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_projector_interleave_mode_depth_type);

/**
 *\brief Set Depth Polish LEVEL	0x10BE
 *\param depth_polish_level [In], 0~100
 *\return Error code
 */
errcode mini_isp_drv_set_depth_polish_level(u8 depth_polish_level)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BASIC_SET_DEPTH_POLISH_LEVEL; /*0x10BE*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	cmd_param_buf[0] = depth_polish_level;

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_depth_polish_level);

/**
 *\brief Set Exposure Parameter	0x10BF
 *\param exposure_param [In], ISP Exposure parameter
 *\return Error code
 */
errcode mini_isp_drv_set_exposure_param(
	struct isp_cmd_exposure_param *exposure_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BASIC_SET_EXPOSURE_PARAM; /*0x10BF*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, exposure_param,
		sizeof(struct isp_cmd_exposure_param));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_exposure_param);

/**
 *\brief Set Exposure Parameter	0x10BF
 *\param exposure_param [In], ISP Exposure parameter
 *\return Error code
 */
errcode mini_isp_drv_set_depth_stream_size(
	struct isp_cmd_depth_stream_size *depth_stream_size)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_BASIC_SET_DEPTH_STREAM_SIZE; /*0x10C0*/
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, depth_stream_size,
		sizeof(struct isp_cmd_depth_stream_size));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_depth_stream_size);

/*************************************************************************/
/* cuurent no use
u16 mini_isp_drv_read_spi_status(void)
{
	return ispctrl_if_mast_read_spi_status();
}
EXPORT_SYMBOL(mini_isp_drv_read_spi_status);
*/
/**
 *\brief Write boot code and basic code
 *\param None
 *\return None
 */
int mini_isp_drv_boot_mini_isp(void)
{
	errcode err = ERR_SUCCESS;


	/* Write boot code*/
	err = mini_isp_drv_write_boot_code();
	if (err != ERR_SUCCESS)
		goto mini_isp_drv_boot_mini_isp_end;

	udelay(500);

	/* Write basic code*/
	err = mini_isp_drv_write_basic_code();

mini_isp_drv_boot_mini_isp_end:

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_boot_mini_isp);

/**
 *\brief Write boot code and basic code (short SPI Len)
 *\param None
 *\return None
 */
int mini_isp_drv_boot_mini_isp_shortlen(void)
{
	errcode err = ERR_SUCCESS;


	/* Write boot code*/
	err = mini_isp_drv_write_boot_code_shortlen();
	if (err != ERR_SUCCESS)
		goto mini_isp_drv_boot_mini_isp_shortlen_end;

	udelay(500);

	/* Write basic code*/
	err = mini_isp_drv_write_basic_code_shortlen();

mini_isp_drv_boot_mini_isp_shortlen_end:

	return err;

}
EXPORT_SYMBOL(mini_isp_drv_boot_mini_isp_shortlen);

/**
 *\brief Open boot and FW file then write boot code and FW code
 *\param None
 *\return Error code
 */
errcode mini_isp_drv_load_fw(void)
{
	errcode err = ERR_SUCCESS;

	misp_info("mini_isp_drv_setting(0) mini_isp_drv_load_fw start");
	/* Clear load code ready flag;*/
	load_code_ready = false;
	/*spi isr task*/
	/*g_ptload_code_task = kthread_run(load_code_task, NULL, */
	/*		"miniISP_loadcode_thread");*/

	load_code_task(NULL);

	misp_info("mini_isp_drv_setting(0) mini_isp_drv_load_fw X");
	return err;

}
EXPORT_SYMBOL(mini_isp_drv_load_fw);

/**
 *\brief  Wait miniISP event
 *\param  e [In], MINI_ISP_EVENT
 *\return Errorcode
 */
int mini_isp_drv_wait_for_event(u16 e)
{
	return mini_isp_wait_for_event(e);
}
EXPORT_SYMBOL(mini_isp_drv_wait_for_event);


/**
 *\brief Set mode to miniISP
 *\param  mini_isp_mode [In], Select ISP MODE,
 *0:(isp already in state A)normal case load FW directly,
 *1 :(isp state inital in state E)set state E to state A
 *2 :(isp already in state A)set state A to state E for debug ,
 *3 :leave HW bypass
 *4 :Get Chip ID
 *else : None Support
 *\return Errorcode
 */
errcode mini_isp_drv_setting(u16 mini_isp_mode)
{
	errcode err = ERR_SUCCESS;
	struct misp_global_variable *dev_global_variable;

	misp_info("%s no FSM", __func__);
	dev_global_variable = get_mini_isp_global_variable();

	if (mini_isp_mode == MINI_ISP_MODE_NORMAL) {
		/*check bypass mode be set or not*/
		/*if yes, leave bypass mode*/
		mini_isp_check_and_leave_bypass_mode();
		/*normal case load FW*/
		err = mini_isp_drv_load_fw();

		if (err != 0)
			goto mini_isp_drv_setting_err;
	} else if (mini_isp_mode == MINI_ISP_MODE_E2A) {
		/*isp, inital in E,*/
		mini_isp_e_to_a();
	} else if (mini_isp_mode == MINI_ISP_MODE_A2E) {
		mini_isp_a_to_e();
	} else if (mini_isp_mode == MINI_ISP_MODE_LEAVE_BYPASS) {
		/*check bypass mode be set or not*/
		/*if yes, leave bypass mode*/
		mini_isp_check_and_leave_bypass_mode();
		if (err != 0)
			goto mini_isp_drv_setting_err;
	} else if (mini_isp_mode == MINI_ISP_MODE_GET_CHIP_ID) {
		mini_isp_get_chip_id();
	} else if (mini_isp_mode == MINI_ISP_MODE_CHIP_INIT) {
		/*set some reg value to let it know should chang to A*/
		/* For SPI_Nor, do not call this */
		mini_isp_chip_init();
	} else {
		misp_err("%s err, none support setting", __func__);
	}
	return err;
mini_isp_drv_setting_err:
	misp_err("%s err, mini_isp_mode = %d, error code = %x", __func__,
			mini_isp_mode, err);
	return err;
}
EXPORT_SYMBOL(mini_isp_drv_setting);

void mini_isp_drv_altek_i2c_mode_change(void)
{
	mini_isp_register_write(0xffea0100, 0x3201);
	mini_isp_register_write(0xffea0104, 0x3201);
}
EXPORT_SYMBOL(mini_isp_drv_altek_i2c_mode_change);

/**
 *\brief set bypass mode
 *\param  bypass_mode [In], Select bypass MODE,
 *\return Errorcode
 */
errcode mini_isp_drv_set_bypass_mode(u16 bypass_mode)
{
	errcode err = ERR_SUCCESS;
	misp_info("%s no FSM", __func__);
	/*check bypass mode be set or not*/
	/*if yes, leave bypass mode*/
	mini_isp_check_and_leave_bypass_mode();
	/*Pure bypass by sensor*/
	err = mini_isp_pure_bypass(bypass_mode);

	if (err != 0)
		goto mini_isp_drv_setting_err;

	return err;
mini_isp_drv_setting_err:
	misp_err("%s err, bypass_mode = %d, error code = %x", __func__,
			bypass_mode, err);
	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_bypass_mode);

/**
 *\brief set Max exposure
 *\param  paramlength [In], Select bypass MODE,
 *\return Errorcode
 */
errcode mini_isp_drv_set_max_exposure(
	struct isp_cmd_exposure_param *max_exposure_info)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_MAX_EXPOSURE;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, max_exposure_info,
		sizeof(struct isp_cmd_exposure_param));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_max_exposure);

/**
 *\brief set target mean
 *\param  paramlength [In], Select bypass MODE,
 *\return Errorcode
 */
errcode mini_isp_drv_set_target_mean(
	struct isp_cmd_target_mean *target_mean_info)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_AE_TARGET_MEAN;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, target_mean_info,
		sizeof(struct isp_cmd_target_mean));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_target_mean);

errcode mini_isp_drv_frame_sync_control(
	struct isp_cmd_frame_sync_control *frame_sync_control_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_FRAME_SYNC_CONTROL;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, frame_sync_control_param,
		sizeof(struct isp_cmd_frame_sync_control));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_frame_sync_control);

errcode mini_isp_drv_set_shot_mode(
	struct isp_cmd_set_shot_mode *set_shot_mode_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_SHOT_MODE;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, set_shot_mode_param,
		sizeof(struct isp_cmd_set_shot_mode));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_shot_mode);

errcode mini_isp_drv_lighting_ctrl(
	struct isp_cmd_lighting_ctrl *lighting_ctrl)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_LIGHTING_CTRL;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, lighting_ctrl,
		sizeof(struct isp_cmd_lighting_ctrl));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_lighting_ctrl);

errcode mini_isp_drv_set_min_exposure(
	struct isp_cmd_exposure_param *min_exposure_info)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_MIN_EXPOSURE;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, min_exposure_info,
		sizeof(struct isp_cmd_exposure_param));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_min_exposure);

errcode mini_isp_drv_set_max_exposure_slope(
	struct isp_cmd_max_exposure_slope *max_exposure_slope_info)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_MAX_EXPOSURE_SLOPE;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);

	memcpy(cmd_param_buf, max_exposure_slope_info,
		sizeof(struct isp_cmd_max_exposure_slope));


	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_set_max_exposure_slope);

/**
 *\brief Set Sensor Mode	0x301C
 *\param en_update
 *\   [In], en_update = update short distance | update compensation
 *\bit[0:3] = update compensation
 *\bit[4:7] = update short distance
 *\param short_distance     [In], short distance
 *\param compensation       [In], compensation value, Ground mode only
 *\return Error code
 */

errcode mini_isp_drv_depth_compensation(
	struct isp_cmd_depth_compensation_param *depth_compensation_param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_DEPTH_COMPENSATION;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, depth_compensation_param,
			sizeof(struct isp_cmd_depth_compensation_param));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_depth_compensation);

/**
 *\brief Set Sensor Mode	0x301D
 *\param cycleLen
 *\   [In], en_update = update short distance | update global shift
 *\param depth_triggerBitField
 *\   [In], bit 0 : 1st frame, bit 1 : 2nd frame
 *\param depthoutput_triggerBitField
 *\   [In], bit 0 : 1st frame, bit 1 : 2nd frame
 *\return Error code
 */
errcode mini_isp_drv_cycle_trigger_depth_process(
	struct isp_cmd_cycle_trigger_depth_process *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_TRIGGER_DEPTH_PROCESS_CTRL;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, param, sizeof(
		struct isp_cmd_cycle_trigger_depth_process));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_cycle_trigger_depth_process);

/**
 *\brief Set led first power on delay time	0x3020
 *\param delay_ms  [In], delay time (mini second)
 *\return Error code
 */
errcode mini_isp_drv_led_active_delay(u32 delay_ms)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_SET_LED_ACTIVE_DELAY;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, &delay_ms, sizeof(u32));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_led_active_delay);


/**
 *\brief Set isp control led level	0x3021
 *\param AL6100 control Led Level  [In], 1: on, 0: off
 *\return Error code
 */
errcode mini_isp_drv_isp_control_led_level(u8 on)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	/* Op code*/
	u16 opcode = ISPCMD_CAMERA_ISPLEDLEVELCONTROLONOFF;
	u8 cmd_param_buf[T_SPI_CMD_LENGTH];

	memset(cmd_param_buf, 0, T_SPI_CMD_LENGTH);
	memcpy(cmd_param_buf, &on, sizeof(u8));

	err = ispctrl_if_mast_execute_cmd(opcode, cmd_param_buf);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_isp_control_led_level);

/**
 *\brief Get isp thermal	0x0115
 *\param None
 *\return Error code
 */
errcode mini_isp_drv_get_chip_thermal(u16 *thermal_val)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u8 recv_buf[T_SPI_CMD_LENGTH];

	/* Op code*/
	u16 opcode = ISPCMD_SYSTEM_GET_CHIP_THERMAL;

	memset(recv_buf, 0, T_SPI_CMD_LENGTH);

	/* recv buffer must use local buffer, can not be global or static. */
	err = ispctrl_if_mast_execute_cmd(opcode, recv_buf);
	misp_info("%s - 0x%x", __func__, *(u16 *)recv_buf);

	memcpy((u8 *)thermal_val, recv_buf, 2);

	return err;
}
EXPORT_SYMBOL(mini_isp_drv_get_chip_thermal);

/******Private Function******/

static int load_code_task(void *data)
{
	/* Error code*/
	errcode err = ERR_SUCCESS;

	misp_info("misp_load_fw start");

	/* Reset mini-isp low for at least 200us, release to high for 20ms*/
	/*mini_isp_reset();*/

#if ENABLE_LINUX_FW_LOADER
	/* Write boot code and basic code*/
	if (!SPI_SHORT_LEN_MODE)
		err = mini_isp_drv_boot_mini_isp();
	/* SPI "Write" NO short Len limitation */
	else if (!SPI_SHORT_LEN_MODE_WRITE_ENABLE)
		err = mini_isp_drv_boot_mini_isp();
	else
		err = mini_isp_drv_boot_mini_isp_shortlen(); /* short SPI Len */
	if (err != ERR_SUCCESS)
		goto load_code_task_end;
#else
	/* Open boot file and FW file*/
	err = mini_isp_drv_open(BOOT_FILE_LOCATION,
				BASIC_FILE_LOCATION,
				ADVANCED_FILE_LOCATION,
				SCENARIO_TABLE_FILE_LOCATION,
				HDR_QMERGE_DATA_FILE_LOCATION,
				IRP0_QMERGE_DATA_FILE_LOCATION,
				IRP1_QMERGE_DATA_FILE_LOCATION,
				PP_MAP_FILE_LOCATION,
				DPETH_QMERGE_DATA_FILE_LOCATION);
	if (err != ERR_SUCCESS)
		goto load_code_task_end;

	/* Write boot code and basic code*/
	if (!SPI_SHORT_LEN_MODE)
		err = mini_isp_drv_boot_mini_isp();
	/* SPI "Write" NO short Len limitation*/
	else if (!SPI_SHORT_LEN_MODE_WRITE_ENABLE)
		err = mini_isp_drv_boot_mini_isp();
	else
		err = mini_isp_drv_boot_mini_isp_shortlen(); /* short SPI Len*/
	if (err != ERR_SUCCESS)
		goto load_code_task_end;
#endif
	/* Set load code ready flag*/
	load_code_ready = true;

load_code_task_end:

	return (int)err;
}

#if (!ENABLE_LINUX_FW_LOADER)
static u16 calibration_check_sum(u8 *input_buffer_addr, u16 input_buffer_size)
{
	u16 i;
	u32 sum = 0;
	u16 sumvalue;

	/* calculating unit is 2 bytes*/
	for (i = 0; i < input_buffer_size; i++) {
		if (0 == (i % 2))
			sum += input_buffer_addr[i];
		else
			sum += (input_buffer_addr[i] << 8);
	}

	/* Do 2's complement*/
	sumvalue = (u16)(65536 - (sum & 0x0000FFFF));  /*get 2's complement*/

	return sumvalue;
}
#endif





/******End Of File******/
