#define LOG_TAG         "Sysfs"

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_test.h"
#include "cts_sfctrl.h"
#include "cts_spi_flash.h"
#include "cts_firmware.h"
#include "cts_strerror.h"

#ifdef CONFIG_CTS_SYSFS

#define SPLIT_LINE_STR \
	"-----------------------------------------------"\
	"------------------------------------------------\n"
#define ROW_NUM_FORMAT_STR  "%2d | "
#define COL_NUM_FORMAT_STR  "%4u "
#define DATA_FORMAT_STR     "%5d"

#define DIFFDATA_BUFFER_SIZE(cts_dev) \
	(cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)

#define MAX_ARG_NUM                 (100)
#define MAX_ARG_LENGTH              (1024)
#define HW_STUB_ADDR                (0XF000)
/*C3T code for HQ-229322 by jishen at 2022/10/12  start*/
bool cts_gesture_flag = false;
/*C3T code for HQ-229322 by jishen at 2022/10/12  end*/
static char cmdline_param[MAX_ARG_LENGTH + 1];
int argc;
char *argv[MAX_ARG_NUM];

static int jitter_test_frame = 10;
static s16 *manualdiff_base;
static int manualdiff_base_updated;

int parse_arg(const char *buf, size_t count)
{
	char *p;

	memcpy(cmdline_param, buf, min((size_t)MAX_ARG_LENGTH, count));
	cmdline_param[count] = '\0';

	argc = 0;
	p = strim(cmdline_param);
	if (p == NULL || p[0] == '\0')
		return 0;

	while (p && p[0] != '\0' && argc < MAX_ARG_NUM)
		argv[argc++] = strsep(&p, " ,");

	return argc;
}

/* echo addr value1 value2 value3 ... valueN > write_reg */
static ssize_t write_firmware_register_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u16 addr;
	int i, ret;
	u8 *data = NULL;

	parse_arg(buf, count);

	cts_info("Write firmware register '%.*s'", (int)count, buf);

	if (argc < 2) {
		cts_err("Too few args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou16(argv[0], 0, &addr);
	if (ret) {
		cts_err("Invalid address %s", argv[0]);
		return -EINVAL;
	}

	data = (u8 *) kmalloc(argc - 1, GFP_KERNEL);
	if (data == NULL) {
		cts_err("Allocate buffer for write data failed\n");
		return -ENOMEM;
	}

	for (i = 1; i < argc; i++) {
		ret = kstrtou8(argv[i], 0, data + i - 1);
		if (ret) {
			cts_err("Invalid value %s", argv[i]);
			goto free_data;
		}
	}

	ret = cts_dev->ops->write_reg(cts_dev, addr, data, argc - 1);
	if (ret) {
		cts_err("Write firmware register addr: 0x%04x size: %d failed",
			addr, argc - 1);
		goto free_data;
	}

free_data:
	kfree(data);

	return (ret < 0 ? ret : count);
}

#ifdef CFG_CTS_FW_UPDATE_SYS

static ssize_t cts_panel_supplier_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *data = dev_get_drvdata(dev);

	if (data->pdata && data->pdata->panel_supplier) {
		return scnprintf(buf, PAGE_SIZE, "%s\n",
				 data->pdata->panel_supplier);
	}
	return 0;
}

static ssize_t buildid_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0000-%04x\n",
			cts_data->cts_dev.fwdata.version);
}

static ssize_t forcereflash_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	unsigned int input;

	if (kstrtouint(buf, 10, &input) != 0)
		return -EINVAL;

	cts_data->force_reflash = (input == 0) ? false : true;

	cts_info("%s force_reflash=%d, count=%zu", __func__,
		 (cts_data->force_reflash ? 1 : 0), count);
	return count;
}

static ssize_t flashprog_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 cts_is_firmware_updating(&cts_data->cts_dev) ? 1 : 0);
}

static bool is_reflash_filename_valid(const struct chipone_ts_data *cts_data,
				      const char *filename)
{
	char prefix[CFG_CTS_FW_FILE_NAME_MAX_LEN];

	if (cts_data->pdata->panel_supplier != NULL) {
		snprintf(prefix, sizeof(prefix), "%s-%s-%s-",
			 CFG_CTS_FW_FILE_NAME_VENDOR,
			 cts_data->pdata->panel_supplier,
			 cts_data->cts_dev.hwdata->name);
	} else {
		/* panel supplier not set, just check vendor. */
		snprintf(prefix, sizeof(prefix), "%s",
			 CFG_CTS_FW_FILE_NAME_VENDOR);
	}

	cts_info("%s: prefix=%s", __func__, prefix);
	if (strncmp(filename, prefix, strlen(prefix))) {
		cts_err("%s: invalid FW file.", __func__);
		return false;
	}

	return true;
}

static ssize_t doreflash_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	int ret;

	cts_info("doreflash FW filename: %s len: %zu", buf, count);

	if (count > CFG_CTS_FW_FILE_NAME_MAX_LEN) {
		cts_err("doreflash FW filename is too long %zu > %d",
			count, CFG_CTS_FW_FILE_NAME_MAX_LEN);
		return -EINVAL;
	}

	if (cts_is_device_suspended(&cts_data->cts_dev)) {
		cts_err("In suspend state, try again later");
		return -EAGAIN;
	}

	if (cts_is_firmware_updating(&cts_data->cts_dev)) {
		cts_err("In FW flashing state, try again later");
		return -EAGAIN;
	}

	if (!cts_data->force_reflash) {
		/* Check filename if force_reflash is false */
		if (!is_reflash_filename_valid(cts_data, buf)) {
			cts_err("Invalid firmware filename '%*.s'",
				(int)count, buf);
			return -EINVAL;
		}
	}

	strncpy(cts_data->cts_dev.config_fw_name, buf, count);

	/* If use echo xxx > doreflash, 0x0A will append to the string,
	 *  if use echo -n xxx > doreflash, nothing will append.
	 */
	if (cts_data->cts_dev.config_fw_name[count - 1] == '\n')
		cts_data->cts_dev.config_fw_name[count - 1] = '\0';
	else
		cts_data->cts_dev.config_fw_name[count] = '\0';

	cts_stop_device(&cts_data->cts_dev);

	cts_lock_device(&cts_data->cts_dev);
	ret = cts_update_firmware_from_file(&cts_data->cts_dev,
					    cts_data->cts_dev.config_fw_name,
					    false);
	cts_unlock_device(&cts_data->cts_dev);

	if (ret)
		cts_err("Update firmware from file '%s' failed %d",
			cts_data->cts_dev.config_fw_name, ret);

	cts_start_device(&cts_data->cts_dev);

	cts_data->force_reflash = false;

	cts_info("%s: end", __func__);

	return ret ? ret : count;
}

static ssize_t cts_poweron_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	bool val;

	/* TBD: check if cts is power to ready for flash.
	 * set "1" if power on ready.
	 */
	val = cts_is_device_suspended(&cts_data->cts_dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", val == false);
}

static ssize_t cts_productinfo_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	/* set chip IC type to productinfo */
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 cts_data->cts_dev.hwdata->name);
}

/* add sys entries for FW update */
/* static DEVICE_ATTR(drv_irq, S_IRUGO | S_IWUSR, drv_irq_show, drv_irq_store); */
/* static DEVICE_ATTR(reset, S_IWUSR | S_IWGRP, NULL, reset_store); */
static DEVICE_ATTR(panel_supplier, 0444, cts_panel_supplier_show, NULL);
static DEVICE_ATTR(buildid, S_IRUGO, buildid_show, NULL);
static DEVICE_ATTR(forcereflash, S_IWUSR | S_IWGRP, NULL, forcereflash_store);
static DEVICE_ATTR(flashprog, S_IRUGO, flashprog_show, NULL);
static DEVICE_ATTR(doreflash, S_IWUSR | S_IWGRP, NULL, doreflash_store);
static DEVICE_ATTR(poweron, S_IRUGO, cts_poweron_show, NULL);
static DEVICE_ATTR(productinfo, S_IRUGO, cts_productinfo_show, NULL);
#endif

static DEVICE_ATTR(write_reg, S_IWUSR, NULL, write_firmware_register_store);

static ssize_t read_firmware_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#define PRINT_ROW_SIZE          (16)
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u16 addr, size, i, remaining;
	u8 *data = NULL;
	ssize_t count = 0;
	int ret;

	cts_info("Read firmware register ");

	if (argc != 2) {
		return snprintf(buf, PAGE_SIZE,
				"Invalid num args %d\n"
				"  1. echo addr size > read_reg\n"
				"  2. cat read_reg\n", argc);
	}

	ret = kstrtou16(argv[0], 0, &addr);
	if (ret) {
		return snprintf(buf, PAGE_SIZE, "Invalid address: %s\n", argv[0]);
	}
	ret = kstrtou16(argv[1], 0, &size);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Invalid size: %s\n", argv[1]);

	data = (u8 *) kmalloc(size, GFP_KERNEL);
	if (data == NULL) {
		return snprintf(buf, PAGE_SIZE, "Allocate buffer for read data failed\n");
	}

	cts_info("Read firmware register from 0x%04x size %u", addr, size);
	cts_lock_device(cts_dev);
	ret = cts_dev->ops->read_reg(cts_dev, addr, data, (size_t)size);
	cts_unlock_device(cts_dev);
	if (ret) {
		count = snprintf(buf, PAGE_SIZE,
			"Read firmware register from 0x%04x size %u failed %d\n", addr, size, ret);
		goto err_free_data;
	}

	remaining = size;
	for (i = 0; i < size && count <= PAGE_SIZE; i += PRINT_ROW_SIZE) {
		size_t linelen = min((size_t)remaining, (size_t)PRINT_ROW_SIZE);

		remaining -= PRINT_ROW_SIZE;

		count +=
		    snprintf(buf + count, PAGE_SIZE - count, "%04x: ", addr);

		/* Lower version kernel return void */
		hex_dump_to_buffer(data + i, linelen, PRINT_ROW_SIZE, 1,
				   buf + count, PAGE_SIZE - count, true);
		count += strlen(buf + count);

		if (count < PAGE_SIZE) {
			buf[count++] = '\n';
			addr += PRINT_ROW_SIZE;
		} else {
			break;
		}
	}

err_free_data:
	kfree(data);

	return count;
#undef PRINT_ROW_SIZE
}

/* echo addr size > read_reg */
static ssize_t read_firmware_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	parse_arg(buf, count);

	return (argc == 0 ? 0 : count);
}

static DEVICE_ATTR(read_reg, S_IWUSR | S_IRUSR,
		   read_firmware_register_show, read_firmware_register_store);

static ssize_t read_hw_reg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#define PRINT_ROW_SIZE          (16)
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u32 addr, size, i, remaining;
	u8 *data = NULL;
	ssize_t count = 0;
	int ret;

	cts_info("Read hw register");

	if (argc != 2) {
		return snprintf(buf, PAGE_SIZE,
				"Invalid num args %d\n"
				"  1. echo addr size > read_hw_reg\n"
				"  2. cat read_hw_reg\n", argc);
	}

	ret = kstrtou32(argv[0], 0, &addr);
	if (ret) {
		return snprintf(buf, PAGE_SIZE, "Invalid address: %s\n", argv[0]);
	}
	ret = kstrtou32(argv[1], 0, &size);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Invalid size: %s\n", argv[1]);

	data = (u8 *) kmalloc(size, GFP_KERNEL);
	if (data == NULL)
		return snprintf(buf, PAGE_SIZE, "Allocate buffer for read data failed\n");

	cts_info("Read hw register from 0x%04x size %u", addr, size);
	cts_lock_device(cts_dev);

	if ((cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911C) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911S) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911)) {
		for (i = 0; i < size; i++) {
			ret = cts_fw_reg_writel_retry(cts_dev, HW_STUB_ADDR, addr + i, 3, 10);
			if (ret) {
				count = snprintf(buf, PAGE_SIZE, "Write hw register error\n");
				goto err_free_data;
			}

			ret = cts_fw_reg_readsb(cts_dev, HW_STUB_ADDR + 4, data + i, 1);
			if (ret) {
				count = snprintf(buf, PAGE_SIZE, "Read hw register error\n");
				goto err_free_data;
			}
		}
	}

	if ((cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916C) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9922)) {
		if (cts_dev->rtdata.program_mode) {
			for (i = 0; i < size; i++) {
				ret = cts_dev_readb(cts_dev, addr + i, data + i, 3, 10);
				if (ret) {
					count = snprintf(buf, PAGE_SIZE, "Write hw register error\n");
					goto err_free_data;
				}
			}
		} else {
			ret = cts_dev->ops->read_hw_reg(cts_dev, addr, data, size);
			if (ret < 0) {
				count = snprintf(buf, PAGE_SIZE, "Read hw register error\n");
				goto err_free_data;
			}
		}
	}

	remaining = size;
	for (i = 0; i < size && count <= PAGE_SIZE; i += PRINT_ROW_SIZE) {
		size_t linelen = min((size_t)remaining, (size_t)PRINT_ROW_SIZE);

		remaining -= PRINT_ROW_SIZE;

		count += snprintf(buf + count, PAGE_SIZE - count, "%04x: ", addr);

		/* Lower version kernel return void */
		hex_dump_to_buffer(data + i, linelen, PRINT_ROW_SIZE, 1,
			buf + count, PAGE_SIZE - count, true);
		count += strlen(buf + count);

		if (count < PAGE_SIZE) {
			buf[count++] = '\n';
			addr += PRINT_ROW_SIZE;
		} else {
			break;
		}
	}

