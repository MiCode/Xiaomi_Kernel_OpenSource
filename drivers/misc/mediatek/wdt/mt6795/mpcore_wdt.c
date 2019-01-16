/*
 *	Watchdog driver for the mpcore watchdog timer
 *
 *	(c) Copyright 2004 ARM Limited
 *
 *	Based on the SoftDog driver:
 *	(c) Copyright 1996 Alan Cox <alan@lxorguk.ukuu.org.uk>,
 *						All Rights Reserved.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Neither Alan Cox nor CymruNet Ltd. admit liability nor provide
 *	warranty for any of this software. This material is provided
 *	"AS-IS" and at no charge.
 *
 *	(c) Copyright 1995    Alan Cox <alan@lxorguk.ukuu.org.uk>
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/fs.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/smp.h>
#include <asm/hardware/gic.h>
#include <asm/smp_twd.h>
#include <wd_kicker.h>
#include <mach/irqs.h>
#include <linux/aee.h>

#include <linux/mt_sched_mon.h>

#if defined(CONFIG_FIQ_GLUE)
#else
static int __percpu *wdt_evt;
#endif

struct mpcore_wdt {
	unsigned long	timer_alive;
	struct device	*dev;
	void __iomem	*base;
	int		irq;
	unsigned int	perturb;
	char		expect_close;
};

static struct platform_device *mpcore_wdt_dev;
extern void mt_irq_unmask(struct irq_data *data);
extern void mt_irq_mask(struct irq_data *data);
/* Externanl functions */
extern void mtk_wdt_disable(void);

