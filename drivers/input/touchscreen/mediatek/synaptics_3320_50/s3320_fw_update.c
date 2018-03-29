/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

#include <mt_gpio.h>

#include "include/s3320_driver.h"

/* Variables for F34 functionality */
unsigned short SynaF34DataBase;
unsigned short SynaF34QueryBase;
unsigned short SynaF01DataBase;
unsigned short SynaF01CommandBase;
unsigned short SynaF01QueryBase;

unsigned short SynaF34Reflash_BlockNum;
unsigned short SynaF34Reflash_BlockData;
unsigned short SynaF34ReflashQuery_BootID;
unsigned short SynaF34ReflashQuery_FlashPropertyQuery;
unsigned short SynaF34ReflashQuery_BlockSize;
unsigned short SynaF34ReflashQuery_FirmwareBlockCount;
unsigned short SynaF34ReflashQuery_ConfigBlockCount;

unsigned char SynaF01Query43Length;

unsigned short SynaFirmwareBlockSize;
unsigned short SynaFirmwareBlockCount;
unsigned long SynaImageSize;

unsigned short SynaConfigBlockSize;
unsigned short SynaConfigBlockCount;
unsigned long SynaConfigImageSize;

unsigned short SynaBootloadID;

unsigned short SynaF34_FlashControl;
unsigned short SynaF34_FlashStatus;

unsigned char *SynafirmwareImgData;
unsigned char *SynaconfigImgData;
unsigned char *SynalockImgData;
unsigned int SynafirmwareImgVersion;

unsigned char *ConfigBlock;

enum FlashCommand {
	m_uF34ReflashCmd_FirmwareCrc = 0x01,	/* prior to V2 bootloaders */
	m_uF34ReflashCmd_FirmwareWrite = 0x02,
	m_uF34ReflashCmd_EraseAll = 0x03,
	m_uF34ReflashCmd_LockDown = 0x04,	/* V2 and later bootloaders */
	m_uF34ReflashCmd_ConfigRead = 0x05,
	m_uF34ReflashCmd_ConfigWrite = 0x06,
	m_uF34ReflashCmd_EraseUIConfig = 0x07,
	m_uF34ReflashCmd_Enable = 0x0F,
	m_uF34ReflashCmd_QuerySensorID = 0x08,
	m_uF34ReflashCmd_EraseBLConfig = 0x09,
	m_uF34ReflashCmd_EraseDisplayConfig = 0x0A,
};

char SynaFlashCommandStr[0x0C][0x20] = {
	"",
	"FirmwareCrc",
	"FirmwareWrite",
	"EraseAll",
	"LockDown",
	"ConfigRead",
	"ConfigWrite",
	"EraseUIConfig",
	"Enable",
	"QuerySensorID",
	"EraseBLConfig",
	"EraseDisplayConfig",
};

/* Variables for F34 functionality */







unsigned char *my_image_bin;
unsigned long my_image_size;


/* extern int synaptics_ts_write(struct i2c_client *client, u8 reg, u8 *buf, int len); */
/* extern int synaptics_ts_read(struct i2c_client *client, u8 reg, int num, u8 *buf); */


int FirmwareUpgrade(struct i2c_client *client, const char *fw_path, unsigned long fw_size,
		    unsigned char *fw_start)
{
	int ret = 0;
	int fd = -1;
	mm_segment_t old_fs = 0;
	struct stat fw_bin_stat;
	unsigned long read_bytes;

	if (unlikely(fw_path[0] != 0)) {
		old_fs = get_fs();
		set_fs(get_ds());

		fd = sys_open((const char __user *)fw_path, O_RDONLY, 0);
		if (fd < 0) {
			TPD_ERR("Can not read FW binary from %s\n", fw_path);
			ret = -EEXIST;
			goto read_fail;
		}

		ret = sys_newstat((char __user *)fw_path, (struct stat *)&fw_bin_stat);
		if (ret < 0) {
			TPD_ERR("Can not read FW binary stat from %s\n", fw_path);
			goto fw_mem_alloc_fail;
		}

		my_image_size = fw_bin_stat.st_size;
		my_image_bin = kzalloc(sizeof(char) * (my_image_size + 1), GFP_KERNEL);
		if (my_image_bin == NULL) {
			TPD_ERR("Can not allocate  memory\n");
			ret = -ENOMEM;
			goto fw_mem_alloc_fail;
		}

		read_bytes = sys_read(fd, (char __user *)my_image_bin, my_image_size);

		/* for checksum */
		*(my_image_bin + my_image_size) = 0xFF;

		TPD_LOG("Touch FW image read %ld bytes from %s\n", read_bytes, fw_path);

	} else {
		my_image_size = fw_size - 1;
		my_image_bin = (unsigned char *)(&fw_start[0]);
	}

#if 1				/* APK_TEST */
	ret = CompleteReflash(client);
	if (ret < 0)
		TPD_ERR("CompleteReflash fail\n");
#else
	ret = CompleteReflash_Lockdown(client);
	if (ret < 0)
		TPD_ERR("CompleteReflash_Lockdown fail\n");
#endif

	if (unlikely(fw_path[0] != 0))
		kfree(my_image_bin);

fw_mem_alloc_fail:
	sys_close(fd);
read_fail:
	set_fs(old_fs);

	return ret;
}

