#ifdef	CONFIG_CTS_I2C_HOST
#define LOG_TAG         "I2CDrv"
#else
#define LOG_TAG         "SPIDrv"
#endif

#include "cts_config.h"
#include "cts_platform.h"
#include "cts_core.h"
#include "cts_sysfs.h"
#include "cts_charger_detect.h"
#include "cts_earjack_detect.h"
#include "cts_oem.h"

/*C3T code for HQ-218218 by jishen at 2022/10/01  start */
extern int cts_set_prox_en(unsigned int enable);
extern int tp_select_proximity;
/*C3T code for HQ-218218 by jishen at 2022/10/01  end */

/*C3T code for HQ-258541 by jishen at 2022/10/25  start */
extern bool lcd_reset_keep_high;
/*C3T code for HQ-258541 by jishen at 2022/10/25  end */

#ifdef CONFIG_CTS_TP_PROXIMITY
struct cts_device *cts_dev;
#endif

static void cts_resume_work_func(struct work_struct *work);
#ifdef CFG_CTS_DRM_NOTIFIER
#include <drm/drm_panel.h>
static struct drm_panel *active_panel;
static int check_dt(struct device_node *np);
#endif
bool cts_show_debug_log;
#ifdef CTS_MTK_GET_PANEL
static char *active_panel_name;
#endif

module_param_named(debug_log, cts_show_debug_log, bool, 0660);
MODULE_PARM_DESC(debug_log, "Show debug log control");

static int cts_suspend(struct chipone_ts_data *cts_data)
{
	int ret;

	cts_info("Suspend");
	/*C3T code for HQ-258541 by jishen at 2022/10/25  start */
     	if(lcd_reset_keep_high){
	cts_info("skip normal cts_suspend when proximity status = %d",lcd_reset_keep_high);
	msleep(20);
	return 0;
	}
	/*C3T code for HQ-258541 by jishen at 2022/10/25  end */
	
	cts_lock_device(&cts_data->cts_dev);
	ret = cts_suspend_device(&cts_data->cts_dev);
	cts_unlock_device(&cts_data->cts_dev);

	if (ret)
		cts_err("Suspend device failed %d", ret);

	ret = cts_stop_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Stop device failed %d", ret);
		return ret;
	}
#ifdef CFG_CTS_GESTURE
	/* Enable IRQ wake if gesture wakeup enabled */
	if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
		ret = cts_plat_enable_irq_wake(cts_data->pdata);
		if (ret) {
			cts_err("Enable IRQ wake failed %d", ret);
			return ret;
		}
		ret = cts_plat_enable_irq(cts_data->pdata);
		if (ret) {
			cts_err("Enable IRQ failed %d", ret);
			return ret;
		}
	}
#endif /* CFG_CTS_GESTURE */

/** - To avoid waking up while not sleeping,
 *delay 20ms to ensure reliability
 */
	msleep(20);

	return 0;
}

static int cts_resume(struct chipone_ts_data *cts_data)
{
	int ret;

	cts_info("Resume");

#ifdef CFG_CTS_GESTURE
	if (cts_is_gesture_wakeup_enabled(&cts_data->cts_dev)) {
		ret = cts_plat_disable_irq_wake(cts_data->pdata);
		if (ret)
			cts_warn("Disable IRQ wake failed %d", ret);
		ret = cts_plat_disable_irq(cts_data->pdata);
		if (ret < 0)
			cts_err("Disable IRQ failed %d", ret);
	}
#endif /* CFG_CTS_GESTURE */

	cts_lock_device(&cts_data->cts_dev);
	ret = cts_resume_device(&cts_data->cts_dev);
	cts_unlock_device(&cts_data->cts_dev);
	if (ret) {
		cts_warn("Resume device failed %d", ret);
		return ret;
	}

	ret = cts_start_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Start device failed %d", ret);
		return ret;
	}

	return 0;
}

