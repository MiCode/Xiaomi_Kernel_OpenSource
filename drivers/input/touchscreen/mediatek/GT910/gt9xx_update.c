/* drivers/input/touchscreen/gt813_827_828_update.c
 *
 * 2010 - 2012 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version:1.6
 * Revision Record:
 *      V1.0:  first release. by Andrew, 2012/08/27.
 *      V1.2:  modify gt9110p pid map, by Andrew, 2012/10/15
 *      V1.4:
 *          1. modify gup_enter_update_mode,
 *          2. rewrite i2c read/write func
 *          3. check update file checksum
 *                  by Andrew, 2012/12/12
 *      v1.6:
 *          1. delete GTP_FW_DOWNLOAD related things.
 *          2. add GTP_DEF_FW_UPDATE switch to update fw by gtp_default_fw in *.h directly
 *                  by Meta, 2013/04/18
 */
#include "tpd.h"
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/uaccess.h>

#include "tpd_custom_gt9xx.h"

#if GTP_COMPATIBLE_MODE
unsigned char gtp_default_FW_fl[] = {
#include "gt9xx_firmware.h"
};
#endif

#define GUP_REG_HW_INFO             0x4220
#define GUP_REG_FW_MSG              0x41E4
#define GUP_REG_PID_VID             0x8140

#define GUP_SEARCH_FILE_TIMES       50
#define UPDATE_FILE_PATH_1          "/data/_goodix_update_.bin"
#define UPDATE_FILE_PATH_2          "/sdcard/_goodix_update_.bin"

#define CONFIG_FILE_PATH_1          "/data/_goodix_config_.cfg"
#define CONFIG_FILE_PATH_2          "/sdcard/_goodix_config_.cfg"

#define FW_HEAD_LENGTH               14
#define FW_DOWNLOAD_LENGTH           0x4000
#define FW_SECTION_LENGTH            0x2000
#define FW_DSP_ISP_LENGTH            0x1000
#define FW_DSP_LENGTH                0x1000
#define FW_BOOT_LENGTH               0x800

#define PACK_SIZE                    256
#define MAX_FRAME_CHECK_TIME         5

#define _bRW_MISCTL__SRAM_BANK       0x4048
#define _bRW_MISCTL__MEM_CD_EN       0x4049
#define _bRW_MISCTL__CACHE_EN        0x404B
#define _bRW_MISCTL__TMR0_EN         0x40B0
#define _rRW_MISCTL__SWRST_B0_       0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE 0x4184
#define _rRW_MISCTL__BOOTCTL_B0_     0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_    0x4218
#define _rRW_MISCTL__BOOT_CTL_       0x5094

#define FAIL    0
#define SUCCESS 1

#pragma pack(1)
struct st_fw_head {
	u8 hw_info[4];		/* hardware info// */
	u8 pid[8];		/* product id   // */
	u16 vid;		/* version id   // */
};
#pragma pack()

struct st_update_msg {
	u8 force_update;
	u8 fw_flag;
	struct file *file;
	struct file *cfg_file;
	struct st_fw_head ic_fw_msg;
	mm_segment_t old_fs;
};

struct st_update_msg update_msg;
u16 show_len;
u16 total_len;
u8 searching_file = 0;
s32 file_ready = 0;
static s32 gup_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = (buf[0] << 8) + buf[1];

	ret = i2c_read_bytes(client, addr, &buf[2], len - 2);

	if (!ret)
		return 2;
	return ret;
}

static s32 gup_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	s32 ret = -1;
	u16 addr = (buf[0] << 8) + buf[1];

	ret = i2c_write_bytes(client, addr, &buf[2], len - 2);

	if (!ret)
		return 1;

	return ret;
}

