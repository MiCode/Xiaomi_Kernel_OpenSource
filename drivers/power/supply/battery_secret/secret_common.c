
/*
 *  w1_common.c - w1 battery secret common driver for slg2 and ds28e30
 */

#define pr_fmt(fmt) "[w1sec %s,%d] " fmt "\n", __func__, __LINE__

#include <linux/of.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "battery_secret_class.h"
#include "secret_common.h"

static struct w1_data *g_info;

static void hex2ascii(u8 *output, u8 *input, u32 len)
{
    u32 i;

    for (i = 0; i < len; i++) {
        output[i * 2] = ((input[i] & 0xF0) >> 4) + 0x30;
        output[i * 2 + 1] = (input[i] & 0x0F) + 0x30;
        if (output[i * 2] >= 0x3A)
            output[i * 2] += 7;
        if (output[i * 2 + 1] >= 0x3A)
            output[i * 2 + 1] += 7;
    }
}

void w1_hex_dump(const char *prefix, u8 *data_buf, int length)
{
    char buffer[128];

    if (!prefix) {
        pr_err("prefix null");
        return;
    }

    if ((strlen(prefix) + length * 2) > sizeof(buffer))
        pr_err("length too large");
    else {
        if (length == 0 || data_buf == NULL)
            pr_info("%s", prefix);
        else {
            memset(buffer, 0x00, sizeof(buffer));
            hex2ascii(buffer, data_buf, length);
            pr_info("%s%s", prefix, buffer);
        }
    }
}

static void w1_ndelay(u32 ns)
{
	uint64_t pre, now;

	pre = ktime_get_ns();
	while (1) {
		now = ktime_get_ns();
		if (now - pre >= ns)
			break;
	}
}

static void w1_udelay(u32 us)
{
	w1_ndelay(us * 1000);
}

void w1_mdelay(int ms)
{
	mdelay(ms);
}

static void w1_set_gpio_out(int level)
{
#ifdef ENABLE_GPIO_REGISTER_CTRL
	u32 gpio_dir, gpio_data, gpio_inen;

	gpio_data = readl_relaxed(g_info->w1_gpio_data);
	gpio_data &= ~(1 << 8);
	gpio_data |= !!level << 8;
	writel_relaxed(gpio_data, g_info->w1_gpio_data);

	gpio_dir = readl_relaxed(g_info->w1_gpio_dir);
	gpio_dir |= 1 << 8;
	writel_relaxed(gpio_dir, g_info->w1_gpio_dir);

	gpio_inen = readl_relaxed(g_info->w1_gpio_inen);
	gpio_inen &= ~(1 << 8);
	writel_relaxed(gpio_inen, g_info->w1_gpio_inen);
#else
	gpiod_direction_output(g_info->w1_gpiod, 1);
#endif
}

static void w1_set_gpio_in(void)
{
#ifdef ENABLE_GPIO_REGISTER_CTRL
	u32 gpio_dir, gpio_inen;

	gpio_dir = readl_relaxed(g_info->w1_gpio_dir);
	gpio_dir &= ~(1 << 8);
	writel_relaxed(gpio_dir, g_info->w1_gpio_dir);

	gpio_inen = readl_relaxed(g_info->w1_gpio_inen);
	gpio_inen |= 1 << 8;
	writel_relaxed(gpio_inen, g_info->w1_gpio_inen);
#else
	gpiod_direction_input(g_info->w1_gpiod);
#endif
}

static void w1_set_gpio_value(int level)
{
#ifdef ENABLE_GPIO_REGISTER_CTRL
	u32 gpio_data;

	gpio_data = readl_relaxed(g_info->w1_gpio_data);
	gpio_data &= ~(1 << 8);
	gpio_data |= !!level << 8;
	writel_relaxed(gpio_data, g_info->w1_gpio_data);
#else
	gpiod_set_value(g_info->w1_gpiod);
#endif
}

static int w1_get_gpio_value(void)
{
#ifdef ENABLE_GPIO_REGISTER_CTRL
	u32 gpio_data;

	gpio_data = readl_relaxed(g_info->w1_gpio_data);
	return !!(gpio_data & (1 << 8));
#else
	return gpiod_get_value(g_info->w1_gpiod);
#endif
}

static void w1_write_bit(u8 bit)
{
	w1_set_gpio_out(1);
	if (bit) {
		w1_set_gpio_value(0);
		w1_ndelay(g_info->w1_timings_cfg[0]);
		w1_set_gpio_value(1);
		w1_udelay(g_info->w1_timings_cfg[1]);
	} else {
		w1_set_gpio_value(0);
		w1_udelay(g_info->w1_timings_cfg[2]);
		w1_set_gpio_value(1);
		w1_udelay(g_info->w1_timings_cfg[3]);
	}
}