static void cts_resume_work_func(struct work_struct *work)
{
	struct chipone_ts_data *cts_data =
	    container_of(work, struct chipone_ts_data, ts_resume_work);
	cts_info("%s", __func__);
	cts_resume(cts_data);
}

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
#ifdef CFG_CTS_DRM_NOTIFIER
static int fb_notifier_callback(struct notifier_block *nb,
				unsigned long action, void *data)
{
	volatile int blank;
	const struct cts_platform_data *pdata =
	    container_of(nb, struct cts_platform_data, fb_notifier);
	struct chipone_ts_data *cts_data =
	    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
	struct drm_panel_notifier *evdata = data;

	cts_info("FB notifier callback");
	if (!evdata || !cts_data)
		return 0;

	blank = *(int *)evdata->data;
	cts_info("action=%lu, blank=%d\n", action, blank);

	if (action == DRM_PANEL_EARLY_EVENT_BLANK) {
		if (blank == DRM_PANEL_BLANK_POWERDOWN)
			cts_suspend(cts_data);
	} else if (evdata->data) {
		blank = *(int *)evdata->data;
		if (action == DRM_PANEL_EVENT_BLANK) {
			if (blank == DRM_PANEL_BLANK_UNBLANK)
				/* cts_resume(cts_data); */
				queue_work(cts_data->workqueue,
					   &cts_data->ts_resume_work);
		}
	}

	return 0;
}
#else
static int fb_notifier_callback(struct notifier_block *nb,
				unsigned long action, void *data)
{
	volatile int blank;
	const struct cts_platform_data *pdata =
	    container_of(nb, struct cts_platform_data, fb_notifier);
	struct chipone_ts_data *cts_data =
	    container_of(pdata->cts_dev, struct chipone_ts_data, cts_dev);
	struct fb_event *evdata = data;

	cts_info("FB notifier callback");

	if (evdata && evdata->data) {
		if (action == FB_EVENT_BLANK) {
			blank = *(int *)evdata->data;
			if (blank == FB_BLANK_UNBLANK) {
				/* cts_resume(cts_data); */
				queue_work(cts_data->workqueue,
					   &cts_data->ts_resume_work);
				return NOTIFY_OK;
			}
		} else if (action == FB_EARLY_EVENT_BLANK) {
			blank = *(int *)evdata->data;
			if (blank == FB_BLANK_POWERDOWN) {
				cts_suspend(cts_data);
				return NOTIFY_OK;
			}
		}
	}

	return NOTIFY_DONE;
}
#endif

static int cts_init_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Init FB notifier");

	cts_data->pdata->fb_notifier.notifier_call = fb_notifier_callback;

#ifdef CFG_CTS_DRM_NOTIFIER
	{
		int ret = -ENODEV;

		if (active_panel) {
			ret =
			    drm_panel_notifier_register(active_panel,
							&cts_data->pdata->fb_notifier);
			if (ret)
				cts_err
				    ("register drm_notifier failed. ret=%d\n",
				     ret);
		}
		return ret;
	}
#else
	return fb_register_client(&cts_data->pdata->fb_notifier);
#endif
}

static int cts_deinit_pm_fb_notifier(struct chipone_ts_data *cts_data)
{
	cts_info("Deinit FB notifier");
#ifdef CFG_CTS_DRM_NOTIFIER
	{
		int ret = 0;

		if (active_panel) {
			ret =
			    drm_panel_notifier_unregister(active_panel,
							  &cts_data->pdata->fb_notifier);
			if (ret)
				cts_err
				    ("Error occurred while unregistering drm_notifier.\n");
		}
		return ret;
	}
#else
	return fb_unregister_client(&cts_data->pdata->fb_notifier);
#endif
}
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

#ifdef CFG_CTS_DRM_NOTIFIER
static int check_dt(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			cts_info("check active_panel");
			active_panel = panel;
			return 0;
		}
	}
	if (node)
		pr_err("%s: %s not actived\n", __func__, node->name);
	return -ENODEV;
}

static int check_default_tp(struct device_node *dt, const char *prop)
{
	const char *active_tp;
	const char *compatible;
	char *start;
	int ret;

	ret = of_property_read_string(dt->parent, prop, &active_tp);
	if (ret) {
		cts_err(" %s:fail to read %s %d\n", __func__, prop, ret);
		return -ENODEV;
	}

	ret = of_property_read_string(dt, "compatible", &compatible);
	if (ret < 0) {
		cts_err(" %s:fail to read %s %d\n", __func__, "compatible",
			ret);
		return -ENODEV;
	}

	start = strnstr(active_tp, compatible, strlen(active_tp));
	if (start == NULL) {
		cts_err(" %s:no match compatible, %s, %s\n",
			__func__, compatible, active_tp);
		ret = -ENODEV;
	}

	return ret;
}
#endif

