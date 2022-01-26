/*
 * Copyright (C) 2016 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/iio/iio.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>
#include <linux/platform_data/nanohub.h>

#include "main.h"
#include "comms.h"
#include "bl.h"
#include "nanohub-mtk.h"

#define READ_QUEUE_DEPTH	10
#define APP_FROM_HOST_EVENTID	0x000000F8
#define FIRST_SENSOR_EVENTID	0x00000200
#define LAST_SENSOR_EVENTID	0x000002FF
#define APP_TO_HOST_EVENTID	0x00000401
#define OS_LOG_EVENTID		0x3B474F4C
#define WAKEUP_INTERRUPT	1
#define WAKEUP_TIMEOUT_MS	1000
#define SUSPEND_TIMEOUT_MS	100

struct nanohub_data *g_nanohub_data_p;

/**
 * struct gpio_config - this is a binding between platform data and driver data
 * @label:     for diagnostics
 * @flags:     to pass to gpio_request_one()
 * @options:   one or more of GPIO_OPT_* flags, below
 * @pdata_off: offset of u32 field in platform data with gpio #
 * @data_off:  offset of int field in driver data with irq # (optional)
 */
struct gpio_config {
	const char *label;
	u16 flags;
	u16 options;
	u16 pdata_off;
	u16 data_off;
};

#define GPIO_OPT_HAS_IRQ	0x0001
#define GPIO_OPT_OPTIONAL	0x8000

