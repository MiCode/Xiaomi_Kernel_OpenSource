/*
 * File: ispctrl_if_master.c
 * Description: ISP Ctrl IF Master
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
/* Linux headers*/
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "include/miniisp.h"
#include "include/ispctrl_if_master.h"
#include "include/isp_camera_cmd.h"
#include "include/error/ispctrl_if_master_err.h"
#include "include/ispctrl_if_master_local.h"
#include "include/miniisp_ctrl.h"
#include "include/miniisp_chip_base_define.h"


/*extern struct misp_data *misp_drv_data;*/

/******Private Constant Definition******/
#define MASTERTX_BLOCK_SIZE SPI_TX_BULK_SIZE
#define MINI_ISP_LOG_TAG "[_mini_ispctrl_if_master]"


/******Private Type Definition******/
struct file *g_filp[FIRMWARE_MAX];
struct spi_device *spictrl;
/******Private Function Prototype******/


static errcode execute_system_manage_cmd(void *devdata, u16 opcode,
					u8 *param);
static errcode execute_basic_setting_cmd(void *devdata, u16 opcode,
					u8 *param);
static errcode execute_bulk_data_cmd(void *devdata, u16 opcode,
				u8 *param);
static errcode execute_camera_profile_cmd(void *devdata, u16 opcode,
					u8 *param);
static errcode execute_operation_cmd(void *devdata, u16 opcode,
					char *param);
static u16 isp_mast_calculate_check_sum(u8 *input_buffer_addr,
					u16 input_buffer_size, bool b2sCom);

/******Private Global Variable******/


/******Private Global Variable******/




/******Public Function******/

/*
 *\brief Execute SPI master command
 *\param opcode [In], Op code
 *\param param [In], CMD param buffer
 *\return Error code
 */
errcode ispctrl_if_mast_execute_cmd(u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	struct misp_global_variable *devdata_global_variable;
	void *devdata;

	misp_info("%s opcode: %#04x >>>",
		__func__, opcode);

	devdata_global_variable = get_mini_isp_global_variable();
	devdata = (void *)get_mini_isp_intf(MINIISP_I2C_TOP);

	if (!devdata)
		return -ENODEV;

	if (!devdata_global_variable->en_cmd_send && opcode != 0x210B) {
		misp_info("%s, disable cmd send!", __func__);
		return err;
	}


	mutex_lock(&devdata_global_variable->busy_lock);

	/* System Management 0x0000~0x0FFF*/
	if (opcode <= 0x0FFF)
		err = execute_system_manage_cmd(devdata, opcode, param);
	/* Basic Setting 0x1000~0x1FFF*/
	else if (opcode <= 0x1FFF)
		err = execute_basic_setting_cmd(devdata, opcode, param);
	/* Bulk Data 0x2000~0x2FFF*/
	else if (opcode <= 0x2FFF)
		err = execute_bulk_data_cmd(devdata, opcode, param);
	/* Camera Profile 0x3000~0x3FFF*/
	else if (opcode <= 0x3FFF)
		err = execute_camera_profile_cmd(devdata, opcode, param);
	/*Operation 0x4000~0x4FFF*/
	else if (opcode <= 0x4FFF)
		err = execute_operation_cmd(devdata, opcode, param);
	else
		err = ERR_MASTER_OPCODE_UNSUPPORT;

	/* delay 2ms */
	msleep(20);

	mutex_unlock(&devdata_global_variable->busy_lock);

	misp_info("%s opcode: %#04x <<<",
		__func__, opcode);

	return err;
}
EXPORT_SYMBOL(ispctrl_if_mast_execute_cmd);

/*
 *\brief Send command to slave
 *\param devdata [In], misp_data
 *\param opcode [In], Op code
 *\param param [In], CMD param buffer
 *\param len [In], CMD param size
 *\return Error code
 */
