#include "ois_core.h"
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/delay.h>

#define DRIVER_NAME "XIAOMI_OIS_CORE"

#define LOG_INF(format, args...)                                               \
	pr_info(DRIVER_NAME " [%s] " format, __func__, ##args)

#define MAX_BUF_SIZE 255
#define MAX_MSG_NUM_U8 (MAX_BUF_SIZE / 3)
#define MAX_MSG_NUM_U16 (MAX_BUF_SIZE / 4)
#define MAX_VAL_NUM_U8 (MAX_BUF_SIZE - 2)
#define MAX_VAL_NUM_U16 ((MAX_BUF_SIZE - 2) >> 1)

#define BURST_MAX_BUF_SIZE 1020 // 255 * 4
#define BURST_MAX_MSG_NUM_U8  (BURST_MAX_BUF_SIZE / 3)
#define BURST_MAX_MSG_NUM_U16 (BURST_MAX_BUF_SIZE / 4)

struct cache_burst_wr_regs_u8 {
	u8 buf[BURST_MAX_BUF_SIZE];
	struct i2c_msg msg[BURST_MAX_MSG_NUM_U8];
};

struct cache_burst_wr_regs_u16 {
	u8 buf[BURST_MAX_BUF_SIZE];
	struct i2c_msg msg[BURST_MAX_MSG_NUM_U16];
};

#define OIS_TRANS_SIZE 64

int ois_i2c_rd_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals)
{
	int ret, cnt, total, recv, reg_b;
	u8 buf[2];
	struct i2c_msg msg[2];
	u8 *pbuf;

	recv = 0;
	total = n_vals;
	pbuf = p_vals;
	reg_b = reg;

	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;

	while (recv < total) {

		cnt = total - recv;
		if (cnt > MAX_VAL_NUM_U8)
			cnt = MAX_VAL_NUM_U8;

		buf[0] = reg_b >> 8;
		buf[1] = reg_b & 0xff;

		msg[1].buf = pbuf;
		msg[1].len = cnt;

		ret = i2c_transfer(i2c_client->adapter, msg, 2);
		if (ret < 0) {
			dev_info(&i2c_client->dev,
				"i2c transfer failed (%d)\n", ret);
			return -EIO;
		}

		pbuf += cnt;
		recv += cnt;
		reg_b += cnt;
	}

	return ret;
}
EXPORT_SYMBOL(ois_i2c_rd_p8);

int ois_i2c_rd_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

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
EXPORT_SYMBOL(ois_i2c_rd_u8);

int ois_i2c_rd_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[2];

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr  = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 2;

	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		return ret;
	}

	*val = ((u16)buf[0] << 8) | buf[1];

	return 0;
}
EXPORT_SYMBOL(ois_i2c_rd_u16);

int ois_i2c_rd_u32(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u32 *val)
{
	int ret;
	u8 buf[4];
	struct i2c_msg msg[2];

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = addr;
	msg[0].flags = i2c_client->flags;
	msg[0].buf = buf;
	msg[0].len = 2;

	msg[1].addr  = addr;
	msg[1].flags = i2c_client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 4;

	ret = i2c_transfer(i2c_client->adapter, msg, 2);
	if (ret < 0) {
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);
		return ret;
	}

	*val = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) | ((u32)buf[2] << 8) | buf[3];

	return 0;
}
EXPORT_SYMBOL(ois_i2c_rd_u32);

int ois_i2c_wr_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 val)
{
	int ret;
	u8 buf[3];
	struct i2c_msg msg;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0)
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);

	return ret;
}
EXPORT_SYMBOL(ois_i2c_wr_u8);

int ois_i2c_wr_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 val)
{
	int ret;
	u8 buf[4];
	struct i2c_msg msg;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val >> 8;
	buf[3] = val & 0xff;

	msg.addr = addr;
	msg.flags = i2c_client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(i2c_client->adapter, &msg, 1);
	if (ret < 0)
		dev_info(&i2c_client->dev, "i2c transfer failed (%d)\n", ret);

	return ret;
}
EXPORT_SYMBOL(ois_i2c_wr_u16);

