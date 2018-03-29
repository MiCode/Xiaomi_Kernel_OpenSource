#include <linux/slab.h>
#include "usb2jtag_v1.h"
static unsigned int usb2jtag_mode_flag;
static int __init setup_usb2jtag_mode(char *str)
{
	usb2jtag_mode_flag = 0;

	if (*str++ != '=' || !*str)
		/*
		* No options specified. Switch on full debugging.
		*/
		goto out;

	switch (*str) {
	case '0':
		usb2jtag_mode_flag = 0;
		pr_debug("disable usb2jtag\n");
		break;
	case '1':
		usb2jtag_mode_flag = 1;
		pr_debug("enable usb2jtag\n");
		break;
	default:
		pr_err("usb2jtag option '%c' unknown. skipped\n", *str);
	}
out:
	return 0;

}
__setup("usb2jtag_mode", setup_usb2jtag_mode);

unsigned int usb2jtag_mode(void)
{
	return usb2jtag_mode_flag;
}

static struct mt_usb2jtag_driver mt_usb2jtag_drv = {
	.usb2jtag_init = NULL,
	.usb2jtag_resume = NULL,
	.usb2jtag_suspend = NULL,
};

struct mt_usb2jtag_driver *get_mt_usb2jtag_drv(void)
{
	return &mt_usb2jtag_drv;
}

static int mt_usb2jtag_resume_default(void)
{
	return (usb2jtag_mode()) ?
		mt_usb2jtag_drv.usb2jtag_init() : 0;
}

int mt_usb2jtag_resume(void)
{
	return (mt_usb2jtag_drv.usb2jtag_resume) ?
		mt_usb2jtag_drv.usb2jtag_resume() :
		mt_usb2jtag_resume_default();
}

static int __init mt_usb2jtag_init(void)
{
	return (usb2jtag_mode() && mt_usb2jtag_drv.usb2jtag_init) ?
		mt_usb2jtag_drv.usb2jtag_init() : -1;
}

static void __exit mt_usb2jtag_exit(void)
{
}

module_init(mt_usb2jtag_init);
module_exit(mt_usb2jtag_exit);