#define PLAT_GPIO_DEF(name, _flags) \
	.pdata_off = offsetof(struct nanohub_platform_data, name ## _gpio), \
	.label = "nanohub_" #name, \
	.flags = _flags

#define PLAT_GPIO_DEF_IRQ(name, _opts) \
	.data_off = offsetof(struct nanohub_data, name), \
	.options = GPIO_OPT_HAS_IRQ | (_opts)

static struct class *sensor_class;
static int major;

static const struct gpio_config gconf[] = {
	{ PLAT_GPIO_DEF(nreset, GPIOF_OUT_INIT_HIGH) },
	{ PLAT_GPIO_DEF(wakeup, GPIOF_OUT_INIT_HIGH) },
	{ PLAT_GPIO_DEF(boot0, GPIOF_OUT_INIT_LOW) },
	{ PLAT_GPIO_DEF(irq1, GPIOF_DIR_IN),
	  PLAT_GPIO_DEF_IRQ(irq1, 0)
	},
	{ PLAT_GPIO_DEF(irq2, GPIOF_DIR_IN),
	  PLAT_GPIO_DEF_IRQ(irq2, GPIO_OPT_OPTIONAL)
	},
};

static const struct iio_info nanohub_iio_info = {
	.driver_module = THIS_MODULE,
};

static const struct file_operations nanohub_fileops = {
	.owner = THIS_MODULE,
};

enum {
	ST_IDLE,
	ST_ERROR,
	ST_RUNNING
};

static inline bool gpio_is_optional(const struct gpio_config *_cfg)
{
	return _cfg->options & GPIO_OPT_OPTIONAL;
}

static inline bool gpio_has_irq(const struct gpio_config *_cfg)
{
	return _cfg->options & GPIO_OPT_HAS_IRQ;
}

static inline bool
nanohub_has_priority_lock_locked(struct nanohub_data *data)
{
	return  atomic_read(&data->wakeup_lock_cnt) >
		atomic_read(&data->wakeup_cnt);
}

static inline void nanohub_notify_thread(struct nanohub_data *data)
{
	atomic_set(&data->kthread_run, 1);
	/* wake_up implementation works as memory barrier */
	wake_up_interruptible_sync(&data->kthread_wait);
}

static inline void nanohub_io_init(struct nanohub_io *io,
				   struct nanohub_data *data,
				   struct device *dev)
{
	init_waitqueue_head(&io->buf_wait);
	INIT_LIST_HEAD(&io->buf_list);
	io->data = data;
	io->dev = dev;
}

static inline bool nanohub_io_has_buf(struct nanohub_io *io)
{
	return !list_empty(&io->buf_list);
}

static struct nanohub_buf *nanohub_io_get_buf(struct nanohub_io *io,
					      bool wait)
{
	struct nanohub_buf *buf = NULL;
	int ret;

	spin_lock(&io->buf_wait.lock);
	if (wait) {
		ret = wait_event_interruptible_locked(io->buf_wait,
						      nanohub_io_has_buf(io));
		if (ret < 0) {
			spin_unlock(&io->buf_wait.lock);
			return ERR_PTR(ret);
		}
	}

	if (nanohub_io_has_buf(io)) {
		buf = list_first_entry(&io->buf_list, struct nanohub_buf, list);
		list_del(&buf->list);
	}
	spin_unlock(&io->buf_wait.lock);

	return buf;
}

static void nanohub_io_put_buf(struct nanohub_io *io,
			       struct nanohub_buf *buf)
{
	bool was_empty;

	spin_lock(&io->buf_wait.lock);
	was_empty = !nanohub_io_has_buf(io);
	list_add_tail(&buf->list, &io->buf_list);
	spin_unlock(&io->buf_wait.lock);

	if (was_empty) {
		if (&io->data->free_pool == io)
			nanohub_notify_thread(io->data);
		else
			wake_up_interruptible(&io->buf_wait);
	}
}

static inline bool mcu_wakeup_try_lock(struct nanohub_data *data, int key)
{
	/* implementation contains memory barrier */
	return atomic_cmpxchg(&data->wakeup_acquired, 0, key) == 0;
}

static inline void mcu_wakeup_unlock(struct nanohub_data *data, int key)
{
	WARN(atomic_cmpxchg(&data->wakeup_acquired, key, 0) != key,
	     "%s: failed to unlock with key %d; current state: %d",
	     __func__, key, atomic_read(&data->wakeup_acquired));
}

static inline void nanohub_set_state(struct nanohub_data *data, int state)
{
	atomic_set(&data->thread_state, state);
	smp_mb__after_atomic(); /* updated thread state is now visible */
}

static inline int nanohub_get_state(struct nanohub_data *data)
{
	smp_mb__before_atomic(); /* wait for all updates to finish */
	return atomic_read(&data->thread_state);
}

int request_wakeup_ex(struct nanohub_data *data, long timeout_ms,
		      int key, int lock_mode)
{
	return 0;
}

void release_wakeup_ex(struct nanohub_data *data, int key, int lock_mode)
{
}

int nanohub_wait_for_interrupt(struct nanohub_data *data)
{
	return 0;
}

int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat)
{
	return 0;
}

static void __nanohub_interrupt_cfg(struct nanohub_data *data,
				    u8 interrupt, bool mask)
{
	int ret;
	u8 mask_ret = 0;
	int cnt = 10;
	struct device *dev = data->io[ID_NANOHUB_SENSOR].dev;
	int cmd = mask ? CMD_COMMS_MASK_INTR : CMD_COMMS_UNMASK_INTR;

	do {
		ret = request_wakeup_timeout(data, WAKEUP_TIMEOUT_MS);
		if (ret) {
			dev_err(dev,
				"%s: interrupt %d %smask failed: ret=%d\n",
				__func__, interrupt, mask ? "" : "un", ret);
			return;
		}

		ret =
		    nanohub_comms_tx_rx_retrans(data, cmd,
						&interrupt, sizeof(interrupt),
						&mask_ret, sizeof(mask_ret),
						false, 10, 0);
		release_wakeup(data);
		dev_dbg(dev,
			"%smasking interrupt %d, ret=%d, mask_ret=%d\n",
			mask ? "" : "un",
			interrupt, ret, mask_ret);
	} while ((ret != 1 || mask_ret != 1) && --cnt > 0);
}

