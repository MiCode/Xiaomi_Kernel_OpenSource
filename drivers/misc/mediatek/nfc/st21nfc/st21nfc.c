// SPDX-License-Identifier: GPL-2.0-only
/*
 * NFC Controller Driver
 * Copyright (C) 2020 ST Microelectronics S.A.
 * Copyright (C) 2010 Stollmann E+V GmbH
 * Copyright (C) 2010 Trusted Logic S.A.
 */

#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#ifndef LEGACY
#include <linux/workqueue.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <net/nfc/nci.h>
#include <linux/clk.h>
#else
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/of_irq.h>
#include "st21nfc.h"

// Comment this out to remove the check of CLF response during probe
#define WITH_PING_DURING_PROBE


// Kernel 4.9 on some platforms is using legacy drivers (kernel-4.9-lc)
// I2C: CONFIG_MACH_MT6735 / 6735M / 6753 / 6580 / 6755 use legacy driver
// CLOCK: 4.9 has right includes, no need for special handling.
// GPIO : same as I2C -- we use the same condition at the moment.
//#if (defined(CONFIG_MACH_MT6735) || defined(CONFIG_MACH_MT6735M) ||
//    defined(CONFIG_MACH_MT6753) || defined(CONFIG_MACH_MT6580) ||
//	defined(CONFIG_MACH_MT6755))
// test on I2C special define instead of listing the platforms
#ifdef CONFIG_MTK_I2C_EXTENSION
#define KRNMTKLEGACY_I2C 1
#define KRNMTKLEGACY_GPIO 1
#define KRNMTKLEGACY_CLK 1
#endif

/* Set NO_MTK_CLK_MANAGEMENT if using xtal integration */
#ifndef NO_MTK_CLK_MANAGEMENT
#ifdef KRNMTKLEGACY_CLK
#include <mt_clkbuf_ctl.h>
#else
#include <mtk-clkbuf-bridge.h>
#endif
#endif

#define MAX_BUFFER_SIZE 260
#define HEADER_LENGTH 3
#define IDLE_CHARACTER 0x7e
#define ST21NFC_POWER_STATE_MAX 3
// wake up for the duration of a typical transaction
#define WAKEUP_SRC_TIMEOUT (500)

#define DRIVER_VERSION "2.2.0.17"

#define PROP_PWR_MON_RW_ON_NTF nci_opcode_pack(NCI_GID_PROPRIETARY, 5)
#define PROP_PWR_MON_RW_OFF_NTF nci_opcode_pack(NCI_GID_PROPRIETARY, 6)

#define I2C_ID_NAME "st21nfc"

#ifdef KRNMTKLEGACY_I2C
#include <linux/dma-mapping.h>
#define NFC_CLIENT_TIMING 400 /* I2C speed */
static char *I2CDMAWriteBuf; /*= NULL;*/ /* unnecessary initialise */
static unsigned int I2CDMAWriteBuf_pa; /* = NULL; */
static char *I2CDMAReadBuf; /*= NULL;*/ /* unnecessary initialise */
static unsigned int I2CDMAReadBuf_pa; /* = NULL; */
#endif /* KRNMTKLEGACY_I2C */

static bool enable_debug_log;

/*The enum is used to index a pw_states array, the values matter here*/
enum st21nfc_power_state {
	ST21NFC_IDLE = 0,
	ST21NFC_ACTIVE = 1,
	ST21NFC_ACTIVE_RW = 2
};

static const char *const st21nfc_power_state_name[] = {

	"IDLE", "ACTIVE", "ACTIVE_RW"
};

enum st21nfc_read_state { ST21NFC_HEADER, ST21NFC_PAYLOAD };

struct nfc_sub_power_stats {
	uint64_t count;
	uint64_t duration;
	uint64_t last_entry;
	uint64_t last_exit;
};

struct nfc_sub_power_stats_error {
	/* error transition header --> payload state machine */
	uint64_t header_payload;
	/* error transition from an active state when not in idle state */
	uint64_t active_not_idle;
	/* error transition from idle state to idle state */
	uint64_t idle_to_idle;
	/* warning transition from active_rw state to idle state */
	uint64_t active_rw_to_idle;
	/* error transition from active state to active state */
	uint64_t active_to_active;
	/* error transition from idle state to active state with notification */
	uint64_t idle_to_active_ntf;
	/* error transition from active_rw state to active_rw state */
	uint64_t act_rw_to_act_rw;
	/* error transition from idle state to */
	/* active_rw state with notification   */
	uint64_t idle_to_active_rw_ntf;
};

/*
 * The member 'polarity_mode' defines
 * how the wakeup pin is configured and handled.
 * it can take the following values :
 * IRQF_TRIGGER_RISING
 * IRQF_TRIGGER_HIGH
 */
struct st21nfc_device {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct mutex pidle_mutex;
	struct mutex irq_dir_mutex;
	struct i2c_client *client;
	struct miscdevice st21nfc_device;
	uint8_t buffer[MAX_BUFFER_SIZE];
	bool irq_enabled;
	bool irq_wake_up;
	struct wakeup_source * irq_wakeup_source;
	bool irq_is_attached;
	bool device_open; /* Is device open? */
	spinlock_t irq_enabled_lock;
	enum st21nfc_power_state pw_current;
	enum st21nfc_read_state r_state_current;
	int irq_pw_stats_idle;
	int p_idle_last;
	struct nfc_sub_power_stats pw_states[ST21NFC_POWER_STATE_MAX];
	struct nfc_sub_power_stats_error pw_states_err;
	struct workqueue_struct *st_p_wq;
	struct work_struct st_p_work;
	/*Power state shadow copies for reading*/
	enum st21nfc_power_state c_pw_current;
	struct nfc_sub_power_stats c_pw_states[ST21NFC_POWER_STATE_MAX];
	struct nfc_sub_power_stats_error c_pw_states_err;

	/* CLK control */
	bool clk_run;
	struct clk *s_clk;
	uint8_t pinctrl_en;

	/* GPIO for NFCC IRQ pin (input) */
	struct gpio_desc *gpiod_irq;
	/* GPIO for NFCC Reset pin (output) */
	struct gpio_desc *gpiod_reset;
	/* GPIO for NFCC CLK_REQ pin (input) */
	struct gpio_desc *gpiod_clkreq;
	/* GPIO for NFCC CLF_MONITOR_PWR (input) */
	struct gpio_desc *gpiod_pidle;
	bool pidle_active_low;

	/* irq_gpio polarity to be used */
	unsigned int polarity_mode;
};

/*
 * Routine to enable clock.
 * this routine can be extended to select from multiple
 * sources based on clk_src_name.
 */
static int st21nfc_clock_select(struct st21nfc_device *st21nfc_dev)
{
#ifndef NO_MTK_CLK_MANAGEMENT
	/*If use XTAL mode, please remove this function "clk_buf_ctrl" to
	 * avoid additional power consumption.
	 */
	clk_buf_ctrl(CLK_BUF_NFC, true);
#endif
	return 0;
}

/*
 * Routine to disable clocks
 */