int ois_i2c_poll_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 val)
{
	int i;
	int try = 100;
	u8 data = 0;
	int ret = -1;

	for (i = 0; i < try; i++) {
		ois_i2c_rd_u8(i2c_client, addr, reg, &data);
		if (data == val) {
			ret = 0;
			break;
		}
		usleep_range(1000, 1010);
	}

	return ret;
}
EXPORT_SYMBOL(ois_i2c_poll_u8);

int ois_i2c_poll_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 val)
{
	int i;
	int try  = 100;
	u16 data = 0;
	int ret  = -1;

	for (i = 0; i < try; i++) {
		ois_i2c_rd_u16(i2c_client, addr, reg, &data);
		if (data == val) {
			ret = 0;
			break;
		}
		usleep_range(1000, 1010);
	}

	return ret;

}
EXPORT_SYMBOL(ois_i2c_poll_u16);

int ois_i2c_wr_regs_u16_burst(struct i2c_client *i2c_client,
		u16 *list, u32 len)
{
	struct cache_burst_wr_regs_u16 *pmem;
	struct i2c_msg *pmsg;
	u8  *pbuf;
	u16 *plist;
	int ret;
	u32 tosend  = 0;
	u16 regAddr = 0;
	u16 regVal  = 0;
	u32 idx     = 0;
	u32 i       = 0;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 4 bytes: addr(u16) + val(u16) */
	plist = list;

	pbuf = pmem->buf;
	pmsg = pmem->msg;

	while (idx < len) {

		regAddr = plist[idx];
		regVal  = plist[idx + 1];
		idx    += 2;

		if (tosend == 0) {
			pbuf[tosend++] = regAddr >> 8;
			pbuf[tosend++] = regAddr & 0xFF;
			pbuf[tosend++] = regVal >> 8;
			pbuf[tosend++] = regVal & 0xFF;
			i = 1;
		} else {
			pbuf[tosend++] = regVal >> 8;
			pbuf[tosend++] = regVal & 0xFF;
			i++;
		}

		if ((idx >= len) || (tosend >= BURST_MAX_BUF_SIZE) || (i == OIS_TRANS_SIZE)) {
			pmsg->addr  = i2c_client->addr;
			pmsg->flags = i2c_client->flags;
			pmsg->len   = tosend;
			pmsg->buf   = pbuf;

			ret = i2c_transfer(i2c_client->adapter, pmem->msg, 1);
			if (ret != 1) {
				dev_info(&i2c_client->dev,
					"i2c transfer failed (%d)\n", ret);
				kfree(pmem);
				return -EIO;
			}
			tosend = 0;

			pbuf = pmem->buf;
			pmsg = pmem->msg;
		}
	}

	kfree(pmem);

	return 0;
}
EXPORT_SYMBOL(ois_i2c_wr_regs_u16_burst);

int ois_i2c_wr_regs_u8_burst(struct i2c_client *i2c_client,
		u16 *list, u32 len)
{
	struct cache_burst_wr_regs_u16 *pmem;
	struct i2c_msg *pmsg;
	u8  *pbuf;
	u16 *plist;
	int ret;
	u32 tosend  = 0;
	u16 regAddr = 0;
	u16 regVal  = 0;
	u32 idx     = 0;
	u32 i       = 0;

	pmem = kmalloc(sizeof(*pmem), GFP_KERNEL);
	if (!pmem)
		return -ENOMEM;

	/* each msg contains 4 bytes: addr(u16) + val(u16) */
	plist = list;

	pbuf = pmem->buf;
	pmsg = pmem->msg;

	while (idx < len) {

		regAddr = plist[idx];
		regVal  = plist[idx + 1];
		idx    += 2;

		if (tosend == 0) {
			pbuf[tosend++] = regAddr >> 8;
			pbuf[tosend++] = regAddr & 0xFF;
			pbuf[tosend++] = regVal & 0xFF;
			i = 1;
		} else {
			pbuf[tosend++] = regVal & 0xFF;
			i++;
		}

		if ((idx >= len) || (tosend >= BURST_MAX_BUF_SIZE) || (i == OIS_TRANS_SIZE)) {
			pmsg->addr  = i2c_client->addr;
			pmsg->flags = i2c_client->flags;
			pmsg->len   = tosend;
			pmsg->buf   = pbuf;

			ret = i2c_transfer(i2c_client->adapter, pmem->msg, 1);
			if (ret != 1) {
				dev_info(&i2c_client->dev,
					"i2c transfer failed (%d)\n", ret);
				kfree(pmem);
				return -EIO;
			}
			tosend = 0;

			pbuf = pmem->buf;
			pmsg = pmem->msg;
		}
	}

	kfree(pmem);

	return 0;
}
EXPORT_SYMBOL(ois_i2c_wr_regs_u8_burst);

