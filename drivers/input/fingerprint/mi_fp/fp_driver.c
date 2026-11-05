#include "fp_driver.h"
#define WAKELOCK_HOLD_TIME 2000	/* in ms */
#define FP_UNLOCK_REJECTION_TIMEOUT (WAKELOCK_HOLD_TIME - 500) /*ms*/
/*device name after register in charater*/
#define FP_DEV_NAME "xiaomi-fp"
#define FP_CLASS_NAME "xiaomi_fp"
#define FP_INPUT_NAME "uinput-xiaomi"
#define FP_ID_DEV_NAME "mifp_id"

#ifdef CONFIG_FP_MTK_PLATFORM
#include "teei_fp.h"
#include "tee_client_api.h"

extern void mt_spi_enable_master_clk(struct spi_device *spidev);
extern void mt_spi_disable_master_clk(struct spi_device *spidev);

static atomic_t clk_ref = ATOMIC_INIT(0);
static void fp_spi_clk_enable(struct spi_device *spi)
{
        if (atomic_read(&clk_ref) == 0) {
                pr_debug("enable spi clk\n");
                mt_spi_enable_master_clk(spi);
                atomic_inc(&clk_ref);
                pr_debug("increase spi clk ref to %d\n",atomic_read(&clk_ref));
        }
}
static void fp_spi_clk_disable(struct spi_device *spi)
{
        if (atomic_read(&clk_ref) == 1) {
                atomic_dec(&clk_ref);
                pr_debug(" disable spi clk\n");
                mt_spi_disable_master_clk(spi);
                pr_debug( "decrease spi clk ref to %d\n",atomic_read(&clk_ref));
        }
}
#endif

/*************************************************************/
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
struct work_struct fp_display_work;
static struct fp_device fp;

#ifndef XIAOMI_DRM_INTERFACE_WA

static struct drm_panel *prim_panel;
static void *cookie = NULL;
#if IS_ENABLED(CONFIG_FP_HAVE_MULTI_SCREEN)
static struct drm_panel *sec_panel;
static void *cookie_sec = NULL;
#endif

static int fp_check_panel(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	if(!np) {
		pr_err("device is null,failed to find active panel\n");
		return -ENODEV;
	}
	count = of_count_phandle_with_args(np, "panel", NULL);
	pr_info("%s:of_count_phandle_with_args:count=%d\n", __func__,count);
	if (count <= 0) {
	#if IS_ENABLED(CONFIG_FP_HAVE_MULTI_SCREEN)
		goto find_sec_panel;
	#endif
		return -ENODEV;
	}
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			prim_panel = panel;
			pr_info("%s:prim_panel = panel\n", __func__);
			break;
		} else {
			prim_panel = NULL;
			pr_info("%s:prim_panel = NULL\n", __func__);
		}
	}
	if (PTR_ERR(prim_panel) == -EPROBE_DEFER) {
		pr_err("%s ERROR: Cannot find prim_panel of node!", __func__);
	}

#if IS_ENABLED(CONFIG_FP_HAVE_MULTI_SCREEN)
find_sec_panel:
	count = of_count_phandle_with_args(np, "panel1", NULL);
	pr_info("%s:of_count_phandle_with_args:count=%d\n", __func__,count);
	if (count <= 0)
		return -ENODEV;
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel1", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			sec_panel = panel;
			pr_info("%s:sec_panel = panel\n", __func__);
			break;
		} else {
			sec_panel = NULL;
			pr_info("%s:sec_panel = NULL\n", __func__);
		}
	}
	if (PTR_ERR(sec_panel) == -EPROBE_DEFER) {
		pr_err("%s ERROR: Cannot find sec_panel of node!", __func__);
	}
#endif
	return PTR_ERR(panel);
}

static void fp_screen_state_for_fingerprint_callback(enum panel_event_notifier_tag notifier_tag,
            struct panel_event_notification *notification, void *client_data)
{
	struct fp_device *fp_dev = client_data;
	if (!fp_dev)
		return;

	if (!notification) {
		pr_err("%s:Invalid notification\n", __func__);
		return;
	}

	if(notification->notif_data.early_trigger) {
		return;
	}
	if(notifier_tag == PANEL_EVENT_NOTIFICATION_PRIMARY || notifier_tag == PANEL_EVENT_NOTIFICATION_SECONDARY){
		switch (notification->notif_type) {
			case DRM_PANEL_EVENT_UNBLANK:
				pr_debug("%s:DRM_PANEL_EVENT_UNBLANK\n", __func__);
				if (fp_dev->device_available == 1) {
					fp_dev->fb_black = 0;
					if (fp_dev->fp_netlink_enabled)
						fp_netlink_send(fp_dev, FP_NETLINK_SCREEN_ON);
				}
				break;
			case DRM_PANEL_EVENT_BLANK:
				pr_debug("%s:DRM_PANEL_EVENT_BLANK\n", __func__);
				if (fp_dev->device_available == 1 && fp_dev->fb_black == 0) {
					fp_dev->fb_black = 1;
					fp_dev->wait_finger_down = true;
					if (fp_dev->fp_netlink_enabled)
						fp_netlink_send(fp_dev, FP_NETLINK_SCREEN_OFF);
				}
				break;
			default:
				break;
		}
	}
}

static void fp_register_panel_notifier_work(struct work_struct *work)
{
	struct fp_device *fp_dev = container_of(work, struct fp_device, screen_state_dw.work);
	int error = 0;
	static int retry_count = 0;
	struct device_node *node;
	node = of_find_node_by_name(NULL, "fingerprint-screen");
	if (!node) {
		pr_err("%s ERROR: Cannot find node with panel!", __func__);
		return;
	}

	error = fp_check_panel(node);
	if (prim_panel) {
		pr_info("success to get primary panel, retry times = %d",retry_count);
		if (!cookie) {
			cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
					PANEL_EVENT_NOTIFIER_CLIENT_FINGERPRINT, prim_panel,
					fp_screen_state_for_fingerprint_callback, (void*)fp_dev);
			if (IS_ERR(cookie))
				pr_err("%s:Failed to register for prim_panel events\n", __func__);
			else
				pr_info("%s:prim_panel_event_notifier_register register succeed\n", __func__);
		}
	} else {
		pr_err("Failed to register primary panel notifier, try again\n");
		if (retry_count++ < 5) {
			queue_delayed_work(fp_dev->screen_state_wq, &fp_dev->screen_state_dw, 5 * HZ);
		} else {
			pr_err("Failed to register primary panel notifier, not try\n");
		}
	}

#if IS_ENABLED(CONFIG_FP_HAVE_MULTI_SCREEN)
	if (sec_panel) {
		pr_info("success to get second panel, retry times = %d",retry_count);
		if (!cookie_sec) {
			cookie_sec = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_SECONDARY,
					PANEL_EVENT_NOTIFIER_CLIENT_FINGERPRINT_SECOND, sec_panel,
					fp_screen_state_for_fingerprint_callback, (void*)fp_dev);
			if (IS_ERR(cookie_sec))
				pr_err("%s:Failed to register for sec_panel events\n", __func__);
			else
				pr_info("%s:sec_panel_event_notifier_register register succeed\n", __func__);
		}
	} else {
		pr_err("Failed to register second panel notifier, try again\n");
		if (retry_count++ < 5) {
			queue_delayed_work(fp_dev->screen_state_wq, &fp_dev->screen_state_dw, 5 * HZ);
		} else {
			pr_err("Failed to register second panel notifier, not try\n");
		}
    }
#endif
}
#endif /*XIAOMI_DRM_INTERFACE_WA*/

