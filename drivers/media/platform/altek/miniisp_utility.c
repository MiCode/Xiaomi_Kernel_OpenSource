/*
 * File: miniisp_utility.c
 * Description: Mini ISP Utility sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *  2017/03/29; Louis Wang; Initial version
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
#include "linux/init.h"
#include "linux/module.h"
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/timekeeping.h>
#include <linux/wait.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef NEW_CLOCK_HEADER
#include <linux/sched/clock.h>
#endif

#include "include/miniisp.h"
#include "include/error.h"
#include "include/miniisp_chip_base_define.h"
#include "include/miniisp_customer_define.h"


#define MINI_ISP_LOG_TAG	"[miniisp_utility]"

#define DEBUG_INFO_DEPTH_FLOW_INFO_ADDR (0x28)
#define DEBUG_INFO_METADATA_ADDR        (0x14)
#define DEBUG_INFO_FEC_DEBUG_MODE_ADDR  (0x40)
#define DEBUG_INFO_SENSOR_REAL_INFO_ADDR (0x44)
#define DEBUG_INFO_LED_INFO_ADDR        (0x48)

#define ADV_LOG_LIGHTING_PROJECTOR  (0x5)
#define ADV_LOG_LIGHTING_FLOOD      (0x6)

#define PACKDATA_SIZE (1824)
#define PACKDATA_OFFSET (0x4C)

#define LOG_BUF_SIZE (200*1024)

GPIO g_atGPIO[] = {

	{ "GPIO0", 0xFFEA0200, "NoUse" },
	{ "GPIO1", 0xFFEA0204, "NoUse" },
	{ "GPIO2", 0xFFEA0208, "NoUse" },
	{ "GPIO3", 0xFFEA020c, "NoUse" },
	{ "GPIO4", 0xFFEA0210, "NoUse" },
	{ "GPIO5", 0xFFEA0214, "NoUse" },
	{ "GPIO6", 0xFFEA0218, "NoUse" },
	{ "GPIO7", 0xFFEA021C, "NoUse" },
	{ "GPIO8", 0xFFEA0220, "NoUse" },
	{ "GPIO9", 0xFFEA0224, "NoUse" },
	{ "GPIO10", 0xFFEA0228, "NoUse" }
};
#define GPIO_NUMBER		(sizeof(g_atGPIO)/sizeof(GPIO))

 /******Private Global Variable******/


 /******Public Function******/
/**
 *\brief dump AL6100 register for debug bypass mode
 *\return Error code
*/
errcode mini_isp_utility_read_reg_e_mode_for_bypass_use(void)
{
	errcode err = ERR_SUCCESS;
	u8 filepath[80];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	/* switch to E mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	snprintf(filepath, 80, "%s/dump_bypass_reg/",
		MINIISP_INFO_DUMPLOCATION);
	err = mini_isp_create_directory(filepath);
	err = mini_isp_chip_base_dump_bypass_mode_register(filepath);

	/* switch to A mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return err;
}
EXPORT_SYMBOL(mini_isp_utility_read_reg_e_mode_for_bypass_use);


/**
 *\brief dump AL6100 register for debug
 *\return Error code
*/
errcode mini_isp_utility_read_reg_e_mode(void)
{
	errcode err = ERR_SUCCESS;
	u8 filepath[80];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	/* switch to E mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	snprintf(filepath, 80, "%s/dump_normal_reg/",
		MINIISP_INFO_DUMPLOCATION);
	err = mini_isp_create_directory(filepath);
	err = mini_isp_chip_base_dump_normal_mode_register(filepath);

	/* switch to A mode */
	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return err;
}
EXPORT_SYMBOL(mini_isp_utility_read_reg_e_mode);

/**
 *\brief Read irp and depth image based information
 *\return Error code
*/
errcode mini_isp_utility_get_irp_and_depth_information(
	struct irp_and_depth_information *info)
{
	errcode err = ERR_SUCCESS;
	u32 rg_img_in_size;
	u32 rg_depth_in_size;
	u32 crop_src;
	u8 fov_mode;

	mini_isp_register_read(0xfff8401c, &rg_img_in_size);
	info->irp_width = ((rg_img_in_size & 0x00001fff)+1);
	info->irp_height = (((rg_img_in_size & 0x1fff0000)>>16)+1);

	mini_isp_register_read(0xfffa7020, &crop_src);

	if (crop_src)
		info->irp_format = 1;
	else
		info->irp_format = 0;

	mini_isp_register_read(0xfff5601c, &rg_depth_in_size);
	info->depth_width = ((rg_depth_in_size & 0x00001fff)+1);
	info->depth_height = (((rg_depth_in_size & 0x1fff0000)>>16)+1);

	info->depth_image_address = 0x20715400;

	mini_isp_memory_read(0x24, &fov_mode, 1);

	info->fov_mode = fov_mode;
	if (fov_mode == 1)
		info->irp_image_address = 0x20500000 + 288000 -
			((rg_img_in_size & 0x00001fff) + 1) *
			(((rg_img_in_size & 0x1fff0000)>>16) + 1);
	else
		info->irp_image_address = 0x20500000;
	misp_info("%s:depth_image_address:%u, depth_width:%u, depth_height:%u, \
		irp_format:%u, irp_image_address:%u, irp_width:%u, \
		irp_height:%u, fov_mode:%u\n", __func__,
		info->depth_image_address, info->depth_width,
		info->depth_height,	info->irp_format,
		info->irp_image_address, info->irp_width,
		info->irp_height, info->fov_mode);
	return err;
}
EXPORT_SYMBOL(mini_isp_utility_get_irp_and_depth_information);

/**
 *\brief dump compress irp img and rectify depth
 *\return Error code
 */
int mini_isp_debug_dump_img(void)
{
	#define left_rotate_val  1
	#define right_rotate_val 2

	int errcode = ERR_SUCCESS;
	u8 write_buffer[4];
	u32 rg_img_in_size;
	u32 rotate_mode = 0;
	u32 img_width = 0, img_height = 0;
	u32 HighDistRateFlag = 0;
	u32 YUVFlag = 0;
	u32 img_size_mul = 100, img_size_div = 100;
	u32 img_addr = 0;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}
	mini_isp_register_read(0xfff8401c, &rg_img_in_size);
	mini_isp_register_read(0xfff84014, &rotate_mode);
	rotate_mode &= 0x3;

	/* Step 1 */
	write_buffer[0] = 1;
	mini_isp_memory_write(0x10, write_buffer, 1);

	msleep(2000); /* delay 2 sec */
	/* IRP image rotate */
	if (rotate_mode == right_rotate_val || rotate_mode == left_rotate_val) {
		img_height = (rg_img_in_size & 0x1fff) + 1;         /* 0 base */
		img_width = ((rg_img_in_size >> 16) & 0x1fff) + 1;  /* 0 base */
	} else {
		img_width = (rg_img_in_size & 0x1fff) + 1;          /* 0 base */
		img_height = ((rg_img_in_size >> 16) & 0x1fff) + 1; /* 0 base */
	}

	mini_isp_register_read(0xffe80064, &YUVFlag);
	YUVFlag = !YUVFlag;
	mini_isp_register_read(0x24, &HighDistRateFlag);
	HighDistRateFlag &= 0xFF;

	if (HighDistRateFlag) {
		img_addr = 0x20500000 + 288000 - (img_width * img_height);
		img_size_mul = 100;
		img_size_div = 100;
	} else {
		img_addr = 0x20500000;
		img_size_mul = 125;
		img_size_div = 100;
	}

	misp_info("%s -  [W,H]: [%d, %d], rotate_mode: %d, YUV: %d, HighDistRateFlag: %d",
			__func__, img_width, img_height,
			rotate_mode, YUVFlag, HighDistRateFlag);

	mini_isp_memory_read_then_write_file(img_addr,
		(img_width * img_height * img_size_mul / img_size_div),
		MINIISP_INFO_DUMPLOCATION, "compress_irp");

	/* Step 2 */
	write_buffer[0] = 2;
	mini_isp_memory_write(0x10, write_buffer, 1);
	msleep(100); /* image will broken if no delay time */

	mini_isp_register_read(0xfff5601C, &rg_img_in_size);
	img_width = (rg_img_in_size & 0x1fff) + 1;
	img_height = ((rg_img_in_size >> 16) & 0x1fff) + 1;

	mini_isp_memory_read_then_write_file(0x20715400,
		(img_width * img_height * 10/8),
		MINIISP_INFO_DUMPLOCATION, "rect_depth");

	/* step 3, restore */
	write_buffer[0] = 0;
	mini_isp_memory_write(0x10, write_buffer, 1);
	msleep(100);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
			misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_dump_img);

/**
 *\brief dump every phase of depth module info, and save as file
 *\param is_ground_mode     [in] 0: For depth normal mdoe.
 *\                              1: For depth ground mode
 *\return Error code
 */
