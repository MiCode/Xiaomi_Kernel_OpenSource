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
	.flags = _flags \

#define PLAT_GPIO_DEF_IRQ(name, _flags, _opts) \
	PLAT_GPIO_DEF(name, _flags), \
	.data_off = offsetof(struct nanohub_data, name), \
	.options = GPIO_OPT_HAS_IRQ | (_opts) \

static int nanohub_open(struct inode *, struct file *);
static ssize_t nanohub_read(struct file *, char *, size_t, loff_t *);
static ssize_t nanohub_write(struct file *, const char *, size_t, loff_t *);
static unsigned int nanohub_poll(struct file *, poll_table *);
static int nanohub_release(struct inode *, struct file *);

static struct class *sensor_class;
static int major;

static const struct gpio_config gconf[] = {
	{ PLAT_GPIO_DEF(nreset, GPIOF_OUT_INIT_HIGH) },
	{ PLAT_GPIO_DEF(wakeup, GPIOF_OUT_INIT_HIGH) },
	{ PLAT_GPIO_DEF(boot0, GPIOF_OUT_INIT_LOW) },
	{ PLAT_GPIO_DEF_IRQ(irq1, GPIOF_DIR_IN, 0) },
	{ PLAT_GPIO_DEF_IRQ(irq2, GPIOF_DIR_IN, GPIO_OPT_OPTIONAL) },
};

static const struct iio_info nanohub_iio_info = {
	.driver_module = THIS_MODULE,
};

static const struct file_operations nanohub_fileops = {
	.owner = THIS_MODULE,
	.open = nanohub_open,
	.read = nanohub_read,
	.write = nanohub_write,
	.poll = nanohub_poll,
	.release = nanohub_release,
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

static inline bool nanohub_has_priority_lock_locked(struct nanohub_data *data)
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
#if 0
static inline int plat_gpio_get(struct nanohub_data *data,
				const struct gpio_config *_cfg)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	return *(u32 *)(((char *)pdata) + (_cfg)->pdata_off);
}

static inline void nanohub_set_irq_data(struct nanohub_data *data,
					const struct gpio_config *_cfg, int val)
{
	int *data_addr = ((int *)(((char *)data) + _cfg->data_off));

	if ((void *)data_addr > (void *)data &&
	    (void *)data_addr < (void *)(data + 1))
		*data_addr = val;
	else
		WARN(1, "No data binding defined for %s", _cfg->label);
}

static inline void mcu_wakeup_gpio_set_value(struct nanohub_data *data,
					     int val)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	gpio_set_value(pdata->wakeup_gpio, val);
}

static inline void mcu_wakeup_gpio_get_locked(struct nanohub_data *data,
					      int priority_lock)
{
	atomic_inc(&data->wakeup_lock_cnt);
	if (!priority_lock && atomic_inc_return(&data->wakeup_cnt) == 1 &&
	    !nanohub_has_priority_lock_locked(data))
		mcu_wakeup_gpio_set_value(data, 0);
}

static inline bool mcu_wakeup_gpio_put_locked(struct nanohub_data *data,
					      int priority_lock)
{
	bool gpio_done = priority_lock ?
			 atomic_read(&data->wakeup_cnt) == 0 :
			 atomic_dec_and_test(&data->wakeup_cnt);
	bool done = atomic_dec_and_test(&data->wakeup_lock_cnt);

	if (!nanohub_has_priority_lock_locked(data))
		mcu_wakeup_gpio_set_value(data, gpio_done ? 1 : 0);

	return done;
}

static inline bool mcu_wakeup_gpio_is_locked(struct nanohub_data *data)
{
	return atomic_read(&data->wakeup_lock_cnt) != 0;
}

static inline void nanohub_handle_irq1(struct nanohub_data *data)
{
	bool locked;

	spin_lock(&data->wakeup_wait.lock);
	locked = mcu_wakeup_gpio_is_locked(data);
	spin_unlock(&data->wakeup_wait.lock);
	if (!locked)
		nanohub_notify_thread(data);
	else
		wake_up_interruptible_sync(&data->wakeup_wait);
}