#ifdef FP_ULTRA_QCOM
static int get_events_fifo_len_locked(
		struct fp_device *fp_dev, int minor_no)
{
	int len = 0;
#ifndef FP_ULTRA_QCOM_P11
	if (minor_no == MINOR_NUM_FD) {
		mutex_lock(&fp_dev->fd_events_mutex);
		len = kfifo_len(&fp_dev->fd_events);
		mutex_unlock(&fp_dev->fd_events_mutex);
	} else if (minor_no == MINOR_NUM_IPC) {
#else
	if (minor_no == MINOR_NUM_IPC) {
#endif
		mutex_lock(&fp_dev->ipc_events_mutex);
		len = kfifo_len(&fp_dev->ipc_events);
		mutex_unlock(&fp_dev->ipc_events_mutex);
	}

	return len;
}

static irqreturn_t qbt_ipc_irq_handler(int irq, void *dev_id)
{
	struct fp_device *fp_dev = (struct fp_device *)dev_id;

	if (!fp_dev) {
		pr_err("NULL pointer passed\n");
		return IRQ_HANDLED;
	}

	if (irq != fp_dev->fw_ipc.irq) {
		pr_warn("invalid irq %d (expected %d)\n",
			irq, fp_dev->fw_ipc.irq);
		return IRQ_HANDLED;
	}

#ifdef FP_ULTRA_QCOM_P11
	fp_local_time_printk("IPC event received at time %lu uS\n",
			(unsigned long)ktime_to_us(ktime_get()));
#else
	pr_debug("IPC event received at time %lu uS\n",
			(unsigned long)ktime_to_us(ktime_get()));
#endif

	fp_acquire_stay_wake(fp_dev);
	schedule_work(&fp_dev->fw_ipc.work);

	return IRQ_HANDLED;
}

static void qbt_irq_report_event(struct work_struct *work)
{
	struct fp_device *fp_dev;
	struct ipc_event fw_ev_des;

	if (!work) {
		pr_err("NULL pointer passed\n");
		return;
	}
	fp_dev = container_of(work, struct fp_device, fw_ipc.work);

	fw_ev_des.ev = FW_EVENT_IPC;
	mutex_lock(&fp_dev->ipc_events_mutex);
	if (!kfifo_put(&fp_dev->ipc_events, fw_ev_des)) {
		pr_err("ipc events: fifo full, drop event %d\n",
				(int) fw_ev_des.ev);
	} else {
#ifdef FP_ULTRA_QCOM_P11
		fp_local_time_printk("IPC event %d queued at time %lu uS\n", fw_ev_des.ev,
				(unsigned long)ktime_to_us(ktime_get()));
#else
		pr_debug("IPC event %d queued at time %lu uS\n", fw_ev_des.ev,
				(unsigned long)ktime_to_us(ktime_get()));
#endif
	}
	mutex_unlock(&fp_dev->ipc_events_mutex);
	wake_up_interruptible(&fp_dev->read_wait_queue_ipc);
	fp_release_stay_wake(fp_dev);
}

static int enable_intr2_report(struct fp_device *fp_dev)
{
	int rc = 0;
	if (!(fp_dev->position.reqIntr2)) {
		rc = gpio_request(fp_dev->intr2_gpio, DTS_INTR2_GPIO);
		if (rc) {
			pr_err("failed to request intr2 gpio %d, error %d\n", fp_dev->intr2_gpio, rc);
		} else {
			pr_debug( "set intr2 gpio direction output\n" );
			gpio_direction_output(fp_dev->intr2_gpio, 0);
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
			fp_dev->position.reqIntr2 = 1;
		}
	}

	fp_dev->position.enable = true;

	return rc;
}

static void disable_intr2_report(struct fp_device *fp_dev)
{
	fp_dev->position.enable = false;
	fp_dev->position.SLOT_MAP = 0;
	fp_dev->position.SLOT_CUR = 0;
	memset(fp_dev->position.TRACKING_ID, 0, sizeof(int) * 32);
	memset(fp_dev->position.touch_x, 0, sizeof(int) * 32);
	memset(fp_dev->position.touch_y, 0, sizeof(int) * 32);
}

static int setup_ipc_irq(struct fp_device *fp_dev)
{
	int rc = 0;
	const char *desc = "qbt_ipc";

	INIT_WORK(&fp_dev->fw_ipc.work, qbt_irq_report_event);
	fp_dev->fw_ipc.work_init = true;

	pr_debug("irq %d gpio %d\n",
			fp_dev->fw_ipc.irq, fp_dev->fw_ipc.gpio);

	if (fp_dev->fw_ipc.irq < 0) {
		rc = fp_dev->fw_ipc.irq;
		pr_err("no irq for gpio %d, error=%d\n",
		  fp_dev->fw_ipc.gpio, rc);
		goto end;
	}

	gpio_direction_input(fp_dev->fw_ipc.gpio);

	rc = request_threaded_irq(fp_dev->fw_ipc.irq, NULL, qbt_ipc_irq_handler,
			IRQF_ONESHOT |
			IRQF_TRIGGER_FALLING, desc,
			fp_dev);

	if (rc < 0) {
		pr_err("failed to register for ipc irq %d, rc = %d\n",
			fp_dev->fw_ipc.irq, rc);
		goto end;
	}

end:
	pr_debug("Setup ipc irq success, rc=%d\n", rc);
	return rc;
}

static void free_ipc_irq(struct fp_device *fp_dev)
{
	if (fp_dev->fw_ipc.irq) {
		if (fp_dev->fw_ipc.irq_enabled) {
			disable_irq(fp_dev->fw_ipc.irq);
			fp_dev->fw_ipc.irq_enabled = false;
			pr_debug("disable fw_ipc.irq success!\n");
		} else {
			pr_err("fw_ipc.irq is already disable!\n");
		}
		free_irq(fp_dev->fw_ipc.irq, fp_dev);
		fp_dev->fw_ipc.work_init = false;
	} else {
		pr_err("fw_ipc.irq is not apply!\n");
	}
}

#ifndef FP_ULTRA_QCOM_P11
static void qbt_fd_report_event(struct fp_device *fp_dev,
		struct fd_event *event)
{
	mutex_lock(&fp_dev->fd_events_mutex);

	if (!kfifo_put(&fp_dev->fd_events, *event)) {
		pr_err("FD events fifo: error adding item\n");
	} else {
		pr_debug("FD event %d at slot %d queued at time %lu uS\n",
				event->state, event->id,
				(unsigned long)ktime_to_us(ktime_get()));
	}
	mutex_unlock(&fp_dev->fd_events_mutex);
	wake_up_interruptible(&fp_dev->read_wait_queue_fd);
}

static void qbt_gpio_report_event(struct fp_device *fp_dev, int state)
{
	struct fd_event event;

	memset(&event, 0, sizeof(event));

	if (!fp_dev->is_wuhb_connected) {
		pr_err("Skipping as WUHB_INT is disconnected\n");
		return;
	}

	if (fp_dev->fd_gpio.event_reported
			&& state == fp_dev->fd_gpio.last_gpio_state)
		return;

	pr_debug("gpio %d: report state %d current_time %lu uS\n",
		fp_dev->fd_gpio.gpio, state,
		(unsigned long)ktime_to_us(ktime_get()));

	fp_dev->fd_gpio.event_reported = 1;
	fp_dev->fd_gpio.last_gpio_state = state;

	event.state = state;
	event.touch_valid = false;
	event.timestamp = ktime_to_timespec64(ktime_get());
	qbt_fd_report_event(fp_dev, &event);
}

static void qbt_gpio_work_func(struct work_struct *work)
{
	int state;
	struct fp_device *fp_dev;

	if (!work) {
		pr_err("NULL pointer passed\n");
		return;
	}

	fp_dev = container_of(work, struct fp_device, fd_gpio.work);

	state = (gpio_get_value(fp_dev->fd_gpio.gpio) ?
			QBT_EVENT_FINGER_DOWN : QBT_EVENT_FINGER_UP)
			^ fp_dev->fd_gpio.active_low;

	qbt_gpio_report_event(fp_dev, state);
	pm_relax(fp_dev->device);
}

static irqreturn_t qbt_gpio_isr(int irq, void *dev_id)
{
	struct fp_device *fp_dev = dev_id;

	if (!fp_dev) {
		pr_err("NULL pointer passed\n");
		return IRQ_HANDLED;
	}

	if (irq != fp_dev->fd_gpio.irq) {
		pr_warn("invalid irq %d (expected %d)\n",
			irq, fp_dev->fd_gpio.irq);
		return IRQ_HANDLED;
	}

	pr_debug("FD event received at time %lu uS\n",
			(unsigned long)ktime_to_us(ktime_get()));

	pm_stay_awake(fp_dev->device);
	schedule_work(&fp_dev->fd_gpio.work);

	return IRQ_HANDLED;
}

static int setup_fd_gpio_irq(struct fp_device *fp_dev)
{
	int rc = 0;
	int irq;
	const char *desc = "qbt_finger_detect";

	if (!fp_dev->is_wuhb_connected) {
		pr_err("Skipping as WUHB_INT is disconnected\n");
		goto end;
	}

	pr_debug("irq %d gpio %d\n",
			fp_dev->fd_gpio.irq, fp_dev->fd_gpio.gpio);

	rc = devm_gpio_request_one(&fp_dev->driver_device->dev, fp_dev->fd_gpio.gpio,
		GPIOF_IN, desc);
	if (rc < 0) {
		pr_err("failed to request gpio %d, error %d\n",
			fp_dev->fd_gpio.gpio, rc);
		goto end;
	}


	irq = gpio_to_irq(fp_dev->fd_gpio.gpio);
	if (irq < 0) {
		rc = irq;
		pr_err("unable to get irq number for gpio %d, error %d\n",
			fp_dev->fd_gpio.gpio, rc);
		goto end;
	}


	fp_dev->fd_gpio.irq = irq;
	INIT_WORK(&fp_dev->fd_gpio.work, qbt_gpio_work_func);
	fp_dev->fd_gpio.work_init = true;

	rc = devm_request_any_context_irq(&fp_dev->driver_device->dev, fp_dev->fd_gpio.irq,
		qbt_gpio_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		desc, fp_dev);

	if (rc < 0) {
		pr_err("unable to claim irq %d; error %d\n",
			fp_dev->fd_gpio.irq, rc);
		goto end;
	}

end:
	pr_debug("Setup fd irq success, rc=%d\n", rc);
	return rc;
}

static ssize_t set_intr2_fd(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
#ifndef FP_ULTRA_QCOM_P11
	struct fp_device *fp_dev = &fp;
	dev_info(fp_dev->device, "%s -> %s\n", __func__, buf);

	if (!strncmp(buf, "0", strlen("0"))) {
		fp_local_time_printk("sysfs: set intr2 low");
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	} else if (!strncmp(buf, "1", strlen("1"))) {
		fp_local_time_printk("sysfs: set intr2 high");
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
	} else {
		pr_err("unknown operation set 0");
	}
	return count;
#else
	return 0;
#endif
}

static ssize_t get_intr2_fd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifndef FP_ULTRA_QCOM_P11
	struct fp_device *fp_dev = &fp;
	int intr2 = gpio_get_value(fp_dev->intr2_gpio);
	pr_debug("%s -> %d\n", __func__, intr2);
	return scnprintf(buf, PAGE_SIZE, "%i\n", intr2);
#else
	return 0;
#endif
}
#else
static ssize_t set_intr2_fd(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fp_device *fp_dev = &fp;
#ifdef FP_INTR2_AP_PER_LOW
	if (fp_dev->intr2_side_config ==1) {
#else
	if (fp_dev->intr2_enable) {
#endif
		//spin_lock(&fp_dev->intr2_events_lock);
		if (!strncmp(buf, "0", strlen("0"))) {
			fp_local_time_printk("sysfs: set intr2 low");
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
		} else if (!strncmp(buf, "1", strlen("1"))) {
			fp_local_time_printk("sysfs: set intr2 high");
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_high);
		} else {
			pr_err("unknown operation set 0");
		}
		//spin_unlock(&fp_dev->intr2_events_lock);
	} else {
		pr_err("intr2 is not enable");
		return -EINVAL;
	}

	return count;
}

