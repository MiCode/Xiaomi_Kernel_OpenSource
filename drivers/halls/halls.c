#include <linux/gpio.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/pm.h>

#include <asm/current.h>

#define HALLS_LOG printk

#define LOGTAG "halls "
#define DEF_HALLS_DEVICE_NAME "halls"

#define DEF_HALLS_IOCTRL_READ_HALL_1 1000
#define DEF_HALLS_IOCTRL_READ_HALL_2 2000
#define DEF_HALLS_IOCTRL_FLUSH 3000

#define DEF_HALLS_IOCTRL_STEUP 4000
#define DEF_HALLS_IOCTRL_RELEASE 5000

#define DEF_HALL_IRQ_1_FIRED "h-1"
#define DEF_HALL_IRQ_2_FIRED "h-2"
#define DEF_HALL_FLUSH  "flu"

#define DEF_HALL_GPIO_1 49
#define DEF_HALL_GPIO_2 113

extern int elliptic_set_hall_state(int state);

static int g_gpio_1_irq;
static int g_gpio_2_irq;
static int g_in_firedirq;
static int g_in_end;
static int g_in_flush;

static int g_in_opencounter;

struct _hall_device {
	struct miscdevice miscdev;
	struct delayed_work notify_work;
	struct wakeup_source *hall_wakelock;
	int hall_status;
};

struct _hall_device *g_st_halls;
struct semaphore g_st_semhallirq;

static void hall_notify_work_func(struct work_struct *work)
{
	int iRet1 = 0, iRet2 = 0;
	iRet1 = gpio_get_value_cansleep(DEF_HALL_GPIO_1);
	iRet2 = gpio_get_value_cansleep(DEF_HALL_GPIO_2);
	HALLS_LOG(LOGTAG "g_in_firedirq %d, g_gpio_1_irq %d, g_gpio_2_irq %d",
		  g_in_firedirq, g_gpio_1_irq, g_gpio_2_irq);
	HALLS_LOG(LOGTAG " iRet1 %d	iRet2 %d", iRet1, iRet2);
	if (iRet1 == 0 && iRet2 == 1) {
		g_st_halls->hall_status = 1;
	} else if (iRet1 == 1 && iRet2 == 0) {
		g_st_halls->hall_status = 0;
	} else {
		g_st_halls->hall_status = 2;
	}

	HALLS_LOG(LOGTAG "hall_notify_work_func notify audio hall_status: %d!",
		  g_st_halls->hall_status);
	elliptic_set_hall_state(g_st_halls->hall_status);
}

static irqreturn_t halls_irq_handler(int irqno, void *dev_id)
{
	HALLS_LOG(LOGTAG "Entered halls_irq_handler irqno %d\n", irqno);
	__pm_wakeup_event(g_st_halls->hall_wakelock, 20);
	g_in_firedirq = irqno;
	up(&g_st_semhallirq);
	schedule_delayed_work(&g_st_halls->notify_work, 0);
	return 0;
}

static int halls_configurate_input(int in_GPIO, const char *ch_mark)
{
	int iRet = 0;

	iRet = gpio_request(in_GPIO, ch_mark);
	if (iRet)
		pr_err("%d is being used!\n", in_GPIO);

	iRet = gpio_direction_input(in_GPIO);
	if (iRet)
		pr_err("Setting hall gpio direction failed! iRet %d\n", iRet);

	return iRet;
}

static int halls_configurate_irq(int in_GPIO,
				 const char *ch_mark, void *arg, int *out_irq)
{
	int iRet = 0, in_IRQ = 0;

	in_IRQ = gpio_to_irq(in_GPIO);
	*out_irq = in_IRQ;
	HALLS_LOG(LOGTAG "in_GPIO %d, IRQ %d\n", in_GPIO, in_IRQ);
	iRet = request_irq(in_IRQ, halls_irq_handler,
			   IRQF_TRIGGER_RISING
			   | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ch_mark, arg);
	if (iRet)
		pr_err("request_irq failed! iRet %d\n", iRet);
	irq_set_irq_wake(in_IRQ, 1);

	HALLS_LOG(LOGTAG "request_irq return %d\n", iRet);
	return iRet;
}

static ssize_t halls_read(struct file *file,
			  char __user *buf, size_t count, loff_t *ppos)
{
	int iRead = 0;

	HALLS_LOG(LOGTAG "Entereed halls_read  count  %d", (int)count);
	if (down_interruptible(&g_st_semhallirq)) {
		HALLS_LOG(LOGTAG "down_interruptible failed!");
	}
	if (g_in_end == 1) {
		HALLS_LOG(LOGTAG "Ending... ");
		return -EPERM;
	}
	HALLS_LOG(LOGTAG "g_in_firedirq %d, g_gpio_1_irq %d, g_gpio_2_irq %d",
		  g_in_firedirq, g_gpio_1_irq, g_gpio_2_irq);
	if (g_in_flush == 1) {
		g_in_flush = 0;
		iRead =
		    copy_to_user(buf, DEF_HALL_FLUSH, strlen(DEF_HALL_FLUSH));
		HALLS_LOG(LOGTAG "End of copy_to_user flash... ");
	} else if (g_in_firedirq == g_gpio_1_irq) {
		iRead = copy_to_user(buf, DEF_HALL_IRQ_1_FIRED,
				     strlen(DEF_HALL_IRQ_1_FIRED));
		HALLS_LOG(LOGTAG "End of copy_to_user 1... ");
	} else if (g_in_firedirq == g_gpio_2_irq) {
		iRead = copy_to_user(buf, DEF_HALL_IRQ_2_FIRED,
				     strlen(DEF_HALL_IRQ_2_FIRED));
		HALLS_LOG(LOGTAG "End of copy_to_user 2... ");
	}
	return iRead;
}