static DEFINE_SPINLOCK(wdt_lock);
//#define WDT_CPU0
#define DEBUG_INFO "[WDK-mpwdt]:"
//#define MPCORE_WDT_DEBUG 1
#ifdef  MPCORE_WDT_DEBUG
#define mpcore_wdt_print(fmt, arg...) \
do {\
	printk(DEBUG_INFO fmt, ##arg);\
	printk(DEBUG_INFO"%s, %d\n", __FUNCTION__, __LINE__);\
} while(0)
#else
#define mpcore_wdt_print(fmt, arg...) do{} while(0)
#endif

#define TIMER_MARGIN	30 //default 30s
int mpcore_margin = TIMER_MARGIN;
#define PERIPHCLK   (13000000)
static struct mpcore_wdt mp_wdt = {.base = (void __iomem*)0xf000a600};

module_param(mpcore_margin, int, 00664);
MODULE_PARM_DESC(mpcore_margin,
	"MPcore timer margin in seconds. (0 < mpcore_margin < 65536, default="
				__MODULE_STRING(TIMER_MARGIN) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 00664);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

#define ONLY_TESTING	1 // setting timer mode
static int mpcore_noboot = ONLY_TESTING;
module_param(mpcore_noboot, int, 0);
MODULE_PARM_DESC(mpcore_noboot, "MPcore watchdog action, "
	"set to 1 to ignore reboots, 0 to reboot (default="
					__MODULE_STRING(ONLY_TESTING) ")");

static void mpcore_wdt_irq_enable(unsigned int irq)
{
	unsigned long flags;

	local_irq_save(flags);
	irq_set_status_flags(irq, IRQ_NOPROBE);
	mt_irq_unmask(irq_get_irq_data(irq));
	local_irq_restore(flags);
}

static void mpcore_wdt_irq_disable(unsigned int irq)
{
	unsigned long flags;

	local_irq_save(flags);
	irq_set_status_flags(irq, IRQ_NOPROBE);
	mt_irq_mask(irq_get_irq_data(irq));
	local_irq_restore(flags);
}

/*
 *	This is the interrupt handler.  Note that we only use this
 *	in testing mode, so don't actually do a reboot here.
 */
 #ifdef WDT_CPU0 
static irqreturn_t mpcore_wdt_fire(int irq, void *arg)
{
	struct mpcore_wdt *wdt = arg;
	
	printk(DEBUG_INFO"Triggered :cpu-%d\n", get_cpu());
	/* Check it really was our interrupt */
	if (readl(wdt->base + TWD_WDOG_INTSTAT)) {
		printk(DEBUG_INFO"Triggered :cpu-%d\n", get_cpu());
		/* Clear the interrupt on the watchdog */
		writel(1, wdt->base + TWD_WDOG_INTSTAT);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
#endif
/*
 *	This is the interrupt handler.  Note that we only use this
 *	in testing mode, so don't actually do a reboot here.
 */
#if 0 
static void do_local_wdt(void)
{
	printk(DEBUG_INFO"Triggered :cpu-%d\n", get_cpu());
	/* Check it really was our interrupt */
	if (readl(wdt.base + TWD_WDOG_INTSTAT)) {
		printk(DEBUG_INFO"Triggered :cpu-%d\n", get_cpu());
		/* Clear the interrupt on the watchdog */
		writel(1, wdt.base + TWD_WDOG_INTSTAT);
	}
		printk(DEBUG_INFO"exit :cpu-%d\n", get_cpu());
	return;
}
#endif

#if 0 // remove for linux3.0-->linux3.4
asmlinkage void __exception_irq_entry do_local_wdt(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	int cpu = smp_processor_id();
	struct mpcore_wdt wdt;

    mt_trace_ISR_start(30);
	/* Disable ext wdt (rgu) */
	mtk_wdt_disable();
	wdt.base = (void __iomem*)0xf000a600;
	printk(DEBUG_INFO"control: 0x%x, count:0x%x, load:0x%x\n", readl(wdt.base + TWD_WDOG_CONTROL), readl(wdt.base + TWD_WDOG_COUNTER), readl(wdt.base + TWD_WDOG_LOAD));
	printk(DEBUG_INFO"Triggered :cpu-%d\n", cpu);
	if (readl(wdt.base + TWD_WDOG_INTSTAT)) {
//		__inc_irq_stat(cpu, local_timer_irqs);
		writel(1, wdt.base + TWD_WDOG_INTSTAT);
		
	}
	printk(DEBUG_INFO"exit :cpu-%d\n", cpu);
	aee_wdt_irq_info();
    mt_trace_ISR_end(30);
	set_irq_regs(old_regs);
}
#endif
/*
 *	mpcore_wdt_keepalive - reload the timer
 *
 *	Note that the spec says a DIFFERENT value must be written to the reload
 *	register each time.  The "perturb" variable deals with this by adding 1
 *	to the count every other time the function is called.
 */
static void mpcore_wdt_keepalive(struct mpcore_wdt *wdt)
{
	unsigned long count,prescale, flags;
	int cpu = 0;
	spin_lock_irqsave(&wdt_lock, flags);
	cpu = smp_processor_id();
#if 0	
	/* Assume prescale is set to 256 */
	count =  __raw_readl(wdt->base + TWD_WDOG_COUNTER);
	count = (0xFFFFFFFFU - count) * (HZ / 5);
#endif
	//prescale =  ((__raw_readl(wdt->base + TWD_WDOG_CONTROL)&0xffff)>>8);
	prescale = 0x0;
	count = PERIPHCLK/(prescale + 1)*mpcore_margin;

	/* Reload the counter */
	writel(count + wdt->perturb, wdt->base + TWD_WDOG_LOAD);
	wdt->perturb = wdt->perturb ? 0 : 1;
		
	spin_unlock_irqrestore(&wdt_lock, flags);
	printk("WDT mpcore_wdt_keepalive:cpu-%d: count:%ld. prescale:%ld, mpcore_margin:%d,TWD_WDOG_LOAD:%x\n", cpu, count, prescale, mpcore_margin,readl(wdt->base + TWD_WDOG_LOAD));
	mpcore_wdt_print("cpu-%d: count:%d. prescale:%d,  mpcore_margin:%d\n", cpu, count, prescale, mpcore_margin);
}

static void mpcore_wdt_stop(struct mpcore_wdt *wdt)
{
	unsigned long flags;
	int cpu = 0;
	spin_lock_irqsave(&wdt_lock, flags);
	cpu = smp_processor_id();
	writel(0x12345678, wdt->base + TWD_WDOG_DISABLE);
	writel(0x87654321, wdt->base + TWD_WDOG_DISABLE);
	writel(0x0, wdt->base + TWD_WDOG_CONTROL);
	spin_unlock_irqrestore(&wdt_lock, flags);
	mpcore_wdt_print("cpu-%d,control: 0x%x, count:0x%x, load:0x%x\n", cpu, readl(wdt->base + TWD_WDOG_CONTROL), readl(wdt->base + TWD_WDOG_COUNTER), readl(wdt->base + TWD_WDOG_LOAD));
}
static void mpcore_wdt_stop_fiq(struct mpcore_wdt *wdt)
{
	//unsigned long flags;
	//int cpu = 0;
	//spin_lock_irqsave(&wdt_lock, flags);
	//cpu = smp_processor_id();
	*(volatile u32 *)(wdt->base + TWD_WDOG_DISABLE)=0x12345678;
	*(volatile u32 *)(wdt->base + TWD_WDOG_DISABLE)=0x87654321;
	*(volatile u32 *)(wdt->base + TWD_WDOG_CONTROL)=0x0;
	//spin_unlock_irqrestore(&wdt_lock, flags);
	//mpcore_wdt_print("cpu-%d,control: 0x%x, count:0x%x, load:0x%x\n", cpu, readl(wdt->base + TWD_WDOG_CONTROL), readl(wdt->base + TWD_WDOG_COUNTER), readl(wdt->base + TWD_WDOG_LOAD));
}


static void mpcore_wdt_start(struct mpcore_wdt *wdt)
{
	unsigned long flags;
	int cpu = 0;
	mpcore_wdt_print("start watchdog.\n");

	/* This loads the count register but does NOT start the count yet */
	mpcore_wdt_keepalive(wdt);

	spin_lock_irqsave(&wdt_lock, flags);
	cpu = smp_processor_id();
	if (mpcore_noboot) {
		/* Enable watchdog - prescale=0, watchdog mode=0, enable=1, IT = 1, reload = 1 */
		writel(0x00000007, wdt->base + TWD_WDOG_CONTROL);
	} else {
		/* Enable watchdog - prescale=0, watchdog mode=1, enable=1, IT = 1, reload = 1 */
		writel(0x0000000F, wdt->base + TWD_WDOG_CONTROL);
	}
	spin_unlock_irqrestore(&wdt_lock, flags);
	printk("WDT mpcore_wdt_start:cpu-%d,control: 0x%x, count:0x%x, load:0x%x\n", cpu, readl(wdt->base + TWD_WDOG_CONTROL), readl(wdt->base + TWD_WDOG_COUNTER), readl(wdt->base + TWD_WDOG_LOAD));
	//mpcore_wdt_print("cpu-%d,control: 0x%x, count:0x%x, load:0x%x\n", cpu, readl(wdt->base + TWD_WDOG_CONTROL), readl(wdt->base + TWD_WDOG_COUNTER), readl(wdt->base + TWD_WDOG_LOAD));
}

static void mpcore_wdt_start_fiq(struct mpcore_wdt *wdt)
{
	//unsigned long flags;
	//int cpu = 0;
	//mpcore_wdt_print("start watchdog.\n");

	/* This loads the count register but does NOT start the count yet */
	mpcore_wdt_keepalive(wdt);

	//spin_lock_irqsave(&wdt_lock, flags);
	//cpu = smp_processor_id();
	if (mpcore_noboot) {
		/* Enable watchdog - prescale=0, watchdog mode=0, enable=1, IT = 1, reload = 1 */
		*(volatile u32 *)(wdt->base + TWD_WDOG_CONTROL)=0x00000007;
	} else {
		/* Enable watchdog - prescale=0, watchdog mode=1, enable=1, IT = 1, reload = 1 */
		*(volatile u32 *)( wdt->base + TWD_WDOG_CONTROL)=0x0000000F;
	}
	//spin_unlock_irqrestore(&wdt_lock, flags);
	//printk("WDT mpcore_wdt_start:cpu-%d,control: 0x%x, count:0x%x, load:0x%x\n", cpu, readl(wdt->base + TWD_WDOG_CONTROL), readl(wdt->base + TWD_WDOG_COUNTER), readl(wdt->base + TWD_WDOG_LOAD));
	//mpcore_wdt_print("cpu-%d,control: 0x%x, count:0x%x, load:0x%x\n", cpu, readl(wdt->base + TWD_WDOG_CONTROL), readl(wdt->base + TWD_WDOG_COUNTER), readl(wdt->base + TWD_WDOG_LOAD));
}

static int mpcore_wdt_set_heartbeat(int t)
{
	if (t < 0x0001 || t > 0xFFFF)
		return -EINVAL;

	mpcore_margin = t;
	return 0;
}
unsigned long mpcore_wdt_get_counter(void)
{
	int cpu = 0;
	unsigned long counter_value, flags;
	spin_lock_irqsave(&wdt_lock, flags);
	cpu = smp_processor_id();
	counter_value = readl(mp_wdt.base + TWD_WDOG_COUNTER);
	spin_unlock_irqrestore(&wdt_lock, flags);
	mpcore_wdt_print("mpcore_wdt_get_counter:cpu-%d,counter_value: 0x%x\n", cpu, counter_value);
    return counter_value;
}
void mpcore_wdt_set_counter(unsigned long counter_value)
{
	int cpu = 0;
	unsigned long flags;
	spin_lock_irqsave(&wdt_lock, flags);
	cpu = smp_processor_id();
	writel(counter_value, mp_wdt.base + TWD_WDOG_COUNTER);
	spin_unlock_irqrestore(&wdt_lock, flags);
	printk(KERN_INFO "WDT mpcore_wdt_set_counter:cpu-%d,counter_value: %ld, TWD_WDOG_COUNTER: 0x%x\n", cpu, counter_value,readl(mp_wdt.base + TWD_WDOG_COUNTER));	
	//mpcore_wdt_print("mpcore_wdt_get_counter:cpu-%d,counter_value: 0x%x\n", cpu, counter_value);
}

/*
 *	/dev/watchdog handling
 */
static int mpcore_wdt_open(struct inode *inode, struct file *file)
{
	struct mpcore_wdt *wdt = platform_get_drvdata(mpcore_wdt_dev);
  
  mpcore_wdt_print("enter %s\n", __FUNCTION__);
	if (test_and_set_bit(0, &wdt->timer_alive))
		return -EBUSY;

	if (nowayout)
		__module_get(THIS_MODULE);

	file->private_data = wdt;

	/*
	 *	Activate timer
	 */
	mpcore_wdt_start(wdt);
  
	return nonseekable_open(inode, file);
}

static int mpcore_wdt_release(struct inode *inode, struct file *file)
{
	struct mpcore_wdt *wdt = file->private_data;

	/*
	 *	Shut off the timer.
	 *	Lock it in if it's a module and we set nowayout
	 */
	if (wdt->expect_close == 42)
		mpcore_wdt_stop(wdt);
	else {
		dev_printk(KERN_CRIT, wdt->dev,
				"unexpected close, not stopping watchdog!\n");
		mpcore_wdt_keepalive(wdt);
	}
	clear_bit(0, &wdt->timer_alive);
	wdt->expect_close = 0;
	mpcore_wdt_print("enter %s\n", __FUNCTION__);
	return 0;
}

static ssize_t mpcore_wdt_write(struct file *file, const char *data,
						size_t len, loff_t *ppos)
{
	struct mpcore_wdt *wdt = file->private_data;

	/*
	 *	Refresh the timer.
	 */
	if (len) {
		if (!nowayout) {
			size_t i;

			/* In case it was set long ago */
			wdt->expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;

				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V')
					wdt->expect_close = 42;
			}
		}
		mpcore_wdt_keepalive(wdt);
	}
	mpcore_wdt_print("enter %s\n", __FUNCTION__);
	return len;
}

static const struct watchdog_info ident = {
	.options		= WDIOF_SETTIMEOUT |
				  WDIOF_KEEPALIVEPING |
				  WDIOF_MAGICCLOSE,
	.identity		= "MPcore Watchdog",
};

static long mpcore_wdt_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct mpcore_wdt *wdt = file->private_data;
	int ret;
	union {
		struct watchdog_info ident;
		int i;
	} uarg;

	if (_IOC_DIR(cmd) && _IOC_SIZE(cmd) > sizeof(uarg))
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = copy_from_user(&uarg, (void __user *)arg, _IOC_SIZE(cmd));
		if (ret)
			return -EFAULT;
	}

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		uarg.ident = ident;
		ret = 0;
		break;

	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		uarg.i = 0;
		ret = 0;
		break;

	case WDIOC_SETOPTIONS:
		ret = -EINVAL;
		if (uarg.i & WDIOS_DISABLECARD) {
			mpcore_wdt_stop(wdt);
			ret = 0;
		}
		if (uarg.i & WDIOS_ENABLECARD) {
			mpcore_wdt_start(wdt);
			ret = 0;
		}
		break;

	case WDIOC_KEEPALIVE:
		mpcore_wdt_keepalive(wdt);
		ret = 0;
		break;

	case WDIOC_SETTIMEOUT:
		ret = mpcore_wdt_set_heartbeat(uarg.i);
		if (ret)
			break;

		mpcore_wdt_keepalive(wdt);
		/* Fall */
	case WDIOC_GETTIMEOUT:
		uarg.i = mpcore_margin;
		ret = 0;
		break;

	default:
		return -ENOTTY;
	}

	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) {
		ret = copy_to_user((void __user *)arg, &uarg, _IOC_SIZE(cmd));
		if (ret)
			ret = -EFAULT;
	}
	return ret;
}