static inline void nanohub_mask_interrupt(struct nanohub_data *data,
					  u8 interrupt)
{
	__nanohub_interrupt_cfg(data, interrupt, true);
}

static inline void nanohub_unmask_interrupt(struct nanohub_data *data,
					    u8 interrupt)
{
	__nanohub_interrupt_cfg(data, interrupt, false);
}

static ssize_t nanohub_wakeup_query(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	const struct nanohub_platform_data *pdata = data->pdata;

	data->err_cnt = 0;
	if (nanohub_irq1_fired(data) || nanohub_irq2_fired(data))
		wake_up_interruptible(&data->wakeup_wait);

	return scnprintf(buf, PAGE_SIZE, "WAKEUP: %d INT1: %d INT2: %d\n",
			 gpio_get_value(pdata->wakeup_gpio),
			 gpio_get_value(pdata->irq1_gpio),
			 data->irq2 ? gpio_get_value(pdata->irq2_gpio) : -1);
}

static ssize_t nanohub_app_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	struct {
		u64 appid;
		u32 appver;
		u32 appsize;
	} __packed buffer;
	u32 i = 0;
	int ret;
	ssize_t len = 0;

	do {
		if (request_wakeup(data))
			return -ERESTARTSYS;

		if (nanohub_comms_tx_rx_retrans
		    (data, CMD_COMMS_QUERY_APP_INFO, (u8 *)&i,
		     sizeof(i), (u8 *)&buffer, sizeof(buffer),
		     false, 10, 10) == sizeof(buffer)) {
			ret =
			    scnprintf(buf + len, PAGE_SIZE - len,
				      "app %d id:%016llx ver:%08x size:%08x\n",
				      i, buffer.appid, buffer.appver,
				      buffer.appsize);
			if (ret > 0) {
				len += ret;
				i++;
			}
		} else {
			ret = -1;
		}

		release_wakeup(data);
	} while (ret > 0);

	return len;
}

static ssize_t nanohub_firmware_query(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	u16 buffer[6];

	if (request_wakeup(data))
		return -ERESTARTSYS;

	if (nanohub_comms_tx_rx_retrans
	    (data, CMD_COMMS_GET_OS_HW_VERSIONS, NULL, 0, (u8 *)&buffer,
	     sizeof(buffer), false, 10, 10) == sizeof(buffer)) {
		release_wakeup(data);
		return scnprintf(buf, PAGE_SIZE,
			"hw type: %04x hw ver: %04x bl ver: %04x os ver: %04x variant ver: %08x\n",
			buffer[0], buffer[1], buffer[2], buffer[3],
			buffer[5] << 16 | buffer[4]);
	} else {
		release_wakeup(data);
		return 0;
	}
}

static inline int nanohub_wakeup_lock(struct nanohub_data *data,
				      int mode)
{
	return 0;
}

/* returns lock mode used to perform this lock */
static inline int nanohub_wakeup_unlock(struct nanohub_data *data)
{
	int mode = atomic_read(&data->lock_mode);

	atomic_set(&data->lock_mode, LOCK_MODE_NONE);
	if (mode != LOCK_MODE_SUSPEND_RESUME)
		enable_irq(data->irq1);
	if (mode == LOCK_MODE_IO)
		nanohub_bl_close(data);
	if (data->irq2)
		enable_irq(data->irq2);
	release_wakeup_ex(data, KEY_WAKEUP_LOCK, mode);
	if (!data->irq2)
		nanohub_unmask_interrupt(data, 2);
	nanohub_notify_thread(data);

	return mode;
}

/*
 *static void __nanohub_hw_reset(struct nanohub_data *data, int boot0)
 *{
 *	const struct nanohub_platform_data *pdata = data->pdata;
 *
 *	gpio_set_value(pdata->nreset_gpio, 0);
 *	gpio_set_value(pdata->boot0_gpio, boot0 ? 1 : 0);
 *	usleep_range(30, 40);
 *	gpio_set_value(pdata->nreset_gpio, 1);
 *	if (boot0)
 *		usleep_range(70000, 75000);
 *	else
 *		usleep_range(750000, 800000);
 *}
 */
