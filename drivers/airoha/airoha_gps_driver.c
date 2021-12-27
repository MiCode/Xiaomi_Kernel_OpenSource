#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <linux/semaphore.h>
#include "airoha_gps_driver.h"
#include <linux/pinctrl/consumer.h>



#include <linux/ioport.h>

#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/clk.h>

#include <linux/tty_flip.h>
#include <linux/sysrq.h>

#include <linux/serial_core.h>


#define GPS_MAJOR 1
#define GPS_MINOR 2
//Todo: parser pin info from .dtsi
#define AIROHA_LDO_PIN 1146
//#define AIROHA_HOST_TO_GPS_PIN 1258
//#define AIROHA_GPS_TO_HOST_PIN 122
#define AIROHA_GPS_DEVICE_NAME "airoha_gps"
#define AIROHA_LOG_TAG "[airoha_gps] "

#define airoha_printk(format, args...) printk(AIROHA_LOG_TAG format, ##args)
#define UNUSED(obj) (void)obj;


/* GLOBAL SYMBOL      */
static dev_t gps_dev_number;
static struct class *gps_class;
struct cdev* gps_dev = NULL;

//condition
static wait_queue_head_t gps_wait_queue;
static atomic_t is_interrupt_happen;
static struct semaphore gps_file_lock;
static struct semaphore gps_file_operation_lock; //read /write can't reenter
static int open_num = 0;
struct pinctrl *pctrl;
struct pinctrl_state *pctrl_mode_active, *pctrl_mode_idle;
int request_ldo_pin_global;
/* GLOBAL SYMBOL END */

static int airoha_gps_open(struct inode *inode, struct file *file_p){
	down(&gps_file_lock);
	open_num++;
	airoha_printk("gps_open,count:%d\n",open_num);
	up(&gps_file_lock);

	return 0;
}
static int airoha_gps_release(struct inode *inode, struct file *file_p){
	down(&gps_file_lock);
	open_num--;
	airoha_printk("gps_release,count:%d\n",open_num);
	up(&gps_file_lock);
	return 0;
}
static ssize_t airoha_gps_read(struct file *file_p, char __user *user, size_t len, loff_t *offset){
	int result = 0;
	int copy_len = 0;
	char buffer[20] = "INTERRUPT\n";
	wait_event(gps_wait_queue ,atomic_read(&is_interrupt_happen) > 0);
	down(&gps_file_operation_lock);

	if (len > 20)
		copy_len = 20;
	else
		copy_len = len;
	result = copy_to_user(user,buffer,copy_len);
	atomic_dec(&is_interrupt_happen);
	up(&gps_file_operation_lock);
	return len;

}
static ssize_t  airoha_gps_write(struct file *file_p, const char __user *user, size_t len, loff_t *offset){
	char buffer[21] = {0};
	int result = 0;
	int copy_len;

	if (len > 20)
		copy_len = 20;
	else
		copy_len = len;
	result = copy_from_user(buffer,user,copy_len);
	airoha_printk("airoha_gps_write %s\n", buffer);
	if (strstr(buffer,"OPEN") != NULL)
		gps_chip_enable(1);
	else if (strstr(buffer,"CLOSE") != NULL)
		gps_chip_enable(0);
	else if (strstr(buffer,"DI") != NULL) {
                	result = pinctrl_select_state(pctrl, pctrl_mode_idle);
	                if (result < 0) {
		            airoha_printk("%s : change gps chip idle failed!\n", __func__);
	                }
             }
		//gpio_direction_output(AIROHA_HOST_TO_GPS_PIN, 0);
	else if (strstr(buffer,"DF") != NULL) {
                	result = pinctrl_select_state(pctrl, pctrl_mode_active);
	                if (result < 0) {
		            airoha_printk("%s : change gps chip active failed!\n", __func__);
	                }
		        //gpio_direction_output(AIROHA_HOST_TO_GPS_PIN, 1);
                }
	return copy_len;
}
static struct file_operations gps_cdev_ops = {
	.open = airoha_gps_open,
	.write = airoha_gps_write,
	.read = airoha_gps_read,
	.release = airoha_gps_release,

	.owner = THIS_MODULE,
};

