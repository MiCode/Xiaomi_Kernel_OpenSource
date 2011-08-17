/* Source for:
 * Cypress TrueTouch(TM) Standard Product I2C touchscreen driver.
 * drivers/input/touchscreen/cyttsp-i2c.c
 *
 * Copyright (C) 2009, 2010 Cypress Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Cypress reserves the right to make changes without further notice
 * to the materials described herein. Cypress does not assume any
 * liability arising out of the application described herein.
 *
 * Contact Cypress Semiconductor at www.cypress.com
 *
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/byteorder/generic.h>
#include <linux/bitops.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#define CY_DECLARE_GLOBALS

#include <linux/cyttsp.h>

uint32_t cyttsp_tsdebug1 = 0xff;
module_param_named(tsdebug1, cyttsp_tsdebug1, uint, 0664);

#define FW_FNAME_LEN 40

/* CY TTSP I2C Driver private data */
struct cyttsp {
	struct i2c_client *client;
	struct input_dev *input;
	struct work_struct work;
	struct timer_list timer;
	struct mutex mutex;
	char phys[32];
	struct cyttsp_platform_data *platform_data;
	u8 num_prv_st_tch;
	u16 act_trk[CY_NUM_TRK_ID];
	u16 prv_st_tch[CY_NUM_ST_TCH_ID];
	u16 prv_mt_tch[CY_NUM_MT_TCH_ID];
	u16 prv_mt_pos[CY_NUM_TRK_ID][2];
	atomic_t irq_enabled;
	bool cyttsp_update_fw;
	bool cyttsp_fwloader_mode;
	bool is_suspended;
	struct regulator **vdd;
	char fw_fname[FW_FNAME_LEN];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif /* CONFIG_HAS_EARLYSUSPEND */
};
static u8 irq_cnt;		/* comparison counter with register valuw */
static u32 irq_cnt_total;	/* total interrupts */
static u32 irq_err_cnt;		/* count number of touch interrupts with err */
#define CY_IRQ_CNT_MASK	0x000000FF	/* mapped for sizeof count in reg */
#define CY_IRQ_CNT_REG	0x00		/* tt_undef[0]=reg 0x1B - Gen3 only */

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp_early_suspend(struct early_suspend *handler);
static void cyttsp_late_resume(struct early_suspend *handler);
#endif /* CONFIG_HAS_EARLYSUSPEND */

static struct workqueue_struct *cyttsp_ts_wq;


/* ****************************************************************************
 * Prototypes for static functions
 * ************************************************************************** */
static void cyttsp_xy_worker(struct work_struct *work);
static irqreturn_t cyttsp_irq(int irq, void *handle);
static int cyttsp_inlist(u16 prev_track[],
			u8 cur_trk_id, u8 *prev_loc, u8 num_touches);
static int cyttsp_next_avail_inlist(u16 cur_trk[],
			u8 *new_loc, u8 num_touches);
static int cyttsp_putbl(struct cyttsp *ts, int show,
			int show_status, int show_version, int show_cid);
static int __devinit cyttsp_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
static int __devexit cyttsp_remove(struct i2c_client *client);
static int cyttsp_resume(struct device *dev);
static int cyttsp_suspend(struct device *dev);

/* Static variables */
static struct cyttsp_gen3_xydata_t g_xy_data;
static struct cyttsp_bootloader_data_t g_bl_data;
static struct cyttsp_sysinfo_data_t g_sysinfo_data;
static const struct i2c_device_id cyttsp_id[] = {
	{ CY_I2C_NAME, 0 },  { }
};
static u8 bl_cmd[] = {
	CY_BL_FILE0, CY_BL_CMD, CY_BL_EXIT,
	CY_BL_KEY0, CY_BL_KEY1, CY_BL_KEY2,
	CY_BL_KEY3, CY_BL_KEY4, CY_BL_KEY5,
	CY_BL_KEY6, CY_BL_KEY7};

MODULE_DEVICE_TABLE(i2c, cyttsp_id);

static const struct dev_pm_ops cyttsp_pm_ops = {
	.suspend = cyttsp_suspend,
	.resume = cyttsp_resume,
};

static struct i2c_driver cyttsp_driver = {
	.driver = {
		.name = CY_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cyttsp_pm_ops,
	},
	.probe = cyttsp_probe,
	.remove = __devexit_p(cyttsp_remove),
	.id_table = cyttsp_id,
};

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen driver");
MODULE_AUTHOR("Cypress");

static ssize_t cyttsp_irq_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct cyttsp *ts = i2c_get_clientdata(client);
	return sprintf(buf, "%u\n", atomic_read(&ts->irq_enabled));
}

static ssize_t cyttsp_irq_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct cyttsp *ts = i2c_get_clientdata(client);
	int err = 0;
	unsigned long value;

	if (size > 2)
		return -EINVAL;

	err = strict_strtoul(buf, 10, &value);
	if (err != 0)
		return err;

	switch (value) {
	case 0:
		if (atomic_cmpxchg(&ts->irq_enabled, 1, 0)) {
			pr_info("touch irq disabled!\n");
			disable_irq_nosync(ts->client->irq);
		}
		err = size;
		break;
	case 1:
		if (!atomic_cmpxchg(&ts->irq_enabled, 0, 1)) {
			pr_info("touch irq enabled!\n");
			enable_irq(ts->client->irq);
		}
		err = size;
		break;
	default:
		pr_info("cyttsp_irq_enable failed -> irq_enabled = %d\n",
		atomic_read(&ts->irq_enabled));
		err = -EINVAL;
		break;
	}

	return err;
}

static DEVICE_ATTR(irq_enable, 0777, cyttsp_irq_status, cyttsp_irq_enable);

static ssize_t cyttsp_fw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d.%d.%d\n", g_bl_data.appid_lo,
				g_bl_data.appver_hi, g_bl_data.appver_lo);
}

static DEVICE_ATTR(cyttsp_fw_ver, 0777, cyttsp_fw_show, NULL);

/* firmware flashing block */
#define BLK_SIZE     16
#define DATA_REC_LEN 64
#define START_ADDR   0x0b00
#define BLK_SEED     0xff
#define RECAL_REG    0x1b

enum bl_commands {
	BL_CMD_WRBLK     = 0x39,
	BL_CMD_INIT      = 0x38,
	BL_CMD_TERMINATE = 0x3b,
};
/* TODO: Add key as part of platform data */
#define KEY_CS  (0 + 1 + 2 + 3 + 4 + 5 + 6 + 7)
#define KEY {0, 1, 2, 3, 4, 5, 6, 7}

static const  char _key[] = KEY;
#define KEY_LEN sizeof(_key)

static int rec_cnt;
struct fw_record {
	u8 seed;
	u8 cmd;
	u8 key[KEY_LEN];
	u8 blk_hi;
	u8 blk_lo;
	u8 data[DATA_REC_LEN];
	u8 data_cs;
	u8 rec_cs;
};
#define fw_rec_size (sizeof(struct fw_record))

struct cmd_record {
	u8 reg;
	u8 seed;
	u8 cmd;
	u8 key[KEY_LEN];
};
#define cmd_rec_size (sizeof(struct cmd_record))

static struct fw_record data_record = {
	.seed = BLK_SEED,
	.cmd = BL_CMD_WRBLK,
	.key = KEY,
};

static const struct cmd_record terminate_rec = {
	.reg = 0,
	.seed = BLK_SEED,
	.cmd = BL_CMD_TERMINATE,
	.key = KEY,
};
static const struct cmd_record initiate_rec = {
	.reg = 0,
	.seed = BLK_SEED,
	.cmd = BL_CMD_INIT,
	.key = KEY,
};

#define BL_REC1_ADDR          0x0780
#define BL_REC2_ADDR          0x07c0

#define ID_INFO_REC           ":40078000"
#define ID_INFO_OFFSET_IN_REC 77

#define REC_START_CHR     ':'
#define REC_LEN_OFFSET     1
#define REC_ADDR_HI_OFFSET 3
#define REC_ADDR_LO_OFFSET 5
#define REC_TYPE_OFFSET    7
#define REC_DATA_OFFSET    9
#define REC_LINE_SIZE	141

static int cyttsp_soft_reset(struct cyttsp *ts)
{
	int retval = 0, tries = 0;
	u8 host_reg = CY_SOFT_RESET_MODE;

	do {
		retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE, sizeof(host_reg), &host_reg);
		if (retval < 0)
			msleep(20);
	} while (tries++ < 10 && (retval < 0));

	if (retval < 0) {
		pr_err("%s: failed\n", __func__);
		return retval;
	}

	tries = 0;
	do {
		msleep(20);
		cyttsp_putbl(ts, 1, true, true, false);
	} while (g_bl_data.bl_status != 0x10 &&
		g_bl_data.bl_status != 0x11 &&
		tries++ < 100);

	if (g_bl_data.bl_status != 0x11 && g_bl_data.bl_status != 0x10)
		return -EINVAL;

	return 0;
}

static void cyttsp_exit_bl_mode(struct cyttsp *ts)
{
	int retval, tries = 0;

	do {
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_BASE, sizeof(bl_cmd), bl_cmd);
		if (retval < 0)
			msleep(20);
	} while (tries++ < 10 && (retval < 0));
}

static void cyttsp_set_sysinfo_mode(struct cyttsp *ts)
{
	int retval, tries = 0;
	u8 host_reg = CY_SYSINFO_MODE;

	do {
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_BASE, sizeof(host_reg), &host_reg);
		if (retval < 0)
			msleep(20);
	} while (tries++ < 10 && (retval < 0));

	/* wait for TTSP Device to complete switch to SysInfo mode */
	if (!(retval < 0)) {
		retval = i2c_smbus_read_i2c_block_data(ts->client,
				CY_REG_BASE,
				sizeof(struct cyttsp_sysinfo_data_t),
				(u8 *)&g_sysinfo_data);
	} else
		pr_err("%s: failed\n", __func__);
}

static void cyttsp_set_opmode(struct cyttsp *ts)
{
	int retval, tries = 0;
	u8 host_reg = CY_OP_MODE;

	do {
		retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE, sizeof(host_reg), &host_reg);
		if (retval < 0)
			msleep(20);
	} while (tries++ < 10 && (retval < 0));
}

static int str2uc(char *str, u8 *val)
{
	char substr[3];
	unsigned long ulval;
	int rc;

	if (!str && strlen(str) < 2)
		return -EINVAL;

	substr[0] = str[0];
	substr[1] = str[1];
	substr[2] = '\0';

	rc = strict_strtoul(substr, 16, &ulval);
	if (rc != 0)
		return rc;

	*val = (u8) ulval;

	return 0;
}

static int flash_block(struct cyttsp *ts, u8 *blk, int len)
{
	int retval, i, tries = 0;
	char buf[(2 * (BLK_SIZE + 1)) + 1];
	char *p = buf;

	for (i = 0; i < len; i++, p += 2)
		sprintf(p, "%02x", blk[i]);
	pr_debug("%s: size %d, pos %ld payload %s\n",
		       __func__, len, (long)0, buf);

	do {
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_BASE, len, blk);
		if (retval < 0)
			msleep(20);
	} while (tries++ < 20 && (retval < 0));

	if (retval < 0) {
		pr_err("%s: failed\n", __func__);
		return retval;
	}

	return 0;
}

static int flash_command(struct cyttsp *ts, const struct cmd_record *record)
{
	return flash_block(ts, (u8 *)record, cmd_rec_size);
}

static void init_data_record(struct fw_record *rec, unsigned short addr)
{
	addr >>= 6;
	rec->blk_hi = (addr >> 8) & 0xff;
	rec->blk_lo = addr & 0xff;
	rec->rec_cs = rec->blk_hi + rec->blk_lo +
			(unsigned char)(BLK_SEED + BL_CMD_WRBLK + KEY_CS);
	rec->data_cs = 0;
}

static int check_record(u8 *rec)
{
	int rc;
	u16 addr;
	u8 r_len, type, hi_off, lo_off;

	rc = str2uc(rec + REC_LEN_OFFSET, &r_len);
	if (rc < 0)
		return rc;

	rc = str2uc(rec + REC_TYPE_OFFSET, &type);
	if (rc < 0)
		return rc;

	if (*rec != REC_START_CHR || r_len != DATA_REC_LEN || type != 0)
		return -EINVAL;

	rc = str2uc(rec + REC_ADDR_HI_OFFSET, &hi_off);
	if (rc < 0)
		return rc;

	rc = str2uc(rec + REC_ADDR_LO_OFFSET, &lo_off);
	if (rc < 0)
		return rc;

	addr = (hi_off << 8) | lo_off;

	if (addr >= START_ADDR || addr == BL_REC1_ADDR || addr == BL_REC2_ADDR)
		return 0;

	return -EINVAL;
}

static struct fw_record *prepare_record(u8 *rec)
{
	int i, rc;
	u16 addr;
	u8 hi_off, lo_off;
	u8 *p;

	rc = str2uc(rec + REC_ADDR_HI_OFFSET, &hi_off);
	if (rc < 0)
		return ERR_PTR((long) rc);

	rc = str2uc(rec + REC_ADDR_LO_OFFSET, &lo_off);
	if (rc < 0)
		return ERR_PTR((long) rc);

	addr = (hi_off << 8) | lo_off;

	init_data_record(&data_record, addr);
	p = rec + REC_DATA_OFFSET;
	for (i = 0; i < DATA_REC_LEN; i++) {
		rc = str2uc(p, &data_record.data[i]);
		if (rc < 0)
			return ERR_PTR((long) rc);
		data_record.data_cs += data_record.data[i];
		data_record.rec_cs += data_record.data[i];
		p += 2;
	}
	data_record.rec_cs += data_record.data_cs;

