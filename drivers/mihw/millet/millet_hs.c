#define pr_fmt(fmt) "millet-millet_hs: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>

static int __init millet_hs_init(void)
{

	pr_err("enter millet_hs_init func!\n");
	return 0;
}

static void __exit millet_hs_exit(void)
{
	return;
}

module_init(millet_hs_init);
module_exit(millet_hs_exit);

MODULE_LICENSE("GPL");