err_free_data:
	cts_unlock_device(cts_dev);
	kfree(data);

	return count;
#undef PRINT_ROW_SIZE
}

/* echo addr size > read_reg */
static ssize_t read_hw_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	parse_arg(buf, count);

	return (argc == 0 ? 0 : count);
}

static DEVICE_ATTR(read_hw_reg, S_IRUSR | S_IWUSR, read_hw_reg_show,
		   read_hw_reg_store);

static ssize_t write_hw_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u32 addr;
	int i, ret;
	u8 *data = NULL;

	parse_arg(buf, count);

	cts_info("Write hw register");

	if (argc < 2) {
		cts_err("Too few args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou32(argv[0], 0, &addr);
	if (ret) {
		cts_err("Invalid address %s", argv[0]);
		return -EINVAL;
	}

	data = (u8 *) kmalloc(argc - 1, GFP_KERNEL);
	if (data == NULL) {
		cts_err("Allocate buffer for write data failed\n");
		return -ENOMEM;
	}

	for (i = 1; i < argc; i++) {
		ret = kstrtou8(argv[i], 0, data + i - 1);
		if (ret) {
			cts_err("Invalid value %s", argv[i]);
			goto free_data;
		}
	}

	cts_lock_device(cts_dev);

	if ((cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911C) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911S) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9911)) {
		for (i = 0; i < argc - 1; i++) {
			ret = cts_fw_reg_writel_retry(cts_dev, HW_STUB_ADDR, addr + i, 3, 10);
			if (ret) {
				cts_err("Write hw register error");
				break;
			}
			ret = cts_fw_reg_writeb_retry(cts_dev, HW_STUB_ADDR + 4, data[i], 3, 10);
			if (ret) {
				cts_err("Write hw register error");
				break;
			}
		}
	}

	if ((cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9916C) ||
	    (cts_dev->hwdata->hwid == CTS_DEV_HWID_ICNL9922)) {
		if (cts_dev->rtdata.program_mode) {
			for (i = 0; i < argc - 1; i++) {
				ret = cts_dev_writeb(cts_dev, addr + i, data[i], 3, 10);
				if (ret) {
					cts_err("Write hw register error");
					break;
				}
			}
		} else {
			ret = cts_dev->ops->write_hw_reg(cts_dev, addr, data, argc - 1);
			if (ret < 0)
				cts_err("Write hw register error");
		}
	}

	cts_unlock_device(cts_dev);
free_data:
	kfree(data);

	return (ret < 0 ? ret : count);
}

static DEVICE_ATTR(write_hw_reg, S_IWUSR, NULL, write_hw_reg_store);

static ssize_t curr_firmware_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Current firmware version: %04x\n",
		cts_data->cts_dev.fwdata.version);
}

static DEVICE_ATTR(curr_version, S_IRUGO, curr_firmware_version_show, NULL);

static ssize_t curr_ddi_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Current ddi version: %02x\n",
		cts_data->cts_dev.fwdata.ddi_version);
}

static DEVICE_ATTR(curr_ddi_version, S_IRUGO, curr_ddi_version_show, NULL);

static ssize_t rows_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Num rows: %u\n",
		cts_data->cts_dev.fwdata.rows);
}

static DEVICE_ATTR(rows, S_IRUGO, rows_show, NULL);

static ssize_t cols_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Num cols: %u\n",
		cts_data->cts_dev.fwdata.cols);
}

static DEVICE_ATTR(cols, S_IRUGO, cols_show, NULL);

static ssize_t res_x_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "X Resolution: %u\n",
		cts_data->cts_dev.fwdata.res_x);
}

static DEVICE_ATTR(res_x, S_IRUGO, res_x_show, NULL);

static ssize_t res_y_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Y Resolution: %u\n",
		cts_data->cts_dev.fwdata.res_y);
}

static DEVICE_ATTR(res_y, S_IRUGO, res_y_show, NULL);

static ssize_t esd_protection_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;
	u8 esd_protection;

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_esd_protection(cts_dev, &esd_protection);
	cts_unlock_device(cts_dev);
	if (ret)
		return snprintf(buf, PAGE_SIZE,
				"Read firmware ESD protection register failed %d\n",
				ret);

	return snprintf(buf, PAGE_SIZE, "ESD protection: %u\n", esd_protection);
}

static DEVICE_ATTR(esd_protection, S_IRUGO, esd_protection_show, NULL);

static ssize_t monitor_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;
	u8 value;

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->is_mnt_enabled(cts_dev, &value);
	cts_unlock_device(cts_dev);
	if (ret)
		return snprintf(buf, PAGE_SIZE,
			"Read firmware monitor enable register failed %d\n", ret);

	return snprintf(buf, PAGE_SIZE, "Monitor mode: %s\n",
		value & BIT(0) ? "Enable" : "Disable");
}

static ssize_t monitor_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;
	u8 value, enable = 0;

	if (buf[0] == 'Y' || buf[0] == 'y' || buf[0] == '1')
		enable = 1;

	cts_info("Write firmware monitor mode to '%c', %s",
		 buf[0], enable ? "Enable" : "Disable");

	cts_lock_device(cts_dev);
	ret = cts_fw_reg_readb(&cts_data->cts_dev, 0x8000 + 344, &value);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("Write firmware monitor enable register failed %d", ret);
		return -EIO;
	}

	if ((value & BIT(0)) && enable)
		cts_info("Monitor mode already enabled");
	else if ((value & BIT(0)) == 0 && enable == 0)
		cts_info("Monitor mode already disabled");
	else {
		if (enable)
			value |= BIT(0);
		else
			value &= ~BIT(0);

		cts_lock_device(cts_dev);
		ret = cts_fw_reg_writeb(&cts_data->cts_dev, 0x8000 + 344, value);
		cts_unlock_device(cts_dev);
		if (ret) {
			cts_err("Write firmware monitor enable register failed %d", ret);
			return -EIO;
		}
	}

	return count;
}

static DEVICE_ATTR(monitor_mode, S_IRUGO, monitor_mode_show,
		   monitor_mode_store);

static ssize_t auto_compensate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;
	u8 value;

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->is_cneg_enabled(&cts_data->cts_dev, &value);
	cts_unlock_device(cts_dev);
	if (ret)
		return snprintf(buf, PAGE_SIZE,
			"Read auto compensate enable register failed %d\n", ret);

	return snprintf(buf, PAGE_SIZE, "Auto compensate: %s\n",
		value ? "Enable" : "Disable");
}

static DEVICE_ATTR(auto_compensate, S_IRUGO, auto_compensate_show, NULL);

#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
static ssize_t driver_builtin_firmware_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i, count = 0;

	count += snprintf(buf + count, PAGE_SIZE - count,
		"Total %d builtin firmware:\n", cts_get_num_driver_builtin_firmware());

	for (i = 0; i < cts_get_num_driver_builtin_firmware(); i++) {
		const struct cts_firmware *firmware =
		    cts_request_driver_builtin_firmware_by_index(i);
		if (firmware)
			count += snprintf(buf + count, PAGE_SIZE - count,
					  "%-2d: hwid: %04x fwid: %04x ver: %04x size: %6zu desc: %s\n",
					  i, firmware->hwid, firmware->fwid,
					  FIRMWARE_VERSION(firmware),
					  firmware->size, firmware->name);
		else
			count += snprintf(buf + count, PAGE_SIZE - count,
					  "%-2d: INVALID\n", i);
	}

	return count;
}

/* echo index/name [flash/sram] > driver_builtin_firmware */
static ssize_t driver_builtin_firmware_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	const struct cts_firmware *firmware;
	bool to_flash = true;
	int ret, index = -1;

	parse_arg(buf, count);

	if (argc != 1 && argc != 2) {
		cts_err("Invalid num args %d\n"
			"  echo index/name [flash/sram] > driver_builtin_firmware\n",
			argc);
		return -EFAULT;
	}

	if (isdigit(*argv[0]))
		index = simple_strtoul(argv[0], NULL, 0);

	if (argc > 1) {
		if (strncasecmp(argv[1], "flash", 5) == 0)
			to_flash = true;
		else if (strncasecmp(argv[1], "sram", 4) == 0)
			to_flash = false;
		else {
			cts_err
			    ("Invalid location '%s', must be 'flash' or 'sram'",
			     argv[1]);
			return -EINVAL;
		}
	}

	cts_info("Update driver builtin firmware '%s' to %s",
		 argv[1], to_flash ? "flash" : "sram");

	if (index >= 0 && index < cts_get_num_driver_builtin_firmware())
		firmware = cts_request_driver_builtin_firmware_by_index(index);
	else
		firmware = cts_request_driver_builtin_firmware_by_name(argv[0]);

	if (firmware) {
		ret = cts_stop_device(cts_dev);
		if (ret) {
			cts_err("Stop device failed %d", ret);
			return ret;
		}

		cts_lock_device(cts_dev);
		ret = cts_update_firmware(cts_dev, firmware, to_flash);
		cts_unlock_device(cts_dev);

		if (ret) {
			cts_err("Update firmware failed %d", ret);
			goto err_start_device;
		}

		ret = cts_start_device(cts_dev);
		if (ret) {
			cts_err("Start device failed %d", ret);
			return ret;
		}
	} else {
		cts_err("Firmware '%s' NOT found", argv[0]);
		return -ENOENT;
	}

	return count;

err_start_device:
	cts_start_device(cts_dev);

	return ret;
}

static DEVICE_ATTR(driver_builtin_firmware, S_IWUSR | S_IRUGO,
		   driver_builtin_firmware_show, driver_builtin_firmware_store);
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */

#ifdef CFG_CTS_FIRMWARE_IN_FS
/* echo filepath [flash/sram] > update_firmware_from_file */
static ssize_t update_firmware_from_file_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	const struct cts_firmware *firmware;
	bool to_flash = true;
	int ret;

	parse_arg(buf, count);

	if (argc > 2) {
		cts_err("Invalid num args %d\n"
			"  echo filepath [flash/sram] > update_from_file\n",
			argc);
		return -EFAULT;
	} else if (argc > 1) {
		if (strncasecmp(argv[1], "flash", 5) == 0)
			to_flash = true;
		else if (strncasecmp(argv[1], "sram", 4) == 0)
			to_flash = false;
		else {
			cts_err
			    ("Invalid location '%s', must be 'flash' or 'sram'",
			     argv[1]);
			return -EINVAL;
		}
	}

	cts_info("Update firmware from file '%s'", argv[0]);

	firmware = cts_request_firmware_from_fs(cts_dev, argv[0]);
	if (firmware == NULL) {
		cts_err("Request firmware from file '%s' failed", argv[0]);
		return -ENOENT;
	}

	ret = cts_stop_device(cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
		goto err_release_firmware;
	}

	cts_lock_device(cts_dev);
	ret = cts_update_firmware(cts_dev, firmware, to_flash);
	cts_unlock_device(cts_dev);

	if (ret) {
		cts_err("Update firmware failed %d", ret);
		goto err_release_firmware;
	}

	ret = cts_start_device(cts_dev);
	if (ret) {
		cts_err("Start device failed %d", ret);
		goto err_release_firmware;
	}

	ret = count;

err_release_firmware:
	cts_release_firmware(firmware);

	return ret;
}

static DEVICE_ATTR(update_from_file, S_IWUSR, NULL,
		   update_firmware_from_file_store);
#endif /* CFG_CTS_FIRMWARE_IN_FS */