	return &data_record;
}

static int flash_record(struct cyttsp *ts, const struct fw_record *record)
{
	int len = fw_rec_size;
	int blk_len, rc;
	u8 *rec = (u8 *)record;
	u8 data[BLK_SIZE + 1];
	u8 blk_offset;

	for (blk_offset = 0; len; len -= blk_len) {
		data[0] = blk_offset;
		blk_len = len > BLK_SIZE ? BLK_SIZE : len;
		memcpy(data + 1, rec, blk_len);
		rec += blk_len;
		rc = flash_block(ts, data, blk_len + 1);
		if (rc < 0)
			return rc;
		blk_offset += blk_len;
	}
	return 0;
}

static int flash_data_rec(struct cyttsp *ts, u8 *buf)
{
	struct fw_record *rec;
	int rc, tries;

	if (!buf)
		return -EINVAL;

	rc = check_record(buf);

	if (rc < 0) {
		pr_debug("%s: record ignored %s", __func__, buf);
		return 0;
	}

	rec = prepare_record(buf);
	if (IS_ERR_OR_NULL(rec))
		return PTR_ERR(rec);

	rc = flash_record(ts, rec);
	if (rc < 0)
		return rc;

	tries = 0;
	do {
		if (rec_cnt%2)
			msleep(20);
		cyttsp_putbl(ts, 4, true, false, false);
	} while (g_bl_data.bl_status != 0x10 &&
		g_bl_data.bl_status != 0x11 &&
		tries++ < 100);
	rec_cnt++;
	return rc;
}

static int cyttspfw_flash_firmware(struct cyttsp *ts, const u8 *data,
					int data_len)
{
	u8 *buf;
	int i, j;
	int rc, tries = 0;

	/* initiate bootload: this will erase all the existing data */
	rc = flash_command(ts, &initiate_rec);
	if (rc < 0)
		return rc;

	do {
		msleep(100);
		cyttsp_putbl(ts, 4, true, false, false);
	} while (g_bl_data.bl_status != 0x10 &&
		g_bl_data.bl_status != 0x11 &&
		tries++ < 100);

	buf = kzalloc(REC_LINE_SIZE + 1, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: no memory\n", __func__);
		return -ENOMEM;
	}

	rec_cnt = 0;
	/* flash data records */
	for (i = 0, j = 0; i < data_len; i++, j++) {
		if ((data[i] == REC_START_CHR) && j) {
			buf[j] = 0;
			rc = flash_data_rec(ts, buf);
			if (rc < 0)
				return rc;
			j = 0;
		}
		buf[j] = data[i];
	}

	/* flash last data record */
	if (j) {
		buf[j] = 0;
		rc = flash_data_rec(ts, buf);
		if (rc < 0)
			return rc;
	}

	kfree(buf);

	/* termiate bootload */
	tries = 0;
	rc = flash_command(ts, &terminate_rec);
	do {
		msleep(100);
		cyttsp_putbl(ts, 4, true, false, false);
	} while (g_bl_data.bl_status != 0x10 &&
		g_bl_data.bl_status != 0x11 &&
		tries++ < 100);

	return rc;
}

static int get_hex_fw_ver(u8 *p, u8 *ttspver_hi, u8 *ttspver_lo,
			u8 *appid_hi, u8 *appid_lo, u8 *appver_hi,
			u8 *appver_lo, u8 *cid_0, u8 *cid_1, u8 *cid_2)
{
	int rc;

	p = p + ID_INFO_OFFSET_IN_REC;
	rc = str2uc(p, ttspver_hi);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, ttspver_lo);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, appid_hi);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, appid_lo);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, appver_hi);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, appver_lo);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, cid_0);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, cid_1);
	if (rc < 0)
		return rc;
	p += 2;
	rc = str2uc(p, cid_2);
	if (rc < 0)
		return rc;

	return 0;
}

static void cyttspfw_flash_start(struct cyttsp *ts, const u8 *data,
				int data_len, u8 *buf, bool force)
{
	int rc;
	u8 ttspver_hi = 0, ttspver_lo = 0, fw_upgrade = 0;
	u8 appid_hi = 0, appid_lo = 0;
	u8 appver_hi = 0, appver_lo = 0;
	u8 cid_0 = 0, cid_1 = 0, cid_2 = 0;
	char *p = buf;

	/* get hex firmware version */
	rc = get_hex_fw_ver(p, &ttspver_hi, &ttspver_lo,
		&appid_hi, &appid_lo, &appver_hi,
		&appver_lo, &cid_0, &cid_1, &cid_2);

	if (rc < 0) {
		pr_err("%s: unable to get hex firmware version\n", __func__);
		return;
	}

	/* disable interrupts before flashing */
	if (ts->client->irq == 0)
		del_timer(&ts->timer);
	else
		disable_irq(ts->client->irq);

	rc = cancel_work_sync(&ts->work);

	if (rc && ts->client->irq)
		enable_irq(ts->client->irq);

	/* enter bootloader idle mode */
	rc = cyttsp_soft_reset(ts);

	if (rc < 0) {
		pr_err("%s: try entering into idle mode"
				" second time\n", __func__);
		msleep(1000);
		rc = cyttsp_soft_reset(ts);
	}

	if (rc < 0) {
		pr_err("%s: try again later\n", __func__);
		return;
	}


	pr_info("Current firmware: %d.%d.%d", g_bl_data.appid_lo,
				g_bl_data.appver_hi, g_bl_data.appver_lo);
	pr_info("New firmware: %d.%d.%d", appid_lo, appver_hi, appver_lo);

	if (force)
		fw_upgrade = 1;
	else
		if ((appid_hi == g_bl_data.appid_hi) &&
			(appid_lo == g_bl_data.appid_lo)) {
			if (appver_hi > g_bl_data.appver_hi) {
				fw_upgrade = 1;
			} else if ((appver_hi == g_bl_data.appver_hi) &&
					 (appver_lo > g_bl_data.appver_lo)) {
					fw_upgrade = 1;
				} else {
					fw_upgrade = 0;
					pr_info("%s: Firmware version "
					"lesser/equal to existing firmware, "
					"upgrade not needed\n", __func__);
				}
		} else {
			fw_upgrade = 0;
			pr_info("%s: Firware versions do not match, "
						"cannot upgrade\n", __func__);
		}

	if (fw_upgrade) {
		pr_info("%s: Starting firmware upgrade\n", __func__);
		rc = cyttspfw_flash_firmware(ts, data, data_len);
		if (rc < 0)
			pr_err("%s: firmware upgrade failed\n", __func__);
		else
			pr_info("%s: firmware upgrade success\n", __func__);
	}

	/* enter bootloader idle mode */
	cyttsp_soft_reset(ts);
	/* exit bootloader mode */
	cyttsp_exit_bl_mode(ts);
	msleep(100);
	/* set sysinfo details */
	cyttsp_set_sysinfo_mode(ts);
	/* enter application mode */
	cyttsp_set_opmode(ts);

	/* enable interrupts */
	if (ts->client->irq == 0)
		mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
	else
		enable_irq(ts->client->irq);
}

static void cyttspfw_upgrade_start(struct cyttsp *ts, const u8 *data,
					int data_len, bool force)
{
	int i, j;
	u8 *buf;

	buf = kzalloc(REC_LINE_SIZE + 1, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: no memory\n", __func__);
		return;
	}

	for (i = 0, j = 0; i < data_len; i++, j++) {
		if ((data[i] == REC_START_CHR) && j) {
			buf[j] = 0;
			j = 0;
			if (!strncmp(buf, ID_INFO_REC, strlen(ID_INFO_REC))) {
				cyttspfw_flash_start(ts, data, data_len,
							buf, force);
				break;
			}
		}
		buf[j] = data[i];
	}

	/* check in the last record of firmware */
	if (j) {
		buf[j] = 0;
		if (!strncmp(buf, ID_INFO_REC, strlen(ID_INFO_REC))) {
			cyttspfw_flash_start(ts, data, data_len,
						buf, force);
		}
	}

	kfree(buf);
}

static void cyttspfw_upgrade(struct device *dev, bool force)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	const struct firmware *cyttsp_fw;
	int retval = 0;

	if (ts->is_suspended == true) {
		pr_err("%s: in suspend state, resume it\n", __func__);
		retval = cyttsp_resume(dev);
		if (retval < 0) {
			pr_err("%s: unable to resume\n", __func__);
			return;
		}
	}

	retval = request_firmware(&cyttsp_fw, ts->fw_fname, dev);
	if (retval < 0) {
		pr_err("%s: %s request failed(%d)\n", __func__,
						ts->fw_fname, retval);
	} else {
		/* check and start upgrade */
		cyttspfw_upgrade_start(ts, cyttsp_fw->data,
				cyttsp_fw->size, force);
		release_firmware(cyttsp_fw);
	}
}

static ssize_t cyttsp_update_fw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	return snprintf(buf, 2, "%d\n", ts->cyttsp_fwloader_mode);
}

static ssize_t cyttsp_force_update_fw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long val;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = strict_strtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	mutex_lock(&ts->mutex);
	if (!ts->cyttsp_fwloader_mode  && val) {
		ts->cyttsp_fwloader_mode = 1;
		cyttspfw_upgrade(dev, true);
		ts->cyttsp_fwloader_mode = 0;
	}
	mutex_unlock(&ts->mutex);
	return size;
}

static DEVICE_ATTR(cyttsp_force_update_fw, 0777, cyttsp_update_fw_show,
					cyttsp_force_update_fw_store);

static ssize_t cyttsp_update_fw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	unsigned long val;
	int rc;

	if (size > 2)
		return -EINVAL;

	rc = strict_strtoul(buf, 10, &val);
	if (rc != 0)
		return rc;

	mutex_lock(&ts->mutex);
	if (!ts->cyttsp_fwloader_mode  && val) {
		ts->cyttsp_fwloader_mode = 1;
		cyttspfw_upgrade(dev, false);
		ts->cyttsp_fwloader_mode = 0;
	}
	mutex_unlock(&ts->mutex);

	return size;
}

static DEVICE_ATTR(cyttsp_update_fw, 0777, cyttsp_update_fw_show,
					cyttsp_update_fw_store);

static ssize_t cyttsp_fw_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	return snprintf(buf, FW_FNAME_LEN - 1, "%s\n", ts->fw_fname);
}

static ssize_t cyttsp_fw_name_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct cyttsp *ts = dev_get_drvdata(dev);

	if (size > FW_FNAME_LEN - 1)
		return -EINVAL;

	strncpy(ts->fw_fname, buf, size);
	if (ts->fw_fname[size-1] == '\n')
		ts->fw_fname[size-1] = 0;

	return size;
}

static DEVICE_ATTR(cyttsp_fw_name, 0777, cyttsp_fw_name_show,
					cyttsp_fw_name_store);

/* The cyttsp_xy_worker function reads the XY coordinates and sends them to
 * the input layer.  It is scheduled from the interrupt (or timer).
 */