static ssize_t get_intr2_fd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fp_device *fp_dev = &fp;

	if (fp_dev->intr2_enable) {
		int intr2 = gpio_get_value(fp_dev->intr2_gpio);
		pr_debug("%s -> %d\n", __func__, intr2);
		return scnprintf(buf, PAGE_SIZE, "%i\n", intr2);
	} else {
		pr_err("intr2 is not enable");
		return -EINVAL;
	}
}
#endif

static DEVICE_ATTR(intr2, S_IRUSR | S_IWUSR, get_intr2_fd, set_intr2_fd);

#endif

#ifdef FP_INTR2_AP_PER_LOW
static ssize_t set_intr2_ap_per_low_fd(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fp_device *fp_dev = &fp;

	if (fp_dev->intr2_side_config == 1 && fp_dev->intr2_AP_pin_enable) {
		//spin_lock(&fp_dev->intr2_events_lock);
		if (!strncmp(buf, "0", strlen("0"))) {
			fp_local_time_printk("sysfs: set intr2 per low");
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
			fp_dev->intr2_ctrl_enable = 0;
		} else if (!strncmp(buf, "1", strlen("1"))) {
			fp_local_time_printk("sysfs: set intr2 ctrl enable");
			fp_dev->intr2_ctrl_enable = 1;
		} else {
			pr_err("unknown operation set 0");
		}
		//spin_unlock(&fp_dev->intr2_events_lock);
	} else {
		pr_err("intr2 is not enable or config to AP");
		return -EINVAL;
	}

	return count;
}

static ssize_t get_intr2_ap_per_low_fd(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fp_device *fp_dev = &fp;

	if (fp_dev->intr2_side_config == 1 && fp_dev->intr2_AP_pin_enable) {
		pr_debug("%s -> %d\n", __func__, fp_dev->intr2_ctrl_enable);
		return scnprintf(buf, PAGE_SIZE, "%i\n", fp_dev->intr2_ctrl_enable);
	} else {
		pr_err("intr2 is not enable or config to AP");
		return -EINVAL;
	}
}

static DEVICE_ATTR(intr2_ap_per_low, S_IRUSR | S_IWUSR, get_intr2_ap_per_low_fd, set_intr2_ap_per_low_fd);
#endif	// FP_INTR2_AP_PER_LOW

#ifndef FP_ULTRA_QCOM
static ssize_t irq_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fp_device *fp_dev = &fp;
	int irq = gpio_get_value(fp_dev->irq_gpio);

	return scnprintf(buf, PAGE_SIZE, "%i\n", irq);
}

static ssize_t irq_ack(struct device *dev,
	       	struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fp_device *fp_dev = &fp;

	pr_debug("%s %d\n", __func__, fp_dev->irq_num);

	return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);
#endif

#ifndef CONFIG_FOD_FINGERPRINT
static ssize_t get_fingerdown_event(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fp_device *fp_dev = &fp;

	return snprintf(buf, PAGE_SIZE, "%d\n", fp_dev->fingerdown);
}

static ssize_t set_fingerdown_event(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct fp_device *fp_dev = &fp;

	dev_info(fp_dev->device, "%s -> %s\n", __func__, buf);
	if (!strncmp(buf, "1", strlen("1"))) {
		fp_dev->fingerdown = 1;
#ifdef FP_ULTRA_QCOM_P11
		fp_local_time_printk("set fingerdown 1");
#else
		dev_info(dev, "%s set fingerdown 1 \n", __func__);
#endif
		sysfs_notify(&fp_dev->driver_device->dev.kobj, NULL, "fingerdown");
	}
	else if (!strncmp(buf, "0", strlen("0"))) {
		fp_dev->fingerdown = 0;
#ifdef FP_ULTRA_QCOM_P11
		fp_local_time_printk("set fingerdown 0");
#else
		dev_info(dev, "%s set fingerdown 0 \n", __func__);
#endif
	}
	else {
#ifdef FP_ULTRA_QCOM_P11
		fp_local_time_printk("failed to set fingerdown");
#else
		dev_err(dev,"failed to set fingerdown\n");
#endif
		return -EINVAL;
	}
	return count;
}
static DEVICE_ATTR(fingerdown, S_IRUSR | S_IWUSR, get_fingerdown_event, set_fingerdown_event);
#endif

static struct attribute *attributes[] = {
#ifdef FP_ULTRA_QCOM
	&dev_attr_intr2.attr,
	&dev_attr_fingerdown.attr,
#endif
#ifdef CONFIG_SIDE_FINGERPRINT
	&dev_attr_irq.attr,
	&dev_attr_fingerdown.attr,
#endif
#ifdef FP_INTR2_AP_PER_LOW
	&dev_attr_intr2_ap_per_low.attr,
#endif
#ifdef CONFIG_FOD_FINGERPRINT
		&dev_attr_irq.attr,
#endif
	NULL
};

static const struct attribute_group attribute_group = {
	.attrs = attributes,
};

static irqreturn_t fp_irq(int irq, void *handle)
{
	struct fp_device *fp_dev = (struct fp_device *)handle;
#ifdef CONFIG_SIDE_FINGERPRINT
	sysfs_notify(&fp_dev->driver_device->dev.kobj, NULL, dev_attr_irq.attr.name);
#endif
	fp_acquire_stay_wake_time_ms(fp_dev, WAKELOCK_HOLD_TIME);
	fp_netlink_send(fp_dev, FP_NETLINK_IRQ);
	if ((fp_dev->wait_finger_down == true) && (fp_dev->fb_black == 1)) {
	       	fp_dev->wait_finger_down = false;
	}
	return IRQ_HANDLED;
}

/* -------------------------------------------------------------------- */
/* file operation function                                              */
/* -------------------------------------------------------------------- */