static u8 gup_get_ic_msg(struct i2c_client *client, u16 addr, u8 *msg, s32 len)
{
	s32 i = 0;

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;

	for (i = 0; i < 5; i++) {
		if (gup_i2c_read(client, msg, GTP_ADDR_LENGTH + len) > 0)
			break;
	}

	if (i >= 5) {
		GTP_ERROR("Read data from 0x%02x%02x failed!", msg[0], msg[1]);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_set_ic_msg(struct i2c_client *client, u16 addr, u8 val)
{
	s32 i = 0;
	u8 msg[3];

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;
	msg[2] = val;

	for (i = 0; i < 5; i++) {
		if (gup_i2c_write(client, msg, GTP_ADDR_LENGTH + 1) > 0)
			break;
	}

	if (i >= 5) {
		GTP_ERROR("Set data to 0x%02x%02x failed!", msg[0], msg[1]);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_get_ic_fw_msg(struct i2c_client *client)
{
	s32 ret = -1;
	u8 retry = 0;
	u8 buf[16];
	u8 i;

	/* step1:get hardware info */
	ret = gtp_i2c_read_dbl_check(client, GUP_REG_HW_INFO, &buf[GTP_ADDR_LENGTH], 4);
	if (FAIL == ret) {
		GTP_ERROR("[get_ic_fw_msg]get hw_info failed,exit");
		return FAIL;
	}

	/* buf[2~5]: 00 06 90 00 */
	/* hw_info: 00 90 06 00 */
	for (i = 0; i < 4; i++)
		update_msg.ic_fw_msg.hw_info[i] = buf[GTP_ADDR_LENGTH + 3 - i];

	GTP_INFO("IC Hardware info:%02x%02x%02x%02x",
		 update_msg.ic_fw_msg.hw_info[0],
		 update_msg.ic_fw_msg.hw_info[1],
		 update_msg.ic_fw_msg.hw_info[2], update_msg.ic_fw_msg.hw_info[3]);

	/* step2:get firmware message */
	for (retry = 0; retry < 2; retry++) {
		ret = gup_get_ic_msg(client, GUP_REG_FW_MSG, buf, 1);

		if (FAIL == ret) {
			GTP_ERROR("Read firmware message fail.");
			return ret;
		}

		update_msg.force_update = buf[GTP_ADDR_LENGTH];

		if ((0xBE != update_msg.force_update) && (!retry)) {
			GTP_INFO("The check sum in ic is error.");
			GTP_INFO("The IC will be updated by force.");
			continue;
		}
		break;
	}

	GTP_INFO("IC force update flag:0x%x", update_msg.force_update);

	/* step3:get pid & vid */
	ret = gtp_i2c_read_dbl_check(client, GUP_REG_PID_VID, &buf[GTP_ADDR_LENGTH], 6);
	if (FAIL == ret) {
		GTP_ERROR("[get_ic_fw_msg]get pid & vid failed,exit");
		return FAIL;
	}

	memset(update_msg.ic_fw_msg.pid, 0, sizeof(update_msg.ic_fw_msg.pid));
	memcpy(update_msg.ic_fw_msg.pid, &buf[GTP_ADDR_LENGTH], 4);

	/* GT9XX PID MAPPING */
	/*|-----FLASH-----RAM-----|
	   |------918------918-----|
	   |------968------968-----|
	   |------913------913-----|
	   |------913P-----913P----|
	   |------927------927-----|
	   |------927P-----927P----|
	   |------9110-----9110----|
	   |------9110P----9111----| */
	if (update_msg.ic_fw_msg.pid[0] != 0) {
		if (!memcmp(update_msg.ic_fw_msg.pid, "9111", 4)) {
			GTP_INFO("IC Mapping Product id:%s", update_msg.ic_fw_msg.pid);
			memcpy(update_msg.ic_fw_msg.pid, "9110P", 5);
		}
	}

	update_msg.ic_fw_msg.vid = buf[GTP_ADDR_LENGTH + 4] + (buf[GTP_ADDR_LENGTH + 5] << 8);
	return SUCCESS;
}

s32 gup_enter_update_mode(struct i2c_client *client)
{
	s32 ret = -1;
	s32 retry = 0;
	u8 rd_buf[3];

	/* step1:RST output low last at least 2ms */
	tpd_gpio_output(GTP_RST_PORT, 0);
	msleep(20);

	/* step2:select I2C slave addr,INT:0--0xBA;1--0x28. */
	tpd_gpio_output(GTP_INT_PORT, (client->addr == 0x14));
	msleep(20);

	/* step3:RST output high reset guitar */
	tpd_gpio_output(GTP_RST_PORT, 1);

	/* 20121211 modify start */
	msleep(20);
	while (retry++ < 200) {
		/* step4:Hold ss51 & dsp */
		ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}

		/* step5:Confirm hold */
		ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}
		if (0x0C == rd_buf[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("Hold ss51 & dsp confirm SUCCESS");
			break;
		}
		GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d",
			  rd_buf[GTP_ADDR_LENGTH]);
	}
	if (retry >= 200) {
		GTP_ERROR("Enter update Hold ss51 failed.");
		return FAIL;
	}

	/* step6:DSP_CK and DSP_ALU_CK PowerOn */
	ret = gup_set_ic_msg(client, 0x4010, 0x00);

	/* 20121211 modify end */
	return ret;
}

void gup_leave_update_mode(void)
{
	tpd_gpio_as_int(GTP_INT_PORT);

	GTP_DEBUG("[leave_update_mode]reset chip.");
	gtp_reset_guitar(i2c_client_point, 20);
}

static u8 gup_enter_update_judge(struct st_fw_head *fw_head)
{
	u16 u16_tmp;
	s32 i = 0;
	u8 pid_cmp_len = 0;
	/* Get the correct nvram data */
	/* The correct conditions: */
	/* 1. the hardware info is the same */
	/* 2. the product id is the same */
	/* 3. the firmware version in update file is greater than the firmware version in ic */
	/* or the check sum in ic is wrong */

	u16_tmp = fw_head->vid;
	fw_head->vid = (u16) (u16_tmp >> 8) + (u16) (u16_tmp << 8);

	GTP_INFO("FILE HARDWARE INFO:%02x%02x%02x%02x", fw_head->hw_info[0],
		 fw_head->hw_info[1], fw_head->hw_info[2], fw_head->hw_info[3]);
	GTP_INFO("FILE PID:%s", fw_head->pid);
	GTP_INFO("FILE VID:%04x", fw_head->vid);
	GTP_INFO("IC HARDWARE INFO:%02x%02x%02x%02x",
		 update_msg.ic_fw_msg.hw_info[0],
		 update_msg.ic_fw_msg.hw_info[1],
		 update_msg.ic_fw_msg.hw_info[2], update_msg.ic_fw_msg.hw_info[3]);
	GTP_INFO("IC PID:%s", update_msg.ic_fw_msg.pid);
	GTP_INFO("IC VID:%04x", update_msg.ic_fw_msg.vid);

	/* First two conditions */
	if (!memcmp
	    (fw_head->hw_info, update_msg.ic_fw_msg.hw_info,
	     sizeof(update_msg.ic_fw_msg.hw_info))) {
		GTP_INFO("Get the same hardware info.");
		if (update_msg.force_update != 0xBE) {
			GTP_INFO("FW chksum error,need enter update.");
			return SUCCESS;
		}

		/* 20130523 start */
		if (strlen(update_msg.ic_fw_msg.pid) < 3) {
			GTP_INFO("Illegal IC pid, need enter update");
			return SUCCESS;
		}
		for (i = 0; i < 3; i++) {
			if ((update_msg.ic_fw_msg.pid[i] < 0x30)
			    || (update_msg.ic_fw_msg.pid[i] > 0x39)) {
				GTP_INFO("Illegal IC pid, out of bound, need enter update");
				return SUCCESS;
			}
		}
		/* 20130523 end */

		pid_cmp_len = strlen(fw_head->pid);
		if (pid_cmp_len < strlen(update_msg.ic_fw_msg.pid))
			pid_cmp_len = strlen(update_msg.ic_fw_msg.pid);

		if ((!memcmp(fw_head->pid, update_msg.ic_fw_msg.pid, pid_cmp_len))
		    || (!memcmp(update_msg.ic_fw_msg.pid, "91XX", 4))
		    || (!memcmp(fw_head->pid, "91XX", 4))) {
			if (!memcmp(fw_head->pid, "91XX", 4))
				GTP_DEBUG("Force none same pid update mode.");
			else
				GTP_DEBUG("Get the same pid.");

			/* The third condition */
			if (fw_head->vid > update_msg.ic_fw_msg.vid) {

				GTP_INFO("Need enter update.");
				return SUCCESS;
			}

			GTP_INFO("Don't meet the third condition.");
			GTP_ERROR("File VID <= IC VID, update aborted!");
		} else {
			GTP_ERROR("File PID != IC PID, update aborted!");
		}
	} else {
		GTP_ERROR("Different Hardware, update aborted!");
	}

	return FAIL;
}

static s32 gup_update_config(struct i2c_client *client, char *cfg_path)
{
	struct file *cfg_filp;
	s32 ret = 0;
	s32 i = 0;
	u32 cfg = 0;
	u8 chksum = 0;
	u8 gt_config[228] = { 0 };
	char *sconfig = NULL;
	u16 file_len = 0;

	cfg_filp = filp_open(cfg_path, O_RDONLY, 0);

	if (IS_ERR(cfg_filp)) {
		GTP_ERROR("Failed to open %s to config update", cfg_path);
		return FAIL;
	}

	cfg_filp->f_op->llseek(cfg_filp, 0, SEEK_SET);
	file_len = cfg_filp->f_op->llseek(cfg_filp, 0, SEEK_END);

	GTP_DEBUG("file len: %d", file_len);
	if (file_len < (cfg_len * 3)) {
		GTP_ERROR("invalid config file, config update aborted!");
		filp_close(cfg_filp, NULL);
		return FAIL;
	}

	sconfig = kmalloc(sizeof(char) * (file_len + 1), GFP_KERNEL);
	memset(sconfig, 0, sizeof(char) * (file_len + 1));

	cfg_filp->f_op->llseek(cfg_filp, 0, SEEK_SET);
	ret = cfg_filp->f_op->read(cfg_filp, sconfig, file_len, &cfg_filp->f_pos);
	filp_close(cfg_filp, NULL);
	if (ret < 0) {
		GTP_ERROR("failed to read config file");
		kfree(sconfig);
		return FAIL;
	}
	GTP_DEBUG("sconfig: %s", sconfig);
	/* clear whitespace */
	for (ret = 0, i = 0; ret < file_len; ++ret) {
		if ((sconfig[ret] == ' ') ||
		    (sconfig[ret] == '\n') ||
		    (sconfig[ret] == '\r') || (sconfig[ret] == '\t') || (sconfig[ret] == '\\')
		    ) {
			continue;
		}
		sconfig[i++] = sconfig[ret];
	}
	GTP_DEBUG("After cleanup: %d", i);
	file_len = i;

	ret = sscanf(sconfig, "0x%02X", &cfg);
	gt_config[0] = (u8) cfg;
	for (ret = 4, i = 1; ret < file_len; ret += 5) {
		ret = sscanf(sconfig + ret, ",0x%02X", &cfg);
		gt_config[i] = (u8) cfg;
		++i;
	}

	kfree(sconfig);

	GTP_DEBUG("config len: %d", i);
	GTP_DEBUG_ARRAY(gt_config, i);

	/* check checksum */
	for (ret = 0; ret < (i - 2); ++ret)
		chksum += gt_config[ret];
	chksum = (~chksum) + 1;
	if (chksum != gt_config[i - 2]) {
		GTP_ERROR("Wrong checksum in %s, config update aborted!", cfg_path);
		return FAIL;
	}
	/* check update flag */
	if (gt_config[i - 1] != 0x01) {
		GTP_ERROR("Unsetted config update flag, config update aborted!");
		return FAIL;
	}

	ret = i2c_write_bytes(client, GTP_REG_CONFIG_DATA, gt_config, i);

	if (ret < 0) {
		GTP_ERROR("write config data failed, config update failed!");
		return FAIL;
	}
	GTP_INFO("Config update successfully!");
	msleep(500);		/* ic store config info into flash */
	return SUCCESS;
}

u8 gup_check_fs_mounted(char *path_name)
{
	struct path root_path;
	struct path path;
	int err;

	err = kern_path("/", LOOKUP_FOLLOW, &root_path);

	if (err)
		return FAIL;

	err = kern_path(path_name, LOOKUP_FOLLOW, &path);

	if (err)
		return FAIL;

	if (path.mnt->mnt_sb == root_path.mnt->mnt_sb) {
		/* -- not mounted */
		path_put(&root_path);
		path_put(&path);
		return FAIL;
	}
	path_put(&root_path);
	path_put(&path);
	return SUCCESS;

	/*
	   struct file *pfile;

	   pfile = filp_open(GTP_BAK_REF_PATH, O_RDONLY | O_CREAT, 0);

	   if (IS_ERR(pfile))
	   {
	   return FAIL;
	   }
	   else
	   {
	   filp_close(pfile, NULL);
	   return SUCCESS;
	   }
	 */
}

static u8 gup_check_update_file(struct i2c_client *client, struct st_fw_head *fw_head, u8 *path)
{
	s32 ret = 0;
	s32 i = 0;
	s32 fw_checksum = 0;
	u8 buf[FW_HEAD_LENGTH];

	if (path) {
		update_msg.file = filp_open(path, O_RDONLY, 0644);

		if (IS_ERR(update_msg.file)) {
			GTP_ERROR("Open update file(%s) error!", path);
			return FAIL;
		}
		file_ready = 1;
	} else {
		for (i = 0; i < GUP_SEARCH_FILE_TIMES; i++) {
			msleep(500);
			GTP_DEBUG("Wati for FS /data mounted %d", i);

			if (gup_check_fs_mounted("/data") == SUCCESS) {
				GTP_DEBUG("/data mounted ~!!!!");
				break;
			}
		}

		/* Begin to search update file */
		searching_file = 1;
		for (i = 0; i < GUP_SEARCH_FILE_TIMES; i++) {

#if GTP_DEF_FW_UPDATE
			file_ready = 0;
			break;
#endif
			if (0 == searching_file) {
				GTP_INFO("Force terminate searching file auto update.");
				return FAIL;
			}
			update_msg.file = filp_open(UPDATE_FILE_PATH_1, O_RDONLY, 0);
			if (!IS_ERR(update_msg.file)) {
				GTP_DEBUG("%s opened, size: %d bytes!",
					  UPDATE_FILE_PATH_1,
					  (int)update_msg.file->f_dentry->d_inode->i_size);

				if (update_msg.file->f_dentry->d_inode->i_size == 0) {
					filp_close(update_msg.file, NULL);
				} else {
					GTP_DEBUG("Fw Update File: %s ready!", UPDATE_FILE_PATH_1);
					file_ready = 1;
					break;
				}
			}
			update_msg.file = filp_open(UPDATE_FILE_PATH_2, O_RDONLY, 0);
			if (!IS_ERR(update_msg.file)) {
				GTP_DEBUG("%s opened, size: %d bytes!",
					  UPDATE_FILE_PATH_2,
					  (int)update_msg.file->f_dentry->d_inode->i_size);

				if (update_msg.file->f_dentry->d_inode->i_size == 0) {
					filp_close(update_msg.file, NULL);
				} else {
					GTP_DEBUG("Fw Update File: %s ready!", UPDATE_FILE_PATH_2);
					file_ready = 1;
					break;
				}
			}
			msleep(2000);
		}
		searching_file = 0;
		update_msg.cfg_file = filp_open(CONFIG_FILE_PATH_1, O_RDONLY, 0);
		if (IS_ERR(update_msg.cfg_file)) {
			update_msg.cfg_file = filp_open(CONFIG_FILE_PATH_2, O_RDONLY, 0);
			if (IS_ERR(update_msg.cfg_file)) {
				GTP_DEBUG("config update file unavailable");
			} else {
				GTP_INFO("Config Update file: %s", CONFIG_FILE_PATH_2);
				filp_close(update_msg.cfg_file, NULL);
				gup_update_config(client, CONFIG_FILE_PATH_2);
			}
		} else {
			GTP_INFO("Config Update file: %s", CONFIG_FILE_PATH_1);
			filp_close(update_msg.cfg_file, NULL);
			gup_update_config(client, CONFIG_FILE_PATH_1);
		}
	}

	if (file_ready == 0) {
#if GTP_DEF_FW_UPDATE
		GTP_INFO("Update by default firmware array");
		if (sizeof(gtp_default_FW) <
		    (FW_HEAD_LENGTH + FW_SECTION_LENGTH * 4 +
		     FW_DSP_ISP_LENGTH + FW_DSP_LENGTH + FW_BOOT_LENGTH)) {
			GTP_INFO("[check_update_file]default firmware array is INVALID!");
			return FAIL;
		}
		memcpy(buf, &gtp_default_FW[0], FW_HEAD_LENGTH);
		memcpy(fw_head, buf, FW_HEAD_LENGTH);
		/* check firmware legality */
		fw_checksum = 0;
		for (i = 0;
		     i <
		     FW_SECTION_LENGTH * 4 + FW_DSP_ISP_LENGTH + FW_DSP_LENGTH +
		     FW_BOOT_LENGTH; i += 2) {
			u16 temp;

			memcpy(buf, &gtp_default_FW[FW_HEAD_LENGTH + i], 2);
			temp = (buf[0] << 8) + buf[1];
			fw_checksum += temp;
		}

		GTP_DEBUG("firmware checksum:%x", fw_checksum & 0xFFFF);
		if (fw_checksum & 0xFFFF) {
			GTP_ERROR("Illegal firmware file.");
			return FAIL;
		}
		return SUCCESS;
#else
		return FAIL;
#endif
	}

	update_msg.old_fs = get_fs();
	set_fs(KERNEL_DS);

	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	/* update_msg.file->f_pos = 0; */

	ret =
	    update_msg.file->f_op->read(update_msg.file, (char *)buf,
					FW_HEAD_LENGTH, &update_msg.file->f_pos);

	if (ret < 0) {
		GTP_ERROR("Read firmware head in update file error.");
		goto load_failed;
	}

	memcpy(fw_head, buf, FW_HEAD_LENGTH);

	/* check firmware legality */
	fw_checksum = 0;
	for (i = 0;
	     i <
	     FW_SECTION_LENGTH * 4 + FW_DSP_ISP_LENGTH + FW_DSP_LENGTH + FW_BOOT_LENGTH; i += 2) {
		u16 temp;

		ret =
		    update_msg.file->f_op->read(update_msg.file, (char *)buf, 2,
						&update_msg.file->f_pos);
		if (ret < 0) {
			GTP_ERROR("Read firmware file error.");
			goto load_failed;
		}
		/* GTP_DEBUG("BUF[0]:%x", buf[0]); */
		temp = (buf[0] << 8) + buf[1];
		fw_checksum += temp;
	}

	GTP_DEBUG("firmware checksum:%x", fw_checksum & 0xFFFF);
	if (fw_checksum & 0xFFFF) {
		GTP_ERROR("Illegal firmware file.");
		goto load_failed;
	}

	return SUCCESS;

load_failed:
	set_fs(update_msg.old_fs);
	if (update_msg.file && !IS_ERR(update_msg.file))
		filp_close(update_msg.file, NULL);
	return FAIL;
}

static u8 gup_burn_proc(struct i2c_client *client, u8 *burn_buf, u16 start_addr, u16 total_length)
{
	s32 ret = 0;
	u16 burn_addr = start_addr;
	u16 frame_length = 0;
	u16 burn_length = 0;
	u8 wr_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	u8 rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	u8 retry = 0;

	GTP_DEBUG("Begin burn %dk data to addr 0x%x", (total_length / 1024), start_addr);

	while (burn_length < total_length) {
		GTP_DEBUG("B/T:%04d/%04d", burn_length, total_length);
		frame_length =
		    ((total_length - burn_length) >
		     PACK_SIZE) ? PACK_SIZE : (total_length - burn_length);
		wr_buf[0] = (u8) (burn_addr >> 8);
		rd_buf[0] = wr_buf[0];
		wr_buf[1] = (u8) burn_addr;
		rd_buf[1] = wr_buf[1];
		memcpy(&wr_buf[GTP_ADDR_LENGTH], &burn_buf[burn_length], frame_length);

		for (retry = 0; retry < MAX_FRAME_CHECK_TIME; retry++) {
			ret = gup_i2c_write(client, wr_buf, GTP_ADDR_LENGTH + frame_length);

			if (ret <= 0) {
				GTP_ERROR("Write frame data i2c error.");
				continue;
			}

			ret = gup_i2c_read(client, rd_buf, GTP_ADDR_LENGTH + frame_length);

			if (ret <= 0) {
				GTP_ERROR("Read back frame data i2c error.");
				continue;
			}

			if (memcmp
			    (&wr_buf[GTP_ADDR_LENGTH], &rd_buf[GTP_ADDR_LENGTH], frame_length)) {
				GTP_ERROR("Check frame data fail,not equal.");
				GTP_DEBUG("write array:");
				GTP_DEBUG_ARRAY(&wr_buf[GTP_ADDR_LENGTH], frame_length);
				GTP_DEBUG("read array:");
				GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
				continue;
			} else {
				/* GTP_DEBUG("Check frame data success."); */
				break;
			}
		}

		if (retry > MAX_FRAME_CHECK_TIME) {
			GTP_ERROR("Burn frame data time out,exit.");
			return FAIL;
		}

		burn_length += frame_length;
		burn_addr += frame_length;
	}

	return SUCCESS;
}

static u8 gup_load_section_file(u8 *buf, u16 offset, u16 length)
{
#if (GTP_AUTO_UPDATE && GTP_DEF_FW_UPDATE)
	if (0 == file_ready) {
		memcpy(buf, &gtp_default_FW[FW_HEAD_LENGTH + offset], length);
		return SUCCESS;
	}
#endif
	s32 ret = 0;

	if (update_msg.file == NULL) {
		GTP_ERROR("cannot find update file,load section file fail.");
		return FAIL;
	}

	update_msg.file->f_pos = FW_HEAD_LENGTH + offset;

	ret =
	    update_msg.file->f_op->read(update_msg.file, (char *)buf,
					length, &update_msg.file->f_pos);

	if (ret < 0) {
		GTP_ERROR("Read update file fail.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_recall_check(struct i2c_client *client, u8 *chk_src,
			   u16 start_rd_addr, u16 chk_length)
{
	u8 rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	s32 ret = 0;
	u16 recall_addr = start_rd_addr;
	u16 recall_length = 0;
	u16 frame_length = 0;

	while (recall_length < chk_length) {
		frame_length =
		    ((chk_length - recall_length) >
		     PACK_SIZE) ? PACK_SIZE : (chk_length - recall_length);
		ret = gup_get_ic_msg(client, recall_addr, rd_buf, frame_length);

		if (ret <= 0) {
			GTP_ERROR("recall i2c error,exit");
			return FAIL;
		}

		if (memcmp(&rd_buf[GTP_ADDR_LENGTH], &chk_src[recall_length], frame_length)) {
			GTP_ERROR("Recall frame data fail,not equal.");
			GTP_DEBUG("chk_src array:");
			GTP_DEBUG_ARRAY(&chk_src[recall_length], frame_length);
			GTP_DEBUG("recall array:");
			GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
			return FAIL;
		}

		recall_length += frame_length;
		recall_addr += frame_length;
	}

	GTP_DEBUG("Recall check %dk firmware success.", (chk_length / 1024));

	return SUCCESS;
}

static u8 gup_burn_fw_section(struct i2c_client *client, u8 *fw_section,
			      u16 start_addr, u8 bank_cmd)
{
	s32 ret = 0;
	u8 rd_buf[5];

	/* step1:hold ss51 & dsp */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]hold ss51 & dsp fail.");
		return FAIL;
	}

	/* step2:set scramble */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]set scramble fail.");
		return FAIL;
	}

	/* step3:select bank */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4) & 0x0F);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4) & 0x0F);
		return FAIL;
	}

	/* step4:enable accessing code */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]enable accessing code fail.");
		return FAIL;
	}

	/* step5:burn 8k fw section */
	ret = gup_burn_proc(client, fw_section, start_addr, FW_SECTION_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_section]burn fw_section fail.");
		return FAIL;
	}

	/* step6:hold ss51 & release dsp */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]hold ss51 & release dsp fail.");
		return FAIL;
	}

	/* must delay */
	msleep(20);

	/* step7:send burn cmd to move data to flash from sram */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd & 0x0f);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]send burn cmd fail.");
		return FAIL;
	}

	GTP_DEBUG("[burn_fw_section]Wait for the burn is complete......");

	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);

		if (ret <= 0) {
			GTP_ERROR("[burn_fw_section]Get burn state fail");
			return FAIL;
		}

		msleep(20);
		/* GTP_DEBUG("[burn_fw_section]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]); */
	} while (rd_buf[GTP_ADDR_LENGTH]);

	/* step8:select bank */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4) & 0x0F);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]select bank %d fail.", (bank_cmd >> 4) & 0x0F);
		return FAIL;
	}

	/* step9:enable accessing code */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]enable accessing code fail.");
		return FAIL;
	}

	/* step10:recall 8k fw section */
	ret = gup_recall_check(client, fw_section, start_addr, FW_SECTION_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_section]recall check 8k firmware fail.");
		return FAIL;
	}

	/* step11:disable accessing code */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_section]disable accessing code fail.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_burn_dsp_isp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_dsp_isp = NULL;
	u8 retry = 0;

	GTP_INFO("[burn_dsp_isp]Begin burn dsp isp---->>");

	/* step1:alloc memory */
	GTP_DEBUG("[burn_dsp_isp]step1:alloc memory");

	while (retry++ < 5) {
		fw_dsp_isp = kzalloc(FW_DSP_ISP_LENGTH, GFP_KERNEL);

		if (fw_dsp_isp == NULL) {
			continue;
		} else {
			GTP_INFO("[burn_dsp_isp]Alloc %dk byte memory success.",
				 (FW_DSP_ISP_LENGTH / 1024));
			break;
		}
	}

	if (retry >= 5) {
		GTP_ERROR("[burn_dsp_isp]Alloc memory fail,exit.");
		return FAIL;
	}

	/* step2:load dsp isp file data */
	GTP_DEBUG("[burn_dsp_isp]step2:load dsp isp file data");
	ret =
	    gup_load_section_file(fw_dsp_isp,
				  (4 * FW_SECTION_LENGTH + FW_DSP_LENGTH +
				   FW_BOOT_LENGTH), FW_DSP_ISP_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_dsp_isp]load firmware dsp_isp fail.");
		goto exit_burn_dsp_isp;
	}

	/* step3:disable wdt,clear cache enable */
	GTP_DEBUG("[burn_dsp_isp]step3:disable wdt,clear cache enable");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]disable wdt fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]clear cache enable fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	/* step4:hold ss51 & dsp */
	GTP_DEBUG("[burn_dsp_isp]step4:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	/* step5:set boot from sram */
	GTP_DEBUG("[burn_dsp_isp]step5:set boot from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]set boot from sram fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	/* step6:software reboot */
	GTP_DEBUG("[burn_dsp_isp]step6:software reboot");
	ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]software reboot fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	/* step7:select bank2 */
	GTP_DEBUG("[burn_dsp_isp]step7:select bank2");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]select bank2 fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	/* step8:enable accessing code */
	GTP_DEBUG("[burn_dsp_isp]step8:enable accessing code");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]enable accessing code fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	/* step9:burn 4k dsp_isp */
	GTP_DEBUG("[burn_dsp_isp]step9:burn 4k dsp_isp");
	ret = gup_burn_proc(client, fw_dsp_isp, 0xC000, FW_DSP_ISP_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_dsp_isp]burn dsp_isp fail.");
		goto exit_burn_dsp_isp;
	}

	/* step10:set scramble */
	GTP_DEBUG("[burn_dsp_isp]step10:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_dsp_isp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = SUCCESS;

exit_burn_dsp_isp:
	kfree(fw_dsp_isp);
	return ret;
}