void cyttsp_xy_worker(struct work_struct *work)
{
	struct cyttsp *ts = container_of(work, struct cyttsp, work);
	u8 id, tilt, rev_x, rev_y;
	u8 i, loc;
	u8 prv_tch;		/* number of previous touches */
	u8 cur_tch;	/* number of current touches */
	u16 tmp_trk[CY_NUM_MT_TCH_ID];
	u16 snd_trk[CY_NUM_MT_TCH_ID];
	u16 cur_trk[CY_NUM_TRK_ID];
	u16 cur_st_tch[CY_NUM_ST_TCH_ID];
	u16 cur_mt_tch[CY_NUM_MT_TCH_ID];
	/* if NOT CY_USE_TRACKING_ID then
	 * only uses CY_NUM_MT_TCH_ID positions */
	u16 cur_mt_pos[CY_NUM_TRK_ID][2];
	/* if NOT CY_USE_TRACKING_ID then
	 * only uses CY_NUM_MT_TCH_ID positions */
	u8 cur_mt_z[CY_NUM_TRK_ID];
	u8 curr_tool_width;
	u16 st_x1, st_y1;
	u8 st_z1;
	u16 st_x2, st_y2;
	u8 st_z2;
	s32 retval;

	cyttsp_xdebug("TTSP worker start 1:\n");

	/* get event data from CYTTSP device */
	i = CY_NUM_RETRY;
	do {
		retval = i2c_smbus_read_i2c_block_data(ts->client,
			CY_REG_BASE,
			sizeof(struct cyttsp_gen3_xydata_t), (u8 *)&g_xy_data);
	} while ((retval < CY_OK) && --i);

	if (retval < CY_OK) {
		/* return immediately on
		 * failure to read device on the i2c bus */
		goto exit_xy_worker;
	}

	cyttsp_xdebug("TTSP worker start 2:\n");

	/* compare own irq counter with the device irq counter */
	if (ts->client->irq) {
		u8 host_reg;
		u8 cur_cnt;
		if (ts->platform_data->use_hndshk) {

			host_reg = g_xy_data.hst_mode & CY_HNDSHK_BIT ?
				g_xy_data.hst_mode & ~CY_HNDSHK_BIT :
				g_xy_data.hst_mode | CY_HNDSHK_BIT;
			retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE, sizeof(host_reg), &host_reg);
		}
		cur_cnt = g_xy_data.tt_undef[CY_IRQ_CNT_REG];
		irq_cnt_total++;
		irq_cnt++;
		if (irq_cnt != cur_cnt) {
			irq_err_cnt++;
			cyttsp_debug("i_c_ER: dv=%d fw=%d hm=%02X t=%lu te=%lu\n", \
				irq_cnt, \
				cur_cnt, g_xy_data.hst_mode, \
				(unsigned long)irq_cnt_total, \
				(unsigned long)irq_err_cnt);
		} else {
			cyttsp_debug("i_c_ok: dv=%d fw=%d hm=%02X t=%lu te=%lu\n", \
				irq_cnt, \
				cur_cnt, g_xy_data.hst_mode, \
				(unsigned long)irq_cnt_total, \
				(unsigned long)irq_err_cnt);
		}
		irq_cnt = cur_cnt;
	}

	/* Get the current num touches and return if there are no touches */
	if ((GET_BOOTLOADERMODE(g_xy_data.tt_mode) == 1) ||
		(GET_HSTMODE(g_xy_data.hst_mode) != CY_OK)) {
		u8 host_reg, tries;
		/* the TTSP device has suffered spurious reset or mode switch */
		cyttsp_debug( \
			"Spurious err opmode (tt_mode=%02X hst_mode=%02X)\n", \
			g_xy_data.tt_mode, g_xy_data.hst_mode);
		cyttsp_debug("Reset TTSP Device; Terminating active tracks\n");
		/* terminate all active tracks */
		cur_tch = CY_NTCH;
		/* reset TTSP part and take it back out of Bootloader mode */
		/* reset TTSP Device back to bootloader mode */
		host_reg = CY_SOFT_RESET_MODE;
		retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
			sizeof(host_reg), &host_reg);
		/* wait for TTSP Device to complete reset back to bootloader */
		tries = 0;
		do {
			mdelay(1);
			cyttsp_putbl(ts, 1, false, false, false);
		} while (g_bl_data.bl_status != 0x10 &&
			g_bl_data.bl_status != 0x11 &&
			tries++ < 100);
		retval = cyttsp_putbl(ts, 1, true, true, true);
		/* switch back to operational mode */
		/* take TTSP device out of bootloader mode;
		 * switch back to TrueTouch operational mode */
		if (!(retval < CY_OK)) {
			int tries;
			retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE,
				sizeof(bl_cmd), bl_cmd);
			/* wait for TTSP Device to complete
			 * switch to Operational mode */
			tries = 0;
			do {
				mdelay(100);
				cyttsp_putbl(ts, 2, false, false, false);
			} while (GET_BOOTLOADERMODE(g_bl_data.bl_status) &&
				tries++ < 100);
			cyttsp_putbl(ts, 2, true, false, false);
		}
		goto exit_xy_worker;
	} else {
		cur_tch = GET_NUM_TOUCHES(g_xy_data.tt_stat);
		if (IS_LARGE_AREA(g_xy_data.tt_stat)) {
			/* terminate all active tracks */
			cur_tch = CY_NTCH;
			cyttsp_debug("Large obj detect (tt_stat=0x%02X). Terminate act trks\n", \
			    g_xy_data.tt_stat);
		} else if (cur_tch > CY_NUM_MT_TCH_ID) {
			/* if the number of fingers on the touch surface
			 * is more than the maximum then
			 * there will be no new track information
			 * even for the original touches.
			 * Therefore, terminate all active tracks.
			 */
			cur_tch = CY_NTCH;
			cyttsp_debug("Num touch err (tt_stat=0x%02X). Terminate act trks\n", \
			    g_xy_data.tt_stat);
		}
	}

	/* set tool size */
	curr_tool_width = CY_SMALL_TOOL_WIDTH;

	/* translate Gen2 interface data into comparable Gen3 data */
	if (ts->platform_data->gen == CY_GEN2) {
		struct cyttsp_gen2_xydata_t *pxy_gen2_data;
		pxy_gen2_data = (struct cyttsp_gen2_xydata_t *)(&g_xy_data);

		/* use test data? */
		cyttsp_testdat(&g_xy_data, &tt_gen2_testray, \
			sizeof(struct cyttsp_gen3_xydata_t));

		if (pxy_gen2_data->evnt_idx == CY_GEN2_NOTOUCH) {
			cur_tch = 0;
		} else if (cur_tch == CY_GEN2_GHOST) {
			cur_tch = 0;
		} else if (cur_tch == CY_GEN2_2TOUCH) {
			/* stuff artificial track ID1 and ID2 */
			g_xy_data.touch12_id = 0x12;
			g_xy_data.z1 = CY_MAXZ;
			g_xy_data.z2 = CY_MAXZ;
			cur_tch--;			/* 2 touches */
		} else if (cur_tch == CY_GEN2_1TOUCH) {
			/* stuff artificial track ID1 and ID2 */
			g_xy_data.touch12_id = 0x12;
			g_xy_data.z1 = CY_MAXZ;
			g_xy_data.z2 = CY_NTCH;
			if (pxy_gen2_data->evnt_idx == CY_GEN2_TOUCH2) {
				/* push touch 2 data into touch1
				 * (first finger up; second finger down) */
				/* stuff artificial track ID1 for touch2 info */
				g_xy_data.touch12_id = 0x20;
				/* stuff touch 1 with touch 2 coordinate data */
				g_xy_data.x1 = g_xy_data.x2;
				g_xy_data.y1 = g_xy_data.y2;
			}
		} else {
			cur_tch = 0;
		}
	} else {
		/* use test data? */
		cyttsp_testdat(&g_xy_data, &tt_gen3_testray, \
			sizeof(struct cyttsp_gen3_xydata_t));
	}



	/* clear current active track ID array and count previous touches */
	for (id = 0, prv_tch = CY_NTCH;
		id < CY_NUM_TRK_ID; id++) {
		cur_trk[id] = CY_NTCH;
		prv_tch += ts->act_trk[id];
	}

	/* send no events if no previous touches and no new touches */
	if ((prv_tch == CY_NTCH) &&
		((cur_tch == CY_NTCH) ||
		(cur_tch > CY_NUM_MT_TCH_ID))) {
		goto exit_xy_worker;
	}

	cyttsp_debug("prev=%d  curr=%d\n", prv_tch, cur_tch);

	for (id = 0; id < CY_NUM_ST_TCH_ID; id++) {
		/* clear current single touches array */
		cur_st_tch[id] = CY_IGNR_TCH;
	}

	/* clear single touch positions */
	st_x1 = CY_NTCH;
	st_y1 = CY_NTCH;
	st_z1 = CY_NTCH;
	st_x2 = CY_NTCH;
	st_y2 = CY_NTCH;
	st_z2 = CY_NTCH;

	for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
		/* clear current multi-touches array and
		 * multi-touch positions/z */
		cur_mt_tch[id] = CY_IGNR_TCH;
	}

	if (ts->platform_data->use_trk_id) {
		for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
			cur_mt_pos[id][CY_XPOS] = 0;
			cur_mt_pos[id][CY_YPOS] = 0;
			cur_mt_z[id] = 0;
		}
	} else {
		for (id = 0; id < CY_NUM_TRK_ID; id++) {
			cur_mt_pos[id][CY_XPOS] = 0;
			cur_mt_pos[id][CY_YPOS] = 0;
			cur_mt_z[id] = 0;
		}
	}

	/* Determine if display is tilted */
	if (FLIP_DATA(ts->platform_data->flags))
		tilt = true;
	else
		tilt = false;

	/* Check for switch in origin */
	if (REVERSE_X(ts->platform_data->flags))
		rev_x = true;
	else
		rev_x = false;

	if (REVERSE_Y(ts->platform_data->flags))
		rev_y = true;
	else
		rev_y = false;

	if (cur_tch) {
		struct cyttsp_gen2_xydata_t *pxy_gen2_data;
		struct cyttsp_gen3_xydata_t *pxy_gen3_data;
		switch (ts->platform_data->gen) {
		case CY_GEN2: {
			pxy_gen2_data =
				(struct cyttsp_gen2_xydata_t *)(&g_xy_data);
			cyttsp_xdebug("TTSP Gen2 report:\n");
			cyttsp_xdebug("%02X %02X %02X\n", \
				pxy_gen2_data->hst_mode, \
				pxy_gen2_data->tt_mode, \
				pxy_gen2_data->tt_stat);
			cyttsp_xdebug("%04X %04X %02X  %02X\n", \
				pxy_gen2_data->x1, \
				pxy_gen2_data->y1, \
				pxy_gen2_data->z1, \
				pxy_gen2_data->evnt_idx);
			cyttsp_xdebug("%04X %04X %02X\n", \
				pxy_gen2_data->x2, \
				pxy_gen2_data->y2, \
				pxy_gen2_data->tt_undef1);
			cyttsp_xdebug("%02X %02X %02X\n", \
				pxy_gen2_data->gest_cnt, \
				pxy_gen2_data->gest_id, \
				pxy_gen2_data->gest_set);
			break;
		}
		case CY_GEN3:
		default: {
			pxy_gen3_data =
				(struct cyttsp_gen3_xydata_t *)(&g_xy_data);
			cyttsp_xdebug("TTSP Gen3 report:\n");
			cyttsp_xdebug("%02X %02X %02X\n", \
				pxy_gen3_data->hst_mode,
				pxy_gen3_data->tt_mode,
				pxy_gen3_data->tt_stat);
			cyttsp_xdebug("%04X %04X %02X  %02X", \
				pxy_gen3_data->x1,
				pxy_gen3_data->y1,
				pxy_gen3_data->z1, \
				pxy_gen3_data->touch12_id);
			cyttsp_xdebug("%04X %04X %02X\n", \
				pxy_gen3_data->x2, \
				pxy_gen3_data->y2, \
				pxy_gen3_data->z2);
			cyttsp_xdebug("%02X %02X %02X\n", \
				pxy_gen3_data->gest_cnt, \
				pxy_gen3_data->gest_id, \
				pxy_gen3_data->gest_set);
			cyttsp_xdebug("%04X %04X %02X  %02X\n", \
				pxy_gen3_data->x3, \
				pxy_gen3_data->y3, \
				pxy_gen3_data->z3, \
				pxy_gen3_data->touch34_id);
			cyttsp_xdebug("%04X %04X %02X\n", \
				pxy_gen3_data->x4, \
				pxy_gen3_data->y4, \
				pxy_gen3_data->z4);
			break;
		}
		}
	}

	/* process the touches */
	switch (cur_tch) {
	case 4: {
		g_xy_data.x4 = be16_to_cpu(g_xy_data.x4);
		g_xy_data.y4 = be16_to_cpu(g_xy_data.y4);
		if (tilt)
			FLIP_XY(g_xy_data.x4, g_xy_data.y4);

		if (rev_x) {
			g_xy_data.x4 = INVERT_X(g_xy_data.x4,
						ts->platform_data->panel_maxx);
			if (g_xy_data.x4 < 0)
				pr_debug("X value is negative. Please configure"
					" maxx in platform data structure\n");
		}
		if (rev_y) {
			g_xy_data.y4 = INVERT_X(g_xy_data.y4,
						ts->platform_data->panel_maxy);
			if (g_xy_data.y4 < 0)
				pr_debug("Y value is negative. Please configure"
					" maxy in platform data structure\n");

		}
		id = GET_TOUCH4_ID(g_xy_data.touch34_id);
		if (ts->platform_data->use_trk_id) {
			cur_mt_pos[CY_MT_TCH4_IDX][CY_XPOS] =
				g_xy_data.x4;
			cur_mt_pos[CY_MT_TCH4_IDX][CY_YPOS] =
				g_xy_data.y4;
			cur_mt_z[CY_MT_TCH4_IDX] = g_xy_data.z4;
		} else {
			cur_mt_pos[id][CY_XPOS] = g_xy_data.x4;
			cur_mt_pos[id][CY_YPOS] = g_xy_data.y4;
			cur_mt_z[id] = g_xy_data.z4;
		}
		cur_mt_tch[CY_MT_TCH4_IDX] = id;
		cur_trk[id] = CY_TCH;
		if (ts->prv_st_tch[CY_ST_FNGR1_IDX] <
			CY_NUM_TRK_ID) {
			if (ts->prv_st_tch[CY_ST_FNGR1_IDX] == id) {
				st_x1 = g_xy_data.x4;
				st_y1 = g_xy_data.y4;
				st_z1 = g_xy_data.z4;
				cur_st_tch[CY_ST_FNGR1_IDX] = id;
			} else if (ts->prv_st_tch[CY_ST_FNGR2_IDX] == id) {
				st_x2 = g_xy_data.x4;
				st_y2 = g_xy_data.y4;
				st_z2 = g_xy_data.z4;
				cur_st_tch[CY_ST_FNGR2_IDX] = id;
			}
		}
		cyttsp_xdebug("4th XYZ:% 3d,% 3d,% 3d  ID:% 2d\n\n", \
			g_xy_data.x4, g_xy_data.y4, g_xy_data.z4, \
			(g_xy_data.touch34_id & 0x0F));
		/* do not break */
	}
	case 3: {
		g_xy_data.x3 = be16_to_cpu(g_xy_data.x3);
		g_xy_data.y3 = be16_to_cpu(g_xy_data.y3);
		if (tilt)
			FLIP_XY(g_xy_data.x3, g_xy_data.y3);

		if (rev_x) {
			g_xy_data.x3 = INVERT_X(g_xy_data.x3,
						ts->platform_data->panel_maxx);
			if (g_xy_data.x3 < 0)
				pr_debug("X value is negative. Please configure"
					" maxx in platform data structure\n");

		}
		if (rev_y) {
			g_xy_data.y3 = INVERT_X(g_xy_data.y3,
						ts->platform_data->panel_maxy);
			if (g_xy_data.y3 < 0)
				pr_debug("Y value is negative. Please configure"
					" maxy in platform data structure\n");

		}
		id = GET_TOUCH3_ID(g_xy_data.touch34_id);
		if (ts->platform_data->use_trk_id) {
			cur_mt_pos[CY_MT_TCH3_IDX][CY_XPOS] =
				g_xy_data.x3;
			cur_mt_pos[CY_MT_TCH3_IDX][CY_YPOS] =
				g_xy_data.y3;
			cur_mt_z[CY_MT_TCH3_IDX] = g_xy_data.z3;
		} else {
			cur_mt_pos[id][CY_XPOS] = g_xy_data.x3;
			cur_mt_pos[id][CY_YPOS] = g_xy_data.y3;
			cur_mt_z[id] = g_xy_data.z3;
		}
		cur_mt_tch[CY_MT_TCH3_IDX] = id;
		cur_trk[id] = CY_TCH;
		if (ts->prv_st_tch[CY_ST_FNGR1_IDX] <
			CY_NUM_TRK_ID) {
			if (ts->prv_st_tch[CY_ST_FNGR1_IDX] == id) {
				st_x1 = g_xy_data.x3;
				st_y1 = g_xy_data.y3;
				st_z1 = g_xy_data.z3;
				cur_st_tch[CY_ST_FNGR1_IDX] = id;
			} else if (ts->prv_st_tch[CY_ST_FNGR2_IDX] == id) {
				st_x2 = g_xy_data.x3;
				st_y2 = g_xy_data.y3;
				st_z2 = g_xy_data.z3;
				cur_st_tch[CY_ST_FNGR2_IDX] = id;
			}
		}
		cyttsp_xdebug("3rd XYZ:% 3d,% 3d,% 3d  ID:% 2d\n", \
			g_xy_data.x3, g_xy_data.y3, g_xy_data.z3, \
			((g_xy_data.touch34_id >> 4) & 0x0F));
		/* do not break */
	}
	case 2: {
		g_xy_data.x2 = be16_to_cpu(g_xy_data.x2);
		g_xy_data.y2 = be16_to_cpu(g_xy_data.y2);
		if (tilt)
			FLIP_XY(g_xy_data.x2, g_xy_data.y2);

		if (rev_x) {
			g_xy_data.x2 = INVERT_X(g_xy_data.x2,
						ts->platform_data->panel_maxx);
			if (g_xy_data.x2 < 0)
				pr_debug("X value is negative. Please configure"
					" maxx in platform data structure\n");
		}
		if (rev_y) {
			g_xy_data.y2 = INVERT_X(g_xy_data.y2,
						ts->platform_data->panel_maxy);
			if (g_xy_data.y2 < 0)
				pr_debug("Y value is negative. Please configure"
					" maxy in platform data structure\n");
		}
		id = GET_TOUCH2_ID(g_xy_data.touch12_id);
		if (ts->platform_data->use_trk_id) {
			cur_mt_pos[CY_MT_TCH2_IDX][CY_XPOS] =
				g_xy_data.x2;
			cur_mt_pos[CY_MT_TCH2_IDX][CY_YPOS] =
				g_xy_data.y2;
			cur_mt_z[CY_MT_TCH2_IDX] = g_xy_data.z2;
		} else {
			cur_mt_pos[id][CY_XPOS] = g_xy_data.x2;
			cur_mt_pos[id][CY_YPOS] = g_xy_data.y2;
			cur_mt_z[id] = g_xy_data.z2;
		}
		cur_mt_tch[CY_MT_TCH2_IDX] = id;
		cur_trk[id] = CY_TCH;
		if (ts->prv_st_tch[CY_ST_FNGR1_IDX] <
			CY_NUM_TRK_ID) {
			if (ts->prv_st_tch[CY_ST_FNGR1_IDX] == id) {
				st_x1 = g_xy_data.x2;
				st_y1 = g_xy_data.y2;
				st_z1 = g_xy_data.z2;
				cur_st_tch[CY_ST_FNGR1_IDX] = id;
			} else if (ts->prv_st_tch[CY_ST_FNGR2_IDX] == id) {
				st_x2 = g_xy_data.x2;
				st_y2 = g_xy_data.y2;
				st_z2 = g_xy_data.z2;
				cur_st_tch[CY_ST_FNGR2_IDX] = id;
			}
		}
		cyttsp_xdebug("2nd XYZ:% 3d,% 3d,% 3d  ID:% 2d\n", \
			g_xy_data.x2, g_xy_data.y2, g_xy_data.z2, \
			(g_xy_data.touch12_id & 0x0F));
		/* do not break */
	}
	case 1:	{
		g_xy_data.x1 = be16_to_cpu(g_xy_data.x1);
		g_xy_data.y1 = be16_to_cpu(g_xy_data.y1);
		if (tilt)
			FLIP_XY(g_xy_data.x1, g_xy_data.y1);

		if (rev_x) {
			g_xy_data.x1 = INVERT_X(g_xy_data.x1,
						ts->platform_data->panel_maxx);
			if (g_xy_data.x1 < 0)
				pr_debug("X value is negative. Please configure"
					" maxx in platform data structure\n");
		}
		if (rev_y) {
			g_xy_data.y1 = INVERT_X(g_xy_data.y1,
						ts->platform_data->panel_maxy);
			if (g_xy_data.y1 < 0)
				pr_debug("Y value is negative. Please configure"
					" maxy in platform data structure");
		}
		id = GET_TOUCH1_ID(g_xy_data.touch12_id);
		if (ts->platform_data->use_trk_id) {
			cur_mt_pos[CY_MT_TCH1_IDX][CY_XPOS] =
				g_xy_data.x1;
			cur_mt_pos[CY_MT_TCH1_IDX][CY_YPOS] =
				g_xy_data.y1;
			cur_mt_z[CY_MT_TCH1_IDX] = g_xy_data.z1;
		} else {
			cur_mt_pos[id][CY_XPOS] = g_xy_data.x1;
			cur_mt_pos[id][CY_YPOS] = g_xy_data.y1;
			cur_mt_z[id] = g_xy_data.z1;
		}
		cur_mt_tch[CY_MT_TCH1_IDX] = id;
		cur_trk[id] = CY_TCH;
		if (ts->prv_st_tch[CY_ST_FNGR1_IDX] <
			CY_NUM_TRK_ID) {
			if (ts->prv_st_tch[CY_ST_FNGR1_IDX] == id) {
				st_x1 = g_xy_data.x1;
				st_y1 = g_xy_data.y1;
				st_z1 = g_xy_data.z1;
				cur_st_tch[CY_ST_FNGR1_IDX] = id;
			} else if (ts->prv_st_tch[CY_ST_FNGR2_IDX] == id) {
				st_x2 = g_xy_data.x1;
				st_y2 = g_xy_data.y1;
				st_z2 = g_xy_data.z1;
				cur_st_tch[CY_ST_FNGR2_IDX] = id;
			}
		}
		cyttsp_xdebug("1st XYZ:% 3d,% 3d,% 3d  ID:% 2d\n", \
			g_xy_data.x1, g_xy_data.y1, g_xy_data.z1, \
			((g_xy_data.touch12_id >> 4) & 0x0F));
		break;
	}
	case 0:
	default:{
		break;
	}
	}

	/* handle Single Touch signals */
	if (ts->platform_data->use_st) {
		cyttsp_xdebug("ST STEP 0 - ST1 ID=%d  ST2 ID=%d\n", \
			cur_st_tch[CY_ST_FNGR1_IDX], \
			cur_st_tch[CY_ST_FNGR2_IDX]);
		if (cur_st_tch[CY_ST_FNGR1_IDX] > CY_NUM_TRK_ID) {
			/* reassign finger 1 and 2 positions to new tracks */
			if (cur_tch > 0) {
				/* reassign st finger1 */
				if (ts->platform_data->use_trk_id) {
					id = CY_MT_TCH1_IDX;
					cur_st_tch[CY_ST_FNGR1_IDX] = cur_mt_tch[id];
				} else {
					id = GET_TOUCH1_ID(g_xy_data.touch12_id);
					cur_st_tch[CY_ST_FNGR1_IDX] = id;
				}
				st_x1 = cur_mt_pos[id][CY_XPOS];
				st_y1 = cur_mt_pos[id][CY_YPOS];
				st_z1 = cur_mt_z[id];
				cyttsp_xdebug("ST STEP 1 - ST1 ID=%3d\n", \
					cur_st_tch[CY_ST_FNGR1_IDX]);
				if ((cur_tch > 1) &&
					(cur_st_tch[CY_ST_FNGR2_IDX] >
					CY_NUM_TRK_ID)) {
					/* reassign st finger2 */
					if (cur_tch > 1) {
						if (ts->platform_data->use_trk_id) {
							id = CY_MT_TCH2_IDX;
							cur_st_tch[CY_ST_FNGR2_IDX] = cur_mt_tch[id];
						} else {
							id = GET_TOUCH2_ID(g_xy_data.touch12_id);
							cur_st_tch[CY_ST_FNGR2_IDX] = id;
						}
						st_x2 = cur_mt_pos[id][CY_XPOS];
						st_y2 = cur_mt_pos[id][CY_YPOS];
						st_z2 = cur_mt_z[id];
						cyttsp_xdebug("ST STEP 2 - ST2 ID=%3d\n", \
							cur_st_tch[CY_ST_FNGR2_IDX]);
					}
				}
			}
		} else if (cur_st_tch[CY_ST_FNGR2_IDX] > CY_NUM_TRK_ID) {
			if (cur_tch > 1) {
				/* reassign st finger2 */
				if (ts->platform_data->use_trk_id) {
					/* reassign st finger2 */
					id = CY_MT_TCH2_IDX;
					cur_st_tch[CY_ST_FNGR2_IDX] =
						cur_mt_tch[id];
				} else {
					/* reassign st finger2 */
					id = GET_TOUCH2_ID(g_xy_data.touch12_id);
					cur_st_tch[CY_ST_FNGR2_IDX] = id;
				}
				st_x2 = cur_mt_pos[id][CY_XPOS];
				st_y2 = cur_mt_pos[id][CY_YPOS];
				st_z2 = cur_mt_z[id];
				cyttsp_xdebug("ST STEP 3 - ST2 ID=%3d\n", \
					cur_st_tch[CY_ST_FNGR2_IDX]);
			}
		}
		/* if the 1st touch is missing and there is a 2nd touch,
		 * then set the 1st touch to 2nd touch and terminate 2nd touch
		 */
		if ((cur_st_tch[CY_ST_FNGR1_IDX] > CY_NUM_TRK_ID) &&
		    (cur_st_tch[CY_ST_FNGR2_IDX] < CY_NUM_TRK_ID)) {
			st_x1 = st_x2;
			st_y1 = st_y2;
			st_z1 = st_z2;
			cur_st_tch[CY_ST_FNGR1_IDX] =
				cur_st_tch[CY_ST_FNGR2_IDX];
			cur_st_tch[CY_ST_FNGR2_IDX] =
				CY_IGNR_TCH;
		}
		/* if the 2nd touch ends up equal to the 1st touch,
		 * then just report a single touch */
		if (cur_st_tch[CY_ST_FNGR1_IDX] ==
			cur_st_tch[CY_ST_FNGR2_IDX]) {
			cur_st_tch[CY_ST_FNGR2_IDX] =
				CY_IGNR_TCH;
		}
		/* set Single Touch current event signals */
		if (cur_st_tch[CY_ST_FNGR1_IDX] < CY_NUM_TRK_ID) {
			input_report_abs(ts->input,
				ABS_X, st_x1);
			input_report_abs(ts->input,
				ABS_Y, st_y1);
			input_report_abs(ts->input,
				ABS_PRESSURE, st_z1);
			input_report_key(ts->input,
				BTN_TOUCH,
				CY_TCH);
			input_report_abs(ts->input,
				ABS_TOOL_WIDTH,
				curr_tool_width);
			cyttsp_debug("ST->F1:%3d X:%3d Y:%3d Z:%3d\n", \
				cur_st_tch[CY_ST_FNGR1_IDX], \
				st_x1, st_y1, st_z1);
			if (cur_st_tch[CY_ST_FNGR2_IDX] < CY_NUM_TRK_ID) {
				input_report_key(ts->input, BTN_2, CY_TCH);
				input_report_abs(ts->input, ABS_HAT0X, st_x2);
				input_report_abs(ts->input, ABS_HAT0Y, st_y2);
				cyttsp_debug("ST->F2:%3d X:%3d Y:%3d Z:%3d\n", \
					cur_st_tch[CY_ST_FNGR2_IDX],
					st_x2, st_y2, st_z2);
			} else {
				input_report_key(ts->input,
					BTN_2,
					CY_NTCH);
			}
		} else {
			input_report_abs(ts->input, ABS_PRESSURE, CY_NTCH);
			input_report_key(ts->input, BTN_TOUCH, CY_NTCH);
			input_report_key(ts->input, BTN_2, CY_NTCH);
		}
		/* update platform data for the current single touch info */
		ts->prv_st_tch[CY_ST_FNGR1_IDX] = cur_st_tch[CY_ST_FNGR1_IDX];
		ts->prv_st_tch[CY_ST_FNGR2_IDX] = cur_st_tch[CY_ST_FNGR2_IDX];

	}

	/* handle Multi-touch signals */
	if (ts->platform_data->use_mt) {
		if (ts->platform_data->use_trk_id) {
			/* terminate any previous touch where the track
			 * is missing from the current event */
			for (id = 0; id < CY_NUM_TRK_ID; id++) {
				if ((ts->act_trk[id] != CY_NTCH) &&
					(cur_trk[id] == CY_NTCH)) {
					input_report_abs(ts->input,
						ABS_MT_TRACKING_ID,
						id);
					input_report_abs(ts->input,
						ABS_MT_TOUCH_MAJOR,
						CY_NTCH);
					input_report_abs(ts->input,
						ABS_MT_WIDTH_MAJOR,
						curr_tool_width);
					input_report_abs(ts->input,
						ABS_MT_POSITION_X,
						ts->prv_mt_pos[id][CY_XPOS]);
					input_report_abs(ts->input,
						ABS_MT_POSITION_Y,
						ts->prv_mt_pos[id][CY_YPOS]);
					CY_MT_SYNC(ts->input);
					ts->act_trk[id] = CY_NTCH;
					ts->prv_mt_pos[id][CY_XPOS] = 0;
					ts->prv_mt_pos[id][CY_YPOS] = 0;
				}
			}
			/* set Multi-Touch current event signals */
			for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
				if (cur_mt_tch[id] < CY_NUM_TRK_ID) {
					input_report_abs(ts->input,
						ABS_MT_TRACKING_ID,
						cur_mt_tch[id]);
					input_report_abs(ts->input,
						ABS_MT_TOUCH_MAJOR,
						cur_mt_z[id]);
					input_report_abs(ts->input,
						ABS_MT_WIDTH_MAJOR,
						curr_tool_width);
					input_report_abs(ts->input,
						ABS_MT_POSITION_X,
						cur_mt_pos[id][CY_XPOS]);
					input_report_abs(ts->input,
						ABS_MT_POSITION_Y,
						cur_mt_pos[id][CY_YPOS]);
					CY_MT_SYNC(ts->input);
					ts->act_trk[id] = CY_TCH;
					ts->prv_mt_pos[id][CY_XPOS] =
						cur_mt_pos[id][CY_XPOS];
					ts->prv_mt_pos[id][CY_YPOS] =
						cur_mt_pos[id][CY_YPOS];
				}
			}
		} else {
			/* set temporary track array elements to voids */
			for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
				tmp_trk[id] = CY_IGNR_TCH;
				snd_trk[id] = CY_IGNR_TCH;
			}

			/* get what is currently active */
			for (i = 0, id = 0;
				id < CY_NUM_TRK_ID && i < CY_NUM_MT_TCH_ID;
				id++) {
				if (cur_trk[id] == CY_TCH) {
					/* only incr counter if track found */
					tmp_trk[i] = id;
					i++;
				}
			}
			cyttsp_xdebug("T1: t0=%d, t1=%d, t2=%d, t3=%d\n", \
				tmp_trk[0], tmp_trk[1], tmp_trk[2], \
				tmp_trk[3]);
			cyttsp_xdebug("T1: p0=%d, p1=%d, p2=%d, p3=%d\n", \
				ts->prv_mt_tch[0], ts->prv_mt_tch[1], \
				ts->prv_mt_tch[2], ts->prv_mt_tch[3]);

			/* pack in still active previous touches */
			for (id = 0, prv_tch = 0;
				id < CY_NUM_MT_TCH_ID; id++) {
				if (tmp_trk[id] < CY_NUM_TRK_ID) {
					if (cyttsp_inlist(ts->prv_mt_tch,
						tmp_trk[id], &loc,
						CY_NUM_MT_TCH_ID)) {
						loc &= CY_NUM_MT_TCH_ID - 1;
						snd_trk[loc] = tmp_trk[id];
						prv_tch++;
						cyttsp_xdebug("inlist s[%d]=%d t[%d]=%d l=%d p=%d\n", \
							loc, snd_trk[loc], \
							id, tmp_trk[id], \
							loc, prv_tch);
					} else {
						cyttsp_xdebug("not inlist s[%d]=%d t[%d]=%d l=%d \n", \
							id, snd_trk[id], \
							id, tmp_trk[id], \
							loc);
					}
				}
			}
			cyttsp_xdebug("S1: s0=%d, s1=%d, s2=%d, s3=%d p=%d\n", \
				snd_trk[0], snd_trk[1], snd_trk[2], \
				snd_trk[3], prv_tch);

			/* pack in new touches */
			for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
				if (tmp_trk[id] < CY_NUM_TRK_ID) {
					if (!cyttsp_inlist(snd_trk, tmp_trk[id], &loc, CY_NUM_MT_TCH_ID)) {
						cyttsp_xdebug("not inlist t[%d]=%d l=%d\n", \
							id, tmp_trk[id], loc);
						if (cyttsp_next_avail_inlist(snd_trk, &loc, CY_NUM_MT_TCH_ID)) {
							loc &= CY_NUM_MT_TCH_ID - 1;
							snd_trk[loc] = tmp_trk[id];
							cyttsp_xdebug("put inlist s[%d]=%d t[%d]=%d\n",
								loc, snd_trk[loc], id, tmp_trk[id]);
						}
					} else {
						cyttsp_xdebug("is in list s[%d]=%d t[%d]=%d loc=%d\n", \
							id, snd_trk[id], id, tmp_trk[id], loc);
					}
				}
			}
			cyttsp_xdebug("S2: s0=%d, s1=%d, s2=%d, s3=%d\n", \
				snd_trk[0], snd_trk[1],
				snd_trk[2], snd_trk[3]);

			/* sync motion event signals for each current touch */
			for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
				/* z will either be 0 (NOTOUCH) or
				 * some pressure (TOUCH) */
				cyttsp_xdebug("MT0 prev[%d]=%d temp[%d]=%d send[%d]=%d\n", \
					id, ts->prv_mt_tch[id], \
					id, tmp_trk[id], \
					id, snd_trk[id]);
				if (snd_trk[id] < CY_NUM_TRK_ID) {
					input_report_abs(ts->input,
						ABS_MT_TOUCH_MAJOR,
						cur_mt_z[snd_trk[id]]);
					input_report_abs(ts->input,
						ABS_MT_WIDTH_MAJOR,
						curr_tool_width);
					input_report_abs(ts->input,
						ABS_MT_POSITION_X,
						cur_mt_pos[snd_trk[id]][CY_XPOS]);
					input_report_abs(ts->input,
						ABS_MT_POSITION_Y,
						cur_mt_pos[snd_trk[id]][CY_YPOS]);
					CY_MT_SYNC(ts->input);
					cyttsp_debug("MT1->TID:%2d X:%3d Y:%3d Z:%3d touch-sent\n", \
						snd_trk[id], \
						cur_mt_pos[snd_trk[id]][CY_XPOS], \
						cur_mt_pos[snd_trk[id]][CY_YPOS], \
						cur_mt_z[snd_trk[id]]);
				} else if (ts->prv_mt_tch[id] < CY_NUM_TRK_ID) {
					/* void out this touch */
					input_report_abs(ts->input,
						ABS_MT_TOUCH_MAJOR,
						CY_NTCH);
					input_report_abs(ts->input,
						ABS_MT_WIDTH_MAJOR,
						curr_tool_width);
					input_report_abs(ts->input,
						ABS_MT_POSITION_X,
						ts->prv_mt_pos[ts->prv_mt_tch[id]][CY_XPOS]);
					input_report_abs(ts->input,
						ABS_MT_POSITION_Y,
						ts->prv_mt_pos[ts->prv_mt_tch[id]][CY_YPOS]);
					CY_MT_SYNC(ts->input);
					cyttsp_debug("MT2->TID:%2d X:%3d Y:%3d Z:%3d lift off-sent\n", \
						ts->prv_mt_tch[id], \
						ts->prv_mt_pos[ts->prv_mt_tch[id]][CY_XPOS], \
						ts->prv_mt_pos[ts->prv_mt_tch[id]][CY_YPOS], \
						CY_NTCH);
				} else {
					/* do not stuff any signals for this
					 * previously and currently
					 * void touches */
					cyttsp_xdebug("MT3->send[%d]=%d - No touch - NOT sent\n", \
							id, snd_trk[id]);
				}
			}

			/* save current posted tracks to
			 * previous track memory */
			for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
				ts->prv_mt_tch[id] = snd_trk[id];
				if (snd_trk[id] < CY_NUM_TRK_ID) {
					ts->prv_mt_pos[snd_trk[id]][CY_XPOS] =
						cur_mt_pos[snd_trk[id]][CY_XPOS];
					ts->prv_mt_pos[snd_trk[id]][CY_YPOS] =
						cur_mt_pos[snd_trk[id]][CY_YPOS];
					cyttsp_xdebug("MT4->TID:%2d X:%3d Y:%3d Z:%3d save for previous\n", \
						snd_trk[id], \
						ts->prv_mt_pos[snd_trk[id]][CY_XPOS], \
						ts->prv_mt_pos[snd_trk[id]][CY_YPOS], \
						CY_NTCH);
				}
			}
			for (id = 0; id < CY_NUM_TRK_ID; id++)
				ts->act_trk[id] = CY_NTCH;
			for (id = 0; id < CY_NUM_MT_TCH_ID; id++) {
				if (snd_trk[id] < CY_NUM_TRK_ID)
					ts->act_trk[snd_trk[id]] = CY_TCH;
			}
		}
	}

	/* handle gestures */
	if (ts->platform_data->use_gestures) {
		if (g_xy_data.gest_id) {
			input_report_key(ts->input,
				BTN_3, CY_TCH);
			input_report_abs(ts->input,
				ABS_HAT1X, g_xy_data.gest_id);
			input_report_abs(ts->input,
				ABS_HAT2Y, g_xy_data.gest_cnt);
		}
	}

	/* signal the view motion event */
	input_sync(ts->input);

	for (id = 0; id < CY_NUM_TRK_ID; id++) {
		/* update platform data for the current MT information */
		ts->act_trk[id] = cur_trk[id];
	}