static ssize_t nanohub_hw_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return -EIO;
/*
 *	struct nanohub_data *data = dev_get_nanohub_data(dev);
 *	int ret;
 *
 *	ret = nanohub_wakeup_lock(data, LOCK_MODE_RESET);
 *	if (!ret) {
 *		data->err_cnt = 0;
 *		__nanohub_hw_reset(data, 0);
 *		nanohub_wakeup_unlock(data);
 *	}
 *
 *	return ret < 0 ? ret : count;
 */
}

static ssize_t nanohub_erase_shared(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	return -EIO;
/*
 *	struct nanohub_data *data = dev_get_nanohub_data(dev);
 *	u8 status = CMD_ACK;
 *	int ret;
 *
 *	ret = nanohub_wakeup_lock(data, LOCK_MODE_IO);
 *	if (ret < 0)
 *		return ret;
 *
 *	data->err_cnt = 0;
 *	__nanohub_hw_reset(data, 1);
 *
 *	status = nanohub_bl_erase_shared(data);
 *	dev_info(dev, "nanohub_bl_erase_shared: status=%02x\n",
 *		 status);
 *
 *	__nanohub_hw_reset(data, 0);
 *	nanohub_wakeup_unlock(data);
 *
 *	return ret < 0 ? ret : count;
 */
}

static ssize_t nanohub_download_bl(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return -EIO;
/*
 *	struct nanohub_data *data = dev_get_nanohub_data(dev);
 *	const struct nanohub_platform_data *pdata = data->pdata;
 *	const struct firmware *fw_entry;
 *	int ret;
 *	u8 status = CMD_ACK;
 *
 *	ret = nanohub_wakeup_lock(data, LOCK_MODE_IO);
 *	if (ret < 0)
 *		return ret;
 *
 *	data->err_cnt = 0;
 *	__nanohub_hw_reset(data, 1);
 *
 *	ret = request_firmware(&fw_entry, "nanohub.full.bin", dev);
 *	if (ret) {
 *		dev_err(dev, "%s: err=%d\n", __func__, ret);
 *	} else {
 *		status = nanohub_bl_download(data, pdata->bl_addr,
 *					     fw_entry->data, fw_entry->size);
 *		dev_info(dev, "%s: status=%02x\n", __func__, status);
 *		release_firmware(fw_entry);
 *	}
 *
 *	__nanohub_hw_reset(data, 0);
 *	nanohub_wakeup_unlock(data);
 *
 *	return ret < 0 ? ret : count;
 */
}

static ssize_t nanohub_download_kernel(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	return -EIO;
/*
 *	struct nanohub_data *data = dev_get_nanohub_data(dev);
 *	const struct firmware *fw_entry;
 *	int ret;
 *
 *	ret = request_firmware(&fw_entry, "nanohub.update.bin", dev);
 *	if (ret) {
 *		dev_err(dev, "nanohub_download_kernel: err=%d\n", ret);
 *		return -EIO;
 *	}
 *	ret = nanohub_comms_kernel_download(data, fw_entry->data,
 *					    fw_entry->size);
 *
 *	release_firmware(fw_entry);
 *
 *	return count;
 */
}