static u8 gup_burn_fw_ss51(struct i2c_client *client)
{
	u8 *fw_ss51 = NULL;
	u8 retry = 0;
	s32 ret = 0;

	GTP_INFO("[burn_fw_ss51]Begin burn ss51 firmware---->>");

	/* step1:alloc memory */
	GTP_DEBUG("[burn_fw_ss51]step1:alloc memory");

	while (retry++ < 5) {
		fw_ss51 = kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);

		if (fw_ss51 == NULL) {
			continue;
		} else {
			GTP_INFO("[burn_fw_ss51]Alloc %dk byte memory success.",
				 (FW_SECTION_LENGTH / 1024));
			break;
		}
	}

	if (retry >= 5) {
		GTP_ERROR("[burn_fw_ss51]Alloc memory fail,exit.");
		return FAIL;
	}

	/* step2:load ss51 firmware section 1 file data */
	GTP_DEBUG("[burn_fw_ss51]step2:load ss51 firmware section 1 file data");
	ret = gup_load_section_file(fw_ss51, 0, FW_SECTION_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 1 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step3:clear control flag */
	GTP_DEBUG("[burn_fw_ss51]step3:clear control flag");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_ss51]clear control flag fail.");
		ret = FAIL;
		goto exit_burn_fw_ss51;
	}

	/* step4:burn ss51 firmware section 1 */
	GTP_DEBUG("[burn_fw_ss51]step4:burn ss51 firmware section 1");
	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 1 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step5:load ss51 firmware section 2 file data */
	GTP_DEBUG("[burn_fw_ss51]step5:load ss51 firmware section 2 file data");
	ret = gup_load_section_file(fw_ss51, FW_SECTION_LENGTH, FW_SECTION_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 2 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step6:burn ss51 firmware section 2 */
	GTP_DEBUG("[burn_fw_ss51]step6:burn ss51 firmware section 2");
	ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x02);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 2 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step7:load ss51 firmware section 3 file data */
	GTP_DEBUG("[burn_fw_ss51]step7:load ss51 firmware section 3 file data");
	ret = gup_load_section_file(fw_ss51, 2 * FW_SECTION_LENGTH, FW_SECTION_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 3 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step8:burn ss51 firmware section 3 */
	GTP_DEBUG("[burn_fw_ss51]step8:burn ss51 firmware section 3");
	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x13);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 3 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step9:load ss51 firmware section 4 file data */
	GTP_DEBUG("[burn_fw_ss51]step9:load ss51 firmware section 4 file data");
	ret = gup_load_section_file(fw_ss51, 3 * FW_SECTION_LENGTH, FW_SECTION_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]load ss51 firmware section 4 fail.");
		goto exit_burn_fw_ss51;
	}

	/* step10:burn ss51 firmware section 4 */
	GTP_DEBUG("[burn_fw_ss51]step10:burn ss51 firmware section 4");
	ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x14);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_ss51]burn ss51 firmware section 4 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = SUCCESS;