int mini_isp_debug_depth_rect_combo_dump(u8 is_ground_mode)
{
	int errcode = ERR_SUCCESS;
	u8 write_buffer[4];
	u8 filepath[80];
	u8 filename[80];
	u8 loopNum = 1, loopIdx = 0;
	u32 rg_img_in_size;
	u32 ref_img_width = 0, ref_img_height = 0;
	u32 start_x = 0, start_y = 0;
	u32 imgAddr, ref_imgLinePixelOffset;
	u32 tmp;

	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	if (is_ground_mode)
		loopNum = 2;
	else
		loopNum = 1;

	/* create base folder */
	snprintf(filepath, 80, "%s/depth_combo_dump/",
		MINIISP_INFO_DUMPLOCATION);
	errcode = mini_isp_create_directory(filepath);

	/* First loop for Normal, second loop for gound */
	for (loopIdx = 0; loopIdx < loopNum; loopIdx++) {
		/* step 1 */
		write_buffer[0] = 1;
		mini_isp_memory_write(0x10, write_buffer, 1);
		msleep(100);

		/* step 2 */
		write_buffer[0] = 1;
		mini_isp_memory_write(0x20, write_buffer, 1);
		msleep(100);

		/* step 3 */
		write_buffer[0] = 2;
		mini_isp_memory_write(0x10, write_buffer, 1);
		msleep(100);

		/* dump reg */
		snprintf(filepath, 80,
			"%s/depth_combo_dump/%s_step3_rectify_a_0/",
		    MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(filepath, "rectify_a_0");

		mini_isp_register_read(0xFFF8C104, &tmp);
		start_x = (tmp & 0x00001fff)+1;
		start_y = ((tmp & 0x1fff0000) >> 16)+1;

		mini_isp_register_read(0xFFF8C108, &tmp);
		ref_img_width = (tmp & 0x00001fff)+1;
		ref_img_height = ((tmp & 0x1fff0000) >> 16)+1;

		ref_img_width = ref_img_width - start_x + 1;
		ref_img_height = ref_img_height - start_y + 1;

		misp_info("step3 m [ref_w,ref_h]: %d ,%d", ref_img_width, ref_img_height);

		mini_isp_register_read(0xFFF8C510, &imgAddr);
		mini_isp_register_read(0xFFF8C518, &ref_imgLinePixelOffset);
		ref_imgLinePixelOffset *= 8;

		/* step 4 */
		write_buffer[0] = 2;
		mini_isp_memory_write(0x20, write_buffer, 1);
		msleep(100);

		/* dump reg */
		snprintf(filepath, 80,
			"%s/depth_combo_dump/%s_step4_rectify_a_0/",
		    MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(filepath, "rectify_a_0");

		/* dump img */
		misp_info("[Step4] %s Main W,H: %d, %d", (loopIdx == 0)?"n":"g",
			ref_img_width, ref_img_height);
			snprintf(filepath, 80, "%s/depth_combo_dump/",
			MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		snprintf(filename, 80, "%s_%s", (loopIdx == 0)?"n":"g", "step4_mainRect_img_8bit.raw");
		errcode = mini_isp_woi_memory_read_then_write_file(imgAddr,
			ref_imgLinePixelOffset, ref_img_width, ref_img_height, filepath, filename);

		mini_isp_register_read(0xFFF8C104, &tmp);
		start_x = (tmp & 0x00001fff)+1;
		start_y = ((tmp & 0x1fff0000) >> 16)+1;

		mini_isp_register_read(0xFFF8C108, &tmp);
		ref_img_width = (tmp & 0x00001fff)+1;
		ref_img_height = ((tmp & 0x1fff0000) >> 16)+1;

		ref_img_width = ref_img_width - start_x + 1;
		ref_img_height = ref_img_height - start_y + 1;

		mini_isp_register_read(0xFFF8C510, &imgAddr);
		mini_isp_register_read(0xFFF8C518, &ref_imgLinePixelOffset);
		ref_imgLinePixelOffset *= 8;

		/* dump img */
		misp_info("[Step4] %s Sub W,H: %d, %d",
			(loopIdx == 0)?"n":"g",
			ref_img_width, ref_img_height);
		snprintf(filepath, 80, "%s/depth_combo_dump/",
			MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		snprintf(filename, 80, "%s_%s", (loopIdx == 0)?"n":"g",
			"step4_subRect_img_8bit.raw");
		errcode = mini_isp_woi_memory_read_then_write_file(imgAddr,
			ref_imgLinePixelOffset, ref_img_width,
			ref_img_height, filepath, filename);

		/* step 5 */
		write_buffer[0] = 3;
		mini_isp_memory_write(0x20, write_buffer, 1);
		msleep(100);

		/* dump reg */
		snprintf(filepath, 80,
			"%s/depth_combo_dump/%s_step5_rectify_a_0/",
		    MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(
			filepath, "rectify_a_0");

		snprintf(filepath, 80,
			"%s/depth_combo_dump/%s_step5_rectify_b_0/",
			MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(
			filepath, "rectify_b_0");

		snprintf(filepath, 80,
			"%s/depth_combo_dump/%s_step5_dg_ca_a_0/",
			MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(
			filepath, "dg_ca_a_0");

		snprintf(filepath, 80,
			"%s/depth_combo_dump/%s_step5_dg_mcc_a_0/",
			MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(
			filepath, "dg_mcc_a_0");

		/* dump img */
		misp_info("[Step5] %s dg W,H: %d, %d",
			(loopIdx == 0)?"n":"g", 1024,
			ref_img_height);
		snprintf(filepath, 80, "%s/depth_combo_dump/",
			MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		snprintf(filename, 80, "%s_%s",
			(loopIdx == 0)?"n":"g",
			"step5_dg_img_10unpack.raw");
		errcode = mini_isp_woi_memory_read_then_write_file(0x20100000,
			1024*2, ref_img_width*2, ref_img_height, filepath, filename);

		/* step 6 */
		write_buffer[0] = 4;
		mini_isp_memory_write(0x20, write_buffer, 1);
		msleep(100);

		/* dump reg */
		snprintf(filepath, 80, "%s/depth_combo_dump/%s_step6_dp_top_a_0/",
		    MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(filepath, "dp_top_a_0");

		/* dump img */
		misp_info("[Step6] %s dg_top W,H: %d, %d",
			(loopIdx == 0)?"n":"g", 1024,
			ref_img_height);
		snprintf(filepath, 80, "%s/depth_combo_dump/",
			MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		snprintf(filename, 80, "%s_%s", (loopIdx == 0)?"n":"g",
			"step6_dp_img_10unpack.raw");
		errcode = mini_isp_woi_memory_read_then_write_file(0x20100000,
			1024*2, ref_img_width*2,
			ref_img_height, filepath, filename);

		/* step 7 */
		write_buffer[0] = 5;
		mini_isp_memory_write(0x20, write_buffer, 1);
		msleep(100);

		/* dump reg */
		snprintf(filepath, 80, "%s/depth_combo_dump/%s_step7_rectify_b_0/",
		    MINIISP_INFO_DUMPLOCATION, (loopIdx == 0)?"n":"g");
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(filepath, "rectify_b_0");

		/* dump img */
		if (is_ground_mode) {
			mini_isp_register_read(0xFFF8C104, &tmp);
			start_x = (tmp & 0x00001fff)+1;
			start_y = ((tmp & 0x1fff0000) >> 16)+1;

			mini_isp_register_read(0xFFF8C108, &tmp);
			ref_img_width = (tmp & 0x00001fff)+1;
			ref_img_height = ((tmp & 0x1fff0000) >> 16)+1;

			ref_img_width = ref_img_width - start_x + 1;
			ref_img_height = ref_img_height - start_y + 1;
		} else {
			mini_isp_register_read(0xfff5601c, &rg_img_in_size);
			ref_img_width = (rg_img_in_size & 0x00001fff) + 1;
			ref_img_height = ((rg_img_in_size & 0x1fff0000)>>16) + 1;
		}

		misp_info("[Step7] %s InvRect W,H: %d, %d",
			(loopIdx == 0)?"n":"g",
			ref_img_width, ref_img_height);
		snprintf(filepath, 80, "%s/depth_combo_dump/",
			MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);

		imgAddr = (loopIdx == 0) ? 0x20715400 : 0x20669AC0;
		snprintf(filename, 80, "%s_%s",
			(loopIdx == 0)?"n":"g",
			"step7_InvRect_img_10pack.raw");

		errcode = mini_isp_memory_read_then_write_file(imgAddr,
			(ref_img_width * ref_img_height * 10 / 8), filepath, filename);

	}

	if (is_ground_mode) {
		write_buffer[0] = 6;
		mini_isp_memory_write(0x20, write_buffer, 1);
		msleep(300);

		/* dump reg */
		snprintf(filepath, 80, "%s/depth_combo_dump/step9_rectify_b_0/",
		    MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		mini_isp_chip_base_define_module_reg_dump(filepath, "rectify_b_0");

		/* dump img */
		misp_info("[Step9] blending W,H: %d, %d", 1024, ref_img_height);
		snprintf(filepath, 80, "%s/depth_combo_dump/", MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		errcode = mini_isp_woi_memory_read_then_write_file(0x20100000,
			1024*2, ref_img_width*2, ref_img_height, filepath, "g_step9_blending_img_10unpack.raw");

		misp_info("[Step9] InvRect W,H: %d, %d", ref_img_width, ref_img_height);
		snprintf(filepath, 80, "%s/depth_combo_dump/", MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);

		mini_isp_register_read(0xfff5601c, &rg_img_in_size);
		ref_img_width = (rg_img_in_size & 0x00001fff) + 1;
		ref_img_height = ((rg_img_in_size & 0x1fff0000)>>16) + 1;
		errcode = mini_isp_memory_read_then_write_file(0x20715400,
			(ref_img_width * ref_img_height * 10 / 8), filepath, "g_step9_InvRect_img_10pack.raw");
	}

	/* step final restore */
	write_buffer[0] = 0;
	mini_isp_memory_write(0x20, write_buffer, 1);
	msleep(300);

	write_buffer[0] = 0;
	mini_isp_memory_write(0x10, write_buffer, 1);
	msleep(300);

	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_depth_rect_combo_dump);

errcode mini_isp_debug_packdata_dump(void)
{
	errcode errcode = ERR_SUCCESS;
	u32 packdebuginfo_addr = 0;
	u32 packdata_addr = 0;
	char filepath[80];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	mini_isp_memory_read(DEBUG_INFO_DEPTH_FLOW_INFO_ADDR, (u8 *)&packdebuginfo_addr, 4);
	if (packdebuginfo_addr != 0) {
		mini_isp_memory_read(packdebuginfo_addr+PACKDATA_OFFSET, (u8 *)&packdata_addr, 4);
	} else{
		misp_info("%s, addr err", __func__);
		goto EXIT;
	}

	if (packdata_addr != 0) {
		misp_info("%s dump 1824 bytes", __func__);
		snprintf(filepath, 80, "%s", MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		errcode = mini_isp_memory_read_then_write_file(packdata_addr,
			PACKDATA_SIZE, filepath, "Dump_packdata.bin");
	} else{
		misp_info("%s, addr err", __func__);
		goto EXIT;
	}

EXIT:
	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_packdata_dump);

/**
 *\brief save IQCalibration data as a file
 *\return Error code
 */
errcode mini_isp_debug_IQCalib_dump(void)
{
	errcode errcode = ERR_SUCCESS;
	u32 IQCalib_addr = 0;
	char filepath[80];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	mini_isp_memory_read(0x30, (u8 *)&IQCalib_addr, 4);
	if (IQCalib_addr != 0) {
		misp_info("%s dump", __func__);
		snprintf(filepath, 80, "%s", MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		errcode = mini_isp_memory_read_then_write_file(IQCalib_addr,
			16556, filepath, "Dump_IQCalibration.bin");
	} else
		misp_info("%s, addr err", __func__);

	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_IQCalib_dump);

/**
 *\brief save metadata as a file
 *\return Error code
 */
errcode mini_isp_debug_metadata_dump(void)
{
	errcode errcode = ERR_SUCCESS;
	u32 metadata_addr = 0;
	char filepath[80];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	mini_isp_memory_read(0x14, (u8 *)&metadata_addr, 4);
	if (metadata_addr != 0) {
		misp_info("%s dump", __func__);
		snprintf(filepath, 80, "%s", MINIISP_INFO_DUMPLOCATION);
		errcode = mini_isp_create_directory(filepath);
		errcode = mini_isp_memory_read_then_write_file(metadata_addr,
			4*1024, filepath, "Dump_MetaData.bin");
	} else
		misp_info("%s, addr err", __func__);

	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_metadata_dump);

static u32 CalculatePackdataCheckSum(u8 *PackdataBuf)
{
	u32 *PackdataBuf4B = (u32 *)PackdataBuf;
	u32 CheckSum = 0;
	u32 ii = 0;

	for (ii = 0; ii < PACKDATA_SIZE/4; ii++) {
		CheckSum += *PackdataBuf4B;
		PackdataBuf4B++;
	}

	return CheckSum;
}

static void mini_isp_debug_depth_info_parser(DEPTHFLOWMGR_DEBUG_INFO *ptDepthFlowInfo)
{
	u32 modeid;
	u32 refImg1_w, refImg1_h, refImg2_w, refImg2_h, refImg3_w, refImg3_h;
	u32 refImg1_Idx, refImg2_Idx, refImg3_Idx;
	u32 KernelSizeRatio;
	u32 DepthType;
	u32 GroundMode;
	u32 ValidPackdata;
	u32 ValidDisparityToDistanceTable;
	u32 HighDistortionRateFlag;
	u32 PackdataIdxNormal, PackdataIdxGround, PackdataIdxRGB;
	u32 GlobalCompensation;
	u32 HwCoefficient;
	u32 HighDistortionIdx;
	u32 PackdataSrc;
	u32 WOI_x, WOI_y, WOI_w, WOI_h, Full_w, Full_h;
	u32 ValidBlendingTable;
	u32 BlendingSrc;
	u32 ValidQmerge;
	u32 dist[] = { 0, 0, 0 };
	u32 tmp;

	const u32 modeid_min = 0;
	const u32 modeid_max = 8;
	const u32 KernelSizeRatio_min = 0;
	const u32 KernelSizeRatio_max = 15;
	const u32 GlobalCompensation_21 = 21;
	const u32 GlobalCompensation_36 = 36;
	const u32 HwCoefficient_16 = 16;
	const u32 uwHighDistortionIdx_max = 5;
	const char ResStr[4][64] = {"16:9", "16:10", "4:3", "@:@"};

	if (ptDepthFlowInfo == NULL) {
		misp_info("%s fail!", __func__);
		return;
	}
	if (!ptDepthFlowInfo->ValidFlag) {
		misp_info("depth info ValidFlag err! L:%d", __LINE__);
		return;
	}

	if (ptDepthFlowInfo->Ver != DEPTH_FLOWMGR_INFO_VER) {
		misp_info("depth info version err! L:%d", __LINE__);
		misp_info("Depth debug version: 0x%X", ptDepthFlowInfo->Ver);
		misp_info("Linux depth debug ver: 0x%X", DEPTH_FLOWMGR_INFO_VER);
		return;
	}

	misp_info("====== Depth Debug Info ======");
	misp_info("=== Check Depth Debug Info Version ===");
	misp_info("Depth debug version: 0x%X", ptDepthFlowInfo->Ver);
	misp_info("alDU ver: %d.%d", ptDepthFlowInfo->ALDU_MainVer, ptDepthFlowInfo->ALDU_SubVer);
	misp_info("=== DepthFlowInfo: General ===");


	if (ptDepthFlowInfo->tDepthFlowDebugInfo.ValidDepthFlowDebugInfo == false) {
		misp_info("General information all invalid.");
		goto DepthFlowDebugInfoEnd;
	} else{
		modeid = ptDepthFlowInfo->tDepthFlowDebugInfo.DepthFlowModeId;
		if (modeid > modeid_max) {
			misp_info("Depth Flow Mode ID: %d. (error! mode id out of range %d~%d)",
				modeid, modeid_min, modeid_max);
			goto DepthFlowDebugInfoEnd;
		} else
		    misp_info("Depth Flow Mode ID: %d. (valid range %d~%d)",
			modeid, modeid_min, modeid_max);
	}

	/* ref img1 w/h */
	refImg1_w = ptDepthFlowInfo->tDepthFlowDebugInfo.refImg1_Width;
	refImg1_h = ptDepthFlowInfo->tDepthFlowDebugInfo.refImg1_Height;
	switch (16 * refImg1_h / refImg1_w) {
	case 9:
		refImg1_Idx = RESOLUTION_16_9;
	break;
	case 10:
		refImg1_Idx = RESOLUTION_16_10;
	break;
	case 12:
		refImg1_Idx = RESOLUTION_4_3;
	break;
	default:
		refImg1_Idx = RESOLUTION_OTHER;
	break;
	}

	if (refImg1_Idx != RESOLUTION_OTHER)
		misp_info("ref1 Img Resolution: %dx%d. (%s)",
			refImg1_w, refImg1_h, ResStr[refImg1_Idx]);
	else {
		/* mutiple 100 for division */
		tmp = refImg1_h * 16 * 100 / refImg1_w;
		misp_info("ref1 Img Resolution: %dx%d. (16:%d.%d)",
			refImg1_w, refImg1_h, tmp/100, tmp%100);
	}

	/* ref img2 w/h */
	refImg2_w = ptDepthFlowInfo->tDepthFlowDebugInfo.refImg2_Width;
	refImg2_h = ptDepthFlowInfo->tDepthFlowDebugInfo.refImg2_Height;

	/* check resolution index */
	switch (16 * refImg2_h / refImg2_w) {
	case 9:
		refImg2_Idx = RESOLUTION_16_9;
	break;
	case 10:
		refImg2_Idx = RESOLUTION_16_10;
	break;
	case 12:
		refImg2_Idx = RESOLUTION_4_3;
	break;
	default:
		refImg2_Idx = RESOLUTION_OTHER;
	break;
	}

	/* display solution */
	if (refImg2_Idx != RESOLUTION_OTHER)
		misp_info("ref2 Img Resolution: %dx%d. (%s)",
			refImg2_w, refImg2_h, ResStr[refImg2_Idx]);
	else {
		/* mutiple 100 for division */
		tmp = refImg2_h * 16 * 100 / refImg2_w;
		misp_info("ref2 Img Resolution: %dx%d. (16:%d.%d)",
			refImg2_w, refImg2_h, tmp/100, tmp%100);
	}
	/* display warning */
	if (refImg2_Idx != refImg1_Idx)
		misp_info("(Warning!. ref1 Img resolution is %s, but ref2 Img resolution is %s.", ResStr[refImg1_Idx], ResStr[refImg2_Idx]);

	if (refImg2_w > refImg1_w)
		misp_info("(Warning!. ref2 Img width %d > ref1 Img width %d.", refImg2_w, refImg1_w);

	if (refImg2_h > refImg1_h)
		misp_info("(Warning!. ref2 Img height %d > ref1 Img height %d.", refImg2_h, refImg1_h);


	/* ref img3 w/h */
	refImg3_w = ptDepthFlowInfo->tDepthFlowDebugInfo.refImg3_Width;
	refImg3_h = ptDepthFlowInfo->tDepthFlowDebugInfo.refImg3_Height;
	switch (16 * refImg3_h / refImg3_w) {
	case 9:
		refImg3_Idx = RESOLUTION_16_9;
	break;
	case 10:
		refImg3_Idx = RESOLUTION_16_10;
	break;
	case 12:
		refImg3_Idx = RESOLUTION_4_3;
	break;
	default:
		refImg3_Idx = RESOLUTION_OTHER;
	break;
	}

	if (refImg3_Idx != RESOLUTION_OTHER)
		misp_info("ref3 Img Resolution: %dx%d. (%s)", refImg3_w, refImg3_h, ResStr[refImg3_Idx]);
	else {
		tmp = refImg3_h * 16 * 100 / refImg3_w ; /* mutiple 100 for division */
		misp_info("ref3 Img Resolution: %dx%d. (16:%d.%d)", refImg3_w, refImg3_h,
			tmp/100, tmp%100);
	}

	/* kernel size ratio */
	KernelSizeRatio = ptDepthFlowInfo->tDepthFlowDebugInfo.DepthImgKernelSizeRatio;
	if (KernelSizeRatio > KernelSizeRatio_max)
		misp_info("Kernel Size Ratio: %d. (error! ration output of range %d~%d).", KernelSizeRatio, KernelSizeRatio_min, KernelSizeRatio_max);
	else
		misp_info("Kernel Size Ratio: %d. (valid range %d~%d).", KernelSizeRatio, KernelSizeRatio_min, KernelSizeRatio_max);

	/* Depth type */
	DepthType = ptDepthFlowInfo->tDepthFlowDebugInfo.DepthType;
	misp_info("Depth Type: %d. (%s)", DepthType, (DepthType > 0) ? "Active" : "Passive");

	/* Ground mode */
	GroundMode = ptDepthFlowInfo->tDepthFlowDebugInfo.DepthGroundType;
	switch (GroundMode) {
	case 0:
		misp_info("Ground Type: 0 (Normal mode only).");
	break;
	case 0x10:
		misp_info("Ground Type: 0x%X. (Ground Enhance).",
			GroundMode);
	break;
	case 0x30:
		misp_info("Ground Type: 0x%X. (Ground Enhance + Advance Hold Fill).", GroundMode);
	break;
	case 0x50:
		misp_info("Ground Type: 0x%X. (Ground Enhance + Anti-Cliff).", GroundMode);
	break;
	case 0x70:
		misp_info("Ground Type: 0x%X. (Ground Enhance + Advance Hold Fill + Anti-Cliff).", GroundMode);
	break;
	default:
		misp_info("Ground Type: 0x%X. (Unkown feature).", GroundMode);
	break;
	}

	/* Processing time */
	if (0 != ptDepthFlowInfo->tDepthFlowDebugInfo.DephtProcTime) {
		tmp = ptDepthFlowInfo->tDepthFlowDebugInfo.DephtProcTime*100/1000;
		misp_info("Processing Time: %d.%d ms.", tmp/100, tmp%100);
		tmp = 1000000*100/ptDepthFlowInfo->tDepthFlowDebugInfo.DephtProcTime;
		misp_info("Processing max fps: %d.%d", tmp/100, tmp%100);
	} else
		misp_info("Processing Time: 0.");

	/* Blending process time */
	tmp = ptDepthFlowInfo->tDepthFlowDebugInfo.BlendingProcTime*100 / 1000;
	misp_info("Blending processing time: %d.%d ms.", tmp/100, tmp%100);

	/* Frame count */
	misp_info("Frame Count: %d.", ptDepthFlowInfo->tDepthFlowDebugInfo.DepthProcCnt);

	/* Feature flag: InvRec LDC bypass */
	misp_info("Feature flag: %d.", ptDepthFlowInfo->tDepthFlowDebugInfo.FeatureFlag);

	/* InvRECT_BypassLDC */
	misp_info("InvRect bypass LDC: %d.", ptDepthFlowInfo->tDepthFlowDebugInfo.InvRECT_BypassLDC);

	/* check feature flag */
	if ((ptDepthFlowInfo->tDepthFlowDebugInfo.FeatureFlag & 0x1) !=
		ptDepthFlowInfo->tDepthFlowDebugInfo.InvRECT_BypassLDC)
		misp_info("InvRect bypass flag is not eqaual feature flag.");

	/* PerformanceFlag */
	/* [Debug] FALSE: DEPTH_PERFORMANCE_FLAG disable, TRUE: DEPTH_PERFORMANCE_FLAG enable */
	misp_info("Performance flag: %d.", ptDepthFlowInfo->tDepthFlowDebugInfo.PerformanceFlag);

	/* NormalKernalSizeMatch */
	/* [Debug] Valid when DEPTH_PERFORMANCE_FLAG enable, TRUE: Kernel size of normal mode can be extimated by clock and Frame rate */
	misp_info("Normal kernel size match flag: %d.", ptDepthFlowInfo->tDepthFlowDebugInfo.NormalKernalSizeMatch);

	/* GroundKernalSizeMatch */
	/* [Debug] Valid when DEPTH_PERFORMANCE_FLAG enable, TRUE: Kernel size of ground mode can be extimated by clock and Frame rate */
	misp_info("Ground kernel size match flag: %d.", ptDepthFlowInfo->tDepthFlowDebugInfo.GroundKernalSizeMatch);

	/* NormalKernelSizeIdx */
	/* [Debug] Kernel size idx of normal mode, if NormalKernalSizeMatch is False,
	then it means the kernel size could not be extimated by clock and frame rate */
	misp_info("Normal kernel size index: %d. (0~15)", ptDepthFlowInfo->tDepthFlowDebugInfo.NormalKernelSizeIdx);

	/* GroundKernelSizeIdx */
	/* [Debug] Kernel size idx of ground mode, if GroundKernalSizeMatch is False,
	then it means the kernel size could not be extimated by clock and frame rate */
	misp_info("Ground kernel size index: %d. (0~15)", ptDepthFlowInfo->tDepthFlowDebugInfo.GroundKernelSizeIdx);
DepthFlowDebugInfoEnd:

	misp_info("=== DepthFlowInfo: Utility ===");
	if (ptDepthFlowInfo->tDepthUtiDebugInfo.ValidDepthUtiDebugInfo == false) {
		misp_info("Utility information all invalid.");
		goto UtilityEnd;
	}

	/* ValidBlendingTable */
	ValidBlendingTable = ptDepthFlowInfo->tDepthUtiDebugInfo.ValidBlendingTable;
	if (ValidBlendingTable) {
		/* Set blending table source */
		BlendingSrc = ptDepthFlowInfo->tDepthUtiDebugInfo.BlendingTableSource;
		if (BlendingSrc == DEPTHBLENDING_SRC_CMD)
			misp_info("Blending table is available: %d. (AP send to ISP).", BlendingSrc);
		else if (BlendingSrc == DEPTHBLENDING_SRC_ISP)
			misp_info("Blending table is available: %d. (ISP calculated done).", BlendingSrc);
		else
			misp_info("Warning!! Blending table source from nowhere: %d. (ground mode may fail).", BlendingSrc);
	}


	/* Distance */
	dist[0] = ptDepthFlowInfo->tDepthUtiDebugInfo.DistanceVal[0];
	dist[1] = ptDepthFlowInfo->tDepthUtiDebugInfo.DistanceVal[1];
	dist[2] = ptDepthFlowInfo->tDepthUtiDebugInfo.DistanceVal[2];

	misp_info("Distance: (upper, middle, lower) = (%d, %d, %d)mm. (after v764 change cm to mm)",
		dist[0], dist[1], dist[2]);

	/* Blending table source and size */
	misp_info("Blending table source: %d. (0:None 1:AP 2:ISP)", ptDepthFlowInfo->tDepthUtiDebugInfo.BlendingTableSource);
	misp_info("Blending table size: %d.", ptDepthFlowInfo->tDepthUtiDebugInfo.BlendingTableSize);

	/* Blending start line */
	misp_info("Blending start line: %d.", ptDepthFlowInfo->tDepthUtiDebugInfo.BlendingStartLine);

	/* Valid QMerge */
	ValidQmerge = ptDepthFlowInfo->tDepthUtiDebugInfo.ValidQmerge;
	if (ValidQmerge)
		misp_info("QMerge table updated by AP: %d. (Received)", ValidQmerge);
	else
		misp_info("QMerge table updated by AP: %d. (No updated)", ValidQmerge);

	/* Dynamic warping */
	misp_info("Dynamic warping updated or not: %d.",
		ptDepthFlowInfo->tDepthUtiDebugInfo.DWUpdated);
UtilityEnd:

	misp_info("=== DepthFlowInfo: Packdata ===");
	if (ptDepthFlowInfo->tDepthPackdataDebugInfo.ValidDepthPacadataDebugInfo == false) {
		misp_info("Packdata information all invalid.");
		goto PackdataDebugInfoEnd;
	}

	/* check AP download packdata to ISP or not */
	ValidPackdata = ptDepthFlowInfo->tDepthPackdataDebugInfo.ValidPackdata;
	if (ValidPackdata)
		misp_info("Valid Packdata=%d. (Packdata download completed.).", ValidPackdata);
	else{
		misp_info("Valid Packdata=%d.", ValidPackdata);
		misp_info("(Packdata doesn't be downloaded, depth qulity and flow may fail.)");
	}

	/* Disparity to distance table convert. */
	ValidDisparityToDistanceTable = ptDepthFlowInfo->tDepthPackdataDebugInfo.ValidDisparityToDistanceTable;
	if (ValidDisparityToDistanceTable)
		misp_info("Valid Disparity to Distance Table = %d. (Valid.).", ValidDisparityToDistanceTable);
	else
		misp_info("Valid Disparity to Distance Table = %d. (Table not ready, convet may fail).",
			ValidDisparityToDistanceTable);

	/* High distortion rate flag */
	HighDistortionRateFlag = ptDepthFlowInfo->tDepthPackdataDebugInfo.HighDistortionRate;
	if (HighDistortionRateFlag)
		misp_info("High distortion rate flag: %d. (High distortion)", HighDistortionRateFlag);
	else {
		misp_info("High distortion rate flag: %d. (Low distortion)", HighDistortionRateFlag);
		misp_info("(Warning! in current lib, high distortion rate flag should be 1.");
	}

	/* Packdata usage index */
	PackdataIdxNormal = ptDepthFlowInfo->tDepthPackdataDebugInfo.PackdataNormalIdx;
	PackdataIdxGround = ptDepthFlowInfo->tDepthPackdataDebugInfo.PackdataGroundIdx;
	PackdataIdxRGB = ptDepthFlowInfo->tDepthPackdataDebugInfo.PackdataRGBIdx;
	if ((PackdataIdxNormal <= 2) && (PackdataIdxNormal <= 2) && (PackdataIdxNormal <= 2))
		misp_info("Packdata index:(normal, ground, RGB) = (%d, %d, %d). (generally, it is (0,0,2))",
			PackdataIdxNormal, PackdataIdxGround, PackdataIdxRGB);
	else
		misp_info("Packdata index:(normal, ground, RGB) = (%d, %d, %d). (error! valid value should 0~2)",
			PackdataIdxNormal, PackdataIdxGround, PackdataIdxRGB);

	/* Global compensation */
	GlobalCompensation = ptDepthFlowInfo->tDepthPackdataDebugInfo.AbsoluteGlobalVal;
	if (GlobalCompensation == GlobalCompensation_21 || GlobalCompensation == GlobalCompensation_36)
		misp_info("Global Compensation value: %d. (valid value is %d or %d)",
			GlobalCompensation, GlobalCompensation_21, GlobalCompensation_36);
	else
		misp_info("Global Compensation value: %d. (error! it should be %d or %d)",
			GlobalCompensation, GlobalCompensation_21, GlobalCompensation_36);

	/* HwCoefficient */
	HwCoefficient = ptDepthFlowInfo->tDepthPackdataDebugInfo.HwCoefficient;
	if (HwCoefficient == HwCoefficient_16)
		misp_info("HW coefficient: %d. (valid value is %d)", HwCoefficient, HwCoefficient_16);
	else
		misp_info("HW coefficient: %d. (error! it should be %d)", HwCoefficient, HwCoefficient_16);

	/* index of high distrotion rate */
	HighDistortionIdx = ptDepthFlowInfo->tDepthPackdataDebugInfo.HighDistortionIdx;
	if (HighDistortionIdx <= uwHighDistortionIdx_max)
		misp_info("High distortion index: %d. (valid range 0~5)", HighDistortionIdx);
	else
		misp_info("High distortion index: %d. (error! if should be 0~5)", HighDistortionIdx);

	/* Packdata source */
	PackdataSrc = ptDepthFlowInfo->tDepthPackdataDebugInfo.PackdataSource;
	if (PackdataSrc == DEPTHPACKDATA_SRC_CMD)
		misp_info("Packdata source from AP: %d. (Received)", PackdataSrc);
	else if (PackdataSrc == DEPTHPACKDATA_SRC_OTP)
		misp_info("Packdata source from ISP reading OTP: %d.", PackdataSrc);
	else
		misp_info("Warning! packdata source from nowhere: %d. (Depth may fail)", PackdataSrc);

	/* WOI */
	WOI_x = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOIMainXBase;
	WOI_y = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOIMainYBase;
	WOI_w = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOIMainXLength;
	WOI_h = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOIMainYLength;
	Full_w = WOI_w + (WOI_x) * 2;
	Full_h = WOI_h + (WOI_y) * 2;
	misp_info("Main Camera WOI (x,y,w,h) = (%d, %d, %d, %d). (FullSize WxH = %dx%d)",
		WOI_x, WOI_y, WOI_w, WOI_h, Full_w, Full_h);

	WOI_x = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOISubXBase;
	WOI_y = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOISubYBase;
	WOI_w = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOISubXLength;
	WOI_h = ptDepthFlowInfo->tDepthPackdataDebugInfo.WOISubYLength;
	Full_w = WOI_w + (WOI_x) * 2;
	Full_h = WOI_h + (WOI_y) * 2;
	misp_info("Sub Camera WOI (x,y,w,h) = (%d, %d, %d, %d). (FullSize WxH = %dx%d)",
		WOI_x, WOI_y, WOI_w, WOI_h, Full_w, Full_h);

	/* Sub can rectify left shift */
	misp_info("Short distance(cm): %d. (For sub-cam retify left shift)",
		ptDepthFlowInfo->tDepthPackdataDebugInfo.SubCamRectShift);

	misp_info("IntrinsicK_Main[9] = %d, %d, %d, %d, %d, %d, %d, %d, %d.",
		ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[0], ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[1],
		ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[2], ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[3],
		ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[4], ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[5],
		ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[6], ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[7],
		ptDepthFlowInfo->tDepthPackdataDebugInfo.IntrinsicK_Main[8]);

	/* check ALDU errcode */
	if (0 == ptDepthFlowInfo->tDepthPackdataDebugInfo.AlDUErrCode)
		misp_info("alDU operation error code: 0x%X. (success)", ptDepthFlowInfo->tDepthPackdataDebugInfo.AlDUErrCode);
	else {
		misp_info("alDU operation fail, error code: 0x%X.", ptDepthFlowInfo->tDepthPackdataDebugInfo.AlDUErrCode);
		switch (ptDepthFlowInfo->tDepthPackdataDebugInfo.AlDUErrCode) {
		case 0x9001:
			misp_info("alDU Buffer size too small.");
		break;
		case 0x9002:
			misp_info("alDU Buffer is null.");
		break;
		case 0x9003:
			misp_info("alDU Rse size too small.");
		break;
		case 0x9004:
			misp_info("alDU Rse packdata invalid.");
		break;
		default:
			misp_info("alDU Unkown error code.");
		break;
		}
	}

	if (0 == ptDepthFlowInfo->tDepthPackdataDebugInfo.ConvertMatch)
		misp_info("Packdata convert task process time doen't match alDU convert process time: %d.",
			ptDepthFlowInfo->tDepthPackdataDebugInfo.ConvertMatch);
	else
		misp_info("Packdata convert task process time match alDU convert process time: %d.",
			ptDepthFlowInfo->tDepthPackdataDebugInfo.ConvertMatch);

	misp_info("Packdata convert count: %d.", ptDepthFlowInfo->tDepthPackdataDebugInfo.ConvertCount);
	misp_info("Normal mode rectify QP pingpong index: %d.", ptDepthFlowInfo->tDepthPackdataDebugInfo.NormalRectQPReadPingPongIdx);

	if (0 == ptDepthFlowInfo->tDepthFlowDebugInfo.DepthGroundType) {
		if (ptDepthFlowInfo->tDepthPackdataDebugInfo.NormalRectQPReadPingPongIdx !=
			((ptDepthFlowInfo->tDepthPackdataDebugInfo.ConvertCount + 1) % 2))
			misp_info("Normal QP buffer index invalid.");
		else
			misp_info("Normal QP buffer index valid.");
	}
PackdataDebugInfoEnd:

	return;
}

/**
 *\brief log depth module debug info
 *\param a_IsLog2File   [In] 0: print to dmesg. 1: log to file.
 *\return Error code
 */
errcode mini_isp_debug_depth_info(void)
{
	errcode errcode = ERR_SUCCESS;
	u32 depthinfo_addr = 0;
	u32 packdata_addr = 0;
	u32 PackdataCheckSum = 0;
	u8 *packdata_buffer = NULL;
	DEPTHFLOWMGR_DEBUG_INFO tDepthFlowInfo = {0};

	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	/* 1. read depth flow info structure addr */
	mini_isp_memory_read(DEBUG_INFO_DEPTH_FLOW_INFO_ADDR, (u8 *)&depthinfo_addr, 4);
	if (depthinfo_addr == 0) {
		misp_info("read depth info addr fail! L:%d", __LINE__);
		goto EXIT;
	}

	/* 2. read depth flow info structure context */
	mini_isp_memory_read(depthinfo_addr, (u8 *)&tDepthFlowInfo,
		sizeof(DEPTHFLOWMGR_DEBUG_INFO));

	/* 3. show depth flow info */
	mini_isp_debug_depth_info_parser(&tDepthFlowInfo);

	/* 4. check packdata checksum */
	packdata_addr = tDepthFlowInfo.tDepthPackdataDebugInfo.PackdataAddr;
	misp_info("Packdata address: 0x%X.", packdata_addr);

	packdata_buffer = kzalloc(PACKDATA_SIZE, GFP_KERNEL);
	if (packdata_buffer != NULL) {
		mini_isp_memory_read(packdata_addr, packdata_buffer, PACKDATA_SIZE);
		PackdataCheckSum = CalculatePackdataCheckSum(packdata_buffer);
		if (PackdataCheckSum == tDepthFlowInfo.tDepthPackdataDebugInfo.PackdataChkSum)
			misp_info("Packdata check sum: 0x%X. (equal tool).", tDepthFlowInfo.tDepthPackdataDebugInfo.PackdataChkSum);
		else
			misp_info("Packdata check fail: ISP:0x%X. Tool:0x%X.", tDepthFlowInfo.tDepthPackdataDebugInfo.PackdataChkSum, PackdataCheckSum);

		kfree(packdata_buffer);
	} else{
		misp_info("allocate packdata buffer fail!");
	}

EXIT:
	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_depth_info);

static void mini_isp_debug_metadata_parser(CAPFLOWMGR_METADATA *ptMetadata)
{
	u64 DualPathFrameSyncTimeDiff = 0;

	if (ptMetadata == NULL) {
		misp_info("%s fail!", __func__);
		return;
	}
	misp_info("=============== Metadata ===============");
	misp_info("---=== Metadata:Common ===----");
	misp_info("Metadata version: %d.", ptMetadata->tCommonInfo.MetaDataVer);
	misp_info("SCID: %d.", ptMetadata->tCommonInfo.SCID);
	misp_info("Swap(Rx,Tx): (%d,%d).", ptMetadata->tCommonInfo.RxSwap, ptMetadata->tCommonInfo.TxSwap);
	misp_info("SensorType: %d.", ptMetadata->tCommonInfo.SensorType);
	misp_info("HDR (PPIdx, LPPw, LPPh, RPPw, RPPh): (%d,%d,%d,%d,%d).",
		ptMetadata->tCommonInfo.HDRPPPipeIdx, ptMetadata->tCommonInfo.LPPWidth,
		ptMetadata->tCommonInfo.LPPHeight, ptMetadata->tCommonInfo.RPPWidth,
		ptMetadata->tCommonInfo.RPPHeight);

	misp_info("---=== Metadata:PIPE0 Info ===----");

	misp_info("Frame Index: %d.", ptMetadata->tPipe0Info.FrameIndex);
	misp_info("SrcImg (width, height, ColorOrder): (%d, %d, %d).",
		ptMetadata->tPipe0Info.SrcImgWidth, ptMetadata->tPipe0Info.SrcImgHeight,
		ptMetadata->tPipe0Info.ColorOrder);
	misp_info("(ExpTime, wBV): (%d, %d).", ptMetadata->tPipe0Info.ExpTime, ptMetadata->tPipe0Info.BV);
	misp_info("(ISO, ADGain): (%d, %d).", ptMetadata->tPipe0Info.ISO, ptMetadata->tPipe0Info.AD_Gain);
	misp_info("AWBGain(R,G,B): (%d, %d, %d).", ptMetadata->tPipe0Info.AWB_RGain,
		ptMetadata->tPipe0Info.AWB_GGain, ptMetadata->tPipe0Info.AWB_BGain);
	misp_info("BlkOst(R,G,B): (%d, %d, %d).", ptMetadata->tPipe0Info.BlackOffset_R,
		ptMetadata->tPipe0Info.BlackOffset_G, ptMetadata->tPipe0Info.BlackOffset_B);
	misp_info("Crop(x,y,w,h): (%d, %d, %d, %d).", ptMetadata->tPipe0Info.Crop_X,
		ptMetadata->tPipe0Info.Crop_Y, ptMetadata->tPipe0Info.Crop_Width,
		ptMetadata->tPipe0Info.Crop_Height);
	misp_info("Scalar(w,h): (%d, %d).", ptMetadata->tPipe0Info.ScalarWidth,
		ptMetadata->tPipe0Info.ScalarHeight);
	misp_info("VCM(macro,infinity,curStep,status): (%d, %d, %d, %d).",
		ptMetadata->tPipe0Info.VCM_macro, ptMetadata->tPipe0Info.VCM_infinity,
		ptMetadata->tPipe0Info.VCM_CurStep, ptMetadata->tPipe0Info.Depth_VCMStatus);

	misp_info("---=== Metadata:PIPE1 Info ===----");
	misp_info("Frame Index: %d.", ptMetadata->tPipe1Info.FrameIndex);
	misp_info("SrcImg (width, height, ColorOrder): (%d, %d, %d).", ptMetadata->tPipe1Info.SrcImgWidth,
		ptMetadata->tPipe1Info.SrcImgHeight, ptMetadata->tPipe1Info.ColorOrder);
	misp_info("(ExpTime, wBV): (%d, %d).", ptMetadata->tPipe1Info.ExpTime, ptMetadata->tPipe1Info.BV);
	misp_info("(ISO, ADGain): (%d, %d).", ptMetadata->tPipe1Info.ISO, ptMetadata->tPipe1Info.AD_Gain);
	misp_info("AWBGain(R,G,B): (%d, %d, %d).", ptMetadata->tPipe1Info.AWB_RGain,
		ptMetadata->tPipe1Info.AWB_GGain, ptMetadata->tPipe1Info.AWB_BGain);
	misp_info("BlkOst(R,G,B): (%d, %d, %d).", ptMetadata->tPipe1Info.BlackOffset_R,
		ptMetadata->tPipe1Info.BlackOffset_G, ptMetadata->tPipe1Info.BlackOffset_B);
	misp_info("Crop(x,y,w,h): (%d, %d, %d, %d).", ptMetadata->tPipe1Info.Crop_X,
		ptMetadata->tPipe1Info.Crop_Y, ptMetadata->tPipe1Info.Crop_Width,
		ptMetadata->tPipe1Info.Crop_Height);
	misp_info("Scalar(w,h): (%d, %d).", ptMetadata->tPipe1Info.ScalarWidth,
		ptMetadata->tPipe1Info.ScalarHeight);
	misp_info("VCM(macro,infinity,curStep,status): (%d, %d, %d, %d).",
		ptMetadata->tPipe1Info.VCM_macro, ptMetadata->tPipe1Info.VCM_infinity,
		ptMetadata->tPipe1Info.VCM_CurStep, ptMetadata->tPipe1Info.Depth_VCMStatus);

	misp_info("---=== Metadata:Depth Info ===----");
	misp_info("Depth index: %d.", ptMetadata->tDpethInfo.DepthIndex);
	misp_info("Reference Frame Index: %d.", ptMetadata->tDpethInfo.ReferenceFrameIndex);
	misp_info("Depth(w,h,type) = (%d, %d, %d).", ptMetadata->tDpethInfo.DepthWidth,
		ptMetadata->tDpethInfo.DepthHeight, ptMetadata->tDpethInfo.DepthType);
	misp_info("VCM Step = (%d, %d).", ptMetadata->tDpethInfo.awDepth_VCMStep[0],
		ptMetadata->tDpethInfo.awDepth_VCMStep[1]);

	misp_info("---=== Metadata:SWDebug ===----");
	misp_info("ModuleBypass[0~ 5]: 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X.",
		ptMetadata->tSWDebugInfo.aucModuleByPass[0], ptMetadata->tSWDebugInfo.aucModuleByPass[1],
		ptMetadata->tSWDebugInfo.aucModuleByPass[2], ptMetadata->tSWDebugInfo.aucModuleByPass[3],
		ptMetadata->tSWDebugInfo.aucModuleByPass[4], ptMetadata->tSWDebugInfo.aucModuleByPass[5]);
	misp_info("ModuleBypass[6~12]: 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X.",
		ptMetadata->tSWDebugInfo.aucModuleByPass[6], ptMetadata->tSWDebugInfo.aucModuleByPass[7],
		ptMetadata->tSWDebugInfo.aucModuleByPass[8], ptMetadata->tSWDebugInfo.aucModuleByPass[9],
		ptMetadata->tSWDebugInfo.aucModuleByPass[10], ptMetadata->tSWDebugInfo.aucModuleByPass[11]);
	misp_info("DepthInSize: %dx%d.", ptMetadata->tSWDebugInfo.DepthInWidth,
		ptMetadata->tSWDebugInfo.DepthInHeight);
	misp_info("Pipe0Shading (w,h,inw,inh,woix,woiy,woiw,woih): (%d, %d, %d, %d, %d, %d, %d, %d).",
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHDTableW,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHDTableH,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHDNoWOIOutWidth,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHDNoWOIOutHeight,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHD_WOI_X,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHD_WOI_Y,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHD_WOI_Width,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[0].SHD_WOI_Height);
	misp_info("Pipe1Shading (w,h,inw,inh,woix,woiy,woiw,woih): (%d, %d, %d, %d, %d, %d, %d, %d).",
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHDTableW,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHDTableH,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHDNoWOIOutWidth,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHDNoWOIOutHeight,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHD_WOI_X,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHD_WOI_Y,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHD_WOI_Width,
		ptMetadata->tSWDebugInfo.tPipeSHDInfo[1].SHD_WOI_Height);
	misp_info("Pipe0DepthCalibWOI (x,y,w,h): (%d, %d, %d, %d).",
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[0].XBase,
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[0].YBase,
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[0].XLength,
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[0].YLength);
	misp_info("Pipe1DepthCalibWOI (x,y,w,h): (%d, %d, %d, %d).",
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[1].XBase,
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[1].YBase,
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[1].XLength,
		ptMetadata->tSWDebugInfo.tPipeDepthCaliWOI[1].YLength);
	misp_info("SysTime(us): %llu us.", ptMetadata->tSWDebugInfo.SysTimeus);
	misp_info("ProjectorStatus[0] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[0].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[0].Level);
	misp_info("ProjectorStatus[1] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[1].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[1].Level);
	misp_info("ProjectorStatus[2] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[2].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[2].Level);
	misp_info("ProjectorStatus[3] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[3].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[3].Level);
	misp_info("ProjectorStatus[4] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[4].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[4].Level);
	misp_info("ProjectorStatus[5] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[5].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[5].Level);
	misp_info("ProjectorStatus[6] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[6].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[6].Level);
	misp_info("ProjectorStatus[7] (on/off, level): (%d, %d).",
		ptMetadata->tSWDebugInfo.tProjectorStatus[7].TurnOn,
		ptMetadata->tSWDebugInfo.tProjectorStatus[7].Level);
	misp_info("GPIO_Dev on/off: (Dev0, Dev1) = (%d, %d).",
		ptMetadata->tSWDebugInfo.tGPIO_Device[0].TurnOn,
		ptMetadata->tSWDebugInfo.tGPIO_Device[1].TurnOn);
	misp_info("IDD0_ISRTimeus: %llu us.", ptMetadata->tSWDebugInfo.IDD0_ISRTimeus);
	misp_info("IDD1_ISRTimeus: %llu us.", ptMetadata->tSWDebugInfo.IDD1_ISRTimeus);

#ifdef NEW_ABS
	DualPathFrameSyncTimeDiff =
		abs((s64)(ptMetadata->tSWDebugInfo.IDD1_ISRTimeus - ptMetadata->tSWDebugInfo.IDD0_ISRTimeus));
#else
	DualPathFrameSyncTimeDiff =
		abs64((s64)(ptMetadata->tSWDebugInfo.IDD1_ISRTimeus - ptMetadata->tSWDebugInfo.IDD0_ISRTimeus));
#endif

	misp_info("DualPathFrameSyncTimeDiff: %llu us.", DualPathFrameSyncTimeDiff);

	misp_info("---=== Metadata:IQDebug ===----");
	misp_info("QMerge_Ver: %d.", ptMetadata->tIQDebugInfo.Qmerge_Ver);
	misp_info("Tool_Ver: %d. %d .%d.", ptMetadata->tIQDebugInfo.audTool_Ver[0],
		ptMetadata->tIQDebugInfo.audTool_Ver[1], ptMetadata->tIQDebugInfo.audTool_Ver[2]);
	misp_info("audTuning_Ver: %d. %d .%d.", ptMetadata->tIQDebugInfo.audTuning_Ver[0],
		ptMetadata->tIQDebugInfo.audTuning_Ver[1], ptMetadata->tIQDebugInfo.audTuning_Ver[2]);
	misp_info("Verify Debug: %d. %d.", ptMetadata->tIQDebugInfo.aucVerifyDebug[0],
		ptMetadata->tIQDebugInfo.aucVerifyDebug[1]);
	misp_info("2PDInfo: %d.", ptMetadata->tIQDebugInfo.uw2PD_Info);
	misp_info("EngineEnable: %d. %d. %d. %d. %d. %d. %d. %d.",
		ptMetadata->tIQDebugInfo.aucEngineEnable[0], ptMetadata->tIQDebugInfo.aucEngineEnable[1],
		ptMetadata->tIQDebugInfo.aucEngineEnable[2], ptMetadata->tIQDebugInfo.aucEngineEnable[3],
		ptMetadata->tIQDebugInfo.aucEngineEnable[4], ptMetadata->tIQDebugInfo.aucEngineEnable[5],
		ptMetadata->tIQDebugInfo.aucEngineEnable[6], ptMetadata->tIQDebugInfo.aucEngineEnable[7]);
	misp_info("CCM[0]: %d. %d. %d. %d. %d. %d. %d. %d. %d.", ptMetadata->tIQDebugInfo.awCCM[0][0],
		ptMetadata->tIQDebugInfo.awCCM[0][1], ptMetadata->tIQDebugInfo.awCCM[0][2],
		ptMetadata->tIQDebugInfo.awCCM[0][3], ptMetadata->tIQDebugInfo.awCCM[0][4],
		ptMetadata->tIQDebugInfo.awCCM[0][5], ptMetadata->tIQDebugInfo.awCCM[0][6],
		ptMetadata->tIQDebugInfo.awCCM[0][7], ptMetadata->tIQDebugInfo.awCCM[0][8]);
	misp_info("CCM[1]: %d. %d. %d .%d. %d. %d. %d. %d. %d.", ptMetadata->tIQDebugInfo.awCCM[1][0],
		ptMetadata->tIQDebugInfo.awCCM[1][1], ptMetadata->tIQDebugInfo.awCCM[1][2],
		ptMetadata->tIQDebugInfo.awCCM[1][3], ptMetadata->tIQDebugInfo.awCCM[1][4],
		ptMetadata->tIQDebugInfo.awCCM[1][5], ptMetadata->tIQDebugInfo.awCCM[1][6],
		ptMetadata->tIQDebugInfo.awCCM[1][7], ptMetadata->tIQDebugInfo.awCCM[1][8]);
	misp_info("ExpRatio: %d.", ptMetadata->tIQDebugInfo.ExpRatio);
	misp_info("ParaAddr[0][ 0~ 9]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][0], ptMetadata->tIQDebugInfo.audParaAddr[0][1],
		ptMetadata->tIQDebugInfo.audParaAddr[0][2], ptMetadata->tIQDebugInfo.audParaAddr[0][3],
		ptMetadata->tIQDebugInfo.audParaAddr[0][4], ptMetadata->tIQDebugInfo.audParaAddr[0][5],
		ptMetadata->tIQDebugInfo.audParaAddr[0][6], ptMetadata->tIQDebugInfo.audParaAddr[0][7],
		ptMetadata->tIQDebugInfo.audParaAddr[0][8], ptMetadata->tIQDebugInfo.audParaAddr[0][9]);
	misp_info("ParaAddr[0][10~19]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][10], ptMetadata->tIQDebugInfo.audParaAddr[0][11],
		ptMetadata->tIQDebugInfo.audParaAddr[0][12], ptMetadata->tIQDebugInfo.audParaAddr[0][13],
		ptMetadata->tIQDebugInfo.audParaAddr[0][14], ptMetadata->tIQDebugInfo.audParaAddr[0][15],
		ptMetadata->tIQDebugInfo.audParaAddr[0][16], ptMetadata->tIQDebugInfo.audParaAddr[0][17],
		ptMetadata->tIQDebugInfo.audParaAddr[0][18], ptMetadata->tIQDebugInfo.audParaAddr[0][19]);
	misp_info("ParaAddr[0][20~29]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][20], ptMetadata->tIQDebugInfo.audParaAddr[0][21],
		ptMetadata->tIQDebugInfo.audParaAddr[0][22], ptMetadata->tIQDebugInfo.audParaAddr[0][23],
		ptMetadata->tIQDebugInfo.audParaAddr[0][24], ptMetadata->tIQDebugInfo.audParaAddr[0][25],
		ptMetadata->tIQDebugInfo.audParaAddr[0][26], ptMetadata->tIQDebugInfo.audParaAddr[0][27],
		ptMetadata->tIQDebugInfo.audParaAddr[0][28], ptMetadata->tIQDebugInfo.audParaAddr[0][29]);
	misp_info("ParaAddr[0][30~39]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][30], ptMetadata->tIQDebugInfo.audParaAddr[0][31],
		ptMetadata->tIQDebugInfo.audParaAddr[0][32], ptMetadata->tIQDebugInfo.audParaAddr[0][33],
		ptMetadata->tIQDebugInfo.audParaAddr[0][34], ptMetadata->tIQDebugInfo.audParaAddr[0][35],
		ptMetadata->tIQDebugInfo.audParaAddr[0][36], ptMetadata->tIQDebugInfo.audParaAddr[0][37],
		ptMetadata->tIQDebugInfo.audParaAddr[0][38], ptMetadata->tIQDebugInfo.audParaAddr[0][39]);
	misp_info("ParaAddr[0][40~49]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][40], ptMetadata->tIQDebugInfo.audParaAddr[0][41],
		ptMetadata->tIQDebugInfo.audParaAddr[0][42], ptMetadata->tIQDebugInfo.audParaAddr[0][43],
		ptMetadata->tIQDebugInfo.audParaAddr[0][44], ptMetadata->tIQDebugInfo.audParaAddr[0][45],
		ptMetadata->tIQDebugInfo.audParaAddr[0][46], ptMetadata->tIQDebugInfo.audParaAddr[0][47],
		ptMetadata->tIQDebugInfo.audParaAddr[0][48], ptMetadata->tIQDebugInfo.audParaAddr[0][49]);
	misp_info("ParaAddr[0][50~59]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][50], ptMetadata->tIQDebugInfo.audParaAddr[0][51],
		ptMetadata->tIQDebugInfo.audParaAddr[0][52], ptMetadata->tIQDebugInfo.audParaAddr[0][53],
		ptMetadata->tIQDebugInfo.audParaAddr[0][54], ptMetadata->tIQDebugInfo.audParaAddr[0][55],
		ptMetadata->tIQDebugInfo.audParaAddr[0][56], ptMetadata->tIQDebugInfo.audParaAddr[0][57],
		ptMetadata->tIQDebugInfo.audParaAddr[0][58], ptMetadata->tIQDebugInfo.audParaAddr[0][59]);
	misp_info("ParaAddr[0][60~69]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][60], ptMetadata->tIQDebugInfo.audParaAddr[0][61],
		ptMetadata->tIQDebugInfo.audParaAddr[0][62], ptMetadata->tIQDebugInfo.audParaAddr[0][63],
		ptMetadata->tIQDebugInfo.audParaAddr[0][64], ptMetadata->tIQDebugInfo.audParaAddr[0][65],
		ptMetadata->tIQDebugInfo.audParaAddr[0][66], ptMetadata->tIQDebugInfo.audParaAddr[0][67],
		ptMetadata->tIQDebugInfo.audParaAddr[0][68], ptMetadata->tIQDebugInfo.audParaAddr[0][69]);
	misp_info("ParaAddr[0][70~79]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[0][70], ptMetadata->tIQDebugInfo.audParaAddr[0][71],
		ptMetadata->tIQDebugInfo.audParaAddr[0][72], ptMetadata->tIQDebugInfo.audParaAddr[0][73],
		ptMetadata->tIQDebugInfo.audParaAddr[0][74], ptMetadata->tIQDebugInfo.audParaAddr[0][75],
		ptMetadata->tIQDebugInfo.audParaAddr[0][76], ptMetadata->tIQDebugInfo.audParaAddr[0][77],
		ptMetadata->tIQDebugInfo.audParaAddr[0][78], ptMetadata->tIQDebugInfo.audParaAddr[0][79]);
	misp_info("ParaAddr[1][ 0~ 9]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][0], ptMetadata->tIQDebugInfo.audParaAddr[1][1],
		ptMetadata->tIQDebugInfo.audParaAddr[1][2], ptMetadata->tIQDebugInfo.audParaAddr[1][3],
		ptMetadata->tIQDebugInfo.audParaAddr[1][4], ptMetadata->tIQDebugInfo.audParaAddr[1][5],
		ptMetadata->tIQDebugInfo.audParaAddr[1][6], ptMetadata->tIQDebugInfo.audParaAddr[1][7],
		ptMetadata->tIQDebugInfo.audParaAddr[1][8], ptMetadata->tIQDebugInfo.audParaAddr[1][9]);
	misp_info("ParaAddr[1][10~19]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][10], ptMetadata->tIQDebugInfo.audParaAddr[1][11],
		ptMetadata->tIQDebugInfo.audParaAddr[1][12], ptMetadata->tIQDebugInfo.audParaAddr[1][13],
		ptMetadata->tIQDebugInfo.audParaAddr[1][14], ptMetadata->tIQDebugInfo.audParaAddr[1][15],
		ptMetadata->tIQDebugInfo.audParaAddr[1][16], ptMetadata->tIQDebugInfo.audParaAddr[1][17],
		ptMetadata->tIQDebugInfo.audParaAddr[1][18], ptMetadata->tIQDebugInfo.audParaAddr[1][19]);
	misp_info("ParaAddr[1][20~29]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][20], ptMetadata->tIQDebugInfo.audParaAddr[1][21],
		ptMetadata->tIQDebugInfo.audParaAddr[1][22], ptMetadata->tIQDebugInfo.audParaAddr[1][23],
		ptMetadata->tIQDebugInfo.audParaAddr[1][24], ptMetadata->tIQDebugInfo.audParaAddr[1][25],
		ptMetadata->tIQDebugInfo.audParaAddr[1][26], ptMetadata->tIQDebugInfo.audParaAddr[1][27],
		ptMetadata->tIQDebugInfo.audParaAddr[1][28], ptMetadata->tIQDebugInfo.audParaAddr[1][29]);
	misp_info("ParaAddr[1][30~39]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][30], ptMetadata->tIQDebugInfo.audParaAddr[1][31],
		ptMetadata->tIQDebugInfo.audParaAddr[1][32], ptMetadata->tIQDebugInfo.audParaAddr[1][33],
		ptMetadata->tIQDebugInfo.audParaAddr[1][34], ptMetadata->tIQDebugInfo.audParaAddr[1][35],
		ptMetadata->tIQDebugInfo.audParaAddr[1][36], ptMetadata->tIQDebugInfo.audParaAddr[1][37],
		ptMetadata->tIQDebugInfo.audParaAddr[1][38], ptMetadata->tIQDebugInfo.audParaAddr[1][39]);
	misp_info("ParaAddr[1][40~49]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][40], ptMetadata->tIQDebugInfo.audParaAddr[1][41],
		ptMetadata->tIQDebugInfo.audParaAddr[1][42], ptMetadata->tIQDebugInfo.audParaAddr[1][43],
		ptMetadata->tIQDebugInfo.audParaAddr[1][44], ptMetadata->tIQDebugInfo.audParaAddr[1][45],
		ptMetadata->tIQDebugInfo.audParaAddr[1][46], ptMetadata->tIQDebugInfo.audParaAddr[1][47],
		ptMetadata->tIQDebugInfo.audParaAddr[1][48], ptMetadata->tIQDebugInfo.audParaAddr[1][49]);
	misp_info("ParaAddr[1][50~59]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][50], ptMetadata->tIQDebugInfo.audParaAddr[1][51],
		ptMetadata->tIQDebugInfo.audParaAddr[1][52], ptMetadata->tIQDebugInfo.audParaAddr[1][53],
		ptMetadata->tIQDebugInfo.audParaAddr[1][54], ptMetadata->tIQDebugInfo.audParaAddr[1][55],
		ptMetadata->tIQDebugInfo.audParaAddr[1][56], ptMetadata->tIQDebugInfo.audParaAddr[1][57],
		ptMetadata->tIQDebugInfo.audParaAddr[1][58], ptMetadata->tIQDebugInfo.audParaAddr[1][59]);
	misp_info("ParaAddr[1][60~69]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][60], ptMetadata->tIQDebugInfo.audParaAddr[1][61],
		ptMetadata->tIQDebugInfo.audParaAddr[1][62], ptMetadata->tIQDebugInfo.audParaAddr[1][63],
		ptMetadata->tIQDebugInfo.audParaAddr[1][64], ptMetadata->tIQDebugInfo.audParaAddr[1][65],
		ptMetadata->tIQDebugInfo.audParaAddr[1][66], ptMetadata->tIQDebugInfo.audParaAddr[1][67],
		ptMetadata->tIQDebugInfo.audParaAddr[1][68], ptMetadata->tIQDebugInfo.audParaAddr[1][69]);
	misp_info("ParaAddr[1][70~79]: 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x. 0x%x.",
		ptMetadata->tIQDebugInfo.audParaAddr[1][70], ptMetadata->tIQDebugInfo.audParaAddr[1][71],
		ptMetadata->tIQDebugInfo.audParaAddr[1][72], ptMetadata->tIQDebugInfo.audParaAddr[1][73],
		ptMetadata->tIQDebugInfo.audParaAddr[1][74], ptMetadata->tIQDebugInfo.audParaAddr[1][75],
		ptMetadata->tIQDebugInfo.audParaAddr[1][76], ptMetadata->tIQDebugInfo.audParaAddr[1][77],
		ptMetadata->tIQDebugInfo.audParaAddr[1][78], ptMetadata->tIQDebugInfo.audParaAddr[1][79]);

	misp_info("---=== Metadata:Histogram ===----");
	misp_info("Hist_S[256~259]: [%d. %d. %d. %d].",
		ptMetadata->audHDRAEHistogram_Short[256], ptMetadata->audHDRAEHistogram_Short[257],
		ptMetadata->audHDRAEHistogram_Short[258], ptMetadata->audHDRAEHistogram_Short[259]);
	misp_info("Hist_L[256~259]: [%d. %d. %d. %d].", ptMetadata->audHDRAEHistogram_Long[256],
		ptMetadata->audHDRAEHistogram_Long[257], ptMetadata->audHDRAEHistogram_Long[258],
		ptMetadata->audHDRAEHistogram_Long[259]);

}

/**
 *\brief log metadata status
 *\param a_IsLog2File   [In] 0: print to dmesg. 1: log to file.
 *\return Error code
 */
errcode mini_isp_debug_metadata_info(void)
{
	errcode errcode = ERR_SUCCESS;
	u32 metadata_addr;
	CAPFLOWMGR_METADATA *ptMetadata = NULL;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	/* 1. read metadata structure addr */
	mini_isp_memory_read(DEBUG_INFO_METADATA_ADDR, (u8 *)&metadata_addr, 4);
	if (metadata_addr == 0) {
		misp_info("read meta data addr fail! L:%d", __LINE__);
		goto EXIT;
	}

	/* 2. allocate metadata buffer */
	ptMetadata = kzalloc(sizeof(CAPFLOWMGR_METADATA), GFP_KERNEL);
	if (ptMetadata == NULL) {
		errcode = -ENOMEM;
		misp_info("allocate metadata buffer fail!");
		goto EXIT;
	}
	/* 3. read metadata structure context */
	mini_isp_memory_read(metadata_addr, (u8 *)ptMetadata,
		sizeof(CAPFLOWMGR_METADATA));

	/* 4. show metadata context */
	mini_isp_debug_metadata_parser(ptMetadata);

	/* 5. free metadata buffer */
	kfree(ptMetadata);
EXIT:
	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_metadata_info);

static void mini_isp_debug_sensor_info_parser(
			FEC_SENSOR_REAL_INFO atRealSensorInfo[],
			u8 sensor_info_num, bool a_IsLog2File)
{
	u8 i = 0;
	u32 ret = 0;
	char *LogBuffer = NULL;
	char filename[128];
	struct file *fp = NULL;
	mm_segment_t fs = {0};
	u32 CurLogSize = 0;

	u64 SysTimeNow_ns, SysTimeNow_sec, Temp_u64;

	if (atRealSensorInfo == NULL) {
		misp_info("%s fail!", __func__);
		goto EXIT;
	}

	if (a_IsLog2File) {
		/* 200KB enougth for two hours */
		LogBuffer = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
		if (LogBuffer == NULL) {
			ret = -ENOMEM;
			misp_err("%s, LogBuffer allocate fail", __func__);
			goto EXIT;
		}

		ret = mini_isp_create_directory(MINIISP_INFO_DUMPLOCATION);

		snprintf(filename, 128, "%s/sensor_info_log",
			MINIISP_INFO_DUMPLOCATION);
	#if ENABLE_FILP_OPEN_API
		/* use file open */
	#else
		misp_info("Error! Currently not support file open api");
		misp_info("See define ENABLE_FILP_OPEN_API");
		goto EXIT;
	#endif
		/*Get current segment descriptor*/
		fs = get_fs();

		/*Set segment descriptor associated*/
		set_fs(get_ds());

		if (IS_ERR(fp)) {
			ret = PTR_ERR(fp);
			set_fs(fs);
			misp_err("%s open file failed. err: %d", __func__, ret);
			goto EXIT;
		}

		/* get linux system time */
		SysTimeNow_ns = local_clock();
		SysTimeNow_sec = SysTimeNow_ns;
		SysTimeNow_ns = do_div(SysTimeNow_sec, 1000000000);
		Temp_u64 = SysTimeNow_ns;
		do_div(Temp_u64, 1000);
		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
					"SysTime: [%5lld.%06lld]\n",
					SysTimeNow_sec, Temp_u64);

		for (i = 0; i < sensor_info_num; i++) {
			/* LogBuffer full, write the file */
			if (CurLogSize + 512 > LOG_BUF_SIZE) {
				vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);
				CurLogSize = 0;
			}

			CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
				"=============== Sensor Real Info ===============\n");

			for (i = 0; i < sensor_info_num; i++) {
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "---=== Sensor %d ===----\n", i);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "VTS: %d\n", atRealSensorInfo[i].VTS);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "HTS: %d\n", atRealSensorInfo[i].HTS);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"ImageWidth: %d\n", atRealSensorInfo[i].ImageWidth);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"ImageHeight: %d\n", atRealSensorInfo[i].ImageHeight);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"FrameTime: %d us\n", atRealSensorInfo[i].FrameTime);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"FrameRate: %d.%d fps\n",
							atRealSensorInfo[i].FrameRate/100, atRealSensorInfo[i].FrameRate % 100);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"ExpTime: %d us\n", atRealSensorInfo[i].ExpTime);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"Gain: %d.%dx\n", atRealSensorInfo[i].Gain/100, atRealSensorInfo[i].Gain % 100);
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
							"Mode: %d. (0~2: normal, master, slave)\n", atRealSensorInfo[i].SensorMode);
			}
		}

		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "\n");

		/* save final Log */
		vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);

		/*Restore segment descriptor*/
		set_fs(fs);
		filp_close(fp, NULL);

	} else {
		/* print to dmesg */
		misp_info("=============== Sensor Real Info ===============");
		for (i = 0; i < sensor_info_num; i++) {
			misp_info("---=== Sensor %d ===----", i);
			misp_info("VTS: %d.", atRealSensorInfo[i].VTS);
			misp_info("HTS: %d.", atRealSensorInfo[i].HTS);
			misp_info("ImageWidth: %d.", atRealSensorInfo[i].ImageWidth);
			misp_info("ImageHeight: %d.", atRealSensorInfo[i].ImageHeight);
			misp_info("FrameTime: %d us.", atRealSensorInfo[i].FrameTime);

			misp_info("FrameRate: %d.%d fps.", atRealSensorInfo[i].FrameRate/100,
				atRealSensorInfo[i].FrameRate % 100);
			misp_info("ExpTime: %d us.", atRealSensorInfo[i].ExpTime);
			misp_info("Gain: %d.%dx.", atRealSensorInfo[i].Gain/100, atRealSensorInfo[i].Gain % 100);
			misp_info("Mode: %d. (0~2: normal, master, slave)", atRealSensorInfo[i].SensorMode);
		}
	}

EXIT:
	if (LogBuffer != NULL)
		kfree(LogBuffer);
	return;
}

/**
 *\brief log sensor status
 *\param a_IsLog2File   [In] 0: print to dmesg. 1: log to file.
 *\return Error code
 */
errcode mini_isp_debug_sensor_info(bool a_IsLog2File)
{
	errcode errcode = ERR_SUCCESS;
	u32 SensorInfo_addr = 0;
	FEC_SENSOR_REAL_INFO atRealSensorInfo[2];
	u8 write_buffer[4];
	u8 sensor_info_num = 1;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	sensor_info_num = sizeof(atRealSensorInfo) / sizeof(FEC_SENSOR_REAL_INFO);
	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	/* 1. active ISP front-end debug mode. AL6100 stop AEC control */
	write_buffer[0] = 1;
	mini_isp_memory_write(DEBUG_INFO_FEC_DEBUG_MODE_ADDR, write_buffer, 1);
	msleep(300);

	/* 2. read Sensor Info addr */
	mini_isp_memory_read(DEBUG_INFO_SENSOR_REAL_INFO_ADDR, (u8 *)&SensorInfo_addr, 4);
	if (SensorInfo_addr == 0) {
		misp_info("read SensorInfo addr fail! L:%d", __LINE__);
		goto EXIT;
	}

	/* 3. read sensor info context */
	mini_isp_memory_read(SensorInfo_addr, (u8 *)atRealSensorInfo, sizeof(atRealSensorInfo));


	/* 4. close ISP front-end debug mode. */
	write_buffer[0] = 0;
	mini_isp_memory_write(DEBUG_INFO_FEC_DEBUG_MODE_ADDR, write_buffer, 1);

	/* 5. show sensor info context */
	mini_isp_debug_sensor_info_parser(atRealSensorInfo, sensor_info_num, a_IsLog2File);

EXIT:
	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_sensor_info);

static void mini_isp_debug_led_info_parser(
				PROJECTOR_INFO atLED_INFO[], u8 led_info_num, bool a_IsLog2File)
{
	u8 i = 0;
	u32 ret = 0;
	char *LogBuffer = NULL;
	char filename[128];
	struct file *fp = NULL;
	mm_segment_t fs = {0};

	u32 CurLogSize = 0;
	u64 SysTimeNow_ns, SysTimeNow_sec, Temp_u64;

	if (atLED_INFO == NULL) {
		misp_info("%s fail!", __func__);
		goto EXIT;
	}

	if (a_IsLog2File) {
		/* 200KB enougth for two hours */
		LogBuffer = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
		if (LogBuffer == NULL) {
			ret = -ENOMEM;
			misp_err("%s, LogBuffer allocate fail", __func__);
			goto EXIT;
		}

		ret = mini_isp_create_directory(MINIISP_INFO_DUMPLOCATION);

		snprintf(filename, 128, "%s/led_info_log", MINIISP_INFO_DUMPLOCATION);
	#if ENABLE_FILP_OPEN_API
		/* use file open */
	#else
		misp_info("Error! Currently not support file open api");
		misp_info("See define ENABLE_FILP_OPEN_API");
		kfree(LogBuffer);
		goto EXIT;
	#endif
		/*Get current segment descriptor*/
		fs = get_fs();

		/*Set segment descriptor associated*/
		set_fs(get_ds());

		if (IS_ERR(fp)) {
			ret = PTR_ERR(fp);
			set_fs(fs);
			kfree(LogBuffer);
			misp_err("%s open file failed. err: %d", __func__, ret);
			goto EXIT;
		}

		/* get linux system time */
		SysTimeNow_ns = local_clock();
		SysTimeNow_sec = SysTimeNow_ns;
		SysTimeNow_ns = do_div(SysTimeNow_sec, 1000000000);
		Temp_u64 = SysTimeNow_ns;
		do_div(Temp_u64, 1000);
		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
						"SysTime: [%5lld.%06lld]\n", SysTimeNow_sec, Temp_u64);
		for (i = 0; i < led_info_num; i++) {
			/* LogBuffer full, write the file */
			if (CurLogSize + 256 > LOG_BUF_SIZE) {
				vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);
				CurLogSize = 0;
			}

			if (atLED_INFO[i].Type == E_LED_PROJECTOR)
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "Projector ID: %d\n", i);
			else if (atLED_INFO[i].Type == E_LED_FLOOD)
				CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "Flood ID: %d\n", i);
			else {
				kfree(LogBuffer);
				misp_info("%s parser err", __func__);
				goto EXIT;
			}

			/* one line 128 byte at most */
			CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
						"level: %d, current: %d(mA), MaxCurrent: %d(mA)\n",
						atLED_INFO[i].Level, atLED_INFO[i].Current, atLED_INFO[i].MaxCurrent);
			CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
						"err: 0x%x, ErrStatus: 0x%x, ErrTemperature: %d\n",
						atLED_INFO[i].errCode, atLED_INFO[i].ErrStatus, atLED_INFO[i].Temperature);
		}

		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "\n");

		/* save final Log */
		vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);

		/*Restore segment descriptor*/
		set_fs(fs);
		filp_close(fp, NULL);

		if (LogBuffer)
			kfree(LogBuffer);
	} else {
		/* printf to dmesg */
		for (i = 0; i < led_info_num; i++) {
			if (atLED_INFO[i].Type == E_LED_PROJECTOR)
				misp_info("Projector ID: %d", i);
			else if (atLED_INFO[i].Type == E_LED_FLOOD)
				misp_info("Flood ID: %d", i);
			else {
				misp_info("%s parser err", __func__);
				goto EXIT;
			}

			misp_info("level: %d, current: %d(mA), MaxCurrent: %d(mA)",
				atLED_INFO[i].Level, atLED_INFO[i].Current, atLED_INFO[i].MaxCurrent);
			misp_info("err: 0x%x, ErrStatus: 0x%x, ErrTemperature: %d",
				atLED_INFO[i].errCode, atLED_INFO[i].ErrStatus, atLED_INFO[i].Temperature);
		}
	}


