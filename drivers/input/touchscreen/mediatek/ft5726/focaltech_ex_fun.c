/************************************************************************
* File Name: focaltech_ex_fun.c
*
* Author:
*
* Created: 2015-01-01
*
* Abstract: function for fw upgrade, adb command, create apk second entrance
*
************************************************************************/

#include <linux/mount.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include "tpd_custom_fts.h"
#include "focaltech_ex_fun.h"

int fts_5x22_ctpm_fw_upgrade(struct i2c_client *client,
				u8 *pbt_buf, u32 dw_length);

struct i2c_client *G_Client;
static struct mutex g_device_mutex;
/* 0 for no apk upgrade, 1 for apk upgrade */
int apk_debug_flag = 0;

static unsigned char CTPM_FW_GIS_7mm[] = {
	#include "FT5726_GIS_0x07_20150925_app.h"
};

static unsigned char CTPM_FW_GIS_4mm[] = {
	#include "FT5726_GIS_0x09_20151016_app.h"
};

/************************************************************************
* Name: fts_ctpm_auto_clb
* Brief:  auto calibration
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int fts_ctpm_auto_clb(struct i2c_client *client)
{
	unsigned char uc_temp = 0x00;
	unsigned char i = 0;

	/*start auto CLB */
	msleep(200);

	fts_write_reg(client, 0, FTS_FACTORYMODE_VALUE);
	/*make sure already enter factory mode */
	msleep(100);
	/*write command to start calibration */
	fts_write_reg(client, 2, 0x4);
	msleep(300);

	for (i = 0; i < 100; i++) {
		fts_read_reg(client, 0, &uc_temp);
		/*return to normal mode, calibration finish*/
		if (0x0 == ((uc_temp&0x70)>>4))
			break;

		msleep(20);
	}

	/*calibration OK*/

	/*goto factory mode for store*/
	fts_write_reg(client, 0, 0x40);
	/*make sure already enter factory mode*/
	msleep(200);
	/*store CLB result*/
	fts_write_reg(client, 2, 0x5);
	msleep(300);
	/*return to normal mode */
	fts_write_reg(client, 0, FTS_WORKMODE_VALUE);
	msleep(300);

	/*store CLB result OK */
	return 0;
}

