// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 *
 * Not aligned burst write only can be use
 * on supporting unaligned access cpu.
 * TODO: use get_unaligned
 *
 */
#define pr_fmt(fmt) "ispv4 regops: " fmt

// #define DEBUG

#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <ispv4_regops.h>
#include <linux/component.h>
#include <linux/mfd/ispv4_defs.h>

/* Retry times for checking after read-request/write. */
#define RW_CHECK_TIMES 3

/* spi2ahb operation command */
#define REGOPS_READ_CMD 0x10
#define REGOPS_READ_REQ_CMD 0x20
#define REGOPS_READ_STATUS_CMD 0x50
#define REGOPS_DREAD_CMD 0x70
#define REGOPS_WRITE_CMD 0x80
#define REGOPS_WRITE_STATUS_CMD 0xD0

#define REGOPS_LOCK() mutex_lock(base_lock);
#define REGOPS_UNLOCK() mutex_unlock(base_lock);

/* TODO: change to MAP index by coprocesser name */
static struct spi_device *ispv4_spi;
static struct device *local_dev;
static struct mutex *base_lock;

// clang-format on
static inline int __dev_availd(void)
{
	return local_dev != NULL && ispv4_spi != NULL &&
		base_lock != NULL ? 0 : -ENODEV;
}
// clang-format on

static bool _regops_check(u8 *buf, char *info)
{
	struct spi_transfer tran[2] = { 0 };
	struct spi_message message;
	int ret, retry = RW_CHECK_TIMES;
	while (retry > 0) {
		retry--;
		spi_message_init(&message);
		tran[0].tx_buf = buf;
		tran[0].len = 1;
		tran[1].rx_buf = buf + 1;
		tran[1].len = 2;
		tran[1].bits_per_word = 10;
		spi_message_add_tail(&tran[0], &message);
		spi_message_add_tail(&tran[1], &message);
		ret = spi_sync(ispv4_spi, &message);
		if (ret != 0) {
			pr_err("SPI get status(%s) sync failed! %d\n", info,
			       ret);
			return false;
		}

		if ((buf[1] & 0x40) != 0) {
			pr_err("SPI poll status(%s) meet error!\n", info);
			return false;
		} else if ((buf[1] & 0x80) != 0) {
			return true;
		} else if (buf[1] != 0) {
			pr_err("SPI poll status(%s) error fmt!\n", info);
			return false;
		}
		pr_warn("SPI poll met zero %d times!\n",
			RW_CHECK_TIMES - retry);
	}

	pr_err("SPI poll status(%s) no ready %d times !\n", info,
	       RW_CHECK_TIMES);
	return false;
}

__maybe_unused static bool _regops_read_avalid_check(void)
{
	static u8 *buf = NULL;
	const u8 r_poll[4] = { REGOPS_READ_STATUS_CMD, 0, 0, 0 };
	if (buf == NULL)
		buf = devm_kmalloc(local_dev, sizeof(r_poll), GFP_KERNEL);
	memcpy(buf, r_poll, sizeof(r_poll));
	return _regops_check(buf, "read avalid");
}

__maybe_unused static bool __regops_write_finish_check(char *s)
{
	static u8 *buf = NULL;
	const u8 w_poll[4] = { REGOPS_WRITE_STATUS_CMD, 0, 0, 0 };
	if (buf == NULL)
		buf = devm_kmalloc(local_dev, sizeof(w_poll), GFP_KERNEL);
	memcpy(buf, w_poll, sizeof(w_poll));
	return _regops_check(buf, s);
}

__maybe_unused static bool _regops_write_finish_check(void)
{
	return __regops_write_finish_check("write finish");
}

__maybe_unused static bool _regops_basewrite_finish_check(void)
{
	return __regops_write_finish_check("base write finish");
}