static u8 w1_read_bit(void)
{
	int result;

	w1_set_gpio_out(1);
	w1_set_gpio_value(0);
	w1_ndelay(g_info->w1_timings_cfg[4]);
	w1_set_gpio_in();
	w1_ndelay(g_info->w1_timings_cfg[5]);
	result = w1_get_gpio_value();
	w1_udelay(g_info->w1_timings_cfg[6]);

	return result;
}

int w1_reset_bus(void)
{
	int level;
	unsigned long flags = 0;

	w1_set_gpio_out(1);
	raw_spin_lock_irqsave(&g_info->io_lock, flags);
	w1_set_gpio_value(0);
	w1_udelay(g_info->w1_timings_cfg[7]);
	w1_set_gpio_in();
	w1_udelay(g_info->w1_timings_cfg[8]);
	level = w1_get_gpio_value();
	raw_spin_unlock_irqrestore(&g_info->io_lock, flags);
	w1_mdelay(1);

	return level;
}

void w1_bus_recovery(void)
{
	w1_set_gpio_out(0);
	w1_mdelay(20);
	w1_set_gpio_out(1);
	w1_mdelay(50);
}

void w1_write_byte(u8 byte)
{
	int i;
	unsigned long flags = 0;

	raw_spin_lock_irqsave(&g_info->io_lock, flags);
	for (i = 0; i < 8; ++i)
		w1_write_bit((byte >> i) & 0x1);
	raw_spin_unlock_irqrestore(&g_info->io_lock, flags);
}

void w1_write_block(const u8 *buf, int len)
{
	int i;

	for (i = 0; i < len; ++i)
		w1_write_byte(buf[i]);
}

u8 w1_read_byte(void)
{
	int i;
	u8 res = 0;
	unsigned long flags = 0;

	raw_spin_lock_irqsave(&g_info->io_lock, flags);
	for (i = 0; i < 8; ++i)
		res |= w1_read_bit() << i;
	raw_spin_unlock_irqrestore(&g_info->io_lock, flags);

	return res;
}

void w1_read_block(u8 *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		buf[i] = w1_read_byte();
}

static int w1_read_page(int page_num, u8 *read_buf)
{
	if (page_num == CYCLE_SOH_PAGE) {
		if (g_info->page2_valid) {
			memcpy(read_buf, g_info->page2_buf, 32);
			pr_err("use last page2 buffer data");
			return 0;
		}
		if (!g_info->page_read(page_num, read_buf)) {
			g_info->page2_valid = 1;
			memcpy(g_info->page2_buf, read_buf, 32);
			return 0;
		} else {
			g_info->page2_valid = 0;
			pr_err("read page:%d failed", page_num);
			return -EINVAL;
		}
	} else if (page_num == FIRST_USE_PAGE) {
		if (g_info->page1_valid) {
			memcpy(read_buf, g_info->page1_buf, 32);
			pr_err("use last page1 buffer data");
			return 0;
		}
		if (!g_info->page_read(page_num, read_buf)) {
			g_info->page1_valid = 1;
			memcpy(g_info->page1_buf, read_buf, 32);
			return 0;
		} else {
			g_info->page1_valid = 0;
			pr_err("read page:%d failed", page_num);
			return -EINVAL;
		}
	} else {
		pr_err("invalid page index:%d", page_num);
		return -EINVAL;
	}
}

static int w1_write_page(int page_num, const u8 *write_data)
{
	if (page_num == CYCLE_SOH_PAGE) {
		if (!g_info->page_write(page_num, write_data)) {
			g_info->page2_valid = 1;
			memcpy(g_info->page2_buf, write_data, 32);
			return 0;
		} else {
			g_info->page2_valid = 0;
			pr_err("write page:%d failed", page_num);
			return -EINVAL;
		}
	} else if (page_num == FIRST_USE_PAGE) {
		if (!g_info->page_write(page_num, write_data)) {
			g_info->page1_valid = 1;
			memcpy(g_info->page1_buf, write_data, 32);
			return 0;
		} else {
			g_info->page1_valid = 0;
			pr_err("write page:%d failed", page_num);
			return -EINVAL;
		}
	} else {
		pr_err("invalid page index:%d", page_num);
		return -EINVAL;
	}
}

