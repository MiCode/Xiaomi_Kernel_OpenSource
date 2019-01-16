#include "wake_gesture.h"

static struct wag_context *wag_context_obj = NULL;

static struct wag_init_info* wake_gesture_init= {0}; //modified
static void wag_early_suspend(struct early_suspend *h);
static void wag_late_resume(struct early_suspend *h);

static int resume_enable_status = 0;
static struct wake_lock wag_lock; 
static void notify_wag_timeout(unsigned long);

static struct wag_context *wag_context_alloc_object(void)
{
	struct wag_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    	WAG_LOG("wag_context_alloc_object++++\n");
	if(!obj)
	{
		WAG_ERR("Alloc wag object error!\n");
		return NULL;
	}	
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->wag_op_mutex);

	WAG_LOG("wag_context_alloc_object----\n");
	return obj;
}

static void notify_wag_timeout(unsigned long data)
{
    wake_unlock(&wag_lock);
}

int wag_notify()
{
	int err=0;
	int value=0;
	struct wag_context *cxt = NULL;
  	cxt = wag_context_obj;
	WAG_LOG("wag_notify++++\n");
	
	value =1;
	input_report_rel(cxt->idev, EVENT_TYPE_WAG_VALUE, value);
	input_sync(cxt->idev); 

	wake_lock(&wag_lock);
	mod_timer(&cxt->notify_timer, jiffies + HZ/5);
	
	return err;
}

static int wag_real_enable(int enable)
{
	int err =0;
	struct wag_context *cxt = NULL;
	cxt = wag_context_obj;
	
	if(WAG_RESUME == enable)
	{
		enable = resume_enable_status;
	}
	
	if(1==enable)
	{
		resume_enable_status = 1;
		if(atomic_read(&(wag_context_obj->early_suspend))) //not allow to enable under suspend
		{
			return 0;
		}
		if(false==cxt->is_active_data)
		{
			err = cxt->wag_ctl.open_report_data(1);
			if(err)
			{ 
				err = cxt->wag_ctl.open_report_data(1);
				if(err)
				{
					err = cxt->wag_ctl.open_report_data(1);
					if(err)
					{
						WAG_ERR("enable_wake_gesture enable(%d) err 3 timers = %d\n", enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			WAG_LOG("enable_wake_gesture real enable  \n" );
		}
	}
	else if((0==enable) || (WAG_SUSPEND == enable))
	{
	if(0==enable)
			resume_enable_status = 0;
		if(true==cxt->is_active_data)
		{
			err = cxt->wag_ctl.open_report_data(0);
			if(err)
			{ 
				WAG_ERR("enable_wake_gestureenable(%d) err = %d\n", enable, err);
			}
			cxt->is_active_data =false;
			WAG_LOG("enable_wake_gesture real disable  \n" );
		} 
	}
	return err;
}

int wag_enable_nodata(int enable)
{
	struct wag_context *cxt = NULL;
	cxt = wag_context_obj;
	if(NULL  == cxt->wag_ctl.open_report_data)
	{
		WAG_ERR("wag_enable_nodata:wag ctl path is NULL\n");
		return -1;
	}

	if(1 == enable)
	{
		cxt->is_active_nodata = true;
	}
	if(0 == enable)
	{
		cxt->is_active_nodata = false;
	}
	wag_real_enable(enable);
	return 0;
}

static ssize_t wag_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct wag_context *cxt = NULL;
	cxt = wag_context_obj;
	
	WAG_LOG("wag active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata); 
}

static ssize_t wag_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct wag_context *cxt = NULL;
	WAG_LOG("wag_store_enable nodata buf=%s\n",buf);
	mutex_lock(&wag_context_obj->wag_op_mutex);
	cxt = wag_context_obj;
	if(NULL == cxt->wag_ctl.open_report_data)
	{
		WAG_LOG("wag_ctl enable nodata NULL\n");
		mutex_unlock(&wag_context_obj->wag_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		wag_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
		wag_enable_nodata(0);
	}
	else
	{
		WAG_ERR(" wag_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&wag_context_obj->wag_op_mutex);
	return count;
}

static ssize_t wag_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct wag_context *cxt = NULL;
	int res =0;
	int en=0;
	WAG_LOG("wag_store_active buf=%s\n",buf);
	mutex_lock(&wag_context_obj->wag_op_mutex);
	
	cxt = wag_context_obj;
	if((res = sscanf(buf, "%d", &en))!=1)
	{
		WAG_LOG(" wag_store_active param error: res = %d\n", res);
	}
	WAG_LOG(" wag_store_active en=%d\n",en);
	if(1 == en)
	{
		wag_real_enable(1);
	}
	else if(0 == en)
	{
		wag_real_enable(0);
	}
	else
	{
		WAG_ERR(" wag_store_active error !!\n");
	}
	mutex_unlock(&wag_context_obj->wag_op_mutex);
	WAG_LOG(" wag_store_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t wag_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct wag_context *cxt = NULL;
	cxt = wag_context_obj;

	WAG_LOG("wag active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data); 
}

static ssize_t wag_store_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	WAG_LOG(" not support now\n");
	return len;
}


static ssize_t wag_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	WAG_LOG(" not support now\n");
	return len;
}


static ssize_t wag_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	WAG_LOG(" not support now\n");
	return len;
}

static ssize_t wag_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	WAG_LOG(" not support now\n");
	return len;
}

static ssize_t wag_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	WAG_LOG(" not support now\n");
	return len;
}

static ssize_t wag_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	WAG_LOG(" not support now\n");
	return len;
}

static ssize_t wag_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	char *devname = NULL;
	devname = dev_name(&wag_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5);  //TODO: why +5?
}
static int wake_gesture_remove(struct platform_device *pdev)
{
	WAG_LOG("wake_gesture_remove\n");
	return 0;
}