u32 default_ois_fw_download(struct ois_driver_info *info)
{
	int ret;
	int i;
	struct device *dev        = NULL;
	u8 *ptr                   = NULL;
	const char *fw_name_prog  = NULL;
	const char *fw_name_coeff = NULL;
	const char *fw_name_mem   = NULL;
	const struct firmware *fw = NULL;
	u16 *plist                = NULL;
	u32 len             = 0;
	u32 sum             = 0;
	char name_prog[32]  = {0};
	char name_coeff[32] = {0};
	char name_mem[32]   = {0};

	if (!info || !info->ois_name || !info->client) {
		LOG_INF("Invalid Args");
		return 0;
	}

	dev = &info->client->dev;
	snprintf(name_coeff, 32, "%s.coeff", info->ois_name);
	snprintf(name_prog,  32, "%s.prog",  info->ois_name);
	snprintf(name_mem,   32, "%s.mem",   info->ois_name);
	/* cast pointer as const pointer*/
	fw_name_prog  = name_prog;
	fw_name_coeff = name_coeff;
	fw_name_mem   = name_mem;

	/* 1th */
	/* Load prog FW */
	LOG_INF("get firmware %s +", fw_name_prog);
	ret = request_firmware(&fw, fw_name_prog, dev);
	if (ret) {
		LOG_INF("Failed to locate %s", fw_name_prog);
		return 0;
	}
	LOG_INF("%s size is %d", fw_name_prog, fw->size);
	if (fw->size <= 0) {
		return 0;
	}

	len   = (info->data_type == OIS_I2C_TYPE_BYTE) ? fw->size * 2 : fw->size;
	ptr   = (u8 *)fw->data;
	plist = (u16 *)vmalloc(len * sizeof(u16));
	if (!plist) {
		LOG_INF("Failed in vmalloc %s, size is %d",
				fw_name_prog, len * sizeof(u16));
		return 0;
	}

	LOG_INF("update firmware %s +", fw_name_prog);
	if ((info->addr_type == OIS_I2C_TYPE_WORD) &&
		(info->data_type == OIS_I2C_TYPE_BYTE)) {
		for (i = 0; i < fw->size; i++) {
			plist[2 * i]	 = info->prog_addr + i;
			plist[2 * i + 1] = ptr[i];
			sum += plist[2 * i + 1];
		}

		ois_i2c_wr_regs_u8_burst(info->client, plist, len);
	} else if ((info->addr_type == OIS_I2C_TYPE_WORD) &&
			   (info->data_type == OIS_I2C_TYPE_WORD)) {
		for (i = 0; i < fw->size; i+=2) {
			plist[2 * i]	 = info->prog_addr + i;
			plist[2 * i + 1] = (ptr[i] << 8) | ptr[i + 1];
			sum += plist[2 * i + 1];
		}

		ois_i2c_wr_regs_u16_burst(info->client, plist, len);
	} else {
		LOG_INF("Failed to match write func! : %s", fw_name_prog);
	}
	LOG_INF("update firmware %s -", fw_name_prog);


	/*release prog FW*/
	release_firmware(fw);
	vfree(plist);
	fw    = NULL;
	plist = NULL;


	/* 2th */
	/* Load coeff FW */
	LOG_INF("get firmware %s +", fw_name_coeff);
	ret = request_firmware(&fw, fw_name_coeff, dev);
	if (ret) {
		LOG_INF("Failed to locate %s", fw_name_coeff);
		return 0;
	}
	LOG_INF("%s size is %d", fw_name_coeff, fw->size);
	if (fw->size <= 0) {
		return 0;
	}

	len   = (info->data_type == OIS_I2C_TYPE_BYTE) ? fw->size * 2 : fw->size;
	ptr   = (u8 *)fw->data;
	plist = (u16 *)vmalloc(len * sizeof(u16));
	if (!plist) {
		LOG_INF("Failed in vmalloc %s, size is %d",
				fw_name_coeff, len * sizeof(u16));
		return 0;
	}

	LOG_INF("update firmware %s +", fw_name_coeff);
	if ((info->addr_type == OIS_I2C_TYPE_WORD) &&
		(info->data_type == OIS_I2C_TYPE_BYTE)) {
		for (i = 0; i < fw->size; i++) {
			plist[2 * i]	 = info->coeff_addr + i;
			plist[2 * i + 1] = ptr[i];
			sum += plist[2 * i + 1];
		}

		ois_i2c_wr_regs_u8_burst(info->client, plist, len);
	} else if ((info->addr_type == OIS_I2C_TYPE_WORD) &&
			   (info->data_type == OIS_I2C_TYPE_WORD)) {
		for (i = 0; i < fw->size; i+=2) {
			plist[2 * i]	 = info->coeff_addr + i;
			plist[2 * i + 1] = (ptr[i] << 8) | ptr[i + 1];
			sum += plist[2 * i + 1];
		}

		ois_i2c_wr_regs_u16_burst(info->client, plist, len);
	} else {
		LOG_INF("Failed to match write func! : %s", fw_name_coeff);
	}
	LOG_INF("update firmware %s -", fw_name_coeff);


	/*release coeff FW*/
	release_firmware(fw);
	vfree(plist);
	fw    = NULL;
	plist = NULL;



	/* 3th */
	/* Load mem FW */
	LOG_INF("get firmware %s +", fw_name_mem);
	ret = request_firmware(&fw, fw_name_mem, dev);
	if (ret) {
		LOG_INF("Failed to locate %s", fw_name_mem);
		return 0;
	}
	LOG_INF("%s size is %d", fw_name_mem, fw->size);
	if (fw->size <= 0) {
		return 0;
	}

	len   = (info->data_type == OIS_I2C_TYPE_BYTE) ? fw->size * 2 : fw->size;
	ptr   = (u8 *)fw->data;
	plist = (u16 *)vmalloc(len * sizeof(u16));
	if (!plist) {
		LOG_INF("Failed in vmalloc %s, size is %d",
				fw_name_mem, len * sizeof(u16));
		return 0;
	}

	LOG_INF("update firmware %s +", fw_name_mem);
	if ((info->addr_type == OIS_I2C_TYPE_WORD) &&
		(info->data_type == OIS_I2C_TYPE_BYTE)) {
		for (i = 0; i < fw->size; i++) {
			plist[2 * i]	 = info->mem_addr + i;
			plist[2 * i + 1] = ptr[i];
			sum += plist[2 * i + 1];
		}

		ois_i2c_wr_regs_u8_burst(info->client, plist, len);
	} else if ((info->addr_type == OIS_I2C_TYPE_WORD) &&
			   (info->data_type == OIS_I2C_TYPE_WORD)) {
		for (i = 0; i < fw->size; i+=2) {
			plist[2 * i]	 = info->mem_addr + i;
			plist[2 * i + 1] = (ptr[i] << 8) | ptr[i + 1];
			sum += plist[2 * i + 1];
		}

		ois_i2c_wr_regs_u16_burst(info->client, plist, len);
	} else {
		LOG_INF("Failed to match write func! : %s", fw_name_mem);
	}
	LOG_INF("update firmware %s -", fw_name_mem);

	LOG_INF("sum = 0x%x", sum);

	/*release mem FW*/
	release_firmware(fw);
	vfree(plist);
	fw    = NULL;
	plist = NULL;

	return sum;
}
EXPORT_SYMBOL(default_ois_fw_download);