static long fp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct fp_device *fp_dev = &fp;
	struct fp_key fp_key;
	int retval = 0;
	u8 buf = 0;
	u8 netlink_route = fp_dev->fp_netlink_num;
	struct fp_ioc_chip_info info;
	char vendor_name[10];
	u_int32_t rst_time[2];

	FUNC_ENTRY();
	if (_IOC_TYPE(cmd) != FP_IOC_MAGIC)
		return -EINVAL;

	if (_IOC_DIR(cmd) & _IOC_READ)
		retval =!access_ok( (void __user *)arg,_IOC_SIZE(cmd));
	if (retval == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		retval =!access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (retval)
		return -EINVAL;

	switch (cmd) {
		case FP_IOC_INIT:
			pr_debug( "FP_IOC_INIT ======\n" );
			if(fp_dev->fp_netlink_num <= 0){
				pr_err("netlink init fail,check dts config.");
				retval = -EFAULT;
				break;
			}
			if(fp_dev->fp_netlink_enabled == 0) {
				retval = fp_netlink_init(fp_dev);
				if(retval != 0) {
					break;
				}
			}
			if(copy_to_user((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
				retval = -EFAULT;
				break;
			}
			fp_dev->fp_netlink_enabled = 1;
			break;

		case FP_IOC_EXIT:
			pr_debug( "FP_IOC_EXIT ======\n" );
			fp_disable_irq(fp_dev);
			if(fp_dev->fp_netlink_enabled)
				fp_netlink_destroy(fp_dev);
			fp_dev->fp_netlink_enabled = 0;
			fp_dev->device_available = 0;
			break; 

		case FP_IOC_ENABLE_IRQ:
			pr_debug( "FP_IOC_ENABLE_IRQ ======\n" );
			fp_enable_irq(fp_dev);
			break;

		case FP_IOC_DEV_INFO:
			pr_debug( "FP_IOC_DEV_INFO ======\n" );
			if(copy_to_user((char __user *)arg, (char *)fp_dev->vendor_names, 30*sizeof(char))) {
				pr_debug( "FP_IOC_DEV_INFO failed!\n" );
				retval = -EFAULT;
				break;
			}
			break;

		case FP_IOC_DISABLE_IRQ:
			pr_debug( "FP_IOC_DISABLE_IRQ ======\n" );
			fp_disable_irq(fp_dev);
			break;

#ifdef FP_ULTRA_QCOM_P11
		case FP_IOC_RESET:
			pr_debug( "FP_IOC_RESET  ======\n" );
			if (fp_dev->reset_pin_enable) {
				fp_hw_reset(fp_dev);
			} else {
				retval = -EFAULT;
				pr_err("reset pin is not init\n");
			}
			break;

		case FP_IOC_RESET_TIME_MS:
			pr_debug( "FP_IOC_RESET_TIME_MS  ======\n" );
			if (fp_dev->reset_pin_enable) {
				if(copy_from_user((u_int32_t *)rst_time, (u_int32_t *)arg, 2*sizeof(u_int32_t))) {
					pr_info("%s: FP_IOC_RESET_TIME_MS failed.\n", __func__);
					retval = -EFAULT;
					break;
				}
				if (rst_time[0] == 0 || rst_time[1] == 0) {
					pr_err("FP_IOC_RESET_TIME_MS: low_time or high_time is 0.\n");
					retval = -EFAULT;
					break;
				} else {
					fp_hw_reset_delay(fp_dev, rst_time[0], rst_time[1]);
				}
			} else {
				retval = -EFAULT;
				pr_err("reset pin is not init\n");
			}
			break;

		case FP_IOC_RESET_OUT_LOW:
			pr_debug( "FP_IOC_RESET_OUT_LOW  ======\n" );
			if (fp_dev->reset_pin_enable) {
				pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
			} else {
				retval = -EFAULT;
				pr_err("reset pin is not init\n");
			}
			break;
#else
		case FP_IOC_RESET:
			pr_debug( "FP_IOC_RESET  ======\n" );
			fp_hw_reset(fp_dev);
			break;
		
		case FP_IOC_RESET_TIME_MS:
			pr_debug( "FP_IOC_RESET_TIME_MS  ======\n" );
			if(copy_from_user((u_int32_t *)rst_time, (u_int32_t *)arg, 2*sizeof(u_int32_t))) {
				pr_info("%s: FP_IOC_RESET_TIME_MS failed.\n", __func__);
				retval = -EFAULT;
				break;
			}
			if (rst_time[0] == 0 || rst_time[1] == 0) {
				pr_err("FP_IOC_RESET_TIME_MS: low_time or high_time is 0.\n");
				retval = -EFAULT;
				break;
			} else {
				fp_hw_reset_delay(fp_dev, rst_time[0], rst_time[1]);
			}
			break;

		case FP_IOC_RESET_OUT_LOW:
			pr_debug( "FP_IOC_RESET_OUT_LOW  ======\n" );
			pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
			break;
#endif

		case FP_IOC_ENABLE_POWER:
			pr_debug( "FP_IOC_ENABLE_POWER ======\n" );
			if(copy_from_user((char *)vendor_name, (char *)arg, 10*sizeof(char))) {
				pr_info("%s: FP_IOC_ENABLE_POWER failed.\n", __func__);
				retval = -EFAULT;
				break;
			}
			if (!strcmp(vendor_name, POWER_LDO_3V3)) {
				fp_power_on(&fp_dev->vreg_3v3);
			} else if (!strcmp(vendor_name, POWER_LDO_1V8)) {
				fp_power_on(&fp_dev->vreg_1v8);
			} else if (!strcmp(vendor_name, POWER_VDDIO)) {
				fp_power_on(&fp_dev->vreg_gpio);
			} else {
				pr_err("%s is unknown vendor name", vendor_name);
				retval = -EPERM;
			}
			break;

		case FP_IOC_DISABLE_POWER:
			pr_debug( "FP_IOC_DISABLE_POWER ======\n" );
			if(copy_from_user((char *)vendor_name, (char *)arg, 10*sizeof(char))) {
				pr_info("%s: FP_IOC_DISABLE_POWER failed.\n", __func__);
				retval = -EFAULT;
				break;
			}
			if (!strcmp(vendor_name, "vreg3v3")) {
				fp_power_off(&fp_dev->vreg_3v3);
			} else if (!strcmp(vendor_name, "vreg1v8")) {
				fp_power_off(&fp_dev->vreg_1v8);
			} else if (!strcmp(vendor_name, "pwr_gpio")) {
				fp_power_off(&fp_dev->vreg_gpio);
			} else {
				pr_err("%s is unknown vendor name", vendor_name);
				retval = -EPERM;
			}
			break;

		case FP_IOC_ENABLE_SPI_CLK:
			pr_debug( "FP_IOC_ENABLE_SPI_CLK ======\n" );
#ifdef CONFIG_FP_MTK_PLATFORM
			pr_debug( "FP_IOC_ENABLE_SPI_CLK ======\n" );
			fp_spi_clk_enable(fp_dev->driver_device);
#endif
			break;
		case FP_IOC_DISABLE_SPI_CLK:
			pr_debug( "FP_IOC_DISABLE_SPI_CLK ======\n" );
#ifdef CONFIG_FP_MTK_PLATFORM
			pr_debug( "FP_IOC_DISABLE_SPI_CLK ======\n" );
			fp_spi_clk_disable(fp_dev->driver_device);
#endif
			break;

		case FP_IOC_INPUT_KEY_EVENT:
			pr_debug( "FP_IOC_INPUT_KEY_EVENT ======\n" );
			if (copy_from_user(&fp_key, (struct fp_key *)arg, sizeof(struct fp_key))) {
				pr_debug("Failed to copy input key event from user to kernel\n");
				retval = -EFAULT;
				break;
			}
			fp_kernel_key_input(fp_dev, &fp_key);
			break;

		case FP_IOC_ENTER_SLEEP_MODE:
			pr_debug( "FP_IOC_ENTER_SLEEP_MODE ======\n" );
			break;
		case FP_IOC_GET_FW_INFO:
			pr_debug( "FP_IOC_GET_FW_INFO ======\n" );
			pr_debug( "firmware info  0x%x\n" , buf);
			if (copy_to_user((void __user *)arg, (void *)&buf, sizeof(u8))) {
				pr_debug( "Failed to copy data to user\n");
				retval = -EFAULT;
			}
			break;

		case FP_IOC_REMOVE:
			pr_debug( "FP_IOC_REMOVE ======\n" );
			break;

		case FP_IOC_CHIP_INFO:
			pr_debug( "FP_IOC_CHIP_INFO ======\n" );
			if (copy_from_user(&info, (struct fp_ioc_chip_info *)arg,sizeof(struct fp_ioc_chip_info))) {
				retval = -EFAULT;
				break;
			}
			pr_debug( " vendor_id 0x%x\n" ,info.vendor_id);
			pr_debug( " mode 0x%x\n" , info.mode);
			pr_debug( " operation 0x%x\n" , info.operation);
			break;

		case FP_IOC_REQUEST_RESOURCE:
			pr_debug( "FP_IOC_REQUEST_RESOURCE ======\n" );
			if (!fp_dev->irq_request) {
				gpio_direction_input(fp_dev->irq_gpio);
				int ret = request_threaded_irq(fp_dev->irq_num, NULL, fp_irq,
							IRQF_TRIGGER_RISING |
							IRQF_ONESHOT, "xiaomi_fp_irq",
							fp_dev);
				if (!ret){
					pr_debug( "irq thread request success!\n");
					fp_dev->irq_request = true;
					//fp_disable_irq(fp_dev);
				} else {
					retval = -EFAULT;
					pr_err("irq thread request failed, status = %d\n", ret);
				}
			} else {
				retval = -EFAULT;
				pr_err("irq thread already request\n");
			}
			break;

		case FP_IOC_RELEASE_RESOURCE:
			pr_debug( "FP_IOC_RELEASE_RESOURCE ======\n" );
			if (fp_dev->irq_request && fp_dev->irq_num) {
				fp_disable_irq(fp_dev);
				free_irq(fp_dev->irq_num, fp_dev);
				fp_dev->irq_request = false;
			} else {
				retval = -EFAULT;
				pr_err("irq free failed, irq_num = %d\n", fp_dev->irq_num);
			}
			break;

#ifdef FP_ULTRA_QCOM
		case FP_IOC_REQUEST_IPC:
			pr_debug( "FP_IOC_REQUEST_IPC ======\n" );
			if (!fp_dev->fw_ipc.work_init) {
				int ret = setup_ipc_irq(fp_dev);
				if (ret < 0) {
					retval = -EFAULT;
					pr_err("setup_ipc_irq failed.\n");
					break;
				}
				fp_dev->fw_ipc.irq_enabled = false;
				disable_irq(fp_dev->fw_ipc.irq);
			} else {
				retval = -EFAULT;
				pr_err("fw_ipc thread already request.\n");
			}
			break;
		case FP_IOC_FREE_IPC:
			pr_debug( "FP_IOC_FREE_IPC ======\n" );
			if (fp_dev->fw_ipc.work_init) {
				free_ipc_irq(fp_dev);
			} else {
				retval = -EFAULT;
				pr_err("fw_ipc thread is not request.\n");
			}
			break;

#ifndef FP_ULTRA_QCOM_P11
		case FP_IOC_REQUEST_WUHB:
			pr_debug( "FP_IOC_REQUEST_WUHB ======\n" );
			if (!fp_dev->fd_gpio.work_init) {
				int ret = setup_fd_gpio_irq(fp_dev);
				if (ret < 0) {
					retval = -EFAULT;
					pr_err("setup_fd_gpio_irq failed\n");
					break;
				}
				fp_dev->fd_gpio.irq_enabled = false;
				disable_irq(fp_dev->fd_gpio.irq);
			} else {
				retval = -EFAULT;
				pr_err("fd_gpio thread already request\n");
			}
			break;

		case FP_IOC_FREE_WUHB:
			pr_debug( "FP_IOC_FREE_WUHB ======\n" );
			fp_dev->fd_gpio.work_init = false;
			break;
#endif

		case QBT_ENABLE_IPC:
			pr_debug( "QBT_ENABLE_IPC ======\n" );
			if (!fp_dev->fw_ipc.irq_enabled) {
				enable_irq(fp_dev->fw_ipc.irq);
				fp_dev->fw_ipc.irq_enabled = true;
			}
			break;

		case QBT_DISABLE_IPC:
			pr_debug( "QBT_DISABLE_IPC ======\n" );
			if (fp_dev->fw_ipc.irq_enabled) {
				disable_irq(fp_dev->fw_ipc.irq);
				fp_dev->fw_ipc.irq_enabled = false;
				pr_debug("%s: QBT_DISABLE_IPC\n", __func__);
			}
			break;

#ifndef FP_ULTRA_QCOM_P11
		case QBT_ENABLE_FD:
			pr_debug( "QBT_ENABLE_FD ======\n" );
			if (fp_dev->is_wuhb_connected &&
					!fp_dev->fd_gpio.irq_enabled) {
				enable_irq(fp_dev->fd_gpio.irq);
				fp_dev->fd_gpio.irq_enabled = true;
				pr_debug("%s: QBT_ENABLE_FD\n", __func__);
			}
			break;

		case QBT_DISABLE_FD:
			pr_debug( "QBT_DISABLE_FD ======\n" );
			if (fp_dev->is_wuhb_connected &&
					fp_dev->fd_gpio.irq_enabled) {
				disable_irq(fp_dev->fd_gpio.irq);
				fp_dev->fd_gpio.irq_enabled = false;
			}
			break;

		case QBT_IS_WUHB_CONNECTED:
			pr_debug( "QBT_IS_WUHB_CONNECTED ======\n" );
			struct qbt_wuhb_connected_status wuhb_connected_status;

			memset(&wuhb_connected_status, 0,
					sizeof(wuhb_connected_status));
			wuhb_connected_status.is_wuhb_connected =
					fp_dev->is_wuhb_connected;
			retval = copy_to_user((void __user *)arg,
					&wuhb_connected_status,
					sizeof(wuhb_connected_status));

			if (retval != 0) {
				pr_err("Failed to copy wuhb connected status: %d\n",
						retval);
				retval = -EFAULT;
			}
			break;
#endif

		case QBT_ACQUIRE_WAKELOCK:
			pr_debug( "QBT_ACQUIRE_WAKELOCK ======\n" );
			if (atomic_read(&fp_dev->wakelock_acquired) == 0) {
				pr_debug("Acquiring wakelock\n");
				fp_acquire_stay_wake(fp_dev);
			}
			atomic_inc(&fp_dev->wakelock_acquired);
			break;

		case QBT_RELEASE_WAKELOCK:
			pr_debug( "QBT_RELEASE_WAKELOCK ======\n" );
			if (atomic_read(&fp_dev->wakelock_acquired) == 0)
				break;
			if (atomic_dec_and_test(&fp_dev->wakelock_acquired)) {
				pr_debug("Releasing wakelock\n");
				fp_release_stay_wake(fp_dev);
			}
			break;

#ifdef FP_ULTRA_QCOM_P11
		case FP_IOC_DISABLE_INTR2:
			pr_debug( "FP_IOC_DISABLE_INTR2 ======\n" );
			if (fp_dev->intr2_enable) {
				disable_intr2_report(fp_dev);
			} else {
				retval = -EINVAL;
				pr_err("intr2 is not init\n");
			}
			break;

		case FP_IOC_REQUEST_INTR2:
			pr_debug( "FP_IOC_REQUEST_INTR2 ======\n" );
			if (fp_dev->intr2_enable) {
				int rc = enable_intr2_report(fp_dev);
				if (rc < 0) {
					retval = -EFAULT;
					pr_err("intr2 request fail\n");
				}
			} else {
				retval = -EINVAL;
				pr_err("intr2 is not init\n");
			}
			break;
#else
		case FP_IOC_DISABLE_INTR2:
			pr_debug( "FP_IOC_DISABLE_INTR2 ======\n" );
			disable_intr2_report(fp_dev);
			break;
		case FP_IOC_REQUEST_INTR2:
			pr_debug( "FP_IOC_REQUEST_INTR2 ======\n" );
			if (gpio_is_valid(fp_dev->intr2_gpio)) {
				int rc = enable_intr2_report(fp_dev);
				if (rc < 0) {
					retval = -EFAULT;
					pr_err("intr2 request fail\n");
				}
			}
			break;
#endif
		case FP_IOC_INTR2_SIDE_CONFIG:
			pr_debug( "FP_IOC_INTR2_SIDE_CONFIG ======\n" );
			pr_debug( "fp_dev->intr2_side_config = %d\n", fp_dev->intr2_side_config);
			if(copy_to_user((u_int32_t __user *)arg, &fp_dev->intr2_side_config, sizeof(fp_dev->intr2_side_config))) {
				pr_debug( "FP_IOC_INTR2_SIDE_CONFIG failed!\n" );
				retval = -EFAULT;
				break;
			}
			break;
#endif
		default:
			pr_debug( "fp doesn't support this command(%x)\n", cmd);
			break;
	}

	return retval;
}

#ifdef CONFIG_COMPAT
static long fp_compat_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	int retval = 0;
	FUNC_ENTRY();
	retval = filp->f_op->unlocked_ioctl(filp, cmd, arg);
	return retval;
}
#endif

/* -------------------------------------------------------------------- */
/* device function							*/
/* -------------------------------------------------------------------- */
static int fp_open(struct inode *inode, struct file *filp)
{
	struct fp_device *fp_dev = NULL;
	int status = -ENXIO;
	int minor_no = -1;
#ifdef FP_ULTRA_QCOM
	int rc = 0;
#endif

	FUNC_ENTRY();

	if (!inode || (!inode->i_cdev && !inode->i_rdev) || !filp) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}
	minor_no = iminor(inode);

	if (minor_no == MINOR_NUM_MIFP) {
		mutex_lock(&device_list_lock);
		list_for_each_entry(fp_dev, &device_list, device_entry) {
			if (fp_dev->devt == inode->i_rdev) {
				pr_debug("Found MIFP\n" );
				status = 0;
				break;
			}
		}
		mutex_unlock(&device_list_lock);

		if (status == 0) {
			filp->private_data = fp_dev;
			nonseekable_open(inode, filp);
#ifndef XIAOMI_DRM_INTERFACE_WA
			if (fp_dev->screen_state_wq) {
				queue_delayed_work(fp_dev->screen_state_wq, &fp_dev->screen_state_dw, 5 * HZ);
				pr_info("%s:queue_delayed_work\n", __func__);
			}
#endif
		fp_dev->device_available = 1;
		}
	}

	else if (minor_no == MINOR_NUM_MIFP_ID) {
		pr_debug("Found MIFP_ID\n" );

		fp_dev = container_of(inode->i_cdev,
					struct fp_device, fpid_cdev);

		filp->private_data = fp_dev;

		status = 0;
	}

#ifdef FP_ULTRA_QCOM
	else if (minor_no == MINOR_NUM_IPC) {
		pr_debug("Found QBT_IPC\n" );
		fp_dev = container_of(inode->i_cdev,
				struct fp_device, qbt_ipc_cdev);

		filp->private_data = fp_dev;
		status = 0;

#ifndef FP_ULTRA_QCOM_P11
		pr_debug("entry fd_available=%d\n", atomic_read(&fp_dev->fd_available));
#endif

		/* disallowing concurrent opens */
		if (!atomic_dec_and_test(&fp_dev->ipc_available)) {
			atomic_inc(&fp_dev->ipc_available);
			rc = -EBUSY;
		}

#ifndef FP_ULTRA_QCOM_P11
		pr_debug("exit rc = %d, fd_available=%d\n",
				rc, atomic_read(&fp_dev->fd_available));
	}

	else if (minor_no == MINOR_NUM_FD) {
		pr_debug("Found QBT_FD\n" );
		fp_dev = container_of(inode->i_cdev,
				struct fp_device, qbt_fd_cdev);

		filp->private_data = fp_dev;
		status = 0;

		pr_debug("entry fd_available=%d\n", atomic_read(&fp_dev->fd_available));

		/* disallowing concurrent opens */
		if (!atomic_dec_and_test(&fp_dev->fd_available)) {
			atomic_inc(&fp_dev->fd_available);
			rc = -EBUSY;
		}
		pr_debug("exit rc = %d, fd_available=%d\n",
				rc, atomic_read(&fp_dev->fd_available));
#endif
	}
#endif

	else {
		pr_err("Invalid minor number %d\n", minor_no);
		return -EINVAL;
	}

	return status;
}