static inline void nanohub_handle_irq2(struct nanohub_data *data)
{
	nanohub_notify_thread(data);
}
#endif

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

/* the following fragment is based on wait_event_* code from wait.h */
#define wait_event_interruptible_timeout_locked(q, cond, tmo)		\
({									\
	long __ret = (tmo);						\
	DEFINE_WAIT(__wait);						\
	if (!(cond)) {							\
		for (;;) {						\
			__wait.flags &= ~WQ_FLAG_EXCLUSIVE;		\
			if (list_empty(&__wait.task_list))		\
				__add_wait_queue_tail(&(q), &__wait);	\
			set_current_state(TASK_INTERRUPTIBLE);		\
			if ((cond))					\
				break;					\
			if (signal_pending(current)) {			\
				__ret = -ERESTARTSYS;			\
				break;					\
			}						\
			spin_unlock(&(q).lock);				\
			__ret = schedule_timeout(__ret);		\
			spin_lock(&(q).lock);				\
			if (!__ret) {					\
				if ((cond))				\
					__ret = 1;			\
				break;					\
			}						\
		}							\
		__set_current_state(TASK_RUNNING);			\
		if (!list_empty(&__wait.task_list))			\
			list_del_init(&__wait.task_list);		\
		else if (__ret == -ERESTARTSYS &&			\
			 /*reimplementation of wait_abort_exclusive() */\
			 waitqueue_active(&(q)))			\
			__wake_up_locked_key(&(q), TASK_INTERRUPTIBLE,	\
			NULL);						\
	} else {							\
		__ret = 1;						\
	}								\
	__ret;								\
})									\


int request_wakeup_ex(struct nanohub_data *data, long timeout_ms,
		      int key, int lock_mode)
{
#if 0
	long timeout;
	bool priority_lock = lock_mode > LOCK_MODE_NORMAL;

	spin_lock(&data->wakeup_wait.lock);
	mcu_wakeup_gpio_get_locked(data, priority_lock);
	timeout = (timeout_ms != MAX_SCHEDULE_TIMEOUT) ?
		   msecs_to_jiffies(timeout_ms) :
		   MAX_SCHEDULE_TIMEOUT;

	timeout = wait_event_interruptible_timeout_locked(
			data->wakeup_wait,
			((priority_lock || nanohub_irq1_fired(data)) &&
			 mcu_wakeup_try_lock(data, key)),
			timeout
		  );

	if (timeout <= 0) {
		mcu_wakeup_gpio_put_locked(data, priority_lock);

		if (timeout == 0)
			timeout = -ETIME;
	} else {
		timeout = 0;
	}
	spin_unlock(&data->wakeup_wait.lock);

	return timeout;
#else
	return 0;
#endif
}

void release_wakeup_ex(struct nanohub_data *data, int key, int lock_mode)
{
#if 0
	bool done;
	bool priority_lock = lock_mode > LOCK_MODE_NORMAL;

	spin_lock(&data->wakeup_wait.lock);
	done = mcu_wakeup_gpio_put_locked(data, priority_lock);
	mcu_wakeup_unlock(data, key);
	spin_unlock(&data->wakeup_wait.lock);

	if (!done)
		wake_up_interruptible_sync(&data->wakeup_wait);
	else if (nanohub_irq1_fired(data) || nanohub_irq2_fired(data))
		nanohub_notify_thread(data);
#endif
}

int nanohub_wait_for_interrupt(struct nanohub_data *data)
{
#if 0
	int ret = -EFAULT;

	/* release the wakeup line, and wait for nanohub to send
	 * us an interrupt indicating the transaction completed.
	 */
	spin_lock(&data->wakeup_wait.lock);
	if (mcu_wakeup_gpio_is_locked(data)) {
		mcu_wakeup_gpio_set_value(data, 1);
		ret = wait_event_interruptible_locked(data->wakeup_wait,
						      nanohub_irq1_fired(data));
		mcu_wakeup_gpio_set_value(data, 0);
	}
	spin_unlock(&data->wakeup_wait.lock);

	return ret;
#else
	return 0;
#endif
}