static ssize_t nanohub_download_app(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	return -EIO;
/*
 *	struct nanohub_data *data = dev_get_nanohub_data(dev);
 *	const struct firmware *fw_entry;
 *	char buffer[70];
 *	int i, ret, ret1, ret2, file_len = 0, appid_len = 0, ver_len = 0;
 *	const char *appid = NULL, *ver = NULL;
 *	unsigned long version;
 *	u64 id;
 *	u32 cur_version;
 *	bool update = true;
 *
 *	for (i = 0; i < count; i++) {
 *		if (buf[i] == ' ') {
 *			if (i + 1 == count)
 *				break;
 *			if (!appid)
 *				appid = buf + i + 1;
 *			else if (!ver)
 *				ver = buf + i + 1;
 *			else
 *				break;
 *		} else if (buf[i] == '\n' || buf[i] == '\r') {
 *			break;
 *		}
 *		if (ver)
 *			ver_len++;
 *		else if (appid)
 *			appid_len++;
 *		else
 *			file_len++;
 *	}
 *
 *	if (file_len > 64 || appid_len > 16 || ver_len > 8 || file_len < 1)
 *		return -EIO;
 *
 *	memcpy(buffer, buf, file_len);
 *	memcpy(buffer + file_len, ".napp", 5);
 *	buffer[file_len + 5] = '\0';
 *
 *	ret = request_firmware(&fw_entry, buffer, dev);
 *	if (ret) {
 *		dev_err(dev, "nanohub_download_app(%s): err=%d\n",
 *			buffer, ret);
 *		return -EIO;
 *	}
 *	if (appid_len > 0 && ver_len > 0) {
 *		memcpy(buffer, appid, appid_len);
 *		buffer[appid_len] = '\0';
 *
 *		ret1 = kstrtoull(buffer, 16, &id);
 *
 *		memcpy(buffer, ver, ver_len);
 *		buffer[ver_len] = '\0';
 *
 *		ret2 = kstrtoul(buffer, 16, &version);
 *
 *		if (ret1 == 0 && ret2 == 0) {
 *			if (request_wakeup(data))
 *				return -ERESTARTSYS;
 *			if (nanohub_comms_tx_rx_retrans
 *			    (data, CMD_COMMS_GET_APP_VERSIONS,
 *			     (u8 *)&id, sizeof(id),
 *			     (u8 *)&cur_version,
 *			     sizeof(cur_version), false, 10,
 *			     10) == sizeof(cur_version)) {
 *				if (cur_version == version)
 *					update = false;
 *			}
 *			release_wakeup(data);
 *		}
 *	}
 *
 *	if (update)
 *		ret =
 *		    nanohub_comms_app_download(data, fw_entry->data,
 *					       fw_entry->size);
 *
 *	release_firmware(fw_entry);
 *
 *	return count;
 */
}

static struct device_attribute attributes[] = {
	__ATTR(wakeup, 0440, nanohub_wakeup_query, NULL),
	__ATTR(app_info, 0440, nanohub_app_info, NULL),
	__ATTR(firmware_version, 0440, nanohub_firmware_query, NULL),
	__ATTR(download_bl, 0220, NULL, nanohub_download_bl),
	__ATTR(download_kernel, 0220, NULL, nanohub_download_kernel),
	__ATTR(download_app, 0220, NULL, nanohub_download_app),
	__ATTR(erase_shared, 0220, NULL, nanohub_erase_shared),
	__ATTR(reset, 0220, NULL, nanohub_hw_reset),
};

static inline int nanohub_create_sensor(struct nanohub_data *data)
{
	int i, ret;
	struct device *sensor_dev = data->io[ID_NANOHUB_SENSOR].dev;

	for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(sensor_dev, &attributes[i]);
		if (ret) {
			dev_err(sensor_dev,
				"create sysfs attr %d [%s] failed; err=%d\n",
				i, attributes[i].attr.name, ret);
			goto fail_attr;
		}
	}

	ret = sysfs_create_link(&sensor_dev->kobj,
				&data->iio_dev->dev.kobj, "iio");
	if (ret) {
		dev_err(sensor_dev,
			"sysfs_create_link failed; err=%d\n", ret);
		goto fail_attr;
	}
	goto done;

fail_attr:
	for (i--; i >= 0; i--)
		device_remove_file(sensor_dev, &attributes[i]);
done:
	return ret;
}

