#include <linux/kernel.h>  /* printk */
#include <linux/init.h>  /* __init __exit */
#include <linux/module.h>  /* module_init() */

static int __init ds28e30_verify_init(void)
{
        printk("ds28e30_verify\n");
        return 0;
}

static void __exit ds28e30_verify_exit(void)
{
        printk("ds28e30_verify\n");
}

module_init(ds28e30_verify_init);
module_exit(ds28e30_verify_exit);

MODULE_LICENSE("GPL");