static int w1_dc_get(u32 *dc_cnt)
{
	if (g_info->dc_value_curr != W1_ERR_VALUE) {
		*dc_cnt = g_info->dc_value_curr;
		pr_err("use last dc value:%d", g_info->dc_value_curr);
		return 0;
	}

	if (!g_info->dc_get(dc_cnt)) {
		g_info->dc_value_curr = *dc_cnt;
		pr_err("new dc value:%d", g_info->dc_value_curr);
		return 0;
	} else {
		pr_err("read dc failed");
		g_info->dc_value_curr = W1_ERR_VALUE;
    	return -EINVAL;
	}
}

static int w1_dc_decrease(void)
{
	if (!g_info->dc_decrease()) {
		g_info->dc_value_curr--;
		return 0;
	} else {
		pr_err("dc decrease failed");
		g_info->dc_value_curr = W1_ERR_VALUE;
    	return -EINVAL;
	}
}

static int w1_get_cycle_count(struct secret_device *secret_dev, int *cycle_count)
{
	u32 dc_val;
	u8 page_buf[32];
	u32 get_count, dc_cycle_count;
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev); 

	if (!info){
		pr_err("get w1_data fail");
		return -EINVAL;
	}

	if (info->cycle_count_curr != W1_ERR_VALUE) {
		*cycle_count = info->cycle_count_curr;
		pr_info("use last cycle count:%d", info->cycle_count_curr);
		return 0;
	}

	if (w1_dc_get(&dc_val)) {
		info->cycle_count_curr = W1_ERR_VALUE;
		pr_err("read dc failed");
		return -EINVAL;
	}

	// use dc for cycle count
	if (dc_val != DC_INIT_VALUE) {
		dc_cycle_count = DC_INIT_VALUE - dc_val + 29;
		*cycle_count = dc_cycle_count;
		info->cycle_count_curr = dc_cycle_count;
		pr_info("dc cycle count:%d", dc_cycle_count);
	} else { // use page2[30] for cycle count
		if (w1_read_page(CYCLE_SOH_PAGE, page_buf)) {
			pr_err("read page2 failed");
			info->cycle_count_curr = W1_ERR_VALUE;
			return -EINVAL;
		}
		get_count = page_buf[30];
		pr_info("page cycle count:%d", get_count);
		if (get_count == 0xFF)
			get_count = 0;
		*cycle_count = get_count;
		info->cycle_count_curr = get_count;
	}

	return 0;
}

static int w1_set_cycle_count(struct secret_device *secret_dev, int set_cycle_count)
{
	int cnt;
	u32 dc_val, last_dc_cycle_count;
	u8 page_buf[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}
	pr_info("set new cycle count:%d", set_cycle_count);
	if (w1_dc_get(&dc_val)) {
		info->cycle_count_curr = W1_ERR_VALUE;
		pr_err("read dc value failed");
		return -EINVAL;
	}
	// dc cycle count not use
	if (dc_val == DC_INIT_VALUE && set_cycle_count < 30) {
		if (w1_read_page(CYCLE_SOH_PAGE, page_buf)) {
			info->cycle_count_curr = W1_ERR_VALUE;
			pr_err("read page2 failed");
			return -EINVAL;
		}
		page_buf[30] = set_cycle_count;
		if (w1_write_page(CYCLE_SOH_PAGE, page_buf)) {
			info->cycle_count_curr = W1_ERR_VALUE;
			pr_err("write cycle count:%d failed", set_cycle_count);
			return -EINVAL;
		}
	} else { // update dc cycle count
		last_dc_cycle_count = DC_INIT_VALUE - dc_val + 29;
		if (set_cycle_count <= last_dc_cycle_count) {
			info->cycle_count_curr = W1_ERR_VALUE;
			pr_err("invalid new cycle count:%d, last dc cycle count:%d",
				set_cycle_count, last_dc_cycle_count);
			return -EINVAL;
		} else {
			cnt = set_cycle_count - last_dc_cycle_count;
			while (cnt) {
				if (w1_dc_get(&dc_val)) {
					info->cycle_count_curr = W1_ERR_VALUE;
					pr_err("read last dc value failed");
					return -EINVAL;
				}
				if (DC_INIT_VALUE - dc_val + 29 >= set_cycle_count) {
					info->cycle_count_curr = W1_ERR_VALUE;
					pr_err("dc decrease to cycle count:%d succeeded", set_cycle_count);
					return 0;
				}
				if (w1_dc_decrease()) {
					info->cycle_count_curr = W1_ERR_VALUE;
					pr_err("dc decrease failed, cnt:%d", cnt);
					return -EINVAL;
				}
				cnt--;
			}
		}
	}

	info->cycle_count_curr = set_cycle_count;

    return 0;
}