static ssize_t updating_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Updating: %s\n",
			cts_data->cts_dev.rtdata.updating ? "Y" : "N");
}

static DEVICE_ATTR(updating, S_IRUGO, updating_show, NULL);

static struct attribute *cts_dev_firmware_atts[] = {
	&dev_attr_curr_version.attr,
	&dev_attr_curr_ddi_version.attr,
	&dev_attr_rows.attr,
	&dev_attr_cols.attr,
	&dev_attr_res_x.attr,
	&dev_attr_res_y.attr,
	&dev_attr_esd_protection.attr,
	&dev_attr_monitor_mode.attr,
	&dev_attr_auto_compensate.attr,
#ifdef CFG_CTS_DRIVER_BUILTIN_FIRMWARE
	&dev_attr_driver_builtin_firmware.attr,
#endif /* CFG_CTS_DRIVER_BUILTIN_FIRMWARE */
#ifdef CFG_CTS_FIRMWARE_IN_FS
	&dev_attr_update_from_file.attr,
#endif /* CFG_CTS_FIRMWARE_IN_FS */
	&dev_attr_updating.attr,
	NULL
};

static const struct attribute_group cts_dev_firmware_attr_group = {
	.name = "cts_firmware",
	.attrs = cts_dev_firmware_atts,
};

static ssize_t flash_info_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	const struct cts_flash *flash;

	if (cts_dev->flash == NULL) {
		bool program_mode;
		bool enabled;
		int ret;

		program_mode = cts_is_device_program_mode(cts_dev);
		enabled = cts_is_device_enabled(cts_dev);

		ret = cts_prepare_flash_operation(cts_dev);
		if (ret)
			return snprintf(buf, PAGE_SIZE,
					"Prepare flash operation failed %d",
					ret);

		cts_post_flash_operation(cts_dev);

		if (!program_mode) {
			ret = cts_enter_normal_mode(cts_dev);
			if (ret)
				return snprintf(buf, PAGE_SIZE,
						"Enter normal mode failed %d",
						ret);
		}

		if (enabled) {
			ret = cts_start_device(cts_dev);
			if (ret)
				return snprintf(buf, PAGE_SIZE,
						"Start device failed %d", ret);
		}

		if (cts_dev->flash == NULL)
			return snprintf(buf, PAGE_SIZE, "Flash not found\n");
	}

	flash = cts_dev->flash;
	return snprintf(buf, PAGE_SIZE,
			"%s:\n"
			"  JEDEC ID   : %06X\n"
			"  Page size  : 0x%zx\n"
			"  Sector size: 0x%zx\n"
			"  Block size : 0x%zx\n"
			"  Total size : 0x%zx\n",
			flash->name, flash->jedec_id, flash->page_size,
			flash->sector_size, flash->block_size,
			flash->total_size);
}

static DEVICE_ATTR(info, S_IRUGO, flash_info_show, NULL);

static ssize_t read_flash_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u32 flash_addr, size, i, remaining;
	u8 *data = NULL;
	ssize_t count = 0;
	int ret;
	bool program_mode;
	bool enabled;
#ifndef CFG_CTS_FOR_GKI
	loff_t pos = 0;
#endif

	if (argc != 2 && argc != 3)
		return snprintf(buf, PAGE_SIZE, "Invalid num args %d\n", argc);

	ret = kstrtou32(argv[0], 0, &flash_addr);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Invalid flash addr: %s\n", argv[0]);
	ret = kstrtou32(argv[1], 0, &size);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Invalid size: %s\n", argv[1]);

	data = (u8 *) kmalloc(size, GFP_KERNEL);
	if (data == NULL)
		return snprintf(buf, PAGE_SIZE, "Allocate buffer for read data failed\n");

	cts_info("Read flash from 0x%06x size %u%s%s", flash_addr, size,
		argc == 3 ? " to file " : "", argc == 3 ? argv[2] : "");

	cts_lock_device(cts_dev);
	program_mode = cts_is_device_program_mode(cts_dev);
	enabled = cts_is_device_enabled(cts_dev);

	ret = cts_prepare_flash_operation(cts_dev);
	if (ret) {
		count += snprintf(buf, PAGE_SIZE, "Prepare flash operation failed %d", ret);
		goto err_free_data;
	}

	ret = cts_read_flash(cts_dev, flash_addr, data, size);
	if (ret) {
		count = snprintf(buf, PAGE_SIZE, "Read flash data failed %d\n", ret);
		goto err_post_flash_operation;
	}

	if (argc == 3) {
#ifdef CFG_CTS_FOR_GKI
		cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
#else
		struct file *file;

		cts_info("Write flash data to file '%s'", argv[2]);

		file = filp_open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (IS_ERR(file)) {
			count += snprintf(buf, PAGE_SIZE, "Open file '%s' failed %ld",
				argv[2], PTR_ERR(file));
			goto err_post_flash_operation;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		ret = kernel_write(file, data, size, &pos);
#else
		ret = kernel_write(file, data, size, pos);
#endif
		if (ret != size)
			count += snprintf(buf, PAGE_SIZE,
				"Write flash data to file '%s' failed %d", argv[2], ret);

		ret = filp_close(file, NULL);
		if (ret)
			count += snprintf(buf, PAGE_SIZE,
				"Close file '%s' failed %d", argv[2], ret);
#endif
	} else {
#define PRINT_ROW_SIZE          (16)
		remaining = size;
		for (i = 0; i < size && count <= PAGE_SIZE; i += PRINT_ROW_SIZE) {
			size_t linelen = min((size_t)remaining, (size_t)PRINT_ROW_SIZE);

			remaining -= PRINT_ROW_SIZE;

			count += snprintf(buf + count, PAGE_SIZE - count - 1,
				"%04x-%04x: ", flash_addr >> 16, flash_addr & 0xFFFF);
			/* Lower version kernel return void */
			hex_dump_to_buffer(data + i, linelen, PRINT_ROW_SIZE, 1,
				buf + count, PAGE_SIZE - count - 1, true);
			count += strlen(buf + count);
			buf[count++] = '\n';
			flash_addr += linelen;
#undef PRINT_ROW_SIZE
		}
	}

err_post_flash_operation:
	cts_post_flash_operation(cts_dev);

	if (!program_mode) {
		int r = cts_enter_normal_mode(cts_dev);

		if (r)
			count += snprintf(buf, PAGE_SIZE, "Enter normal mode failed %d", r);
	}

	if (enabled) {
		int r = cts_start_device(cts_dev);

		if (r) {
			cts_unlock_device(cts_dev);
			return snprintf(buf, PAGE_SIZE, "Start device failed %d", r);
		}
	}
err_free_data:
	cts_unlock_device(cts_dev);
	kfree(data);

	return (ret < 0 ? ret : count);
}

/* echo start_addr size [filepath] > read */
static ssize_t read_flash_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	parse_arg(buf, count);

	return count;
}

static DEVICE_ATTR(read, S_IWUSR | S_IRUGO, read_flash_show, read_flash_store);

/* echo addr size > erase */
static ssize_t erase_flash_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u32 flash_addr, size;
	int ret;
	bool program_mode;
	bool enabled;

	parse_arg(buf, count);

	if (argc != 2) {
		cts_err("Invalid num args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou32(argv[0], 0, &flash_addr);
	if (ret) {
		cts_err("Invalid flash addr: %s", argv[0]);
		return -EINVAL;
	}
	ret = kstrtou32(argv[1], 0, &size);
	if (ret) {
		cts_err("Invalid size: %s", argv[1]);
		return -EINVAL;
	}

	cts_info("Erase flash from 0x%06x size %u", flash_addr, size);

	cts_lock_device(cts_dev);
	program_mode = cts_is_device_program_mode(cts_dev);
	enabled = cts_is_device_enabled(cts_dev);

	ret = cts_prepare_flash_operation(cts_dev);
	if (ret) {
		cts_err("Prepare flash operation failed %d", ret);
		cts_unlock_device(cts_dev);
		return ret;
	}

	ret = cts_erase_flash(cts_dev, flash_addr, size);
	if (ret) {
		cts_err("Erase flash from 0x%06x size %u failed %d",
			flash_addr, size, ret);
		goto err_post_flash_operation;
	}

err_post_flash_operation:
	cts_post_flash_operation(cts_dev);

	if (!program_mode) {
		int r = cts_enter_normal_mode(cts_dev);

		if (r)
			cts_err("Enter normal mode failed %d", r);
	}

	if (enabled) {
		int r = cts_start_device(cts_dev);

		if (r)
			cts_err("Start device failed %d", r);
	}
	cts_unlock_device(cts_dev);

	return (ret < 0 ? ret : count);
}

static DEVICE_ATTR(erase, S_IWUSR, NULL, erase_flash_store);

static struct attribute *cts_dev_flash_attrs[] = {
	&dev_attr_info.attr,
	&dev_attr_read.attr,
	&dev_attr_erase.attr,

	NULL
};

static const struct attribute_group cts_dev_flash_attr_group = {
	.name = "flash",
	.attrs = cts_dev_flash_attrs,
};

static ssize_t open_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_OPEN,
		.flags = CTS_TEST_FLAG_VALIDATE_DATA |
				 CTS_TEST_FLAG_VALIDATE_MIN |
				 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
		.test_data_filepath =
		    "/sdcard/open-test-data.txt",
		.num_invalid_node = 0,
		.invalid_nodes = NULL,
    };
    int min = 0;
    int ret;
    ktime_t start_time, end_time, delta_time;

    if (argc != 1) {
		return scnprintf(buf, PAGE_SIZE, "Invalid num args %d\n", argc);
    }

    ret = kstrtoint(argv[0], 0, &min);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE, "Invalid min thres: %s\n", argv[0]);
	}

    cts_info("Open test, threshold = %u", min);

    test_param.min = &min;

    start_time = ktime_get();

    ret = cts_test_open(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

	if (ret > 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Open test has %d nodes FAIL, min: %u, ELAPSED TIME: %lldms\n",
			ret, min, ktime_to_ms(delta_time));
	} else if (ret < 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Open test FAIL %d(%s), ELAPSED TIME: %lldms\n",
			ret, cts_strerror(ret), ktime_to_ms(delta_time));
	} else {
		return scnprintf(buf, PAGE_SIZE,
			"Open test PASS, ELAPSED TIME: %lldms\n",
			ktime_to_ms(delta_time));
	}

}

/* echo threshod > open_test */
static ssize_t open_test_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	parse_arg(buf, count);

	return count;
}

static DEVICE_ATTR(open_test, S_IWUSR | S_IRUGO, open_test_show,
		   open_test_store);

static ssize_t short_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_SHORT,
		.flags = CTS_TEST_FLAG_VALIDATE_DATA |
				 CTS_TEST_FLAG_VALIDATE_MIN |
				 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
		.test_data_filepath =
		    "/sdcard/short-test-data.txt",
		.num_invalid_node = 0,
		.invalid_nodes = NULL,
    };
    int min = 0;
    int ret;
    ktime_t start_time, end_time, delta_time;

	if (argc != 1) {
		return scnprintf(buf, PAGE_SIZE, "Invalid num args %d\n", argc);
	}

    ret = kstrtoint(argv[0], 0, &min);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE, "Invalid min thres: %s\n", argv[0]);
	}

    cts_info("Short test, threshold = %u", min);

    test_param.min = &min;

    start_time = ktime_get();

    ret = cts_test_short(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

	if (ret > 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Short test has %d nodes FAIL, min: %u, ELAPSED TIME: %lldms\n",
			ret, min, ktime_to_ms(delta_time));
	} else if (ret < 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Short test FAIL %d(%s), ELAPSED TIME: %lldms\n",
			ret, cts_strerror(ret), ktime_to_ms(delta_time));
	} else {
		return scnprintf(buf, PAGE_SIZE,
			"Short test PASS, ELAPSED TIME: %lldms\n",
			ktime_to_ms(delta_time));
	}
}

/* echo threshod > short_test */
static ssize_t short_test_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	parse_arg(buf, count);

	return count;
}

static DEVICE_ATTR(short_test, S_IWUSR | S_IRUGO,
		   short_test_show, short_test_store);

static ssize_t testing_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Testting: %s\n",
			cts_data->cts_dev.rtdata.testing ? "Y" : "N");
}

static DEVICE_ATTR(testing, S_IRUGO, testing_show, NULL);