EXIT:
	return;
}

/**
 *\brief log projector & flood status
 *\param a_IsLog2File   [In] 0: print to dmesg. 1: log to file.
 *\return Error code
 */
errcode mini_isp_debug_led_info(bool a_IsLog2File)
{
	errcode errcode = ERR_SUCCESS;
	const u32 maxWaitNum = 180;
	u8 LED_INFO_NUM = 1;
	u8 write_buffer[4];
	u32 FEC_DEBUG_FLAG = 0;
	u32 led_info_addr = 0;
	u32 i = 0;
	PROJECTOR_INFO atLED_INFO[5];
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();
	LED_INFO_NUM = sizeof(atLED_INFO)/sizeof(PROJECTOR_INFO);

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}


	/* 1. Set Projector debug mode. AL6100 stop AEC control */
	write_buffer[0] = ADV_LOG_LIGHTING_PROJECTOR;
	mini_isp_memory_write(DEBUG_INFO_FEC_DEBUG_MODE_ADDR, write_buffer, 1);

	/* 2. wait projector log collect */
	for (i = 0; i < maxWaitNum; i++) {
		msleep(100);
		mini_isp_memory_read(DEBUG_INFO_FEC_DEBUG_MODE_ADDR, (u8 *)&FEC_DEBUG_FLAG, 4);
		if (FEC_DEBUG_FLAG == 0)
			break;

		if (i == maxWaitNum-1) {
			misp_info("wait projector log fail!");
			goto EXIT;
		} else
			misp_info("wait projector log..");
	}

	/* 3. Set flood debug mode. AL6100 stop AEC control */
	write_buffer[0] = ADV_LOG_LIGHTING_FLOOD;
	mini_isp_memory_write(DEBUG_INFO_FEC_DEBUG_MODE_ADDR, write_buffer, 1);

	/* 4. wait flood log collect */
	for (i = 0; i < maxWaitNum; i++) {
		msleep(100);
		mini_isp_memory_read(DEBUG_INFO_FEC_DEBUG_MODE_ADDR, (u8 *)&FEC_DEBUG_FLAG, 4);
		if (FEC_DEBUG_FLAG == 0)
			break;

		if (i == maxWaitNum-1) {
			misp_info("wait flood log fail!");
			goto EXIT;
		} else
			misp_info("wait flood log..");
	}

	/* 5. read led info addr */
	mini_isp_memory_read(DEBUG_INFO_LED_INFO_ADDR, (u8 *)&led_info_addr, 4);

	/* 6. read led info context */
	mini_isp_memory_read(led_info_addr, (u8 *)atLED_INFO, sizeof(atLED_INFO));

	/* 7. show led info context */
	mini_isp_debug_led_info_parser(atLED_INFO, LED_INFO_NUM, a_IsLog2File);