int nanohub_wakeup_eom(struct nanohub_data *data, bool repeat)
{
#if 0
	int ret = -EFAULT;

	spin_lock(&data->wakeup_wait.lock);
	if (mcu_wakeup_gpio_is_locked(data)) {
		mcu_wakeup_gpio_set_value(data, 1);
		if (repeat)
			mcu_wakeup_gpio_set_value(data, 0);
		ret = 0;
	}
	spin_unlock(&data->wakeup_wait.lock);

	return ret;
#else
	return 0;
#endif
}

static void __nanohub_interrupt_cfg(struct nanohub_data *data,
				    u8 interrupt, bool mask)
{
	int ret;
	uint8_t mask_ret;
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
		uint64_t appId;
		uint32_t appVer;
		uint32_t appSize;
	} __packed buffer;
	uint32_t i = 0;
	int ret;
	ssize_t len = 0;

	do {
		if (request_wakeup(data))
			return -ERESTARTSYS;

		if (nanohub_comms_tx_rx_retrans
		    (data, CMD_COMMS_QUERY_APP_INFO, (uint8_t *)&i,
		     sizeof(i), (u8 *)&buffer, sizeof(buffer),
		     false, 10, 10) == sizeof(buffer)) {
			ret =
			    scnprintf(buf + len, PAGE_SIZE - len,
				      "app: %d id: %016llx ver: %08x size: %08x\n",
				      i, buffer.appId, buffer.appVer,
				      buffer.appSize);
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
	uint16_t buffer[6];

	if (request_wakeup(data))
		return -ERESTARTSYS;

	if (nanohub_comms_tx_rx_retrans
	    (data, CMD_COMMS_GET_OS_HW_VERSIONS, NULL, 0, (uint8_t *)&buffer,
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

static inline int nanohub_wakeup_lock(struct nanohub_data *data, int mode)
{
#if 0
	int ret;

	if (data->irq2)
		disable_irq(data->irq2);
	else
		nanohub_mask_interrupt(data, 2);

	ret = request_wakeup_ex(data,
				mode == LOCK_MODE_SUSPEND_RESUME ?
				SUSPEND_TIMEOUT_MS : WAKEUP_TIMEOUT_MS,
				KEY_WAKEUP_LOCK, mode);
	if (ret < 0) {
		if (data->irq2)
			enable_irq(data->irq2);
		else
			nanohub_unmask_interrupt(data, 2);
		return ret;
	}

	if (mode == LOCK_MODE_IO)
		ret = nanohub_bl_open(data);
	if (ret < 0) {
		release_wakeup_ex(data, KEY_WAKEUP_LOCK, mode);
		return ret;
	}
	if (mode != LOCK_MODE_SUSPEND_RESUME)
		disable_irq(data->irq1);

	atomic_set(&data->lock_mode, mode);
	mcu_wakeup_gpio_set_value(data, 1);
#endif
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

static void __nanohub_hw_reset(struct nanohub_data *data, int boot0)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	gpio_set_value(pdata->nreset_gpio, 0);
	gpio_set_value(pdata->boot0_gpio, boot0 ? 1 : 0);
	usleep_range(30, 40);
	gpio_set_value(pdata->nreset_gpio, 1);
	if (boot0)
		usleep_range(70000, 75000);
	else
		usleep_range(750000, 800000);
}

static ssize_t nanohub_hw_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	int ret;

	ret = nanohub_wakeup_lock(data, LOCK_MODE_RESET);
	if (!ret) {
		data->err_cnt = 0;
		__nanohub_hw_reset(data, 0);
		nanohub_wakeup_unlock(data);
	}

	return ret < 0 ? ret : count;
}

static ssize_t nanohub_erase_shared(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	uint8_t status = CMD_ACK;
	int ret;

	ret = nanohub_wakeup_lock(data, LOCK_MODE_IO);
	if (ret < 0)
		return ret;

	data->err_cnt = 0;
	__nanohub_hw_reset(data, 1);

	status = nanohub_bl_erase_shared(data);
	dev_info(dev, "nanohub_bl_erase_shared: status=%02x\n",
		 status);

	__nanohub_hw_reset(data, 0);
	nanohub_wakeup_unlock(data);

	return ret < 0 ? ret : count;
}

static ssize_t nanohub_download_bl(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	const struct nanohub_platform_data *pdata = data->pdata;
	const struct firmware *fw_entry;
	int ret;
	uint8_t status = CMD_ACK;

	ret = nanohub_wakeup_lock(data, LOCK_MODE_IO);
	if (ret < 0)
		return ret;

	data->err_cnt = 0;
	__nanohub_hw_reset(data, 1);

	ret = request_firmware(&fw_entry, "nanohub.full.bin", dev);
	if (ret) {
		dev_err(dev, "%s: err=%d\n", __func__, ret);
	} else {
		status = nanohub_bl_download(data, pdata->bl_addr,
					     fw_entry->data, fw_entry->size);
		dev_info(dev, "%s: status=%02x\n", __func__, status);
		release_firmware(fw_entry);
	}

	__nanohub_hw_reset(data, 0);
	nanohub_wakeup_unlock(data);

	return ret < 0 ? ret : count;
}

static ssize_t nanohub_download_kernel(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	const struct firmware *fw_entry;
	int ret;

	ret = request_firmware(&fw_entry, "nanohub.update.bin", dev);
	if (ret) {
		dev_err(dev, "nanohub_download_kernel: err=%d\n", ret);
		return -EIO;
	}
	ret = nanohub_comms_kernel_download(data, fw_entry->data,
					fw_entry->size);

	release_firmware(fw_entry);

	return count;

}

static ssize_t nanohub_download_app(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct nanohub_data *data = dev_get_nanohub_data(dev);
	const struct firmware *fw_entry;
	char buffer[70];
	int i, ret, ret1, ret2, file_len = 0, appid_len = 0, ver_len = 0;
	const char *appid = NULL, *ver = NULL;
	unsigned long version;
	uint64_t id;
	uint32_t cur_version;
	bool update = true;

	for (i = 0; i < count; i++) {
		if (buf[i] == ' ') {
			if (i + 1 == count)
				break;
			if (appid == NULL)
				appid = buf + i + 1;
			else if (ver == NULL)
				ver = buf + i + 1;
			else
				break;
		} else if (buf[i] == '\n' || buf[i] == '\r') {
			break;
		}
		if (ver)
			ver_len++;
		else if (appid)
			appid_len++;
		else
			file_len++;
	}

	if (file_len > 64 || appid_len > 16 || ver_len > 8 || file_len < 1)
		return -EIO;

	memcpy(buffer, buf, file_len);
	memcpy(buffer + file_len, ".napp", 5);
	buffer[file_len + 5] = '\0';

	ret = request_firmware(&fw_entry, buffer, dev);
	if (ret) {
		dev_err(dev, "nanohub_download_app(%s): err=%d\n",
			buffer, ret);
		return -EIO;
	}
	if (appid_len > 0 && ver_len > 0) {
		memcpy(buffer, appid, appid_len);
		buffer[appid_len] = '\0';

		ret1 = kstrtoull(buffer, 16, &id);

		memcpy(buffer, ver, ver_len);
		buffer[ver_len] = '\0';

		ret2 = kstrtoul(buffer, 16, &version);

		if (ret1 == 0 && ret2 == 0) {
			if (request_wakeup(data))
				return -ERESTARTSYS;
			if (nanohub_comms_tx_rx_retrans
			    (data, CMD_COMMS_GET_APP_VERSIONS,
			     (uint8_t *)&id, sizeof(id),
			     (uint8_t *)&cur_version,
			     sizeof(cur_version), false, 10,
			     10) == sizeof(cur_version)) {
				if (cur_version == version)
					update = false;
			}
			release_wakeup(data);
		}
	}

	if (update)
		ret =
		    nanohub_comms_app_download(data, fw_entry->data,
					       fw_entry->size);

	release_firmware(fw_entry);

	return count;
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

static int nanohub_match_devt(struct device *dev, const void *data)
{
	const dev_t *devt = data;

	return dev->devt == *devt;
}

static int nanohub_open(struct inode *inode, struct file *file)
{
	dev_t devt = inode->i_rdev;
	struct device *dev;

	dev = class_find_device(sensor_class, NULL, &devt, nanohub_match_devt);
	if (dev) {
		file->private_data = dev_get_drvdata(dev);
		nonseekable_open(inode, file);
		return 0;
	}

	return -ENODEV;
}

static ssize_t nanohub_read(struct file *file, char *buffer, size_t length,
			    loff_t *offset)
{
	struct nanohub_io *io = file->private_data;
	struct nanohub_data *data = io->data;
	struct nanohub_buf *buf;
	int ret;

	if (!nanohub_io_has_buf(io) && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	buf = nanohub_io_get_buf(io, true);
	if (IS_ERR_OR_NULL(buf))
		return PTR_ERR(buf);

	ret = copy_to_user(buffer, buf->buffer, buf->length);
	if (ret != 0)
		ret = -EFAULT;
	else
		ret = buf->length;

	nanohub_io_put_buf(&data->free_pool, buf);

	return ret;
}

static ssize_t nanohub_write(struct file *file, const char *buffer,
			     size_t length, loff_t *offset)
{
	struct nanohub_io *io = file->private_data;
	struct nanohub_data *data = io->data;
	int ret;

	ret = request_wakeup_timeout(data, WAKEUP_TIMEOUT_MS);
	if (ret)
		return ret;

	ret = nanohub_comms_write(data, buffer, length);

	release_wakeup(data);

	return ret;
}

ssize_t nanohub_external_write(const char *buffer, size_t length)
{
	struct nanohub_data *data = g_nanohub_data_p;
	int ret;
	uint8_t ret_data;

	if (request_wakeup(data))
		return -ERESTARTSYS;

	if (nanohub_comms_tx_rx_retrans
		(data, CMD_COMMS_WRITE, buffer, length, &ret_data, sizeof(ret_data), false,
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

static unsigned int nanohub_poll(struct file *file, poll_table *wait)
{
	struct nanohub_io *io = file->private_data;
	unsigned int mask = POLLOUT | POLLWRNORM;

	poll_wait(file, &io->buf_wait, wait);

	if (nanohub_io_has_buf(io))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int nanohub_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}
#if 0
static void nanohub_destroy_devices(struct nanohub_data *data)
{
	int i;
	struct device *sensor_dev = data->io[ID_NANOHUB_SENSOR].dev;

	sysfs_remove_link(&sensor_dev->kobj, "iio");
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(sensor_dev, &attributes[i]);
	for (i = 0; i < ID_NANOHUB_MAX; ++i)
		device_destroy(sensor_class, MKDEV(major, i));
}

static irqreturn_t nanohub_irq1(int irq, void *dev_id)
{
	struct nanohub_data *data = (struct nanohub_data *)dev_id;

	nanohub_handle_irq1(data);

	return IRQ_HANDLED;
}

static irqreturn_t nanohub_irq2(int irq, void *dev_id)
{
	struct nanohub_data *data = (struct nanohub_data *)dev_id;

	nanohub_handle_irq2(data);

	return IRQ_HANDLED;
}
#endif
static bool nanohub_os_log(char *buffer, int len)
{
	if (le32_to_cpu((((uint32_t *)buffer)[0]) & 0x7FFFFFFF) ==
	    OS_LOG_EVENTID) {
		char *mtype, *mdata = &buffer[5];

		buffer[len] = 0x00;

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
	uint32_t event_id;
	uint8_t interrupt;
	bool wakeup = false;
	struct nanohub_io *io = &data->io[ID_NANOHUB_SENSOR];

	data->err_cnt = 0;
	if (ret < 4 || nanohub_os_log((*buf)->buffer, ret)) {
		release_wakeup(data);
		return;
	}

	(*buf)->length = ret;

	event_id = le32_to_cpu((((uint32_t *)(*buf)->buffer)[0]) & 0x7FFFFFFF);
	if (ret >= sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) &&
	    event_id > FIRST_SENSOR_EVENTID &&
	    event_id <= LAST_SENSOR_EVENTID) {
		interrupt = (*buf)->buffer[sizeof(uint32_t) +
					   sizeof(uint64_t) + 3];
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
	 * has time to grab its own wake lock */
	if (wakeup)
		wake_lock_timeout(&data->wakelock_read, msecs_to_jiffies(10));
	release_wakeup(data);
}

static int nanohub_kthread(void *arg)
{
	struct nanohub_data *data = (struct nanohub_data *)arg;
	struct nanohub_buf *buf = NULL;
	int ret;
	uint32_t clear_interrupts[8] = { 0x00000006 };
	struct device *sensor_dev = data->io[ID_NANOHUB_SENSOR].dev;
	static const struct sched_param param = {
		.sched_priority = (MAX_USER_RT_PRIO/2)-1,
	};

	data->err_cnt = 0;
	sched_setscheduler(current, SCHED_FIFO, &param);
	nanohub_set_state(data, ST_IDLE);

	while (!kthread_should_stop()) {
		switch (nanohub_get_state(data)) {
		case ST_IDLE:
			wait_event_interruptible(data->kthread_wait,
						 atomic_read(&data->kthread_run)
						 );
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
			 * clear interrupts */
			if (request_wakeup(data))
				continue;
			nanohub_comms_tx_rx_retrans(data,
						    CMD_COMMS_CLR_GET_INTR,
						    (uint8_t *)
						    clear_interrupts,
						    sizeof(clear_interrupts),
						    (uint8_t *) data->
						    interrupts,
						    sizeof(data->interrupts),
						    false, 10, 0);
			release_wakeup(data);
			nanohub_set_state(data, ST_IDLE);
		}
	}

	return 0;
}
#if 0
#ifdef CONFIG_OF
static struct nanohub_platform_data *nanohub_parse_dt(struct device *dev)
{
	struct nanohub_platform_data *pdata;
	struct device_node *dt = dev->of_node;
	const uint32_t *tmp;
	struct property *prop;
	uint32_t u, i;
	int ret;

	if (!dt)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	ret = pdata->irq1_gpio =
	    of_get_named_gpio(dt, "sensorhub,irq1-gpio", 0);
	if (ret < 0) {
		pr_err("nanohub: missing sensorhub,irq1-gpio in device tree\n");
		goto free_pdata;
	}

	/* optional (strongly recommended) */
	pdata->irq2_gpio = of_get_named_gpio(dt, "sensorhub,irq2-gpio", 0);

	ret = pdata->wakeup_gpio =
	    of_get_named_gpio(dt, "sensorhub,wakeup-gpio", 0);
	if (ret < 0) {
		pr_err
		    ("nanohub: missing sensorhub,wakeup-gpio in device tree\n");
		goto free_pdata;
	}

	ret = pdata->nreset_gpio =
	    of_get_named_gpio(dt, "sensorhub,nreset-gpio", 0);
	if (ret < 0) {
		pr_err
		    ("nanohub: missing sensorhub,nreset-gpio in device tree\n");
		goto free_pdata;
	}

	/* optional (stm32f bootloader) */
	pdata->boot0_gpio = of_get_named_gpio(dt, "sensorhub,boot0-gpio", 0);

	/* optional (spi) */
	pdata->spi_cs_gpio = of_get_named_gpio(dt, "sensorhub,spi-cs-gpio", 0);

	/* optional (stm32f bootloader) */
	of_property_read_u32(dt, "sensorhub,bl-addr", &pdata->bl_addr);

	/* optional (stm32f bootloader) */
	tmp = of_get_property(dt, "sensorhub,num-flash-banks", NULL);
	if (tmp) {
		pdata->num_flash_banks = be32_to_cpup(tmp);
		pdata->flash_banks =
		    devm_kzalloc(dev,
				 sizeof(struct nanohub_flash_bank) *
				 pdata->num_flash_banks, GFP_KERNEL);
		if (!pdata->flash_banks)
			goto no_mem;

		/* TODO: investigate replacing with of_property_read_u32_array
		 */
		i = 0;
		of_property_for_each_u32(dt, "sensorhub,flash-banks", prop, tmp,
					 u) {
			if (i / 3 >= pdata->num_flash_banks)
				break;
			switch (i % 3) {
			case 0:
				pdata->flash_banks[i / 3].bank = u;
				break;
			case 1:
				pdata->flash_banks[i / 3].address = u;
				break;
			case 2:
				pdata->flash_banks[i / 3].length = u;
				break;
			}
			i++;
		}
	}

	/* optional (stm32f bootloader) */
	tmp = of_get_property(dt, "sensorhub,num-shared-flash-banks", NULL);
	if (tmp) {
		pdata->num_shared_flash_banks = be32_to_cpup(tmp);
		pdata->shared_flash_banks =
		    devm_kzalloc(dev,
				 sizeof(struct nanohub_flash_bank) *
				 pdata->num_shared_flash_banks, GFP_KERNEL);
		if (!pdata->shared_flash_banks)
			goto no_mem_shared;

		/* TODO: investigate replacing with of_property_read_u32_array
		 */
		i = 0;
		of_property_for_each_u32(dt, "sensorhub,shared-flash-banks",
					 prop, tmp, u) {
			if (i / 3 >= pdata->num_shared_flash_banks)
				break;
			switch (i % 3) {
			case 0:
				pdata->shared_flash_banks[i / 3].bank = u;
				break;
			case 1:
				pdata->shared_flash_banks[i / 3].address = u;
				break;
			case 2:
				pdata->shared_flash_banks[i / 3].length = u;
				break;
			}
			i++;
		}
	}

	return pdata;

no_mem_shared:
	devm_kfree(dev, pdata->flash_banks);
no_mem:
	ret = -ENOMEM;
free_pdata:
	devm_kfree(dev, pdata);
	return ERR_PTR(ret);
}
#else
static struct nanohub_platform_data *nanohub_parse_dt(struct device *dev)
{
	struct nanohub_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	return pdata;
}
#endif
#endif
#if 0
static int nanohub_request_irqs(struct nanohub_data *data)
{
	int ret;

	ret = request_threaded_irq(data->irq1, NULL, nanohub_irq1,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "nanohub-irq1", data);
	if (ret < 0)
		data->irq1 = 0;
	else
		disable_irq(data->irq1);
	if (data->irq2 <= 0 || ret < 0) {
		data->irq2 = 0;
		return ret;
	}

	ret = request_threaded_irq(data->irq2, NULL, nanohub_irq2,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "nanohub-irq2", data);
	if (ret < 0) {
		data->irq2 = 0;
		WARN(1, "failed to request optional IRQ %d; err=%d",
		     data->irq2, ret);
	} else {
		disable_irq(data->irq2);
	}

	/* if 2d request fails, hide this; it is optional IRQ,
	 * and failure should not interrupt driver init sequence.
	 */
	return 0;
}

static int nanohub_request_gpios(struct nanohub_data *data)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(gconf); ++i) {
		const struct gpio_config *cfg = &gconf[i];
		unsigned int gpio = plat_gpio_get(data, cfg);
		const char *label;
		bool optional = gpio_is_optional(cfg);

		ret = 0; /* clear errors on optional pins, if any */

		if (!gpio_is_valid(gpio) && optional)
			continue;

		label = cfg->label;
		ret = gpio_request_one(gpio, cfg->flags, label);
		if (ret && !optional) {
			pr_err("nanohub: gpio %d[%s] request failed;err=%d\n",
			       gpio, label, ret);
			break;
		}
		if (gpio_has_irq(cfg)) {
			int irq = gpio_to_irq(gpio);

			if (irq > 0) {
				nanohub_set_irq_data(data, cfg, irq);
			} else if (!optional) {
				ret = -EINVAL;
				pr_err("nanohub: no irq; gpio %d[%s];err=%d\n",
				       gpio, label, irq);
				break;
			}
		}
	}
	if (i < ARRAY_SIZE(gconf)) {
		for (--i; i >= 0; --i)
			gpio_free(plat_gpio_get(data, &gconf[i]));
	}

	return ret;
}

static void nanohub_release_gpios_irqs(struct nanohub_data *data)
{
	const struct nanohub_platform_data *pdata = data->pdata;

	if (data->irq2)
		free_irq(data->irq2, data);
	if (data->irq1)
		free_irq(data->irq1, data);
	if (gpio_is_valid(pdata->irq2_gpio))
		gpio_free(pdata->irq2_gpio);
	gpio_free(pdata->irq1_gpio);
	gpio_set_value(pdata->nreset_gpio, 0);
	gpio_free(pdata->nreset_gpio);
	mcu_wakeup_gpio_set_value(data, 1);
	gpio_free(pdata->wakeup_gpio);
	gpio_set_value(pdata->boot0_gpio, 0);
	gpio_free(pdata->boot0_gpio);
}
#endif
struct iio_dev *nanohub_probe(struct device *dev, struct iio_dev *iio_dev)
{
	int ret, i;
	/* const struct nanohub_platform_data *pdata;*/
	struct nanohub_data *data;
	struct nanohub_buf *buf;
	bool own_iio_dev = !iio_dev;
#if 0	/* add device tree support later */
	pdata = dev_get_platdata(dev);
	if (!pdata) {
		pdata = nanohub_parse_dt(dev);
		if (IS_ERR(pdata))
			return ERR_PTR(PTR_ERR(pdata));
	}
#endif
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
	wake_lock_init(&data->wakelock_read, WAKE_LOCK_SUSPEND,
		       "nanohub_wakelock_read");

	atomic_set(&data->lock_mode, LOCK_MODE_NONE);
	atomic_set(&data->wakeup_cnt, 0);
	atomic_set(&data->wakeup_lock_cnt, 0);
	atomic_set(&data->wakeup_acquired, 0);
	init_waitqueue_head(&data->wakeup_wait);
#if 0
	ret = nanohub_request_gpios(data);
	if (ret)
		goto fail_gpio;

	ret = nanohub_request_irqs(data);
	if (ret)
		goto fail_irq;
#endif
	ret = iio_device_register(iio_dev);
	if (ret) {
		pr_err("nanohub: iio_device_register failed\n");
		goto fail_irq;
	}
	ret = nanohub_create_devices(data);
	if (ret)
		goto fail_dev;
	data->thread = kthread_run(nanohub_kthread, data, "nanohub");

	udelay(30);

	return iio_dev;
fail_dev:
	iio_device_unregister(iio_dev);

fail_irq:
#if 0
	nanohub_release_gpios_irqs(data);
fail_gpio:
#endif
	wake_lock_destroy(&data->wakelock_read);
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
#if 0
int nanohub_remove(struct iio_dev *iio_dev)
{
	struct nanohub_data *data = iio_priv(iio_dev);

	nanohub_notify_thread(data);
	kthread_stop(data->thread);

	nanohub_destroy_devices(data);
	iio_device_unregister(iio_dev);
	nanohub_release_gpios_irqs(data);
	wake_lock_destroy(&data->wakelock_read);
	vfree(data->vbuf);
	iio_device_free(iio_dev);

	return 0;
}
#endif
int nanohub_suspend(struct iio_dev *iio_dev)
{
#if 0 /* MTK: add suspend setting here */
	struct nanohub_data *data = iio_priv(iio_dev);
	int ret;

	ret = nanohub_wakeup_lock(data, LOCK_MODE_SUSPEND_RESUME);
	if (!ret) {
		int cnt;
		const int max_cnt = 10;

		for (cnt = 0; cnt < max_cnt; ++cnt) {
			if (!nanohub_irq1_fired(data))
				break;
			usleep_range(10, 15);
		}
		if (cnt < max_cnt) {
			dev_dbg(&iio_dev->dev, "%s: cnt=%d\n", __func__, cnt);
			enable_irq_wake(data->irq1);
			return 0;
		}
		ret = -EBUSY;
		dev_info(&iio_dev->dev,
			 "%s: failed to suspend: IRQ1=%d, state=%d\n",
			 __func__, nanohub_irq1_fired(data),
			 nanohub_get_state(data));
		nanohub_wakeup_unlock(data);
	} else {
		dev_info(&iio_dev->dev, "%s: could not take wakeup lock\n",
			 __func__);
	}

	return ret;
#else
	return 0;
#endif
}

int nanohub_resume(struct iio_dev *iio_dev)
{
#if 0 /* MTK: add suspend setting here */
	struct nanohub_data *data = iio_priv(iio_dev);

	disable_irq_wake(data->irq1);
	nanohub_wakeup_unlock(data);
#endif
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
