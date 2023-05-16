/** ***************************************************************************
 * @file nano_driver.c
 *
 * @brief provided interface of initialza and release nanosic driver .
 *
 * <em>Copyright (C) 2010, Nanosic, Inc.  All rights reserved.</em>
 * Author : Bin.yuan bin.yuan@nanosic.com 
 * */

/*
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
//#include <linux/regulator/consumer.h>
#include <drm/drm_notifier_mi.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include "nano_macro.h"

static bool initial = false;
struct nano_i2c_client* gI2c_client=NULL;

static int nanosic_803_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret=0;
    struct device_node* of_node;
    int irq_pin=-1;
    u32 irq_flags=0;
    int reset_pin=-1;
    int status_pin=-1;
    int vdd_pin=-1;
    int sleep_pin=-1;

//    struct regulator *vddh_reg=NULL;
    struct nano_i2c_client* I2client=NULL;

	if(IS_ERR_OR_NULL(client)) {
        dbgprint(ERROR_LEVEL,"nanosic_803_probe client IS_ERR_OR_NULL\n");
        return -1;
    }

    dbgprint(ALERT_LEVEL,"probe adapter nr %d, addr 0x%x\n",client->adapter->nr,client->addr);

    of_node = client->dev.of_node;
    if(!of_node) {
        dbgprint(ERROR_LEVEL,"nanosic_803_probe of_node == 0\n");
        return -1;
    }

    irq_pin = of_get_named_gpio_flags(of_node, "irq_pin", 0, &irq_flags);
    dbgprint(ALERT_LEVEL,"irq_pin=%d,irq_flags=%d ret=%d\n", irq_pin,irq_flags,ret);
    reset_pin = of_get_named_gpio(of_node, "reset_pin", 0);
    dbgprint(ALERT_LEVEL,"reset_pin=%d\n", reset_pin);
    status_pin = of_get_named_gpio(of_node, "status_pin", 0);
    dbgprint(ALERT_LEVEL,"status_pin=%d\n", status_pin);
    vdd_pin = of_get_named_gpio(of_node, "vdd_pin", 0);
    dbgprint(ALERT_LEVEL,"vdd_pin=%d\n", vdd_pin);
    sleep_pin = of_get_named_gpio(of_node, "sleep_pin", 0);
    dbgprint(ALERT_LEVEL,"sleep_pin=%d\n", sleep_pin);

    gpio_hall_n_pin = of_get_named_gpio(of_node, "hall_n_pin", 0);
    gpio_hall_s_pin = of_get_named_gpio(of_node, "hall_s_pin", 0);
    dbgprint(ALERT_LEVEL,"hall_n_pin=%d hall_s_pin:%d \n", gpio_hall_n_pin,gpio_hall_s_pin);
/*    vddh_reg = regulator_get(client->dev, "vddh-supply");
    if(IS_ERR_OR_NULL(vddh_reg))
        dbgprint(ERROR_LEVEL,"Failed to get vddh_reg\n");
*/
    /*initialize chardev module*/
    ret = Nanosic_chardev_register();
    if(ret < 0)
        goto _err1;

    /*initialize input module*/
    ret = Nanosic_input_register();
    if(ret < 0)
        goto _err2;

    ret = Nanosic_GPIO_register(vdd_pin,reset_pin,status_pin,irq_pin,sleep_pin);
    if(ret < 0)
        dbgprint(ERROR_LEVEL,"GPIO register ERROR!\n\n");

#if 0
    ret = Nanosic_i2c_detect(client,NULL);
    if(ret < 0){
        dbgprint(ERROR_LEVEL,"I2C communication ERROR!\n\n");
        return -1;
    }
#endif
    /*initialize i2c module*/
    I2client = Nanosic_i2c_register(irq_pin,irq_flags,client->adapter->nr,client->addr);
    if(IS_ERR_OR_NULL(I2client))
        goto _err3;

    i2c_set_clientdata(client,I2client);
    gI2c_client = I2client;
    gI2c_client->dev = &(client->dev);

    xiaomi_keyboard_init(I2client);
    /*initialize timer for test*/

    ret = Nanosic_cache_init();
    if(ret < 0) {
        dbgprint(ERROR_LEVEL,"Nanosic cache init ERROR!\n");
        goto _err4;
    }

    initial = true;
    dbgprint(ALERT_LEVEL,"probe nanosic driver\n");

    return 0;