errcode ispctrl_mast_send_cmd_to_slave(void *devdata,
					u16 opcode, u8 *param, u32 len)
{
	u16 *send_len, *send_opcode, total_len;
	errcode err = ERR_SUCCESS;
	u16 chksum;
	u8 *tx_buf;
	struct misp_global_variable *devdata_global_variable;

	devdata_global_variable = get_mini_isp_global_variable();
	tx_buf = ((struct misp_data *)devdata)->tx_buf;

	memset(tx_buf, 0, TX_BUF_SIZE);

	send_len = (u16 *)&tx_buf[0];
	send_opcode = (u16 *)&tx_buf[ISPCMD_LENFLDBYTES];

	/*[2-byte len field] + [2-byte opcode field] + (params len)*/
	total_len = ISPCMD_CMDSIZE + len;
	/* totoal - 2-byte length field*/
	*send_len = total_len - ISPCMD_LENFLDBYTES;

	*send_opcode = opcode;
	if (len > 0)
		memcpy(&tx_buf[ISPCMD_CMDSIZE], param, len);

	/*calculate checksum*/
	chksum = isp_mast_calculate_check_sum(tx_buf, total_len, true);
	memcpy(&tx_buf[total_len], &chksum, ISPCMD_CKSUMBYTES);

	/*add bytes for checksum*/
	total_len += ISPCMD_CKSUMBYTES;

	/**
	 * tx_buf:
	 * |--len_field(2)--|
	 *		 |--opcode(2)--|--param(len)--|
	 *					       |--cksum(2)--|
	 * len_field size: ISPCMD_LENFLDBYTES (2 bytes)
	 * opcode size: ISPCMD_OPCODEBYTES (2 bytes)
	 * param size: (len bytes)
	 * ISP_CMD_HDR_SIZE = ISPCMD_LENFLDBYTES + ISPCMD_OPCODEBYTES (4 bytes)
	 *
	 * total_len: (len_field_size + opcode_size + param_size + cksum_size)
	 * len(param_len) = (opcode_size + param_size), excluding cksum
	 */

	/* Send command to slave*/
	err = ((struct misp_data *)devdata)->intf_fn->send(devdata, total_len);


	return err;
}

/*
 *\brief Receive response from slave
 *\param devdata [In], misp_data
 *\param param [Out], Respons e buffer
 *\param len [Out], Response size
 *\return Error code
 */
errcode ispctrl_mast_recv_response_from_slave(void *devdata,
				u8 *param, u32 len, bool wait_int)
{
	errcode err = ERR_SUCCESS;
	u32 total_len;
	u16  org_chk_sum;
	u8 *rx_buf;
	struct misp_global_variable *devdata_global_variable;

	devdata_global_variable = get_mini_isp_global_variable();
	rx_buf = ((struct misp_data *)devdata)->rx_buf;

	memset(rx_buf, 0, RX_BUF_SIZE);
	total_len = len + ISPCMD_CMDSIZEWDUMMY + ISPCMD_CKSUMBYTES;

	/*Receive command from slave*/
	err = ((struct misp_data *)devdata)->intf_fn->
		recv(devdata, total_len, wait_int);
	if (err != ERR_SUCCESS)
		goto ispctrl_mast_recv_response_from_slave_end;


	/*checksum*/
	memcpy(&org_chk_sum, &rx_buf[(total_len - ISPCMD_CKSUMBYTES)],
		ISPCMD_CKSUMBYTES);
	if (org_chk_sum != isp_mast_calculate_check_sum(rx_buf,
				(total_len - ISPCMD_CKSUMBYTES), true)) {
		misp_err("%s - checksum error", __func__);
		err = ERR_MASTERCMDCKSM_INVALID;
		goto ispctrl_mast_recv_response_from_slave_end;
	}


	/* Copy param data*/
	memcpy(param, &rx_buf[ISPCMD_CMDSIZEWDUMMY], len);

ispctrl_mast_recv_response_from_slave_end:

	return err;

}

/*
 *\brief Receive Memory data from slave
 *\param devdata [In], misp_data
 *\param response_buf [Out], Response buffer
 *\param response_size [Out], Response size
 *\param wait_int [In], waiting INT flag
 *\return Error code
 */
errcode ispctrl_if_mast_recv_memory_data_from_slave(
						void *devdata,
						u8 *response_buf,
						u32 *response_size,
						u32 block_size,
						bool wait_int)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 expect_size;
	u32 altek_event_state;
	u16 i;

	expect_size = *response_size;

	misp_info("%s - Start.", __func__);

	if (wait_int) {
		err = mini_isp_wait_for_event(MINI_ISP_RCV_CMD_READY);
		if (err) {
			misp_err("%s - irq error: status=%d",
				__func__, err);
			goto ispctrl_if_mast_recv_memory_data_from_slave_end;
		}
	} else {
		for (i = 0; i < 200; i++) {
			err = mini_isp_get_altek_status(devdata,
						&altek_event_state);
			if (altek_event_state & COMMAND_COMPLETE) {
				altek_event_state = (altek_event_state &
					~((~0) << 1));
				break;
			}
			/* delay 5ms */
			msleep(20);
		}
		if (i >= 200) {
			misp_err("%s time out.", __func__);
			err = ERR_MASTER_EVENT_TIMEOUT;
			goto ispctrl_if_mast_recv_memory_data_from_slave_end;
		}

	}

	err = mini_isp_get_bulk((struct misp_data *)devdata,
			response_buf, expect_size, block_size);