#ifdef CTS_MTK_GET_PANEL
char panel_name[50] = { 0 };

static int cts_get_panel(void)
{
	int ret = -1;

	cts_info("enter");
	if (saved_command_line) {
		char *sub;
		char key_prefix[] = "mipi_mot_vid_";
		char ic_prefix[] = "icnl";

		cts_info("saved_command_line is %s", saved_command_line);
		sub = strstr(saved_command_line, key_prefix);
		if (sub) {
			char *d;
			int n, len, len_max = 50;

			d = strstr(sub, " ");
			if (d)
				n = strlen(sub) - strlen(d);
			else
				n = strlen(sub);

			if (n > len_max)
				len = len_max;
			else
				len = n;

			strncpy(panel_name, sub, len);
			active_panel_name = panel_name;
			if (strstr(active_panel_name, ic_prefix))
				cts_info("active_panel_name=%s",
					 active_panel_name);
			else {
				cts_info("Not chipone panel!");
				return ret;
			}

		} else {
			cts_info("chipone active panel not found!");
			return ret;
		}
	} else {
		cts_info("saved_command_line null!");
		return ret;
	}

	return 0;
}
#endif
#ifdef CONFIG_CTS_I2C_HOST
static int cts_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
#else
static int cts_driver_probe(struct spi_device *client)
#endif
{
	struct chipone_ts_data *cts_data = NULL;
	int ret = 0;
#ifdef CTS_MTK_GET_PANEL
	ret = cts_get_panel();
	if (ret) {
		cts_info("MTK get chipone panel error");
		return ret;
	}
#endif

#ifdef CFG_CTS_DRM_NOTIFIER
	{
		struct device_node *dp = client->dev.of_node;

		if (check_dt(dp)) {
			if (!check_default_tp(dp, "qcom,i2c-touch-active"))
				ret = -EPROBE_DEFER;
			else
				ret = -ENODEV;

			cts_err("%s: %s not actived\n", __func__, dp->name);
			return ret;
		}
	}
#endif

#ifdef CONFIG_CTS_I2C_HOST
	cts_info("Probe i2c client: name='%s' addr=0x%02x flags=0x%02x irq=%d",
		 client->name, client->addr, client->flags, client->irq);

#if !defined(CONFIG_MTK_PLATFORM)
	if (client->addr != CTS_DEV_NORMAL_MODE_I2CADDR) {
		cts_err("Probe i2c addr 0x%02x != driver config addr 0x%02x",
			client->addr, CTS_DEV_NORMAL_MODE_I2CADDR);
		return -ENODEV;
	};
#endif

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		cts_err("Check functionality failed");
		return -ENODEV;
	}
#endif

	cts_data =
	    (struct chipone_ts_data *)kzalloc(sizeof(*cts_data), GFP_KERNEL);
	if (cts_data == NULL) {
		cts_err("Allocate chipone_ts_data failed");
		return -ENOMEM;
	}
	cts_data->pdata = (struct cts_platform_data *)
	    kzalloc(sizeof(struct cts_platform_data), GFP_KERNEL);
	if (cts_data->pdata == NULL) {
		cts_err("Allocate cts_platform_data failed");
		ret = -ENOMEM;
		goto err_free_cts_data;
	}
#ifdef CONFIG_CTS_I2C_HOST
	i2c_set_clientdata(client, cts_data);
	cts_data->i2c_client = client;
	cts_data->device = &client->dev;
#else
	spi_set_drvdata(client, cts_data);
	cts_data->spi_client = client;
	cts_data->device = &client->dev;
#endif

	cts_init_platform_data(cts_data->pdata, client);

	cts_data->cts_dev.pdata = cts_data->pdata;
	cts_data->pdata->cts_dev = &cts_data->cts_dev;
#ifdef CONFIG_CTS_TP_PROXIMITY
	cts_dev = &cts_data->cts_dev;
#endif

	cts_data->workqueue =
	    create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-workqueue");
	if (cts_data->workqueue == NULL) {
		cts_err("Create workqueue failed");
		ret = -ENOMEM;
		goto err_deinit_platform_data;
	}