long halls_in_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int iRet = 0;
	void *p_ReadValue = (void *)arg;
	char ch_data[10] = { 0 };

	HALLS_LOG(LOGTAG "Enter halls_in_ioctl  cmd %d", cmd);
	switch (cmd) {
	case DEF_HALLS_IOCTRL_READ_HALL_1:
		{
			iRet = gpio_get_value_cansleep(DEF_HALL_GPIO_1);
			HALLS_LOG(LOGTAG "Read HALL_1 %d", iRet);
			snprintf(ch_data, sizeof(ch_data), "%d", iRet);
			iRet =
			    copy_to_user(p_ReadValue, ch_data, strlen(ch_data));
			break;
		}
	case DEF_HALLS_IOCTRL_READ_HALL_2:
		{
			iRet = gpio_get_value_cansleep(DEF_HALL_GPIO_2);
			HALLS_LOG(LOGTAG "Read HALL_2 %d", iRet);
			snprintf(ch_data, sizeof(ch_data), "%d", iRet);
			iRet =
			    copy_to_user(p_ReadValue, ch_data, strlen(ch_data));
			break;
		}
	case DEF_HALLS_IOCTRL_FLUSH:
		{
			g_in_flush = 1;
			up(&g_st_semhallirq);
			schedule_delayed_work(&g_st_halls->notify_work, 0);
			break;
		}
	case DEF_HALLS_IOCTRL_STEUP:
		{
			if (g_in_opencounter == 0) {
				g_in_end = 0;
				sema_init(&g_st_semhallirq, 0);
				iRet =
				    halls_configurate_irq(DEF_HALL_GPIO_1,
							  "hall-1",
							  (void *)g_st_halls,
							  &g_gpio_1_irq);
				if (iRet)
					pr_err
					    ("halls_configurate_irq failed!%d, iRet %d\n",
					     DEF_HALL_GPIO_1, iRet);
				g_in_firedirq = g_gpio_1_irq;

				iRet =
				    halls_configurate_irq(DEF_HALL_GPIO_2,
							  "hall-2",
							  (void *)g_st_halls,
							  &g_gpio_2_irq);
				if (iRet)
					pr_err
					    ("halls_configurate_irq failed! %d iRet %d\n",
					     DEF_HALL_GPIO_2, iRet);

				HALLS_LOG(LOGTAG
					  "g_gpio_2_irq %d, g_gpio_2_irq %d\n",
					  g_gpio_1_irq, g_gpio_2_irq);
				g_in_opencounter = 1;
			}
			break;
		}
	case DEF_HALLS_IOCTRL_RELEASE:
		{
			g_in_end = 1;
			up(&g_st_semhallirq);
			if (g_gpio_1_irq > 0)
				free_irq(g_gpio_1_irq, g_st_halls);

			if (g_gpio_2_irq > 0)
				free_irq(g_gpio_2_irq, g_st_halls);
			g_in_opencounter = 0;
			break;
		}
	default:
		iRet = -1;
	}
	HALLS_LOG(LOGTAG "read value cmd %d, iRet %d", cmd, iRet);
	return iRet;
}

int halls_flush(struct file *file, fl_owner_t id)
{
	HALLS_LOG(LOGTAG " Entered halls_flush \n");
	return 0;
}

int halls_open(struct inode *node, struct file *file)
{
	int iRet = 0;

	return iRet;
}

static const struct file_operations halls_fops = {
	.owner = THIS_MODULE,
	.read = halls_read,
	.unlocked_ioctl = halls_in_ioctl,
	.flush = halls_flush,
	.open = halls_open
};

static int __init halls_init(void)
{
	int ret;
	g_st_halls = kzalloc(sizeof(struct _hall_device), GFP_KERNEL);
	if (!g_st_halls)
		return -ENOMEM;
	g_st_halls->miscdev.fops = &halls_fops;
	g_st_halls->miscdev.minor = MISC_DYNAMIC_MINOR;
	g_st_halls->miscdev.name = DEF_HALLS_DEVICE_NAME;
	ret = misc_register(&g_st_halls->miscdev);
	if (ret < 0) {
		kfree(g_st_halls);
		return ret;
	}
	ret = halls_configurate_input(DEF_HALL_GPIO_1, "hall-1");
	if (ret) {
		pr_err("halls_configurate_input failed!%d, ret %d\n",
		       DEF_HALL_GPIO_1, ret);
		gpio_free(DEF_HALL_GPIO_1);
		return ret;
	}
	ret = halls_configurate_input(DEF_HALL_GPIO_2, "hall-2");
	if (ret) {
		pr_err("halls_configurate_input failed! %d ret %d\n",
		       DEF_HALL_GPIO_2, ret);
		gpio_free(DEF_HALL_GPIO_2);
		return ret;
	}

	g_st_halls->hall_status = 0;
	INIT_DELAYED_WORK(&g_st_halls->notify_work, hall_notify_work_func);

	g_st_halls->hall_wakelock = wakeup_source_register("dual_hall_ws");
	if (!g_st_halls->hall_wakelock) {
		pr_err("g_st_halls wakeup_source_register failed!\n");
		return ret;
	}

	return ret;
}

device_initcall(halls_init);

static void __exit halls_exit(void)
{
	gpio_free(DEF_HALL_GPIO_1);
	gpio_free(DEF_HALL_GPIO_2);
}

module_exit(halls_exit);
MODULE_LICENSE("GPL v2");