exit_burn_fw_ss51:
	kfree(fw_ss51);
	return ret;
}

static u8 gup_burn_fw_dsp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_dsp = NULL;
	u8 retry = 0;
	u8 rd_buf[5];

	GTP_INFO("[burn_fw_dsp]Begin burn dsp firmware---->>");
	/* step1:alloc memory */
	GTP_DEBUG("[burn_fw_dsp]step1:alloc memory");

	while (retry++ < 5) {
		fw_dsp = kzalloc(FW_DSP_LENGTH, GFP_KERNEL);

		if (fw_dsp == NULL) {
			continue;
		} else {
			GTP_INFO("[burn_fw_dsp]Alloc %dk byte memory success.",
				 (FW_SECTION_LENGTH / 1024));
			break;
		}
	}

	if (retry >= 5) {
		GTP_ERROR("[burn_fw_dsp]Alloc memory fail,exit.");
		return FAIL;
	}

	/* step2:load firmware dsp */
	GTP_DEBUG("[burn_fw_dsp]step2:load firmware dsp");
	ret = gup_load_section_file(fw_dsp, 4 * FW_SECTION_LENGTH, FW_DSP_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_dsp]load firmware dsp fail.");
		goto exit_burn_fw_dsp;
	}

	/* step3:select bank3 */
	GTP_DEBUG("[burn_fw_dsp]step3:select bank3");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	/* step4:hold ss51 & dsp */
	GTP_DEBUG("[burn_fw_dsp]step4:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	/* step5:set scramble */
	GTP_DEBUG("[burn_fw_dsp]step5:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	/* step6:release ss51 & dsp */
	GTP_DEBUG("[burn_fw_dsp]step6:release ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);	/* 20121212 */

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	/* must delay */
	msleep(20);

	/* step7:burn 4k dsp firmware */
	GTP_DEBUG("[burn_fw_dsp]step7:burn 4k dsp firmware");
	ret = gup_burn_proc(client, fw_dsp, 0x9000, FW_DSP_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_dsp]burn fw_section fail.");
		goto exit_burn_fw_dsp;
	}

	/* step8:send burn cmd to move data to flash from sram */
	GTP_DEBUG("[burn_fw_dsp]step8:send burn cmd to move data to flash from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x05);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_dsp]send burn cmd fail.");
		goto exit_burn_fw_dsp;
	}

	GTP_DEBUG("[burn_fw_dsp]Wait for the burn is complete......");

	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);

		if (ret <= 0) {
			GTP_ERROR("[burn_fw_dsp]Get burn state fail");
			goto exit_burn_fw_dsp;
		}

		msleep(20);
		/* GTP_DEBUG("[burn_fw_dsp]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]); */
	} while (rd_buf[GTP_ADDR_LENGTH]);

	/* step9:recall check 4k dsp firmware */
	GTP_DEBUG("[burn_fw_dsp]step9:recall check 4k dsp firmware");
	ret = gup_recall_check(client, fw_dsp, 0x9000, FW_DSP_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_dsp]recall check 4k dsp firmware fail.");
		goto exit_burn_fw_dsp;
	}

	ret = SUCCESS;

exit_burn_fw_dsp:
	kfree(fw_dsp);
	return ret;
}