ispctrl_if_mast_recv_memory_data_from_slave_end:

	return err;
}

#if ENABLE_LINUX_FW_LOADER
/*
 * \brief  Master send bulk (large data) to slave
 *   \param  devdata [In], misp_data
 *   \param  buffer [In], Data buffer to be sent, address 8-byte alignment
 *   \param  filp [In], file pointer, used to read the file and send the data
 *   \param  total_size [In], file size
 *   \param  block_size [In], transfer buffer block size
 *   \param  is_raw [In], true: mini boot code  false: other files
 *   \return Error code
 */
errcode ispctrl_if_mast_send_bulk(void *devdata, const u8 *buffer,
	u32 total_size, u32 block_size, bool is_raw)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;

	if (buffer == NULL)
		return -ENOENT;

	misp_info("ispctrl_if_master_send_bulk Start ============");
	/* Transfer basic code*/
	err = ((struct misp_data *)devdata)->intf_fn->send_bulk(devdata,
			total_size, block_size, is_raw, buffer);

	if (err != ERR_SUCCESS)
		misp_err("ispctrl_if_master_send_bulk failed!!");

	misp_info("ispctrl_if_master_send_bulk End ============");
	return err;

}

#else
/*
 * \brief  Master send bulk (large data) to slave
 *   \param  devdata [In], misp_data
 *   \param  buffer [In], Data buffer to be sent, address 8-byte alignment
 *   \param  filp [In], file pointer, used to read the file and send the data
 *   \param  total_size [In], file size
 *   \param  block_size [In], transfer buffer block size
 *   \param  is_raw [In], true: mini boot code  false: other files
 *   \return Error code
 */
errcode ispctrl_if_mast_send_bulk(void *devdata, u8 *buffer,
	struct file *filp, u32 total_size, u32 block_size, bool is_raw)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;


	if ((!filp) && (buffer == NULL))
		return -ENOENT;

	misp_info("ispctrl_if_master_send_bulk Start ============");
	/* Transfer basic code*/
	err = ((struct misp_data *)devdata)->intf_fn->send_bulk(devdata,
			filp, total_size, block_size, is_raw, buffer);

	if (err != ERR_SUCCESS)
		misp_err("ispctrl_if_master_send_bulk failed!!");
	misp_info("ispctrl_if_master_send_bulk End ============");


	/*close the file*/
	if (filp && (buffer == NULL))
		filp_close(filp, NULL);
	return err;

}
#endif


/* open boot / basic / advanced / sc table file*/
errcode ispctrl_if_mast_request_firmware(u8 *filepath, u8 firmwaretype)
{
	errcode err = ERR_SUCCESS;

	misp_info("%s filepath : %s", __func__, filepath);
#if ENABLE_FILP_OPEN_API
	/* use file open */
#else
	misp_info("Error! Currently not support file open api");
	misp_info("See define ENABLE_FILP_OPEN_API");
	return err;
#endif
	if (IS_ERR(g_filp[firmwaretype])) {
		err = PTR_ERR(g_filp[firmwaretype]);
		misp_err("%s open file failed. err: %x", __func__, err);
	} else {
		misp_info("%s open file success!", __func__);
	}

	return err;
}


/* current no use
u16 ispctrl_if_mast_read_spi_status(void)
{
	return mini_isp_get_status();
}
*/

/*
 *\brief Receive ISP register response from slave
 *\param devdata [In], misp_data
 *\param response_buf [Out], Response buffer
 *\param response_size [Out], Response size
 *\param total_count [In], Total reg count
 *\return Error code
 */