static int fp_release(struct inode *inode, struct file *filp)
{
	struct fp_device *fp_dev = NULL;
	int minor_no = -1;

	FUNC_ENTRY();

	if (!filp || !filp->private_data || !inode) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}

	fp_dev = filp->private_data;
	minor_no = iminor(inode);

	if (minor_no == MINOR_NUM_MIFP) {
		pr_debug("Found MIFP\n" );
		if (fp_dev->irq_request && fp_dev->irq_num) {
			fp_disable_irq(fp_dev);
			free_irq(fp_dev->irq_num, fp_dev);
			fp_dev->irq_request = false;
		}
#ifndef XIAOMI_DRM_INTERFACE_WA
	if (fp_dev->screen_state_wq) {
		cancel_delayed_work_sync(&fp_dev->screen_state_dw);
		pr_info("%s:cancel_delayed_work_sync\n", __func__);
	}
#endif
	fp_dev->device_available = 0;
	}
	else if (minor_no == MINOR_NUM_MIFP_ID) {
		pr_debug("Found MIFP_ID\n" );
	}

#ifdef FP_ULTRA_QCOM
#ifndef FP_ULTRA_QCOM_P11
	else if (minor_no == MINOR_NUM_IPC || minor_no == MINOR_NUM_FD) {
		pr_debug("entry fd_available=%d\n", atomic_read(&fp_dev->fd_available));
		if (minor_no == MINOR_NUM_FD) {
			pr_debug("Found QBT_FD\n");
			atomic_inc(&fp_dev->fd_available);
		} else if (minor_no == MINOR_NUM_IPC) {
			pr_debug("Found QBT_IPC\n");
			atomic_inc(&fp_dev->ipc_available);
			if (fp_dev->fw_ipc.work_init) {
				free_ipc_irq(fp_dev);
			}
#else
	else if (minor_no == MINOR_NUM_IPC) {
		pr_debug("Found QBT_IPC\n");
		atomic_inc(&fp_dev->ipc_available);
		if (fp_dev->fw_ipc.work_init) {
			free_ipc_irq(fp_dev);
#endif
		}

		if (atomic_read(&fp_dev->wakelock_acquired) != 0) {
			pr_debug("Releasing wakelock\n");
			pm_relax(fp_dev->device);
			atomic_set(&fp_dev->wakelock_acquired, 0);
		}
#ifndef FP_ULTRA_QCOM_P11
		pr_debug("exit fd_available=%d\n", atomic_read(&fp_dev->fd_available));
#endif
	}
#endif
	else {
		pr_err("Invalid minor number %d\n", minor_no);
	}
	return 0;
}

#ifdef FP_ULTRA_QCOM
static ssize_t fp_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	struct ipc_event fw_event;
	struct fp_device *fp_dev;
	wait_queue_head_t *read_wait_queue = NULL;
#ifndef FP_ULTRA_QCOM_P11
	struct fd_event *fd_evt;
	struct fd_userspace_buf *scratch_buf;
	int i = 0;
#endif
	int minor_no = -1;
	int fifo_len = 0;
	ssize_t num_bytes = 0;

	if (!filp || !filp->private_data) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}
	fp_dev = filp->private_data;

	minor_no = iminor(filp->f_path.dentry->d_inode);

	pr_debug("entry with numBytes = %zd, minor_no = %d\n", cnt, minor_no);