static u8 gup_burn_fw_boot(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_boot = NULL;
	u8 retry = 0;
	u8 rd_buf[5];

	GTP_INFO("[burn_fw_boot]Begin burn bootloader firmware---->>");

	/* step1:Alloc memory */
	GTP_DEBUG("[burn_fw_boot]step1:Alloc memory");

	while (retry++ < 5) {
		fw_boot = kzalloc(FW_BOOT_LENGTH, GFP_KERNEL);
		if (fw_boot == NULL) {
			continue;
		} else {
			GTP_INFO("[burn_fw_boot]Alloc %dk byte memory success.",
				 (FW_BOOT_LENGTH / 1024));
			break;
		}
	}

	if (retry >= 5) {
		GTP_ERROR("[burn_fw_boot]Alloc memory fail,exit.");
		return FAIL;
	}

	/* step2:load firmware bootloader */
	GTP_DEBUG("[burn_fw_boot]step2:load firmware bootloader");
	ret =
	    gup_load_section_file(fw_boot, (4 * FW_SECTION_LENGTH + FW_DSP_LENGTH), FW_BOOT_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot]load firmware dsp fail.");
		goto exit_burn_fw_boot;
	}

	/* step3:hold ss51 & dsp */
	GTP_DEBUG("[burn_fw_boot]step3:hold ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	/* step4:set scramble */
	GTP_DEBUG("[burn_fw_boot]step4:set scramble");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	/* step5:release ss51 & dsp */
	GTP_DEBUG("[burn_fw_boot]step5:release ss51 & dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);	/* 20121212 */

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	/* must delay */
	msleep(20);

	/* step6:burn 2k bootloader firmware */
	GTP_DEBUG("[burn_fw_boot]step6:burn 2k bootloader firmware");
	ret = gup_burn_proc(client, fw_boot, 0x9000, FW_BOOT_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot]burn fw_section fail.");
		goto exit_burn_fw_boot;
	}

	/* step7:send burn cmd to move data to flash from sram */
	GTP_DEBUG("[burn_fw_boot]step7:send burn cmd to move data to flash from sram");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x06);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]send burn cmd fail.");
		goto exit_burn_fw_boot;
	}

	GTP_DEBUG("[burn_fw_boot]Wait for the burn is complete......");

	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);

		if (ret <= 0) {
			GTP_ERROR("[burn_fw_boot]Get burn state fail");
			goto exit_burn_fw_boot;
		}

		msleep(20);
		/* GTP_DEBUG("[burn_fw_boot]Get burn state:%d.", rd_buf[GTP_ADDR_LENGTH]); */
	} while (rd_buf[GTP_ADDR_LENGTH]);

	/* step8:recall check 2k bootloader firmware */
	GTP_DEBUG("[burn_fw_boot]step8:recall check 2k bootloader firmware");
	ret = gup_recall_check(client, fw_boot, 0x9000, FW_BOOT_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[burn_fw_boot]recall check 4k dsp firmware fail.");
		goto exit_burn_fw_boot;
	}

	/* step9:enable download DSP code */
	GTP_DEBUG("[burn_fw_boot]step9:enable download DSP code ");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x99);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]enable download DSP code fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	/* step10:release ss51 & hold dsp */
	GTP_DEBUG("[burn_fw_boot]step10:release ss51 & hold dsp");
	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x08);

	if (ret <= 0) {
		GTP_ERROR("[burn_fw_boot]release ss51 & hold dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	ret = SUCCESS;

exit_burn_fw_boot:
	kfree(fw_boot);
	return ret;
}

s32 gup_update_proc(void *dir)
{
	s32 ret = 0;
	u8 retry = 0;
	struct st_fw_head fw_head;

	GTP_INFO("[update_proc]Begin update ......");

	if (dir == NULL)
		msleep(3000);	/* wait main thread to be completed */

	if (1 == searching_file) {
		searching_file = 0;	/* exit .bin update file searching */
		GTP_INFO("Exiting searching file for auto update...");
		while ((show_len != 200) && (show_len != 100))
			msleep(100);
	}

	show_len = 1;
	total_len = 100;

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == gtp_chip_type)
		return gup_fw_download_proc(dir, GTP_FL_FW_BURN);
#endif
	ret = gup_check_update_file(i2c_client_point, &fw_head, (u8 *) dir);	/* 20121212 */

	if (FAIL == ret) {
		GTP_ERROR("[update_proc]check update file fail.");
		goto file_fail;
	}

	/* gtp_reset_guitar(i2c_client_point, 20); */
	ret = gup_get_ic_fw_msg(i2c_client_point);

	if (FAIL == ret) {
		GTP_ERROR("[update_proc]get ic message fail.");
		goto file_fail;
	}

	ret = gup_enter_update_judge(&fw_head);	/* 20121212 */

	if (FAIL == ret) {
		GTP_ERROR("[update_proc]Check *.bin file fail.");
		goto file_fail;
	}
	/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);
#if GTP_ESD_PROTECT
	gtp_esd_switch(i2c_client_point, SWITCH_OFF);
#endif
	ret = gup_enter_update_mode(i2c_client_point);

	if (FAIL == ret) {
		GTP_ERROR("[update_proc]enter update mode fail.");
		goto update_fail;
	}

	while (retry++ < 5) {
		show_len = 10;
		total_len = 100;
		ret = gup_burn_dsp_isp(i2c_client_point);

		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn dsp isp fail.");
			continue;
		}

		show_len += 10;
		ret = gup_burn_fw_ss51(i2c_client_point);

		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn ss51 firmware fail.");
			continue;
		}

		show_len += 40;
		ret = gup_burn_fw_dsp(i2c_client_point);

		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn dsp firmware fail.");
			continue;
		}

		show_len += 20;
		ret = gup_burn_fw_boot(i2c_client_point);

		if (FAIL == ret) {
			GTP_ERROR("[update_proc]burn bootloader firmware fail.");
			continue;
		}

		show_len += 10;
		GTP_INFO("[update_proc]UPDATE SUCCESS.");
		break;
	}

	if (retry >= 5) {
		GTP_ERROR("[update_proc]retry timeout,UPDATE FAIL.");
		goto update_fail;
	}

/* GTP_DEBUG("[update_proc]reset chip."); */
/* gtp_reset_guitar(i2c_client_point, 20); */
/* msleep(100); */

	show_len = 100;
	total_len = 100;
	GTP_DEBUG("[update_proc]leave update mode.");
	gup_leave_update_mode();

	GTP_DEBUG("[update_proc]send config.");
	ret = gtp_send_cfg(i2c_client_point);

	if (ret < 0)
		GTP_ERROR("[update_proc]send config fail.");

	if (update_msg.file && !IS_ERR(update_msg.file))
		filp_close(update_msg.file, NULL);
	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
#if GTP_ESD_PROTECT
	gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
	return SUCCESS;

update_fail:

	GTP_DEBUG("[update_proc]leave update mode.");
	gup_leave_update_mode();

	GTP_DEBUG("[update_proc]send config.");
	ret = gtp_send_cfg(i2c_client_point);

	if (ret < 0)
		GTP_ERROR("[update_proc]send config fail.");

	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
#if GTP_ESD_PROTECT
	gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif

file_fail:

	if (update_msg.file && !IS_ERR(update_msg.file))
		filp_close(update_msg.file, NULL);
	show_len = 200;
	total_len = 100;

	return FAIL;
}

u8 gup_init_update_proc(struct i2c_client *client)
{
	struct task_struct *thread = NULL;

	GTP_INFO("Ready to run update thread");

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == gtp_chip_type) {
		thread = kthread_run(gup_update_proc, "update", "fl_auto_update");
	} else
#endif
	{
		thread = kthread_run(gup_update_proc, (void *)NULL, "guitar_update");
	}
	if (IS_ERR(thread)) {
		GTP_ERROR("Failed to create update thread.\n");
		return -1;
	}

	return 0;
}

/* ******************* For GT9XXF Start ********************/
#define FL_UPDATE_PATH              "/data/_fl_update_.bin"
#define FL_UPDATE_PATH_SD           "/sdcard/_fl_update_.bin"
#define GUP_FW_CHK_SIZE              256
#define MAX_CHECK_TIMES              128	/* max: 2 * (16 * 1024) / 256 = 128 */
/* for clk cal */
#define PULSE_LENGTH      (200)
#define INIT_CLK_DAC      (50)
#define MAX_CLK_DAC       (120)
#define CLK_AVG_TIME      (1)
#define MILLION           1000000
#define _wRW_MISCTL__RG_DMY                       0x4282
#define _bRW_MISCTL__RG_OSC_CALIB                 0x4268
#define _fRW_MISCTL__GIO0                         0x41e9
#define _fRW_MISCTL__GIO1                         0x41ed
#define _fRW_MISCTL__GIO2                         0x41f1
#define _fRW_MISCTL__GIO3                         0x41f5
#define _fRW_MISCTL__GIO4                         0x41f9
#define _fRW_MISCTL__GIO5                         0x41fd
#define _fRW_MISCTL__GIO6                         0x4201
#define _fRW_MISCTL__GIO7                         0x4205
#define _fRW_MISCTL__GIO8                         0x4209
#define _fRW_MISCTL__GIO9                         0x420d
#define _fRW_MISCTL__MEA                          0x41a0
#define _bRW_MISCTL__MEA_MODE                     0x41a1
#define _wRW_MISCTL__MEA_MAX_NUM                  0x41a4
#define _dRO_MISCTL__MEA_VAL                      0x41b0
#define _bRW_MISCTL__MEA_SRCSEL                   0x41a3
#define _bRO_MISCTL__MEA_RDY                      0x41a8
#define _rRW_MISCTL__ANA_RXADC_B0_                0x4250
#define _bRW_MISCTL__RG_LDO_A18_PWD               0x426f
#define _bRW_MISCTL__RG_BG_PWD                    0x426a
#define _bRW_MISCTL__RG_CLKGEN_PWD                0x4269
#define _fRW_MISCTL__RG_RXADC_PWD                 0x426a
#define _bRW_MISCTL__OSC_CK_SEL                   0x4030
#define _rRW_MISCTL_RG_DMY83                      0x4283
#define _rRW_MISCTL__GIO1CTL_B2_                  0x41ee
#define _rRW_MISCTL__GIO1CTL_B1_                  0x41ed
#if GTP_COMPATIBLE_MODE