static int w1_get_ui_soh(struct secret_device *secret_dev, u8 *ui_soh_data, int len)
{
	u8 page_buf[32];
	int i = 0, invalid_count = 0;
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data failed");
		return -EINVAL;
	}

	if (info->uisoh_valid) {
		memcpy(ui_soh_data, info->uisoh_buf, len);
		w1_hex_dump("use last ui soh:", info->uisoh_buf, len);
		goto UISOH_CHECK;
	}

	if (w1_read_page(CYCLE_SOH_PAGE, page_buf)) {
		info->uisoh_valid = 0;
		pr_err("read ui soh paged failed");
		return -EINVAL;
	}
	info->uisoh_valid = 1;
	info->rawsoh_curr = page_buf[15];
	memcpy(ui_soh_data, page_buf, len);
UISOH_CHECK:
	for (i = 0; i < len; i++) {
		if (ui_soh_data[i] == 0xff)
			invalid_count++;
	}
	if (invalid_count >= 5) {
		pr_err("%s invalid value, set 0", __func__);
		memset(ui_soh_data, 0, len);
	}
	memcpy(info->uisoh_buf, ui_soh_data, len);
	w1_hex_dump("ui soh:", ui_soh_data, len);

	return 0;
}

static int w1_set_ui_soh(struct secret_device *secret_dev, u8 *ui_soh_data, int len, int raw_soh)
{
	u8 page_buf[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data failed");
		return -EINVAL;
	}
	if (w1_read_page(CYCLE_SOH_PAGE, page_buf)) {
		info->uisoh_valid = 0;
		pr_err("read ui soh page failed");
		return -EINVAL;
	}

	memcpy(page_buf, ui_soh_data, len);
	page_buf[15] = raw_soh;
	if (w1_write_page(CYCLE_SOH_PAGE, page_buf)) {
		info->uisoh_valid = 0;
		info->rawsoh_curr = W1_ERR_VALUE;
		pr_err("update ui soh data failed");
		return -EINVAL;
	} else {
		info->uisoh_valid = 1;
		info->rawsoh_curr = page_buf[15];
		memcpy(info->uisoh_buf, ui_soh_data, len);
		w1_hex_dump("new ui soh:", ui_soh_data, len);
		return 0;
	}
}

static int w1_get_raw_soh(struct secret_device *secret_dev, int *val)
{
	u8 page_buf[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data failed");
		return -EINVAL;
	}

	if (info->rawsoh_curr != W1_ERR_VALUE) {
		*val = info->rawsoh_curr;
		pr_info("use last raw soh:%d", info->rawsoh_curr);
		goto RAWSOH_CHECK;
	}

	if (w1_read_page(CYCLE_SOH_PAGE, page_buf)) {
		info->rawsoh_curr = W1_ERR_VALUE;
		pr_err("read raw soh page failed");
		return -EINVAL;
	}
	*val = page_buf[15];
RAWSOH_CHECK:
	if (*val > 100) {
		*val = 100;
		page_buf[15] = *val;
		if (w1_write_page(CYCLE_SOH_PAGE, page_buf)) {
			info->rawsoh_curr = W1_ERR_VALUE;
			pr_err("set raw_soh 100 failed");
			return -EINVAL;
		}
	}
	info->rawsoh_curr = *val;
	pr_err("raw soh:%d", *val);

	return 0;
}

static int w1_set_raw_soh(struct secret_device *secret_dev, int raw_soh)
{
	u8 page_buf[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}

	if (w1_read_page(CYCLE_SOH_PAGE, page_buf)) {
		info->rawsoh_curr = W1_ERR_VALUE;
		pr_err("read raw soh failed");
		return -EINVAL;
	}

	page_buf[15] = raw_soh;
	if (w1_write_page(CYCLE_SOH_PAGE, page_buf)) {
		info->rawsoh_curr = W1_ERR_VALUE;
		pr_err("update raw soh failed");
		return -EINVAL;
	}
	info->rawsoh_curr = page_buf[15];
	pr_info("new raw soh:%d", page_buf[15]);

	return 0;
}