static int nanohub_create_devices(struct nanohub_data *data)
{
	int i, ret;
	static const char *names[ID_NANOHUB_MAX] = {
			"nanohub", "nanohub_comms"
	};

	for (i = 0; i < ID_NANOHUB_MAX; ++i) {
		struct nanohub_io *io = &data->io[i];

		nanohub_io_init(io, data, device_create(sensor_class, NULL,
							MKDEV(major, i),
							io, names[i]));
		if (IS_ERR(io->dev)) {
			ret = PTR_ERR(io->dev);
			pr_err("nanohub: device_create failed for %s; err=%d\n",
			       names[i], ret);
			goto fail_dev;
		}
	}

	ret = nanohub_create_sensor(data);
	if (!ret)
		goto done;
fail_dev:
	for (--i; i >= 0; --i)
		device_destroy(sensor_class, MKDEV(major, i));
done:
	return ret;
}

ssize_t nanohub_external_write(const char *buffer, size_t length)
{
	struct nanohub_data *data = g_nanohub_data_p;
	int ret;
	u8 ret_data;

	if (request_wakeup(data))
		return -ERESTARTSYS;

	if (nanohub_comms_tx_rx_retrans
		(data, CMD_COMMS_WRITE, buffer, length, &ret_data,
		sizeof(ret_data), false,
		10, 10) == sizeof(ret_data)) {
		if (ret_data)
			ret = length;
		else
			ret = 0;
	} else {
		ret = ERROR_NACK;
	}

	release_wakeup(data);

	return ret;
}

static bool nanohub_os_log(char *buffer, int len)
{
	if (le32_to_cpu((((u32 *)buffer)[0]) & 0x7FFFFFFF) ==
	    OS_LOG_EVENTID) {
		char *mtype, *mdata = &buffer[5];

		buffer[len - 1] = '\0';

		switch (buffer[4]) {
		case 'E':
			mtype = KERN_ERR;
			break;
		case 'W':
			mtype = KERN_WARNING;
			break;
		case 'I':
			mtype = KERN_INFO;
			break;
		case 'D':
			mtype = KERN_DEBUG;
			break;
		default:
			mtype = KERN_DEFAULT;
			mdata--;
			break;
		}
		pr_debug("%snanohub: %s", mtype, mdata);
		return true;
	} else {
		return false;
	}
}

static void nanohub_process_buffer(struct nanohub_data *data,
				   struct nanohub_buf **buf,
				   int ret)
{
	u32 event_id;
	u8 interrupt;
	bool wakeup = false;
	struct nanohub_io *io = &data->io[ID_NANOHUB_SENSOR];

	data->err_cnt = 0;
	if (ret < 4 || nanohub_os_log((*buf)->buffer, ret)) {
		release_wakeup(data);
		return;
	}

	(*buf)->length = ret;

	event_id = le32_to_cpu((((u32 *)(*buf)->buffer)[0]) & 0x7FFFFFFF);
	if (ret >= sizeof(u32) + sizeof(u64) + sizeof(u32) &&
	    event_id > FIRST_SENSOR_EVENTID &&
	    event_id <= LAST_SENSOR_EVENTID) {
		interrupt = (*buf)->buffer[sizeof(u32) +
					   sizeof(u64) + 3];
		if (interrupt == WAKEUP_INTERRUPT)
			wakeup = true;
	}
	if (event_id == APP_TO_HOST_EVENTID) {
		wakeup = true;
		io = &data->io[ID_NANOHUB_COMMS];
	}

	nanohub_io_put_buf(io, *buf);

	*buf = NULL;
	/* (for wakeup interrupts): hold a wake lock for 10ms so the sensor hal
	 * has time to grab its own wake lock
	 */
	if (wakeup)
		__pm_wakeup_event(&data->ws, 10);
	release_wakeup(data);
}

