#include "ftsCrossCompile.h"
#include "ftsError.h"

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
/* #include <linux/sec_sysfs.h> */
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/spi/spidev.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>

/* static char tag[8]="[ FTS ]\0"; */
void *stmalloc(size_t size)
{
	return kmalloc(size, GFP_KERNEL);

}

void stfree(void *ptr)
{
	kfree(ptr);
}