/*
 *	System shutdown handler.  Turn off the watchdog if we're
 *	restarting or halting the system.
 */
static void mpcore_wdt_shutdown(struct platform_device *dev)
{
	#if 0
	struct mpcore_wdt *wdt = platform_get_drvdata(dev);
	#else
	struct mpcore_wdt *wdt = &mp_wdt;
	#endif

	if (system_state == SYSTEM_RESTART || system_state == SYSTEM_HALT)
		mpcore_wdt_stop(wdt);
}

/*
 *	Kernel Interfaces
 */
static const struct file_operations mpcore_wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= mpcore_wdt_write,
	.unlocked_ioctl	= mpcore_wdt_ioctl,
	.open		= mpcore_wdt_open,
	.release	= mpcore_wdt_release,
};

static struct miscdevice mpcore_wdt_miscdev = {
	.minor		= WATCHDOG_MINOR,
	.name		= "mpcore-watchdog",
	.fops		= &mpcore_wdt_fops,
};

#if defined(CONFIG_FIQ_GLUE)

static void fiq_wdt_func(void *arg, void *regs, void *svc_sp)
{
	struct mpcore_wdt wdt;

	wdt.base = (void __iomem *)0xf000a600;
	if(*(volatile u32 *)(wdt.base + TWD_WDOG_INTSTAT))
	{
	    *(volatile u32 *)(wdt.base + TWD_WDOG_INTSTAT) =1 ;
	}

	aee_wdt_fiq_info(arg, regs, svc_sp);
}