#ifdef CFG_CTS_HAS_RESET_PIN
static ssize_t reset_pin_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CFG_CTS_HAS_RESET_PIN
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_RESET_PIN,
		.flags = 0,
    };
    int ret;
    ktime_t start_time, end_time, delta_time;

    start_time = ktime_get();

    ret = cts_test_reset_pin(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

	if (ret) {
		return scnprintf(buf, PAGE_SIZE,
				"Reset-Pin test FAIL %d(%s), ELAPSED TIME: %lldms\n",
				ret, cts_strerror(ret), ktime_to_ms(delta_time));
	} else {
		return scnprintf(buf, PAGE_SIZE,
			"Reset-Pin test PASS, ELAPSED TIME: %lldms\n",
			ktime_to_ms(delta_time));
	}
#else /* CFG_CTS_HAS_RESET_PIN */
	return scnprintf(buf, PAGE_SIZE,
		"Reset-Pin test NOT supported(CFG_CTS_HAS_RESET_PIN not defined)\n");
#endif
}
static DEVICE_ATTR(reset_pin_test, S_IRUGO, reset_pin_test_show, NULL);
#endif

static ssize_t int_pin_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_INT_PIN,
		.flags = 0,
    };
    int ret;
    ktime_t start_time, end_time, delta_time;

    start_time = ktime_get();

    ret = cts_test_int_pin(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

    if (ret) {
		return scnprintf(buf, PAGE_SIZE,
			"Int-Pin test FAIL %d(%s), ELAPSED TIME: %lldms\n",
				ret, cts_strerror(ret), ktime_to_ms(delta_time));
    } else {
		return scnprintf(buf, PAGE_SIZE,
			"Int-Pin test PASS, ELAPSED TIME: %lldms\n",
				ktime_to_ms(delta_time));
    }
}
static DEVICE_ATTR(int_pin_test, S_IRUGO, int_pin_test_show, NULL);

static ssize_t compensate_cap_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_COMPENSATE_CAP,
		.flags = CTS_TEST_FLAG_VALIDATE_DATA |
				 CTS_TEST_FLAG_VALIDATE_MIN |
				 CTS_TEST_FLAG_VALIDATE_MAX |
				 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
		.test_data_filepath =
			"/sdcard/comp-cap-test-data.txt",
		.num_invalid_node = 0,
		.invalid_nodes = NULL,
    };
    int min = 0, max = 0;
    int ret;
    ktime_t start_time, end_time, delta_time;

	if (argc != 2) {
		return scnprintf(buf, PAGE_SIZE, "Invalid num args\n"
					"USAGE:\n"
					"  1. echo min max > compensate_cap_test\n"
					"  2. cat compensate_cap_test\n");
	}

    ret = kstrtoint(argv[0], 0, &min);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE,
				"Invalid min thres: %s\n", argv[0]);
	}

    ret = kstrtoint(argv[1], 0, &max);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE,
			"Invalid max thres: %s\n", argv[1]);
	}

	cts_info("Compensate cap test, min: %u, max: %u",
		 min, max);

    test_param.min = &min;
    test_param.max = &max;

    start_time = ktime_get();

    ret = cts_test_compensate_cap(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

	if (ret > 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Compensate cap test has %d nodes FAIL, "
			"threshold[%u, %u], ELAPSED TIME: %lldms\n",
			ret, min, max, ktime_to_ms(delta_time));
	} else if (ret < 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Compensate cap test FAIL %d(%s), ELAPSED TIME: %lldms\n",
			ret, cts_strerror(ret), ktime_to_ms(delta_time));
	} else {
		return scnprintf(buf, PAGE_SIZE,
			"Compensate cap test PASS, ELAPSED TIME: %lldms\n",
			ktime_to_ms(delta_time));
	}
}

/* echo threshod > short_test */
static ssize_t compensate_cap_test_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	parse_arg(buf, count);

	return count;
}

static DEVICE_ATTR(compensate_cap_test, S_IWUSR | S_IRUGO,
		   compensate_cap_test_show, compensate_cap_test_store);

static ssize_t rawdata_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_rawdata_test_priv_param priv_param = {
		.frames = 16,
		//.work_mode = 0,
    };
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_RAWDATA,
		.flags = CTS_TEST_FLAG_VALIDATE_DATA |
				 CTS_TEST_FLAG_VALIDATE_MIN |
				 CTS_TEST_FLAG_VALIDATE_MAX |
				 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
		.test_data_filepath =
			"/sdcard/rawdata-test-data.txt",
		.num_invalid_node = 0,
		.invalid_nodes = NULL,
		.priv_param = &priv_param,
		.priv_param_size = sizeof(priv_param),
    };

    int min, max;
    int ret;
    ktime_t start_time, end_time, delta_time;

	if (argc < 2 || argc > 3) {
		return scnprintf(buf, PAGE_SIZE, "Invalid num args\n"
			"USAGE:\n"
			"  1. echo min max [frames] > rawdata_test\n"
			"  2. cat rawdata_test\n");
	}

    ret = kstrtoint(argv[0], 0, &min);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE,
			"Invalid min thres: %s\n", argv[0]);
	}

	ret = kstrtoint(argv[1], 0, &max);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE,
			"Invalid max thres: %s\n", argv[1]);
	}

	if (argc > 2) {
		ret = kstrtou32(argv[2], 0, &priv_param.frames);
		if (ret) {
			return scnprintf(buf, PAGE_SIZE,
				"Invalid frames: %s\n", argv[2]);
		}
	}
	cts_info("Rawdata test, frames: %u min: %d, max: %d",
		priv_param.frames, min, max);

    test_param.min = &min;
    test_param.max = &max;

    start_time = ktime_get();

    ret = cts_test_rawdata(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

	if (ret > 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Rawdata test has %d nodes FAIL, threshold[%u, %u], "
			"ELAPSED TIME: %lldms\n",
			ret, min, max, ktime_to_ms(delta_time));
	} else if (ret < 0) {
		return scnprintf(buf, PAGE_SIZE,
		    "Rawdata test FAIL %d(%s), ELAPSED TIME: %lldms\n",
		    ret, cts_strerror(ret), ktime_to_ms(delta_time));
	} else {
		return scnprintf(buf, PAGE_SIZE,
			"Rawdata test PASS, ELAPSED TIME: %lldms\n",
			ktime_to_ms(delta_time));
	}
}

/* echo threshod > short_test */
static ssize_t rawdata_test_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	parse_arg(buf, count);

	return count;
}

static DEVICE_ATTR(rawdata_test, S_IWUSR | S_IRUGO,
		   rawdata_test_show, rawdata_test_store);

static ssize_t noise_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    struct cts_device *cts_dev = &cts_data->cts_dev;
    struct cts_noise_test_priv_param priv_param = {
		.frames = 50,
		//.work_mode = 0,
    };
    struct cts_test_param test_param = {
		.test_item = CTS_TEST_NOISE,
		.flags = CTS_TEST_FLAG_VALIDATE_DATA |
				 CTS_TEST_FLAG_VALIDATE_MAX |
				 CTS_TEST_FLAG_STOP_TEST_IF_VALIDATE_FAILED |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_CONSOLE |
				 CTS_TEST_FLAG_DUMP_TEST_DATA_TO_FILE,
		.test_data_filepath =
			"/sdcard/noise-test-data.txt",
		.num_invalid_node = 0,
		.invalid_nodes = NULL,
		.priv_param = &priv_param,
		.priv_param_size = sizeof(priv_param),
    };

    int max;
    int ret;
    ktime_t start_time, end_time, delta_time;

	if (argc < 1 || argc > 2) {
		return scnprintf(buf, PAGE_SIZE, "Invalid num args\n"
			"USAGE:\n"
			"  1. echo threshold [frames] > noise_test\n"
			"  2. cat noise_test\n");
	}

    ret = kstrtoint(argv[0], 0, &max);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE,
			"Invalid max thres: %s\n", argv[0]);
	}

	if (argc > 1) {
		ret = kstrtou32(argv[1], 0, &priv_param.frames);
		if (ret) {
			return scnprintf(buf, PAGE_SIZE,
				"Invalid frames: %s\n", argv[1]);
		}
	}
	cts_info("Noise test, frames: %u threshold: %d",
		priv_param.frames, max);

    test_param.max = &max;

    start_time = ktime_get();

    ret = cts_test_noise(cts_dev, &test_param);

    end_time = ktime_get();
    delta_time = ktime_sub(end_time, start_time);

	if (ret > 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Noise test has %d nodes FAIL, max: %u, ELAPSED TIME: %lldms\n",
			ret, max, ktime_to_ms(delta_time));
	} else if (ret < 0) {
		return scnprintf(buf, PAGE_SIZE,
			"Noise test FAIL %d(%s), ELAPSED TIME: %lldms\n",
			ret, cts_strerror(ret), ktime_to_ms(delta_time));
	} else {
		return scnprintf(buf, PAGE_SIZE,
			"Noise test PASS, ELAPSED TIME: %lldms\n",
			ktime_to_ms(delta_time));
	}
}

static ssize_t noise_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
    parse_arg(buf, count);

    return count;
}
static DEVICE_ATTR(noise_test, S_IWUSR | S_IRUGO,
		noise_test_show, noise_test_store);

static struct attribute *cts_dev_test_atts[] = {
	&dev_attr_open_test.attr,
	&dev_attr_short_test.attr,
	&dev_attr_testing.attr,
#ifdef CFG_CTS_HAS_RESET_PIN
	&dev_attr_reset_pin_test.attr,
#endif
	&dev_attr_int_pin_test.attr,
	&dev_attr_compensate_cap_test.attr,
	&dev_attr_rawdata_test.attr,
	&dev_attr_noise_test.attr,
	NULL
};

static const struct attribute_group cts_dev_test_attr_group = {
	.name = "test",
	.attrs = cts_dev_test_atts,
};

static ssize_t ic_type_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "IC Type : %s\n",
			cts_data->cts_dev.hwdata->name);
}

static DEVICE_ATTR(ic_type, S_IRUGO, ic_type_show, NULL);

static ssize_t program_mode_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "Program mode: %s\n",
			cts_data->cts_dev.rtdata.program_mode ? "Y" : "N");
}

static ssize_t program_mode_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	int ret;

	parse_arg(buf, count);

	if (argc != 1) {
		cts_err("Invalid num args %d", argc);
		return -EFAULT;
	}

	if (*argv[0] == '1' || tolower(*argv[0]) == 'y') {
		ret = cts_enter_program_mode(&cts_data->cts_dev);
		if (ret) {
			cts_err("Enter program mode failed %d", ret);
			return ret;
		}
	} else if (*argv[0] == '0' || tolower(*argv[0]) == 'n') {
		ret = cts_enter_normal_mode(&cts_data->cts_dev);
		if (ret) {
			cts_err("Exit program mode failed %d", ret);
			return ret;
		}
	} else
		cts_err("Invalid args");

	return count;
}

static DEVICE_ATTR(program_mode, S_IWUSR | S_IRUGO,
	program_mode_show, program_mode_store);

static ssize_t rawdata_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
#define RAWDATA_BUFFER_SIZE(cts_dev) \
	(cts_dev->hwdata->num_row * cts_dev->hwdata->num_col * 2)

	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u16 *rawdata = NULL;
	int ret, r, c, count = 0;
	u32 max, min, sum, average;
	int max_r, max_c, min_r, min_c;
	bool data_valid = true;

	cts_info("Show rawdata");

	rawdata = (u16 *) kmalloc(RAWDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata == NULL)
		return snprintf(buf, PAGE_SIZE,
				"Allocate memory for rawdata failed\n");

	cts_lock_device(cts_dev);
	ret =
	    cts_dev->ops->top_get_rawdata(cts_dev, (u8 *) rawdata,
					  RAWDATA_BUFFER_SIZE(cts_dev));
	if (ret < 0) {
		cts_err("Get rawdata failed");
		goto err_free_rawdata;
	}
	data_valid = true;

	if (data_valid) {
		max = min = rawdata[0];
		sum = 0;
		max_r = max_c = min_r = min_c = 0;
		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			for (c = 0; c < cts_dev->fwdata.cols; c++) {
				u16 val =
				    rawdata[r * cts_dev->hwdata->num_col + c];
				sum += val;
				if (val > max) {
					max = val;
					max_r = r;
					max_c = c;
				} else if (val < min) {
					min = val;
					min_r = r;
					min_c = c;
				}
			}
		}
		average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

		count += snprintf(buf + count, PAGE_SIZE - count,
				  SPLIT_LINE_STR
				  "Raw data MIN: [%d][%d]=%u, MAX: [%d][%d]=%u, AVG=%u\n"
				  SPLIT_LINE_STR
				  "   |  ", min_r, min_c, min, max_r, max_c,
				  max, average);
		for (c = 0; c < cts_dev->fwdata.cols; c++)
			count += snprintf(buf + count, PAGE_SIZE - count,
					  COL_NUM_FORMAT_STR, c);

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "\n" SPLIT_LINE_STR);

		for (r = 0; r < cts_dev->fwdata.rows && count < PAGE_SIZE; r++) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					  ROW_NUM_FORMAT_STR, r);
			for (c = 0;
			     c < cts_dev->fwdata.cols && count < PAGE_SIZE;
			     c++)
				count +=
				    snprintf(buf + count, PAGE_SIZE - count - 1,
					     DATA_FORMAT_STR,
					     rawdata[r *
						     cts_dev->hwdata->num_col +
						     c]);

			buf[count++] = '\n';
		}
	}