static int st21nfc_clock_deselect(struct st21nfc_device *st21nfc_dev)
{
#ifndef NO_MTK_CLK_MANAGEMENT
	clk_buf_ctrl(CLK_BUF_NFC, false);
#endif
	return 0;
}

static void st21nfc_disable_irq(struct st21nfc_device *st21nfc_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&st21nfc_dev->irq_enabled_lock, flags);
	if (st21nfc_dev->irq_enabled) {
		disable_irq_nosync(st21nfc_dev->client->irq);
		st21nfc_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&st21nfc_dev->irq_enabled_lock, flags);
}

static void st21nfc_enable_irq(struct st21nfc_device *st21nfc_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&st21nfc_dev->irq_enabled_lock, flags);
	if (!st21nfc_dev->irq_enabled) {
		st21nfc_dev->irq_enabled = true;
		enable_irq(st21nfc_dev->client->irq);
	}
	spin_unlock_irqrestore(&st21nfc_dev->irq_enabled_lock, flags);
}

static irqreturn_t st21nfc_dev_irq_handler(int irq, void *dev_id)
{
	struct st21nfc_device *st21nfc_dev = dev_id;

	if (st21nfc_dev->irq_wakeup_source != NULL)
		__pm_wakeup_event(st21nfc_dev->irq_wakeup_source, WAKEUP_SRC_TIMEOUT);
	st21nfc_disable_irq(st21nfc_dev);

	/* Wake up waiting readers */
	wake_up(&st21nfc_dev->read_wq);

	return IRQ_HANDLED;
}

static int st21nfc_loc_set_polaritymode(struct st21nfc_device *st21nfc_dev,
					int mode)
{
	struct i2c_client *client = st21nfc_dev->client;
	struct device *dev = &client->dev;
	unsigned int irq_type;
	int ret;

	if (enable_debug_log)
		pr_info("%s:%d mode %d", __FILE__, __LINE__, mode);

	st21nfc_dev->polarity_mode = mode;
	/* setup irq_flags */
	switch (mode) {
	case IRQF_TRIGGER_RISING:
		irq_type = IRQ_TYPE_EDGE_RISING;
		break;
	case IRQF_TRIGGER_HIGH:
		irq_type = IRQ_TYPE_LEVEL_HIGH;
		break;
	default:
		irq_type = IRQ_TYPE_EDGE_RISING;
		break;
	}
	if (st21nfc_dev->irq_is_attached) {
		devm_free_irq(dev, client->irq, st21nfc_dev);
		st21nfc_dev->irq_is_attached = false;
	}
	ret = irq_set_irq_type(client->irq, irq_type);
	if (ret) {
		pr_err("%s : set_irq_type failed\n", __func__);
		return -ENODEV;
	}
	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	if (enable_debug_log)
		pr_debug("%s : requesting IRQ %d\n", __func__, client->irq);
	st21nfc_dev->irq_enabled = true;

	ret = devm_request_irq(dev, client->irq, st21nfc_dev_irq_handler,
			       st21nfc_dev->polarity_mode,
			       client->name, st21nfc_dev);

	if (ret) {
		pr_err("%s : devm_request_irq failed\n", __func__);
		return -ENODEV;
	}
	st21nfc_dev->irq_is_attached = true;
	st21nfc_disable_irq(st21nfc_dev);

	if (enable_debug_log)
		pr_info("%s:%d ret %d", __FILE__, __LINE__, ret);

	return ret;
}

static void st21nfc_power_stats_switch(struct st21nfc_device *st21nfc_dev,
				       uint64_t current_time_ms,
				       enum st21nfc_power_state old_state,
				       enum st21nfc_power_state new_state,
				       bool is_ntf)
{
	mutex_lock(&st21nfc_dev->pidle_mutex);

	if (new_state == old_state) {
		if ((st21nfc_dev->pw_states[ST21NFC_IDLE].last_entry != 0) ||
		    (old_state != ST21NFC_IDLE)) {
			pr_err("%s Error: Switched from %s to %s!: %llx, ntf=%d\n",
			       __func__, st21nfc_power_state_name[old_state],
			       st21nfc_power_state_name[new_state],
			       current_time_ms, is_ntf);

			if (new_state == ST21NFC_IDLE)
				st21nfc_dev->pw_states_err.idle_to_idle++;
			else if (new_state == ST21NFC_ACTIVE)
				st21nfc_dev->pw_states_err.active_to_active++;
			else if (new_state == ST21NFC_ACTIVE_RW)
				st21nfc_dev->pw_states_err.act_rw_to_act_rw++;

			mutex_unlock(&st21nfc_dev->pidle_mutex);
			return;
		}
	} else if (!is_ntf && new_state == ST21NFC_ACTIVE &&
		   old_state != ST21NFC_IDLE) {
		st21nfc_dev->pw_states_err.active_not_idle++;
	} else if (!is_ntf && new_state == ST21NFC_IDLE &&
		   old_state == ST21NFC_ACTIVE_RW) {
		st21nfc_dev->pw_states_err.active_rw_to_idle++;
	} else if (is_ntf && new_state == ST21NFC_ACTIVE &&
		   old_state == ST21NFC_IDLE) {
		st21nfc_dev->pw_states_err.idle_to_active_ntf++;
	} else if (is_ntf && new_state == ST21NFC_ACTIVE_RW &&
		   old_state == ST21NFC_IDLE) {
		st21nfc_dev->pw_states_err.idle_to_active_rw_ntf++;
	}

	pr_debug("%s Switching from %s to %s: %llx, ntf=%d\n", __func__,
		 st21nfc_power_state_name[old_state],
		 st21nfc_power_state_name[new_state], current_time_ms, is_ntf);
	st21nfc_dev->pw_states[old_state].last_exit = current_time_ms;
	st21nfc_dev->pw_states[old_state].duration +=
		st21nfc_dev->pw_states[old_state].last_exit -
		st21nfc_dev->pw_states[old_state].last_entry;
	st21nfc_dev->pw_states[new_state].count++;
	st21nfc_dev->pw_current = new_state;
	st21nfc_dev->pw_states[new_state].last_entry = current_time_ms;

	mutex_unlock(&st21nfc_dev->pidle_mutex);
}

static void st21nfc_power_stats_idle_signal(struct st21nfc_device *st21nfc_dev)
{
	uint64_t current_time_ms = ktime_to_ms(ktime_get_boottime());
	int value = gpiod_get_value(st21nfc_dev->gpiod_pidle);

	if (st21nfc_dev->pidle_active_low)
		value = !value;

	if (value != 0) {
		st21nfc_power_stats_switch(st21nfc_dev, current_time_ms,
					   st21nfc_dev->pw_current,
					   ST21NFC_ACTIVE, false);
	} else {
		st21nfc_power_stats_switch(st21nfc_dev, current_time_ms,
					   st21nfc_dev->pw_current,
					   ST21NFC_IDLE, false);
	}
}

static void st21nfc_pstate_wq(struct work_struct *work)
{
	struct st21nfc_device *st21nfc_dev =
		container_of(work, struct st21nfc_device, st_p_work);

	st21nfc_power_stats_idle_signal(st21nfc_dev);
}