#else
//irq handler
static irqreturn_t wdt_handler(int irq, void *dev_id)

{
	
	int cpu = smp_processor_id();
	struct mpcore_wdt wdt;

      // mt_trace_ISR_start(30);
	/* Disable ext wdt (rgu) */
	//mtk_wdt_disable();
	wdt.base = (void __iomem*)0xf000a600;
	printk("wdt_irq control: 0x%x, count:0x%x, load:0x%x\n", readl(wdt.base + TWD_WDOG_CONTROL), readl(wdt.base + TWD_WDOG_COUNTER), readl(wdt.base + TWD_WDOG_LOAD));
	printk("wdt_irq Triggered :cpu-%d\n", cpu);
	if (readl(wdt.base + TWD_WDOG_INTSTAT)) {
//		__inc_irq_stat(cpu, local_timer_irqs);
		writel(1, wdt.base + TWD_WDOG_INTSTAT);
		
	}
	printk("wdt_irq exit :cpu-%d\n", cpu);
	aee_wdt_irq_info();
     // mt_trace_ISR_end(30);
	
	return IRQ_HANDLED;
}

#endif

#if defined(CONFIG_FIQ_GLUE)

static void fiq_wdt_init(void *info)
{
	int err;
	

	err = request_fiq(GIC_PPI_WATCHDOG_TIMER, fiq_wdt_func, 0, NULL);
	if (err) {
		printk(KERN_ERR "Fail to request FIQ for local WDT\n");
	} else {
		printk(KERN_INFO "Use FIQ for PPI WDT on cpu %d\n", smp_processor_id());
	}
	return;
}
#else