EXIT:
	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_led_info);

bool mipi_rx_fps_first_run = true;
bool mipi_rx_fps_task_active = false;
static struct task_struct *mipi_rx_fps_task;

static void mipi_rx_get_frame_count(u32 *rx0_count, u32 *rx1_count)
{
	#define RX0_FRAME_COUNT_REG 0xfff92010
	#define RX1_FRAME_COUNT_REG 0xfff95010

	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	if (rx0_count == NULL && rx1_count == NULL) {
		misp_info("%s input err", __func__);
		return;
	}

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	if (rx0_count != NULL) {
		mini_isp_register_read(RX0_FRAME_COUNT_REG, rx0_count);
		*rx0_count = *rx0_count & 0xFFFF; /* only use 0~15 bit*/
	}

	if (rx1_count != NULL) {
		mini_isp_register_read(RX1_FRAME_COUNT_REG, rx1_count);
		*rx1_count = *rx1_count & 0xFFFF; /* only use 0~15 bit*/
	}

	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);
}

int mipi_rx_fps_body(void *arg)
{
	u32 ret;
	struct timeval TimeNow = {0};
	struct timeval TimePrev = {0};
	u32 Rx0_count_Now = 0, Rx0_count_Prev = 0;
	u32 Rx1_count_Now = 0, Rx1_count_Prev = 0;
	u32 Rx0_fps_d, Rx0_fps_f, Rx1_fps_d, Rx1_fps_f;
	u32 Rx0_count_diff, Rx1_count_diff;
	u32 tmp;
	u32 Timediff_ms = 0;
	u64 TimeNow_ms, TimePrev_ms, Temp_u64;
	u64 SysTimeNow_sec;
	u64 SysTimeNow_ns;

	char filename[128];
	struct file *fp = NULL;
	mm_segment_t fs = {0};
	u32 CurLogSize = 0;
	char *LogBuffer = NULL;
	bool IsLog2File = *(bool *)arg;

	misp_info("%s S", __func__);

	if (IsLog2File) {
		LogBuffer = kzalloc(LOG_BUF_SIZE, GFP_KERNEL); /* 200KB enougth for two hours */
		if (LogBuffer == NULL) {
			misp_err("%s, LogBuffer allocate fail", __func__);
			goto EXIT;
		}

		ret = mini_isp_create_directory(MINIISP_INFO_DUMPLOCATION);

		snprintf(filename, 128, "%s/rx_fps_log", MINIISP_INFO_DUMPLOCATION);
	#if ENABLE_FILP_OPEN_API
		/* use file open */
	#else
		misp_info("Error! Currently not support file open api");
		misp_info("See define ENABLE_FILP_OPEN_API");
		goto EXIT;
	#endif
		/*Get current segment descriptor*/
		fs = get_fs();

		/*Set segment descriptor associated*/
		set_fs(get_ds());

		if (IS_ERR(fp)) {
			ret = PTR_ERR(fp);
			set_fs(fs);
			misp_err("%s open file failed. err: %d", __func__, ret);
			goto EXIT;
		}

	}

	while (!kthread_should_stop()) {
		do_gettimeofday(&TimeNow);
		mipi_rx_get_frame_count(&Rx0_count_Now, &Rx1_count_Now);

		if (mipi_rx_fps_first_run) {
			mipi_rx_fps_first_run = false;

			/* keep previous time record */
			memcpy(&TimePrev, &TimeNow, sizeof(struct timespec));
			Rx0_count_Prev = Rx0_count_Now;
			Rx1_count_Prev = Rx1_count_Now;

			msleep(2000);
			continue;
		}

		/* Calc  rx frame count fps */
		TimeNow_ms = (TimeNow.tv_sec * 1000) + (TimeNow.tv_usec / 1000);
		TimePrev_ms = (TimePrev.tv_sec * 1000) + (TimePrev.tv_usec / 1000);
		Timediff_ms = (u32) (TimeNow_ms - TimePrev_ms);

		/* X1000 for ms precision, X100 for the two digit after the decimal point*/
		Rx0_count_diff = Rx0_count_Now - Rx0_count_Prev;
		tmp = (Rx0_count_diff * 100 * 1000) / Timediff_ms;
		Rx0_fps_d = tmp / 100;
		Rx0_fps_f = tmp % 100;

		Rx1_count_diff = Rx1_count_Now - Rx1_count_Prev;
		tmp = (Rx1_count_diff * 100 * 1000) / Timediff_ms;
		Rx1_fps_d = tmp / 100;
		Rx1_fps_f = tmp % 100;

		/* get linux system time */
		SysTimeNow_ns = local_clock();
		SysTimeNow_sec = SysTimeNow_ns;
		SysTimeNow_ns = do_div(SysTimeNow_sec, 1000000000);
		if (IsLog2File) {
			/* LogBuffer full, write the file */
			if (CurLogSize + 128 > LOG_BUF_SIZE) {
				vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);
				CurLogSize = 0;
			}

			/* one line 128 byte at most */
			Temp_u64 = SysTimeNow_ns;
			do_div(Temp_u64, 1000);
			CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
					"[%5lld.%06lld]: Rx0 fps %d.%02d, Rx1 fps %d.%02d\n",
					SysTimeNow_sec, Temp_u64,
					Rx0_fps_d, Rx0_fps_f, Rx1_fps_d, Rx1_fps_f);
		} else{
			Temp_u64 = SysTimeNow_ns;
			do_div(Temp_u64, 1000);
			misp_info("%s test", __func__);
			misp_info("[%5lld.%06lld]: Rx0 fps %d.%02d, Rx1 fps %d.%02d",
				SysTimeNow_sec, Temp_u64,
				Rx0_fps_d, Rx0_fps_f, Rx1_fps_d, Rx1_fps_f);
		}

		/* keep previous time record */
		memcpy(&TimePrev, &TimeNow, sizeof(struct timeval));
		Rx0_count_Prev = Rx0_count_Now;
		Rx1_count_Prev = Rx1_count_Now;

		msleep(2000);
	}

	if (IsLog2File) {
		/* save final Log */
		vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);

		/*Restore segment descriptor*/
		set_fs(fs);
		filp_close(fp, NULL);
	}