static irqreturn_t st21nfc_dev_power_stats_handler(int irq, void *dev_id)
{
	struct st21nfc_device *st21nfc_dev = dev_id;

	queue_work(st21nfc_dev->st_p_wq, &(st21nfc_dev->st_p_work));

	return IRQ_HANDLED;
}

#ifdef ST54J_PWRSTATS
static void st21nfc_power_stats_filter(struct st21nfc_device *st21nfc_dev,
				       char *buf, size_t count)
{
	uint64_t current_time_ms = ktime_to_ms(ktime_get_boottime());
	__u16 ntf_opcode = nci_opcode(buf);

	if (IS_ERR(st21nfc_dev->gpiod_pidle))
		return;

	/* In order to avoid counting active state on PAYLOAD where it would
	 * match a possible header, power states are filtered only on NCI
	 * headers.
	 */
	if (st21nfc_dev->r_state_current != ST21NFC_HEADER)
		return;

	if (count != HEADER_LENGTH) {
		pr_err("%s Warning: expect previous one was idle data\n");
		st21nfc_dev->pw_states_err.header_payload++;
		return;
	}

	if (nci_mt(buf) != NCI_MT_NTF_PKT &&
	    nci_opcode_gid(ntf_opcode) != NCI_GID_PROPRIETARY)
		return;

	switch (ntf_opcode) {
	case PROP_PWR_MON_RW_OFF_NTF:
		st21nfc_power_stats_switch(st21nfc_dev, current_time_ms,
					   st21nfc_dev->pw_current,
					   ST21NFC_ACTIVE, true);
		break;
	case PROP_PWR_MON_RW_ON_NTF:
		st21nfc_power_stats_switch(st21nfc_dev, current_time_ms,
					   st21nfc_dev->pw_current,
					   ST21NFC_ACTIVE_RW, true);
		break;
	default:
		return;
	}
}
#endif

static ssize_t st21nfc_dev_read(struct file *filp, char __user *buf,
				size_t count, loff_t *offset)
{
	struct st21nfc_device *st21nfc_dev = container_of(
		filp->private_data, struct st21nfc_device, st21nfc_device);
	int ret;
#ifdef ST54J_PWRSTATS
	int idle = 0;
#endif // ST54J_PWRSTATS

	if (count == 0)
		return 0;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	if (enable_debug_log)
		pr_debug("%s : reading %zu bytes.\n", __func__, count);

	if (gpiod_get_value(st21nfc_dev->gpiod_irq) == 0) {
		pr_info("%s : read called but no IRQ.\n", __func__);
		memset(st21nfc_dev->buffer, 0x7E, count);
		if (copy_to_user(buf, st21nfc_dev->buffer, count)) {
			pr_warn("%s : failed to copy to user space\n",
				__func__);
			return -EFAULT;
		}
		return count;
	}

	mutex_lock(&st21nfc_dev->read_mutex);

	/* Read data */
#ifdef KRNMTKLEGACY_I2C
	st21nfc_dev->client->addr = (st21nfc_dev->client->addr & I2C_MASK_FLAG);
	st21nfc_dev->client->ext_flag |= I2C_DMA_FLAG;
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_DIRECTION_FLAG; */
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_A_FILTER_MSG; */
	st21nfc_dev->client->timing = NFC_CLIENT_TIMING;

	/* Read data */
	ret = i2c_master_recv(st21nfc_dev->client,
			      (unsigned char *)(uintptr_t)I2CDMAReadBuf_pa,
			      count);
	/* copy back to buffer */
	if (ret > 0)
		memcpy(st21nfc_dev->buffer,
		    (unsigned char *)(uintptr_t)I2CDMAReadBuf_pa, ret);
#else
	ret = i2c_master_recv(st21nfc_dev->client, st21nfc_dev->buffer, count);
#endif
#ifdef ST54J_PWRSTATS
	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		mutex_unlock(&st21nfc_dev->read_mutex);
		return ret;
	}
	if (st21nfc_dev->r_state_current == ST21NFC_HEADER) {
		/* Counting idle index */
		for (idle = 0;
		     idle < ret && st21nfc_dev->buffer[idle] == IDLE_CHARACTER;
		     idle++)
			;

		if (idle > 0 && idle < HEADER_LENGTH) {
			memmove(st21nfc_dev->buffer, st21nfc_dev->buffer + idle,
				ret - idle);
			ret = i2c_master_recv(st21nfc_dev->client,
					      st21nfc_dev->buffer + ret - idle,
					      idle);
			if (ret < 0) {
				pr_err("%s: i2c_master_recv returned %d\n",
				       __func__, ret);
				mutex_unlock(&st21nfc_dev->read_mutex);
				return ret;
			}
			ret = count;
		}
	}
#endif // ST54J_PWRSTATS
	mutex_unlock(&st21nfc_dev->read_mutex);

	if (ret < 0) {
		pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		pr_err("%s: received too many bytes from i2c (%d)\n", __func__,
		       ret);
		return -EIO;
	}

#ifdef ST54J_PWRSTATS
	if (idle < HEADER_LENGTH) {
		st21nfc_power_stats_filter(st21nfc_dev, st21nfc_dev->buffer,
					   ret);
		/* change state only if a payload is detected, i.e. size > 0*/
		if ((st21nfc_dev->r_state_current == ST21NFC_HEADER) &&
		    (st21nfc_dev->buffer[2] > 0)) {
			st21nfc_dev->r_state_current = ST21NFC_PAYLOAD;
			if (enable_debug_log)
				pr_debug("%s : new state = ST21NFC_PAYLOAD\n",
					 __func__);
		} else {
			st21nfc_dev->r_state_current = ST21NFC_HEADER;
			if (enable_debug_log)
				pr_debug("%s : new state = ST21NFC_HEADER\n",
					 __func__);
		}
	}
#endif // ST54J_PWRSTATS

	if (copy_to_user(buf, st21nfc_dev->buffer, ret)) {
		pr_warn("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

	return ret;
}

static ssize_t st21nfc_dev_write(struct file *filp, const char __user *buf,
				 size_t count, loff_t *offset)
{
	struct st21nfc_device *st21nfc_dev = container_of(
		filp->private_data, struct st21nfc_device, st21nfc_device);
	char *tmp = NULL;
	int ret = count;

	if (enable_debug_log) {
		//pr_debug("%s: st21nfc_dev ptr %p\n", __func__, st21nfc_dev);
		pr_debug("%s : writing %zu bytes.\n", __func__, count);
	}

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	tmp = memdup_user(buf, count);
	if (IS_ERR_OR_NULL(tmp)) {
		pr_err("%s : memdup_user failed\n", __func__);
		return -EFAULT;
	}

	/* Write data */
#ifdef KRNMTKLEGACY_I2C
	memcpy(I2CDMAWriteBuf, tmp, count);
	st21nfc_dev->client->addr = (st21nfc_dev->client->addr & I2C_MASK_FLAG);

	st21nfc_dev->client->ext_flag |= I2C_DMA_FLAG;
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_DIRECTION_FLAG; */
	/* st21nfc_dev->platform_data.client->ext_flag |= I2C_A_FILTER_MSG; */
	st21nfc_dev->client->timing = NFC_CLIENT_TIMING;

	ret = i2c_master_send(st21nfc_dev->client,
			      (unsigned char *)(uintptr_t)I2CDMAWriteBuf_pa,
			      count);
#else
	ret = i2c_master_send(st21nfc_dev->client, tmp, count);
#endif
	if (ret != count) {
		pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	}
	kfree(tmp);

	return ret;
}

static int st21nfc_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct st21nfc_device *st21nfc_dev = container_of(
		filp->private_data, struct st21nfc_device, st21nfc_device);

	if (enable_debug_log)
		pr_info("%s:%d dev_open", __FILE__, __LINE__);

	if (st21nfc_dev->device_open) {
		ret = -EBUSY;
		pr_err("%s : device already opened ret= %d\n", __func__, ret);
	} else {
		st21nfc_dev->device_open = true;
	}
	return ret;
}