static void irq_wdt_init(void)
{
      int err =0;
      wdt_evt = alloc_percpu(int *);
	if (!wdt_evt) {
		err = -ENOMEM;
		 printk( "fwq ?????\n");
		goto out_free;
	}
      err = request_percpu_irq(GIC_PPI_WATCHDOG_TIMER, wdt_handler, "local_wdt", wdt_evt);
	if (err) {
		printk(KERN_ERR "fwq Fail to request IRQ for local WDT\n");
		
	}else
	{
	      printk( "fwq user IRQ\n");
	}
	return;
out_free:
	 printk( "fwq error \n");
	free_percpu(wdt_evt);
	return;

}
#endif

static int __devinit mpcore_wdt_probe(struct platform_device *dev)
{
	struct mpcore_wdt *wdt = &mp_wdt;
//	struct resource *res;
	int ret;
	
	/* We only accept one device, and it must have an id of -1 */
	if (dev->id != -1)
		return -ENODEV;

	wdt->dev = &dev->dev;
	wdt->irq = GIC_PPI_WATCHDOG_TIMER;
	
	wdt->base = (void __iomem*)0xF000A600;
#if 0	
	mpcore_wdt_miscdev.parent = &dev->dev;
	ret = misc_register(&mpcore_wdt_miscdev);
	if (ret) {
		printk(DEBUG_INFO"cannot register miscdev on minor=%d (err=%d)\n",
							WATCHDOG_MINOR, ret);
		goto err_misc;
	}
#endif
#ifdef WDT_CPU0 
	ret = request_irq(wdt->irq, mpcore_wdt_fire, IRQF_TRIGGER_HIGH,
							"mpcore_wdt", wdt);
	if (ret) {
		printk(DEBUG_INFO"cannot register IRQ%d for watchdog\n", wdt->irq);
		goto err_irq;
	}
#endif	

#if defined(CONFIG_FIQ_GLUE)
	/*
	 * NoteXXX: The FIQ handler's initialization will use one SMP call.
	 *          Thus it cannot be used via another SMP call. 
	 *          We need to do this initialization on the current CPU first.
	 */
	fiq_wdt_init(NULL);
	smp_call_function(fiq_wdt_init, NULL, 1);
#else
      irq_wdt_init();
#endif
  //mpcore_wdt_irq_enable(wdt->irq);
	mpcore_wdt_stop(wdt);
	#if 0 // for debug
	mpcore_wdt_irq_enable(wdt->irq);
	mpcore_wdt_start(wdt);
	
	#endif
	#if 0
	platform_set_drvdata(dev, wdt);
	
	mpcore_wdt_dev = dev;
	#endif
	printk(DEBUG_INFO"mpcore wdt probe success\n");
	return 0;

#ifdef WDT_CPU0 
err_irq:
	misc_deregister(&mpcore_wdt_miscdev);
err_out:
#endif
	
	return ret;
}