#ifdef CONFIG_CTS_ESD_PROTECTION
	cts_data->esd_workqueue =
	    create_singlethread_workqueue(CFG_CTS_DEVICE_NAME "-esd_workqueue");
	if (cts_data->esd_workqueue == NULL) {
		cts_err("Create esd workqueue failed");
		ret = -ENOMEM;
		goto err_destroy_workqueue;
	}
#endif
	ret = cts_plat_request_resource(cts_data->pdata);
	if (ret < 0) {
		cts_err("Request resource failed %d", ret);
		goto err_destroy_esd_workqueue;
	}

	ret = cts_reset_device(&cts_data->cts_dev);
	if (ret < 0) {
		cts_err("Reset device failed %d", ret);
		goto err_free_resource;
	}

	ret = cts_probe_device(&cts_data->cts_dev);
	if (ret) {
		cts_err("Probe device failed %d", ret);
		goto err_free_resource;
	}

	ret = cts_plat_init_touch_device(cts_data->pdata);
	if (ret < 0) {
		cts_err("Init touch device failed %d", ret);
		goto err_free_resource;
	}

	ret = cts_plat_init_vkey_device(cts_data->pdata);
	if (ret < 0) {
		cts_err("Init vkey device failed %d", ret);
		goto err_deinit_touch_device;
	}

	ret = cts_plat_init_gesture(cts_data->pdata);
	if (ret < 0) {
		cts_err("Init gesture failed %d", ret);
		goto err_deinit_vkey_device;
	}

	cts_init_esd_protection(cts_data);

	ret = cts_tool_init(cts_data);
	if (ret < 0)
		cts_warn("Init tool node failed %d", ret);

	ret = cts_sysfs_add_device(&client->dev);
	if (ret < 0)
		cts_warn("Add sysfs entry for device failed %d", ret);

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
	ret = cts_init_pm_fb_notifier(cts_data);
	if (ret) {
		cts_err("Init FB notifier failed %d", ret);
		goto err_deinit_sysfs;
	}
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

	ret = cts_plat_request_irq(cts_data->pdata);
	if (ret < 0) {
		cts_err("Request IRQ failed %d", ret);
		goto err_register_fb;
	}

#ifdef CONFIG_CTS_CHARGER_DETECT
	ret = cts_charger_detect_init(cts_data);
	if (ret)
		cts_err("Init charger detect failed %d", ret);
		/* Ignore this error */
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
    ret = cts_earjack_detect_init(cts_data);
    if (ret) {
        cts_err("Init earjack detect failed %d", ret);
        // Ignore this error
    }
#endif

	/*C3T code for HQ-218218 by jishen at 2022/10/01  start */   
	//if(cts_set_prox_en(1)==0)        //this is just a test operation to open virtual proximity, we should comment out this item in the official release.
        //cts_info("virtual proximity has been enabled in probe,but it's just for testing");   
	/*C3T code for HQ-218218 by jishen at 2022/10/01  end */

    ret = cts_oem_init(cts_data);
    if (ret < 0) {
        cts_warn("Init oem specific faild %d", ret);
		goto err_deinit_oem;
    }

	/* Init firmware upgrade work and schedule */
	INIT_DELAYED_WORK(&cts_data->fw_upgrade_work,
		cts_firmware_upgrade_work);
	queue_delayed_work(cts_data->workqueue, &cts_data->fw_upgrade_work,
		msecs_to_jiffies(15 * 1000));

	INIT_WORK(&cts_data->ts_resume_work, cts_resume_work_func);
	/*C3T code for HQ-253995 by zhangkexin at 2022/10/08  start */
        tp_select_proximity = 3;
        cts_info("we_selected_prox_%d",tp_select_proximity);
	/*C3T code for HQ-253995 by zhangkexin at 2022/10/08  end */
	return 0;

err_deinit_oem:
	cts_oem_deinit(cts_data);

#ifdef CONFIG_CTS_EARJACK_DETECT
    cts_earjack_detect_deinit(cts_data);
#endif
#ifdef CONFIG_CTS_CHARGER_DETECT
    cts_charger_detect_deinit(cts_data);
#endif
    cts_plat_free_irq(cts_data->pdata);

