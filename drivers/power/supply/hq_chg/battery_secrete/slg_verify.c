#include <linux/kernel.h>  /* printk */
#include <linux/init.h>  /* __init __exit */
#include <linux/module.h>  /* module_init() */

static int __init slg_verify_init(void)
{
        printk("slg_verify_init\n");
        return 0;
}

static void __exit slg_verify_exit(void)
{
        printk("slg_verify_exit\n");
}

module_init(slg_verify_init);
module_exit(slg_verify_exit);

MODULE_LICENSE("GPL");