err_free_rawdata:
	cts_unlock_device(cts_dev);
	kfree(rawdata);

	return data_valid ? count : ret;

#undef RAWDATA_BUFFER_SIZE
}

static DEVICE_ATTR(rawdata, S_IRUGO, rawdata_show, NULL);

static ssize_t diffdata_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	s16 *diffdata = NULL;
	int ret, r, c, count = 0;
	int max, min, sum, average;
	int max_r, max_c, min_r, min_c;
	bool data_valid = true;

	cts_info("Show diffdata");

	diffdata = (s16 *) kmalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (diffdata == NULL) {
		cts_err("Allocate memory for diffdata failed");
		return -ENOMEM;
	}

	cts_lock_device(cts_dev);
	ret =
	    cts_dev->ops->top_get_real_diff(cts_dev, (u8 *) diffdata,
					    DIFFDATA_BUFFER_SIZE(cts_dev));
	if (ret < 0) {
		cts_err("Get diffdata failed");
		goto err_free_diffdata;
	}
	data_valid = true;

	if (data_valid) {
		max = min = diffdata[0];
		sum = 0;
		max_r = max_c = min_r = min_c = 0;
		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			for (c = 0; c < cts_dev->fwdata.cols; c++) {
				s16 val =
				    diffdata[r * cts_dev->hwdata->num_col + c];

				sum += val;
				if (val > max) {
					max = val;
					max_r = r;
					max_c = c;
				} else if (val < min) {
					min = val;
					min_r = r;
					min_c = c;
				}
			}
		}
		average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

		count += snprintf(buf + count, PAGE_SIZE - count,
				  SPLIT_LINE_STR
				  "Diff data MIN: [%d][%d]=%d, MAX: [%d][%d]=%d, AVG=%d\n"
				  SPLIT_LINE_STR
				  "   |  ", min_r, min_c, min, max_r, max_c,
				  max, average);
		for (c = 0; c < cts_dev->fwdata.cols; c++)
			count += snprintf(buf + count, PAGE_SIZE - count,
					  COL_NUM_FORMAT_STR, c);

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "\n" SPLIT_LINE_STR);

		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					  ROW_NUM_FORMAT_STR, r);
			for (c = 0; c < cts_dev->fwdata.cols; c++)
				count +=
				    snprintf(buf + count, PAGE_SIZE - count,
					     DATA_FORMAT_STR,
					     diffdata[r *
						      cts_dev->hwdata->num_col +
						      c]);

			buf[count++] = '\n';
		}
	}

err_free_diffdata:
	cts_unlock_device(cts_dev);
	kfree(diffdata);

	return data_valid ? count : ret;
}

static DEVICE_ATTR(diffdata, S_IRUGO, diffdata_show, NULL);

static ssize_t manualdiffdata_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	s16 *rawdata = NULL;
	int ret, r, c, count = 0;
	int max, min, sum, average;
	int max_r, max_c, min_r, min_c;
	bool data_valid = true;
	int frame;
	int i;
#ifndef CFG_CTS_FOR_GKI
	struct file *file = NULL;
	loff_t pos = 0;
#endif

	cts_info("Show manualdiff");

#ifdef CFG_CTS_FOR_GKI
	if (argc != 1) {
		return scnprintf(buf, PAGE_SIZE, "Invalid num args\n"
			"USAGE:\n"
			"  1. echo updatebase > manualdiff\n"
			"  2. cat manualdiff\n"
			"  or\n"
			"  1. echo updatebase > manualdiff\n"
			"  2. echo frame > manualdiff\n"
			"  3. cat manualdiff\n");
	}
	ret = kstrtou32(argv[0], 0, &frame);
	if (ret) {
		return scnprintf(buf, PAGE_SIZE, "Invalid frame num\n");
	}
#else
	if (argc != 1 && argc != 2) {
		return snprintf(buf, PAGE_SIZE, "Invalid num args\n"
				"USAGE:\n"
				"  1. echo updatebase > manualdiff\n"
				"  2. cat manualdiff\n"
				"  or\n"
				"  1. echo updatebase > manualdiff\n"
				"  2. echo frame file > manualdiff\n"
				"  3. cat manualdiff\n");
	}

	if (argc == 2) {
		ret = kstrtou32(argv[0], 0, &frame);
		if (ret)
			return snprintf(buf, PAGE_SIZE, "Invalid frame num\n");
		file = filp_open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
		if (IS_ERR(file))
			return snprintf(buf, PAGE_SIZE, "Can't open file:%s", argv[1]);
	} else
		frame = 1;
#endif

	rawdata = (s16 *) kmalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata == NULL) {
		cts_err("Allocate memory for rawdata failed");
#ifndef CFG_CTS_FOR_GKI
		filp_close(file, NULL);
#endif
		return -ENOMEM;
	}

	if (!manualdiff_base && (DIFFDATA_BUFFER_SIZE(cts_dev) > 0)) {
		manualdiff_base = (s16 *) kzalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
		if (manualdiff_base == NULL) {
			cts_err("Malloc manualdiff_base failed");
#ifndef CFG_CTS_FOR_GKI
			filp_close(file, NULL);
#endif
			kfree(rawdata);
			goto err_free_diffdata;
		}
	}
	cts_lock_device(cts_dev);

	cts_info("frame %d, file:%s", frame, argv[1]);

	if (!manualdiff_base_updated) {
		ret = cts_dev->ops->top_get_rawdata(cts_dev, (u8 *) manualdiff_base,
			DIFFDATA_BUFFER_SIZE(cts_dev));
		if (ret)
			cts_err("Get raw data failed %d", ret);
	}
	for (i = 0; i < frame; i++) {
		ret = cts_dev->ops->top_get_rawdata(cts_dev, (u8 *) rawdata,
			DIFFDATA_BUFFER_SIZE(cts_dev));
		if (ret) {
			cts_err("Get raw data failed %d", ret);
			data_valid = false;
		} else
			data_valid = true;
		msleep(5);

		if (data_valid) {
			max = -32768;
			min = 32767;
			sum = 0;
			max_r = max_c = min_r = min_c = 0;
			for (r = 0; r < cts_dev->fwdata.rows; r++) {
				for (c = 0; c < cts_dev->fwdata.cols; c++) {
					s16 val;

					rawdata[r * cts_dev->hwdata->num_col + c] -=
					    manualdiff_base[r * cts_dev->hwdata->num_col + c];
					val = rawdata[r * cts_dev->hwdata->num_col + c];
					sum += val;
					if (val > max) {
						max = val;
						max_r = r;
						max_c = c;
					} else if (val < min) {
						min = val;
						min_r = r;
						min_c = c;
					}
				}
			}
			average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);
			count += snprintf(buf + count, PAGE_SIZE - count, 
				SPLIT_LINE_STR "Manualdiff data MIN: [%d][%d]=%d, MAX: [%d][%d]=%d, AVG=%d\n"
				SPLIT_LINE_STR "   |  ", min_r, min_c, min, max_r, max_c, max, average);
			for (c = 0; c < cts_dev->fwdata.cols; c++)
				count += snprintf(buf + count, PAGE_SIZE - count, COL_NUM_FORMAT_STR, c);

			count += snprintf(buf + count, PAGE_SIZE - count, "\n" SPLIT_LINE_STR);

			for (r = 0; r < cts_dev->fwdata.rows; r++) {
				count += snprintf(buf + count, PAGE_SIZE - count, ROW_NUM_FORMAT_STR, r);
				for (c = 0; c < cts_dev->fwdata.cols; c++)
					count += snprintf(buf + count, PAGE_SIZE - count, DATA_FORMAT_STR,
						rawdata[r * cts_dev->hwdata->num_col + c]);

				buf[count++] = '\n';
			}
			/*cts_info("manualdiffdata_show:%d, %d", i, frame); */
			if (argc == 2) {
#ifdef CFG_CTS_FOR_GKI
				cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
#else
				pos = file->f_pos;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
				ret = kernel_write(file, buf, count, &pos);
#else
				ret = kernel_write(file, buf, count, pos);
#endif
				if (ret != count)
					cts_err
					    ("Write data to file '%s' failed %d",
					     argv[1], ret);

				file->f_pos += count;
				count = 0;
#endif
			}
		}
	}
#ifndef CFG_CTS_FOR_GKI
	if (argc == 2)
		filp_close(file, NULL);
#endif

err_free_diffdata:
	cts_unlock_device(cts_dev);
	kfree(rawdata);

	return data_valid ? count : ret;
}

static ssize_t manualdiffdata_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret;

	parse_arg(buf, count);

	cts_lock_device(cts_dev);
	if (strncasecmp("updatebase", argv[0], 10) == 0) {
		ret = cts_dev->ops->enable_get_rawdata(cts_dev);
		if (ret) {
			cts_err("Enable read raw data failed %d", ret);
			goto err_manual_diff_store;
		}

		if (!manualdiff_base && (DIFFDATA_BUFFER_SIZE(cts_dev) > 0)) {
			manualdiff_base = (s16 *) kzalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
			if (manualdiff_base == NULL) {
				cts_err("Malloc manualdiff_base failed");
				goto err_manual_diff_store;
			}
		}
		if (manualdiff_base != NULL) {
			ret = cts_get_rawdata(cts_dev, manualdiff_base);
			if (ret)
				cts_err("Get raw data failed %d", ret);
			manualdiff_base_updated = 1;
		}
		/* cts_info("update base successful"); */
		ret = cts_dev->ops->disable_get_rawdata(cts_dev);
		if (ret)
			cts_err("Disable read raw data failed %d", ret);
	}
err_manual_diff_store:
	cts_unlock_device(cts_dev);
	return count;
}

static DEVICE_ATTR(manualdiff, S_IRUSR | S_IWUSR, manualdiffdata_show,
		   manualdiffdata_store);

