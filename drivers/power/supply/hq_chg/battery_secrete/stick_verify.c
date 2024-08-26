#include <linux/kernel.h>  /* printk */
#include <linux/init.h>  /* __init __exit */
#include <linux/module.h>  /* module_init() */

static int __init stick_verify_init(void)
{
        printk("stick_verify_init\n");
        return 0;
}

static void __exit stick_verify_exit(void)
{
        printk("battery_auth_clstick_verify_exitass\n");
}

module_init(stick_verify_init);
module_exit(stick_verify_exit);

MODULE_LICENSE("GPL");