exit_xy_worker:
	if (cyttsp_disable_touch) {
		/* Turn off the touch interrupts */
		cyttsp_debug("Not enabling touch\n");
	} else {
		if (ts->client->irq == 0) {
			/* restart event timer */
			mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
		} else {
			/* re-enable the interrupt after processing */
			enable_irq(ts->client->irq);
		}
	}
	return;
}

static int cyttsp_inlist(u16 prev_track[], u8 cur_trk_id,
			u8 *prev_loc, u8 num_touches)
{
	u8 id = 0;

	*prev_loc = CY_IGNR_TCH;

		cyttsp_xdebug("IN p[%d]=%d c=%d n=%d loc=%d\n", \
			id, prev_track[id], cur_trk_id, \
			num_touches, *prev_loc);
	for (id = 0, *prev_loc = CY_IGNR_TCH;
		(id < num_touches); id++) {
		cyttsp_xdebug("p[%d]=%d c=%d n=%d loc=%d\n", \
			id, prev_track[id], cur_trk_id, \
			num_touches, *prev_loc);
		if (prev_track[id] == cur_trk_id) {
			*prev_loc = id;
			break;
		}
	}
	cyttsp_xdebug("OUT p[%d]=%d c=%d n=%d loc=%d\n", \
		id, prev_track[id], cur_trk_id, num_touches, *prev_loc);

	return ((*prev_loc < CY_NUM_TRK_ID) ? true : false);
}