EXIT:
	misp_info("%s E", __func__);
	if (LogBuffer != NULL)
		kfree(LogBuffer);

	return 0;
}

/**
 *\brief start log mini rx frame count fps
 *\param a_IsLog2File   [In] 0: print to dmesg. 1: log to file.
 *\return Error code
 */
int mini_isp_debug_mipi_rx_fps_start(bool a_IsLog2File)
{
	u32 ret = 0;
	static bool IsLog2File;

	IsLog2File = a_IsLog2File;

	/* create mipi_rx_fps_task */
	if (!mipi_rx_fps_task_active) {
		mipi_rx_fps_task = kthread_create(mipi_rx_fps_body, &IsLog2File, "mipi_rx_fps_task");
		if (IS_ERR(mipi_rx_fps_task)) {
			ret = PTR_ERR(mipi_rx_fps_task);
			mipi_rx_fps_task = NULL;
			misp_info("%s, mipi_rx_fps_task initial fail, 0x%x", __func__, ret);
			goto EXIT;
		}
		wake_up_process(mipi_rx_fps_task);

		mipi_rx_fps_task_active = true;
		mipi_rx_fps_first_run = true;
		misp_info("%s task create success!", __func__);
	} else {
		misp_info("%s already initial", __func__);
	}

EXIT:
	return ret;
}
EXPORT_SYMBOL(mini_isp_debug_mipi_rx_fps_start);