static int w1_get_batt_manufacture_date(struct secret_device *secret_dev, char * const buff, size_t size)
{
	const char *batt_sn_cmdline;
	char batt_info[33], batt_date[9];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data failed");
		return -EINVAL;
	}
	batt_sn_cmdline = find_secret_attr_by_name("batt_sn");
	if (batt_sn_cmdline) {
		strcpy(batt_info, batt_sn_cmdline);
		batt_date[0] = '2'; batt_date[1] = '0';
		batt_date[2] = '2'; batt_date[3] = batt_info[6];
		// month
		if (batt_info[7] <= '9') {
			batt_date[4] = '0';
			batt_date[5] = batt_info[7];
		} else {
			batt_date[4] = '1';
			batt_date[5] = '0' + batt_info[7] - 'A';
		}
		// day
		batt_date[6] = batt_info[8];
		batt_date[7] = batt_info[9];
		strcpy(buff, batt_date);
		pr_info("battery manufacture date:%s", batt_date);
		return 0;
	} else {
		pr_err("get batt_sn_cmdline failed");
		return -EINVAL;
	}
}

static int w1_get_battery_first_use_time(struct secret_device *secret_dev, char * const buff, size_t size)
{
	int i = 0, invalid_count = 0;
	uint8_t page_data[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}

	if (info->fst_use_time_valid) {
		memcpy(buff, info->fst_use_time_buf, FST_USE_TIME_LEN);
		w1_hex_dump("use last first use time:", info->fst_use_time_buf, FST_USE_TIME_LEN);
		goto TIME_CHECK;
	}

	if (w1_read_page(FIRST_USE_PAGE, page_data)) {
		info->fst_use_time_valid = 0;
		pr_err("read first_use_time failed");
		return -EINVAL;
	}
	info->fst_use_time_valid = 1;
	memcpy(buff, page_data, FST_USE_TIME_LEN);
TIME_CHECK:
	for (i = 0; i < FST_USE_TIME_LEN; i++) {
		if (buff[i] == 0xff)
			invalid_count++;
	}

	if (invalid_count) {
		pr_err("invalid value, set 0");
		memset(buff, '0', FST_USE_TIME_LEN);
	}

	w1_hex_dump("first_use_time:", buff, FST_USE_TIME_LEN);
	return 0;
}

static int w1_set_battery_first_use_time(struct secret_device *secret_dev, const char *time)
{
	int i = 0;
	uint8_t page_data[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}
	if (w1_read_page(FIRST_USE_PAGE, page_data)) {
		info->fst_use_time_valid = 0;
		pr_err("read first_use_time failed");
		return -EINVAL;
	}
	info->fst_use_time_valid = 1;
	for (i = 0; i < FST_USE_TIME_LEN; i++)
		page_data[i] = time[i];

	if (w1_write_page(FIRST_USE_PAGE, page_data)) {
		info->fst_use_time_valid = 0;
		pr_err("update first_use_time failed");
		return -EINVAL;
	}
	info->fst_use_time_valid = 1;
	w1_hex_dump("first_use_time:", page_data, FST_USE_TIME_LEN);
	return 0;
}

static int w1_get_battery_id(struct secret_device *secret_dev, int *batt_id)
{
	int ret, val;
	const char *batt_id_cmdline;
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}
	batt_id_cmdline = find_secret_attr_by_name("batt_id");
	if (batt_id_cmdline) {
		ret = kstrtoint(batt_id_cmdline, 10, &val);
		if (ret < 0) {
			pr_err("parse cycle count failed");
			return -EINVAL;
		} else {
			info->batt_id = val;
			*batt_id = val;
			return 0;
		}
	}
	return -EINVAL;
}

static int w1_is_battery_auth(struct secret_device *secret_dev, int *is_auth)
{
	int ret, val;
	const char *is_auth_cmdline;
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}
	is_auth_cmdline = find_secret_attr_by_name("is_auth");
	if (is_auth_cmdline) {
		ret = kstrtoint(is_auth_cmdline, 10, &val);
		if (ret < 0) {
			pr_err("parse cycle count failed");
			return -EINVAL;
		} else {
			info->is_auth = val;
			*is_auth = val;
			return 0;
		}
	}
	return -EINVAL;
}

static int w1_get_battery_sn(struct secret_device *secret_dev, char * const buff, size_t size)
{
	const char *batt_sn_cmdline;
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data failed");
		return -EINVAL;
	}
	batt_sn_cmdline = find_secret_attr_by_name("batt_sn");
	if (batt_sn_cmdline) {
		strcpy(buff, batt_sn_cmdline);
		pr_info("battery sn:%s", buff);
		return 0;
	} else {
		pr_err("get batt_sn_cmdline failed");
		return -EINVAL;
	}
}