err_register_fb:
#ifdef CONFIG_CTS_PM_FB_NOTIFIER
	cts_deinit_pm_fb_notifier(cts_data);
err_deinit_sysfs:
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */
	cts_sysfs_remove_device(&client->dev);
#ifdef CONFIG_CTS_LEGACY_TOOL
	cts_tool_deinit(cts_data);
#endif /* CONFIG_CTS_LEGACY_TOOL */

#ifdef CONFIG_CTS_ESD_PROTECTION
	cts_deinit_esd_protection(cts_data);
#endif /* CONFIG_CTS_ESD_PROTECTION */

#ifdef CFG_CTS_GESTURE
	cts_plat_deinit_gesture(cts_data->pdata);
#endif /* CFG_CTS_GESTURE */

err_deinit_vkey_device:
#ifdef CONFIG_CTS_VIRTUALKEY
	cts_plat_deinit_vkey_device(cts_data->pdata);
#endif /* CONFIG_CTS_VIRTUALKEY */

err_deinit_touch_device:
	cts_plat_deinit_touch_device(cts_data->pdata);

err_free_resource:
	cts_plat_free_resource(cts_data->pdata);
err_destroy_esd_workqueue:
#ifdef CONFIG_CTS_ESD_PROTECTION
	destroy_workqueue(cts_data->esd_workqueue);
err_destroy_workqueue:
#endif
	destroy_workqueue(cts_data->workqueue);
err_deinit_platform_data:
	cts_deinit_platform_data(cts_data->pdata);
	kfree(cts_data->pdata);
err_free_cts_data:
	kfree(cts_data);

	cts_err("Probe failed %d", ret);

	return ret;
}