static int st21nfc_release(struct inode *inode, struct file *file)
{
	struct st21nfc_device *st21nfc_dev = container_of(
		file->private_data, struct st21nfc_device, st21nfc_device);

	st21nfc_dev->device_open = false;
	if (enable_debug_log)
		pr_debug("%s : device_open  = false\n", __func__);

	return 0;
}

static void (*st21nfc_st54spi_cb)(int, void *);
static void *st21nfc_st54spi_data;
void st21nfc_register_st54spi_cb(void (*cb)(int, void *), void *data)
{
	if (enable_debug_log)
		pr_info("%s\n", __func__);

	st21nfc_st54spi_cb = cb;
	st21nfc_st54spi_data = data;
}
void st21nfc_unregister_st54spi_cb(void)
{
	if (enable_debug_log)
		pr_info("%s\n", __func__);

	st21nfc_st54spi_cb = NULL;
	st21nfc_st54spi_data = NULL;
}

static long st21nfc_dev_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	struct st21nfc_device *st21nfc_dev = container_of(
		filp->private_data, struct st21nfc_device, st21nfc_device);

	int ret = 0;

	u32 tmp;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != ST21NFC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !ACCESS_OK(VERIFY_WRITE, (void __user *)arg,
				 _IOC_SIZE(cmd));
	if (ret == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		ret = !ACCESS_OK(VERIFY_READ, (void __user *)arg,
				 _IOC_SIZE(cmd));
	if (ret)
		return -EFAULT;

	switch (cmd) {
	case ST21NFC_SET_POLARITY_RISING:
	case ST21NFC_LEGACY_SET_POLARITY_RISING:
		pr_info(" ### ST21NFC_SET_POLARITY_RISING ###\n");
		st21nfc_loc_set_polaritymode(st21nfc_dev, IRQF_TRIGGER_RISING);
		break;

	case ST21NFC_SET_POLARITY_HIGH:
	case ST21NFC_LEGACY_SET_POLARITY_HIGH:
		pr_info(" ### ST21NFC_SET_POLARITY_HIGH ###\n");
		st21nfc_loc_set_polaritymode(st21nfc_dev, IRQF_TRIGGER_HIGH);
		break;

	case ST21NFC_PULSE_RESET:
	case ST21NFC_LEGACY_PULSE_RESET:
		pr_info("%s Double Pulse Request\n", __func__);
		if (!IS_ERR_OR_NULL(st21nfc_dev->gpiod_reset)) {
			if (st21nfc_st54spi_cb != 0)
				(*st21nfc_st54spi_cb)(ST54SPI_CB_RESET_START,
						      st21nfc_st54spi_data);

			/* pulse low for 20 millisecs */
			gpiod_set_value(st21nfc_dev->gpiod_reset, 0);
			msleep(20);
			gpiod_set_value(st21nfc_dev->gpiod_reset, 1);
			usleep_range(10000, 11000);
			/* pulse low for 20 millisecs */
			gpiod_set_value(st21nfc_dev->gpiod_reset, 0);
			msleep(20);
			gpiod_set_value(st21nfc_dev->gpiod_reset, 1);
			pr_info("%s done Double Pulse Request\n", __func__);
			if (st21nfc_st54spi_cb != 0)
				(*st21nfc_st54spi_cb)(ST54SPI_CB_RESET_END,
						      st21nfc_st54spi_data);
		}
		st21nfc_dev->r_state_current = ST21NFC_HEADER;
		break;

	case ST21NFC_GET_WAKEUP:
	case ST21NFC_LEGACY_GET_WAKEUP:
		/* deliver state of Wake_up_pin as return value of ioctl */
		ret = gpiod_get_value(st21nfc_dev->gpiod_irq);

		/*
		 * Warning: depending on gpiod_get_value implementation,
		 * it can returns a value different than 1 in case of high level
		 */
		if (ret != 0)
			ret = 1;

		if (enable_debug_log)
			pr_debug("%s get gpio result %d\n", __func__, ret);
		break;
	case ST21NFC_GET_POLARITY:
	case ST21NFC_LEGACY_GET_POLARITY:
		ret = st21nfc_dev->polarity_mode;
		if (enable_debug_log)
			pr_debug("%s get polarity %d\n", __func__, ret);
		break;
	case ST21NFC_RECOVERY:
	case ST21NFC_LEGACY_RECOVERY:
		/* For ST21NFCD usage only */
		pr_info("%s Recovery Request\n", __func__);
		mutex_lock(&st21nfc_dev->irq_dir_mutex);
		if (!IS_ERR_OR_NULL(st21nfc_dev->gpiod_reset)) {
			if (st21nfc_dev->irq_is_attached) {
				devm_free_irq(&st21nfc_dev->client->dev,
					      st21nfc_dev->client->irq,
					      st21nfc_dev);
				st21nfc_dev->irq_is_attached = false;
			}
			/* pulse low for 20 millisecs */
			gpiod_set_value(st21nfc_dev->gpiod_reset, 0);
			usleep_range(10000, 11000);
			/* During the reset, force IRQ OUT as */
			/* DH output instead of input in normal usage */
			ret = gpiod_direction_output(st21nfc_dev->gpiod_irq, 1);
			if (ret) {
				pr_err("%s : gpiod_direction_output failed\n",
				       __func__);
				ret = -ENODEV;
				mutex_unlock(&st21nfc_dev->irq_dir_mutex);
				break;
			}

			gpiod_set_value(st21nfc_dev->gpiod_irq, 1);
			usleep_range(10000, 11000);
			gpiod_set_value(st21nfc_dev->gpiod_reset, 1);

			pr_info("%s done Pulse Request\n", __func__);
		}

		msleep(20);
		gpiod_set_value(st21nfc_dev->gpiod_irq, 0);
		msleep(20);
		gpiod_set_value(st21nfc_dev->gpiod_irq, 1);
		msleep(20);
		gpiod_set_value(st21nfc_dev->gpiod_irq, 0);
		msleep(20);
		pr_info("%s Recovery procedure finished\n", __func__);
		ret = gpiod_direction_input(st21nfc_dev->gpiod_irq);
		if (ret) {
			pr_err("%s : gpiod_direction_input failed\n", __func__);
			ret = -ENODEV;
		}

		st21nfc_dev->irq_enabled = true;

		ret = devm_request_irq(&st21nfc_dev->client->dev,
				       st21nfc_dev->client->irq,
				       st21nfc_dev_irq_handler,
				       st21nfc_dev->polarity_mode,
				       st21nfc_dev->client->name, st21nfc_dev);
		if (ret) {
			pr_err("%s : devm_request_irq failed\n", __func__);
			mutex_unlock(&st21nfc_dev->irq_dir_mutex);
			return -ENODEV;
		}
		st21nfc_dev->irq_is_attached = true;
		st21nfc_disable_irq(st21nfc_dev);

		mutex_unlock(&st21nfc_dev->irq_dir_mutex);
		break;
	case ST21NFC_USE_ESE:
		ret = __get_user(tmp, (u32 __user *)arg);
		if (ret == 0) {
			if (st21nfc_st54spi_cb != 0)
				(*st21nfc_st54spi_cb)(
					tmp ? ST54SPI_CB_ESE_USED :
					      ST54SPI_CB_ESE_NOT_USED,
					st21nfc_st54spi_data);
		}
		if (enable_debug_log)
			pr_debug("%s use ESE %d : %d\n", __func__, ret, tmp);
		break;
	default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static unsigned int st21nfc_poll(struct file *file, poll_table *wait)
{
	struct st21nfc_device *st21nfc_dev = container_of(
		file->private_data, struct st21nfc_device, st21nfc_device);
	unsigned int mask = 0;
	int pinlev = 0;

	/* wait for Wake_up_pin == high  */
	poll_wait(file, &st21nfc_dev->read_wq, wait);

	pinlev = gpiod_get_value(st21nfc_dev->gpiod_irq);
	mutex_lock(&st21nfc_dev->irq_dir_mutex);
	if (pinlev != 0) {
		if (enable_debug_log)
			pr_debug("%s return ready\n", __func__);

		mask = POLLIN | POLLRDNORM; /* signal data avail */
		st21nfc_disable_irq(st21nfc_dev);
	} else {
		/* Wake_up_pin is low. Activate ISR  */
		if (enable_debug_log)
			pr_debug("%s enable irq\n", __func__);

		st21nfc_enable_irq(st21nfc_dev);
	}

	mutex_unlock(&st21nfc_dev->irq_dir_mutex);
	return mask;
}

#ifdef WITH_PING_DURING_PROBE
/* Attempt a communication with the chip. Return 0 on success, < 0 on failure */
static int st21nfc_ping(struct st21nfc_device *st21nfc_dev)
{
	int ret = -ENODEV;
	int loops = 4;

	if (st21nfc_dev->device_open) {
		ret = -EBUSY;
		pr_err("%s : device already opened ret= %d\n", __func__, ret);
		return ret;
	}

	/* Some I2C masters have lazy init,
	 attempt a dummy read first to initialize the pull-ups if needed */
	(void)i2c_master_recv(st21nfc_dev->client, st21nfc_dev->buffer, 1);

	/* pulse low for 20 millisecs */
	gpiod_set_value(st21nfc_dev->gpiod_reset, 0);
	msleep(20);
	gpiod_set_value(st21nfc_dev->gpiod_reset, 1);
	usleep_range(10000, 11000);
	/* pulse low for 20 millisecs */
	gpiod_set_value(st21nfc_dev->gpiod_reset, 0);
	msleep(20);
	gpiod_set_value(st21nfc_dev->gpiod_reset, 1);
	pr_info("%s done Double Pulse Request\n", __func__);

	msleep(10);
	while ((loops-- > 0) && gpiod_get_value(st21nfc_dev->gpiod_irq)) {
		int len;

		// Read next message.
		len = i2c_master_recv(st21nfc_dev->client, st21nfc_dev->buffer,
				      4);
		if (len != 4) {
			pr_warn("%s Could not read header: %d\n", __func__,
				len);
			break;
		}
		if (st21nfc_dev->buffer[0] == IDLE_CHARACTER) {
			if (st21nfc_dev->buffer[1] == IDLE_CHARACTER) {
				pr_warn("%s Read 7E7E... header, IRQ always high ? Stop\n",
					__func__);
				break;
			} else {
				// 4bytes header, shift
				st21nfc_dev->buffer[0] = st21nfc_dev->buffer[1];
				st21nfc_dev->buffer[1] = st21nfc_dev->buffer[2];
				st21nfc_dev->buffer[2] = st21nfc_dev->buffer[3];
				len = i2c_master_recv(st21nfc_dev->client,
						      st21nfc_dev->buffer + 3,
						      st21nfc_dev->buffer[2]);
				if (len != (int)st21nfc_dev->buffer[2]) {
					pr_warn("%s Could not read payload: %d\n",
						__func__, len);
					break;
				}
			}
		} else {
			// 3 bytes header
			len = i2c_master_recv(st21nfc_dev->client,
					      st21nfc_dev->buffer + 3,
					      st21nfc_dev->buffer[2]);
			if (len != (int)st21nfc_dev->buffer[2]) {
				pr_warn("%s Could not read payload: %d\n",
					__func__, len);
				break;
			}
		}
		pr_info("%s Read message (%d bytes): %02x %02x ...\n", __func__,
			len + 3, st21nfc_dev->buffer[0],
			st21nfc_dev->buffer[1]);

		if (st21nfc_dev->buffer[0] == 0x60 &&
		    st21nfc_dev->buffer[1] == 0x00) {
			ret = 0;
		}

		msleep(5);
	}

	return ret;
}

#endif // WITH_PING_DURING_PROBE
#ifndef KRNMTKLEGACY_GPIO
static int st21nfc_platform_probe(struct platform_device *pdev)
{
	if (enable_debug_log)
		pr_debug("%s\n", __func__);

	return 0;
}

static int st21nfc_platform_remove(struct platform_device *pdev)
{
	if (enable_debug_log)
		pr_debug("%s\n", __func__);

	return 0;
}

#endif /* KRNMTKLEGACY_GPIO */
static const struct file_operations st21nfc_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = st21nfc_dev_read,
	.write = st21nfc_dev_write,
	.open = st21nfc_dev_open,
	.poll = st21nfc_poll,
	.release = st21nfc_release,
	.unlocked_ioctl = st21nfc_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = st21nfc_dev_ioctl
#endif
};

static ssize_t i2c_addr_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (client != NULL)
		return scnprintf(buf, PAGE_SIZE, "0x%.2x\n", client->addr);
	return -ENODEV;
} /* i2c_addr_show() */