static u8 gup_check_and_repair(struct i2c_client *client, s32 chk_start_addr,
			       u8 *target_fw, u32 chk_total_length);


s32 gup_hold_ss51_dsp(struct i2c_client *client)
{
	s32 ret = -1;
	s32 retry = 0;
	u8 rd_buf[3];

	while (retry++ < 200) {
		/* step4:Hold ss51 & dsp */
		ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}

		/* step5:Confirm hold */
		ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
		if (ret <= 0) {
			GTP_DEBUG("Hold ss51 & dsp I2C error,retry:%d", retry);
			continue;
		}
		if (0x0C == rd_buf[GTP_ADDR_LENGTH]) {
			GTP_DEBUG("[enter_update_mode]Hold ss51 & dsp confirm SUCCESS");
			break;
		}
		GTP_DEBUG("Hold ss51 & dsp confirm 0x4180 failed,value:%d",
			  rd_buf[GTP_ADDR_LENGTH]);
	}
	if (retry >= 200) {
		GTP_ERROR("Enter update Hold ss51 failed.");
		return FAIL;
	}
	/* DSP_CK and DSP_ALU_CK PowerOn */
	ret = gup_set_ic_msg(client, 0x4010, 0x00);
	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]DSP_CK and DSP_ALU_CK PowerOn fail.");
		return FAIL;
	}

	/* disable wdt */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]disable wdt fail.");
		return FAIL;
	}

	/* clear cache enable */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]clear cache enable fail.");
		return FAIL;
	}

	/* set boot from sram */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]set boot from sram fail.");
		return FAIL;
	}

	/* software reboot */
	ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);
	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]software reboot fail.");
		return FAIL;
	}

	return SUCCESS;
}

s32 gup_enter_update_mode_fl(struct i2c_client *client)
{
	s32 ret = -1;
	/* s32 retry = 0; */
	/* u8 rd_buf[3]; */

	/* step1:RST output low last at least 2ms */
	tpd_gpio_output(GTP_RST_PORT, 0);
	msleep(20);

	/* step2:select I2C slave addr,INT:0--0xBA;1--0x28. */
	tpd_gpio_output(GTP_INT_PORT, (client->addr == 0x14));
	msleep(20);

	/* step3:RST output high reset guitar */
	tpd_gpio_output(GTP_RST_PORT, 1);

	msleep(20);

	/* select addr & hold ss51_dsp */
	ret = gup_hold_ss51_dsp(client);
	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]hold ss51 & dsp failed.");
		return FAIL;
	}

	/* clear control flag */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]clear control flag fail.");
		return FAIL;
	}

	/* set scramble */
	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]set scramble fail.");
		return FAIL;
	}

	/* enable accessing code */
	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[enter_update_mode]enable accessing code fail.");
		return FAIL;
	}

	return SUCCESS;
}

static s32 gup_prepare_fl_fw(char *path, struct st_fw_head *fw_head)
{
	s32 i = 0;
	s32 ret = 0;
	s32 timeout = 0;

	if (!memcmp(path, "update", 6)) {
		GTP_INFO("Search for Flashless firmware file to update");
		searching_file = 1;
		for (i = 0; i < GUP_SEARCH_FILE_TIMES; ++i) {
			if (0 == searching_file) {
				GTP_INFO("Force terminate searching file auto update.");
				return FAIL;
			}
			update_msg.file = filp_open(FL_UPDATE_PATH, O_RDONLY, 0);
			if (IS_ERR(update_msg.file)) {
				update_msg.file = filp_open(FL_UPDATE_PATH_SD, O_RDONLY, 0);
				if (IS_ERR(update_msg.file)) {
					msleep(2000);
					continue;
				} else {
					path = FL_UPDATE_PATH_SD;
					break;
				}
			} else {
				path = FL_UPDATE_PATH;
				break;
			}
		}
		searching_file = 0;
		if (i >= 50) {
			GTP_ERROR("Search timeout, update aborted");
			return FAIL;
		}
		if (!IS_ERR(update_msg.file))
			filp_close(update_msg.file, NULL);
		while (rqst_processing && (timeout++ < 15)) {
			GTP_INFO("wait for request process completed!");
			msleep(1000);
		}
	}

	GTP_INFO("Firmware update file path: %s", path);
	update_msg.file = filp_open(path, O_RDONLY, 0);

	if (IS_ERR(update_msg.file)) {
		GTP_ERROR("Open update file(%s) error!", path);
		return FAIL;
	}

	ret = gup_get_ic_fw_msg(i2c_client_point);
	if (FAIL == ret) {
		GTP_ERROR("failed to get ic firmware info");
		filp_close(update_msg.file, NULL);
		return FAIL;
	}

	update_msg.old_fs = get_fs();
	set_fs(KERNEL_DS);
	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	update_msg.file->f_op->read(update_msg.file, (char *)fw_head,
				    FW_HEAD_LENGTH, &update_msg.file->f_pos);

	ret = gup_enter_update_judge(fw_head);
	if (FAIL == ret) {
		set_fs(update_msg.old_fs);
		if (update_msg.file && !IS_ERR(update_msg.file))
			filp_close(update_msg.file, NULL);
		return FAIL;
	}

	update_msg.file->f_op->llseek(update_msg.file, 0, SEEK_SET);
	/* copy fw file to gtp_default_FW_fl array */
	ret = update_msg.file->f_op->read(update_msg.file,
					  (char *)gtp_default_FW_fl,
					  FW_HEAD_LENGTH +
					  2 * FW_DOWNLOAD_LENGTH +
					  FW_DSP_LENGTH, &update_msg.file->f_pos);
	if (ret < 0) {
		GTP_ERROR("failed to read firmware data from %s, err-code: %d", path, ret);
		ret = FAIL;
	} else {
		ret = SUCCESS;
	}
	set_fs(update_msg.old_fs);
	if (!IS_ERR(update_msg.file))
		filp_close(update_msg.file, NULL);
	return ret;
}

static u8 gup_check_update_file_fl(struct i2c_client *client,
				   struct st_fw_head *fw_head, char *path)
{
	s32 i = 0;
	s32 fw_checksum = 0;
	s32 ret = 0;

	if (NULL != path) {
		ret = gup_prepare_fl_fw(path, fw_head);
		if (ret == FAIL)
			return FAIL;
	} else {
		memcpy(fw_head, gtp_default_FW_fl, FW_HEAD_LENGTH);
		GTP_INFO("FILE HARDWARE INFO:%02x%02x%02x%02x",
			 fw_head->hw_info[0], fw_head->hw_info[1],
			 fw_head->hw_info[2], fw_head->hw_info[3]);
		GTP_INFO("FILE PID:%s", fw_head->pid);
		fw_head->vid = ((fw_head->vid & 0xFF00) >> 8) + ((fw_head->vid & 0x00FF) << 8);
		GTP_INFO("FILE VID:%04x", fw_head->vid);
	}

	/* check firmware legality */
	fw_checksum = 0;
	for (i = FW_HEAD_LENGTH; i < FW_HEAD_LENGTH + FW_SECTION_LENGTH * 4 + FW_DSP_LENGTH; i += 2) {
		u16 temp;

		/* GTP_DEBUG("BUF[0]:%x", buf[0]); */
		temp = (gtp_default_FW_fl[i] << 8) + gtp_default_FW_fl[i + 1];
		fw_checksum += temp;
	}