u8 read_eeprom_u8(struct i2c_client *client, u16 eeprom_addr, u16 addr)
{
	u8 val;

	ois_i2c_rd_u8(client, eeprom_addr >> 1, addr, &val);

	return val;
}
EXPORT_SYMBOL(read_eeprom_u8);

int update_ois_cali(struct ois_driver_info *info, struct ois_gyro_offset *gyro_offset)
{
	u32 idx    = 0;
	u32 len    = 0;
	u16 *plist = NULL;
	u8 data    = 0;
	int ois_x_gyro_offset = 0x1DC8;
	int ois_y_gyro_offset = 0x1DCA;

	LOG_INF("+\n");

	len = (info->data_type == OIS_I2C_TYPE_BYTE) ? info->cali_size * 2 : info->cali_size;
	plist = (u16 *)vmalloc(len * sizeof(u16));
	if (!plist) {
		LOG_INF("Failed in vmalloc, size is %d", len * sizeof(u16));
		return -1;
	}

	/* read ois cali */
	for (idx = 0; idx < info->cali_size; idx++) {
		plist[2 * idx]     = info->cali_addr + idx;
		plist[2 * idx + 1] = read_eeprom_u8(info->client, info->eeprom_addr, info->cali_offset + idx);
		if (ois_x_gyro_offset == plist[2 * idx]) {
			data = (gyro_offset->OISGyroOffsetX >> 8) & 0xFF;
			plist[2 * idx + 1] = data;
		}
		if ((ois_x_gyro_offset+1) == plist[2 * idx]) {
			data = gyro_offset->OISGyroOffsetX & 0xFF;
			plist[2 * idx + 1] = data;
		}

		if ((ois_y_gyro_offset) == plist[2 * idx]) {
			data = (gyro_offset->OISGyroOffsetY >> 8) & 0xFF;
			plist[2 * idx + 1] = data;
		}
		if ((ois_y_gyro_offset+1) == plist[2 * idx]) {
			data = gyro_offset->OISGyroOffsetY & 0xFF;
			plist[2 * idx + 1] = data;
		}

		LOG_INF("ois cali 0x%04x:0x%02x", plist[2 * idx], plist[2 * idx + 1]);
	}



	/* update ois cali */
	ois_i2c_wr_regs_u8_burst(info->client, plist, len);

	vfree(plist);
	LOG_INF("-\n");

	return 0;
}
EXPORT_SYMBOL(update_ois_cali);