static ssize_t i2c_addr_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct st21nfc_device *data = dev_get_drvdata(dev);
	long new_addr = 0;

	if (data != NULL && data->client != NULL) {
		if (!kstrtol(buf, 10, &new_addr)) {
			mutex_lock(&data->read_mutex);
			data->client->addr = new_addr;
			mutex_unlock(&data->read_mutex);
			return count;
		}
		return -EINVAL;
	}
	return 0;
} /* i2c_addr_store() */

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", DRIVER_VERSION);
} /* version_show */

static uint64_t st21nfc_power_duration(struct st21nfc_device *data,
				       enum st21nfc_power_state pstate,
				       uint64_t current_time_ms)
{
	return data->c_pw_current != pstate ?
		       data->c_pw_states[pstate].duration :
		       data->c_pw_states[pstate].duration +
			       (current_time_ms -
				data->c_pw_states[pstate].last_entry);
}

static ssize_t power_stats_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct st21nfc_device *data = dev_get_drvdata(dev);
	uint64_t current_time_ms;
	uint64_t idle_duration;
	uint64_t active_ce_duration;
	uint64_t active_rw_duration;

	mutex_lock(&data->pidle_mutex);

	data->c_pw_current = data->pw_current;
	data->c_pw_states_err = data->pw_states_err;
	memcpy(data->c_pw_states, data->pw_states,
	       ST21NFC_POWER_STATE_MAX * sizeof(struct nfc_sub_power_stats));

	mutex_unlock(&data->pidle_mutex);

	current_time_ms = ktime_to_ms(ktime_get_boottime());
	idle_duration =
		st21nfc_power_duration(data, ST21NFC_IDLE, current_time_ms);
	active_ce_duration =
		st21nfc_power_duration(data, ST21NFC_ACTIVE, current_time_ms);
	active_rw_duration = st21nfc_power_duration(data, ST21NFC_ACTIVE_RW,
						    current_time_ms);

	return scnprintf(
		buf, PAGE_SIZE,
		"NFC subsystem\n"
		"Idle mode:\n"
		"\tCumulative count: 0x%llx\n"
		"\tCumulative duration msec: 0x%llx\n"
		"\tLast entry timestamp msec: 0x%llx\n"
		"\tLast exit timestamp msec: 0x%llx\n"
		"Active mode:\n"
		"\tCumulative count: 0x%llx\n"
		"\tCumulative duration msec: 0x%llx\n"
		"\tLast entry timestamp msec: 0x%llx\n"
		"\tLast exit timestamp msec: 0x%llx\n"
		"Active Reader/Writer mode:\n"
		"\tCumulative count: 0x%llx\n"
		"\tCumulative duration msec: 0x%llx\n"
		"\tLast entry timestamp msec: 0x%llx\n"
		"\tLast exit timestamp msec: 0x%llx\n"
		"\nError transition header --> payload state machine: 0x%llx\n"
		"Error transition from an Active state when not in Idle state: 0x%llx\n"
		"Error transition from Idle state to Idle state: 0x%llx\n"
		"Warning transition from Active Reader/Writer state to Idle state: 0x%llx\n"
		"Error transition from Active state to Active state: 0x%llx\n"
		"Error transition from Idle state to Active state with notification: 0x%llx\n"
		"Error transition from Active Reader/Writer state to Active Reader/Writer state: 0x%llx\n"
		"Error transition from Idle state to Active Reader/Writer state with notification: 0x%llx\n"
		"\nTotal uptime: 0x%llx Cumulative modes time: 0x%llx\n",
		data->c_pw_states[ST21NFC_IDLE].count, idle_duration,
		data->c_pw_states[ST21NFC_IDLE].last_entry,
		data->c_pw_states[ST21NFC_IDLE].last_exit,
		data->c_pw_states[ST21NFC_ACTIVE].count, active_ce_duration,
		data->c_pw_states[ST21NFC_ACTIVE].last_entry,
		data->c_pw_states[ST21NFC_ACTIVE].last_exit,
		data->c_pw_states[ST21NFC_ACTIVE_RW].count, active_rw_duration,
		data->c_pw_states[ST21NFC_ACTIVE_RW].last_entry,
		data->c_pw_states[ST21NFC_ACTIVE_RW].last_exit,
		data->c_pw_states_err.header_payload,
		data->c_pw_states_err.active_not_idle,
		data->c_pw_states_err.idle_to_idle,
		data->c_pw_states_err.active_rw_to_idle,
		data->c_pw_states_err.active_to_active,
		data->c_pw_states_err.idle_to_active_ntf,
		data->c_pw_states_err.act_rw_to_act_rw,
		data->c_pw_states_err.idle_to_active_rw_ntf, current_time_ms,
		idle_duration + active_ce_duration + active_rw_duration);
}