/***********************************************************************
* Name: fts_ctpm_fw_upgrade_with_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output: no
* Return: fail <0
***********************************************************************/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	int i;
	int fw_len;

	if (hw_rev == 0) {
		fw_len = sizeof(CTPM_FW_GIS_7mm);
		pbt_buf = CTPM_FW_GIS_7mm;
	} else {
		fw_len = sizeof(CTPM_FW_GIS_4mm);
		pbt_buf = CTPM_FW_GIS_4mm;
	}

	if (fw_len < 8 || fw_len > 54 * 1024) {
		dev_err(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	/*call the upgrade function*/
	for (i = 0; i < UPGRADE_RETRY_LOOP; i++) {
		i_ret = fts_5x22_ctpm_fw_upgrade(client, pbt_buf, fw_len);
		if (i_ret != 0) {
			dev_err(&client->dev,
			"[Focal] FW upgrade failed, err=%d.\n", i_ret);
		} else {
			#ifdef AUTO_CLB
				fts_ctpm_auto_clb(client);
			#endif
			break;
		}
	}

	return i_ret;
}

/************************************************************************
* Name: fts_ctpm_get_i_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: .i file version
***********************************************************************/
u8 fts_ctpm_get_i_file_ver(void)
{
	u8 *pbt_buf = NULL;
	u16 ui_sz;

	if (hw_rev == 0) {
		ui_sz = sizeof(CTPM_FW_GIS_7mm);
		pbt_buf = CTPM_FW_GIS_7mm;
	} else {
		ui_sz = sizeof(CTPM_FW_GIS_4mm);
		pbt_buf = CTPM_FW_GIS_4mm;
	}

	if (ui_sz > 2)
		return pbt_buf[0x10a];

	return 0x00;	/*default value */
}

/************************************************************************
* Name: fts_ctpm_auto_upgrade
* Brief:  auto upgrade
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_host_fm_ver = FTS_REG_FW_VER;
	u8 uc_tp_fm_ver;
	int i_ret;

	fts_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver);
	uc_host_fm_ver = fts_ctpm_get_i_file_ver();

	pr_info("[Focal] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
				uc_tp_fm_ver, uc_host_fm_ver);

	if ((uc_tp_fm_ver != uc_host_fm_ver) || (uc_tp_fm_ver >= 0xe0) ||
		(uc_tp_fm_ver == 0x00) || (uc_tp_fm_ver == FTS_REG_FW_VER)) {
		msleep(100);
		i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
		if (i_ret == 0)	{
			msleep(300);
			uc_host_fm_ver = fts_ctpm_get_i_file_ver();
			pr_info("[Focal] Auto upgrade to new version 0x%x\n",
					uc_host_fm_ver);
		} else {
			pr_err("[Focal] Auto upgrade failed ret=%d.\n", i_ret);
			return -EIO;
		}
	}
	return 0;
}

/************************************************************************
* Name: hid_to_i2c
* Brief:  HID to I2C
* Input: i2c info
* Output: no
* Return: fail =0
***********************************************************************/
int hid_to_i2c(struct i2c_client *client)
{
	u8 auc_i2c_write_buf[5] = {0};
	int bRet = 0;

	auc_i2c_write_buf[0] = 0xeb;
	auc_i2c_write_buf[1] = 0xaa;
	auc_i2c_write_buf[2] = 0x09;

	ftxxxx_i2c_Write(client, auc_i2c_write_buf, 3);
	msleep(20);

	auc_i2c_write_buf[0] = 0;
	auc_i2c_write_buf[1] = 0;
	auc_i2c_write_buf[2] = 0;
	ftxxxx_i2c_Read(client, auc_i2c_write_buf, 0, auc_i2c_write_buf, 3);

	if ((0xeb == auc_i2c_write_buf[0]) &&
		(0xaa == auc_i2c_write_buf[1]) && (0x08 == auc_i2c_write_buf[2])) {
		bRet = 1;
	} else
		bRet = 0;

	return bRet;
}

/************************************************************************
* Name: fts_5x22_ctpm_fw_upgrade
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_5x22_ctpm_fw_upgrade(struct i2c_client *client,
					u8 *pbt_buf, u32 dw_length)
{
	u8 reg_val[4] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 length;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;
	u8 bt_ecc_check;
	int i_ret;

	i_ret = hid_to_i2c(client);
	if (i_ret == 0)
		pr_info("[Focal] hid change to i2c fail\n");

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/*********Step 1:Reset CTPM *****/
		pr_info("[Focal] Step 1: reset CTPM\n");
		/*write 0xaa to register 0xfc */
		i_ret = fts_write_reg(client, 0xfc, FTS_UPGRADE_AA);
		msleep(IC_INFO_DELAY_AA);

		i_ret = fts_write_reg(client, 0xfc, FTS_UPGRADE_55);
		msleep(250);
		/*********Step 2:Enter upgrade mode *****/
		pr_info("[Focal] Step 2: Enter upgrade mode\n");
		i_ret = hid_to_i2c(client);
		if (i_ret == 0) {
			pr_info("[Focal] hid change to i2c fail\n");
			continue;
		}
		usleep_range(10000, 11000);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			pr_info("[Focal] failed writing 0x55 and 0xaa\n");
			continue;
		}
		/*********Step 3:check READ-ID***********************/
		pr_info("[Focal] Step 3: Check read-ID\n");
		msleep(20);
		auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = 0x00;
		auc_i2c_write_buf[2] = 0x00;
		auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;

		ftxxxx_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if (reg_val[0] == IC_INFO_UPGRADE_ID1 &&
					reg_val[1] == IC_INFO_UPGRADE_ID2) {
			/*read from bootloader FW*/
			pr_info("[Focal] Check OK, CTPM ID1=0x%x, ID2=0x%x\n",
				reg_val[0], reg_val[1]);
			break;
		}
		dev_err(&client->dev,
		"[Focal]Step3:CTPM ID,ID1 = 0x%x, ID2 = 0x%x\n",
		reg_val[0], reg_val[1]);
		continue;
	}

	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;

	/*Step 4:erase app and panel paramenter area*/
	pr_info("[Focal] Step 4:erase app and panel paramenter area\n");
	auc_i2c_write_buf[0] = 0x61;
	ftxxxx_i2c_Write(client, auc_i2c_write_buf, 1);	/* erase app area */

	msleep(2000);
	for (i = 0; i < 15; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);
		if (0xF0 == reg_val[0] && 0xAA == reg_val[1])
			break;
		msleep(50);
	}

	/*write bin file length to FW bootloader.*/
	auc_i2c_write_buf[0] = 0xB0;
	auc_i2c_write_buf[1] = (u8) ((dw_length >> 16) & 0xFF);
	auc_i2c_write_buf[2] = (u8) ((dw_length >> 8) & 0xFF);
	auc_i2c_write_buf[3] = (u8) (dw_length & 0xFF);
	ftxxxx_i2c_Write(client, auc_i2c_write_buf, 4);

	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	bt_ecc_check = 0;
	pr_info("[Focal] Step 5:write firmware(FW) to ctpm flash\n");
	/*dw_length = dw_length - 8;*/
	temp = 0;
	packet_number = (dw_length) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) {
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		length = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (length >> 8);
		packet_buf[5] = (u8) length;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) {
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc_check ^= pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		/* pr_info("[FTS][%s] bt_ecc = %x\n", __func__, bt_ecc); */
		if (bt_ecc != bt_ecc_check)
			pr_info("[Focal] Host csum error bt_ecc_check = %x\n",
								bt_ecc_check);

		ftxxxx_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1,
							reg_val, 2);
			/*
			pr_info("[Focal][%s] reg_val[0] = %x reg_val[1] = %x\n",
					__func__, reg_val[0], reg_val[1]);
			*/
			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
				break;

			msleep(20);
		}
	}

	if ((dw_length) % FTS_PACKET_LENGTH > 0) {
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_length) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;
		for (i = 0; i < temp; i++) {
			packet_buf[6 + i] =
				pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc_check ^=
				pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		ftxxxx_i2c_Write(client, packet_buf, temp + 6);

		/* pr_info("[Focal][%s] bt_ecc = %x\n", __func__, bt_ecc); */
		if (bt_ecc != bt_ecc_check)
			pr_info("[Focal]Host checksum error bt_ecc_check = %x\n",
								bt_ecc_check);

		for (i = 0; i < 30; i++) {
			auc_i2c_write_buf[0] = 0x6a;
			reg_val[0] = reg_val[1] = 0x00;
			ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1,
							reg_val, 2);
			/*
			pr_info("[Focal][%s] reg_val[0] = %x reg_val[1] = %x\n",
					__func__, reg_val[0], reg_val[1]);
			*/
			if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
				break;

			msleep(20);
		}
	}
	msleep(50);
	/*********Step 6: read out checksum***********************/
	/*send the opration head */
	pr_info("[Focal] Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = 0x64;
	ftxxxx_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(300);

	temp = 0;
	auc_i2c_write_buf[0] = 0x65;
	auc_i2c_write_buf[1] = (u8)(temp >> 16);
	auc_i2c_write_buf[2] = (u8)(temp >> 8);
	auc_i2c_write_buf[3] = (u8)(temp);
	temp = dw_length;
	auc_i2c_write_buf[4] = (u8)(temp >> 8);
	auc_i2c_write_buf[5] = (u8)(temp);
	i_ret = ftxxxx_i2c_Write(client, auc_i2c_write_buf, 6);
	msleep(dw_length/256);

	for (i = 0; i < 100; i++) {
		auc_i2c_write_buf[0] = 0x6a;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

		if ((0xF0 == reg_val[0]) && (0x55 == reg_val[1])) {
			pr_info("[Focal]--reg_val[0]=%02x reg_val[0]=%02x\n",
						reg_val[0], reg_val[1]);
			break;
		}
		usleep_range(10000, 11000);
	}

	auc_i2c_write_buf[0] = 0x66;
	ftxxxx_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) {
		dev_err(&client->dev,
			"[Focal]--ecc error! FW=%02x bt_ecc=%02x\n",
			reg_val[0], bt_ecc);
		return -EIO;
	}

	pr_info("[Focal] checksum %X %X\n", reg_val[0], bt_ecc);
	/*********Step 7: reset the new FW***********************/
	pr_info("[Focal] Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	ftxxxx_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(200);	/*make sure CTP startup normally */

	i_ret = hid_to_i2c(client);
	if (i_ret == 0)
		pr_info("[Focal] HidI2c change to StdI2c fail !\n");
	return 0;
}