static int cyttsp_next_avail_inlist(u16 cur_trk[],
			u8 *new_loc, u8 num_touches)
{
	u8 id;

	for (id = 0, *new_loc = CY_IGNR_TCH;
		(id < num_touches); id++) {
		if (cur_trk[id] > CY_NUM_TRK_ID) {
			*new_loc = id;
			break;
		}
	}

	return ((*new_loc < CY_NUM_TRK_ID) ? true : false);
}

/* Timer function used as dummy interrupt driver */
static void cyttsp_timer(unsigned long handle)
{
	struct cyttsp *ts = (struct cyttsp *) handle;

	cyttsp_xdebug("TTSP Device timer event\n");

	/* schedule motion signal handling */
	queue_work(cyttsp_ts_wq, &ts->work);

	return;
}



/* ************************************************************************
 * ISR function. This function is general, initialized in drivers init
 * function
 * ************************************************************************ */
static irqreturn_t cyttsp_irq(int irq, void *handle)
{
	struct cyttsp *ts = (struct cyttsp *) handle;

	cyttsp_xdebug("%s: Got IRQ\n", CY_I2C_NAME);

	/* disable further interrupts until this interrupt is processed */
	disable_irq_nosync(ts->client->irq);

	/* schedule motion signal handling */
	queue_work(cyttsp_ts_wq, &ts->work);
	return IRQ_HANDLED;
}

/* ************************************************************************
 * Probe initialization functions
 * ************************************************************************ */
static int cyttsp_putbl(struct cyttsp *ts, int show,
			int show_status, int show_version, int show_cid)
{
	int retval = CY_OK;

	int num_bytes = (show_status * 3) + (show_version * 6) + (show_cid * 3);

	if (show_cid)
		num_bytes = sizeof(struct cyttsp_bootloader_data_t);
	else if (show_version)
		num_bytes = sizeof(struct cyttsp_bootloader_data_t) - 3;
	else
		num_bytes = sizeof(struct cyttsp_bootloader_data_t) - 9;

	if (show) {
		retval = i2c_smbus_read_i2c_block_data(ts->client,
			CY_REG_BASE, num_bytes, (u8 *)&g_bl_data);
		if (show_status) {
			cyttsp_debug("BL%d: f=%02X s=%02X err=%02X bl=%02X%02X bld=%02X%02X\n", \
				show, \
				g_bl_data.bl_file, \
				g_bl_data.bl_status, \
				g_bl_data.bl_error, \
				g_bl_data.blver_hi, g_bl_data.blver_lo, \
				g_bl_data.bld_blver_hi, g_bl_data.bld_blver_lo);
		}
		if (show_version) {
			cyttsp_debug("BL%d: ttspver=0x%02X%02X appid=0x%02X%02X appver=0x%02X%02X\n", \
				show, \
				g_bl_data.ttspver_hi, g_bl_data.ttspver_lo, \
				g_bl_data.appid_hi, g_bl_data.appid_lo, \
				g_bl_data.appver_hi, g_bl_data.appver_lo);
		}
		if (show_cid) {
			cyttsp_debug("BL%d: cid=0x%02X%02X%02X\n", \
				show, \
				g_bl_data.cid_0, \
				g_bl_data.cid_1, \
				g_bl_data.cid_2);
		}
	}

	return retval;
}