#ifndef FP_ULTRA_QCOM_P11
	scratch_buf = &fp_dev->scrath_buf;
	memset(scratch_buf, 0, sizeof(*scratch_buf));

	if (minor_no == MINOR_NUM_FD) {
		if (cnt < sizeof(*scratch_buf)) {
			pr_err("Num bytes to read is too small\n");
			return -EINVAL;
		}
		read_wait_queue = &fp_dev->read_wait_queue_fd;
	} else if (minor_no == MINOR_NUM_IPC) {
#else
	if (minor_no == MINOR_NUM_IPC) {
#endif
		if (cnt < sizeof(fw_event.ev)) {
			pr_err("Num bytes to read is too small\n");
			return -EINVAL;
		}
		read_wait_queue = &fp_dev->read_wait_queue_ipc;
	} else {
		pr_err("Invalid minor number\n");
		return -EINVAL;
	}

	fifo_len = get_events_fifo_len_locked(fp_dev, minor_no);
	while (fifo_len == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			pr_debug("fw_events fifo: empty, returning\n");
			return -EAGAIN;
		}
		pr_debug("fw_events fifo: empty, waiting\n");
		if (wait_event_interruptible(*read_wait_queue,
				(get_events_fifo_len_locked(
				fp_dev, minor_no) > 0)))
			return -ERESTARTSYS;
		fifo_len = get_events_fifo_len_locked(fp_dev, minor_no);
	}

#ifndef FP_ULTRA_QCOM_P11
	if (minor_no == MINOR_NUM_FD) {
		mutex_lock(&fp_dev->fd_events_mutex);

		scratch_buf->num_events = kfifo_len(&fp_dev->fd_events);

		for (i = 0; i < scratch_buf->num_events; i++) {
			fd_evt = &scratch_buf->fd_events[i];
			if (!kfifo_get(&fp_dev->fd_events, fd_evt)) {
				pr_err("FD event fifo: err popping item\n");
				scratch_buf->num_events = i;
				break;
			}
			pr_debug("Reading event id: %d state: %d\n",
					fd_evt->id, fd_evt->state);
			pr_debug("x: %d y: %d timestamp: %lld.%03ld\n",
					fd_evt->X, fd_evt->Y,
					fd_evt->timestamp.tv_sec,
					fd_evt->timestamp.tv_nsec);
		}
		pr_debug("%d FD events read at time %lu uS\n",
				scratch_buf->num_events,
				(unsigned long)ktime_to_us(ktime_get()));
		num_bytes = copy_to_user(ubuf, scratch_buf,
				sizeof(*scratch_buf));
		mutex_unlock(&fp_dev->fd_events_mutex);
	} else if (minor_no == MINOR_NUM_IPC) {
#else
	if (minor_no == MINOR_NUM_IPC) {
#endif
		mutex_lock(&fp_dev->ipc_events_mutex);
		if (!kfifo_get(&fp_dev->ipc_events, &fw_event))
			pr_err("IPC events fifo: error removing item\n");
		pr_debug("IPC event %d at minor no %d read at time %lu uS\n",
				(int)fw_event.ev, minor_no,
				(unsigned long)ktime_to_us(ktime_get()));
		num_bytes = copy_to_user(ubuf, &fw_event.ev,
				sizeof(fw_event.ev));
		mutex_unlock(&fp_dev->ipc_events_mutex);
	} else {
		pr_err("Invalid minor number\n");
	}
	if (num_bytes != 0)
		pr_warn("Could not copy %ld bytes\n", num_bytes);
	return num_bytes;
}

static __poll_t fp_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	struct fp_device *fp_dev;
	__poll_t mask = 0;
	int minor_no = -1;

	if (!filp || !filp->private_data) {
		pr_err("NULL pointer passed\n");
		return -EINVAL;
	}
	fp_dev = filp->private_data;

	minor_no = iminor(filp->f_path.dentry->d_inode);

	pr_debug("fp_read enter, minor_no = %d\n", minor_no);

#ifndef FP_ULTRA_QCOM_P11
	if (minor_no == MINOR_NUM_FD) {
		poll_wait(filp, &fp_dev->read_wait_queue_fd, wait);
		if (kfifo_len(&fp_dev->fd_events) > 0)
			mask |= (POLLIN | POLLRDNORM);
	} else if (minor_no == MINOR_NUM_IPC) {
#else
	if (minor_no == MINOR_NUM_IPC) {
#endif
		poll_wait(filp, &fp_dev->read_wait_queue_ipc, wait);
		if (kfifo_len(&fp_dev->ipc_events) > 0)
			mask |= (POLLIN | POLLRDNORM);
	} else {
		pr_err("Invalid minor number\n");
		return -EINVAL;
	}

	return mask;
}
#endif

static const struct file_operations fp_fops = {
	.owner = THIS_MODULE,
	.open = fp_open,
	.unlocked_ioctl = fp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fp_compat_ioctl,
#endif
	.release = fp_release,
#ifdef FP_ULTRA_QCOM
	.poll = fp_poll,
	.read = fp_read,
#endif
};