errcode ispctrl_if_mast_recv_isp_register_response_from_slave(
		void *devdata, u8 *response_buf,
		u32 *response_size, u32 total_count)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 total_len;
	u16 org_chk_sum;

	/* Last packet flag*/
	bool last_packet = false;
	/* Get Reg count*/
	u8 reg_count = 0;
	/* Total count*/
	u32 use_total_count = total_count;
	/* Total return size*/
	u32 total_ret_size = 0;
	/* Max count*/
	u8 max_reg_count = 12;
	/* Checksum*/
	u16 check_sum = 0;
	bool wait_int = true;
	u8 *rx_buf;
	struct misp_global_variable *devdata_global_variable;

	devdata_global_variable = get_mini_isp_global_variable();
	rx_buf = ((struct misp_data *)devdata)->rx_buf;

	/* Update expect size*/
	total_len = ISPCMD_CMDSIZEWDUMMY + ISP_REGISTER_PARA_SIZE;

	/* Multi SPI Tx transfer*/
	/*if One SPI Rx recv*/
	while (last_packet == false) {
		/* One SPI Rx recv*/
		if (use_total_count <= max_reg_count) {
			/* Set reg count*/
			reg_count = use_total_count;
			/* Set last packet*/
			last_packet = true;
		} else {
			/*Multi SPI Rx recv*/
			reg_count = max_reg_count;
		}
		/* Update expect size*/
		total_len += reg_count*ISP_REGISTER_VALUE;

		/* Add bytes for checksum*/
		if (last_packet == true)
			total_len += ISPCMD_CKSUMBYTES;


		/* Receive command from slave*/
		err = ((struct misp_data *)devdata)->intf_fn->recv(
				devdata, total_len, wait_int);
		if (err != ERR_SUCCESS)
			break;

		/* Last packet*/
		if (last_packet == true) {
			/* Get checksum*/
			memcpy(&org_chk_sum,
			    &rx_buf[(total_len - ISPCMD_CKSUMBYTES)],
			    ISPCMD_CKSUMBYTES);

			/* Count checksum*/
			check_sum += isp_mast_calculate_check_sum(
				&rx_buf[total_ret_size],
				total_len - ISPCMD_CKSUMBYTES, false);
			/* Do 2's complement*/
			check_sum = 65536 - check_sum;

			/* Checksum error*/
			if (org_chk_sum != check_sum)	{
				/* Set error code*/
				err = ERR_MASTERCMDCKSM_INVALID;
				break;
			}
		} else {
			/* Normal packet*/
			/* checksum is valid or not*/
			check_sum += isp_mast_calculate_check_sum(
				&rx_buf[total_ret_size], total_len,
				false);
		}

		/* Update total count*/
		use_total_count -= reg_count;
		/* Update total ret size*/
		total_ret_size += total_len;

		/* Reset expect size*/
		total_len = 0;
		/* Update max reg count*/
		max_reg_count = 16;

	}


	#ifdef OUTPUT_ISP_REGISTER
	/*write out the buffer to .arw file here*/
	#endif


	return err;

}

/******Private Function******/


/**
 *\brief Execute system manage command
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
static errcode execute_system_manage_cmd(void *devdata, u16 opcode,
						u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;

	/* Get status of Last Executed Command*/
	if (opcode == ISPCMD_SYSTEM_GET_STATUSOFLASTEXECUTEDCOMMAND)
		err = mast_sys_manage_cmd_get_status_of_last_exec_command(
						devdata, opcode, param);
	else if (opcode == ISPCMD_SYSTEM_GET_ERRORCODE)
		err = mast_sys_manage_cmd_get_error_code_command(devdata,
							opcode, param);
	/* Set ISP register*/
	else if (opcode == ISPCMD_SYSTEM_SET_ISPREGISTER)
		err = mast_sys_manage_cmd_set_isp_register(devdata,
							opcode,	param);
	/* Get ISP register*/
	else if (opcode == ISPCMD_SYSTEM_GET_ISPREGISTER)
		err = mast_sys_manage_cmd_get_isp_register(devdata,
							opcode,	param);
	/* Set common log level*/
	else if (opcode == ISPCMD_SYSTEM_SET_COMLOGLEVEL)
		err = mast_sys_manage_cmd_set_comomn_log_level(devdata,
							opcode,	param);
	/*Get chip test report*/
	else if (opcode == ISPCMD_SYSTEM_GET_CHIPTESTREPORT)
		err = mast_sys_manage_cmd_get_chip_test_report(devdata,
							opcode, param);
	/*Get chip thermal*/
	else if (opcode == ISPCMD_SYSTEM_GET_CHIP_THERMAL)
		err = mast_sys_manage_cmd_get_chip_thermal(devdata,
							opcode, param);
	return err;

}