static ssize_t jitter_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t count)
{
	u16 cnt;
	int ret;

	parse_arg(buf, count);

	if (argc != 1) {
		cts_err("Invalid num args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou16(argv[0], 0, &cnt);
	if (ret == 0) {
		if (cnt > 2 && cnt < 10000)
			jitter_test_frame = cnt;
	}

	cts_info("jitter test frame: %d", jitter_test_frame);
	return count;
}

static ssize_t jitter_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	s16 *rawdata = NULL;
	s16 *rawdata_min = NULL;
	s16 *rawdata_max = NULL;
	int ret, r, c, count = 0;
	int max, min, sum, average;
	int max_r, max_c, min_r, min_c;
	bool data_valid = false;
	int i;

	cts_info("Show jitter");
	cts_lock_device(cts_dev);
	rawdata = (s16 *) kmalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata == NULL) {
		cts_err("Allocate memory for rawdata failed");
		ret = -ENOMEM;
		goto err_jitter_show_exit;
	}
	rawdata_min =
	    (s16 *) kmalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata_min == NULL) {
		cts_err("Allocate memory for rawdata failed");
		ret = -ENOMEM;
		goto err_free_rawdata;
	}
	rawdata_max =
	    (s16 *) kmalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
	if (rawdata_max == NULL) {
		cts_err("Allocate memory for rawdata failed");
		ret = -ENOMEM;
		goto err_free_rawdata_min;
	}

	for (i = 0; i < DIFFDATA_BUFFER_SIZE(cts_dev) / 2; i++) {
		rawdata_min[i] = 32767;
		rawdata_max[i] = -32768;
	}

	for (i = 0; i < jitter_test_frame; i++) {
		ret =
		    cts_dev->ops->top_get_rawdata(cts_dev, (u8 *) rawdata,
						  DIFFDATA_BUFFER_SIZE
						  (cts_dev));
		if (ret) {
			cts_err("Get raw data failed %d", ret);
			msleep(5);
			continue;
		}
		for (r = 0; r < cts_dev->hwdata->num_row; r++) {
			for (c = 0; c < cts_dev->hwdata->num_col; c++) {
				int index;

				index = r * cts_dev->hwdata->num_col + c;
				if (rawdata_min[index] > rawdata[index])
					rawdata_min[index] = rawdata[index];
				else if (rawdata_max[index] < rawdata[index])
					rawdata_max[index] = rawdata[index];
			}
		}
	}
	data_valid = true;

	if (data_valid) {
		max = -32768;
		min = 32767;
		sum = 0;
		max_r = max_c = min_r = min_c = 0;
		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			for (c = 0; c < cts_dev->fwdata.cols; c++) {
				s16 val;
				int index = r * cts_dev->hwdata->num_col + c;

				val = rawdata_max[index] - rawdata_min[index];
				rawdata[index] = val;
				sum += val;
				if (val > max) {
					max = val;
					max_r = r;
					max_c = c;
				} else if (val < min) {
					min = val;
					min_r = r;
					min_c = c;
				}
			}
		}
		average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

		count += snprintf(buf + count, PAGE_SIZE - count,
				  SPLIT_LINE_STR
				  "Jitter data MIN: [%d][%d]=%d, MAX: [%d][%d]=%d, AVG=%d, TOTAL FRAME=%d\n"
				  SPLIT_LINE_STR
				  "   |  ", min_r, min_c, min, max_r, max_c,
				  max, average, jitter_test_frame);
		for (c = 0; c < cts_dev->fwdata.cols; c++)
			count += snprintf(buf + count, PAGE_SIZE - count,
					  COL_NUM_FORMAT_STR, c);

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "\n" SPLIT_LINE_STR);

		for (r = 0; r < cts_dev->fwdata.rows; r++) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					  ROW_NUM_FORMAT_STR, r);
			for (c = 0; c < cts_dev->fwdata.cols; c++)
				count +=
				    snprintf(buf + count, PAGE_SIZE - count,
					     DATA_FORMAT_STR,
					     rawdata[r *
						     cts_dev->hwdata->num_col +
						     c]);

			buf[count++] = '\n';
		}
	}

	//err_free_rawdata_max:
	kfree(rawdata_max);
err_free_rawdata_min:
	kfree(rawdata_min);
err_free_rawdata:
	kfree(rawdata);
err_jitter_show_exit:
	cts_unlock_device(cts_dev);
	return data_valid ? count : ret;
}

static DEVICE_ATTR(jitter, S_IRUSR | S_IWUSR, jitter_show, jitter_store);

static ssize_t compensate_cap_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 *cap = NULL;
	int ret;
	ssize_t count = 0;
	int r, c, min, max, max_r, max_c, min_r, min_c, sum, average;

	cts_info("Read '%s'", attr->attr.name);

	cap = kzalloc(cts_dev->hwdata->num_row * cts_dev->hwdata->num_col,
		      GFP_KERNEL);
	if (cap == NULL)
		return scnprintf(buf, PAGE_SIZE,
				 "Allocate mem for compensate cap failed\n");

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->top_get_cnegdata(cts_dev, cap,
					     cts_dev->hwdata->num_row *
					     cts_dev->hwdata->num_col);
	cts_unlock_device(cts_dev);
	if (ret) {
		kfree(cap);
		return scnprintf(buf, PAGE_SIZE,
				 "Get compensate cap failed %d\n", ret);
	}

	max = min = cap[0];
	sum = 0;
	max_r = max_c = min_r = min_c = 0;
	for (r = 0; r < cts_dev->fwdata.rows; r++) {
		for (c = 0; c < cts_dev->fwdata.cols; c++) {
			u16 val = cap[r * cts_dev->hwdata->num_col + c];

			sum += val;
			if (val > max) {
				max = val;
				max_r = r;
				max_c = c;
			} else if (val < min) {
				min = val;
				min_r = r;
				min_c = c;
			}
		}
	}
	average = sum / (cts_dev->fwdata.rows * cts_dev->fwdata.cols);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "--------------------------------------"\
			   "--------------------------------------\n"
			   " Compensatete Cap MIN: [%d][%d]=%u, MAX: [%d][%d]=%u, AVG=%u\n"
			   "---+----------------------------------"\
			   "--------------------------------------\n"
			   "   |", min_r, min_c, min, max_r, max_c, max,
			   average);
	for (c = 0; c < cts_dev->fwdata.cols; c++)
		count += scnprintf(buf + count, PAGE_SIZE - count, " %3u", c);

	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "\n"
			   "---+----------------------------------"\
			   "--------------------------------------\n");

	for (r = 0; r < cts_dev->fwdata.rows; r++) {
		count += scnprintf(buf + count, PAGE_SIZE - count, "%2u |", r);
		for (c = 0; c < cts_dev->fwdata.cols; c++) {
			count += scnprintf(buf + count, PAGE_SIZE - count,
					   " %3u",
					   cap[r * cts_dev->hwdata->num_col +
					       c]);
		}
		buf[count++] = '\n';
	}
	count += scnprintf(buf + count, PAGE_SIZE - count,
			   "---+----------------------------------"\
			   "--------------------------------------\n");

	kfree(cap);

	return count;
}

static DEVICE_ATTR(compensate_cap, S_IRUGO, compensate_cap_show, NULL);

#ifdef CFG_CTS_HAS_RESET_PIN
static ssize_t reset_pin_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	cts_info("Read RESET-PIN");

	return snprintf(buf, PAGE_SIZE,
			"Reset pin: %d, status: %d\n",
			cts_data->pdata->rst_gpio,
			gpio_get_value(cts_data->pdata->rst_gpio));
}

static ssize_t reset_pin_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;

	cts_info("Write RESET-PIN");
	cts_info("Chip staus maybe changed");

	cts_plat_set_reset(cts_dev->pdata, (buf[0] == '1') ? 1 : 0);
	return count;
}

static DEVICE_ATTR(reset_pin, S_IRUSR | S_IWUSR, reset_pin_show,
		   reset_pin_store);
#endif

static ssize_t irq_pin_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	cts_info("Read IRQ-PIN");

	return snprintf(buf, PAGE_SIZE,
			"IRQ pin: %d, status: %d\n",
			cts_data->pdata->int_gpio,
			gpio_get_value(cts_data->pdata->int_gpio));
}

static DEVICE_ATTR(irq_pin, S_IRUGO, irq_pin_show, NULL);

static ssize_t irq_info_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct irq_desc *desc;

	cts_info("Read IRQ-INFO");

	desc = irq_to_desc(cts_data->pdata->irq);
	if (desc == NULL) {
		return snprintf(buf, PAGE_SIZE,
				"IRQ: %d descriptor not found\n",
				cts_data->pdata->irq);
	}

    return scnprintf(buf, PAGE_SIZE,
	        "IRQ num: %d, depth: %u, "
	        "count: %u, unhandled: %u, last unhandled eslape: %lu, irq flags: 0x%x, int_mode: %s\n",
	        cts_data->pdata->irq, desc->depth,
	        desc->irq_count, desc->irqs_unhandled,
	        desc->last_unhandled, desc->action->flags, 
	        (desc->action->flags & IRQF_TRIGGER_MASK ) == IRQF_TRIGGER_RISING ? "IRQF_TRIGGER_RISING" : 
	        (desc->action->flags & IRQF_TRIGGER_MASK ) == IRQF_TRIGGER_FALLING ? "IRQF_TRIGGER_FALLING" : 
			(desc->action->flags & IRQF_TRIGGER_MASK ) == IRQF_TRIGGER_HIGH ? "IRQF_TRIGGER_HIGH" :
			(desc->action->flags & IRQF_TRIGGER_MASK ) == IRQF_TRIGGER_LOW ? "IRQF_TRIGGER_LOW " : "IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING");
}

static DEVICE_ATTR(irq_info, S_IRUGO, irq_info_show, NULL);

#ifdef CFG_CTS_FW_LOG_REDIRECT
static ssize_t fw_log_redirect_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;

	return snprintf(buf, PAGE_SIZE, "Fw log redirect is %s\n",
			cts_is_fw_log_redirect(cts_dev) ? "enable" : "disable");
}

static ssize_t fw_log_redirect_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 enable = 0;

	if (buf[0] == 'Y' || buf[0] == 'y' || buf[0] == '1')
		enable = 1;
	if (enable)
		cts_enable_fw_log_redirect(cts_dev);
	else
		cts_disable_fw_log_redirect(cts_dev);

	return count;
}

static DEVICE_ATTR(fw_log_redirect, S_IRUSR | S_IWUSR, fw_log_redirect_show,
		   fw_log_redirect_store);
#endif

static ssize_t debug_spi_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;

	return snprintf(buf, PAGE_SIZE, "spi_speed=%d\n",
			cts_dev->pdata->spi_speed);
}

static ssize_t debug_spi_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	u16 s = 0;
	int ret = 0;
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;

	parse_arg(buf, count);

	if (argc != 1) {
		cts_err("Invalid num args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou16(argv[0], 0, &s);
	if (ret) {
		cts_err("Invalid spi speed: %s", argv[0]);
		return -EINVAL;
	}

	cts_dev->pdata->spi_speed = s;

	return count;
}

static DEVICE_ATTR(debug_spi, S_IRUSR | S_IWUSR, debug_spi_show,
		   debug_spi_store);

#ifdef CFG_CTS_GESTURE
static ssize_t gesture_en_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;

	return sprintf(buf, "Gesture wakup is %s\n",
		       cts_is_gesture_wakeup_enabled(cts_dev) ? "enable" :
		       "disable");
}

static ssize_t gesture_en_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 enable = 0;

	if (buf[0] == 'Y' || buf[0] == 'y' || buf[0] == '1')
		enable = 1;
	/*C3T code for HQ-229322 by jishen at 2022/10/12  start*/	
	if (enable){

		cts_enable_gesture_wakeup(cts_dev);
		cts_gesture_flag = true;
		cts_info("cts_gesture_flag : %d\n",cts_gesture_flag);			
	}
	else{
		cts_disable_gesture_wakeup(cts_dev);
		cts_gesture_flag = false;
		cts_info("cts_gesture_flag : %d\n",cts_gesture_flag);
		
	}
	/*C3T code for HQ-229322 by jishen at 2022/10/12  end*/	
	return count;
}

static DEVICE_ATTR(gesture_en, S_IRUSR | S_IWUSR, gesture_en_show,
		   gesture_en_store);
#endif

static ssize_t int_data_types_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%#04x\n",
			 cts_data->cts_dev.fwdata.int_data_types);
}

static ssize_t int_data_types_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	u16 type = 0;
	int ret = 0;

	parse_arg(buf, count);

	if (argc != 1) {
		cts_err("Invalid num args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou16(argv[0], 0, &type);
	if (ret) {
		cts_err("Invalid int data types: %s", argv[0]);
		return -EINVAL;
	}

	cts_lock_device(&cts_data->cts_dev);
	ret = cts_set_int_data_types(&cts_data->cts_dev, type);
	cts_unlock_device(&cts_data->cts_dev);
	if (ret)
		return -EIO;
	return count;
}

static DEVICE_ATTR(int_data_types, S_IWUSR | S_IRUGO,
		   int_data_types_show, int_data_types_store);

static ssize_t int_data_method_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 cts_data->cts_dev.fwdata.int_data_method);
}

static ssize_t int_data_method_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	u8 method = 0;
	int ret = 0;

	parse_arg(buf, count);

	if (argc != 1) {
		cts_err("Invalid num args %d", argc);
		return -EFAULT;
	}

	ret = kstrtou8(argv[0], 0, &method);
	if (ret) {
		cts_err("Invalid int data method: %s", argv[0]);
		return -EINVAL;
	} else if (method >= INT_DATA_METHOD_CNT) {
		cts_err("Invalid int data method: %s", argv[0]);
		return -EINVAL;
	}

	cts_lock_device(&cts_data->cts_dev);
	ret = cts_set_int_data_method(&cts_data->cts_dev, method);
	cts_unlock_device(&cts_data->cts_dev);
	if (ret)
		return -EIO;
	return count;
}

static DEVICE_ATTR(int_data_method, S_IWUSR | S_IRUGO,
		   int_data_method_show, int_data_method_store);