#ifdef FP_ULTRA_QCOM
int fp_ultra_dev_register(void)
{
	struct fp_device *fp_dev = &fp;
	struct device *device;
    int status = 0;
	int ret = 0;
    FUNC_ENTRY();

#ifndef FP_ULTRA_QCOM_P11
    /* qbt_fd_cdev init and add */
	cdev_init(&fp_dev->qbt_fd_cdev, &fp_fops);
	fp_dev->qbt_fd_cdev.owner = THIS_MODULE;
	status = cdev_add(&fp_dev->qbt_fd_cdev,
			MKDEV(MAJOR(fp_dev->devt), MINOR_NUM_FD), 1);
	if (status) {
		pr_debug( "Failed to add qbt_fd_cdev.\n" );
		goto err_fd_cdev;
	}
#endif
	/* qbt_ipc_cdev init and add */
	cdev_init(&fp_dev->qbt_ipc_cdev, &fp_fops);
	fp_dev->qbt_ipc_cdev.owner = THIS_MODULE;
	status = cdev_add(&fp_dev->qbt_ipc_cdev,
			MKDEV(MAJOR(fp_dev->devt), MINOR_NUM_IPC), 1);
	if (status) {
		pr_debug( "Failed to add qbt_ipc_cdev.\n" );
		goto err_ipc_cdev;
	}

#ifndef FP_ULTRA_QCOM_P11
	/* create qbt_fd device */
	device = device_create(fp_dev->class, NULL,
			       fp_dev->qbt_fd_cdev.dev, fp_dev,
			       FP_WUHB_DEV_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("fd device_create failed %d\n", ret);
		goto err_fd_dev_create;
	}
#endif
	/* create qbt_ipc device */
	device = device_create(fp_dev->class, NULL,
			       fp_dev->qbt_ipc_cdev.dev, fp_dev,
			       FP_IPC_DEV_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("ipc device_create failed %d\n", ret);
		goto err_ipc_dev_create;
	}

	pr_debug("fp_ultra_dev register success!");

	return 0;

err_ipc_dev_create:
#ifndef FP_ULTRA_QCOM_P11
	device_destroy(fp_dev->class, fp_dev->qbt_fd_cdev.dev);

err_fd_dev_create:
#endif
	cdev_del(&fp_dev->qbt_ipc_cdev);

err_ipc_cdev:
#ifndef FP_ULTRA_QCOM_P11
	cdev_del(&fp_dev->qbt_fd_cdev);

err_fd_cdev:
#endif
	pr_debug("fp_ultra_dev register failed = %d!", status);
    return status;
}

void fp_ultra_dev_unregister(void)
{
	struct fp_device *fp_dev = &fp;

	device_destroy(fp_dev->class, fp_dev->qbt_ipc_cdev.dev);
#ifndef FP_ULTRA_QCOM_P11
	device_destroy(fp_dev->class, fp_dev->qbt_fd_cdev.dev);
#endif
	cdev_del(&fp_dev->qbt_ipc_cdev);
#ifndef FP_ULTRA_QCOM_P11
	cdev_del(&fp_dev->qbt_fd_cdev);
#endif

	pr_debug("fp_ultra_dev unregister success");
}
#endif

static struct fp_key_map maps[] = {
        {EV_KEY, FP_KEY_INPUT_HOME},
        {EV_KEY, FP_KEY_INPUT_MENU},
        {EV_KEY, FP_KEY_INPUT_BACK},
        {EV_KEY, FP_KEY_INPUT_POWER},
        {EV_KEY, FP_KEY_DOUBLE_CLICK},
};

int fp_create_input_device(void)
{
	struct fp_device *fp_dev = &fp;
	int status = 0;

	/*register device within input system. */
	fp_dev->input = input_allocate_device();
	if (fp_dev->input == NULL) {
		pr_debug( "Failed to allocate input device.\n");
		status = -ENOMEM;
		goto err_input;
	}

	fp_dev->input->name = FP_INPUT_NAME;
	fp_dev->input->id.vendor  = 0x0666;
	fp_dev->input->id.product = 0x0888;

	for (int i = 0; i < ARRAY_SIZE(maps); i++) {
		input_set_capability(fp_dev->input, maps[i].type, maps[i].code);
	}

	if (input_register_device(fp_dev->input)) {
		pr_debug( "Failed to register input device.\n");
		status = -ENODEV;
		goto err_input_2;
	}
	return 0;

err_input_2:
	if (fp_dev->input != NULL) {
		input_free_device(fp_dev->input);
		fp_dev->input = NULL;
	}

err_input:
	return status;
}

void fp_free_input_device(void)
{
	struct fp_device *fp_dev = &fp;

	if (fp_dev->input != NULL) {
		input_free_device(fp_dev->input);
		fp_dev->input = NULL;
	}
}

#ifdef CONFIG_FP_MTK_PLATFORM
static int fp_probe(struct spi_device *driver_device)
#else
static int fp_probe(struct platform_device *driver_device)
#endif
{
	struct fp_device *fp_dev = &fp;
	int status = -EINVAL;
	int ret = 0;
	struct device *device;
	FUNC_ENTRY();

	INIT_LIST_HEAD(&fp_dev->device_entry);

	/* setup fp configurations. */
	fp_dev->irq_gpio = -EINVAL;
	fp_dev->irq_enabled = 1;
	fp_dev->device_available = 0;
	fp_dev->fb_black = 0;
	fp_dev->wait_finger_down = false;
	fp_dev->fp_netlink_enabled = 0;
	fp_dev->fingerdown = 0;
	fp_dev->irq_request = false;
	fp_dev->driver_device = driver_device;
	fp_dev->intr2_side_config = 0;
#ifdef FP_INTR2_AP_PER_LOW
	fp_dev->intr2_AP_pin_enable = false;
#endif
#ifdef FP_ULTRA_QCOM_P11
	fp_dev->intr2_enable = false;
	fp_dev->reset_pin_enable = false;
#endif
	/* config fp power. */
	fp_power_config(fp_dev);
	/* get gpio info from dts or defination */
	status = fp_parse_dts(fp_dev);
	if (status) {
		goto err_dts;
	}
#ifdef CONFIG_FP_MTK_PLATFORM
#ifdef FP_ULTRA_QCOM_P11
	if (!IS_ERR(fp_dev->pins_int_default)) {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_int_default);
	}
#else
	if (!IS_ERR(fp_dev->pins_eint_default)) {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_eint_default);
	}
#endif

	if (IS_ERR(fp_dev->pins_spiio_spi_mode)) {
		ret = PTR_ERR(fp_dev->pins_spiio_spi_mode);
		pr_debug("%s fingerprint pinctrl spiio_spi_mode NULL\n",__func__);
		return ret;
	} else {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_spiio_spi_mode);
	}
#endif
	/* create class */
	fp_dev->class = class_create(FP_CLASS_NAME);
	if (IS_ERR(fp_dev->class)) {
		pr_debug( "Failed to create class.\n" );
		status = -ENODEV;
		goto err_class;
	}
	/* get device no */
	status = alloc_chrdev_region(&fp_dev->devt, 0,1, FP_DEV_NAME);
	if (status < 0) {
		pr_debug( "Failed to alloc devt.\n" );
		goto err_devno;
	}

	/* cdev init and add */
	cdev_init(&fp_dev->cdev, &fp_fops);
	fp_dev->cdev.owner = THIS_MODULE;
	status = cdev_add(&fp_dev->cdev, MKDEV(MAJOR(fp_dev->devt), MINOR_NUM_MIFP), 1);
	if (status) {
		pr_debug( "Failed to add cdev.\n" );
		goto err_cdev;
	}

	/* mifp_vendor cdev init and add */
	cdev_init(&fp_dev->fpid_cdev, &fp_fops);
	fp_dev->fpid_cdev.owner = THIS_MODULE;
	status = cdev_add(&fp_dev->fpid_cdev, MKDEV(MAJOR(fp_dev->devt), MINOR_NUM_MIFP_ID), 1);
	if (status) {
		pr_debug( "Failed to add fpid cdev.\n" );
		goto err_fpid_cdev;
	}

	/* create mifp_vendor device */
	device = device_create(fp_dev->class, NULL,
			fp_dev->fpid_cdev.dev, fp_dev,
			FP_ID_DEV_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("fpid device create failed %d\n", ret);
		goto err_fpid_device;
	}

#ifdef FP_ULTRA_QCOM
	if(fp_ultra_dev_register() != 0) {
		goto err_register_ultra_dev;
	}
#endif

	/* create device */
	fp_dev->device = device_create(fp_dev->class, &driver_device->dev, fp_dev->devt, fp_dev, FP_DEV_NAME);
	if (IS_ERR(fp_dev->device)) {
		pr_debug( "  Failed to create device.\n" );
		status = -ENODEV;
		goto err_device;
	} else {
		mutex_lock(&device_list_lock);
		list_add(&fp_dev->device_entry, &device_list);
		mutex_unlock(&device_list_lock);
	}

	/* create input device */
	if(fp_create_input_device() != 0) {
		goto err_input_device;
	}
#ifdef FP_ULTRA_QCOM
#ifndef FP_ULTRA_QCOM_P11
	INIT_KFIFO(fp_dev->fd_events);
#endif
	INIT_KFIFO(fp_dev->ipc_events);
#ifndef FP_ULTRA_QCOM_P11
	init_waitqueue_head(&fp_dev->read_wait_queue_fd);
#endif
	init_waitqueue_head(&fp_dev->read_wait_queue_ipc);