#ifdef CONFIG_CTS_I2C_HOST
static int cts_driver_remove(struct i2c_client *client)
#else
static int cts_driver_remove(struct spi_device *client)
#endif
{
	struct chipone_ts_data *cts_data;
	int ret = 0;

	cts_info("Remove");

#ifdef CONFIG_CTS_I2C_HOST
	cts_data = (struct chipone_ts_data *)i2c_get_clientdata(client);
#else
	cts_data = (struct chipone_ts_data *)spi_get_drvdata(client);
#endif
	if (cts_data) {
		ret = cts_stop_device(&cts_data->cts_dev);
		if (ret)
			cts_warn("Stop device failed %d", ret);

#ifdef CONFIG_CTS_CHARGER_DETECT
		cts_charger_detect_deinit(cts_data);
#endif

#ifdef CONFIG_CTS_EARJACK_DETECT
        cts_earjack_detect_deinit(cts_data);
#endif

		cts_plat_free_irq(cts_data->pdata);

#ifdef CONFIG_CTS_PM_FB_NOTIFIER
		cts_deinit_pm_fb_notifier(cts_data);
#endif /* CONFIG_CTS_PM_FB_NOTIFIER */

		cts_tool_deinit(cts_data);

		cts_sysfs_remove_device(&client->dev);

		cts_deinit_esd_protection(cts_data);

		cts_plat_deinit_touch_device(cts_data->pdata);

		cts_plat_deinit_vkey_device(cts_data->pdata);

		cts_plat_deinit_gesture(cts_data->pdata);

		cts_plat_free_resource(cts_data->pdata);

		cts_oem_deinit(cts_data);

		if (cts_data->cts_dev.tx)
			kfree(cts_data->cts_dev.tx);
		if (cts_data->cts_dev.rx)
			kfree(cts_data->cts_dev.rx);

#ifdef CONFIG_CTS_ESD_PROTECTION
		if (cts_data->esd_workqueue)
			destroy_workqueue(cts_data->esd_workqueue);
#endif

		if (cts_data->workqueue)
			destroy_workqueue(cts_data->workqueue);

		cts_deinit_platform_data(cts_data->pdata);

		cts_deinit_rtdata(&cts_data->cts_dev);

		if (cts_data->pdata)
			kfree(cts_data->pdata);
		kfree(cts_data);
	} else {
		cts_warn
		    ("Chipone i2c driver remove while NULL chipone_ts_data");
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_CTS_PM_LEGACY
static int cts_i2c_driver_suspend(struct device *dev, pm_message_t state)
{
	cts_info("Suspend by legacy power management");
	return cts_suspend(dev_get_drvdata(dev));
}

static int cts_i2c_driver_resume(struct device *dev)
{
	cts_info("Resume by legacy power management");
	return cts_resume(dev_get_drvdata(dev));
}
#endif /* CONFIG_CTS_PM_LEGACY */

#ifdef CONFIG_CTS_PM_GENERIC
static int cts_i2c_driver_pm_suspend(struct device *dev)
{
	cts_info("Suspend by bus power management");
	return cts_suspend(dev_get_drvdata(dev));
}

static int cts_i2c_driver_pm_resume(struct device *dev)
{
	cts_info("Resume by bus power management");
	return cts_resume(dev_get_drvdata(dev));
}

/* bus control the suspend/resume procedure */
static const struct dev_pm_ops cts_i2c_driver_pm_ops = {
	.suspend = cts_i2c_driver_pm_suspend,
	.resume = cts_i2c_driver_pm_resume,
};
#endif /* CONFIG_CTS_PM_GENERIC */

#ifdef CONFIG_CTS_SYSFS
static ssize_t reset_pin_show(struct device_driver *driver, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_HAS_RESET_PIN
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(reset_pin, S_IRUGO, reset_pin_show, NULL);
#else
static DRIVER_ATTR_RO(reset_pin);
#endif

static ssize_t swap_xy_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_SWAP_XY: %c\n",
#ifdef CFG_CTS_SWAP_XY
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(swap_xy, S_IRUGO, swap_xy_show, NULL);
#else
static DRIVER_ATTR_RO(swap_xy);
#endif

static ssize_t wrap_x_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_WRAP_X: %c\n",
#ifdef CFG_CTS_WRAP_X
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(wrap_x, S_IRUGO, wrap_x_show, NULL);
#else
static DRIVER_ATTR_RO(wrap_x);
#endif

static ssize_t wrap_y_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_WRAP_Y: %c\n",
#ifdef CFG_CTS_WRAP_Y
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(wrap_y, S_IRUGO, wrap_y_show, NULL);
#else
static DRIVER_ATTR_RO(wrap_y);
#endif

static ssize_t force_update_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_HAS_RESET_PIN: %c\n",
#ifdef CFG_CTS_FIRMWARE_FORCE_UPDATE
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(force_update, S_IRUGO, force_update_show, NULL);
#else
static DRIVER_ATTR_RO(force_update);
#endif

static ssize_t max_touch_num_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_TOUCH_NUM: %d\n",
			CFG_CTS_MAX_TOUCH_NUM);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(max_touch_num, S_IRUGO, max_touch_num_show, NULL);
#else
static DRIVER_ATTR_RO(max_touch_num);
#endif

static ssize_t vkey_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_VIRTUALKEY: %c\n",
#ifdef CONFIG_CTS_VIRTUALKEY
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(vkey, S_IRUGO, vkey_show, NULL);
#else
static DRIVER_ATTR_RO(vkey);
#endif

static ssize_t gesture_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_GESTURE: %c\n",
#ifdef CFG_CTS_GESTURE
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(gesture, S_IRUGO, gesture_show, NULL);
#else
static DRIVER_ATTR_RO(gesture);
#endif

static ssize_t esd_protection_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_ESD_PROTECTION: %c\n",
#ifdef CONFIG_CTS_ESD_PROTECTION
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(esd_protection, S_IRUGO, esd_protection_show, NULL);
#else
static DRIVER_ATTR_RO(esd_protection);
#endif

static ssize_t slot_protocol_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "CONFIG_CTS_SLOTPROTOCOL: %c\n",
#ifdef CONFIG_CTS_SLOTPROTOCOL
			'Y'
#else
			'N'
#endif
	    );
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(slot_protocol, S_IRUGO, slot_protocol_show, NULL);
#else
static DRIVER_ATTR_RO(slot_protocol);
#endif

static ssize_t max_xfer_size_show(struct device_driver *dev, char *buf)
{
#ifdef CONFIG_CTS_I2C_HOST
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_I2C_XFER_SIZE: %d\n",
			CFG_CTS_MAX_I2C_XFER_SIZE);
#else
	return snprintf(buf, PAGE_SIZE, "CFG_CTS_MAX_SPI_XFER_SIZE: %d\n",
			CFG_CTS_MAX_SPI_XFER_SIZE);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(max_xfer_size, S_IRUGO, max_xfer_size_show, NULL);
#else
static DRIVER_ATTR_RO(max_xfer_size);
#endif

static ssize_t driver_info_show(struct device_driver *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Driver version: %s\n",
			CFG_CTS_DRIVER_VERSION);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static DRIVER_ATTR(driver_info, S_IRUGO, driver_info_show, NULL);
#else
static DRIVER_ATTR_RO(driver_info);
#endif

static struct attribute *cts_i2c_driver_config_attrs[] = {
	&driver_attr_reset_pin.attr,
	&driver_attr_swap_xy.attr,
	&driver_attr_wrap_x.attr,
	&driver_attr_wrap_y.attr,
	&driver_attr_force_update.attr,
	&driver_attr_max_touch_num.attr,
	&driver_attr_vkey.attr,
	&driver_attr_gesture.attr,
	&driver_attr_esd_protection.attr,
	&driver_attr_slot_protocol.attr,
	&driver_attr_max_xfer_size.attr,
	&driver_attr_driver_info.attr,
	NULL
};

static const struct attribute_group cts_i2c_driver_config_group = {
	.name = "config",
	.attrs = cts_i2c_driver_config_attrs,
};

static const struct attribute_group *cts_i2c_driver_config_groups[] = {
	&cts_i2c_driver_config_group,
	NULL,
};
#endif /* CONFIG_CTS_SYSFS */

#ifdef CONFIG_CTS_OF
static const struct of_device_id cts_i2c_of_match_table[] = {
	{.compatible = CFG_CTS_OF_DEVICE_ID_NAME, },
	{ },
};

MODULE_DEVICE_TABLE(of, cts_i2c_of_match_table);
#endif /* CONFIG_CTS_OF */

#ifdef CONFIG_CTS_I2C_HOST
static const struct i2c_device_id cts_device_id_table[] = {
	{ CFG_CTS_DEVICE_NAME, 0 },
	{ }
};
#else
static const struct spi_device_id cts_device_id_table[] = {
	{ CFG_CTS_DEVICE_NAME, 0 },
	{ }
};
#endif

#ifdef CONFIG_CTS_I2C_HOST
static struct i2c_driver cts_i2c_driver = {
#else
static struct spi_driver cts_spi_driver = {
#endif
	.probe = cts_driver_probe,
	.remove = cts_driver_remove,
	.driver = {
		   .name = CFG_CTS_DRIVER_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_CTS_OF
		   .of_match_table = of_match_ptr(cts_i2c_of_match_table),
#endif /* CONFIG_CTS_OF */
#ifdef CONFIG_CTS_SYSFS
		   .groups = cts_i2c_driver_config_groups,
#endif /* CONFIG_CTS_SYSFS */
#ifdef CONFIG_CTS_PM_LEGACY
		   .suspend = cts_i2c_driver_suspend,
		   .resume = cts_i2c_driver_resume,
#endif /* CONFIG_CTS_PM_LEGACY */
#ifdef CONFIG_CTS_PM_GENERIC
		   .pm = &cts_i2c_driver_pm_ops,
#endif /* CONFIG_CTS_PM_GENERIC */

		    },
	.id_table = cts_device_id_table,
};

static int __init cts_driver_init(void)
{
	cts_info("Init");

#ifdef CONFIG_CTS_I2C_HOST
	return i2c_add_driver(&cts_i2c_driver);
#else
	pr_err("%s spi", __func__);
	return spi_register_driver(&cts_spi_driver);
#endif
}

static void __exit cts_driver_exit(void)
{
	cts_info("Exit");

#ifdef CONFIG_CTS_I2C_HOST
	i2c_del_driver(&cts_i2c_driver);
#else
	spi_unregister_driver(&cts_spi_driver);
#endif
}

module_init(cts_driver_init);
module_exit(cts_driver_exit);

#if KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
MODULE_DESCRIPTION("Chipone TDDI touchscreen Driver for QualComm platform");
MODULE_VERSION(CFG_CTS_DRIVER_VERSION);
MODULE_AUTHOR("Miao Defang <dfmiao@chiponeic.com>");
MODULE_LICENSE("GPL");