static int w1_clear_cycle_count(struct secret_device *secret_dev)
{
	uint8_t flag_data[32];
	uint8_t cycle_data[32];
	struct w1_data *info = dev_get_drvdata(&secret_dev->dev);

	if (!info) {
		pr_err("get w1_data fail");
		return -EINVAL;
	}
	// read page1 data[7]
	if (w1_read_page(FIRST_USE_PAGE, flag_data)) {
		pr_err("read clear_flag failed");
		return -EINVAL;
	}

	//read cyc
	if (w1_read_page(CYCLE_SOH_PAGE, cycle_data)) {
		pr_err("read cycle_data failed");
		return -EINVAL;
	}
	//compare
	if (cycle_data[30] > 10) {
		pr_err("cycle_data over 10");
		return -EINVAL;
	}

	if (flag_data[7] == 0x1 && !cycle_data[30]) {
		pr_info("cycle_data is already clear");
		return 0;
	} else if (flag_data[7] == 0x1 && cycle_data[30]) {
		pr_info("reset flag online, cycle invald:%u", cycle_data[30]);
		return -EINVAL;
	}
	//write 0x00 -> cyc
	cycle_data[30] = 0;
	if (w1_write_page(CYCLE_SOH_PAGE, cycle_data)) {
		pr_err("cycle_data clear failed");
		return -EINVAL;
	}
	//write 0x1 -> page1 data[7]
	flag_data[7] = 0x1;
	if (w1_write_page(FIRST_USE_PAGE, flag_data)) {
		pr_err("update flag_data failed");
		return -EINVAL;
	}
	pr_info("clear cycle success");

	return 0;
}

static int onewire_gpio_init( struct w1_data *info)
{
#ifdef ENABLE_GPIO_REGISTER_CTRL
	w1_set_gpio_out(1);
#else
	enum gpiod_flags gflags = GPIOD_OUT_HIGH;

	info->w1_gpiod = devm_gpiod_get(info->dev, "onewire", gflags);
	if (IS_ERR(info->w1_gpiod)) {
		pr_err("gpio_request w1 gpio failed");
		return PTR_ERR(w1_gpiod);
	}
#endif

	return 0;
}

static int enable_gpio_init( struct w1_data *info)
{
#if 0
	enum gpiod_flags gflags = GPIOD_OUT_HIGH;

	info->enable_gpiod = devm_gpiod_get(info->dev, "enable", gflags);
	if (IS_ERR(info->enable_gpiod)) {
		pr_err("request enable gpio failed");
		return PTR_ERR(info->enable_gpiod);
	}
#else
	int ret = 0;

	info->vdd_power = devm_regulator_get(info->dev, "vdd");
	if (!info->vdd_power) {
		pr_err("failed to get vdd power");
		return -EINVAL;
	}
	if (!regulator_is_enabled(info->vdd_power))
		pr_err("vddkpled is diabled");
	ret = regulator_set_voltage(info->vdd_power, VDD_POWER_VOLTAGE, VDD_POWER_VOLTAGE);
	if (ret) {
		pr_err("set vdd power voltage failed");
		return -EINVAL;
	}
	ret = regulator_enable(info->vdd_power);
	if (ret) {
		pr_err("enable vdd power failed");
		return -EINVAL;
	}
	pr_info("vddkpled enabled");
#endif

	return 0;
}