static int _regops_set_base(u32 base)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	const u8 cmd[8] = { REGOPS_WRITE_CMD, 0x80, 0, 0 };
	static u8 *buf = NULL;
	int ret;

	if (buf == NULL)
		buf = devm_kmalloc(local_dev, sizeof(cmd), GFP_KERNEL);

	memcpy(buf, cmd, sizeof(cmd));

	buf[4] = FIELD_GET(GENMASK(31, 24), base);
	buf[5] = FIELD_GET(GENMASK(23, 16), base);
	buf[6] = FIELD_GET(GENMASK(15, 8), base);
	buf[7] = FIELD_GET(GENMASK(7, 0), base);
	tran.tx_buf = buf;
	tran.len = sizeof(buf);
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI write base sync failed!");
		return ret;
	}

	if (!_regops_basewrite_finish_check())
		return -EIO;

	return 0;
}

__maybe_unused static bool _regops_read_request(u32 addr)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	int ret;
	const u8 rq_cmd[] = { REGOPS_READ_REQ_CMD, 0, 0, 0 };
	static u8 *rq_buf = NULL;

	if (rq_buf == NULL)
		rq_buf = devm_kmalloc(local_dev, sizeof(rq_cmd), GFP_KERNEL);

	memcpy(rq_buf, rq_cmd, sizeof(rq_cmd));

	rq_buf[3] = (u8)(addr >> 2);
	tran.tx_buf = rq_buf;
	tran.len = 4;
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI read request sync failed %d!\n", ret);
		return false;
	}
	return true;
}

__maybe_unused static bool _regops_inner_read_request(u32 addr)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	int ret;
	const u8 rq_cmd[] = { REGOPS_READ_REQ_CMD, 0x80, 0, 0 };
	static u8 *rq_buf = NULL;

	if (rq_buf == NULL)
		rq_buf = devm_kmalloc(local_dev, sizeof(rq_cmd), GFP_KERNEL);

	memcpy(rq_buf, rq_cmd, sizeof(rq_cmd));

	rq_buf[3] = (u8)(addr >> 2);
	tran.tx_buf = rq_buf;
	tran.len = 4;
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI read request sync failed %d!\n", ret);
		return false;
	}
	return true;
}

static int __regops_read_offset(u8 offset, u32 *valp, bool inner)
{
	struct spi_transfer tran[3] = { 0 };
	struct spi_message message;
	static uint8_t *rdata_buf = NULL;
	int ret;

	if (rdata_buf == NULL)
		rdata_buf = devm_kzalloc(local_dev, 16, GFP_KERNEL);

	if (valp == NULL) {
		return -ENOPARAM;
	}

	if (inner) {
		if (!_regops_inner_read_request(offset)) {
			pr_err("SPI read request sync failed!\n");
			return -EIO;
		}
	} else {
		if (!_regops_read_request(offset)) {
			pr_err("SPI read request sync failed!\n");
			return -EIO;
		}
	}

	if (!_regops_read_avalid_check()) {
		pr_err("SPI read check failed!\n");
		return -EIO;
	}

	rdata_buf[0] = REGOPS_READ_CMD;
	tran[0].tx_buf = rdata_buf;
	tran[0].len = 1;
	tran[1].rx_buf = rdata_buf + 1;
	tran[1].bits_per_word = 10;
	tran[1].len = 2;
	tran[2].rx_buf = rdata_buf + 3;
	tran[2].len = 3;
	memset(&rdata_buf[1], 0, 5);
	spi_message_init(&message);
	spi_message_add_tail(&tran[0], &message);
	spi_message_add_tail(&tran[1], &message);
	spi_message_add_tail(&tran[2], &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI read sync failed! %d\n", ret);
		return ret;
	}

	pr_debug("read buf = [%d-%d-%d-%d-%d]!\n", rdata_buf[1], rdata_buf[2],
	       rdata_buf[3], rdata_buf[4], rdata_buf[5]);
	*valp = 0;
	*valp |= rdata_buf[1] << 24;
	*valp |= rdata_buf[3] << 16;
	*valp |= rdata_buf[4] << 8;
	*valp |= rdata_buf[5] << 0;

	return 0;
}