_err4:
    Nanosic_cache_release();
_err3:
    Nanosic_input_release();
_err2:
    Nanosic_chardev_release();
_err1:
    return -1;
}

static int nanosic_803_remove(struct i2c_client *client)
{
    struct nano_i2c_client* I2client=NULL;

    dbgprint(ALERT_LEVEL,"remove\n");
    if(initial == false)
        return 0;

    I2client = i2c_get_clientdata(client);

    /*release chardev module*/
    Nanosic_chardev_release();

    /*release input module*/
    Nanosic_input_release();

    /*release i2c module*/
    Nanosic_i2c_release(I2client);

    /*release timer module*/
    //Nanosic_timer_release();//apply timer module instead of interrupt

    Nanosic_GPIO_release();

    Nanosic_cache_release();

    initial = false;

    dbgprint(ALERT_LEVEL,"remove nanosic driver\n");

    return 0;
}

static const struct of_device_id nanosic_803_of_match[] = {
	{.compatible = "nanosic,803",},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nanosic_803_of_match);

static const struct i2c_device_id nanosic_803_i2c_id[] = {
    { "nanosic,803", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, nanosic_803_i2c_id);

static struct i2c_driver nanosic_803_driver = {
	.probe		= nanosic_803_probe,
	.remove		= nanosic_803_remove,
	.driver = {
		.name	= "nanosic,803",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(nanosic_803_of_match),
        .pm = &xiaomi_keyboard_pm_ops,
	},
    .id_table       = nanosic_803_i2c_id,
	.detect         = Nanosic_i2c_detect,
};

static __init
int nanosic_driver_init(void)
{
    int ret;

    ret = i2c_add_driver(&nanosic_803_driver);

    return ret;
}

static __exit
void nanosic_driver_exit(void)
{
    i2c_del_driver(&nanosic_803_driver);
}

static xiaomi_keyboard_init(struct nano_i2c_client* i2c_client){
    //struct xiaomi_keyboard_platdata *pdata;
    int ret = 0;
    dbgprint(ALERT_LEVEL,"xiaomi_keyboard_init: enter\n");
    mdata = kzalloc(sizeof(struct xiaomi_keyboard_data), GFP_KERNEL);
    mdata->dev_pm_suspend = false;
    mdata->irq = i2c_client->irqno;
    dbgprint(ALERT_LEVEL,"xiaomi_keyboard_init:irq:%d",mdata->irq);
    mdata->event_wq = alloc_workqueue("kb-event-queue", WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
    if (!mdata->event_wq) {
        dbgprint(ALERT_LEVEL,"xiaomi_keyboard_init:Can not create work thread for suspend/resume!!");
        //ret = -ENOMEM;
    }
    INIT_WORK(&mdata->resume_work, keyboard_resume_work);
    INIT_WORK(&mdata->suspend_work, keyboard_suspend_work);

    mdata->drm_notif.notifier_call = keyboard_drm_notifier_callback;
    ret = mi_drm_register_client(&mdata->drm_notif);
    if(ret) {
        dbgprint(ALERT_LEVEL,"xiaomi_keyboard_init:register drm_notifier failed. ret=%d\n", ret);
        goto err_register_drm_notif_failed;
    }
    dbgprint(ALERT_LEVEL,"xiaomi_keyboard_init: success. \n");
    return ret;

err_register_drm_notif_failed:
    if (mdata->event_wq) {
        destroy_workqueue(mdata->event_wq);
    }
    return ret;
}

static void keyboard_resume_work(struct work_struct *work)
{
    int ret = 0;
    dbgprint(ALERT_LEVEL,"keyboard_resume_work: enter\n");
    ret = Nanosic_cache_put();
    if(ret < 0)
        dbgprint(ERROR_LEVEL,"keyboard_resume_work: Nanosic_cache_put err:%d\n",ret);
    Nanosic_GPIO_sleep(true);
    Nanosic_workQueue_schedule(gI2c_client->worker);
    ret = Nanosic_RequestGensor_notify();
    if(ret < 0)
        dbgprint(ERROR_LEVEL,"keyboard_resume_work: Nanosic_RequestGensor_notify err:%d\n",ret);
    ret = Nanosic_Hall_notify(gpio_hall_n_pin, gpio_hall_s_pin);
    if(ret < 0)
        dbgprint(ERROR_LEVEL,"keyboard_resume_work: Nanosic_Hall_notify err:%d\n",ret);
}

static void keyboard_suspend_work(struct work_struct *work)
{
    dbgprint(ALERT_LEVEL,"keyboard_suspend_work: enter\n");
    Nanosic_GPIO_sleep(false);
}

static int xiaomi_keyboard_pm_suspend(struct device *dev)
{
    int ret = 0;
    dbgprint(ALERT_LEVEL,"xiaomi_keyboard_pm_suspend: enter, enable_irq_wake\n");
    ret = enable_irq_wake(mdata->irq);//i2c_client->irqno
    if(ret < 0)
        dbgprint(ALERT_LEVEL,"xiaomi_keyboard_pm_suspend: enter, enable_irq_wake irq failed\n");
    ret = enable_irq_wake(g_wakeup_irqno);
    if(ret < 0)
        dbgprint(ALERT_LEVEL,"xiaomi_keyboard_pm_suspend: enter, enable_irq_wake wakeup_irqno failed\n");
    mdata->dev_pm_suspend = true;
    return ret;
}

static int xiaomi_keyboard_pm_resume(struct device *dev)
{
    int ret = 0;
    dbgprint(ALERT_LEVEL,"xiaomi_keyboard_pm_resume enter, disable_irq_wake\n");
    ret = disable_irq_wake(mdata->irq);//i2c_client->irqno
    if(ret < 0)
        dbgprint(ALERT_LEVEL,"xiaomi_keyboard_pm_resume: enter, disable_irq_wake irq failed\n");
    ret = disable_irq_wake(g_wakeup_irqno);
    if(ret < 0)
        dbgprint(ALERT_LEVEL,"xiaomi_keyboard_pm_resume: enter, disable_irq_wake wakeup_irqno failed\n");

    mdata->dev_pm_suspend = false;
    return ret;
}

static const struct dev_pm_ops xiaomi_keyboard_pm_ops = {
    .suspend = xiaomi_keyboard_pm_suspend,
    .resume = xiaomi_keyboard_pm_resume,
};


static int xiaomi_keyboard_remove(void)
{
    dbgprint(ALERT_LEVEL,"xiaomi_keyboard_remove: enter\n");
    mi_drm_unregister_client(&mdata->drm_notif);
    destroy_workqueue(mdata->event_wq);
    if (mdata) {
        kfree(mdata);
        mdata = NULL;
    }
    return 0;
}

static int keyboard_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    struct mi_drm_notifier *evdata = data;
    int *blank;
    struct xiaomi_keyboard_data *mdata = container_of(self, struct xiaomi_keyboard_data, drm_notif);

    if (!evdata || (evdata->id != 0))
        return 0;

    if (evdata->data && mdata) {
        blank = evdata->data;
        flush_workqueue(mdata->event_wq);
        if (event == MI_DRM_EARLY_EVENT_BLANK) {
            if (*blank == MI_DRM_BLANK_POWERDOWN) {
                dbgprint(ALERT_LEVEL,"keyboard_drm_notifier_callback keyboard suspend");
                queue_work(mdata->event_wq, &mdata->suspend_work);
            }
        } else if (event == MI_DRM_EVENT_BLANK) {
            if (*blank == MI_DRM_BLANK_UNBLANK) {
                dbgprint(ALERT_LEVEL,"keyboard_drm_notifier_callback keyboard resume");
                flush_workqueue(mdata->event_wq);
                queue_work(mdata->event_wq, &mdata->resume_work);
            }
        }
    }

    return 0;
}


late_initcall(nanosic_driver_init);
module_exit(nanosic_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("bin.yuan@nanosic.com");