int ois_write_setting(struct ois_driver_info *info, struct ois_setting *setting, int len)
{
	int i;
	int ret;
	u8  data_8  = 0;
	u16 data_16 = 0;

	if (!info || !setting) {
		LOG_INF("Invalid Args");
		return -1;
	}

	for (i = 0; i < len; i++) {
		switch (setting[i].type) {
		case OP_WRITE:
		{
			if ((info->addr_type == OIS_I2C_TYPE_WORD) && (info->data_type == OIS_I2C_TYPE_BYTE)) {
				ois_i2c_wr_u8(info->client,	info->client->addr, setting[i].addr, setting[i].data);
			} else if ((info->addr_type == OIS_I2C_TYPE_WORD) && (info->data_type == OIS_I2C_TYPE_WORD)) {
				ois_i2c_wr_u16(info->client, info->client->addr, setting[i].addr, setting[i].data);
			} else {
				LOG_INF("Failed to match write func! : write [0x%x <== 0x%x]", setting[i].addr, setting[i].data);
			}
			if (setting[i].delay_us > 0)
				usleep_range(setting[i].delay_us, setting[i].delay_us + 10);
		}
		break;

		case OP_READ:
		{
			if ((info->addr_type == OIS_I2C_TYPE_WORD) && (info->data_type == OIS_I2C_TYPE_BYTE)) {
				ois_i2c_rd_u8(info->client, info->client->addr, setting[i].addr, &data_8);
				LOG_INF("read [0x%04x] = 0x%02x", setting[i].addr, data_8);
			} else if ((info->addr_type == OIS_I2C_TYPE_WORD) && (info->data_type == OIS_I2C_TYPE_WORD)) {
				ois_i2c_rd_u16(info->client, info->client->addr, setting[i].addr, &data_16);
				LOG_INF("read [0x%04x] = 0x%04x", setting[i].addr, data_16);
			} else {
				LOG_INF("Failed to match read func! : read [0x%x]", setting[i].addr);
			}
			if (setting[i].delay_us > 0)
				usleep_range(setting[i].delay_us, setting[i].delay_us + 10);
		}
		break;

		case OP_POLL:
		{
			if ((info->addr_type == OIS_I2C_TYPE_WORD) && (info->data_type == OIS_I2C_TYPE_BYTE)) {
				ret = ois_i2c_poll_u8(info->client, info->client->addr, setting[i].addr, setting[i].data);
				LOG_INF("poll [0x%04x] == 0x%02x, results is %d", setting[i].addr, setting[i].data, ret);
			} else if ((info->addr_type == OIS_I2C_TYPE_WORD) && (info->data_type == OIS_I2C_TYPE_WORD)) {
				ret = ois_i2c_poll_u16(info->client, info->client->addr, setting[i].addr, setting[i].data);
				LOG_INF("poll [0x%04x] == 0x%04x, results is %d", setting[i].addr, setting[i].data, ret);
			} else {
				LOG_INF("Failed to match poll func! : poll [0x%x] == 0x%x", setting[i].addr, setting[i].data);
			}
		}
		break;

		default:
			LOG_INF("not match type : %s", setting[i].type);
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ois_write_setting);




void reset_ois_data(struct ois_driver_info *info)
{
    info->inputIdx  = 0;
    info->outputIdx = 0;
}
EXPORT_SYMBOL(reset_ois_data);

bool is_ois_data_empty(struct ois_driver_info *info)
{
    return info->inputIdx == info->outputIdx;
}
EXPORT_SYMBOL(is_ois_data_empty);

bool is_ois_data_full(struct ois_driver_info *info)
{
    return ((info->inputIdx+1) % OIS_DATA_NUMBER) == (info->outputIdx % OIS_DATA_NUMBER);
}
EXPORT_SYMBOL(is_ois_data_full);

void set_ois_data(struct ois_driver_info *info, u64 timestamps, int x_shifts, int y_shifts)
{
    if (is_ois_data_full(info)) {
		mutex_lock(&info->mutex);
        info->timestamps[info->inputIdx % OIS_DATA_NUMBER] = timestamps;
        info->x_shifts[info->inputIdx % OIS_DATA_NUMBER]   = x_shifts;
        info->y_shifts[info->inputIdx % OIS_DATA_NUMBER]   = y_shifts;
        info->inputIdx++;
        info->outputIdx++;
		mutex_unlock(&info->mutex);
    } else {
        info->timestamps[info->inputIdx % OIS_DATA_NUMBER] = timestamps;
        info->x_shifts[info->inputIdx % OIS_DATA_NUMBER]   = x_shifts;
        info->y_shifts[info->inputIdx % OIS_DATA_NUMBER]   = y_shifts;
        info->inputIdx++;
    }
}
EXPORT_SYMBOL(set_ois_data);

int get_ois_data(struct ois_driver_info *info, u64 *timestamps, int *x_shifts, int *y_shifts)
{
    int size   = 0;
    int endIdx = 0;

    if ((timestamps == NULL) && (x_shifts == NULL) && (y_shifts == NULL)) {
        return 0;
    }

	mutex_lock(&info->mutex);
    endIdx = info->inputIdx;
    while (info->outputIdx < endIdx) {
        timestamps[size] = info->timestamps[info->outputIdx % OIS_DATA_NUMBER];
        x_shifts[size]   = info->x_shifts[info->outputIdx % OIS_DATA_NUMBER];
        y_shifts[size]   = info->y_shifts[info->outputIdx % OIS_DATA_NUMBER];
        info->outputIdx++;
        size++;
    }
	mutex_unlock(&info->mutex);

    return size;
}
EXPORT_SYMBOL(get_ois_data);

MODULE_LICENSE("GPL v2");