static DEVICE_ATTR_RW(i2c_addr);

static DEVICE_ATTR_RO(version);

static DEVICE_ATTR_RO(power_stats);

static struct attribute *st21nfc_attrs[] = {
	&dev_attr_i2c_addr.attr,
	&dev_attr_version.attr,
	&dev_attr_power_stats.attr,
	NULL,
};

static struct attribute_group st21nfc_attr_grp = {
	.attrs = st21nfc_attrs,
};

// QCOM and MTK54 use standard GPIO definition

static int st21nfc_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret;
	struct st21nfc_device *st21nfc_dev;
	struct device *dev = &client->dev;
	int r;
	struct device_node *np = dev->of_node;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	st21nfc_dev = devm_kzalloc(dev, sizeof(*st21nfc_dev), GFP_KERNEL);
	if (st21nfc_dev == NULL)
		return -ENOMEM;

#ifdef KRNMTKLEGACY_I2C
#ifdef CONFIG_64BIT
	I2CDMAWriteBuf =
		(char *)dma_alloc_coherent(&client->dev, MAX_BUFFER_SIZE,
					   (dma_addr_t *)&I2CDMAWriteBuf_pa,
					   GFP_KERNEL);
#else
	I2CDMAWriteBuf =
		(char *)dma_alloc_coherent(NULL, MAX_BUFFER_SIZE,
					   (dma_addr_t *)&I2CDMAWriteBuf_pa,
					   GFP_KERNEL);
#endif

	if (I2CDMAWriteBuf == NULL)
		pr_err("%s : failed to allocate dma buffer\n", __func__);
#ifdef CONFIG_64BIT
	I2CDMAReadBuf = (char *)dma_alloc_coherent(
		&client->dev, MAX_BUFFER_SIZE,
		dma_addr_t *)&I2CDMAReadBuf_pa, GFP_KERNEL);
#else
	I2CDMAReadBuf =
		(char *)dma_alloc_coherent(NULL, MAX_BUFFER_SIZE,
					   (dma_addr_t *)&I2CDMAReadBuf_pa,
					   GFP_KERNEL);
#endif

	if (I2CDMAReadBuf == NULL)
		pr_err("%s : failed to allocate dma buffer\n", __func__);
	pr_debug("%s :I2CDMAWriteBuf_pa %d, I2CDMAReadBuf_pa,%d\n", __func__,
		 I2CDMAWriteBuf_pa, I2CDMAReadBuf_pa);
#endif /* KRNMTKLEGACY_I2C */

	/* store for later use */
	st21nfc_dev->client = client;
	st21nfc_dev->r_state_current = ST21NFC_HEADER;