/**
 *\brief Execute basic setting command
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
static errcode execute_basic_setting_cmd(void *devdata, u16 opcode,
					u8 *param)
{
	/*Error Code*/
	errcode err = ERR_SUCCESS;


	/* Set Depth 3A info*/
	if (opcode == ISPCMD_BASIC_SET_DEPTH_3A_INFO)
		err = mast_basic_setting_cmd_set_depth_3a_info(devdata,
								opcode, param);
	/* Set Depth Auto interleave mode*/
	else if (opcode == ISPCMD_BASIC_SET_DEPTH_AUTO_INTERLEAVE_MODE)
		err = mast_basic_setting_cmd_set_depth_auto_interleave_mode(
			devdata, opcode, param);
	/* Set Depth Auto interleave mode*/
	else if (opcode == ISPCMD_BASIC_SET_INTERLEAVE_MODE_DEPTH_TYPE)
		err = mast_basic_setting_cmd_set_interleave_mode_depth_type(
			devdata, opcode, param);
	/* Set Depth polish level*/
	else if (opcode == ISPCMD_BASIC_SET_DEPTH_POLISH_LEVEL)
		err = mast_basic_setting_cmd_set_depth_polish_level(
			devdata, opcode, param);
	/* Set exposure param*/
	else if (opcode == ISPCMD_BASIC_SET_EXPOSURE_PARAM)
		err = mast_basic_setting_cmd_set_exposure_param(
			devdata, opcode, param);
	/* Set exposure param*/
	else if (opcode == ISPCMD_BASIC_SET_DEPTH_STREAM_SIZE)
		err = mast_basic_setting_cmd_set_depth_stream_size(
			devdata, opcode, param);

	return err;

}
#if ENABLE_LINUX_FW_LOADER
/**
 *\brief Execute bulk data command
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
static errcode execute_bulk_data_cmd(void *devdata,
				u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 *block_size;
	char filename[ISPCMD_FILENAME_SIZE];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	memcpy(filename, param, ISPCMD_FILENAME_SIZE);


	misp_info("%s Start", __func__);

	/*set the size*/
	block_size = (u32 *)&param[8];

	if (opcode == ISPCMD_BULK_WRITE_BOOTCODE) {
		/* Write Boot Code*/
		misp_info("%s : write BOOT_CODE", __func__);

		dev_global_variable->before_booting = 1;
		*block_size = MASTERTX_BLOCK_SIZE;

#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_boot_code(devdata, param);
#endif
		dev_global_variable->before_booting = 0;
	} else if (opcode == ISPCMD_BULK_WRITE_BOOTCODE_SHORT_LEN) {
		/* Write Boot Code (short SPI len)*/
		misp_info("%s : write BOOT_CODE", __func__);
		dev_global_variable->before_booting = 1;
		*block_size = MASTERTX_BLOCK_SIZE;
#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_boot_code_shortlen(
				devdata, param);
#endif
		dev_global_variable->before_booting = 0;
	} else if (opcode == ISPCMD_BULK_WRITE_BASICCODE) {
		/* Write Basic Code*/
		misp_info("%s : write BASIC_CODE", __func__);

#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_basic_code(
			devdata, param);
#endif
	} else if (opcode == ISPCMD_BULK_WRITE_BASICCODE_CODESUM) {
		/* Write Basic Code (short SPI len)*/
		misp_info("%s : write BASIC_CODE", __func__);
#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_basic_code_shortlen(
			devdata, param);