int mpcore_wk_wdt_config(enum wk_wdt_type type, enum wk_wdt_mode mode, int timeout_val)
{
	//mpcore_wdt_print("enter:type:%d, mode:%d, val:%d\n", type, mode, timeout_val);
      printk("WDT enter:type:%d, mode:%d, val:%d\n", type, mode, timeout_val);
	mpcore_wdt_set_heartbeat(timeout_val);
	return 0;
}

void mpcore_wdt_restart(enum wk_wdt_type type)
{
	struct mpcore_wdt *wdt = &mp_wdt;
	mpcore_wdt_print("enter:type:%d\n", type);
	mpcore_wdt_stop(wdt);
	mpcore_wdt_start(wdt);
#ifndef WDT_CPU0	
	mpcore_wdt_irq_enable(wdt->irq);
#endif		
}

void mpcore_wdt_restart_fiq(void)
{
	struct mpcore_wdt *wdt = &mp_wdt;
	mpcore_wdt_stop_fiq(wdt);
	mpcore_wdt_start_fiq(wdt);
#ifndef WDT_CPU0	
	mpcore_wdt_irq_enable(wdt->irq);
#endif		
}

void mpcore_wk_wdt_stop(void)
{
	struct mpcore_wdt *wdt = &mp_wdt;
	mpcore_wdt_print("mpcore_wk_wdt_stop\n");
	mpcore_wdt_irq_disable(wdt->irq);
	mpcore_wdt_stop(wdt);	
}
static int __devexit mpcore_wdt_remove(struct platform_device *dev)
{
	struct mpcore_wdt *wdt = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	misc_deregister(&mpcore_wdt_miscdev);

	mpcore_wdt_dev = NULL;

	free_irq(wdt->irq, wdt);
	iounmap(wdt->base);
	kfree(wdt);
	return 0;
}