static int nanohub_kthread(void *arg)
{
	struct nanohub_data *data = (struct nanohub_data *)arg;
	struct nanohub_buf *buf = NULL;
	int ret;
	u32 clear_interrupts[8] = { 0x00000006 };
	struct device *sensor_dev = data->io[ID_NANOHUB_SENSOR].dev;
	static const struct sched_param param = {
		.sched_priority = (MAX_USER_RT_PRIO / 2) - 1,
	};

	data->err_cnt = 0;
	sched_setscheduler(current, SCHED_FIFO, &param);
	nanohub_set_state(data, ST_IDLE);

	while (!kthread_should_stop()) {
		switch (nanohub_get_state(data)) {
		case ST_IDLE:
			if (wait_event_interruptible(
					data->kthread_wait,
					atomic_read(&data->kthread_run)))
				continue;
			nanohub_set_state(data, ST_RUNNING);
			break;
		case ST_ERROR:
			msleep_interruptible(WAKEUP_TIMEOUT_MS);
			nanohub_set_state(data, ST_RUNNING);
			break;
		case ST_RUNNING:
			break;
		}
		atomic_set(&data->kthread_run, 0);
		if (!buf)
			buf = nanohub_io_get_buf(&data->free_pool,
						 false);
		if (buf) {
			ret = request_wakeup_timeout(data, WAKEUP_TIMEOUT_MS);
			if (ret) {
				dev_info(sensor_dev,
					 "%s: request_wakeup_timeout: ret=%d\n",
					 __func__, ret);
				continue;
			}

			ret = nanohub_comms_rx_retrans_boottime(
			    data, CMD_COMMS_READ, buf->buffer,
			    sizeof(buf->buffer), 10, 0);
			if (ret > 0) {
				nanohub_process_buffer(data, &buf, ret);
				if (!nanohub_irq1_fired(data) &&
				    !nanohub_irq2_fired(data)) {
					nanohub_set_state(data, ST_IDLE);
					continue;
				}
			} else if (ret == 0) {
				/* queue empty, go to sleep */
				data->err_cnt = 0;
				data->interrupts[0] &= ~0x00000006;
				release_wakeup(data);
				nanohub_set_state(data, ST_IDLE);
				continue;
			} else {
				release_wakeup(data);
				if (++data->err_cnt >= 10) {
					dev_err(sensor_dev,
						"%s: err_cnt=%d\n",
						__func__,
						data->err_cnt);
					nanohub_set_state(data, ST_ERROR);
					continue;
				}
			}
		} else {
			if (!nanohub_irq1_fired(data) &&
			    !nanohub_irq2_fired(data)) {
				nanohub_set_state(data, ST_IDLE);
				continue;
			}
			/* pending interrupt, but no room to read data -
			 * clear interrupts
			 */
			if (request_wakeup(data))
				continue;
			nanohub_comms_tx_rx_retrans(data,
						    CMD_COMMS_CLR_GET_INTR,
						    (u8 *)clear_interrupts,
						    sizeof(clear_interrupts),
						    (u8 *)data->interrupts,
						    sizeof(data->interrupts),
						    false, 10, 0);
			release_wakeup(data);
			nanohub_set_state(data, ST_IDLE);
		}
	}

	return 0;
}

struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev)
{
	int ret, i;
	/* const struct nanohub_platform_data *pdata;*/
	struct nanohub_data *data;
	struct nanohub_buf *buf;
	bool own_iio_dev = !iio_dev;

	if (own_iio_dev) {
		iio_dev = iio_device_alloc(sizeof(struct nanohub_data));
		if (!iio_dev)
			return ERR_PTR(-ENOMEM);
	}
	iio_dev->name = "nanohub";
	iio_dev->dev.parent = dev;
	iio_dev->info = &nanohub_iio_info;
	iio_dev->channels = NULL;
	iio_dev->num_channels = 0;
	data = iio_priv(iio_dev);
	g_nanohub_data_p = data;
	data->iio_dev = iio_dev;
	/* data->pdata = pdata; */
	data->pdata = devm_kzalloc(dev, sizeof(struct nanohub_platform_data),
				   GFP_KERNEL);
	init_waitqueue_head(&data->kthread_wait);

	nanohub_io_init(&data->free_pool, data, dev);

	buf = vmalloc(sizeof(*buf) * READ_QUEUE_DEPTH);
	data->vbuf = buf;
	if (!buf) {
		ret = -ENOMEM;
		goto fail_vma;
	}

	for (i = 0; i < READ_QUEUE_DEPTH; i++)
		nanohub_io_put_buf(&data->free_pool, &buf[i]);
	atomic_set(&data->kthread_run, 0);

	wakeup_source_init(&data->ws, "nanohub_wakelock_read");

	atomic_set(&data->lock_mode, LOCK_MODE_NONE);
	atomic_set(&data->wakeup_cnt, 0);
	atomic_set(&data->wakeup_lock_cnt, 0);
	atomic_set(&data->wakeup_acquired, 0);
	init_waitqueue_head(&data->wakeup_wait);
	ret = iio_device_register(iio_dev);
	if (ret) {
		pr_err("nanohub: iio_device_register failed\n");
		goto fail_irq;
	}
	ret = nanohub_create_devices(data);
	if (ret)
		goto fail_dev;
	data->thread = kthread_run(nanohub_kthread, data, "nanohub");

	usleep_range(25, 30);

	return iio_dev;
fail_dev:
	iio_device_unregister(iio_dev);

fail_irq:
	wakeup_source_trash(&data->ws);
	vfree(buf);
fail_vma:
	if (own_iio_dev)
		iio_device_free(iio_dev);

	return ERR_PTR(ret);
}

int nanohub_reset(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	gpio_set_value(pdata->nreset_gpio, 1);
	usleep_range(650000, 700000);
	enable_irq(data->irq1);
	if (data->irq2)
		enable_irq(data->irq2);
	else
		nanohub_unmask_interrupt(data, 2);

	return 0;
}

int nanohub_suspend(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);

	nanohub_mask_interrupt(data, 2);
	return 0;
}

int nanohub_resume(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);

	nanohub_unmask_interrupt(data, 2);
	return 0;
}

static int __init nanohub_init(void)
{
	int ret = 0;

	sensor_class = class_create(THIS_MODULE, "nanohub");
	if (IS_ERR(sensor_class)) {
		ret = PTR_ERR(sensor_class);
		pr_err("nanohub: class_create failed; err=%d\n", ret);
	}
	if (!ret)
		major = __register_chrdev(0, 0, ID_NANOHUB_MAX, "nanohub",
					  &nanohub_fileops);

	if (major < 0) {
		ret = major;
		major = 0;
		pr_err("nanohub: can't register; err=%d\n", ret);
	}

#ifdef CONFIG_NANOHUB_I2C
	if (ret == 0)
		ret = nanohub_i2c_init();
#endif
#ifdef CONFIG_NANOHUB_SPI
	if (ret == 0)
		ret = nanohub_spi_init();
#endif
#ifdef CONFIG_NANOHUB_MTK_IPI
		ret = nanohub_ipi_init();
#endif
	pr_info("nanohub: loaded; ret=%d\n", ret);
	return ret;
}

static void __exit nanohub_cleanup(void)
{
#ifdef CONFIG_NANOHUB_I2C
	nanohub_i2c_cleanup();
#endif
#ifdef CONFIG_NANOHUB_SPI
	nanohub_spi_cleanup();
#endif
	__unregister_chrdev(major, 0, ID_NANOHUB_MAX, "nanohub");
	class_destroy(sensor_class);
	major = 0;
	sensor_class = 0;
}

module_init(nanohub_init);
module_exit(nanohub_cleanup);

MODULE_AUTHOR("Ben Fennema");
MODULE_LICENSE("GPL");