static int _regops_read_offset(u8 offset, u32 *valp)
{
	return __regops_read_offset(offset, valp, false);
}

int ispv4_regops_inner_read(u8 offset, u32 *valp)
{
	return __regops_read_offset(offset, valp, true);
}
EXPORT_SYMBOL_GPL(ispv4_regops_inner_read);

static int _regops_dread_offset(u8 offset, u32 *valp)
{
	struct spi_transfer tran[2] = { 0 };
	struct spi_message message;
	int ret;
	static u8 *dread_buf = NULL;

	if (dread_buf == NULL)
		dread_buf = devm_kzalloc(local_dev, 16, GFP_KERNEL);

	if (valp == NULL) {
		return -ENOPARAM;
	}

	dread_buf[0] = REGOPS_DREAD_CMD;
	dread_buf[3] = (u8)(offset >> 2);
	tran[0].tx_buf = dread_buf;
	tran[0].len = 4;
	tran[1].rx_buf = dread_buf + 4;
	tran[1].len = 7;
	spi_message_init(&message);
	spi_message_add_tail(&tran[0], &message);
	spi_message_add_tail(&tran[1], &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI dread sync failed! %d\n", ret);
		return ret;
	}
	*valp = 0;
	*valp |= dread_buf[7] << 24;
	*valp |= dread_buf[8] << 16;
	*valp |= dread_buf[9] << 8;
	*valp |= dread_buf[10] << 0;

	return 0;
}

int ispv4_regops_read(u32 addr, u32 *valp)
{
	int ret = 0;

	ret = __dev_availd();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_set_base(addr);
	if (ret != 0)
		goto err;
	ret = _regops_read_offset(0, valp);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x]\n", __FUNCTION__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_regops_read);

int ispv4_regops_dread(u32 addr, u32 *valp)
{
	int ret = 0;

	ret = __dev_availd();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_set_base(addr);
	if (ret != 0)
		goto err;
	ret = _regops_dread_offset(0, valp);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x]\n", __FUNCTION__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_regops_dread);

static int _regops_write_offset(u8 offset, u32 data)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	static uint8_t *wdata_buf = NULL;
	int ret;

	if (wdata_buf == NULL)
		wdata_buf = devm_kzalloc(local_dev, 16, GFP_KERNEL);

	wdata_buf[0] = REGOPS_WRITE_CMD;
	wdata_buf[3] = (u8)(offset >> 2);
	wdata_buf[4] = FIELD_GET(GENMASK(31, 24), data);
	wdata_buf[5] = FIELD_GET(GENMASK(23, 16), data);
	wdata_buf[6] = FIELD_GET(GENMASK(15, 8), data);
	wdata_buf[7] = FIELD_GET(GENMASK(7, 0), data);
	tran.tx_buf = wdata_buf;
	tran.len = 8;
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI write sync failed! %d\n", ret);
		return ret;
	}
	if (!_regops_write_finish_check()) {
		pr_err("SPI write check failed!");
		return -EIO;
	}
	return 0;
}

/**
 * ispv4_regops_write() - Use spi to access ispv4 memory&reg.
 *
 * @addr: The absolute ispv4-cpu-addr, Need align to 4Byte
 * @data: -
 *
 * Will lock spi base register when transfer.
 *
 * Return: zero on success or error code on failure.
 */
int ispv4_regops_write(u32 addr, u32 data)
{
	int ret = 0;

	ret = __dev_availd();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_set_base(addr);
	if (ret != 0)
		goto err;
	ret = _regops_write_offset(0, data);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x=0x%x]\n", __FUNCTION__, ret, addr, data);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_regops_write);