	GTP_DEBUG("firmware checksum:%x", fw_checksum & 0xFFFF);
	if (fw_checksum & 0xFFFF) {
		GTP_ERROR("Illegal firmware file.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_download_fw_ss51(struct i2c_client *client, u8 dwn_mode)
{
	s32 ret = 0;

	if (GTP_FL_FW_BURN == dwn_mode)
		GTP_INFO("[download_fw_ss51]Begin download ss51 firmware---->>");
	else
		GTP_INFO("[download_fw_ss51]Begin check ss51 firmware----->>");
	/* step1:download FW section 1 */
	GTP_DEBUG("[download_fw_ss51]step1:download FW section 1");
	ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__SRAM_BANK, 0x00);

	if (ret <= 0) {
		GTP_ERROR("[download_fw_ss51]select bank0 fail.");
		ret = FAIL;
		goto exit_download_fw_ss51;
	}

	if (GTP_FL_FW_BURN == dwn_mode) {
		ret =
		    i2c_write_bytes(client, 0xC000, &gtp_default_FW_fl[FW_HEAD_LENGTH],
				    FW_DOWNLOAD_LENGTH);

		if (ret == -1) {
			GTP_ERROR("[download_fw_ss51]download FW section 1 fail.");
			ret = FAIL;
			goto exit_download_fw_ss51;
		}
	}

	ret = gup_check_and_repair(i2c_client_point,
				   0xC000, &gtp_default_FW_fl[FW_HEAD_LENGTH], FW_DOWNLOAD_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[download_fw_ss51]Checked FW section 1 fail.");
		goto exit_download_fw_ss51;
	}

	/* step2:download FW section 2 */
	GTP_DEBUG("[download_fw_ss51]step2:download FW section 1");
	ret = gup_set_ic_msg(i2c_client_point, _bRW_MISCTL__SRAM_BANK, 0x01);

	if (ret <= 0) {
		GTP_ERROR("[download_fw_ss51]select bank1 fail.");
		ret = FAIL;
		goto exit_download_fw_ss51;
	}

	if (GTP_FL_FW_BURN == dwn_mode) {
		ret =
		    i2c_write_bytes(client, 0xC000,
				    &gtp_default_FW_fl[FW_HEAD_LENGTH +
						       FW_DOWNLOAD_LENGTH], FW_DOWNLOAD_LENGTH);

		if (ret == -1) {
			GTP_ERROR("[download_fw_ss51]download FW section 2 fail.");
			ret = FAIL;
			goto exit_download_fw_ss51;
		}
	}

	ret = gup_check_and_repair(i2c_client_point,
				   0xC000,
				   &gtp_default_FW_fl[FW_HEAD_LENGTH +
						      FW_DOWNLOAD_LENGTH], FW_DOWNLOAD_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[download_fw_ss51]Checked FW section 2 fail.");
		goto exit_download_fw_ss51;
	}

	ret = SUCCESS;

exit_download_fw_ss51:

	return ret;
}

#if (!GTP_SUPPORT_I2C_DMA)
static s32 i2c_auto_read(struct i2c_client *client, u8 *rxbuf, int len)
{
	u8 retry;
	u16 left = len;
	u16 offset = 0;

	struct i2c_msg msg = {
		 .addr = ((client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG)),
		/*.addr = ((client->addr & I2C_MASK_FLAG) | (I2C_PUSHPULL_FLAG)),*/
		.flags = I2C_M_RD,
		.timing = I2C_MASTER_CLOCK
	};

	if (NULL == rxbuf)
		return -1;

	while (left > 0) {
		msg.buf = &rxbuf[offset];

		if (left > MAX_TRANSACTION_LENGTH) {
			msg.len = MAX_TRANSACTION_LENGTH;
			left -= MAX_TRANSACTION_LENGTH;
			offset += MAX_TRANSACTION_LENGTH;
		} else {
			msg.len = left;
			left = 0;
		}

		retry = 0;

		while (i2c_transfer(client->adapter, &msg, 1) != 1) {
			retry++;

			if (retry == 20) {
				GTP_ERROR("I2C read 0x%X length=%d failed\n", offset, len);
				return -1;
			}
		}
	}

	return 0;
}
#endif
static u8 gup_check_and_repair(struct i2c_client *client, s32 chk_start_addr,
			       u8 *target_fw, u32 chk_total_length)
{
	s32 ret = 0;
	u32 checked_len = 0;
	u8 checked_times = 0;
	u32 chk_addr = 0;
	u8 chk_buf[GUP_FW_CHK_SIZE];
	u32 rd_size = 0;
	u8 flag_err = 0;
	s32 i = 0;

	chk_addr = chk_start_addr;
	while ((checked_times < MAX_CHECK_TIMES)
	       && (checked_len < chk_total_length)) {
		rd_size = chk_total_length - checked_len;
		if (rd_size >= GUP_FW_CHK_SIZE)
			rd_size = GUP_FW_CHK_SIZE;
#if GTP_SUPPORT_I2C_DMA
		ret = i2c_read_bytes(client, chk_addr, chk_buf, rd_size);
#else
		if (!i)
			ret = i2c_read_bytes(client, chk_addr, chk_buf, rd_size);
		else
			ret = i2c_auto_read(client, chk_buf, rd_size);
#endif

		if (-1 == ret) {
			GTP_ERROR("Read chk ram fw i2c error, client addr:0x%x", client->addr);
			checked_times++;
			continue;
		}

		for (i = 0; i < rd_size; i++) {
			if (chk_buf[i] != target_fw[i]) {
				GTP_ERROR("Ram pos[0x%04x] checked failed,rewrite.", chk_addr + i);
				i2c_write_bytes(client, chk_addr + i, &target_fw[i], rd_size - i);
				flag_err = 1;
				i = 0;
				break;
			}
		}

		if (!flag_err) {
			GTP_DEBUG("Ram pos[0x%04X] check pass!", chk_addr);
			checked_len += rd_size;
			target_fw += rd_size;
			chk_addr += rd_size;
		} else {
			flag_err = 0;
			checked_times++;
		}
	}

	if (checked_times >= MAX_CHECK_TIMES) {
		GTP_ERROR("Ram data check failed.");
		return FAIL;
	}
	return SUCCESS;
}

static u8 gup_download_fw_dsp(struct i2c_client *client, u8 dwn_mode)
{
	s32 ret = 0;

	if (GTP_FL_FW_BURN == dwn_mode)
		GTP_INFO("[download_fw_dsp]Begin download dsp fw---->>");
	else
		GTP_INFO("[download_fw_dsp]Begin check dsp fw---->>");

	/* step1:select bank2 */
	GTP_DEBUG("[download_fw_dsp]step1:select bank2");
	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);

	if (ret <= 0) {
		GTP_ERROR("[download_fw_dsp]select bank2 fail.");
		ret = FAIL;
		goto exit_download_fw_dsp;
	}
	if (GTP_FL_FW_BURN == dwn_mode) {
		ret = i2c_write_bytes(client, 0xC000,
				      &gtp_default_FW_fl[FW_HEAD_LENGTH + 2 * FW_DOWNLOAD_LENGTH],
				      FW_DSP_LENGTH);
		if (ret == -1) {
			GTP_ERROR("[download_fw_dsp]download FW dsp fail.");
			ret = FAIL;
			goto exit_download_fw_dsp;
		}
	}

	ret = gup_check_and_repair(client,
				   0xC000,
				   &gtp_default_FW_fl[FW_HEAD_LENGTH +
						      2 * FW_DOWNLOAD_LENGTH], FW_DSP_LENGTH);

	if (FAIL == ret) {
		GTP_ERROR("[download_fw_dsp]Checked FW dsp fail.");
		goto exit_download_fw_dsp;
	}

	ret = SUCCESS;

exit_download_fw_dsp:

	return ret;
}

s32 gup_fw_download_proc(void *dir, u8 dwn_mode)
{
	s32 ret = 0;
	u8 retry = 0;
	struct st_fw_head fw_head;

	if (GTP_FL_FW_BURN == dwn_mode)
		GTP_INFO("[fw_download_proc]Begin fw download ......");
	else
		GTP_INFO("[fw_download_proc]Begin fw check ......");
	show_len = 0;
	total_len = 100;

	ret = gup_check_update_file_fl(i2c_client_point, &fw_head, (char *)dir);

	show_len = 10;

	if (FAIL == ret) {
		GTP_ERROR("[fw_download_proc]check update file fail.");
		goto file_fail;
	}

	if (!memcmp(fw_head.pid, "950", 3)) {
		is_950 = 1;
		GTP_DEBUG("GT9XXF ic type: gt950");
	} else {
		is_950 = 0;
		GTP_DEBUG("GT9XXF ic type: others");
	}

	tpd_halt = 1;
	/* mt_eint_mask(CUST_EINT_TOUCH_PANEL_NUM); */
	disable_irq(touch_irq);
	ret = gup_enter_update_mode_fl(i2c_client_point);
	show_len = 20;

	if (FAIL == ret) {
		GTP_ERROR("[fw_download_proc]enter update mode fail.");
		goto download_fail;
	}

	while (retry++ < 5) {

		ret = gup_download_fw_ss51(i2c_client_point, dwn_mode);
		show_len = 60;
		if (FAIL == ret) {
			GTP_ERROR("[fw_download_proc]burn ss51 firmware fail.");
			continue;
		}

		ret = gup_download_fw_dsp(i2c_client_point, dwn_mode);
		show_len = 80;
		if (FAIL == ret) {
			GTP_ERROR("[fw_download_proc]burn dsp firmware fail.");
			continue;
		}

		GTP_INFO("[fw_download_proc]UPDATE SUCCESS.");
		break;
	}

	if (retry >= 5) {
		GTP_ERROR("[fw_download_proc]retry timeout,UPDATE FAIL.");
		goto download_fail;
	}

	if (NULL != dir) {
		gtp_fw_startup(i2c_client_point);
		tpd_halt = 0;
#if GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
	}
	show_len = 100;
	tpd_halt = 0;
	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
	return SUCCESS;

download_fail:
	if (NULL != dir) {
		gtp_fw_startup(i2c_client_point);
		tpd_halt = 0;
#if GTP_ESD_PROTECT
		gtp_esd_switch(i2c_client_point, SWITCH_ON);
#endif
	}
file_fail:
	show_len = 200;
	tpd_halt = 0;
	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	enable_irq(touch_irq);
	return FAIL;
}

static void gup_bit_write(s32 addr, s32 bit, s32 val)
{
	u8 buf;

	i2c_read_bytes(i2c_client_point, addr, &buf, 1);

	buf = (buf & (~((u8) 1 << bit))) | ((u8) val << bit);

	i2c_write_bytes(i2c_client_point, addr, &buf, 1);
}

static void gup_clk_count_init(s32 bCh, s32 bCNT)
{
	u8 buf;

	/* _fRW_MISCTL__MEA_EN = 0; //Frequency measure enable */
	gup_bit_write(_fRW_MISCTL__MEA, 0, 0);
	/* _fRW_MISCTL__MEA_CLR = 1; //Frequency measure clear */
	gup_bit_write(_fRW_MISCTL__MEA, 1, 1);
	/* _bRW_MISCTL__MEA_MODE = 0; //Pulse mode */
	buf = 0;
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__MEA_MODE, &buf, 1);
	/* _bRW_MISCTL__MEA_SRCSEL = 8 + bCh; //From GIO1 */
	buf = 8 + bCh;
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__MEA_SRCSEL, &buf, 1);
	/* _wRW_MISCTL__MEA_MAX_NUM = bCNT; //Set the Measure Counts = 1 */
	buf = bCNT;
	i2c_write_bytes(i2c_client_point, _wRW_MISCTL__MEA_MAX_NUM, &buf, 1);
	/* _fRW_MISCTL__MEA_CLR = 0; //Frequency measure not clear */
	gup_bit_write(_fRW_MISCTL__MEA, 1, 0);
	/* _fRW_MISCTL__MEA_EN = 1; */
	gup_bit_write(_fRW_MISCTL__MEA, 0, 1);
}