#ifdef CY_INCLUDE_LOAD_FILE
#define CY_MAX_I2C_LEN	256
#define CY_MAX_TRY		10
#define CY_BL_PAGE_SIZE	16
#define CY_BL_NUM_PAGES	5
static int cyttsp_i2c_wr_blk_chunks(struct cyttsp *ts, u8 command,
	u8 length, const u8 *values)
{
	int retval = CY_OK;
	int block = 1;

	u8 dataray[CY_MAX_I2C_LEN];

	/* first page already includes the bl page offset */
	retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
		CY_BL_PAGE_SIZE+1, values);
	values += CY_BL_PAGE_SIZE+1;
	length -= CY_BL_PAGE_SIZE+1;

	/* rem blocks require bl page offset stuffing */
	while (length &&
		(block < CY_BL_NUM_PAGES) &&
		!(retval < CY_OK)) {
		udelay(43*2);	/* TRM * 2 */
		dataray[0] = CY_BL_PAGE_SIZE*block;
		memcpy(&dataray[1], values,
			length >= CY_BL_PAGE_SIZE ?
			CY_BL_PAGE_SIZE : length);
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_BASE,
			length >= CY_BL_PAGE_SIZE ?
			CY_BL_PAGE_SIZE + 1 : length+1, dataray);
		values += CY_BL_PAGE_SIZE;
		length = length >= CY_BL_PAGE_SIZE ?
			length - CY_BL_PAGE_SIZE : 0;
		block++;
	}

	return retval;
}

static int cyttsp_bootload_app(struct cyttsp *ts)
{
	int retval = CY_OK;
	int i, tries;
	u8 host_reg;

	cyttsp_debug("load new firmware \n");
	/* reset TTSP Device back to bootloader mode */
	host_reg = CY_SOFT_RESET_MODE;
	retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
		sizeof(host_reg), &host_reg);
	/* wait for TTSP Device to complete reset back to bootloader */
	tries = 0;
	do {
		mdelay(1);
		cyttsp_putbl(ts, 3, false, false, false);
	} while (g_bl_data.bl_status != 0x10 &&
		g_bl_data.bl_status != 0x11 &&
		tries++ < 100);
	cyttsp_debug("load file - tver=0x%02X%02X a_id=0x%02X%02X aver=0x%02X%02X\n", \
		cyttsp_fw_tts_verh, cyttsp_fw_tts_verl, \
		cyttsp_fw_app_idh, cyttsp_fw_app_idl, \
		cyttsp_fw_app_verh, cyttsp_fw_app_verl);

	/* download new TTSP Application to the Bootloader */
	if (!(retval < CY_OK)) {
		i = 0;
		/* send bootload initiation command */
		if (cyttsp_fw[i].Command == CY_BL_INIT_LOAD) {
			g_bl_data.bl_file = 0;
			g_bl_data.bl_status = 0;
			g_bl_data.bl_error = 0;
			retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE,
				cyttsp_fw[i].Length, cyttsp_fw[i].Block);
			/* delay to allow bl to get ready for block writes */
			i++;
			tries = 0;
			do {
				mdelay(100);
				cyttsp_putbl(ts, 4, false, false, false);
			} while (g_bl_data.bl_status != 0x10 &&
				g_bl_data.bl_status != 0x11 &&
				tries++ < 100);
			cyttsp_debug("wait init f=%02X, s=%02X, e=%02X t=%d\n", \
				g_bl_data.bl_file, g_bl_data.bl_status, \
				g_bl_data.bl_error, tries);
			/* send bootload firmware load blocks */
			if (!(retval < CY_OK)) {
				while (cyttsp_fw[i].Command == CY_BL_WRITE_BLK) {
					retval = cyttsp_i2c_wr_blk_chunks(ts,
						CY_REG_BASE,
						cyttsp_fw[i].Length,
						cyttsp_fw[i].Block);
					cyttsp_xdebug("BL DNLD Rec=% 3d Len=% 3d Addr=%04X\n", \
						cyttsp_fw[i].Record, \
						cyttsp_fw[i].Length, \
						cyttsp_fw[i].Address);
					i++;
					if (retval < CY_OK) {
						cyttsp_debug("BL fail Rec=%3d retval=%d\n", \
							cyttsp_fw[i-1].Record, \
							retval);
						break;
					} else {
						tries = 0;
						cyttsp_putbl(ts, 5, false, false, false);
						while (!((g_bl_data.bl_status == 0x10) &&
							(g_bl_data.bl_error == 0x20)) &&
							!((g_bl_data.bl_status == 0x11) &&
							(g_bl_data.bl_error == 0x20)) &&
							(tries++ < 100)) {
							mdelay(1);
							cyttsp_putbl(ts, 5, false, false, false);
						}
					}
				}

				if (!(retval < CY_OK)) {
					while (i < cyttsp_fw_records) {
						retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
							cyttsp_fw[i].Length,
							cyttsp_fw[i].Block);
						i++;
						tries = 0;
						do {
							mdelay(100);
							cyttsp_putbl(ts, 6, true, false, false);
						} while (g_bl_data.bl_status != 0x10 &&
							g_bl_data.bl_status != 0x11 &&
							tries++ < 100);
						cyttsp_debug("wait term f=%02X, s=%02X, e=%02X t=%d\n", \
							g_bl_data.bl_file, \
							g_bl_data.bl_status, \
							g_bl_data.bl_error, \
							tries);
						if (retval < CY_OK)
							break;
					}
				}
			}
		}
	}

	/* reset TTSP Device back to bootloader mode */
	host_reg = CY_SOFT_RESET_MODE;
	retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
		sizeof(host_reg), &host_reg);
	/* wait for TTSP Device to complete reset back to bootloader */
	tries = 0;
	do {
		mdelay(1);
		cyttsp_putbl(ts, 3, false, false, false);
	} while (g_bl_data.bl_status != 0x10 &&
		g_bl_data.bl_status != 0x11 &&
		tries++ < 100);

	/* set arg2 to non-0 to activate */
	retval = cyttsp_putbl(ts, 8, true, true, true);

	return retval;
}
#else
static int cyttsp_bootload_app(struct cyttsp *ts)
{
	cyttsp_debug("no-load new firmware \n");
	return CY_OK;
}
#endif /* CY_INCLUDE_LOAD_FILE */


static int cyttsp_power_on(struct cyttsp *ts)
{
	int retval = CY_OK;
	u8 host_reg;
	int tries;

	cyttsp_debug("Power up \n");

	/* check if the TTSP device has a bootloader installed */
	host_reg = CY_SOFT_RESET_MODE;
	retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
		sizeof(host_reg), &host_reg);
	tries = 0;
	do {
		mdelay(1);

		/* set arg2 to non-0 to activate */
		retval = cyttsp_putbl(ts, 1, true, true, true);
		cyttsp_info("BL%d: f=%02X s=%02X err=%02X bl=%02X%02X bld=%02X%02X R=%d\n", \
			101, \
			g_bl_data.bl_file, g_bl_data.bl_status, \
			g_bl_data.bl_error, \
			g_bl_data.blver_hi, g_bl_data.blver_lo, \
			g_bl_data.bld_blver_hi, g_bl_data.bld_blver_lo,
			retval);
		cyttsp_info("BL%d: tver=%02X%02X a_id=%02X%02X aver=%02X%02X\n", \
			102, \
			g_bl_data.ttspver_hi, g_bl_data.ttspver_lo, \
			g_bl_data.appid_hi, g_bl_data.appid_lo, \
			g_bl_data.appver_hi, g_bl_data.appver_lo);
		cyttsp_info("BL%d: c_id=%02X%02X%02X\n", \
			103, \
			g_bl_data.cid_0, g_bl_data.cid_1, g_bl_data.cid_2);
	} while (!(retval < CY_OK) &&
		!GET_BOOTLOADERMODE(g_bl_data.bl_status) &&
		!(g_bl_data.bl_file == CY_OP_MODE + CY_LOW_PWR_MODE) &&
		tries++ < 100);

	/* is bootloader missing? */
	if (!(retval < CY_OK)) {
		cyttsp_xdebug("Ret=%d  Check if bootloader is missing...\n", \
			retval);
		if (!GET_BOOTLOADERMODE(g_bl_data.bl_status)) {
			/* skip all bl and sys info and go to op mode */
			if (!(retval < CY_OK)) {
				cyttsp_xdebug("Bl is missing (ret=%d)\n", \
					retval);
				host_reg = CY_OP_MODE/* + CY_LOW_PWR_MODE*/;
				retval = i2c_smbus_write_i2c_block_data(ts->client, CY_REG_BASE,
					sizeof(host_reg), &host_reg);
				/* wait for TTSP Device to complete switch to
				 * Operational mode */
				mdelay(1000);
				goto bypass;
			}
		}
	}


	/* take TTSP out of bootloader mode; go to TrueTouch operational mode */
	if (!(retval < CY_OK)) {
		cyttsp_xdebug1("exit bootloader; go operational\n");
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_BASE, sizeof(bl_cmd), bl_cmd);
		tries = 0;
		do {
			mdelay(100);
			cyttsp_putbl(ts, 4, true, false, false);
			cyttsp_info("BL%d: f=%02X s=%02X err=%02X bl=%02X%02X bld=%02X%02X\n", \
				104, \
				g_bl_data.bl_file, g_bl_data.bl_status, \
				g_bl_data.bl_error, \
				g_bl_data.blver_hi, g_bl_data.blver_lo, \
				g_bl_data.bld_blver_hi, g_bl_data.bld_blver_lo);
		} while (GET_BOOTLOADERMODE(g_bl_data.bl_status) &&
			tries++ < 100);
	}



	if (!(retval < CY_OK) &&
		cyttsp_app_load()) {
		if (CY_DIFF(g_bl_data.ttspver_hi, cyttsp_tts_verh())  ||
			CY_DIFF(g_bl_data.ttspver_lo, cyttsp_tts_verl())  ||
			CY_DIFF(g_bl_data.appid_hi, cyttsp_app_idh())  ||
			CY_DIFF(g_bl_data.appid_lo, cyttsp_app_idl())  ||
			CY_DIFF(g_bl_data.appver_hi, cyttsp_app_verh())  ||
			CY_DIFF(g_bl_data.appver_lo, cyttsp_app_verl())  ||
			CY_DIFF(g_bl_data.cid_0, cyttsp_cid_0())  ||
			CY_DIFF(g_bl_data.cid_1, cyttsp_cid_1())  ||
			CY_DIFF(g_bl_data.cid_2, cyttsp_cid_2())  ||
			cyttsp_force_fw_load()) {
			cyttsp_debug("blttsp=0x%02X%02X flttsp=0x%02X%02X force=%d\n", \
				g_bl_data.ttspver_hi, g_bl_data.ttspver_lo, \
				cyttsp_tts_verh(), cyttsp_tts_verl(), \
				cyttsp_force_fw_load());
			cyttsp_debug("blappid=0x%02X%02X flappid=0x%02X%02X\n", \
				g_bl_data.appid_hi, g_bl_data.appid_lo, \
				cyttsp_app_idh(), cyttsp_app_idl());
			cyttsp_debug("blappver=0x%02X%02X flappver=0x%02X%02X\n", \
				g_bl_data.appver_hi, g_bl_data.appver_lo, \
				cyttsp_app_verh(), cyttsp_app_verl());
			cyttsp_debug("blcid=0x%02X%02X%02X flcid=0x%02X%02X%02X\n", \
				g_bl_data.cid_0, \
				g_bl_data.cid_1, \
				g_bl_data.cid_2, \
				cyttsp_cid_0(), \
				cyttsp_cid_1(), \
				cyttsp_cid_2());
			/* enter bootloader to load new app into TTSP Device */
			retval = cyttsp_bootload_app(ts);
			/* take TTSP device out of bootloader mode;
			 * switch back to TrueTouch operational mode */
			if (!(retval < CY_OK)) {
				retval = i2c_smbus_write_i2c_block_data(ts->client,
					CY_REG_BASE,
					sizeof(bl_cmd), bl_cmd);
				/* wait for TTSP Device to complete
				 * switch to Operational mode */
				tries = 0;
				do {
					mdelay(100);
					cyttsp_putbl(ts, 9, false, false, false);
				} while (GET_BOOTLOADERMODE(g_bl_data.bl_status) &&
					tries++ < 100);
				cyttsp_putbl(ts, 9, true, false, false);
			}
		}
	}