static int writeRMI(struct i2c_client *client, u8 uRmiAddress, u8 *data, unsigned int length)
{
	return synaptics_ts_write(client, uRmiAddress, data, length);
}

static int readRMI(struct i2c_client *client, u8 uRmiAddress, u8 *data, unsigned int length)
{
	return synaptics_ts_read(client, uRmiAddress, length, data);
}

bool CheckFlashStatus(enum FlashCommand command, struct i2c_client *client)
{
	unsigned char uData = 0;
	/* Read the "Program Enabled" bit of the F34 Control register, and proceed only if the */
	/* bit is set. */
	readRMI(client, SynaF34_FlashStatus, &uData, 1);

	/* if ((uData & 0x3F) != 0) */
	/* printf("Command %s failed.\n\tFlash status : 0x%X\n", SynaFlashCommandStr[command], uData & 0x3F); */

	return !(uData & 0x3F);
}

void SynaImageParser(struct i2c_client *client)
{
	TPD_LOG("%s\n", __func__);
	/* img file parsing */
	SynaImageSize = ((unsigned int)my_image_bin[0x08] |
			 (unsigned int)my_image_bin[0x09] << 8 |
			 (unsigned int)my_image_bin[0x0A] << 16 |
			 (unsigned int)my_image_bin[0x0B] << 24);
	SynafirmwareImgData = (unsigned char *)((&my_image_bin[0]) + 0x100);
	SynaconfigImgData = (unsigned char *)(SynafirmwareImgData + SynaImageSize);
	SynafirmwareImgVersion = (unsigned int)(my_image_bin[7]);

	switch (SynafirmwareImgVersion) {
	case 2:
		SynalockImgData = (unsigned char *)((&my_image_bin[0]) + 0xD0);
		break;
	case 3:
	case 4:
		SynalockImgData = (unsigned char *)((&my_image_bin[0]) + 0xC0);
		break;
	case 5:
	case 6:
		SynalockImgData = (unsigned char *)((&my_image_bin[0]) + 0xB0);
	default:
		break;
	}
}