/*
*note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
/************************************************************************
* Name: fts_GetFirmwareSize
* Brief:  get file size
* Input: file name
* Output: no
* Return: file size
***********************************************************************/
static int fts_GetFirmwareSize(char *firmware_name)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize = 0;
	char filepath[128];

	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occurred while opening file %s.\n", filepath);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	filp_close(pfile, NULL);
	return fsize;
}

/************************************************************************
* Name: fts_ReadFirmware
* Brief:  read firmware buf for .bin file.
* Input: file name, data buf
* Output: data buf
* Return: 0
***********************************************************************/
/*
note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int fts_ReadFirmware(char *firmware_name,
			       unsigned char *firmware_buf)
{
	struct file *pfile = NULL;
	struct inode *inode;
	unsigned long magic;
	off_t fsize;
	char filepath[128];
	loff_t pos;
	mm_segment_t old_fs;

	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);
	if (NULL == pfile)
		pfile = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		pr_err("error occurred while opening file %s.\n", filepath);
		return -EIO;
	}
	inode = pfile->f_dentry->d_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_read(pfile, firmware_buf, fsize, &pos);
	filp_close(pfile, NULL);
	set_fs(old_fs);
	return 0;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_app_file
* Brief:  upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
				       char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	int fwsize = fts_GetFirmwareSize(firmware_name);

	if (fwsize <= 0) {
		dev_err(&client->dev,
			"%s ERROR:Get firmware size failed\n", __func__);
		return -EIO;
	}

	if (fwsize < 8 || fwsize > 54 * 1024) {
		dev_dbg(&client->dev, "%s:FW length error\n", __func__);
		return -EIO;
	}

	/*=========FW upgrade========================*/
	pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		dev_err(&client->dev,
			"%s() - ERROR: request_firmware failed\n", __func__);
		kfree(pbt_buf);
		return -EIO;
	}

	/*call the upgrade function */
	i_ret = fts_5x22_ctpm_fw_upgrade(client, pbt_buf, fwsize);
	if (i_ret != 0) {
		dev_err(&client->dev, "%s() - ERROR:[FTS] upgrade failed..\n",
					__func__);
	}

	kfree(pbt_buf);

	return i_ret;
}