/*irqreturn_t gps_interrupt_proc(int n,void *args){
	//airoha_printk("Interrupt happen!!!\n");
	atomic_inc(&is_interrupt_happen);
	wake_up(&gps_wait_queue);
	return IRQ_HANDLED;
}*/
static int xiaomi_uart_probe(struct platform_device *pdev) {
	int request_ldo_pin;
	int result;
	int ret;
	airoha_printk("====GPIO init Begin======\n");
	//request_ldo_pin = gpio_request(AIROHA_LDO_PIN,"airoha_gps_ldo_pin");
	//request_data_in_pin = gpio_request(AIROHA_HOST_TO_GPS_PIN,"airoha_host_to_gps_pin");
	//request_interrupt_pin = gpio_request(AIROHA_GPS_TO_HOST_PIN,"airoha_gps_to_host_pin");
	if (!pdev->dev.of_node) {
		airoha_printk("Failed to find of_node\n");
		goto err_exit;
	}
	request_ldo_pin = of_get_named_gpio(pdev->dev.of_node, "gps-ldo", 0);
	if ((!gpio_is_valid(request_ldo_pin)))
		return -EINVAL;
	ret = gpio_request(request_ldo_pin, "gps-ldo");
	if (ret) {
		airoha_printk( "request GPS ldo pin fail %d", ret);
		goto err_exit;
	}
	ret = gpio_direction_output(request_ldo_pin, 0);
	if (ret) {
		airoha_printk( "set GPS ldo pin as out mode fail %d", ret);
		goto err_exit;
	}
	request_ldo_pin_global = request_ldo_pin;
	airoha_printk("pin info:\n");
	airoha_printk("ldo...%d\n",request_ldo_pin);
	//airoha_printk("data_in...%d\n",request_data_in_pin);
	//airoha_printk("interrupt_pin...%d\n",request_interrupt_pin);
	//Set GPIO direction
	//gpio_direction_input(AIROHA_GPS_TO_HOST_PIN);
	//gpio_direction_output(AIROHA_LDO_PIN, 0);
	//gpio_direction_output(AIROHA_HOST_TO_GPS_PIN, 1);
	//register interrupt
	//gps_interrupt = gpio_to_irq(AIROHA_GPS_TO_HOST_PIN);
	//airoha_printk("get_interrupt_num...%d\n",gps_interrupt);
	/*result = request_irq(gps_interrupt, gps_interrupt_proc,
		IRQF_TRIGGER_RISING, "airoha_gps_interrupt", NULL);
	airoha_printk("request interrupt...%d\n",result);*/

	airoha_printk("====GPIO init done!!======\n");
	airoha_printk("====Device init...\n");
	result = alloc_chrdev_region(&gps_dev_number,0,1,"airoha_gps_dev");

	gps_dev = cdev_alloc();
	if (!gps_dev) {
		airoha_printk("cdev alloc error!\n");
		return -EPERM;
	}
	gps_dev->owner = THIS_MODULE;
	gps_dev->ops = &gps_cdev_ops;
	cdev_init(gps_dev,&gps_cdev_ops);
	cdev_add(gps_dev, gps_dev_number, 1);
	gps_class = class_create(THIS_MODULE, AIROHA_GPS_DEVICE_NAME);
	device_create(gps_class, NULL, gps_dev_number, 0, AIROHA_GPS_DEVICE_NAME);

	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl)) {
		airoha_printk("%s: Unable to allocate pinctrl: %d\n",
				__FILE__, PTR_ERR(pctrl));
		return -1;
	}

	pctrl_mode_active = pinctrl_lookup_state(pctrl, "gps_enable_active");
	if (IS_ERR(pctrl_mode_active)) {
		airoha_printk("%s: Unable to find pinctrl_state_mode_spi: %d\n",
			__FILE__, PTR_ERR(pctrl_mode_active));
		return -1;
	}

	pctrl_mode_idle = pinctrl_lookup_state(pctrl, "gps_enable_suspend");
	if (IS_ERR(pctrl_mode_idle)) {
		airoha_printk("%s: Unable to find pinctrl_state_mode_idle: %d\n",
			__FILE__, PTR_ERR(pctrl_mode_idle));
		return -1;
	}
	airoha_printk("[dsc]%s : pinctrl initialized\n", __func__);

	airoha_printk("====Device init Done\n");
	airoha_printk("Please check /dev/%s\n",AIROHA_GPS_DEVICE_NAME);
	airoha_printk("Wait....\n");
	//Init wait queue
	init_waitqueue_head(&gps_wait_queue);
	atomic_set(&is_interrupt_happen,0);
	sema_init(&gps_file_lock,1);
	sema_init(&gps_file_operation_lock,1);
	//

	return 0;
err_exit:
	return -1;
}

//Common Api
static int gps_chip_enable(bool enable){
	if (enable)
		gpio_direction_output(request_ldo_pin_global, 1);
	else
		gpio_direction_output(request_ldo_pin_global, 0);
	return 0;
}

static int xiaomi_uart_remove(struct platform_device *pdev)
{

	//g_bcm_gps = NULL;
	//platform_driver_unregister(&airoha_gps_driver);
	device_destroy(gps_class, gps_dev_number);
	class_destroy(gps_class);
	unregister_chrdev_region(gps_dev_number, 1);
	gpio_free(request_ldo_pin_global);
	//gpio_free(AIROHA_GPS_TO_HOST_PIN);
	//gpio_free(AIROHA_HOST_TO_GPS_PIN);
	//free_irq(gps_interrupt,NULL);

	airoha_printk("====gps driver exit======\n");
	printk("[airoha_gps] exit! \n");
	return 0;
}


static const struct of_device_id match_table[] = {
	{ .compatible = "bcm4775",},
	{},
};


/*
 * platform driver stuff
 */
static struct platform_driver xiaomi_uart_platform_driver = {
	.probe	= xiaomi_uart_probe,
	.remove	= xiaomi_uart_remove,
	.driver	= {
		.name  = "bcm4775",
		.of_match_table = match_table,
	},
};

static int __init xiaomi_tty_init(void)
{
	int ret;
	printk(KERN_ERR "!!! bcm_tty_init  to go");
	ret = platform_driver_register(&xiaomi_uart_platform_driver);
	printk(KERN_ERR "!!! platform_driver_register  sspbbd bcm_gps_tty misc_register ret is %d",ret);

	return ret;
}

static void __exit xiaomi_tty_exit(void){
	platform_driver_unregister(&xiaomi_uart_platform_driver);
}


MODULE_LICENSE("GPL");
module_init(xiaomi_tty_init);
module_exit(xiaomi_tty_exit);