static int __regops_wxdword_offset(u8 offset, u32 *data, int frames, u8 cmd)
{
	struct spi_transfer tran = { 0 };
	struct spi_message message;
	static uint8_t *wdata_buf = NULL;
	int ret, i;

	if (wdata_buf == NULL)
		wdata_buf = devm_kzalloc(local_dev, 64 * 4 + 4, GFP_KERNEL);

	wdata_buf[0] = cmd;
	wdata_buf[3] = (u8)(offset >> 2);
	for (i = 4; i < (frames + 1) * 4; i += 4) {
		wdata_buf[i + 0] =
			FIELD_GET(GENMASK(31, 24), data[(i >> 2) - 1]);
		wdata_buf[i + 1] =
			FIELD_GET(GENMASK(23, 16), data[(i >> 2) - 1]);
		wdata_buf[i + 2] =
			FIELD_GET(GENMASK(15, 8), data[(i >> 2) - 1]);
		wdata_buf[i + 3] = FIELD_GET(GENMASK(7, 0), data[(i >> 2) - 1]);
	}
	tran.tx_buf = wdata_buf;
	tran.len = frames * 4 + 4;
	spi_message_init(&message);
	spi_message_add_tail(&tran, &message);
	ret = spi_sync(ispv4_spi, &message);
	if (ret != 0) {
		pr_err("SPI group write failed! %d\n", ret);
		return ret;
	}
	if (!_regops_write_finish_check()) {
		pr_err("SPI group write check failed!");
		ret = -EIO;
	}
	return 0;
}

static int _regops_w8dword_offset(u8 offset, u32 *data)
{
	return __regops_wxdword_offset(offset, data, 8, REGOPS_WRITE_CMD + 2);
}

static int _regops_w64dword_offset(u8 offset, u32 *data)
{
	return __regops_wxdword_offset(offset, data, 64, REGOPS_WRITE_CMD + 5);
}

/**
 * ispv4_regops_w8dw() - Use spi to access ispv4 memory&reg
 * burst access len is 32Byte.
 *
 * @addr: The absolute ispv4-cpu-addr, Need align to 4Byte
 * @data: pointer to data array.
 *
 * Will lock spi base register when transfer.
 *
 * Return: zero on success or error code on failure.
 */