bypass:
	/* switch to System Information mode to read versions
	 * and set interval registers */
	if (!(retval < CY_OK)) {
		cyttsp_debug("switch to sysinfo mode \n");
		host_reg = CY_SYSINFO_MODE;
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_BASE, sizeof(host_reg), &host_reg);
		/* wait for TTSP Device to complete switch to SysInfo mode */
		mdelay(100);
		if (!(retval < CY_OK)) {
			retval = i2c_smbus_read_i2c_block_data(ts->client,
				CY_REG_BASE,
				sizeof(struct cyttsp_sysinfo_data_t),
				(u8 *)&g_sysinfo_data);
			cyttsp_debug("SI2: hst_mode=0x%02X mfg_cmd=0x%02X mfg_stat=0x%02X\n", \
				g_sysinfo_data.hst_mode, \
				g_sysinfo_data.mfg_cmd, \
				g_sysinfo_data.mfg_stat);
			cyttsp_debug("SI2: bl_ver=0x%02X%02X\n", \
				g_sysinfo_data.bl_verh, \
				g_sysinfo_data.bl_verl);
			cyttsp_debug("SI2: sysinfo act_int=0x%02X tch_tmout=0x%02X lp_int=0x%02X\n", \
				g_sysinfo_data.act_intrvl, \
				g_sysinfo_data.tch_tmout, \
				g_sysinfo_data.lp_intrvl);
			cyttsp_info("SI%d: tver=%02X%02X a_id=%02X%02X aver=%02X%02X\n", \
				102, \
				g_sysinfo_data.tts_verh, \
				g_sysinfo_data.tts_verl, \
				g_sysinfo_data.app_idh, \
				g_sysinfo_data.app_idl, \
				g_sysinfo_data.app_verh, \
				g_sysinfo_data.app_verl);
			cyttsp_info("SI%d: c_id=%02X%02X%02X\n", \
				103, \
				g_sysinfo_data.cid[0], \
				g_sysinfo_data.cid[1], \
				g_sysinfo_data.cid[2]);
			if (!(retval < CY_OK) &&
				(CY_DIFF(ts->platform_data->act_intrvl,
					CY_ACT_INTRVL_DFLT)  ||
				CY_DIFF(ts->platform_data->tch_tmout,
					CY_TCH_TMOUT_DFLT) ||
				CY_DIFF(ts->platform_data->lp_intrvl,
					CY_LP_INTRVL_DFLT))) {
				if (!(retval < CY_OK)) {
					u8 intrvl_ray[sizeof(ts->platform_data->act_intrvl) +
						sizeof(ts->platform_data->tch_tmout) +
						sizeof(ts->platform_data->lp_intrvl)];
					u8 i = 0;

					intrvl_ray[i++] =
						ts->platform_data->act_intrvl;
					intrvl_ray[i++] =
						ts->platform_data->tch_tmout;
					intrvl_ray[i++] =
						ts->platform_data->lp_intrvl;

					cyttsp_debug("SI2: platinfo act_intrvl=0x%02X tch_tmout=0x%02X lp_intrvl=0x%02X\n", \
						ts->platform_data->act_intrvl, \
						ts->platform_data->tch_tmout, \
						ts->platform_data->lp_intrvl);
					/* set intrvl registers */
					retval = i2c_smbus_write_i2c_block_data(
						ts->client,
						CY_REG_ACT_INTRVL,
						sizeof(intrvl_ray), intrvl_ray);
					mdelay(CY_DLY_SYSINFO);
				}
			}
		}
		/* switch back to Operational mode */
		cyttsp_debug("switch back to operational mode \n");
		if (!(retval < CY_OK)) {
			host_reg = CY_OP_MODE/* + CY_LOW_PWR_MODE*/;
			retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE,
				sizeof(host_reg), &host_reg);
			/* wait for TTSP Device to complete
			 * switch to Operational mode */
			mdelay(100);
		}
	}
	/* init gesture setup;
	 * this is required even if not using gestures
	 * in order to set the active distance */
	if (!(retval < CY_OK)) {
		u8 gesture_setup;
		cyttsp_debug("init gesture setup \n");
		gesture_setup = ts->platform_data->gest_set;
		retval = i2c_smbus_write_i2c_block_data(ts->client,
			CY_REG_GEST_SET,
			sizeof(gesture_setup), &gesture_setup);
		mdelay(CY_DLY_DFLT);
	}

	if (!(retval < CY_OK))
		ts->platform_data->power_state = CY_ACTIVE_STATE;
	else
		ts->platform_data->power_state = CY_IDLE_STATE;

	cyttsp_debug("Retval=%d Power state is %s\n", \
		retval, \
		ts->platform_data->power_state == CY_ACTIVE_STATE ? \
		 "ACTIVE" : "IDLE");

	return retval;
}

static int cyttsp_power_device(struct cyttsp *ts, bool on)
{
	int rc = 0, i;
	const struct cyttsp_regulator *reg_info =
				ts->platform_data->regulator_info;
	u8 num_reg = ts->platform_data->num_regulators;

	if (!reg_info) {
		pr_err("regulator pdata not specified\n");
		return -EINVAL;
	}

	if (on == false) /* Turn off the regulators */
		goto ts_reg_disable;

	ts->vdd = kzalloc(num_reg * sizeof(struct regulator *), GFP_KERNEL);
	if (!ts->vdd) {
		pr_err("unable to allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_reg; i++) {
		ts->vdd[i] = regulator_get(&ts->client->dev, reg_info[i].name);
		if (IS_ERR(ts->vdd[i])) {
			rc = PTR_ERR(ts->vdd[i]);
			pr_err("%s:regulator get failed rc=%d\n",
							__func__, rc);
			goto error_vdd;
		}

		if (regulator_count_voltages(ts->vdd[i]) > 0) {
			rc = regulator_set_voltage(ts->vdd[i],
				reg_info[i].min_uV, reg_info[i].max_uV);
			if (rc) {
				pr_err("%s: regulator_set_voltage"
					"failed rc =%d\n", __func__, rc);
				regulator_put(ts->vdd[i]);
				goto error_vdd;
			}
		}

		rc = regulator_set_optimum_mode(ts->vdd[i],
						reg_info[i].load_uA);
		if (rc < 0) {
			pr_err("%s: regulator_set_optimum_mode failed rc=%d\n",
								__func__, rc);

			regulator_set_voltage(ts->vdd[i], 0,
						reg_info[i].max_uV);
			regulator_put(ts->vdd[i]);
			goto error_vdd;
		}

		rc = regulator_enable(ts->vdd[i]);
		if (rc) {
			pr_err("%s: regulator_enable failed rc =%d\n",
								__func__, rc);
			regulator_set_optimum_mode(ts->vdd[i], 0);
			regulator_set_voltage(ts->vdd[i], 0,
						reg_info[i].max_uV);
			regulator_put(ts->vdd[i]);
			goto error_vdd;
		}
	}

	return rc;

ts_reg_disable:
	i = ts->platform_data->num_regulators;
error_vdd:
	while (--i >= 0) {
		if (regulator_count_voltages(ts->vdd[i]) > 0)
			regulator_set_voltage(ts->vdd[i], 0,
						reg_info[i].max_uV);
		regulator_set_optimum_mode(ts->vdd[i], 0);
		regulator_disable(ts->vdd[i]);
		regulator_put(ts->vdd[i]);
	}
	kfree(ts->vdd);
	return rc;
}

/* cyttsp_initialize: Driver Initialization. This function takes
 * care of the following tasks:
 * 1. Create and register an input device with input layer
 * 2. Take CYTTSP device out of bootloader mode; go operational
 * 3. Start any timers/Work queues.  */
static int cyttsp_initialize(struct i2c_client *client, struct cyttsp *ts)
{
	struct input_dev *input_device;
	int error = 0;
	int retval = CY_OK;
	u8 id;

	/* Create the input device and register it. */
	input_device = input_allocate_device();
	if (!input_device) {
		error = -ENOMEM;
		cyttsp_xdebug1("err input allocate device\n");
		goto error_free_device;
	}

	if (!client) {
		error = ~ENODEV;
		cyttsp_xdebug1("err client is Null\n");
		goto error_free_device;
	}

	if (!ts) {
		error = ~ENODEV;
		cyttsp_xdebug1("err context is Null\n");
		goto error_free_device;
	}

	ts->input = input_device;
	input_device->name = CY_I2C_NAME;
	input_device->phys = ts->phys;
	input_device->dev.parent = &client->dev;

	/* init the touch structures */
	ts->num_prv_st_tch = CY_NTCH;
	for (id = 0; id < CY_NUM_TRK_ID; id++) {
		ts->act_trk[id] = CY_NTCH;
		ts->prv_mt_pos[id][CY_XPOS] = 0;
		ts->prv_mt_pos[id][CY_YPOS] = 0;
	}

	for (id = 0; id < CY_NUM_MT_TCH_ID; id++)
		ts->prv_mt_tch[id] = CY_IGNR_TCH;

	for (id = 0; id < CY_NUM_ST_TCH_ID; id++)
		ts->prv_st_tch[id] = CY_IGNR_TCH;

	set_bit(EV_SYN, input_device->evbit);
	set_bit(EV_KEY, input_device->evbit);
	set_bit(EV_ABS, input_device->evbit);
	set_bit(BTN_TOUCH, input_device->keybit);
	set_bit(BTN_2, input_device->keybit);
	if (ts->platform_data->use_gestures)
		set_bit(BTN_3, input_device->keybit);

	input_set_abs_params(input_device, ABS_X, ts->platform_data->disp_minx,
		ts->platform_data->disp_maxx, 0, 0);
	input_set_abs_params(input_device, ABS_Y, ts->platform_data->disp_miny,
		ts->platform_data->disp_maxy, 0, 0);
	input_set_abs_params(input_device,
		ABS_TOOL_WIDTH, 0, CY_LARGE_TOOL_WIDTH, 0 , 0);
	input_set_abs_params(input_device,
		ABS_PRESSURE, 0, CY_MAXZ, 0, 0);
	input_set_abs_params(input_device,
		ABS_HAT0X, 0, ts->platform_data->panel_maxx, 0, 0);
	input_set_abs_params(input_device,
		ABS_HAT0Y, 0, ts->platform_data->panel_maxy, 0, 0);
	if (ts->platform_data->use_gestures) {
		input_set_abs_params(input_device,
			ABS_HAT1X, 0, CY_MAXZ, 0, 0);
		input_set_abs_params(input_device,
			ABS_HAT1Y, 0, CY_MAXZ, 0, 0);
	}
	if (ts->platform_data->use_mt) {
		input_set_abs_params(input_device, ABS_MT_POSITION_X,
			ts->platform_data->disp_minx,
			ts->platform_data->disp_maxx, 0, 0);
		input_set_abs_params(input_device, ABS_MT_POSITION_Y,
			ts->platform_data->disp_miny,
			ts->platform_data->disp_maxy, 0, 0);
		input_set_abs_params(input_device,
			ABS_MT_TOUCH_MAJOR, 0, CY_MAXZ, 0, 0);
		input_set_abs_params(input_device,
			ABS_MT_WIDTH_MAJOR, 0, CY_LARGE_TOOL_WIDTH, 0, 0);
		if (ts->platform_data->use_trk_id) {
			input_set_abs_params(input_device,
				ABS_MT_TRACKING_ID, 0, CY_NUM_TRK_ID, 0, 0);
		}
	}

	/* set dummy key to make driver work with virtual keys */
	input_set_capability(input_device, EV_KEY, KEY_PROG1);

	cyttsp_info("%s: Register input device\n", CY_I2C_NAME);
	error = input_register_device(input_device);
	if (error) {
		cyttsp_alert("%s: Failed to register input device\n", \
			CY_I2C_NAME);
		retval = error;
		goto error_free_device;
	}

	/* Prepare our worker structure prior to setting up the timer/ISR */
	INIT_WORK(&ts->work, cyttsp_xy_worker);

	if (gpio_is_valid(ts->platform_data->resout_gpio)) {
		/* configure touchscreen reset out gpio */
		retval = gpio_request(ts->platform_data->resout_gpio,
						"cyttsp_resout_gpio");
		if (retval) {
			pr_err("%s: unable to request reset gpio %d\n",
				__func__, ts->platform_data->resout_gpio);
			goto error_free_device;
		}

		retval = gpio_direction_output(
					ts->platform_data->resout_gpio, 1);
		if (retval) {
			pr_err("%s: unable to set direction for gpio %d\n",
				__func__, ts->platform_data->resout_gpio);
			goto error_resout_gpio_dir;
		}
	}

	if (gpio_is_valid(ts->platform_data->sleep_gpio)) {
		/* configure touchscreen reset out gpio */
		retval = gpio_request(ts->platform_data->sleep_gpio,
						"cy8c_sleep_gpio");
		if (retval) {
			pr_err("%s: unable to request sleep gpio %d\n",
				__func__, ts->platform_data->sleep_gpio);
			goto error_sleep_gpio_req;
		}

		retval = gpio_direction_output(
					ts->platform_data->sleep_gpio, 0);
		if (retval) {
			pr_err("%s: unable to set direction for gpio %d\n",
			__func__, ts->platform_data->resout_gpio);
			goto error_sleep_gpio_dir;
		}
	}

	if (gpio_is_valid(ts->platform_data->irq_gpio)) {
		/* configure touchscreen irq gpio */
		retval = gpio_request(ts->platform_data->irq_gpio,
							"ts_irq_gpio");
		if (retval) {
			pr_err("%s: unable to request gpio [%d]\n", __func__,
						ts->platform_data->irq_gpio);
			goto error_irq_gpio_req;
		}
		retval = gpio_direction_input(ts->platform_data->irq_gpio);
		if (retval) {
			pr_err("%s: unable to set_direction for gpio [%d]\n",
					__func__, ts->platform_data->irq_gpio);
			goto error_irq_gpio_dir;
		}
	}

	if (ts->platform_data->regulator_info) {
		retval = cyttsp_power_device(ts, true);
		if (retval) {
			pr_err("%s: Unable to power device %d\n",
						 __func__, retval);
			goto error_irq_gpio_dir;
		}
	}

	/* Power on the chip and make sure that I/Os are set as specified
	 * in the platform */
	if (ts->platform_data->init) {
		retval = ts->platform_data->init(client);
		if (retval) {
			pr_err("%s: ts init failed\n", __func__);
			goto error_power_device;
		}
	}

	msleep(100);

	/* check this device active by reading first byte/register */
	retval = i2c_smbus_read_byte_data(ts->client, 0x01);
	if (retval < 0) {
		pr_err("%s: i2c sanity check failed\n", __func__);
		goto error_power_device;
	}

	retval = cyttsp_power_on(ts);
	if (retval < 0) {
		pr_err("%s: cyttsp_power_on failed\n", __func__);
		goto error_power_device;
	}

	/* Timer or Interrupt setup */
	if (ts->client->irq == 0) {
		cyttsp_info("Setting up timer\n");
		setup_timer(&ts->timer, cyttsp_timer, (unsigned long) ts);
		mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
	} else {
		cyttsp_info("Setting up interrupt\n");
		/* request_irq() will also call enable_irq() */
		error = request_irq(client->irq, cyttsp_irq,
			IRQF_TRIGGER_FALLING,
			client->dev.driver->name, ts);
		if (error) {
			cyttsp_alert("error: could not request irq\n");
			retval = error;
			goto error_power_device;
		}
	}

	irq_cnt = 0;
	irq_cnt_total = 0;
	irq_err_cnt = 0;

	atomic_set(&ts->irq_enabled, 1);
	retval = device_create_file(&ts->client->dev, &dev_attr_irq_enable);
	if (retval < CY_OK) {
		cyttsp_alert("File device creation failed: %d\n", retval);
		retval = -ENODEV;
		goto error_free_irq;
	}

	retval = device_create_file(&client->dev, &dev_attr_cyttsp_fw_ver);
	if (retval) {
		cyttsp_alert("sysfs entry for firmware version failed\n");
		goto error_rm_dev_file_irq_en;
	}

	ts->cyttsp_fwloader_mode = 0;
	retval = device_create_file(&client->dev, &dev_attr_cyttsp_update_fw);
	if (retval) {
		cyttsp_alert("sysfs entry for firmware update failed\n");
		goto error_rm_dev_file_fw_ver;
	}

	retval = device_create_file(&client->dev,
				&dev_attr_cyttsp_force_update_fw);
	if (retval) {
		cyttsp_alert("sysfs entry for force firmware update failed\n");
		goto error_rm_dev_file_update_fw;
	}
	if (ts->platform_data->correct_fw_ver) {
		if (g_bl_data.appid_lo != ts->platform_data->correct_fw_ver)
			printk(KERN_INFO "Please update touchscreen firmware\n");
	}

	retval = device_create_file(&client->dev,
				&dev_attr_cyttsp_fw_name);
	if (retval) {
		cyttsp_alert("sysfs entry for file name selection failed\n");
		goto error_rm_dev_file_fupdate_fw;
	}

	cyttsp_info("%s: Successful registration\n", CY_I2C_NAME);

	goto success;

error_rm_dev_file_fupdate_fw:
	device_remove_file(&client->dev, &dev_attr_cyttsp_force_update_fw);
error_rm_dev_file_update_fw:
	device_remove_file(&client->dev, &dev_attr_cyttsp_update_fw);
error_rm_dev_file_fw_ver:
	device_remove_file(&client->dev, &dev_attr_cyttsp_fw_ver);
error_rm_dev_file_irq_en:
	device_remove_file(&client->dev, &dev_attr_irq_enable);
error_free_irq:
	if (ts->client->irq)
		free_irq(client->irq, ts);
error_power_device:
	if (ts->platform_data->regulator_info)
		cyttsp_power_device(ts, false);
error_irq_gpio_dir:
	if (gpio_is_valid(ts->platform_data->irq_gpio))
		gpio_free(ts->platform_data->irq_gpio);
error_irq_gpio_req:
	if (gpio_is_valid(ts->platform_data->sleep_gpio))
		gpio_direction_output(ts->platform_data->sleep_gpio, 1);
error_sleep_gpio_dir:
	if (gpio_is_valid(ts->platform_data->sleep_gpio))
		gpio_free(ts->platform_data->sleep_gpio);
error_sleep_gpio_req:
	if (gpio_is_valid(ts->platform_data->resout_gpio))
		gpio_direction_output(ts->platform_data->resout_gpio, 0);
error_resout_gpio_dir:
	if (gpio_is_valid(ts->platform_data->resout_gpio))
		gpio_free(ts->platform_data->resout_gpio);
error_free_device:
	if (input_device)
		input_free_device(input_device);

success:
	return retval;
}

/* I2C driver probe function */
static int __devinit cyttsp_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct cyttsp *ts;
	int error;
	int retval = CY_OK;

	cyttsp_info("Start Probe 1.2\n");

	/* allocate and clear memory */
	ts = kzalloc(sizeof(struct cyttsp), GFP_KERNEL);
	if (ts == NULL) {
		cyttsp_xdebug1("err kzalloc for cyttsp\n");
		retval = -ENOMEM;
	}

	/* Enable runtime PM ops, start in ACTIVE mode */
	error = pm_runtime_set_active(&client->dev);
	if (error < 0)
		dev_dbg(&client->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&client->dev);

	if (!(retval < CY_OK)) {
		/* register driver_data */
		ts->client = client;
		ts->platform_data = client->dev.platform_data;

		if (ts->platform_data->fw_fname)
			strncpy(ts->fw_fname, ts->platform_data->fw_fname,
							FW_FNAME_LEN - 1);
		else
			strncpy(ts->fw_fname, "cyttsp.hex", FW_FNAME_LEN - 1);

		i2c_set_clientdata(client, ts);

		error = cyttsp_initialize(client, ts);
		if (error) {
			cyttsp_xdebug1("err cyttsp_initialize\n");
			if (ts != NULL) {
				/* deallocate memory */
				kfree(ts);
			}
/*
			i2c_del_driver(&cyttsp_driver);
*/
			retval = -ENODEV;
		} else
			cyttsp_openlog();
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (!(retval < CY_OK)) {
		ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		ts->early_suspend.suspend = cyttsp_early_suspend;
		ts->early_suspend.resume = cyttsp_late_resume;
		register_early_suspend(&ts->early_suspend);
	}
#endif /* CONFIG_HAS_EARLYSUSPEND */
	device_init_wakeup(&client->dev, ts->platform_data->wakeup);
	mutex_init(&ts->mutex);

	cyttsp_info("Start Probe %s\n", \
		(retval < CY_OK) ? "FAIL" : "PASS");

	return retval;
}

/* Function to manage power-on resume */
static int cyttsp_resume(struct device *dev)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	int retval = CY_OK;

	cyttsp_debug("Wake Up\n");

	if (ts->is_suspended == false) {
		pr_err("%s: in wakeup state\n", __func__);
		return 0;
	}

	if (device_may_wakeup(dev)) {
		if (ts->client->irq)
			disable_irq_wake(ts->client->irq);
		return 0;
	}

	/* re-enable the interrupt prior to wake device */
	if (ts->client->irq)
		enable_irq(ts->client->irq);

	if (ts->platform_data->use_sleep &&
		(ts->platform_data->power_state != CY_ACTIVE_STATE)) {
		if (ts->platform_data->resume)
			retval = ts->platform_data->resume(ts->client);
		if (!(retval < CY_OK)) {
			/* take TTSP device out of bootloader mode;
			 * switch back to TrueTouch operational mode */
			if (!(retval < CY_OK)) {
				int tries;
				retval = i2c_smbus_write_i2c_block_data(ts->client,
					CY_REG_BASE,
					sizeof(bl_cmd), bl_cmd);
				/* wait for TTSP Device to complete
				 * switch to Operational mode */
				tries = 0;
				do {
					mdelay(100);
					cyttsp_putbl(ts, 16, false, false, false);
				} while (GET_BOOTLOADERMODE(g_bl_data.bl_status) &&
					tries++ < 100);
				cyttsp_putbl(ts, 16, true, false, false);
			}
		}
	}

	if (!(retval < CY_OK) &&
		(GET_HSTMODE(g_bl_data.bl_file) == CY_OK)) {
		ts->platform_data->power_state = CY_ACTIVE_STATE;

		/* re-enable the timer after resuming */
		if (ts->client->irq == 0)
			mod_timer(&ts->timer, jiffies + TOUCHSCREEN_TIMEOUT);
	} else
		retval = -ENODEV;

	ts->is_suspended = false;
	cyttsp_debug("Wake Up %s\n", \
		(retval < CY_OK) ? "FAIL" : "PASS");

	return retval;
}