/************************************************************************
* Name: fts_tpfwver_show
* Brief:  show tp fw vwersion
* Input: device, device attribute, char buf
* Output: no
* Return: char number
***********************************************************************/
static ssize_t fts_tpfwver_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);

	if (fts_read_reg(client, FTS_REG_FW_VER, &fwver) < 0) {
		num_read_chars = snprintf(buf, PAGE_SIZE,
					"get tp fw version fail!\n");
	} else
		num_read_chars = snprintf(buf, 100, "fw: %02X\n", fwver);

	mutex_unlock(&g_device_mutex);

	return num_read_chars;
}

/************************************************************************
* Name: fts_tprwreg_store
* Brief:  read/write register
* Input: device, device attribute, char buf, char count
* Output: print register value
* Return: char count
***********************************************************************/
static ssize_t fts_tprwreg_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	unsigned long int wmreg = 0;
	u8 regaddr = 0xff, regvalue = 0xff;
	u8 valbuf[5] = {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			pr_info("[Focal] please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);

	if (0 != retval) {
		dev_err(&client->dev, "%s() - ERROR: Could not convert the given input to a number. The given input was: \"%s\"\n",
						__func__, buf);
		goto error_return;
	}

	if (2 == num_read_chars) {
		/*read register*/
		regaddr = wmreg;
		if (fts_read_reg(client, regaddr, &regvalue) < 0) {
			dev_err(&client->dev,
			"Could not read the register(0x%02x)\n", regaddr);
		} else {
			pr_info("the register(0x%02x) is 0x%02x\n",
					regaddr, regvalue);
		}
	} else {
		regaddr = wmreg >> 8;
		regvalue = wmreg;
		if (fts_write_reg(client, regaddr, regvalue) < 0) {
			dev_err(&client->dev,
			"Could not write the register(0x%02x)\n", regaddr);
		} else {
			dev_err(&client->dev,
			"Write 0x%02x into register(0x%02x) successful\n",
							regvalue, regaddr);
		}
	}

error_return:
	mutex_unlock(&g_device_mutex);
	return count;
}

/************************************************************************
* Name: fts_fwupdate_store
* Brief:  upgrade from *.i
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_fwupdate_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	u8 uc_host_fm_ver;
	int i_ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	mutex_lock(&g_device_mutex);

	disable_irq(client->irq);
	apk_debug_flag = 1;

	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);
	if (i_ret == 0) {
		msleep(300);
		uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		pr_info("[Focal] %s:upgrade to new version 0x%x\n",
						__func__, uc_host_fm_ver);
	} else {
		dev_err(&client->dev, "%s ERROR:[FTS] upgrade failed.\n",
						__func__);
	}
	apk_debug_flag = 0;
	enable_irq(client->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

/************************************************************************
* Name: fts_fwupgradeapp_store
* Brief:  upgrade from app.bin
* Input: device, device attribute, char buf, char count
* Output: no
* Return: char count
***********************************************************************/
static ssize_t fts_fwupgradeapp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count - 1] = '\0';

	mutex_lock(&g_device_mutex);
	disable_irq(client->irq);
	apk_debug_flag = 1;
	fts_ctpm_fw_upgrade_with_app_file(client, fwname);
	apk_debug_flag = 0;
	enable_irq(client->irq);
	mutex_unlock(&g_device_mutex);

	return count;
}