int ispv4_regops_w8dw(u32 addr, u32 *data)
{
	int ret = 0;

	ret = __dev_availd();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_set_base(addr);
	if (ret != 0)
		goto err;
	ret = _regops_w8dword_offset(0, data);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;
err:
	pr_err("%s failed %d [0x%x]\n", __FUNCTION__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_regops_w8dw);

/**
 * ispv4_regops_w64dw() - Use spi to access ispv4 memory&reg
 * burst access len is 32Byte.
 *
 * @addr: The absolute ispv4-cpu-addr, Need align to 4Byte
 * @data: pointer to data array.
 *
 * Will lock spi base register when transfer.
 *
 * Return: zero on success or error code on failure.
 */
int ispv4_regops_w64dw(u32 addr, u32 *data)
{
	int ret = 0;

	ret = __dev_availd();
	if (ret != 0)
		return ret;

	REGOPS_LOCK();
	ret = _regops_set_base(addr);
	if (ret != 0)
		goto err;
	ret = _regops_w64dword_offset(0, data);
	if (ret != 0)
		goto err;
	REGOPS_UNLOCK();

	return 0;

err:
	pr_err("%s failed %d [0x%x]\n", __FUNCTION__, ret, addr);
	REGOPS_UNLOCK();
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_regops_w64dw);

static int _ispv4_regops_burst_write(u32 addr, u8 *data, u32 len, int gwdw)
{
	int ret, i;
	u32 align_s, align_e, s_addr, e_addr;
	int w_gdword, w_dword;
	int gwbytes = gwdw * 4;
	int (*bwrite_fn)(u32 addr, u32 * data);

	switch (gwdw) {
	case 8:
		bwrite_fn = ispv4_regops_w8dw;
		break;
	case 64:
		bwrite_fn = ispv4_regops_w64dw;
		break;
	default:
		return -1;
	}

	// fix addr to align 4Bytes
	s_addr = addr;
	e_addr = addr + len;
	align_s = (s_addr / 4 + !!(s_addr % 4)) * 4;
	// 0x02 + 1: as=0x04; ae=0x0; will goto ok
	// 0x02 + 2: as=0x04; ae=0x04;
	// 0x02 + 3: as=0x04; ae=0x04;
	align_e = (e_addr >> 2) << 2;
	w_dword = align_e > align_s ? (align_e - align_s) / 4 : 0;
	w_gdword = (w_dword / gwdw) * gwdw;
	w_dword -= w_gdword;

	if (align_s != s_addr) {
		u32 tmp, j;
		u8 *u8p = (u8 *)&tmp;
		ret = ispv4_regops_read(align_s - 4, &tmp);
		if (ret != 0) {
			return ret;
		}
		for (i = s_addr % 4, j = 0; i < 4 && i < e_addr; i++, j++) {
			u8p[i] = data[j];
		}
		ret = ispv4_regops_write(align_s - 4, tmp);
		if (ret != 0) {
			return ret;
		}
	}

	// Len is smaller than 4
	if (align_e < align_s) {
		return 0;
	}

	for (i = w_gdword * 4; i > 0; i -= gwbytes) {
		ret = bwrite_fn(align_s, (u32 *)&data[align_s - s_addr]);
		if (ret != 0) {
			return ret;
		}
		align_s += gwbytes;
	}

	for (i = w_dword * 4; i > 0; i -= 4) {
		ret = ispv4_regops_write(align_s,
					 *(uint32_t *)&data[align_s - s_addr]);
		if (ret != 0) {
			return ret;
		}
		align_s += 4;
	}

	if (align_e != e_addr) {
		u32 tmp;
		u8 *u8p = (u8 *)&tmp;
		ret = ispv4_regops_read(align_e, &tmp);
		if (ret != 0) {
			return ret;
		}
		for (i = align_e; i < e_addr; i++) {
			u8p[i - align_e] = data[i - s_addr];
		}
		ret = ispv4_regops_write(align_e, tmp);
		if (ret != 0) {
			return ret;
		}
	}

	return 0;
}

/**
 * ispv4_regops_burst_write() - Use spi to access ispv4 memory&reg
 *
 * @addr: The absolute ispv4-cpu-addr, Need not aligen to 4Byte
 * @data: pointer to data array.
 * @len: -, Need not align to 4Byte.
 *
 * Please Only use this API to access to memory.
 * If you want to use this api to write no-read region
 * please align addr and len to 4Byte.
 *
 * Return: zero on success or error code on failure.
 */
int ispv4_regops_burst_write(u32 addr, u8 *data, u32 len)
{
	return _ispv4_regops_burst_write(addr, data, len, 8);
}
EXPORT_SYMBOL_GPL(ispv4_regops_burst_write);

/**
 * ispv4_regops_long_burst_write() - Use spi to access ispv4 memory&reg
 *
 * @addr: The absolute ispv4-cpu-addr, Need not aligen to 4Byte
 * @data: pointer to data array.
 * @len: -, Need not align to 4Byte.
 *
 * Please Only use this API to access to memory.
 * If you want to use this api to write no-read region
 * please align addr and len to 4Byte.
 *
 * Return: zero on success or error code on failure.
 */
int ispv4_regops_long_burst_write(u32 addr, u8 *data, u32 len)
{
	return _ispv4_regops_burst_write(addr, data, len, 64);
}
EXPORT_SYMBOL_GPL(ispv4_regops_long_burst_write);

/*
	The addresses do not need to be aligned; they are automatically aligned.
	@addr: reg start addr
	@data: Each byte sets a value
	@len: Number of bytes
*/
int ispv4_regops_set(u32 addr, u32 data, u32 len)
{
	int ret = 0;
	u8 *arr_ptr = NULL;

	if (len <= 0) {
		pr_err("input len illegality! \n");
		return -ENODEV;
	}

	arr_ptr = (u8 *)kmalloc(len, GFP_KERNEL);
	if (arr_ptr == NULL) {
		pr_err("kmalloc arr_ptr failed! \n");
		return -ENODEV;
	}

	memset(arr_ptr, data, len);
	ret = ispv4_regops_burst_write(addr, arr_ptr, len);

	kfree(arr_ptr);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_regops_set);

inline int ispv4_regops_clear_and_set(u32 addr, u32 mask, u32 value)
{
	uint32_t data;
	int ret = 0;
	ret = ispv4_regops_read(addr, &data);
	if (ret)
		return ret;

	data &= ~mask;
	data |= (value & mask);

	return ispv4_regops_write(addr, data);
}
EXPORT_SYMBOL_GPL(ispv4_regops_clear_and_set);

int ispv4_regops_get_speed(void)
{
	int sp = 0;

	sp = __dev_availd();
	if (sp != 0)
		return -ENODEV;

	sp = ispv4_spi->max_speed_hz;

	pr_crit("%s [%d]\n", __FUNCTION__, sp);
	return sp;
}
EXPORT_SYMBOL_GPL(ispv4_regops_get_speed);

int ispv4_regops_set_speed(u32 sp)
{
	int ret = 0;

	ret = __dev_availd();
	if (ret != 0)
		return ret;

	ispv4_spi->max_speed_hz = sp;
	spi_setup(ispv4_spi);

	pr_info("%s [%d]\n", __FUNCTION__, sp);
	return 0;
}
EXPORT_SYMBOL_GPL(ispv4_regops_set_speed);

/* TODO: add user-space access interface */
static long ispv4_regops_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	return 0;
}