// QCOM and MTK54 use standard GPIO definition
	np = of_find_compatible_node(NULL, NULL, "mediatek,nfc-gpio-v2");
	if (!np) {
		pr_err("%s : cannot find mediatek,nfc-gpio-v2 in DTS.\n",
		       __func__);
		return -ENODEV;
	}

// QCOM and MTK54 use standard GPIO definition
	r = of_get_named_gpio(np, "gpio-irq-std", 0);
	if (!gpio_is_valid(r)) {
		pr_err("%s: get NFC IRQ GPIO failed (%d)", __FILE__, r);
		return -ENODEV;
	}
	st21nfc_dev->gpiod_irq = gpio_to_desc(r);
	ret = gpio_request(r,
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
			   "gpio-irq-std"
#else
			   "gpio-irq"
#endif
	);
	if (ret) {
		pr_err("%s : gpio_request failed\n", __FILE__);
		return -ENODEV;
	}
	pr_info("%s : IRQ GPIO = %d\n", __func__, r);
	ret = gpio_direction_input(r);
	if (ret) {
		pr_err("%s : gpio_direction_input failed\n", __FILE__);
		return -ENODEV;
	}
	if (IS_ERR_OR_NULL(st21nfc_dev->gpiod_irq)) {
		pr_err("%s : Unable to request irq-gpios\n", __func__);
		return -ENODEV;
	}

// QCOM and MTK54 use standard GPIO definition
	r = of_get_named_gpio(np, "gpio-rst-std", 0);
	if (!gpio_is_valid(r)) {
		pr_err("%s: get NFC RST GPIO failed (%d)", __FILE__, r);
		return -ENODEV;
	}
	st21nfc_dev->gpiod_reset = gpio_to_desc(r);
	ret = gpio_request(r,
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
			   "gpio-rst-std"
#else
			   "gpio-rst"
#endif
	);
	if (ret) {
		pr_err("%s : gpio_request failed\n", __FILE__);
		return -ENODEV;
	}
	pr_info("%s : RST GPIO = %d\n", __func__, r);
	ret = gpio_direction_output(r, 1);
	if (ret) {
		pr_err("%s : gpio_direction_output failed\n", __FILE__);
		return -ENODEV;
	}
	gpio_set_value(r, 1);
	if (IS_ERR_OR_NULL(st21nfc_dev->gpiod_reset)) {
		pr_warn("%s : Unable to request reset-gpios\n", __func__);
		return -ENODEV;
	}

// QCOM and MTK54 use standard GPIO definition
	ret = of_get_named_gpio(np, "gpio-pidle-std", 0);
	if (gpio_is_valid(ret))
		st21nfc_dev->gpiod_pidle = gpio_to_desc(ret);
	if (IS_ERR_OR_NULL(st21nfc_dev->gpiod_pidle)) {
		pr_warn("[OPTIONAL] %s: Unable to request pidle-gpio\n",
			__func__);
		ret = 0;
	} else {
		if (!device_property_read_bool(dev, "st,pidle_active_low")) {
			pr_info("%s:[OPTIONAL] pidle_active_low not set\n", __func__);
			st21nfc_dev->pidle_active_low = false;
		} else {
			pr_info("%s:[OPTIONAL] pidle_active_low set\n", __func__);
			st21nfc_dev->pidle_active_low = true;
		}
		/* Prepare a workqueue for st21nfc_dev_power_stats_handler */
		st21nfc_dev->st_p_wq = create_workqueue("st_pstate_work");
		if(!st21nfc_dev->st_p_wq)
			return -ENODEV;
		mutex_init(&st21nfc_dev->pidle_mutex);
		INIT_WORK(&(st21nfc_dev->st_p_work), st21nfc_pstate_wq);
		/* Start the power stat in power mode idle */
		st21nfc_dev->irq_pw_stats_idle =
			gpiod_to_irq(st21nfc_dev->gpiod_pidle);

		ret = irq_set_irq_type(st21nfc_dev->irq_pw_stats_idle,
				       IRQ_TYPE_EDGE_BOTH);
		if (ret) {
			pr_err("%s : set_irq_type failed\n", __func__);
			goto err_pidle_workqueue;
		}

		/* This next call requests an interrupt line */
		ret = devm_request_irq(
			dev, st21nfc_dev->irq_pw_stats_idle,
			(irq_handler_t)st21nfc_dev_power_stats_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			/* Interrupt on both edges */
			"st21nfc_pw_stats_idle_handle", st21nfc_dev);
		if (ret) {
			pr_err("%s : devm_request_irq for power stats idle failed\n",
			       __func__);
			goto err_pidle_workqueue;
		}
	}

	ret = st21nfc_clock_select(st21nfc_dev);
	if (ret < 0) {
		pr_err("%s : st21nfc_clock_select failed\n", __func__);
		goto err_sysfs_power_stats;
	}

	np = of_find_compatible_node(NULL, NULL, "mediatek,irq_nfc-eint");
	if (np) {
		client->irq = irq_of_parse_and_map(np, 0);
		pr_info("%s : MT IRQ GPIO = %d\n", __func__, client->irq);
	}

	/* I2C retry management: we want only 1 attempt at communication.
	   As some busses need retry=1 and most need retry=0, we add optional DTS entry */
	if (of_property_read_u32(dev->of_node, "i2c-retry",
				 &client->adapter->retries)) {
		client->adapter->retries = 0;
	} else {
		pr_debug("%s : i2c-retry = %d\n", __func__,
			 client->adapter->retries);
	}

#ifdef WITH_PING_DURING_PROBE
	if ((ret = st21nfc_ping(st21nfc_dev))) {
		pr_err("%s: Did not get CORE_RESET_NTF, hardware issue? (%d)\n", __func__, ret);
		return ret;
	}
#endif // WITH_PING_DURING_PROBE

	/* init mutex and queues */
	init_waitqueue_head(&st21nfc_dev->read_wq);
	mutex_init(&st21nfc_dev->read_mutex);
	mutex_init(&st21nfc_dev->irq_dir_mutex);
	spin_lock_init(&st21nfc_dev->irq_enabled_lock);
	pr_debug(
		"%s : debug irq_gpio = %d, client-irq =  %d, pidle_gpio = %d\n",
		__func__,
		IS_ERR_OR_NULL(st21nfc_dev->gpiod_irq) ?
			-1 :
			desc_to_gpio(st21nfc_dev->gpiod_irq),
		client->irq,
		IS_ERR_OR_NULL(st21nfc_dev->gpiod_pidle) ?
			-1 :
			desc_to_gpio(st21nfc_dev->gpiod_pidle));

	st21nfc_dev->st21nfc_device.minor = MISC_DYNAMIC_MINOR;
	st21nfc_dev->st21nfc_device.name = "st21nfc";
	st21nfc_dev->st21nfc_device.fops = &st21nfc_dev_fops;
	st21nfc_dev->st21nfc_device.parent = dev;

	i2c_set_clientdata(client, st21nfc_dev);
	ret = misc_register(&st21nfc_dev->st21nfc_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __func__);
		goto err_misc_register;
	}

	ret = sysfs_create_group(&dev->kobj, &st21nfc_attr_grp);
	if (ret) {
		pr_err("%s : sysfs_create_group failed\n", __func__);
		goto err_sysfs_create_group_failed;
	}
	st21nfc_dev->irq_wakeup_source = wakeup_source_register(NULL, "st21nfc");
	st21nfc_dev->irq_wake_up = false;

	return 0;