/*sysfs
*get the fw version
*example:cat ftstpfwver
*/
static DEVICE_ATTR(ftstpfwver, S_IRUGO | S_IWUSR, fts_tpfwver_show, NULL);

/*upgrade from *.i
*example: echo 1 > ftsfwupdate
*/
static DEVICE_ATTR(ftsfwupdate, S_IRUGO | S_IWUSR, NULL, fts_fwupdate_store);

/*read and write register
*read example: echo 88 > ftstprwreg ---read register 0x88
*write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(ftstprwreg, S_IRUGO | S_IWUSR, NULL, fts_tprwreg_store);


/*upgrade from app.bin
*example:echo "*_app.bin" > ftsfwupgradeapp
*/
static DEVICE_ATTR(ftsfwupgradeapp, S_IRUGO | S_IWUSR, NULL,
						fts_fwupgradeapp_store);

/*add your attr in here*/
static struct attribute *fts_attributes[] = {
	&dev_attr_ftstpfwver.attr,
	&dev_attr_ftsfwupdate.attr,
	&dev_attr_ftstprwreg.attr,
	&dev_attr_ftsfwupgradeapp.attr,
	NULL
};

static struct attribute_group fts_attribute_group = {
	.attrs = fts_attributes
};

/************************************************************************
* Name: fts_create_sysfs
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_sysfs(struct i2c_client *client)
{
	int err;

	err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
	if (err != 0) {
		pr_info("%s - ERROR: sysfs_create_group() failed.\n", __func__);
		sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
		return -EIO;
	}
	mutex_init(&g_device_mutex);
	pr_info("%s - sysfs_create_group() succeeded.\n", __func__);

	err = hid_to_i2c(client);
	if (err == 0) {
		pr_info("%s - ERROR: hid_to_i2c failed.\n", __func__);
		return -EIO;
	}
	err = 0;
	return err;
}

/************************************************************************
* Name: fts_release_sysfs
* Brief:  release sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/
void fts_release_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
	mutex_destroy(&g_device_mutex);
}

/*create apk debug channel*/
#define PROC_UPGRADE		0
#define PROC_READ_REGISTER	1
#define PROC_WRITE_REGISTER	2
#define PROC_AUTOCLB		4
#define PROC_UPGRADE_INFO	5
#define PROC_WRITE_DATA	6
#define PROC_READ_DATA		7
#define PROC_SET_TEST_FLAG	8
#define PROC_NAME		"ftxxxx-debug"