#endif

	} else if (opcode == ISPCMD_BULK_WRITE_CALIBRATION_DATA) {
		/*Write Calibration Data*/
		if (param[8] == 0) {
			misp_info("%s : write IQ_CALIBRATION_DATA", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 1) {
			misp_info("%s : write DEPTH_CALIBRATION_DATA",
				__func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 2) {
			misp_info("%s : write SCENARIO_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 3) {
			misp_info("%s : write HDR_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 4) {
			misp_info("%s : write IRP0_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 5) {
			misp_info("%s : write IRP1_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 6) {
			misp_info("%s : write PPMAP_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 7) {
			misp_info("%s : write BLENDING_TABLE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 8) {
			misp_info("%s : write DEPTH_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		} else if (param[8] == 9) {
			misp_info("%s : write OTP DATA", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param);
		}
	}
	/* Read Calibration Data*/
	else if (opcode == ISPCMD_BULK_READ_CALIBRATION_DATA)
		err = mast_bulk_data_cmd_read_calibration_data(devdata, param);

	/* Read memory Data*/
	else if (opcode == ISPCMD_BULK_READ_MEMORY)
		err = mast_bulk_data_cmd_read_memory_data(devdata, param);
	/* Read Common Log Data*/
	else if (opcode == ISPCMD_BULK_READ_COMLOG)
		err = bulk_data_cmd_read_common_log(devdata, param);
	/* Read Depth RectA, B, Invrect*/
	else if (opcode == ISPCMD_BULK_WRITE_DEPTH_RECTAB_INVRECT) {
		err = mast_bulk_data_cmd_write_depth_rectab_invrect(
			devdata, opcode, param);
	} else if (opcode == ISPCMD_BULK_WRITE_SPINOR_DATA) {
		/*Write SPINOR Data*/
		if (param[8] == 0) {
			misp_info("%s : write SPINOR BOOT DATA ", __func__);
			err = mast_bulk_data_cmd_write_spinor_data(
					devdata, param);
		} else if (param[8] == 1) {
			misp_info("%s : write SPINOR MAIN DATA", __func__);
			err = mast_bulk_data_cmd_write_spinor_data(
					devdata, param);
		}
	}
	misp_info("execute_bulk_data_cmd - end Errcode: %d ======",
		(int)err);

	return err;

}

#else
/**
 *\brief Execute bulk data command
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
static errcode execute_bulk_data_cmd(void *devdata,
				u16 opcode, u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;
	u32 *block_size;
	char filename[ISPCMD_FILENAME_SIZE];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	memcpy(filename, param, ISPCMD_FILENAME_SIZE);


	misp_info("%s Start", __func__);

	/*set the size*/
	block_size = (u32 *)&param[8];

	if (opcode == ISPCMD_BULK_WRITE_BOOTCODE) {
		/* Write Boot Code*/
		misp_info("%s : write BOOT_CODE", __func__);

		dev_global_variable->before_booting = 1;
		*block_size = MASTERTX_BLOCK_SIZE;

#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_boot_code(devdata, param,
				g_filp[BOOT_CODE]);

		if (g_filp[BOOT_CODE] != NULL) {
			filp_close(g_filp[BOOT_CODE], NULL);
			g_filp[BOOT_CODE] = NULL;
		}
#endif

		dev_global_variable->before_booting = 0;
	} else if (opcode == ISPCMD_BULK_WRITE_BOOTCODE_SHORT_LEN) {
		/* Write Boot Code (short SPI len)*/
		misp_info("%s : write BOOT_CODE", __func__);
		dev_global_variable->before_booting = 1;
		*block_size = MASTERTX_BLOCK_SIZE;
#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_boot_code_shortlen(
			devdata, param,	g_filp[BOOT_CODE]);

		if (g_filp[BOOT_CODE] != NULL) {
			filp_close(g_filp[BOOT_CODE], NULL);
			g_filp[BOOT_CODE] = NULL;
		}
#endif
		dev_global_variable->before_booting = 0;
	} else if (opcode == ISPCMD_BULK_WRITE_BASICCODE) {
		/* Write Basic Code*/
		misp_info("%s : write BASIC_CODE", __func__);

#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_basic_code(devdata, param,
				g_filp[BASIC_CODE]);

		if (g_filp[BASIC_CODE] != NULL) {
			filp_close(g_filp[BASIC_CODE], NULL);
			g_filp[BASIC_CODE] = NULL;
		}
#endif
	} else if (opcode == ISPCMD_BULK_WRITE_BASICCODE_CODESUM) {
		/* Write Basic Code (short SPI len)*/
		misp_info("%s : write BASIC_CODE", __func__);
#ifndef AL6100_SPI_NOR
		err = mast_bulk_data_cmd_write_basic_code_shortlen(
			devdata, param,	g_filp[BASIC_CODE]);

		if (g_filp[BASIC_CODE] != NULL) {
			filp_close(g_filp[BASIC_CODE], NULL);
			g_filp[BASIC_CODE] = NULL;
		}
#endif

	} else if (opcode == ISPCMD_BULK_WRITE_CALIBRATION_DATA) {
		/*Write Calibration Data*/
		if (param[8] == 0) {
			misp_info("%s : write IQ_CALIBRATION_DATA", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, NULL);
		} else if (param[8] == 1) {
			misp_info("%s : write DEPTH_CALIBRATION_DATA",
				__func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, NULL);
		} else if (param[8] == 2) {
			misp_info("%s : write SCENARIO_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, g_filp[SCENARIO_CODE]);

			if (g_filp[SCENARIO_CODE] != NULL) {
				filp_close(g_filp[SCENARIO_CODE], NULL);
				g_filp[SCENARIO_CODE] = NULL;
			}

		} else if (param[8] == 3) {
			misp_info("%s : write HDR_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, g_filp[HDR_CODE]);

			if (g_filp[HDR_CODE] != NULL) {
				filp_close(g_filp[HDR_CODE], NULL);
				g_filp[HDR_CODE] = NULL;
			}
		} else if (param[8] == 4) {
			misp_info("%s : write IRP0_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, g_filp[IRP0_CODE]);

			if (g_filp[IRP0_CODE] != NULL) {
				filp_close(g_filp[IRP0_CODE], NULL);
				g_filp[IRP0_CODE] = NULL;
			}
		} else if (param[8] == 5) {
			misp_info("%s : write IRP1_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, g_filp[IRP1_CODE]);

			if (g_filp[IRP1_CODE] != NULL) {
				filp_close(g_filp[IRP1_CODE], NULL);
				g_filp[IRP1_CODE] = NULL;
			}
		} else if (param[8] == 6) {
			misp_info("%s : write PPMAP_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, g_filp[PPMAP_CODE]);

			if (g_filp[PPMAP_CODE] != NULL) {
				filp_close(g_filp[PPMAP_CODE], NULL);
				g_filp[PPMAP_CODE] = NULL;
			}
		} else if (param[8] == 7) {
			misp_info("%s : write BLENDING_TABLE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
							devdata, param, NULL);
		} else if (param[8] == 8) {
			misp_info("%s : write DEPTH_CODE", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
					devdata, param, g_filp[DEPTH_CODE]);

			if (g_filp[DEPTH_CODE] != NULL) {
				filp_close(g_filp[DEPTH_CODE], NULL);
				g_filp[DEPTH_CODE] = NULL;
			}
		} else if (param[8] == 9) {
			misp_info("%s : write OTP DATA", __func__);
			err = mast_bulk_data_cmd_write_calibration_data(
							devdata, param, NULL);
		}
	}
	/* Read Calibration Data*/
	else if (opcode == ISPCMD_BULK_READ_CALIBRATION_DATA)
		err = mast_bulk_data_cmd_read_calibration_data(devdata, param);
	/* Read memory Data*/
	else if (opcode == ISPCMD_BULK_READ_MEMORY)
		err = mast_bulk_data_cmd_read_memory_data(devdata, param);
	/* Read Common Log Data*/
	else if (opcode == ISPCMD_BULK_READ_COMLOG)
		err = bulk_data_cmd_read_common_log(devdata, param);
	/* Read Depth RectA, B, Invrect*/
	else if (opcode == ISPCMD_BULK_WRITE_DEPTH_RECTAB_INVRECT) {
		err = mast_bulk_data_cmd_write_depth_rectab_invrect(
			devdata, opcode, param);
	} else if (opcode == ISPCMD_BULK_WRITE_SPINOR_DATA) {
		/*Write SPINOR Data*/
		if (param[8] == 0) {
			misp_info("%s : write SPINOR BOOT DATA ", __func__);
			err = mast_bulk_data_cmd_write_spinor_data(
					devdata, param, g_filp[BOOT_CODE]);
		} else if (param[8] == 1) {
			misp_info("%s : write SPINOR MAIN DATA", __func__);
			err = mast_bulk_data_cmd_write_spinor_data(
					devdata, param, g_filp[BASIC_CODE]);
		}
	}
	misp_info("execute_bulk_data_cmd - end Errcode: %d ======", (int)err);

	return err;

}
#endif
/**
 *\brief Execute camera profile command
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
static errcode execute_camera_profile_cmd(void *devdata, u16 opcode,
					u8 *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;

	/* Set Sensor Mode*/
	if (opcode == ISPCMD_CAMERA_SET_SENSORMODE)
		err = mast_camera_profile_cmd_set_sensor_mode(devdata,
							opcode, param);
	/* Get Sensor Mode*/
	else if (opcode == ISPCMD_CAMERA_GET_SENSORMODE)
		err = mast_camera_profile_cmd_get_sensor_mode(devdata,
							opcode, param);
	/*Set Output format*/
	else if (opcode == ISPCMD_CAMERA_SET_OUTPUTFORMAT)
		err = mast_camera_profile_cmd_set_output_format(devdata,
							opcode, param);
	/*Set CP Mode*/
	else if (opcode == ISPCMD_CAMERA_SET_CP_MODE)
		err = mast_camera_profile_cmd_set_cp_mode(devdata,
							opcode, param);
	/* Preview stream on*/
	else if (opcode == ISPCMD_CAMERA_PREVIEWSTREAMONOFF)
		err = mast_camera_profile_cmd_preview_stream_on_off(
						devdata, opcode, param);
	/* Set AE statistic*/
	else if (opcode == ISPCMD_CAMERA_SET_AE_STATISTICS)
		err = mast_basic_setting_cmd_set_ae_statistics(
						devdata, opcode, param);
	/* Dual PD Y calculation weightings*/
	else if (opcode == ISPCMD_CAMERA_DUALPDYCALCULATIONWEIGHT)
		err = mast_camera_profile_cmd_dual_pd_y_cauculation_weightings(
						devdata, opcode, param);
	/* LED power control*/
	else if (opcode == ISPCMD_LED_POWERCONTROL)
		err = mast_camera_profile_cmd_led_power_control(
						devdata, opcode, param);
	/* Active AE*/
	else if (opcode == ISPCMD_CAMERA_ACTIVE_AE)
		err = mast_camera_profile_cmd_active_ae(
						devdata, opcode, param);
	/* ISP AE Control*/
	else if (opcode == ISPCMD_ISP_AECONTROLONOFF)
		err = mast_camera_profile_cmd_isp_ae_control_on_off(
						devdata, opcode, param);
	/* Set Frame Rate Limits*/
	else if (opcode == ISPCMD_CAMERA_SET_FRAMERATELIMITS)
		err = mast_camera_profile_cmd_set_frame_rate_limits(
						devdata, opcode, param);
	/* Set Period Drop Frame*/
	else if (opcode == ISPCMD_CAMERA_SET_PERIODDROPFRAME)
		err = mast_camera_profile_cmd_set_period_drop_frame(
						devdata, opcode, param);
	/* Set Max exposure*/
	else if (opcode == ISPCMD_CAMERA_SET_MAX_EXPOSURE)
		err = mast_camera_profile_cmd_set_max_exposure(
						devdata, opcode, param);
	/* Set AE Target Mean*/
	else if (opcode == ISPCMD_CAMERA_SET_AE_TARGET_MEAN)
		err = mast_camera_profile_cmd_set_target_mean(
						devdata, opcode, param);
	/* Frame Sync Control*/
	else if (opcode == ISPCMD_CAMERA_FRAME_SYNC_CONTROL)
		err = mast_camera_profile_cmd_frame_sync_control(
						devdata, opcode, param);
	/* Set Shot Mode */
	else if (opcode == ISPCMD_CAMERA_SET_SHOT_MODE)
		err = mast_camera_profile_cmd_set_shot_mode(
						devdata, opcode, param);
	/* Lighting Control*/
	else if (opcode == ISPCMD_CAMERA_LIGHTING_CTRL)
		err = mast_camera_profile_cmd_lighting_ctrl(
						devdata, opcode, param);
	/* Set Depth shift */
	else if (opcode == ISPCMD_CAMERA_DEPTH_COMPENSATION)
		err = mast_camera_profile_cmd_set_depth_compensation(
						devdata, opcode, param);
	/* set cycle trigger depth process */
	else if (opcode == ISPCMD_CAMERA_TRIGGER_DEPTH_PROCESS_CTRL)
		err = mast_camera_profile_cmd_set_cycle_trigger_depth(
						devdata, opcode, param);
	/* Set Min exposure*/
	else if (opcode == ISPCMD_CAMERA_SET_MIN_EXPOSURE)
		err = mast_camera_profile_cmd_set_min_exposure(
						devdata, opcode, param);
	/* Set Max exposure slope*/
	else if (opcode == ISPCMD_CAMERA_SET_MAX_EXPOSURE_SLOPE)
		err = mast_camera_profile_cmd_set_max_exposure_slope(
						devdata, opcode, param);
	/* Set led active delay time */
	else if (opcode == ISPCMD_CAMERA_SET_LED_ACTIVE_DELAY)
		err = mast_camera_profile_cmd_set_led_active_delay(
						devdata, opcode, param);
	/* Set ISP control led level */
	else if (opcode == ISPCMD_CAMERA_ISPLEDLEVELCONTROLONOFF)
		err = mast_camera_profile_cmd_isp_control_led_level(
						devdata, opcode, param);
	else
		misp_err("%s : unknown opcode: %#04x", __func__, opcode);
	return err;

}


/**
 *\brief Execute operation command
 *\param devdata [In], misp_data
 *\param opcode [In], Operation code
 *\param param [In], CMD param
 *\return Error code
 */
static errcode execute_operation_cmd(void *devdata,
				u16 opcode, char *param)
{
	/* Error Code*/
	errcode err = ERR_SUCCESS;

	/* mini_isp_drv_open*/
	if (opcode == ISPCMD_MINIISPOPEN)
		err = mast_operation_cmd_miniisp_open(devdata,
						opcode, (char **)param);

	return err;
}


static u16 isp_mast_calculate_check_sum(u8 *input_buffer_addr,
					u16 input_buffer_size, bool b2sCom)
{
	u16 i;
	u32 sum = 0;
	u16 sumvalue;

	for (i = 0; i < input_buffer_size; i++) {
		if (0 == (i%2))
			sum += input_buffer_addr[i];
		else
			sum += (input_buffer_addr[i] << 8);
	}

	/* Do 2's complement*/
	if (b2sCom == true)
		sumvalue = (u16) (65536 - (sum & 0x0000FFFF));
	/* Update total sum*/
	else
		sumvalue = sum;

	return sumvalue;
}


/******End Of File******/