static const struct file_operations ispv4_regops_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ispv4_regops_unlocked_ioctl,
};

static struct miscdevice ispv4_regops_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ispv4_regops",
	.fops = &ispv4_regops_fops,
};

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_regops.avalid = true;
	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_regops.avalid = false;
}

__maybe_unused static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind
};

static int xm_ispv4_regops_probe(struct platform_device *pdev)
{
	int ret;
	struct spi_device *sdev;

	sdev = container_of(pdev->dev.parent, struct spi_device, dev);

	pr_info("Into %s\n", __FUNCTION__);
	pr_info("spi speed = %d\n", sdev->max_speed_hz);

	base_lock = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	if (base_lock == NULL) {
		pr_err("malloc basklock failed!\n");
		return -ENODEV;
	}
	mutex_init(base_lock);

	ret = misc_register(&ispv4_regops_miscdev);
	if (ret != 0) {
		pr_info("misc register failed %d!\n", ret);
		kfree(base_lock);
	}

	// component_add(&pdev->dev, &comp_ops);
	pr_info("reg ops probe finish!\n");

	ispv4_spi = sdev;
	local_dev = &pdev->dev;

	return 0;
}

static int xm_ispv4_regops_remove(struct platform_device *pdev)
{
	ispv4_spi = NULL;
	local_dev = NULL;
	if (base_lock != NULL) {
		kfree(base_lock);
	}
	// component_del(&pdev->dev, &comp_ops);
	misc_deregister(&ispv4_regops_miscdev);
	return 0;
}

static const struct platform_device_id regops_id_table[] = {
	{
		.name = "xm-ispv4-regops",
		.driver_data = 0,
	},
	{}
};
MODULE_DEVICE_TABLE(platform, regops_id_table);

static struct platform_driver xm_ispv4_regops_driver = {
	.driver = {
		.name = "xm-ispv4-regops",
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	},
	.probe		= xm_ispv4_regops_probe,
	.remove		= xm_ispv4_regops_remove,
	.id_table = regops_id_table,
	.prevent_deferred_probe = true,
};
module_platform_driver(xm_ispv4_regops_driver);

MODULE_AUTHOR("Chenhonglin <chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ISPV4 regops driver");
MODULE_LICENSE("GPL v2");