static void cmdline_attrs_init(struct w1_data *info)
{
	int i, ret, val, len = 0;
	char byte_str[4], byte_str_buf[64];
	const char *cycle_count_cmdline;
	const char *dc_value_cmdline;
	const char *raw_soh_cmdline;
	const char *ui_soh_cmdline;
	const char *fst_use_time_cmdline;

	info->uisoh_valid = 0;
	info->page1_valid = 0;
	info->page2_valid = 0;
	info->fst_use_time_valid = 0;
	info->dc_value_curr = W1_ERR_VALUE;
	info->cycle_count_curr = W1_ERR_VALUE;
	info->rawsoh_curr = W1_ERR_VALUE;

	cycle_count_cmdline = find_secret_attr_by_name("cc");
	dc_value_cmdline = find_secret_attr_by_name("dc");
	raw_soh_cmdline = find_secret_attr_by_name("rsoh");
	ui_soh_cmdline = find_secret_attr_by_name("uisoh");
	fst_use_time_cmdline = find_secret_attr_by_name("futime");

	// parse cycle count from cmdline
	if (cycle_count_cmdline) {
		ret = kstrtoint(cycle_count_cmdline, 10, &val);
		if (ret < 0) {
			pr_err("parse cycle count failed");
		} else {
			pr_err("parse cycle count succ,cycle_count=%d", val);
			info->cycle_count_curr = val;
		}
	}

	// parse dc from cmdline
	if (dc_value_cmdline) {
		ret = kstrtoint(dc_value_cmdline, 10, &val);
		if (ret < 0) {
			pr_err("parse dc failed");
		} else {
			pr_err("parse dc succ,dc=%d", val);
			info->dc_value_curr = val;
			if (DC_INIT_VALUE != info->dc_value_curr) {
				info->cycle_count_curr = DC_INIT_VALUE - info->dc_value_curr + 29;
				pr_err("use dc cycle:%d", info->cycle_count_curr);
			} else if (info->cycle_count_curr >= 30) {
				info->cycle_count_curr = 0;
				pr_err("force cycle:%d", info->cycle_count_curr);
			}
		}
	}

	// parse rawsoh from cmdline
	if (raw_soh_cmdline) {
		ret = kstrtoint(raw_soh_cmdline, 10, &val);
		if (ret < 0) {
			pr_err("parse raw soh failed");
		} else {
			pr_err("parse raw soh succ,raw_soh=%d", val);
			info->rawsoh_curr = val;
			if (info->rawsoh_curr > 100) {
				info->rawsoh_curr = 100;
				pr_err("force rawsoh:%d", info->rawsoh_curr);
			}
		}
	}

	// parse uisoh from cmdline
	if (!ui_soh_cmdline) {
		pr_err("parse ui soh failed");
	} else {
		info->uisoh_valid = 1;
		memset(byte_str_buf, '\0', sizeof(byte_str_buf));
		for (i = 0; i < UISOH_LEN; i++ ) {
			memset(byte_str, '\0', sizeof(byte_str));
			byte_str[0] = ui_soh_cmdline[2 * i];
			byte_str[1] = ui_soh_cmdline[2 * i + 1];
			byte_str[2] = '\0';
			ret = kstrtou8(byte_str, 16, &info->uisoh_buf[i]);
			if (ret < 0)
				pr_err("parse ui soh[%d] failed", i);
			len += snprintf(byte_str_buf + len, PAGE_SIZE, " %02X", info->uisoh_buf[i]);
		}
		pr_info("parse ui soh succ,ui_soh=%s", byte_str_buf);
	}

	// parse first use time from cmdline
	if (!fst_use_time_cmdline) {
		pr_err("parse first use time failed");
	} else {
		len = 0;
		info->fst_use_time_valid = 1;
		memset(byte_str_buf, '\0', sizeof(byte_str_buf));
		for (i = 0; i < FST_USE_TIME_LEN; i++ ) {
			memset(byte_str, '\0', sizeof(byte_str));
			byte_str[0] = fst_use_time_cmdline[2 * i];
			byte_str[1] = fst_use_time_cmdline[2 * i + 1];
			byte_str[2] = '\0';
			ret = kstrtou8(byte_str, 16, &info->fst_use_time_buf[i]);
			if (ret < 0)
				pr_err("parse ui soh[%d] failed", i);
			len += snprintf(byte_str_buf + len, PAGE_SIZE, " %02X", info->fst_use_time_buf[i]);
		}
		pr_info("parse first use time succ,time_soh=%s", byte_str_buf);
	}
}

static struct secret_ops w1_secret_ops = {
	.set_cycle_count = w1_set_cycle_count,
	.get_cycle_count = w1_get_cycle_count,
	.get_uisoh = w1_get_ui_soh,
	.set_uisoh = w1_set_ui_soh,
	.get_rawsoh = w1_get_raw_soh,
	.set_rawsoh = w1_set_raw_soh,
	.get_battery_manufacture_date = w1_get_batt_manufacture_date,
	.get_battery_first_use_time = w1_get_battery_first_use_time,
	.set_battery_first_use_time = w1_set_battery_first_use_time,
	.get_battery_id = w1_get_battery_id,
	.is_battery_auth = w1_is_battery_auth,
	.get_battery_sn = w1_get_battery_sn,
	.clear_cycle_count = w1_clear_cycle_count,
};

// slg2:    /sys/bus/platform/devices/soc:acl_slg2/w1_cmdbuf
// ds28e30: /sys/bus/platform/devices/soc:maxim_ds28e30/w1_cmdbuf
static ssize_t w1_cmdbuf_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, len = 0;

	for (i = 0; i < g_info->cmd_len; i++)
		len += snprintf(buf + len, PAGE_SIZE, "%02X ", g_info->cmd_buffer[i]);
	len += snprintf(buf + len, PAGE_SIZE, "\n");

	return len;
}