#ifdef CFG_DUMP_INT_DATA
/* For dump bin file */
static int cts_write_intdata(struct file *filp, const void *data, size_t size)
{
#ifdef CFG_CTS_FOR_GKI
	cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
	return -EPERM;
#else
    loff_t  pos;
    ssize_t ret;

    pos = filp->f_pos;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
    ret = kernel_write(filp, data, size, &pos);
#else
    ret = kernel_write(filp, data, size, pos);
#endif

    if (ret >= 0) {
        filp->f_pos += ret;
    }

    return ret;
#endif
}

u32 old_cnt;
struct file *cts_int_data_filp = NULL;
char chipone_bin_path[64] = "/sdcard/chipone.bin";
void cts_dump_int_tsdata(const u8 *data, size_t size)
{
    if (cts_int_data_filp) {
        cts_write_intdata(cts_int_data_filp, data, size);
    }
}

static int cts_start_dump_int_data_to_file(const char *filepath, bool append)
{
    int ret;
    u8 header[24] = { 0x00 };

    cts_info("Start dump test data to file '%s'", filepath);

#ifdef CFG_CTS_FOR_GKI
	cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
	ret = -EPERM;
	return ret;
#else
    cts_int_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC),
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_int_data_filp)) {
        ret = PTR_ERR(cts_int_data_filp);
        cts_int_data_filp = NULL;
        cts_err("Open file '%s' for test data failed %d",
            filepath, ret);
        return ret;
    }
#endif

    if (cts_int_data_filp)
        cts_dump_int_tsdata(header, sizeof(header));

    return 0;
}

static void cts_stop_dump_int_data_to_file(void)
{
    cts_info("Stop dump test data to file");

    if (cts_int_data_filp) {
#ifndef CFG_CTS_FOR_GKI
		int r;
        r = filp_close(cts_int_data_filp, NULL);
        if (r) {
            cts_err("Close test data file failed %d", r);
        }
#endif
        cts_int_data_filp = NULL;
    } else {
        cts_warn("Stop dump tsdata to file with filp = NULL");
    }
}

static void cts_redump_header_data_to_file(struct chipone_ts_data *cts_data,
        const char *filepath, bool append)
{
    int ret;
    u32 cnt;
    u8 header[24] = {
            /* Flag */              'C', 'h', 'i', 'p', 'o', 'n', 'e',
            /* Header len */        0x10,
            /* Version */           0x00, 0x00, 0x01 , 0x00,
            /* Data type */         0x00, 0x00,
            /* Rows and Cols */     32, 18,
            /* Frame cnt */         0x00, 0x00, 0x00, 0x00,
            /* Total time */        0x00, 0x00, 0x00, 0x00
    };

    header[12] = cts_data->cts_dev.fwdata.int_data_types & 0xFF;
    header[13] = (cts_data->cts_dev.fwdata.int_data_types >> 8) & 0xFF;
    header[14] = cts_data->cts_dev.fwdata.rows;
    header[15] = cts_data->cts_dev.fwdata.cols;

    if (old_cnt == 0) {
        cnt = 0xffffffff - cts_data->cts_dev.rtdata.dump_cnt;
    } else {
        cnt = old_cnt;
    }

    header[16] = cnt & 0xff;
    header[17] = (cnt >> 8) & 0xff;
    header[18] = (cnt >> 16) & 0xff;
    header[19] = (cnt >> 24) & 0xff;

#ifdef CFG_CTS_FOR_GKI
	cts_info("%s(): some functions are forbiddon with GKI Version!", __func__);
	ret = -EPERM;
	return;
#else
    cts_int_data_filp = filp_open(filepath,
        O_WRONLY | O_CREAT | O_WRONLY,
        S_IRUGO | S_IWUGO);
    if (IS_ERR(cts_int_data_filp)) {
        ret = PTR_ERR(cts_int_data_filp);
        cts_int_data_filp = NULL;
        cts_err("Open file '%s' for test data failed %d",
            filepath, ret);
        return;
    }
#endif

    if (cts_int_data_filp)
        cts_dump_int_tsdata(header, sizeof(header));

    if (cts_int_data_filp) {
#ifndef CFG_CTS_FOR_GKI
        int r = filp_close(cts_int_data_filp, NULL);
        if (r) {
            cts_err("Close test data file failed %d", r);
        }
#endif
        cts_int_data_filp = NULL;
    } else {
        cts_warn("Stop dump tsdata to file with filp = NULL");
    }

    return;
}

static ssize_t dump_int_data_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);

    return scnprintf(buf, PAGE_SIZE, "Dumping: %s\n",
        cts_data->cts_dev.rtdata.dumping ? "Y" : "N");
}

static ssize_t dump_int_data_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
    u8 dumping = 0;
    int ret = 0;

    parse_arg(buf, count);

    if (argc != 2) {
        cts_err("Invalid num args != 2%d", argc);
        return -EFAULT;
    }

    ret = kstrtou8(argv[0], 0, &dumping);
    if (ret) {
        cts_err("Invalid int data dumping: %s", argv[0]);
        return -EINVAL;
    }
    if (!!dumping) {
        if (cts_int_data_filp) {
            cts_err("IS Dumpping ... , please stop dumping then restart.");
            return -EINVAL;
        }
    }

    ret = kstrtou32(argv[1], 0, &old_cnt);
    if (ret) {
        cts_err("Invalid int data dump cnt: %s", argv[1]);
        return -EINVAL;
    }

    if (!!dumping) {
        cts_data->cts_dev.rtdata.dump_cnt = old_cnt;
        cts_start_dump_int_data_to_file(chipone_bin_path, false);
    } else {
        cts_stop_dump_int_data_to_file();
        cts_redump_header_data_to_file(cts_data, chipone_bin_path, true);
    }
    cts_data->cts_dev.rtdata.dumping = !!dumping;

    return count;
}

static DEVICE_ATTR(dump_int_data, S_IWUSR | S_IRUGO,
        dump_int_data_show, dump_int_data_store);
#endif


static ssize_t cts_charger_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8  val;
	int ret;

	cts_info("Read sysfs '/%s'", attr->attr.name);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_charger_plug(cts_dev, &val);
	cts_unlock_device(cts_dev);

	if (ret) {
		cts_err("Get charger state failed %d(%s)",
			ret, cts_strerror(ret));
		return -1;
	}
	switch (val) {
		case 0:  return scnprintf(buf, PAGE_SIZE, "Detached\n");
		case 1:  return scnprintf(buf, PAGE_SIZE, "Attached\n");
		default: return scnprintf(buf, PAGE_SIZE, "Read error\n");
	}
}

static ssize_t cts_charger_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int state;
	int ret;

	cts_info("Write sysfs '/%s' size %zu", attr->attr.name, count);

	switch (buf[0]) {
		case '0': state = 0; break;
		case '1': state = 1; break;
		default:
			cts_err("Invalid arg for state");
			return -EINVAL;
	}

	cts_info("state = %d", state);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->set_charger_plug(cts_dev, state);
	cts_unlock_device(cts_dev);

	if (ret) {
		cts_err("Set charger state failed %d(%s)",
			ret, cts_strerror(ret));
		return -EIO;
	}

	return count;
}
static DEVICE_ATTR(charger_state, S_IRUGO | S_IWUSR,
		cts_charger_state_show, cts_charger_state_store);

static ssize_t cts_earjack_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8  val;
	int ret;

	cts_info("Read sysfs '/%s'", attr->attr.name);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_earjack_plug(cts_dev, &val);
	cts_unlock_device(cts_dev);

	if (ret) {
		cts_err("Get earjack state failed %d(%s)",
			ret, cts_strerror(ret));
		return -1;
	}
	switch (val) {
		case 0:  return scnprintf(buf, PAGE_SIZE, "Detached\n");
		case 1:  return scnprintf(buf, PAGE_SIZE, "Attached\n");
		default: return scnprintf(buf, PAGE_SIZE, "Read error\n");
	}
}

static ssize_t cts_earjack_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int state;
	int ret;

	cts_info("Write sysfs '/%s' size %zu", attr->attr.name, count);

	switch (buf[0]) {
		case '0': state = 0; break;
		case '1': state = 1; break;
		default:
			cts_err("Invalid arg for state");
			return -EINVAL;
	}

	cts_info("state = %d", state);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->set_earjack_plug(cts_dev, state);
	cts_unlock_device(cts_dev);

	if (ret) {
		cts_err("Set earjack state failed %d(%s)",
			ret, cts_strerror(ret));
		return -EIO;
	}

	return count;
}
static DEVICE_ATTR(earjack_state, S_IRUGO | S_IWUSR,
		cts_earjack_state_show, cts_earjack_state_store);

static ssize_t cts_edge_restain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 direction;
	int ret;

	cts_info("Read sysfs '/%s'", attr->attr.name);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_panel_direction(cts_dev, &direction);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("get panel direction failed!");
		return scnprintf(buf, PAGE_SIZE, "Read error\n");
	}

	return scnprintf(buf, PAGE_SIZE, "direction: 0x%02x\n", direction);
}

static ssize_t cts_edge_restain_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 direction;
	int ret;

	cts_info("Write sysfs '/%s' size %zu", attr->attr.name, count);

	switch (buf[0]) {
		case '0': direction = 0; break;//normal
		case '1': direction = 1; break;//notch left
		case '2': direction = 2; break;//notch right
		default:
			cts_err("Invalid arg for mode");
			return -EINVAL;
	}
	cts_info("direction = %d", direction);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->set_panel_direction(cts_dev, direction);
	cts_unlock_device(cts_dev);

	if (ret) {
		cts_err("Set edge restain failed");
		return -EIO;
	}

	return count;
}
static DEVICE_ATTR(edge_restain, S_IWUSR | S_IRUGO,
		cts_edge_restain_show, cts_edge_restain_store);


static ssize_t cts_game_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 enabled;
	int ret;

	cts_info("Read sysfs '/%s'", attr->attr.name);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_game_mode(cts_dev, &enabled);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("get game mode failed!");
		return scnprintf(buf, PAGE_SIZE, "Read error\n");
	}

	return scnprintf(buf, PAGE_SIZE, "game mode: 0x%02x\n", enabled);
}

static ssize_t cts_game_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int enable = -1;
	int ret;

	cts_info("Write sysfs '/%s' size %zu", attr->attr.name, count);

	if (count >= 1) {
		if (buf[0] == 'Y' || buf[0] == 'y' || buf[0] == '1') {
			enable = 1;
		} else if (buf[0] == 'N' || buf[0] == 'n' || buf[0] == '0') {
			enable = 0;
		}
	}
	if (enable == -1) {
		cts_err("Invalid arg for game mode enable");
		return -EINVAL;
	}

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->set_game_mode(cts_dev, enable);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("Set game mode failed");
		return -EIO;
	}

	return count;
}
static DEVICE_ATTR(game_mode,  S_IWUSR | S_IRUGO,
			cts_game_mode_show, cts_game_mode_store);

/*C3T code for HQ-229320 by jishen at 2022/10/20  start*/
static ssize_t cts_pocket_palm_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	u8 enabled;
	int ret;

	cts_info("Read sysfs '/%s'", attr->attr.name);

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->get_pocket_palm_mode(cts_dev, &enabled);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("get pocket palm mode failed!");
		return scnprintf(buf, PAGE_SIZE, "Read error\n");
	}

	return scnprintf(buf, PAGE_SIZE, "pocket palm mode: 0x%02x\n", enabled);
}

static ssize_t cts_pocket_palm_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int enable = -1;
	int ret;

	cts_info("Write sysfs '/%s' size %zu", attr->attr.name, count);

	if (count >= 1) {
		if (buf[0] == 'Y' || buf[0] == 'y' || buf[0] == '1') {
			enable = 1;
		} else if (buf[0] == 'N' || buf[0] == 'n' || buf[0] == '0') {
			enable = 0;
		}
	}
	if (enable == -1) {
		cts_err("Invalid arg for pocket palm mode enable");
		return -EINVAL;
	}

	cts_lock_device(cts_dev);
	ret = cts_dev->ops->set_pocket_palm_mode(cts_dev, enable);
	cts_unlock_device(cts_dev);
	if (ret) {
		cts_err("Set pocket palm mode failed");
		return -EIO;
	}

	return count;
}
static DEVICE_ATTR(pocket_palm_mode,  S_IWUSR | S_IRUGO,
			cts_pocket_palm_mode_show, cts_pocket_palm_mode_store);
/*C3T code for HQ-229320 by jishen at 2022/10/20  end*/

