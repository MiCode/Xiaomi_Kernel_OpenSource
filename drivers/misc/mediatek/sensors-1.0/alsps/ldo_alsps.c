/*add psensor vdd3 compile by luozeng at 2021.3.24 start*/
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>



#include <linux/regulator/consumer.h>

#include <linux/module.h>
#include <linux/printk.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/bug.h>
#include <linux/types.h>
#include <linux/param.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define ALSPS_LDO_NODE  "mediatek,alsps_ldo_enable"

int alsps_ldo3_driver_init(void)
{
    int err = 0,gpio;
    struct device_node *dev_node = NULL;
    enum of_gpio_flags flags;
    //FF_LOGV("'%s' enter.", __func__);

    /* Find device tree node. */
    dev_node = of_find_compatible_node(NULL, NULL, ALSPS_LDO_NODE);
    if (!dev_node) {
        //FF_LOGE("of_find_compatible_node(.., '%s') failed.", ALSPS_LDO_NODE);
	    pr_debug("%s: do not find the alsps node\n", __func__);
    }

    /* Initialize ldoen pin. */
    gpio = of_get_named_gpio_flags(dev_node, "alsps,ldoen_gpio", 0, &flags);
    if (gpio < 0) {
	    //FF_LOGE("g_config->gpio_ldoen_pin(%d) can not find.", gpio);
        pr_debug("alsps:g_config->gpio_ldoen_pin(%d) can not find.", gpio);
    }
    else {
	gpio_free(gpio);
    }
    if (!gpio_is_valid(gpio)) {
        //FF_LOGE("g_config->gpio_ldoen_pin(%d) is invalid.", gpio);
        pr_debug("alsps:g_config->gpio_ldoen_pin(%d) is invalid.", gpio);
    }
    err = gpio_request(gpio, "alsps_gpio_ldoen_pin");
    if (err) {
        //FF_LOGE("gpio_request(%d) failed= %d.", gpio, err);
        pr_debug("alsps:gpio_request(%d) failed= %d.", gpio, err);
    }
    err = gpio_direction_output(gpio, 1);
    if (err) {
        //FF_LOGE("gpio_direction_output(%d, 1) failed= %d.", gpio, err);
        pr_debug("alsps:gpio_direction_output(%d, 1) failed= %d.", gpio, err);
    }
   return err; 
} 
/*add psensor vdd3 compile by luozeng at 2021.3.24 end*/