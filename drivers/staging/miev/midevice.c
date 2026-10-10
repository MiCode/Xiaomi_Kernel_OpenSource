#include <linux/module.h>
#include <linux/init.h>

static int __init miev_init_module(void)
{
	return 0;
}

static void __exit miev_exit_module(void)
{
}

module_init(miev_init_module);
module_exit(miev_exit_module);

MODULE_DESCRIPTION("exception log transfer.");
MODULE_LICENSE("GPL");