/**
 *\brief stop log mini rx frame count fps
 *\return none
 */
void mini_isp_debug_mipi_rx_fps_stop(void)
{
	/* delete mipi_rx_fps_task.
		Can't delete task if this task is done. otherwise, it will busy waiting */
	if (mipi_rx_fps_task_active) {
		if (mipi_rx_fps_task != NULL) {
			kthread_stop(mipi_rx_fps_task);
			mipi_rx_fps_task = NULL;
		}
		mipi_rx_fps_task_active = false;
		misp_info("%s kill task success", __func__);
	} else
		misp_info("%s no task to kill", __func__);
}
EXPORT_SYMBOL(mini_isp_debug_mipi_rx_fps_stop);

/**
 *\brief list GPIO status
 *\return none
 */
errcode mini_isp_debug_GPIO_Status(bool a_IsLog2File)
{
	errcode errcode = ERR_SUCCESS;
	u32 aGPIO_RegVal[GPIO_NUMBER] = {0};
	u32 aGPIO_mode[GPIO_NUMBER] = {0};
	u32 aGPIO_IO[GPIO_NUMBER] = {0};
	u32 aGPIO_data[GPIO_NUMBER] = {0};
	u32 i = 0;
	char filename[128];
	struct file *fp = NULL;
	mm_segment_t fs = {0};

	u32 CurLogSize = 0;
	char *LogBuffer = NULL;
	u64 SysTimeNow_ns, SysTimeNow_sec, Temp_u64;
	struct misp_global_variable *dev_global_variable;

	dev_global_variable = get_mini_isp_global_variable();

	mutex_lock(&dev_global_variable->busy_lock);

	if ((dev_global_variable->intf_status & INTF_SPI_READY) &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_A)) {
		misp_info("%s a_to_e", __func__);
		mini_isp_a_to_e();
	}

	if (a_IsLog2File) {
		LogBuffer = kzalloc(LOG_BUF_SIZE, GFP_KERNEL);
		if (LogBuffer == NULL) {
			misp_err("%s, LogBuffer allocate fail", __func__);
			goto EXIT;
		}

		errcode = mini_isp_create_directory(MINIISP_INFO_DUMPLOCATION);

		snprintf(filename, 128, "%s/GPIO_Status_log", MINIISP_INFO_DUMPLOCATION);

	#if ENABLE_FILP_OPEN_API
		/* use file open */
	#else
		misp_info("Error! Currently not support file open api");
		misp_info("See define ENABLE_FILP_OPEN_API");
		goto EXIT;
	#endif
		/*Get current segment descriptor*/
		fs = get_fs();

		/*Set segment descriptor associated*/
		set_fs(get_ds());

		if (IS_ERR(fp)) {
			errcode = PTR_ERR(fp);
			set_fs(fs);
			misp_err("%s open file failed. err: %d", __func__, errcode);
			goto EXIT;
		}

		/* get linux system time */
		SysTimeNow_ns = local_clock();
		SysTimeNow_sec = SysTimeNow_ns;
		SysTimeNow_ns = do_div(SysTimeNow_sec, 1000000000);
		Temp_u64 = SysTimeNow_ns;
		do_div(Temp_u64, 1000);
		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
						"SysTime: [%5lld.%06lld]\n", SysTimeNow_sec, Temp_u64);
		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128, "=== Check GPIO status ===\n");
		CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
				"SK1 pin\tMapping\t\tmode\t\tI/O\tHigh/Low\n");

		for (i = 0; i < GPIO_NUMBER; i++) {
			/* read GPIO status */
			mini_isp_register_read(g_atGPIO[i].addr, &aGPIO_RegVal[i]);
			aGPIO_mode[i] = aGPIO_RegVal[i] & 0x1F;
			if (aGPIO_mode[i] > 1)
				aGPIO_mode[i] = 1;
			aGPIO_IO[i] = (aGPIO_RegVal[i] >> 6) & 0x1;
			aGPIO_data[i] = (aGPIO_RegVal[i] >> 5) & 0x1;

			/* one line 128 byte at most */
			CurLogSize += snprintf(&LogBuffer[CurLogSize], 128,
					"%s\t%s      \t%s\t\t%s\t%s\n",
					g_atGPIO[i].name, g_atGPIO[i].mapping,
					aGPIO_mode[i] ? "Function" : "GPIO",
					aGPIO_IO[i] ? "GPO" : "GPI",
					aGPIO_data[i] ? "High" : "Low");

		}

		/* save final Log */
		vfs_write(fp, (char *)LogBuffer, CurLogSize, &fp->f_pos);

		/*Restore segment descriptor*/
		set_fs(fs);
		filp_close(fp, NULL);
	} else {
		misp_info("=== Check GPIO status ===");
		misp_info("SK1 pin\tMapping\t\tmode\t\tI/O\tHigh/Low");
		for (i = 0; i < GPIO_NUMBER; i++) {
			/* read GPIO status */
			mini_isp_register_read(g_atGPIO[i].addr, &aGPIO_RegVal[i]);
			aGPIO_mode[i] = aGPIO_RegVal[i] & 0x1F;
			if (aGPIO_mode[i] > 1)
				aGPIO_mode[i] = 1;
			aGPIO_IO[i] = (aGPIO_RegVal[i] >> 6) & 0x1;
			aGPIO_data[i] = (aGPIO_RegVal[i] >> 5) & 0x1;

			misp_info("%s\t%s      \t%s\t\t%s\t%s",
					g_atGPIO[i].name, g_atGPIO[i].mapping,
					aGPIO_mode[i] ? "Function" : "GPIO",
					aGPIO_IO[i] ? "GPO" : "GPI",
					aGPIO_data[i] ? "High" : "Low");
		}
	}

EXIT:
	if (dev_global_variable->intf_status & INTF_SPI_READY &&
		(dev_global_variable->altek_spi_mode == ALTEK_SPI_MODE_E)) {
		misp_info("%s e_to_a", __func__);
		mini_isp_e_to_a();
	}

	mutex_unlock(&dev_global_variable->busy_lock);

	if (LogBuffer != NULL)
		kfree(LogBuffer);

	return errcode;
}
EXPORT_SYMBOL(mini_isp_debug_GPIO_Status);
