#define pr_fmt(fmt) "millet_millet-sig: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include <linux/signal.h>

static int __init sig_mod_init(void)
{
	pr_err("enter sig_mod_init func!\n");

	return 0;
}

module_init(sig_mod_init);
MODULE_LICENSE("GPL");