void SynaBootloaderLock(struct i2c_client *client)
{
	unsigned short lockBlockCount = 0;
	unsigned char uData[2] = { 0, };
	unsigned short uBlockNum = 0;
	enum FlashCommand cmd;

	TPD_LOG("%s\n", __func__);

	if (my_image_bin[0x1E] == 0) {
		TPD_LOG("Skip lockdown process with this .img\n");
		return;
	}
	/* Check if device is in unlocked state */
	readRMI(client, (SynaF34QueryBase + 1), &uData[0], 1);

	/* Device is unlocked */
	if (uData[0] & 0x02) {
		TPD_LOG("Device unlocked. Lock it first...\n");
		/* Different bootloader version has different block count for the lockdown data */
		/* Need to check the bootloader version from the image file being reflashed */
		switch (SynafirmwareImgVersion) {
		case 2:
			lockBlockCount = 3;
			break;
		case 3:
		case 4:
			lockBlockCount = 4;
			break;
		case 5:
		case 6:
			lockBlockCount = 5;
			break;
		default:
			lockBlockCount = 0;
			break;
		}

		/* Write the lockdown info block by block */
		/* This reference code of lockdown process does not check for bootloader version */
		/* currently programmed on the ASIC against the bootloader version of the image to */
		/* be reflashed. Such case should not happen in practice. Reflashing cross different */
		/* bootloader versions is not supported. */
		for (uBlockNum = 0; uBlockNum < lockBlockCount; ++uBlockNum) {
			uData[0] = uBlockNum & 0xff;
			uData[1] = (uBlockNum & 0xff00) >> 8;

			/* Write Block Number */
			writeRMI(client, SynaF34Reflash_BlockNum, &uData[0], 2);

			/* Write Data Block */
			writeRMI(client, SynaF34Reflash_BlockData, SynalockImgData,
				 SynaFirmwareBlockSize);

			/* Move to next data block */
			SynalockImgData += SynaFirmwareBlockSize;

			/* Issue Write Lockdown Block command */
			cmd = m_uF34ReflashCmd_LockDown;
			writeRMI(client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

			/* Wait ATTN until device is done writing the block and is ready for the next. */
			SynaWaitForATTN(1000, client);
			CheckFlashStatus(cmd, client);
		}

		/* Enable reflash again to finish the lockdown process. */
		/* Since this lockdown process is part of the reflash process, we are enabling */
		/* reflash instead, rather than resetting the device to finish the unlock procedure. */
		SynaEnableFlashing(client);
	} else
		TPD_LOG("Device already locked.\n");
}


/* SynaScanPDT scans the Page Description Table (PDT) and sets up the necessary variables
 * for the reflash process. This function is a "slim" version of the PDT scan function in
 * in PDT.c, since only F34 and F01 are needed for reflash.
 */
void SynaScanPDT(struct i2c_client *client)
{
	unsigned char address = 0;
	unsigned char uData[2] = { 0, };
	unsigned char buffer[6] = { 0, };

	TPD_LOG("%s\n", __func__);

	for (address = 0xe9; address > 0xc0; address = address - 6) {
		readRMI(client, address, buffer, 6);

		switch (buffer[5]) {
		case 0x34:
			SynaF34DataBase = buffer[3];
			SynaF34QueryBase = buffer[0];
			break;
		case 0x01:
			SynaF01DataBase = buffer[3];
			SynaF01CommandBase = buffer[1];
			SynaF01QueryBase = buffer[0];
			break;
		}
	}

	SynaF34Reflash_BlockNum = SynaF34DataBase;
	SynaF34Reflash_BlockData = SynaF34DataBase + 1;
	SynaF34ReflashQuery_BootID = SynaF34QueryBase;
	SynaF34ReflashQuery_FlashPropertyQuery = SynaF34QueryBase + 1;
	SynaF34ReflashQuery_BlockSize = SynaF34QueryBase + 2;
	SynaF34ReflashQuery_FirmwareBlockCount = SynaF34QueryBase + 3;
	SynaF34_FlashControl = SynaF34DataBase + 2;
	SynaF34_FlashStatus = SynaF34DataBase + 3;

	readRMI(client, SynaF34ReflashQuery_FirmwareBlockCount, buffer, 4);
	SynaFirmwareBlockCount = buffer[0] | (buffer[1] << 8);
	SynaConfigBlockCount = buffer[2] | (buffer[3] << 8);

	readRMI(client, SynaF34ReflashQuery_BlockSize, &uData[0], 2);
	SynaConfigBlockSize = SynaFirmwareBlockSize = uData[0] | (uData[1] << 8);

	/* cleat ATTN */
	readRMI(client, (SynaF01DataBase + 1), buffer, 1);
}

/* SynaSetup scans the Page Description Table (PDT) and sets up the necessary variables
 * for the reflash process. This function is a "slim" version of the PDT scan function in
 * in PDT.c, since only F34 and F01 are needed for reflash.
 */








/* SynaInitialize sets up the reflahs process
 */
void SynaInitialize(struct i2c_client *client)
{
	u8 data;

	TPD_LOG("%s\n", __func__);
	TPD_LOG("\nInitializing Reflash Process...\n");

	data = 0x00;
	writeRMI(client, 0xff, &data, 1);

	SynaImageParser(client);

	SynaScanPDT(client);
}

/* SynaInitialize sets up the reflahs process
 */





	/* Set all interrupt enable */



/* SynaReadFirmwareInfo reads the F34 query registers and retrieves the block size and count
 * of the firmware section of the image to be reflashed
 */
void SynaReadFirmwareInfo(struct i2c_client *client)
{
	unsigned char uData[3] = { 0, };
	unsigned char product_id[11] = { 0, };
	int firmware_version = 0;

	TPD_LOG("%s\n", __func__);

	readRMI(client, SynaF01QueryBase + 11, product_id, 10);
	product_id[10] = '\0';
	TPD_LOG("Read Product ID %s\n", product_id);

	readRMI(client, SynaF01QueryBase + 18, uData, 3);
	firmware_version = uData[2] << 16 | uData[1] << 8 | uData[0];
	TPD_LOG("Read Firmware Info %d\n", firmware_version);

	CheckTouchControllerType(client);
	/* CheckFimrwareRevision();//APK_TEST */
}

/* SynaReadBootloadID reads the F34 query registers and retrieves the bootloader ID of the firmware
 */
void SynaReadBootloadID(struct i2c_client *client)
{
	unsigned char uData[2] = { 0, };

	TPD_LOG("%s\n", __func__);

	readRMI(client, SynaF34ReflashQuery_BootID, &uData[0], 2);
	SynaBootloadID = uData[0] | (uData[1] << 8);
}

/* SynaWriteBootloadID writes the bootloader ID to the F34 data register to unlock the reflash process
 */
void SynaWriteBootloadID(struct i2c_client *client)
{
	unsigned char uData[2];

	TPD_LOG("%s\n", __func__);

	uData[0] = SynaBootloadID % 0x100;
	uData[1] = SynaBootloadID / 0x100;

	writeRMI(client, SynaF34Reflash_BlockData, &uData[0], 2);
}

/* SynaReadFirmwareInfo reads the F34 query registers and retrieves the block size and count
 * of the firmware section of the image to be reflashed
 */





/* SynaReadConfigInfo reads the F34 query registers and retrieves the block size and count
 * of the configuration section of the image to be reflashed
 */




/* SynaReadBootloadID reads the F34 query registers and retrieves the bootloader ID of the firmware
 */


/* SynaWriteBootloadID writes the bootloader ID to the F34 data register to unlock the reflash process
 */



/* SynaEnableFlashing kicks off the reflash process
 */
void SynaEnableFlashing(struct i2c_client *client)
{
	unsigned char uStatus = 0;
	enum FlashCommand cmd;

	TPD_LOG("%s\n", __func__);
	TPD_LOG("Enable Reflash...\n");

	readRMI(client, SynaF01DataBase, &uStatus, 1);

/* APK_TEST */
	TPD_LOG("APK_TEST uStatus= 0x%02x\n", uStatus);

	if ((uStatus & 0x40) == 0 /*|| force */) {
		/* Reflash is enabled by first reading the bootloader ID from the firmware and write it back */
		SynaReadBootloadID(client);
		SynaWriteBootloadID(client);

		/* Write the "Enable Flash Programming command to F34 Control register */
		/* Wait for ATTN and then clear the ATTN. */
		cmd = m_uF34ReflashCmd_Enable;
		writeRMI(client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);
		SynaWaitForATTN(1000, client);

		/* I2C addrss may change */
		/* ConfigCommunication();//APK_TEST */

		/* Scan the PDT again to ensure all register offsets are correct */
		SynaScanPDT(client);

		/* Read the "Program Enabled" bit of the F34 Control register, and proceed only if the */
		/* bit is set. */
		CheckFlashStatus(cmd, client);
	}
}

/* SynaWaitForATTN waits for ATTN to be asserted within a certain time threshold.
 */
unsigned int SynaWaitForATTN(int timeout, struct i2c_client *client)
{
	unsigned char uStatus;
	/* int duration = 50; */
	/* int retry = timeout/duration; */
	/* int times = 0; */
#ifdef POLLING
	do {
		uStatus = 0x00;
		readRMI((SynaF01DataBase + 1), &uStatus, 1);
		if (uStatus != 0)
			break;
		Sleep(duration);
		times++;
	} while (times < retry);

	if (times == retry)
		return -1;
#else
	/* if (Line_WaitForAttention(timeout) == EErrorTimeout) */
	/* { */
	/* return -1; */
	/* } */
	int trial_us = 0;

#ifdef CONFIG_MTK_LEGACY
	while ((mt_get_gpio_in(GPIO_CTP_EINT_PIN) != 0) && (trial_us < (timeout * 1000))) {
		udelay(1);
		trial_us++;
	}

	if (mt_get_gpio_in(GPIO_CTP_EINT_PIN) != 0) {
		TPD_LOG("interrupt pin is busy...");
		return -EBUSY;
	}
#else
	while ((gpio_get_value(GPIO_CTP_EINT_PIN) != 0) && (trial_us < (timeout * 1000))) {
		udelay(1);
		trial_us++;
	}

	if (gpio_get_value(GPIO_CTP_EINT_PIN) != 0) {
		TPD_LOG("interrupt pin is busy...");
		return -EBUSY;
	}
#endif				/* CONFIG_MTK_LEGACY */

	readRMI(client, (SynaF01DataBase + 1), &uStatus, 1);
#endif
	return 0;
}

/* SynaWaitForATTN waits for ATTN to be asserted within a certain time threshold.
 */




/* SynaWaitATTN waits for ATTN to be asserted within a certain time threshold.
 * The function also checks for the F34 "Program Enabled" bit and clear ATTN accordingly.
 */




/* SynaEnableFlashing kicks off the reflash process
 */


	/* Reflash is enabled by first reading the bootloader ID from the firmware and write it back */

	/* Make sure Reflash is not already enabled */

	/* Clear ATTN */

/* APK_TEST */

		/* Write the "Enable Flash Programming command to F34 Control register */
		/* Wait for ATTN and then clear the ATTN. */
		/* Scan the PDT again to ensure all register offsets are correct */
		/* Read the "Program Enabled" bit of the F34 Control register, and proceed only if the */
		/* bit is set. */
			/* In practice, if uData!=0x80 happens for multiple counts, it indicates reflash */
			/* is failed to be enabled, and program should quit */
			/* APK_TEST */
			/* TPD_LOG("APK_TEST uData= 0x%02x\n", uData); */


/* SynaFinalizeReflash finalizes the reflash process
 */
void SynaFinalizeReflash(struct i2c_client *client)
{
	unsigned char uData;

	char deviceStatusStr[7][20] = {
		"0x00",
		"0x01",
		"0x02",
		"0x03",
		"config CRC failed",
		"firmware CRC failed",
		"CRC in progress\n"
	};

	TPD_LOG("%s\n", __func__);
	TPD_LOG("\nFinalizing Reflash...\n");

	/* Issue the "Reset" command to F01 command register to reset the chip */
	/* This command will also test the new firmware image and check if its is valid */
	uData = 1;
	writeRMI(client, SynaF01CommandBase, &uData, 1);

	/* After command reset, there will be 2 interrupt to be asserted */
	/* Simply sleep 150 ms to skip first attention */
	msleep(150);
	SynaWaitForATTN(1000, client);

	SynaScanPDT(client);

	readRMI(client, SynaF01DataBase, &uData, 1);

	if ((uData & 0x40) != 0) {
		TPD_LOG("\nDevice is in bootloader mode (status: %s).\n",
			deviceStatusStr[uData & 0xF]);
	} else {
		TPD_LOG("\nReflash Completed and Succeed.\n");
	}
}

/* SynaFlashFirmwareWrite writes the firmware section of the image block by block
 */
void SynaFlashFirmwareWrite(struct i2c_client *client)
{
	unsigned char *puFirmwareData = SynafirmwareImgData;
	unsigned char uData[2];
	unsigned short blockNum;
	enum FlashCommand cmd;

	TPD_LOG("%s\n", __func__);
	for (blockNum = 0; blockNum < SynaFirmwareBlockCount; ++blockNum) {
		if (blockNum == 0) {
			/* Block by blcok, write the block number and data to the corresponding F34 data registers */
			uData[0] = blockNum & 0xff;
			uData[1] = (blockNum & 0xff00) >> 8;
			writeRMI(client, SynaF34Reflash_BlockNum, &uData[0], 2);
		}

		writeRMI(client, SynaF34Reflash_BlockData, puFirmwareData, SynaFirmwareBlockSize);
		puFirmwareData += SynaFirmwareBlockSize;

		/* Issue the "Write Firmware Block" command */
		cmd = m_uF34ReflashCmd_FirmwareWrite;
		writeRMI(client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

		SynaWaitForATTN(1000, client);
		CheckFlashStatus(cmd, client);
/* #ifdef SHOW_PROGRESS */
#if 1				/* APK_TEST */
		if (blockNum % 100 == 0)
			TPD_LOG("blk %d / %d\n", blockNum, SynaFirmwareBlockCount);
#endif
	}
/* #ifdef SHOW_PROGRESS */
#if 1				/* APK_TEST */
	TPD_LOG("blk %d / %d\n", SynaFirmwareBlockCount, SynaFirmwareBlockCount);
#endif
}

/* SynaFlashFirmwareWrite writes the firmware section of the image block by block
 */
void SynaFlashConfigWrite(struct i2c_client *client)
{
	unsigned char *puConfigData = SynaconfigImgData;
	unsigned char uData[2];
	unsigned short blockNum;
	enum FlashCommand cmd;

	TPD_LOG("%s\n", __func__);
	for (blockNum = 0; blockNum < SynaConfigBlockCount; ++blockNum) {
		/* Block by blcok, write the block number and data to the corresponding F34 data registers */
		uData[0] = blockNum & 0xff;
		uData[1] = (blockNum & 0xff00) >> 8;
		writeRMI(client, SynaF34Reflash_BlockNum, &uData[0], 2);

		writeRMI(client, SynaF34Reflash_BlockData, puConfigData, SynaConfigBlockSize);
		puConfigData += SynaConfigBlockSize;

		/* Issue the "Write Config Block" command */
		cmd = m_uF34ReflashCmd_ConfigWrite;
		writeRMI(client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

		SynaWaitForATTN(1000, client);
		CheckFlashStatus(cmd, client);
/* #ifdef SHOW_PROGRESS */
#if 1
		if (blockNum % 100 == 0)
			TPD_LOG("blk %d / %d\n", blockNum, SynaConfigBlockCount);
#endif
	}
/* #ifdef SHOW_PROGRESS */
#if 1
	TPD_LOG("blk %d / %d\n", SynaConfigBlockCount, SynaConfigBlockCount);
#endif
}

/* SynaProgramFirmware prepares the firmware writing process
 */
void SynaProgramFirmware(struct i2c_client *client)
{
	TPD_LOG("%s\n", __func__);
	TPD_LOG("\nProgram Firmware Section...\n");

	eraseAllBlock(client);

	SynaFlashFirmwareWrite(client);

	SynaFlashConfigWrite(client);
}

/* SynaProgramFirmware prepares the firmware writing process
 */
void SynaUpdateConfig(struct i2c_client *client)
{
	TPD_LOG("%s\n", __func__);
	TPD_LOG("\nUpdate Config Section...\n");

	EraseConfigBlock(client);

	SynaFlashConfigWrite(client);
}

/* EraseConfigBlock erases the config block
*/
void eraseAllBlock(struct i2c_client *client)
{
	enum FlashCommand cmd;

	TPD_LOG("%s\n", __func__);
	/* Erase of config block is done by first entering into bootloader mode */
	SynaReadBootloadID(client);
	SynaWriteBootloadID(client);

	/* Command 7 to erase config block */
	cmd = m_uF34ReflashCmd_EraseAll;
	writeRMI(client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

	SynaWaitForATTN(6000, client);
	CheckFlashStatus(cmd, client);
}

/* EraseConfigBlock erases the config block
*/
void EraseConfigBlock(struct i2c_client *client)
{
	enum FlashCommand cmd;

	TPD_LOG("%s\n", __func__);
	/* Erase of config block is done by first entering into bootloader mode */
	SynaReadBootloadID(client);
	SynaWriteBootloadID(client);

	/* Command 7 to erase config block */
	cmd = m_uF34ReflashCmd_EraseUIConfig;
	writeRMI(client, SynaF34_FlashControl, (unsigned char *)&cmd, 1);

	SynaWaitForATTN(2000, client);
	CheckFlashStatus(cmd, client);
}

/* This function is to check the touch controller type of the touch controller matches with the firmware image */
bool CheckTouchControllerType(struct i2c_client *client)
{
	int ID = 0;
	char buffer[5] = { 0, };
	char controllerType[20] = { 0, };
	unsigned char uData[4] = { 0, };

	TPD_LOG("%s\n", __func__);
	readRMI(client, (SynaF01QueryBase + 22), &SynaF01Query43Length, 1);	/* 43 */

	if ((SynaF01Query43Length & 0x0f) > 0) {
		readRMI(client, (SynaF01QueryBase + 23), &uData[0], 1);
		if (uData[0] & 0x01) {
			readRMI(client, (SynaF01QueryBase + 17), &uData[0], 2);

			ID = ((int)uData[0] | ((int)uData[1] << 8));
			/* sprintf_s(buffer, "%d\0", ID);//APK_TEST */

			if (strstr(controllerType, buffer) != 0)
				return true;
			return false;
		} else
			return false;
	} else
		return false;
}

#if 0				/* APK_TEST */
bool CheckFimrwareRevision(struct i2c_client *client)
{
	unsigned char uData[16];
	char revision[106];

	TPD_LOG("%s\n", __func__);
	readRMI((SynaF01QueryBase + 28 + SynaF01Query43Length), &uData[0], 16);

	for (int i = 0; i < 0; i++) {
		while (uData[i] != NULL)
			revision[i] = char (uData[0]);
	}

	if (strcmp(revision, FW_REVISION) == 0)
		return true;
	return false;
}
#endif

/* SynaProgramConfiguration writes the configuration section of the image block by block
 */




		/* Block by blcok, write the block number and data to the corresponding F34 data registers */

		/* Issue the "Write Configuration Block" command */

/* SynaFinalizeReflash finalizes the reflash process
 */


	/* Issue the "Reset" command to F01 command register to reset the chip */
	/* This command will also test the new firmware image and check if its is valid */


	/* Sanity check that the reflash process is still enabled */


	/* Check if the "Program Enabled" bit in F01 data register is cleared */
	/* Reflash is completed, and the image passes testing when the bit is cleared */

	/* Rescan PDT the update any changed register offsets */



/* SynaFlashFirmwareWrite writes the firmware section of the image block by block
 */

		/* Block by blcok, write the block number and data to the corresponding F34 data registers */


		/* Issue the "Write Firmware Block" command */



/* SynaProgramFirmware prepares the firmware writing process
 */









/* eraseConfigBlock erases the config block
*/

	/* Erase of config block is done by first entering into bootloader mode */

	/* Command 7 to erase config block */



/* CRC_Calculate illustates how to calculate a checksum from the config block data. */
/* With DS4, the config block checksum is calculated and applies towards the end of */
/* the config block data automatically */
/* Variable data to this function represents the data only portion of the config block */
/* Varaible len represents the length of the variable data. */





	/* Check if device is in unlocked state */

	/* Device is unlocked */
		/* Different bootloader version has different block count for the lockdown data */
		/* Need to check the bootloader version from the image file being reflashed */

		/* Write the lockdown info block by block */
		/* This reference code of lockdown process does not check for bootloader version */
		/* currently programmed on the ASIC against the bootloader version of the image to */
		/* be reflashed. Such case should not happen in practice. Reflashing cross different */
		/* bootloader versions is not supported. */

			/* Write Block Number */

			/* Write Data Block */

			/* Move to next data block */

			/* Issue Write Lockdown Block command */

			/* Wait ATTN until device is done writing the block and is ready for the next. */

		/* Enable reflash again to finish the lockdown process. */
		/* Since this lockdown process is part of the reflash process, we are enabling */
		/* reflash instead, rather than resetting the device to finish the unlock procedure. */


/* ConfigBlockReflash reflashes the config block only
*/







	/* Check if device is in unlocked state */

	/* Device is unlocked */
	   /* Do not reflash config block if not locked. */





/* CompleteReflash reflashes the entire user image, including the configuration block and firmware
*/
int CompleteReflash(struct i2c_client *client)
{
	bool bFlashAll = true;

	SynaInitialize(client);

	SynaReadFirmwareInfo(client);

	SynaEnableFlashing(client);

	SynaBootloaderLock(client);

	if (bFlashAll)
		SynaProgramFirmware(client);
	else
		SynaUpdateConfig(client);

	SynaFinalizeReflash(client);

	return 0;
}

/* CompleteReflash reflashes the entire user image, including the configuration block and firmware
*/