#endif
	init_waitqueue_head(&fp_dev->fp_wait_queue);
#ifndef XIAOMI_DRM_INTERFACE_WA
	fp_dev->screen_state_wq = create_singlethread_workqueue("screen_state_wq");
	if (fp_dev->screen_state_wq){
		INIT_DELAYED_WORK(&fp_dev->screen_state_dw, fp_register_panel_notifier_work);
	}
#endif
	/* netlink interface init */
	status = fp_netlink_init(fp_dev);
	if (status == -1) {
		goto err_netlink;
	}
	fp_dev->fp_netlink_enabled = 1;
	/* register fp wakeup source */
	fp_dev->wakesrc = wakeup_source_register(&fp_dev->driver_device->dev, "fp_wakesrc");
	if (fp_dev->wakesrc == NULL) {
		pr_err("fp wakeup source register failed\n");
	}
#ifndef CONFIG_SIDE_FINGERPRINT
	if (device_may_wakeup(fp_dev->device)) {
		pr_debug("device_may_wakeup\n");
		disable_irq_wake(fp_dev->irq_num);
	}
	pr_debug("Is FOD project\n");
#else
	enable_irq_wake(fp_dev->irq_num);
	pr_debug("Is sidecap fingerprint project\n");
#endif
	status = sysfs_create_group(&fp_dev->driver_device->dev.kobj, &attribute_group);
	if (status) {
		pr_debug("could not create sysfs\n");
	}
#ifdef FP_ULTRA_QCOM
	xiaomifp_evdev_init(fp_dev);
#endif
	pr_debug( "fp probe success" );
	FUNC_EXIT();
	return 0;

err_netlink:
	fp_free_input_device();

err_input_device:
	device_destroy(fp_dev->class, fp_dev->devt);
	list_del(&fp_dev->device_entry);

err_device:
#ifdef FP_ULTRA_QCOM
	fp_ultra_dev_unregister();

err_register_ultra_dev:
#endif
	device_destroy(fp_dev->class, fp_dev->fpid_cdev.dev);

err_fpid_device:
	cdev_del(&fp_dev->fpid_cdev);

err_fpid_cdev:
	cdev_del(&fp_dev->cdev);

err_cdev:
	unregister_chrdev_region(fp_dev->devt, 1);

err_devno:
	class_destroy(fp_dev->class);

err_class:
#ifdef CONFIG_FP_MTK_PLATFORM
	fp_spi_clk_disable(fp_dev->driver_device);
#endif

err_dts:
	fp_dev->driver_device = NULL;
	fp_dev->device_available = 0;
	pr_debug( "fp probe fail\n" );
	FUNC_EXIT();
	return status;
}

#ifdef CONFIG_FP_MTK_PLATFORM
static int fp_remove(struct spi_device *driver_device)
#else
static int fp_remove(struct platform_device *driver_device)
#endif
{
	struct fp_device *fp_dev = &fp;
	FUNC_ENTRY();
	/* unregister fp wakeup source */
	wakeup_source_unregister(fp_dev->wakesrc);
	fp_dev->wakesrc = NULL;
	/* make sure ops on existing fds can abort cleanly */
	if (fp_dev->irq_num) {
		free_irq(fp_dev->irq_num, fp_dev);
		fp_dev->irq_enabled = 0;
		fp_dev->irq_num = 0;
	}
	fp_dev->device_available = 0;

#ifndef XIAOMI_DRM_INTERFACE_WA
	if (fp_dev->screen_state_wq) {
		destroy_workqueue(fp_dev->screen_state_wq);
	}

	if (prim_panel && !IS_ERR(cookie)) {
		panel_event_notifier_unregister(cookie);
	} else {
		pr_err("%s:prim_panel_event_notifier_unregister falt\n", __func__);
	}
#if IS_ENABLED(CONFIG_FP_HAVE_MULTI_SCREEN)
	if (sec_panel && !IS_ERR(cookie_sec)) {
		panel_event_notifier_unregister(cookie_sec);
	} else {
		pr_err("%s:sec_panel_event_notifier_unregister falt\n", __func__);
	}
#endif
#endif

	fp_free_input_device();
	fp_netlink_destroy(fp_dev);
	fp_dev->fp_netlink_enabled = 0;
#ifdef FP_ULTRA_QCOM
	fp_ultra_dev_unregister();
#ifndef FP_ULTRA_QCOM_P11
	fp_dev->fd_gpio.work_init = false;
#endif
	fp_dev->fw_ipc.work_init = false;
#endif
	cdev_del(&fp_dev->fpid_cdev);
	device_destroy(fp_dev->class, fp_dev->fpid_cdev.dev);
	cdev_del(&fp_dev->cdev);
	device_destroy(fp_dev->class, fp_dev->devt);
	list_del(&fp_dev->device_entry);

	unregister_chrdev_region(fp_dev->devt, 1);
	class_destroy(fp_dev->class);
#ifdef CONFIG_FP_MTK_PLATFORM
	if (!IS_ERR(fp_dev->pins_spiio_spi_mode)) {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_spiio_gpio_mode);
	}
	if (!IS_ERR(fp_dev->pins_reset_low)) {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_reset_low);
	}
#ifdef FP_ULTRA_QCOM_P11
	if (!IS_ERR(fp_dev->pins_int_default)) {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_int_default);
	}
#else
	if (!IS_ERR(fp_dev->pins_eint_default)) {
		pinctrl_select_state(fp_dev->pinctrl, fp_dev->pins_eint_default);
	}
#endif
#endif
	fp_dev->driver_device = NULL;
#ifdef FP_ULTRA_QCOM
	xiaomifp_evdev_remove();
#endif
	FUNC_EXIT();
	return 0;
}

#ifdef FP_ULTRA_QCOM

static int fp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct fp_device *fp_dev = &fp;
	/*
	 * Returning an error code if driver currently making a TZ call.
	 * Note: The purpose of this driver is to ensure that the clocks are on
	 * while making a TZ call. Hence the clock check to determine if the
	 * driver will allow suspend to occur.
	 */
	if (!mutex_trylock(&device_list_lock))
		return -EBUSY;

	else {
#ifndef FP_ULTRA_QCOM_P11
		if (fp_dev->fw_ipc.irq_enabled) {
			enable_irq_wake(fp_dev->fw_ipc.irq);
		}
#else
		if (fp_dev->fw_ipc.work_init) {
			if (fp_dev->fw_ipc.irq_enabled) {
				enable_irq_wake(fp_dev->fw_ipc.irq);
				pr_debug("enable IPC irq wake\n");
			}
		}
		if (fp_dev->irq_request) {
			if (fp_dev->irq_enabled) {
				enable_irq_wake(fp_dev->irq_num);
				pr_debug("enable irq wake\n");
			}
		}
#endif
	}

	mutex_unlock(&device_list_lock);

	return 0;
}

static int fp_resume(struct platform_device *pdev)
{
	struct fp_device *fp_dev = &fp;
#ifndef FP_ULTRA_QCOM_P11
	if (fp_dev->fw_ipc.irq_enabled) {
		disable_irq_wake(fp_dev->fw_ipc.irq);
	}
#else
	if (fp_dev->fw_ipc.work_init) {
		if (fp_dev->fw_ipc.irq_enabled) {
			disable_irq_wake(fp_dev->fw_ipc.irq);
			pr_debug("disable IPC irq wake\n");
		}
	}
	if (fp_dev->irq_request) {
		if (fp_dev->irq_enabled) {
			disable_irq_wake(fp_dev->irq_num);
			pr_debug("disable irq wake\n");
		}
	}
#endif
	return 0;
}

#endif

static const struct of_device_id fp_of_match[] = {
	{.compatible = DRIVER_COMPATIBLE,},
	{},
};
MODULE_DEVICE_TABLE(of, fp_of_match);

#ifdef CONFIG_FP_MTK_PLATFORM
static struct spi_driver fp_spi_driver = {
#else
static struct platform_driver fp_platform_driver = {
#endif
	.driver = {
		   .name = FP_DEV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = fp_of_match,
		   },
	.probe = fp_probe,
	.remove = fp_remove,
#ifdef FP_ULTRA_QCOM
	.resume = fp_resume,
	.suspend = fp_suspend,
#endif
};

/*-------------------------------------------------------------------------*/
static int __init fp_init(void)
{
	int status = 0;
	FUNC_ENTRY();
#ifdef CONFIG_FP_MTK_PLATFORM
	status = spi_register_driver(&fp_spi_driver);
#else
	status = platform_driver_register(&fp_platform_driver);
#endif
	if (status < 0) {
		pr_debug( "Failed to register fp driver.\n");
		return -EINVAL;
	}
	FUNC_EXIT();
	return status;
}

module_init(fp_init);

static void __exit fp_exit(void)
{
	FUNC_ENTRY();
#ifdef CONFIG_FP_MTK_PLATFORM
	spi_unregister_driver(&fp_spi_driver);
#else
	platform_driver_unregister(&fp_platform_driver);
#endif
	FUNC_EXIT();
}

module_exit(fp_exit);

MODULE_AUTHOR("xiaomi");
MODULE_DESCRIPTION("Xiaomi Fingerprint driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xiaomi-fp");