static struct attribute *cts_dev_misc_atts[] = {
	&dev_attr_ic_type.attr,
	&dev_attr_program_mode.attr,
	&dev_attr_rawdata.attr,
	&dev_attr_diffdata.attr,
	&dev_attr_manualdiff.attr,
	&dev_attr_jitter.attr,
#ifdef CFG_CTS_HAS_RESET_PIN
	&dev_attr_reset_pin.attr,
#endif
	&dev_attr_irq_pin.attr,
	&dev_attr_irq_info.attr,
#ifdef CFG_CTS_FW_LOG_REDIRECT
	&dev_attr_fw_log_redirect.attr,
#endif
	&dev_attr_compensate_cap.attr,
	&dev_attr_read_reg.attr,
	&dev_attr_write_reg.attr,
	&dev_attr_read_hw_reg.attr,
	&dev_attr_write_hw_reg.attr,
	&dev_attr_debug_spi.attr,
#ifdef CFG_CTS_GESTURE
	&dev_attr_gesture_en.attr,
#endif /* CFG_CTS_GESTURE */
	&dev_attr_int_data_types.attr,
	&dev_attr_int_data_method.attr,
#ifdef CFG_DUMP_INT_DATA
	&dev_attr_dump_int_data.attr,
#endif
	&dev_attr_charger_state.attr,
	&dev_attr_earjack_state.attr,
	&dev_attr_edge_restain.attr,
	&dev_attr_game_mode.attr,
	/*C3T code for HQ-229320 by jishen at 2022/10/20  start*/
	&dev_attr_pocket_palm_mode.attr,
	/*C3T code for HQ-229320 by jishen at 2022/10/20  end*/
	NULL
};

static const struct attribute_group cts_dev_misc_attr_group = {
	.name = "misc",
	.attrs = cts_dev_misc_atts,
};

#ifdef CFG_CTS_FW_UPDATE_SYS
static struct attribute *cts_dev_fw_up_atts[] = {
    /**
     * &dev_attr_drv_irq.attr,
     * &dev_attr_reset.attr,
     */
	&dev_attr_buildid.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_flashprog.attr,
	&dev_attr_doreflash.attr,
	&dev_attr_poweron.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_panel_supplier.attr,
	NULL
};

static const struct attribute_group cts_dev_fw_up_attr_group = {
	.attrs = cts_dev_fw_up_atts,
};
#endif

static const struct attribute_group *cts_dev_attr_groups[] = {
	&cts_dev_firmware_attr_group,
#ifdef CFG_CTS_FW_UPDATE_SYS
	&cts_dev_fw_up_attr_group,
#endif
	&cts_dev_flash_attr_group,
	&cts_dev_test_attr_group,
	&cts_dev_misc_attr_group,
	NULL
};

/*C3T code for HQ-244497 by jishen at 2022/10/01  start */
#ifdef CONFIG_CTS_TP_WORK_IRQ

/*C3T code for HQ-253995 by zhangkexin at 2022/10/08  start */
static ssize_t cts_tp_irq_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct irq_desc *desc = irq_to_desc(cts_data->pdata->irq);

	count = snprintf(buf, PAGE_SIZE, "irq_depth:%d\n", desc->depth);

	return count;
}
/*C3T code for HQ-253995 by zhangkexin at 2022/10/08  end */

static ssize_t cts_tp_irq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	int enable;
	int ret;

	cts_info("Write sysfs '/%s' size %zu", attr->attr.name, count);

	switch (buf[0]) {
		case '0': enable = 0; break;
		case '1': enable = 1; break;
		default:
			cts_err("Invalid arg for mode");
			return -EINVAL;
	}
	cts_info("tp irq enable = %d", enable);

	if (enable) {
		ret = cts_plat_enable_irq(cts_data->pdata);
		if (ret)
			cts_err("Enable IRQ failed %d", ret);
		else
			cts_info("Enable IRQ success");
	} else {
		ret = cts_plat_disable_irq(cts_data->pdata);
		if (ret)
			cts_err("Disable IRQ failed %d", ret);
		else
			cts_info("Disable IRQ success");
	}

	return count;
}
/*C3T code for HQ-253995 by zhangkexin at 2022/10/08  start */
static DEVICE_ATTR(cts_irq, S_IRUGO | S_IWUSR, cts_tp_irq_show, cts_tp_irq_store);
/*C3T code for HQ-253995 by zhangkexin at 2022/10/08  end */

static struct attribute *cts_dev_tp_irq_atts[] = {
	&dev_attr_cts_irq.attr,
	NULL
};

static const struct attribute_group cts_tp_irq_attr_group = {
	.attrs = cts_dev_tp_irq_atts,
};
#endif
/*C3T code for HQ-244497 by jishen at 2022/10/01  end */


#include <linux/major.h>
#include <linux/kdev_t.h>

/* Attribute: path (RO) */
static ssize_t path_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *data = dev_get_drvdata(dev);
	ssize_t blen;
	const char *path;

	if (!data) {
		cts_err("Read 'path' with chipone_ts_data NULL");
		return (ssize_t) 0;
	}
#ifdef CONFIG_CTS_I2C_HOST
	path = kobject_get_path(&data->i2c_client->dev.kobj, GFP_KERNEL);
#else
	path = kobject_get_path(&data->spi_client->dev.kobj, GFP_KERNEL);
#endif
	blen = scnprintf(buf, PAGE_SIZE, "%s", path ? path : "na");
	kfree(path);
	return blen;
}

/* Attribute: vendor (RO) */
static ssize_t vendor_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "chipone");
}

/* Attribute: vendor (RO) */
static ssize_t ic_ver_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct chipone_ts_data *ts = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s%s\n%s%04x\n%s%04x\n",
			 "Product ID: ", ts->cts_dev.hwdata->name,
			 "Build ID: ", ts->cts_dev.fwdata.version
			 ? ts->cts_dev.fwdata.version
			 : ts->pdata->build_id,
			 "Config ID: ", ts->cts_dev.fwdata.ddi_version
			 ? ts->cts_dev.fwdata.ddi_version
			 : ts->pdata->config_id);
}

static struct device_attribute touchscreen_attributes[] = {
	__ATTR_RO(path),
	__ATTR_RO(vendor),
	__ATTR_RO(ic_ver),
	__ATTR_NULL
};

#define TSDEV_MINOR_BASE 128
#define TSDEV_MINOR_MAX 32

/*******************************************************
 *Description:
 *    Chipone touchscreen FW function class. file node
 *    initial function.
 *
 * return:
 *    Executive outcomes. 0---succeed. -1---failed.
 *******************************************************/
static int cts_fw_class_init(void *_data, bool create)
{
	struct chipone_ts_data *data = _data;
	struct device_attribute *attrs = touchscreen_attributes;
	int i, error = 0;
	static struct class *touchscreen_class;
	static struct device *ts_class_dev;
	dev_t devno;

	cts_info("%s touchscreen class files", create ? "Add" : "Remove");

	if (create) {
		if (data->cts_dev.hwdata->name != NULL)
			error =
			    alloc_chrdev_region(&devno, 0, 1,
						data->cts_dev.hwdata->name);
		else
			error =
			    alloc_chrdev_region(&devno, 0, 1,
						CFG_CTS_CHIP_NAME);

		if (error) {
			cts_info("Alloc input devno failed %d", error);
			return error;
		}

		cts_info("Create class 'touchscreen'");
		touchscreen_class = class_create(THIS_MODULE, "touchscreen");
		if (IS_ERR(touchscreen_class)) {
			cts_err("Create class 'touchscreen' failed %ld",
				PTR_ERR(touchscreen_class));
			error = PTR_ERR(touchscreen_class);
			touchscreen_class = NULL;
			return error;
		}

		if (data->cts_dev.hwdata->name != NULL) {
			ts_class_dev = device_create(touchscreen_class, NULL,
						     devno, data, "%s",
						     data->cts_dev.hwdata->name);
			cts_info("Create device for IC: %s",
				 data->cts_dev.hwdata->name);
		} else {
			ts_class_dev = device_create(touchscreen_class, NULL,
						     devno, data, "%s",
						     CFG_CTS_CHIP_NAME);
			cts_info("Create device '" CFG_CTS_CHIP_NAME "'");
		}
		if (IS_ERR(ts_class_dev)) {
			cts_err("Create device '" CFG_CTS_CHIP_NAME
				"'failed %ld", PTR_ERR(ts_class_dev));
			error = PTR_ERR(ts_class_dev);
			ts_class_dev = NULL;
			return error;
		}

		cts_info("Create attr files");
		for (i = 0; attrs[i].attr.name != NULL; ++i) {
			cts_info("  Create attr file '%s'", attrs[i].attr.name);
			error = device_create_file(ts_class_dev, &attrs[i]);
			if (error) {
				cts_err("Create attr file '%s' failed %d",
					attrs[i].attr.name, error);
				break;
			}
		}

		if (error)
			goto device_destroy;
		else
			cts_info("Create /sys/class/touchscreen/ Succeeded");
	} else {
		if (!touchscreen_class || !ts_class_dev)
			return -ENODEV;

		for (i = 0; attrs[i].attr.name != NULL; ++i) {
			cts_info("Remove device file '%s'", attrs[i].attr.name);
			device_remove_file(ts_class_dev, &attrs[i]);
		}
		device_unregister(ts_class_dev);
		class_unregister(touchscreen_class);
	}

	return 0;

device_destroy:
	for (--i; i >= 0; --i)
		device_remove_file(ts_class_dev, &attrs[i]);
	device_destroy(touchscreen_class, devno);
	ts_class_dev = NULL;
	class_unregister(touchscreen_class);
	cts_err("Creating touchscreen class failed %d", error);

	return -ENODEV;
}

int cts_sysfs_add_device(struct device *dev)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	struct cts_device *cts_dev = &cts_data->cts_dev;
	int ret, i;

	cts_info("Add device attr groups");

	/*Low version kernel NOT support sysfs_create_groups() */
	for (i = 0; cts_dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, cts_dev_attr_groups[i]);
		if (ret) {
			while (--i >= 0)
				sysfs_remove_group(&dev->kobj,
						   cts_dev_attr_groups[i]);
			break;
		}
	}

/*C3T code for HQ-244497 by jishen at 2022/10/01  start */
#ifdef CONFIG_CTS_TP_WORK_IRQ
	ret = sysfs_create_group(&dev->kobj, &cts_tp_irq_attr_group);
	if (ret) {
		cts_err("Create sysfs group '%s' failed %d(%s)",
			"cts_irq", ret, cts_strerror(ret));
		sysfs_remove_group(&dev->kobj, &cts_tp_irq_attr_group);
	}
#endif
/*C3T code for HQ-244497 by jishen at 2022/10/01  end */

	if (ret) {
		cts_err("Add device attr failed %d", ret);
		return ret;
	}

	if (DIFFDATA_BUFFER_SIZE(cts_dev) > 0) {
		manualdiff_base =
		    (s16 *) kzalloc(DIFFDATA_BUFFER_SIZE(cts_dev), GFP_KERNEL);
		if (manualdiff_base == NULL) {
			cts_err("Malloc manualdiff_base failed");
			return -ENOMEM;
		}
	}

	ret = sysfs_create_link(NULL, &dev->kobj, "chipone-tddi");
	if (ret)
		cts_err("Create sysfs link error:%d", ret);

	ret = cts_fw_class_init(cts_data, true);
	if (ret) {
		cts_err("Create touchscreen class failed. ret=%d\n", ret);
		return ret;
	}

	return 0;
}

void cts_sysfs_remove_device(struct device *dev)
{
	struct chipone_ts_data *cts_data = dev_get_drvdata(dev);
	int i;

	cts_info("Remove device attr groups");

	if (manualdiff_base != NULL) {
		kfree(manualdiff_base);
		manualdiff_base = NULL;
	}

	sysfs_remove_link(NULL, "chipone-tddi");
	/*Low version kernel NOT support sysfs_remove_groups() */
	for (i = 0; cts_dev_attr_groups[i]; i++)
		sysfs_remove_group(&dev->kobj, cts_dev_attr_groups[i]);
/*C3T code for HQ-244497 by jishen at 2022/10/01  start */
	sysfs_remove_group(&dev->kobj, &cts_tp_irq_attr_group);
/*C3T code for HQ-244497 by jishen at 2022/10/01  end */
	cts_fw_class_init(cts_data, false);
}

#undef SPLIT_LINE_STR
#undef ROW_NUM_FORMAT_STR
#undef COL_NUM_FORMAT_STR
#undef DATA_FORMAT_STR

#endif /* CONFIG_CTS_SYSFS */