static int wake_gesture_probe(struct platform_device *pdev) 
{
	WAG_LOG("wake_gesture_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id wake_gesture_of_match[] = {
	{ .compatible = "mediatek,wake_gesture", },
	{},
};
#endif

static struct platform_driver wake_gesture_driver = {
	.probe      = wake_gesture_probe,
	.remove     = wake_gesture_remove,    
	.driver     = 
	{
		.name  = "wake_gesture",
		#ifdef CONFIG_OF
		.of_match_table = wake_gesture_of_match,
		#endif
	}
};

static int wag_real_driver_init(void) 
{
	int err=0;
	WAG_LOG(" wag_real_driver_init +\n");
	if(0 != wake_gesture_init)
	{
		WAG_LOG(" wag try to init driver %s\n", wake_gesture_init->name);
		err = wake_gesture_init->init();
		if(0 == err)
		{
			WAG_LOG(" wag real driver %s probe ok\n", wake_gesture_init->name);
		}
	}
	wake_lock_init(&wag_lock,WAKE_LOCK_SUSPEND,"wag wakelock");
	init_timer(&wag_context_obj->notify_timer);
	wag_context_obj->notify_timer.expires	= HZ/5; //200 ms
	wag_context_obj->notify_timer.function	= notify_wag_timeout;
	wag_context_obj->notify_timer.data	= (unsigned long)wag_context_obj;

	return err;
}

int wag_driver_add(struct wag_init_info* obj) 
{
	int err=0;
	
	WAG_FUN();
	WAG_LOG("register wake_gesture driver for the first time\n");
	if(platform_driver_register(&wake_gesture_driver))
	{
		WAG_ERR("failed to register gensor driver already exist\n");
	}
	if(NULL == wake_gesture_init)
	{
		obj->platform_diver_addr = &wake_gesture_driver;
		wake_gesture_init = obj;
	}

	if(NULL==wake_gesture_init)
	{
		WAG_ERR("WAG driver add err \n");
		err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(wag_driver_add);

static int wag_misc_init(struct wag_context *cxt)
{
	int err=0;
    //kernel-3.10\include\linux\Miscdevice.h
    //use MISC_DYNAMIC_MINOR exceed 64
	cxt->mdev.minor = M_WAG_MISC_MINOR;
	cxt->mdev.name  = WAG_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		WAG_ERR("unable to register wag misc device!!\n");
	}
	return err;
}

static void wag_input_destroy(struct wag_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int wag_input_init(struct wag_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = WAG_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_WAG_VALUE);
	
	input_set_drvdata(dev, cxt);
	set_bit(EV_REL, dev->evbit);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev= dev;

	return 0;
}

DEVICE_ATTR(wagenablenodata,     	S_IWUSR | S_IRUGO, wag_show_enable_nodata, wag_store_enable_nodata);
DEVICE_ATTR(wagactive,     		S_IWUSR | S_IRUGO, wag_show_active, wag_store_active);
DEVICE_ATTR(wagdelay,      		S_IWUSR | S_IRUGO, wag_show_delay,  wag_store_delay);
DEVICE_ATTR(wagbatch,      		S_IWUSR | S_IRUGO, wag_show_batch,  wag_store_batch);
DEVICE_ATTR(wagflush,      			S_IWUSR | S_IRUGO, wag_show_flush,  wag_store_flush);
DEVICE_ATTR(wagdevnum,      			S_IWUSR | S_IRUGO, wag_show_devnum,  NULL);


static struct attribute *wag_attributes[] = {
	&dev_attr_wagenablenodata.attr,
	&dev_attr_wagactive.attr,
	&dev_attr_wagdelay.attr,
	&dev_attr_wagbatch.attr,
	&dev_attr_wagflush.attr,
	&dev_attr_wagdevnum.attr,
	NULL
};

static struct attribute_group wag_attribute_group = {
	.attrs = wag_attributes
};

int wag_register_data_path(struct wag_data_path *data)
{
	struct wag_context *cxt = NULL;
	cxt = wag_context_obj;
	cxt->wag_data.get_data = data->get_data;
	if(NULL == cxt->wag_data.get_data)
	{
		WAG_LOG("wag register data path fail \n");
	 	return -1;
	}
	return 0;
}

int wag_register_control_path(struct wag_control_path *ctl)
{
	struct wag_context *cxt = NULL;
	int err =0;
	cxt = wag_context_obj;
//	cxt->wag_ctl.enable = ctl->enable;
//	cxt->wag_ctl.enable_nodata = ctl->enable_nodata;
	cxt->wag_ctl.open_report_data = ctl->open_report_data;
	
	if(NULL==cxt->wag_ctl.open_report_data)
	{
		WAG_LOG("wag register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = wag_misc_init(wag_context_obj);
	if(err)
	{
		WAG_ERR("unable to register wag misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&wag_context_obj->mdev.this_device->kobj,
			&wag_attribute_group);
	if (err < 0)
	{
		WAG_ERR("unable to create wag attribute file\n");
		return -3;
	}
	kobject_uevent(&wag_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;	
}

static int wag_probe(struct platform_device *pdev) 
{
	int err;
	WAG_LOG("+++++++++++++wag_probe!!\n");

	wag_context_obj = wag_context_alloc_object();
	if (!wag_context_obj)
	{
		err = -ENOMEM;
		WAG_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	//init real wag driver
    	err = wag_real_driver_init();
	if(err)
	{
		WAG_ERR("wag real driver init fail\n");
		goto real_driver_init_fail;
	}

	//init input dev
	err = wag_input_init(wag_context_obj);
	if(err)
	{
		WAG_ERR("unable to register wag input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
    	atomic_set(&(wag_context_obj->early_suspend), 0);
	wag_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	wag_context_obj->early_drv.suspend  = wag_early_suspend,
	wag_context_obj->early_drv.resume   = wag_late_resume,    
	register_early_suspend(&wag_context_obj->early_drv);
#endif //#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
  
	WAG_LOG("----wag_probe OK !!\n");
	return 0;

	if (err)
	{
	   WAG_ERR("sysfs node creation error \n");
	   wag_input_destroy(wag_context_obj);
	}
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(wag_context_obj);
	exit_alloc_data_failed:
	WAG_LOG("----wag_probe fail !!!\n");
	return err;
}

static int wag_remove(struct platform_device *pdev)
{
	int err=0;
	WAG_FUN(f);
	input_unregister_device(wag_context_obj->idev);        
	sysfs_remove_group(&wag_context_obj->idev->dev.kobj,
				&wag_attribute_group);
	
	if((err = misc_deregister(&wag_context_obj->mdev)))
	{
		WAG_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(wag_context_obj);
	return 0;
}

static void wag_early_suspend(struct early_suspend *h) 
{
	atomic_set(&(wag_context_obj->early_suspend), 1);
	if(!atomic_read(&wag_context_obj->wake)) //not wake up, disable in early suspend
	{
		wag_real_enable(WAG_SUSPEND);
	}
	WAG_LOG(" wag_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(wag_context_obj->early_suspend)));
	return ;
}
/*----------------------------------------------------------------------------*/
static void wag_late_resume(struct early_suspend *h)
{
	atomic_set(&(wag_context_obj->early_suspend), 0);
	if(!atomic_read(&wag_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		wag_real_enable(WAG_RESUME);
	}
	WAG_LOG(" wag_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(wag_context_obj->early_suspend)));
	return ;
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int wag_suspend(struct platform_device *dev, pm_message_t state) 
{
	atomic_set(&(wag_context_obj->suspend), 1);
	if(!atomic_read(&wag_context_obj->wake)) //not wake up, disable in early suspend
	{
		wag_real_enable(WAG_SUSPEND);
	}
	WAG_LOG(" wag_suspend ok------->hwm_obj->suspend=%d \n",atomic_read(&(wag_context_obj->suspend)));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int wag_resume(struct platform_device *dev)
{
	atomic_set(&(wag_context_obj->suspend), 0);
	if(!atomic_read(&wag_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		wag_real_enable(WAG_RESUME);
	}
	WAG_LOG(" wag_resume ok------->hwm_obj->suspend=%d \n",atomic_read(&(wag_context_obj->suspend)));
	return 0;
}
#endif //#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)

#ifdef CONFIG_OF
static const struct of_device_id m_wag_pl_of_match[] = {
	{ .compatible = "mediatek,m_wag_pl", },
	{},
};
#endif

static struct platform_driver wag_driver =
{
	.probe      = wag_probe,
	.remove     = wag_remove,    
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend    = wag_suspend,
	.resume     = wag_resume,
#endif
	.driver     = 
	{
		.name = WAG_PL_DEV_NAME,
		#ifdef CONFIG_OF
		.of_match_table = m_wag_pl_of_match,
		#endif
	}
};

static int __init wag_init(void) 
{
	WAG_FUN();

	if(platform_driver_register(&wag_driver))
	{
		WAG_ERR("failed to register wag driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit wag_exit(void)
{
	platform_driver_unregister(&wag_driver); 
	platform_driver_unregister(&wake_gesture_driver);      
}

late_initcall(wag_init);
//module_init(wag_init);
//module_exit(wag_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WAG device driver");
MODULE_AUTHOR("Mediatek");