/* Function to manage low power suspend */
static int cyttsp_suspend(struct device *dev)
{
	struct cyttsp *ts = dev_get_drvdata(dev);
	u8 sleep_mode = CY_OK;
	int retval = CY_OK;

	cyttsp_debug("Enter Sleep\n");

	if (ts->is_suspended == true) {
		pr_err("%s: in sleep state\n", __func__);
		return 0;
	}

	mutex_lock(&ts->mutex);
	if (ts->cyttsp_fwloader_mode) {
		pr_err("%s:firmware upgrade mode:"
			"suspend not allowed\n", __func__);
		mutex_unlock(&ts->mutex);
		return -EBUSY;
	}
	mutex_unlock(&ts->mutex);

	if (device_may_wakeup(dev)) {
		if (ts->client->irq)
			enable_irq_wake(ts->client->irq);
		return 0;
	}

	/* disable worker */
	if (ts->client->irq == 0)
		del_timer(&ts->timer);
	else
		disable_irq_nosync(ts->client->irq);
	retval = cancel_work_sync(&ts->work);

	if (retval)
		enable_irq(ts->client->irq);

	if (!(retval < CY_OK)) {
		if (ts->platform_data->use_sleep &&
			(ts->platform_data->power_state == CY_ACTIVE_STATE)) {
			if (ts->platform_data->use_sleep & CY_USE_DEEP_SLEEP_SEL)
				sleep_mode = CY_DEEP_SLEEP_MODE;
			else
				sleep_mode = CY_LOW_PWR_MODE;

			retval = i2c_smbus_write_i2c_block_data(ts->client,
				CY_REG_BASE,
				sizeof(sleep_mode), &sleep_mode);
		}
	}

	if (!(retval < CY_OK)) {
		if (sleep_mode == CY_DEEP_SLEEP_MODE)
			ts->platform_data->power_state = CY_SLEEP_STATE;
		else if (sleep_mode == CY_LOW_PWR_MODE)
			ts->platform_data->power_state = CY_LOW_PWR_STATE;
	}

	ts->is_suspended = true;
	cyttsp_debug("Sleep Power state is %s\n", \
		(ts->platform_data->power_state == CY_ACTIVE_STATE) ? \
		"ACTIVE" : \
		((ts->platform_data->power_state == CY_SLEEP_STATE) ? \
		"SLEEP" : "LOW POWER"));

	return retval;
}

/* registered in driver struct */
static int __devexit cyttsp_remove(struct i2c_client *client)
{
	/* clientdata registered on probe */
	struct cyttsp *ts = i2c_get_clientdata(client);
	int err;

	cyttsp_alert("Unregister\n");

	pm_runtime_set_suspended(&client->dev);
	pm_runtime_disable(&client->dev);

	device_init_wakeup(&client->dev, 0);
	device_remove_file(&ts->client->dev, &dev_attr_irq_enable);
	device_remove_file(&client->dev, &dev_attr_cyttsp_fw_ver);
	device_remove_file(&client->dev, &dev_attr_cyttsp_update_fw);
	device_remove_file(&client->dev, &dev_attr_cyttsp_force_update_fw);
	device_remove_file(&client->dev, &dev_attr_cyttsp_fw_name);

	/* Start cleaning up by removing any delayed work and the timer */
	if (cancel_delayed_work((struct delayed_work *)&ts->work) < CY_OK)
		cyttsp_alert("error: could not remove work from workqueue\n");

	/* free up timer or irq */
	if (ts->client->irq == 0) {
		err = del_timer(&ts->timer);
		if (err < CY_OK)
			cyttsp_alert("error: failed to delete timer\n");
	} else
		free_irq(client->irq, ts);

	if (ts->platform_data->regulator_info)
		cyttsp_power_device(ts, false);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	mutex_destroy(&ts->mutex);

	if (gpio_is_valid(ts->platform_data->sleep_gpio)) {
		gpio_direction_output(ts->platform_data->sleep_gpio, 1);
		gpio_free(ts->platform_data->sleep_gpio);
	}

	if (gpio_is_valid(ts->platform_data->resout_gpio)) {
		gpio_direction_output(ts->platform_data->resout_gpio, 0);
		gpio_free(ts->platform_data->resout_gpio);
	}

	if (gpio_is_valid(ts->platform_data->irq_gpio))
		gpio_free(ts->platform_data->irq_gpio);

	/* housekeeping */
	if (ts != NULL)
		kfree(ts);

	cyttsp_alert("Leaving\n");

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp_early_suspend(struct early_suspend *handler)
{
	struct cyttsp *ts;

	ts = container_of(handler, struct cyttsp, early_suspend);
	cyttsp_suspend(&ts->client->dev);
}

static void cyttsp_late_resume(struct early_suspend *handler)
{
	struct cyttsp *ts;

	ts = container_of(handler, struct cyttsp, early_suspend);
	cyttsp_resume(&ts->client->dev);
}
#endif  /* CONFIG_HAS_EARLYSUSPEND */

static int cyttsp_init(void)
{
	int ret;

	cyttsp_info("Cypress TrueTouch(R) Standard Product\n");
	cyttsp_info("I2C Touchscreen Driver (Built %s @ %s)\n", \
		__DATE__, __TIME__);

	cyttsp_ts_wq = create_singlethread_workqueue("cyttsp_ts_wq");
	if (cyttsp_ts_wq == NULL) {
		cyttsp_debug("No memory for cyttsp_ts_wq\n");
		return -ENOMEM;
	}

	ret = i2c_add_driver(&cyttsp_driver);

	return ret;
}

static void cyttsp_exit(void)
{
	if (cyttsp_ts_wq)
		destroy_workqueue(cyttsp_ts_wq);
	return i2c_del_driver(&cyttsp_driver);
}

module_init(cyttsp_init);
module_exit(cyttsp_exit);
MODULE_FIRMWARE("cyttsp.fw");