// slg2:    echo "20 32 CA 01" > /sys/bus/platform/devices/soc:acl_slg2/w1_cmdbuf
// ds28e30: echo "20 32 44 01" > /sys/bus/platform/devices/soc:maxim_ds28e30/w1_cmdbuf
// 20(H): read data length
// 32(H): delay(ms) before read from secret ic
// CA/44(H): command byte
// 02(H): command payload
static ssize_t w1_cmdbuf_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int     i;
	ssize_t len = 0;
	ulong   tx_hex[64];
	u8      tx_hex_u8[64];
	u32     num = 0, wait_time;
	char    *tx_ptr, *hexstr, txbuf[128];

	g_info->cmd_len = 0;
	tx_ptr = txbuf;
	sscanf(buf, "%02X", &g_info->cmd_len);
	sscanf(&buf[3], "%02X", &wait_time);
	memset(txbuf, 0x00, sizeof(txbuf));
	strcpy(txbuf, &buf[6]);
	memset(g_info->cmd_buffer, 0x00, sizeof(g_info->cmd_buffer));
	while ((hexstr = strsep(&tx_ptr, " ")) != NULL) {
		if (kstrtoul(hexstr, 16, &tx_hex[num]) < 0) {
			g_info->cmd_len = 0;
			pr_err("parse byte(%u) failed", num);
			return -EINVAL;
		}
		num++;
	}

	memset(txbuf, 0x00, sizeof(txbuf));
	len += snprintf(txbuf, sizeof(txbuf), "tx data:");
	for (i = 0; i < num; i++)
		len += snprintf(txbuf + len, PAGE_SIZE, " %02X", (u32)tx_hex[i]);
	pr_info("%s", txbuf);

	for (i = 0; i < num; i++)
		tx_hex_u8[i] = tx_hex[i];

	if (g_info->w1_bus_send_recv(tx_hex_u8, num, wait_time, g_info->cmd_buffer, g_info->cmd_len)) {
		g_info->cmd_len = 0;
		pr_err("write cmdbuf failed");
		return -EINVAL;
	}
	// read one more chip status byte
	g_info->cmd_len += 1;

	return size;
}

static struct device_attribute dev_attr_w1_cmdbuf =
	__ATTR(w1_cmdbuf, S_IRUSR | S_IWUSR, w1_cmdbuf_show, w1_cmdbuf_store);

int w1_sec_dev_init(struct w1_data *w1_data_prv, const char *name)
{
	int ret = 0;
	const char *chip_name_cmdline;

	g_info = w1_data_prv;
	ret = of_property_read_string(g_info->dev->of_node, "role", &g_info->role);
	if (ret < 0) {
		pr_err("can not find role (%d)\n", ret);
		g_info->role = MASTER_SECRET;
	}
	ret = of_property_read_string(g_info->dev->of_node, "chip_name", &g_info->chip_name);
	if (ret < 0) {
		pr_err("can not find chip name(%d)\n", ret);
		g_info->chip_name = name;
	}
	chip_name_cmdline = find_secret_attr_by_name("chip_name");
	if (!chip_name_cmdline || strcmp(chip_name_cmdline, g_info->chip_name) != 0){
		pr_info("chip_name:%s not match from cmdline:%s",
			g_info->chip_name, !chip_name_cmdline? "NULL" : chip_name_cmdline);
		return -EINVAL;
	}
	raw_spin_lock_init(&g_info->io_lock);
	dev_set_drvdata(g_info->dev, g_info);
	g_info->w1_gpio_data = ioremap(0x641b0180, 4);
	g_info->w1_gpio_dir  = ioremap(0x641b0188, 4);
	g_info->w1_gpio_inen = ioremap(0x641b01a8, 4);
	cmdline_attrs_init(g_info);
	ret = onewire_gpio_init(g_info);
	if (ret) {
		pr_err("onewire_gpio_init fail");
		return ret;
	}
	ret = enable_gpio_init(g_info);
	if (ret) {
		pr_err("enable_gpio_init fail");
		return ret;
	}
	g_info->secret_dev = secret_device_register(g_info->role, g_info->chip_name, NULL, g_info, &w1_secret_ops);
	if (IS_ERR_OR_NULL(g_info->secret_dev)) {
		pr_err("failed to register secret device");
		return PTR_ERR(g_info->secret_dev);
	}
	if (device_create_file(g_info->dev, &dev_attr_w1_cmdbuf))
		pr_err("create %s file failed", dev_attr_w1_cmdbuf.attr.name);

	return 0;
}
