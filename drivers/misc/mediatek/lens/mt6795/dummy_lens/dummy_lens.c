/*
 * MD218A voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>


/* #define DUMMY_LENS_DEBUG */
#ifdef DUMMY_LENS_DEBUG
#define DUMMY_LENSDB pr_debug
#else
#define DUMMY_LENSDB(x, ...)
#endif

static int __init DUMMY_LENS_i2C_init(void)
{
	return 0;
}

static void __exit DUMMY_LENS_i2C_exit(void)
{

}
module_init(DUMMY_LENS_i2C_init);
module_exit(DUMMY_LENS_i2C_exit);

MODULE_DESCRIPTION("Dummy lens module driver");
MODULE_AUTHOR("KY Chen <ky.chen@Mediatek.com>");
MODULE_LICENSE("GPL");
