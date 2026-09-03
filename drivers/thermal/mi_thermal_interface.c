#include <linux/mm.h>

#include <linux/printk.h>

#include <linux/string.h>

#include <linux/init.h>
#include <linux/module.h>

static int __init mi_thermal_interface_init(void)
{
	return 0;
}
static void __exit mi_thermal_interface_exit(void)
{
	;
}
module_init(mi_thermal_interface_init);
module_exit(mi_thermal_interface_exit);
MODULE_LICENSE("GPL v2");