static u32 gup_clk_count_get(void)
{
	s32 ready = 0;
	s32 temp;
	s8 buf[4];

	while ((ready == 0)) {	/* Wait for measurement complete */
		i2c_read_bytes(i2c_client_point, _bRO_MISCTL__MEA_RDY, buf, 1);
		ready = buf[0];
	}

	udelay(50);

	/* _fRW_MISCTL__MEA_EN = 0; */
	gup_bit_write(_fRW_MISCTL__MEA, 0, 0);
	i2c_read_bytes(i2c_client_point, _dRO_MISCTL__MEA_VAL, buf, 4);
	GTP_INFO("Clk_count 0: %2X", buf[0]);
	GTP_INFO("Clk_count 1: %2X", buf[1]);
	GTP_INFO("Clk_count 2: %2X", buf[2]);
	GTP_INFO("Clk_count 3: %2X", buf[3]);

	temp = (s32) buf[0] + ((s32) buf[1] << 8) + ((s32) buf[2] << 16) + ((s32) buf[3] << 24);
	GTP_INFO("Clk_count : %d", temp);
	return temp;
}

u8 gup_clk_dac_setting(int dac)
{
	s8 buf1, buf2;

	i2c_read_bytes(i2c_client_point, _wRW_MISCTL__RG_DMY, &buf1, 1);
	i2c_read_bytes(i2c_client_point, _bRW_MISCTL__RG_OSC_CALIB, &buf2, 1);

	buf1 = (buf1 & 0xFFCF) | ((dac & 0x03) << 4);
	buf2 = (dac >> 2) & 0x3f;

	i2c_write_bytes(i2c_client_point, _wRW_MISCTL__RG_DMY, &buf1, 1);
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_OSC_CALIB, &buf2, 1);

	return 0;
}

static u8 gup_clk_calibration_pin_select(s32 bCh)
{
	s32 i2c_addr;

	switch (bCh) {
	case 0:
		i2c_addr = _fRW_MISCTL__GIO0;
		break;

	case 1:
		i2c_addr = _fRW_MISCTL__GIO1;
		break;

	case 2:
		i2c_addr = _fRW_MISCTL__GIO2;
		break;

	case 3:
		i2c_addr = _fRW_MISCTL__GIO3;
		break;

	case 4:
		i2c_addr = _fRW_MISCTL__GIO4;
		break;

	case 5:
		i2c_addr = _fRW_MISCTL__GIO5;
		break;

	case 6:
		i2c_addr = _fRW_MISCTL__GIO6;
		break;

	case 7:
		i2c_addr = _fRW_MISCTL__GIO7;
		break;

	case 8:
		i2c_addr = _fRW_MISCTL__GIO8;
		break;

	case 9:
		i2c_addr = _fRW_MISCTL__GIO9;
		break;
	}

	gup_bit_write(i2c_addr, 1, 0);

	return 0;
}

void gup_output_pulse(int t)
{
	unsigned long flags;
	/* s32 i; */

	tpd_gpio_output(GTP_INT_PORT, 0);
	udelay(10);

	local_irq_save(flags);

	tpd_gpio_output(GTP_INT_PORT, 1);
	udelay(50);
	tpd_gpio_output(GTP_INT_PORT, 0);
	udelay(t - 50);
	tpd_gpio_output(GTP_INT_PORT, 1);

	local_irq_restore(flags);

	udelay(20);
	tpd_gpio_output(GTP_INT_PORT, 0);
}

static void gup_sys_clk_init(void)
{
	u8 buf;

	/* _fRW_MISCTL__RG_RXADC_CKMUX = 0; */
	gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 5, 0);
	/* _bRW_MISCTL__RG_LDO_A18_PWD = 0; //DrvMISCTL_A18_PowerON */
	buf = 0;
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_LDO_A18_PWD, &buf, 1);
	/* _bRW_MISCTL__RG_BG_PWD = 0; //DrvMISCTL_BG_PowerON */
	buf = 0;
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_BG_PWD, &buf, 1);
	/* _bRW_MISCTL__RG_CLKGEN_PWD = 0; //DrvMISCTL_CLKGEN_PowerON */
	buf = 0;
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__RG_CLKGEN_PWD, &buf, 1);
	/* _fRW_MISCTL__RG_RXADC_PWD = 0; //DrvMISCTL_RX_ADC_PowerON */
	gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 0, 0);
	/* _fRW_MISCTL__RG_RXADC_REF_PWD = 0; //DrvMISCTL_RX_ADCREF_PowerON */
	gup_bit_write(_rRW_MISCTL__ANA_RXADC_B0_, 1, 0);
	/* gup_clk_dac_setting(60); */
	/* _bRW_MISCTL__OSC_CK_SEL = 1;; */
	buf = 1;
	i2c_write_bytes(i2c_client_point, _bRW_MISCTL__OSC_CK_SEL, &buf, 1);
}

u8 gup_clk_calibration(void)
{
	/* u8 buf; */
	/* u8 trigger; */
	s32 i;
	struct timeval start, end;
	s32 count;
	s32 count_ref;
	s32 sec;
	s32 usec;
	s32 ret = 0;
	/* unsigned long flags; */

	/* buf = 0x0C; // hold ss51 and dsp */
	/* i2c_write_bytes(i2c_client_point, _rRW_MISCTL__SWRST_B0_, &buf, 1); */
	ret = gup_hold_ss51_dsp(i2c_client_point);
	if (ret <= 0) {
		GTP_ERROR("[gup_clk_calibration]hold ss51 & dsp failed.");
		return FAIL;
	}

	/* _fRW_MISCTL__CLK_BIAS = 0; //disable clock bias */
	gup_bit_write(_rRW_MISCTL_RG_DMY83, 7, 0);

	/* _fRW_MISCTL__GIO1_PU = 0; //set TOUCH INT PIN MODE as input */
	gup_bit_write(_rRW_MISCTL__GIO1CTL_B2_, 0, 0);

	/* _fRW_MISCTL__GIO1_OE = 0; //set TOUCH INT PIN MODE as input */
	gup_bit_write(_rRW_MISCTL__GIO1CTL_B1_, 1, 0);

	/* buf = 0x00; */
	/* i2c_write_bytes(i2c_client_point, _rRW_MISCTL__SWRST_B0_, &buf, 1); */
	/* msleep(1000); */

	GTP_INFO("CLK calibration GO");
	gup_sys_clk_init();
	gup_clk_calibration_pin_select(1);	/* use GIO1 to do the calibration */

	tpd_gpio_output(GTP_INT_PORT, 0);

	for (i = INIT_CLK_DAC; i < MAX_CLK_DAC; i++) {
		GTP_INFO("CLK calibration DAC %d", i);

		gup_clk_dac_setting(i);
		gup_clk_count_init(1, CLK_AVG_TIME);

#if 0
		gup_output_pulse(PULSE_LENGTH);
		count = gup_clk_count_get();

		if (count > PULSE_LENGTH * 60) {	/* 60= 60Mhz * 1us */
			break;
		}
#else
		tpd_gpio_output(GTP_INT_PORT, 0);

		/* local_irq_save(flags); */
		do_gettimeofday(&start);
		tpd_gpio_output(GTP_INT_PORT, 1);
		/* local_irq_restore(flags); */

		msleep(20);
		tpd_gpio_output(GTP_INT_PORT, 0);
		msleep(20);

		/* local_irq_save(flags); */
		do_gettimeofday(&end);
		tpd_gpio_output(GTP_INT_PORT, 1);
		/* local_irq_restore(flags); */

		count = gup_clk_count_get();
		udelay(20);
		tpd_gpio_output(GTP_INT_PORT, 0);

		usec = end.tv_usec - start.tv_usec;
		sec = end.tv_sec - start.tv_sec;
		count_ref = 60 * (usec + sec * MILLION);

		GTP_DEBUG("== time %d, %d, %d", sec, usec, count_ref);

		if (count > count_ref) {
			GTP_DEBUG("== count_diff %d", count - count_ref);
			break;
		}
#endif
	}

	/* clk_dac = i; */

	/* gtp_reset_guitar(i2c_client_point, 20); */

#if 0				/* for debug */
	/* -- output clk to GPIO 4 */
	buf = 0x00;
	i2c_write_bytes(i2c_client_point, 0x41FA, &buf, 1);
	buf = 0x00;
	i2c_write_bytes(i2c_client_point, 0x4104, &buf, 1);
	buf = 0x00;
	i2c_write_bytes(i2c_client_point, 0x4105, &buf, 1);
	buf = 0x00;
	i2c_write_bytes(i2c_client_point, 0x4106, &buf, 1);
	buf = 0x01;
	i2c_write_bytes(i2c_client_point, 0x4107, &buf, 1);
	buf = 0x06;
	i2c_write_bytes(i2c_client_point, 0x41F8, &buf, 1);
	buf = 0x02;
	i2c_write_bytes(i2c_client_point, 0x41F9, &buf, 1);
#endif

	tpd_gpio_as_int(GTP_INT_PORT);
	return i;
}

#endif
/* *************** For GT9XXF End ***********************/