/* work with hotplug and coldplug */
MODULE_ALIAS("platform:mpcore_wdt");

static struct platform_driver mpcore_wdt_driver = {
	.probe		= mpcore_wdt_probe,
	.remove		= __devexit_p(mpcore_wdt_remove),
	.shutdown	= mpcore_wdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "mpcore_wdt",
	},
};

#if 1
static struct platform_device mpcore_wdt_device = {
		.name		= "mpcore_wdt",
		.id		= -1,
};
#endif


static int __init mpcore_wdt_init(void)
{
	int ret = -1;

	printk("MPcore Watchdog Timer: 0.1. "
		"mpcore_noboot=%d mpcore_margin=%d sec (nowayout= %d)\n", mpcore_noboot, mpcore_margin, nowayout);
#if 1
	ret = platform_device_register(&mpcore_wdt_device);
	if (ret) {
		printk(DEBUG_INFO"Unable to device register(%d)\n", ret);
		return ret;
	}
#endif	
	ret = platform_driver_register(&mpcore_wdt_driver);
	if (ret) {
		printk(DEBUG_INFO"Unable to driver register(%d)\n", ret);
		return ret;
	}
	printk(DEBUG_INFO"MPcore init\n");
	return 0;
}

static void __exit mpcore_wdt_exit(void)
{
	platform_driver_unregister(&mpcore_wdt_driver);
	platform_device_unregister(&mpcore_wdt_device);
}

module_init(mpcore_wdt_init);
module_exit(mpcore_wdt_exit);