static unsigned char proc_operate_mode = PROC_UPGRADE;
static struct proc_dir_entry *fts_proc_entry;

static ssize_t fts_debug_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos);
static ssize_t fts_debug_write(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos);

static const struct file_operations fts_proc_fops = {
	.owner = THIS_MODULE,
	.read = fts_debug_read,
	.write = fts_debug_write,
};

/************************************************************************
* interface of write proc
* Name: fts_debug_write
* Brief:  interface of write proc
* Input: file point, data buf, data len, no use
* Output: no
* Return: data len
***********************************************************************/
static ssize_t fts_debug_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	struct i2c_client *client = G_Client;
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = count;
	int writelen = 0;
	int ret = 0;

	if (copy_from_user(&writebuf, buf, buflen)) {
		dev_err(&client->dev, "%s:copy from user error\n", __func__);
		return -EFAULT;
	}

	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];

			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			pr_info("[Focal] %s\n", upgrade_file_path);
			disable_irq(client->irq);
			apk_debug_flag = 1;
			ret = fts_ctpm_fw_upgrade_with_app_file(client,
							upgrade_file_path);
			apk_debug_flag = 0;
			enable_irq(client->irq);
			if (ret < 0) {
				dev_err(&client->dev, "%s:upgrade failed.\n",
							__func__);
				return ret;
			}
		}
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		ret = ftxxxx_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = ftxxxx_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_AUTOCLB:
		pr_info("[Focal] %s: autoclb\n", __func__);
		fts_ctpm_auto_clb(client);
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = count - 1;
		ret = ftxxxx_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	default:
		break;
	}

	return count;
}

/************************************************************************
* interface of read proc
* Name: fts_debug_read
* Brief:  interface of read proc
* Input: point to the data, no use, no use, read len, no use, no use
* Output: page point to data
* Return: read char number
***********************************************************************/
static ssize_t fts_debug_read(struct file *file, char __user *buf,
						size_t count, loff_t *ppos)
{
	struct i2c_client *client = G_Client;
	int ret = 0;
	unsigned char *buffer = NULL;
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;

	buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/*after calling fts_debug_write to upgrade*/
		regaddr = 0xA6;
		ret = fts_read_reg(client, regaddr, &regvalue);
		if (ret < 0) {
			num_read_chars = sprintf(buffer,
				"%s", "get fw version failed.\n");
		} else {
			num_read_chars = sprintf(buffer,
				"current fw version:0x%02x\n", regvalue);
		}
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = ftxxxx_i2c_Read(client, NULL, 0, buffer, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		}
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		readlen = count;
		ret = ftxxxx_i2c_Read(client, NULL, 0, buffer, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		}
		num_read_chars = readlen;
		break;
	case PROC_WRITE_DATA:
		break;
	default:
		break;
	}

	memcpy(buf, buffer, num_read_chars);
	kfree(buffer);

	return num_read_chars;
}

/************************************************************************
* Name: fts_create_apk_debug_channel
* Brief:  create apk debug channel
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int fts_create_apk_debug_channel(struct i2c_client *client)
{
	fts_proc_entry = proc_create(PROC_NAME, 0777, NULL, &fts_proc_fops);
	G_Client = client;

	if (NULL == fts_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	}
	dev_info(&client->dev, "Create proc entry success!\n");

	return 0;
}

/************************************************************************
* Name: fts_release_apk_debug_channel
* Brief:  release apk debug channel
* Input: no
* Output: no
* Return: no
***********************************************************************/
void fts_release_apk_debug_channel(void)
{
	if (fts_proc_entry)
		proc_remove(fts_proc_entry);
}