err_sysfs_create_group_failed:
	misc_deregister(&st21nfc_dev->st21nfc_device);
err_misc_register:
	mutex_destroy(&st21nfc_dev->read_mutex);
	mutex_destroy(&st21nfc_dev->irq_dir_mutex);
err_sysfs_power_stats:
	if (!IS_ERR_OR_NULL(st21nfc_dev->gpiod_pidle)) {
		sysfs_remove_file(&client->dev.kobj,
				  &dev_attr_power_stats.attr);
	}
err_pidle_workqueue:
	if (!IS_ERR(st21nfc_dev->gpiod_pidle)) {
		mutex_destroy(&st21nfc_dev->pidle_mutex);
		destroy_workqueue(st21nfc_dev->st_p_wq);
	}
	return ret;
}

static int st21nfc_remove(struct i2c_client *client)
{
	struct st21nfc_device *st21nfc_dev = i2c_get_clientdata(client);

#ifdef KRNMTKLEGACY_I2C
	if (I2CDMAWriteBuf) {
#ifdef CONFIG_64BIT
		dma_free_coherent(&client->dev, MAX_BUFFER_SIZE, I2CDMAWriteBuf,
				  I2CDMAWriteBuf_pa);
#else
		dma_free_coherent(NULL, MAX_BUFFER_SIZE, I2CDMAWriteBuf,
				  I2CDMAWriteBuf_pa);
#endif
		I2CDMAWriteBuf = NULL;
		I2CDMAWriteBuf_pa = 0;
	}

	if (I2CDMAReadBuf) {
#ifdef CONFIG_64BIT
		dma_free_coherent(&client->dev, MAX_BUFFER_SIZE, I2CDMAReadBuf,
				  I2CDMAReadBuf_pa);
#else
		dma_free_coherent(NULL, MAX_BUFFER_SIZE, I2CDMAReadBuf,
				  I2CDMAReadBuf_pa);
#endif
		I2CDMAReadBuf = NULL;
		I2CDMAReadBuf_pa = 0;
	}
#endif /* KRNMTKLEGACY_I2C */

	st21nfc_clock_deselect(st21nfc_dev);
	misc_deregister(&st21nfc_dev->st21nfc_device);
	if (!IS_ERR_OR_NULL(st21nfc_dev->gpiod_pidle)) {
		sysfs_remove_file(&client->dev.kobj,
				  &dev_attr_power_stats.attr);
		mutex_destroy(&st21nfc_dev->pidle_mutex);
	}
	sysfs_remove_group(&client->dev.kobj, &st21nfc_attr_grp);
	if (st21nfc_dev->irq_wakeup_source) {
		wakeup_source_unregister(st21nfc_dev->irq_wakeup_source);
		st21nfc_dev->irq_wakeup_source = NULL;
	}
	mutex_destroy(&st21nfc_dev->read_mutex);
	mutex_destroy(&st21nfc_dev->irq_dir_mutex);
	acpi_dev_remove_driver_gpios(ACPI_COMPANION(&client->dev));

	return 0;
}

static int st21nfc_suspend(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct st21nfc_device *st21nfc_dev = i2c_get_clientdata(client);

	if (st21nfc_dev->irq_enabled) {
		if (!enable_irq_wake(client->irq))
			st21nfc_dev->irq_wake_up = true;
	}

	if (!IS_ERR_OR_NULL(st21nfc_dev->gpiod_pidle))
		st21nfc_dev->p_idle_last =
			gpiod_get_value(st21nfc_dev->gpiod_pidle);

	return 0;
}

static int st21nfc_resume(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct st21nfc_device *st21nfc_dev = i2c_get_clientdata(client);
	int pidle;

	if (st21nfc_dev->irq_wake_up) {
		if (!disable_irq_wake(client->irq))
			st21nfc_dev->irq_wake_up = false;
	}

	if (!IS_ERR(st21nfc_dev->gpiod_pidle)) {
		pidle = gpiod_get_value(st21nfc_dev->gpiod_pidle);
		if ((st21nfc_dev->p_idle_last != pidle) ||
		    (st21nfc_dev->pw_current == ST21NFC_IDLE && pidle != 0) ||
		    (st21nfc_dev->pw_current == ST21NFC_ACTIVE && pidle == 0))
			queue_work(st21nfc_dev->st_p_wq,
				   &(st21nfc_dev->st_p_work));
	}

	return 0;
}

static const struct i2c_device_id st21nfc_id[] = { { "st21nfc", 0 }, {} };

static const struct of_device_id st21nfc_of_match[] = {
	{ .compatible = "mediatek,nfc" },
	{}
};
MODULE_DEVICE_TABLE(of, st21nfc_of_match);

static const struct dev_pm_ops st21nfc_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	st21nfc_suspend, st21nfc_resume) };


static struct i2c_driver st21nfc_driver = {
	.id_table = st21nfc_id,
	.probe = st21nfc_probe,
	.remove = st21nfc_remove,
	.driver =
		{
			.owner = THIS_MODULE,
			.name = I2C_ID_NAME,
			.of_match_table = st21nfc_of_match,
			.probe_type = PROBE_PREFER_ASYNCHRONOUS,
			.pm = &st21nfc_pm_ops,
		},
};

#ifndef KRNMTKLEGACY_GPIO
/*  platform driver */
static const struct of_device_id nfc_dev_of_match[] = {
	{
		.compatible = "mediatek,nfc-gpio-v2",
	},
	{},
};

static struct platform_driver st21nfc_platform_driver = {
	.probe = st21nfc_platform_probe,
	.remove = st21nfc_platform_remove,
	.driver =
		{
			.name = I2C_ID_NAME,
			.owner = THIS_MODULE,
			.of_match_table = nfc_dev_of_match,
		},
};
#endif /* KRNMTKLEGACY_GPIO */

#ifdef GKI_MODULE
module_i2c_driver(st21nfc_driver);
#else // GKI_MODULE

/* module load/unload record keeping */
static int __init st21nfc_dev_init(void)
{
	pr_info("Loading st21nfc driver\n");
#ifndef KRNMTKLEGACY_GPIO
	platform_driver_register(&st21nfc_platform_driver);
	if (enable_debug_log)
		pr_debug("Loading st21nfc i2c driver\n");
#endif
	return i2c_add_driver(&st21nfc_driver);
}

module_init(st21nfc_dev_init);

static void __exit st21nfc_dev_exit(void)
{
	pr_info("Unloading st21nfc driver\n");
	i2c_del_driver(&st21nfc_driver);
}

module_exit(st21nfc_dev_exit);
#endif // GKI_MODULE

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("NFC ST21NFC driver");
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
