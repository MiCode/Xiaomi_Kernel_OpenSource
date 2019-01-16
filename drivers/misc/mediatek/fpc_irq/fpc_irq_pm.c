/* Fingerprint Cards, Hybrid Touch sensor driver
 *
 * Copyright (c) 2014,2015 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 *
 * Software license : "Dual BSD/GPL"
 * see <linux/module.h> and ./Documentation
 * for  details.
 *
*/

#define DEBUG

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
//#include <linux/wakelock.h>

#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
#include <linux/earlysuspend.h>
#warning "Early suspend, not tested !"
#endif

#include "fpc_irq_common.h"
#include "fpc_irq_pm.h"

/* -------------------------------------------------------------------------- */
/* function prototypes                                                        */
/* -------------------------------------------------------------------------- */
static int fpc_irq_pm_issue_wakeup(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_pm_suspend_ack(fpc_irq_data_t *fpc_irq_data);

static int fpc_irq_pm_resume_ack(fpc_irq_data_t *fpc_irq_data);

#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
static void fpc_irq_early_suspend(struct early_suspend *h);
static void fpc_irq_late_resume(struct early_suspend *h);

static int fpc_irq_pm_suspend_early_ack(fpc_irq_data_t *fpc_irq_data);
static int fpc_irq_pm_resume_late_ack(fpc_irq_data_t *fpc_irq_data);
#endif

/* -------------------------------------------------------------------------- */
/* fpc_irq driver constants                                                   */
/* -------------------------------------------------------------------------- */
#define FPC_IRQ_INPUT_DEV_NAME	"fpc_irq_wakeup"

#define FPC_IRQ_KEY_WAKEUP	KEY_F18	/* 188*/

#define FPC_IRQ_PM_SUSPEND_TIMEOUT (1 * HZ) /* jiffies */
#define FPC_IRQ_PM_RESUME_TIMEOUT  (1 * HZ) /* jiffies */

extern fpc_irq_data_t *fpc_irq;
//static struct wake_lock fpc_lock;
/* -------------------------------------------------------------------------- */
/* function definitions                                                       */
/* -------------------------------------------------------------------------- */
int fpc_irq_pm_init(fpc_irq_data_t *fpc_irq_data)
{
	int ret = 0;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	fpc_irq_data->pm.state = -EINVAL;

	fpc_irq_data->pm.supply_on      =
	fpc_irq_data->pm.hw_reset       =
	fpc_irq_data->pm.notify_enabled =
	fpc_irq_data->pm.wakeup_req     =
	fpc_irq_data->pm.notify_ack     = false;
	//wake_lock_init(&fpc_lock, WAKE_LOCK_SUSPEND, "fpc wakelock") ;

	sema_init(&fpc_irq_data->pm_mutex, 0);

	fpc_irq_data->input_dev = input_allocate_device();

	if (!fpc_irq_data->input_dev) {
		dev_err(fpc_irq_data->dev,
			"%s, Input_allocate_device failed.\n",
			__func__);

		ret = -ENODEV;
	}


	if (!ret) {
		fpc_irq_data->input_dev->name = FPC_IRQ_INPUT_DEV_NAME;

		set_bit(EV_KEY,	fpc_irq_data->input_dev->evbit);

		set_bit(FPC_IRQ_KEY_WAKEUP, fpc_irq_data->input_dev->keybit);

		set_bit(FPC_IRQ_KEY_CLICK, fpc_irq_data->input_dev->keybit);
		set_bit(FPC_IRQ_KEY_RIGHT, fpc_irq_data->input_dev->keybit);
		set_bit(FPC_IRQ_KEY_LEFT, fpc_irq_data->input_dev->keybit);
		set_bit(FPC_IRQ_KEY_UP, fpc_irq_data->input_dev->keybit);
		set_bit(FPC_IRQ_KEY_DOWN, fpc_irq_data->input_dev->keybit);		
		ret = input_register_device(fpc_irq_data->input_dev);
	}

	if (ret) {
		dev_err(fpc_irq_data->dev,
			"%s, Input_register_device failed.\n",
			__func__);

		input_free_device(fpc_irq_data->input_dev);
			
		fpc_irq_data->input_dev = NULL;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND_DEL

#warning "Early suspend, setup, not tested !"
	fpc_irq_data->pm_early.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	fpc_irq_data->pm_early.suspend = fpc_irq_early_suspend;
	fpc_irq_data->pm_early.resume  = fpc_irq_late_resume;
	
	register_early_suspend(&fpc_irq_data->pm_early);
#endif

	fpc_irq_data->pm.state = FPC_IRQ_STATE_ACTIVE;

	up(&fpc_irq_data->pm_mutex);

	return ret;
}


/* -------------------------------------------------------------------------- */
int fpc_irq_pm_destroy(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (fpc_irq_data->input_dev != NULL)
		input_free_device(fpc_irq_data->input_dev);

#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
#warning "Early suspend, tear down, not tested !"
	unregister_early_suspend(&fpc_irq_data->pm_early);
#endif

	return 0;
}


/* -------------------------------------------------------------------------- */
int fpc_irq_pm_notify_enable(fpc_irq_data_t *fpc_irq_data, int req_state)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
	
	if (req_state == 0) {
		if (fpc_irq_data->pm.notify_enabled) {
			fpc_irq_data->pm.notify_enabled = false;
		}
	} else {
		if (!fpc_irq_data->pm.notify_enabled) {
			fpc_irq_data->pm.notify_enabled = true;
		}
	}
	return 0;
}


/* -------------------------------------------------------------------------- */
int fpc_irq_pm_notify_ack(fpc_irq_data_t *fpc_irq_data, int val)
{
        printk(" %s val=%d\n",__func__,val);
	switch(val) {
	case FPC_IRQ_SIGNAL_SUSPEND_REQ:
		return fpc_irq_pm_suspend_ack(fpc_irq_data);

	case FPC_IRQ_SIGNAL_RESUME_REQ:
		return fpc_irq_pm_resume_ack(fpc_irq_data);

#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
//#warning "early suspend/late resume, not tested !"
	case FPC_IRQ_SIGNAL_SUSPEND_EARLY_REQ:
		return fpc_irq_pm_suspend_early_ack(fpc_irq_data);

	case FPC_IRQ_SIGNAL_RESUME_LATE_REQ:
		return fpc_irq_pm_resume_late_ack(fpc_irq_data);
#endif

	default:
		return -EINVAL;
	}
}


/* -------------------------------------------------------------------------- */
int fpc_irq_pm_wakeup_req(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

// Todo: prerequisites?

	return fpc_irq_pm_issue_wakeup(fpc_irq_data);
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_pm_issue_wakeup(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	if (fpc_irq_data->input_dev == NULL)
		return -ENODEV;	

	//input_report_key(fpc_irq_data->input_dev, FPC_IRQ_KEY_WAKEUP, 1);
	//input_report_key(fpc_irq_data->input_dev, FPC_IRQ_KEY_WAKEUP, 0);

	//input_sync(fpc_irq_data->input_dev);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_pm_suspend_ack(fpc_irq_data_t *fpc_irq_data)
{
	printk( "%s\n", __func__);
	complete(&fpc_irq_data->pm_suspend_completion);

	return 0;
}


/* -------------------------------------------------------------------------- */
static int fpc_irq_pm_resume_ack(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
	complete(&fpc_irq_data->pm_resume_completion);

	return 0;
}


/* -------------------------------------------------------------------------- */
#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
static int fpc_irq_pm_suspend_early_ack(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
	complete(&fpc_irq_data->pm_suspend_early_completion);
}
#endif


/* -------------------------------------------------------------------------- */
#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
static int fpc_irq_pm_resume_late_ack(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
	complete(&fpc_irq_data->pm_resume_late_completion);
}
#endif


/* -------------------------------------------------------------------------- */
int fpc_irq_pm_suspend(struct platform_device *plat_dev, pm_message_t state)
{
	fpc_irq_data_t *fpc_irq_data=fpc_irq;

	fpc_irq_data->suspend_index =true;

	printk(" %s\n",__func__);
	return 0;
#if 0
	long res;
	fpc_irq_data_t *fpc_irq_data;

	dev_dbg(&plat_dev->dev, "%s event = %d\n", __func__, state.event);

	if (fpc_irq_check_instance(plat_dev->name) < 0)
		return 0;

	fpc_irq_data = platform_get_drvdata(plat_dev);

	if (down_interruptible(&fpc_irq_data->pm_mutex))
		return -ERESTARTSYS;

	fpc_irq_data->pm.state = FPC_IRQ_STATE_SUSPENDED;

	if(!fpc_irq_data->pm.notify_enabled) {

		up(&fpc_irq_data->pm_mutex);

		return 0;
	}

	// Note: Possibly we can't use notifications from this state, the
	//       receiver may already be asleep (TBD).
	init_completion(&fpc_irq_data->pm_suspend_completion);

	res = fpc_irq_send_signal(fpc_irq_data->dev,
				fpc_irq_data->setup.dst_pid,
				fpc_irq_data->setup.dst_signo,
				FPC_IRQ_SIGNAL_SUSPEND_REQ);

	if (res < 0) {
		dev_err(&plat_dev->dev, "%s unable to send signal.\n", __func__);

		up(&fpc_irq_data->pm_mutex);

		return res;
	}

	res = wait_for_completion_interruptible_timeout(
					&fpc_irq_data->pm_suspend_completion,
					FPC_IRQ_PM_SUSPEND_TIMEOUT);

	up(&fpc_irq_data->pm_mutex);

	return (res == 0)? -ETIMEDOUT : /* timeout */
		(res > 0) ? 0 : 	/* completed */
		res;			/* interrupted */
#endif

}


/* -------------------------------------------------------------------------- */
int fpc_irq_pm_resume(struct platform_device *plat_dev)
{
	//fpc_irq_data_t *fpc_irq_data=fpc_irq;

	//fpc_irq_data->suspend_index =false;
	
	printk(" %s\n",__func__);
	return 0;
#if 0
	long res;
	fpc_irq_data_t *fpc_irq_data;

	dev_dbg(&plat_dev->dev, "%s\n", __func__);

	if (fpc_irq_check_instance(plat_dev->name) < 0)
		return 0;

	fpc_irq_data = platform_get_drvdata(plat_dev);

	if (down_interruptible(&fpc_irq_data-> pm_mutex))
		return -ERESTARTSYS;

	fpc_irq_data->pm.state = FPC_IRQ_STATE_ACTIVE;

	if(!fpc_irq_data->pm.notify_enabled) {

		up(&fpc_irq_data->pm_mutex);

		return 0;
	}

	init_completion(&fpc_irq_data->pm_resume_completion);

	res = fpc_irq_send_signal(fpc_irq_data->dev,
				fpc_irq_data->setup.dst_pid,
				fpc_irq_data->setup.dst_signo,
				FPC_IRQ_SIGNAL_RESUME_REQ);
	if (res < 0) {
		dev_err(&plat_dev->dev, "%s unable to send signal.\n", __func__);

		up(&fpc_irq_data->pm_mutex);

		return res;
	}

	res = wait_for_completion_interruptible_timeout(
					&fpc_irq_data->pm_resume_completion,
					FPC_IRQ_PM_RESUME_TIMEOUT);

	up(&fpc_irq_data->pm_mutex);

	return (res == 0)? -ETIMEDOUT : /* timeout */
		(res > 0) ? 0 : 	/* completed */
		res;			/* interrupted */
#endif
}


/* -------------------------------------------------------------------------- */
#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
#warning "Early suspend, not tested !"
static void fpc_irq_early_suspend(struct early_suspend *__data)
{
	int res = 0;

	fpc_irq_data_t *fpc_irq_data = fpc_irq;
	
        printk(" %s\n",__func__);
		
	dev_dbg(fpc_irq_data->dev, "%s \n", __func__);
	if((fpc_irq_data->init_index)&&(fpc_irq_data->unlock_index))
	{

	fpc_irq_data->pm.state = FPC_IRQ_STATE_EARLY_SUSPEND;

	//if (fpc_irq_data->pm.notify_enabled) {

		init_completion(&fpc_irq_data->pm_suspend_early_completion);

		res = fpc_irq_send_signal(fpc_irq_data->dev,
					fpc_irq_data->setup.dst_pid,
					fpc_irq_data->setup.dst_signo,
					FPC_IRQ_SIGNAL_SUSPEND_EARLY_REQ);

		if (res < 0) {
			dev_err(fpc_irq_data->dev, "%s unable to send signal.\n",__func__);
		}

		res = wait_for_completion_interruptible_timeout(
					&fpc_irq_data->pm_suspend_early_completion,
					FPC_IRQ_PM_SUSPEND_TIMEOUT);

		dev_dbg(fpc_irq_data->dev, "%s %s\n", __func__,
					(res == 0) ?	"Timed out" :
					(res > 0)  ?	"Completed" :
							"Interrupted");
	//}

	fpc_irq_data->suspend_index =true;
	}
	else
	{
	printk(" %s tac not init & unlock is not open tac_init=%d;unlock=%d\n",__func__,fpc_irq_data->init_index,fpc_irq_data->unlock_index);
	}
}
#endif


/* -------------------------------------------------------------------------- */
#ifdef CONFIG_HAS_EARLYSUSPEND_DEL
#warning "Late resume, not tested !"
static void fpc_irq_late_resume(struct early_suspend *__data)
{
        //int res=0;
	fpc_irq_data_t *fpc_irq_data = fpc_irq;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	printk(" %s \n",__func__);

	fpc_irq_data->pm.state = FPC_IRQ_STATE_LATE_RESUME;
	//fpc_irq_data->suspend_index =false;
	
#if 0
	if (fpc_irq_data->pm.notify_enabled) {

		init_completion(&fpc_irq_data->pm_resume_late_completion);

		res = fpc_irq_send_signal(fpc_irq_data->dev,
					fpc_irq_data->setup.dst_pid,
					fpc_irq_data->setup.dst_signo,
					FPC_IRQ_SIGNAL_RESUME_LATE_REQ);

		if (res < 0) {
			dev_err(fpc_irq_data->dev,"%s unable to send signal.\n",__func__);
		}

		res = wait_for_completion_interruptible_timeout(
					&fpc_irq_data->pm_resume_late_completion,
					FPC_IRQ_PM_RESUME_TIMEOUT);

		dev_dbg(fpc_irq_data->dev, "%s %s\n", __func__,
					(res == 0) ?	"Timed out" :
					(res > 0)  ?	"Completed" :
							"Interrupted");
	}
#endif	
}
#endif


/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
int fpc_irq_click_event(fpc_irq_data_t *fpc_irq_data, int val)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);
	static int click_cnt = 0; 
	if (fpc_irq_data->input_dev == NULL) {
		dev_err(fpc_irq_data->dev, "%s - input_dev == NULL !\n", __func__);
		return -ENODEV;
	}
	
	if(val == FPC_IRQ_KEY_CLICK) {
		click_cnt++;
		input_report_key(fpc_irq_data->input_dev, val, click_cnt%2);
		input_sync(fpc_irq_data->input_dev);
				
	} else {
		
		input_report_key(fpc_irq_data->input_dev, val, 1);//up
		input_report_key(fpc_irq_data->input_dev, val, 0);//down
		input_sync(fpc_irq_data->input_dev);
		click_cnt = 1;
	}
	
	return 0;
}
