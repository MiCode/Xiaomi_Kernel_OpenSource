/*
 * File: miniisp_customer_define.h
 * Description: miniISP customer define
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
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


#ifndef _MINIISP_CUSTOMER_DEFINE_H_
#define _MINIISP_CUSTOMER_DEFINE_H_

#include <linux/spi/spi.h>
/******Public Define******/

/*SPI MODE*/
#define SPI_MODE (SPI_MODE_3) /*SPI_MODE_3 | SPI_CS_HIGH*/

/* SPI SPEED */
/* SPI speed for generally using. */
/* Note: QCOM605 need to configure beyond 32000000Hz*/
/*       ,otherwise it may happen SPI timeout*/
#define SPI_BUS_SPEED 32000000
#define SPI_BUS_SPEED_BOOT 2000000 /* SPI Speed before boot code ready phase */
#define SPI_BUS_SPEED_LOW 3000000 /* For code persistence mode */

/*boot file location*/
#define BOOT_FILE_LOCATION "/data/firmware/miniBoot.bin"
/*basic code location*/
#define BASIC_FILE_LOCATION "/data/firmware/TBM_SK1.bin"
/*advanced code location*/
#define ADVANCED_FILE_LOCATION NULL
/*scenario table location*/
#define SCENARIO_TABLE_FILE_LOCATION "/data/firmware/SCTable.asb"

/*hdr qmerge data location*/
#define HDR_QMERGE_DATA_FILE_LOCATION "/data/firmware/HDR.bin"
/*irp0 qmerge data location*/
#define IRP0_QMERGE_DATA_FILE_LOCATION "/data/firmware/IRP0.bin"
/*irp1 qmerge data location*/
#define IRP1_QMERGE_DATA_FILE_LOCATION "/data/firmware/IRP1.bin"
/*pp map location*/
#define PP_MAP_FILE_LOCATION NULL/*"/system/etc/firmware/PPmap.bin"*/
/*depth qmerge data location*/
#define DPETH_QMERGE_DATA_FILE_LOCATION "/data/firmware/Depth.bin"

/*iq calibaration data location*/
#define IQCALIBRATIONDATA_FILE_LOCATION \
	"/data/misc/camera_otp/IQCalibrationData_Decrypt.bin"
	/*"/system/etc/firmware/PPmap.bin"*/
/*depth pack data location*/
#define DEPTHPACKDATA_FILE_LOCATION \
	"/data/misc/camera_otp/DepthPackData_Decrypt.bin"
	/*"/system/etc/firmware/PPmap.bin"*/

/*miniISP dump info save location*/
/*Add location folder where you let Altek debug info saving in your device*/
#define MINIISP_INFO_DUMPLOCATION "/data/local/tmp/"

/*miniISP bypass setting file location*/
/*Add location folder where you let Altek debug info saving in your device*/
#define MINIISP_BYPASS_SETTING_FILE_PATH "/data/firmware/"

/*define for gpio*/
/*vcc1 : if no use, set NULL*/
#define VCC1_GPIO NULL /*"vcc1-gpios"*/

/*vcc2 : if no use, set NULL*/
#define VCC2_GPIO NULL/*"vcc2-gpios"*/

/*vcc3 : if no use, set NULL*/
#define VCC3_GPIO NULL/*"vcc3-gpios"*/

/*reset*/
#define RESET_GPIO NULL  /*"reset-gpios"*/

/*irq*/
#define IRQ_GPIO "irq-gpios"

/*wp*/
#define WP_GPIO NULL /*"wp-gpios"*/

/*isp_clk : if no use, set NULL*/
#define ISP_CLK NULL /*"al6100_clk"*/

/*Enable SPI short length mode*/
#define SPI_SHORT_LEN_MODE (false)
#define SPI_BLOCK_LEN (14)
#define SPI_SHORT_LEN_MODE_WRITE_ENABLE (false)
#define SPI_SHORT_LEN_MODE_READ_ENABLE (true)

#define INTERRUPT_METHOD 0
#define POLLING_METHOD 1
/* choose INT or polling mechanism to get AL6100 status */
#define ISR_MECHANISM INTERRUPT_METHOD

#define EN_605_IOCTRL_INTF 1
/******Public Function Prototype******/
#endif
