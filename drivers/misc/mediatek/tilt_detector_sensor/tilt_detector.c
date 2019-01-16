#include "tilt_detector.h"

static struct tilt_context *tilt_context_obj = NULL;

static struct tilt_init_info* tilt_detector_init= {0}; //modified
static void tilt_early_suspend(struct early_suspend *h);
static void tilt_late_resume(struct early_suspend *h);

static int resume_enable_status = 0;

static struct tilt_context *tilt_context_alloc_object(void)
{
	struct tilt_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    	TILT_LOG("tilt_context_alloc_object++++\n");
	if(!obj)
	{
		TILT_ERR("Alloc tilt object error!\n");
		return NULL;
	}	
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->tilt_op_mutex);

	TILT_LOG("tilt_context_alloc_object----\n");
	return obj;
}

int tilt_notify()
{
	int err=0;
	int value=0;
	struct tilt_context *cxt = NULL;
  	cxt = tilt_context_obj;
	TILT_LOG("tilt_notify++++\n");
	
	value =1;
	input_report_rel(cxt->idev, EVENT_TYPE_TILT_VALUE, value);
	input_sync(cxt->idev); 
	
	return err;
}

static int tilt_real_enable(int enable)
{
	int err =0;
	struct tilt_context *cxt = NULL;
	cxt = tilt_context_obj;

	if(TILT_RESUME == enable)
	{
		enable = resume_enable_status;
	}

	if(1==enable)
	{
		resume_enable_status = 1;
		if(atomic_read(&(tilt_context_obj->early_suspend))) //not allow to enable under suspend
		{
			return 0;
		}
		if(false==cxt->is_active_data)
		{
			err = cxt->tilt_ctl.open_report_data(1);
			if(err)
			{ 
				err = cxt->tilt_ctl.open_report_data(1);
				if(err)
				{
					err = cxt->tilt_ctl.open_report_data(1);
					if(err)
					{
						TILT_ERR("enable_tilt_detector enable(%d) err 3 timers = %d\n", enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			TILT_LOG("enable_tilt_detector real enable  \n" );
		}
	}
	else if((0==enable) || (TILT_SUSPEND == enable))
	{
		if(0==enable)
			resume_enable_status = 0;
		if(true==cxt->is_active_data)
		{
			err = cxt->tilt_ctl.open_report_data(0);
			if(err)
			{ 
				TILT_ERR("enable_tilt_detectorenable(%d) err = %d\n", enable, err);
			}
			cxt->is_active_data =false;
			TILT_LOG("enable_tilt_detector real disable  \n" );
		} 
	}
	return err;
}

int tilt_enable_nodata(int enable)
{
	struct tilt_context *cxt = NULL;
	cxt = tilt_context_obj;
	if(NULL  == cxt->tilt_ctl.open_report_data)
	{
		TILT_ERR("tilt_enable_nodata:tilt ctl path is NULL\n");
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
	tilt_real_enable(enable);
	return 0;
}

static ssize_t tilt_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct tilt_context *cxt = NULL;
	cxt = tilt_context_obj;
	
	TILT_LOG("tilt active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata); 
}

static ssize_t tilt_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct tilt_context *cxt = NULL;
	TILT_LOG("tilt_store_enable nodata buf=%s\n",buf);
	mutex_lock(&tilt_context_obj->tilt_op_mutex);
	cxt = tilt_context_obj;
	if(NULL == cxt->tilt_ctl.open_report_data)
	{
		TILT_LOG("tilt_ctl enable nodata NULL\n");
		mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		tilt_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
		tilt_enable_nodata(0);
    	}
	else
	{
		TILT_ERR(" tilt_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	return count;
}

static ssize_t tilt_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct tilt_context *cxt = NULL;
	int res =0;
	int en=0;
	TILT_LOG("tilt_store_active buf=%s\n",buf);
	mutex_lock(&tilt_context_obj->tilt_op_mutex);
	
	cxt = tilt_context_obj;
	if((res = sscanf(buf, "%d", &en))!=1)
	{
		TILT_LOG(" tilt_store_active param error: res = %d\n", res);
	}
	TILT_LOG(" tilt_store_active en=%d\n",en);
	if(1 == en)
	{
		tilt_real_enable(1);
	}
	else if(0 == en)
	{
		tilt_real_enable(0);
	}
	else
	{
		TILT_ERR(" tilt_store_active error !!\n");
	}
	mutex_unlock(&tilt_context_obj->tilt_op_mutex);
	TILT_LOG(" tilt_store_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t tilt_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct tilt_context *cxt = NULL;
	cxt = tilt_context_obj;

	TILT_LOG("tilt active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data); 
}

static ssize_t tilt_store_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	TILT_LOG(" not support now\n");
	return len;
}


static ssize_t tilt_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	TILT_LOG(" not support now\n");
	return len;
}


static ssize_t tilt_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	TILT_LOG(" not support now\n");
	return len;
}

static ssize_t tilt_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	TILT_LOG(" not support now\n");
	return len;
}

static ssize_t tilt_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	TILT_LOG(" not support now\n");
	return len;
}

static ssize_t tilt_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	TILT_LOG(" not support now\n");
	return len;
}

static ssize_t tilt_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	char *devname = NULL;
	devname = dev_name(&tilt_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5);  //TODO: why +5?
}
static int tilt_detector_remove(struct platform_device *pdev)
{
	TILT_LOG("tilt_detector_remove\n");
	return 0;
}

static int tilt_detector_probe(struct platform_device *pdev) 
{
	TILT_LOG("tilt_detector_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tilt_detector_of_match[] = {
	{ .compatible = "mediatek,tilt_detector", },
	{},
};
#endif

static struct platform_driver tilt_detector_driver = {
	.probe      = tilt_detector_probe,
	.remove     = tilt_detector_remove,    
	.driver     = 
	{
		.name  = "tilt_detector",
		#ifdef CONFIG_OF
		.of_match_table = tilt_detector_of_match,
		#endif
	}
};

static int tilt_real_driver_init(void) 
{
	int err=0;
	TILT_LOG(" tilt_real_driver_init +\n");
	if(0 != tilt_detector_init)
	{
		TILT_LOG(" tilt try to init driver %s\n", tilt_detector_init->name);
		err = tilt_detector_init->init();
		if(0 == err)
		{
			TILT_LOG(" tilt real driver %s probe ok\n", tilt_detector_init->name);
		}
	}
	return err;
}

int tilt_driver_add(struct tilt_init_info* obj) 
{
	int err=0;
	
	TILT_FUN();
	TILT_LOG("register tilt_detector driver for the first time\n");
	if(platform_driver_register(&tilt_detector_driver))
	{
		TILT_ERR("failed to register gensor driver already exist\n");
	}
	if(NULL == tilt_detector_init)
	{
		obj->platform_diver_addr = &tilt_detector_driver;
		tilt_detector_init = obj;
	}

	if(NULL==tilt_detector_init)
	{
		TILT_ERR("TILT driver add err \n");
		err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(tilt_driver_add);

static int tilt_misc_init(struct tilt_context *cxt)
{
	int err=0;
    //kernel-3.10\include\linux\Miscdevice.h
    //use MISC_DYNAMIC_MINOR exceed 64
	cxt->mdev.minor = M_TILT_MISC_MINOR;
	cxt->mdev.name  = TILT_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		TILT_ERR("unable to register tilt misc device!!\n");
	}
	return err;
}

static void tilt_input_destroy(struct tilt_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int tilt_input_init(struct tilt_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = TILT_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_TILT_VALUE);
	
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

DEVICE_ATTR(tiltenablenodata,     	S_IWUSR | S_IRUGO, tilt_show_enable_nodata, tilt_store_enable_nodata);
DEVICE_ATTR(tiltactive,     		S_IWUSR | S_IRUGO, tilt_show_active, tilt_store_active);
DEVICE_ATTR(tiltdelay,      		S_IWUSR | S_IRUGO, tilt_show_delay,  tilt_store_delay);
DEVICE_ATTR(tiltbatch,      		S_IWUSR | S_IRUGO, tilt_show_batch,  tilt_store_batch);
DEVICE_ATTR(tiltflush,      			S_IWUSR | S_IRUGO, tilt_show_flush,  tilt_store_flush);
DEVICE_ATTR(tiltdevnum,      			S_IWUSR | S_IRUGO, tilt_show_devnum,  NULL);


static struct attribute *tilt_attributes[] = {
	&dev_attr_tiltenablenodata.attr,
	&dev_attr_tiltactive.attr,
	&dev_attr_tiltdelay.attr,
	&dev_attr_tiltbatch.attr,
	&dev_attr_tiltflush.attr,
	&dev_attr_tiltdevnum.attr,
	NULL
};

static struct attribute_group tilt_attribute_group = {
	.attrs = tilt_attributes
};

int tilt_register_data_path(struct tilt_data_path *data)
{
	struct tilt_context *cxt = NULL;
	cxt = tilt_context_obj;
	cxt->tilt_data.get_data = data->get_data;
	if(NULL == cxt->tilt_data.get_data)
	{
		TILT_LOG("tilt register data path fail \n");
	 	return -1;
	}
	return 0;
}

int tilt_register_control_path(struct tilt_control_path *ctl)
{
	struct tilt_context *cxt = NULL;
	int err =0;
	cxt = tilt_context_obj;
//	cxt->tilt_ctl.enable = ctl->enable;
//	cxt->tilt_ctl.enable_nodata = ctl->enable_nodata;
	cxt->tilt_ctl.open_report_data = ctl->open_report_data;
	
	if(NULL==cxt->tilt_ctl.open_report_data)
	{
		TILT_LOG("tilt register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = tilt_misc_init(tilt_context_obj);
	if(err)
	{
		TILT_ERR("unable to register tilt misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&tilt_context_obj->mdev.this_device->kobj,
			&tilt_attribute_group);
	if (err < 0)
	{
		TILT_ERR("unable to create tilt attribute file\n");
		return -3;
	}
	kobject_uevent(&tilt_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;	
}

static int tilt_probe(struct platform_device *pdev) 
{
	int err;
	TILT_LOG("+++++++++++++tilt_probe!!\n");

	tilt_context_obj = tilt_context_alloc_object();
	if (!tilt_context_obj)
	{
		err = -ENOMEM;
		TILT_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	//init real tilt driver
    	err = tilt_real_driver_init();
	if(err)
	{
		TILT_ERR("tilt real driver init fail\n");
		goto real_driver_init_fail;
	}

	//init input dev
	err = tilt_input_init(tilt_context_obj);
	if(err)
	{
		TILT_ERR("unable to register tilt input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
    	atomic_set(&(tilt_context_obj->early_suspend), 0);
	tilt_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	tilt_context_obj->early_drv.suspend  = tilt_early_suspend,
	tilt_context_obj->early_drv.resume   = tilt_late_resume,    
	register_early_suspend(&tilt_context_obj->early_drv);
#endif //#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
  
	TILT_LOG("----tilt_probe OK !!\n");
	return 0;


	if (err)
	{
	   TILT_ERR("sysfs node creation error \n");
	   tilt_input_destroy(tilt_context_obj);
	}
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(tilt_context_obj);
	exit_alloc_data_failed:
	TILT_LOG("----tilt_probe fail !!!\n");
	return err;
}

static int tilt_remove(struct platform_device *pdev)
{
	int err=0;
	TILT_FUN(f);
	input_unregister_device(tilt_context_obj->idev);        
	sysfs_remove_group(&tilt_context_obj->idev->dev.kobj,
				&tilt_attribute_group);
	
	if((err = misc_deregister(&tilt_context_obj->mdev)))
	{
		TILT_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(tilt_context_obj);
	return 0;
}

static void tilt_early_suspend(struct early_suspend *h) 
{
	atomic_set(&(tilt_context_obj->early_suspend), 1);
	if(!atomic_read(&tilt_context_obj->wake)) //not wake up, disable in early suspend
	{
		tilt_real_enable(TILT_SUSPEND);
	}
	TILT_LOG(" tilt_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(tilt_context_obj->early_suspend)));
	return ;
}
/*----------------------------------------------------------------------------*/
static void tilt_late_resume(struct early_suspend *h)
{
	atomic_set(&(tilt_context_obj->early_suspend), 0);
	if(!atomic_read(&tilt_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		tilt_real_enable(TILT_RESUME);
	}
	TILT_LOG(" tilt_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(tilt_context_obj->early_suspend)));
	return ;
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int tilt_suspend(struct platform_device *dev, pm_message_t state) 
{
	atomic_set(&(tilt_context_obj->suspend), 1);
	if(!atomic_read(&tilt_context_obj->wake)) //not wake up, disable in early suspend
	{
		tilt_real_enable(TILT_SUSPEND);
	}
	TILT_LOG(" tilt_suspend ok------->hwm_obj->suspend=%d \n",atomic_read(&(tilt_context_obj->suspend)));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int tilt_resume(struct platform_device *dev)
{
	atomic_set(&(tilt_context_obj->suspend), 0);
	if(!atomic_read(&tilt_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		tilt_real_enable(TILT_RESUME);
	}
	TILT_LOG(" tilt_resume ok------->hwm_obj->suspend=%d \n",atomic_read(&(tilt_context_obj->suspend)));
	return 0;
}
#endif //#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)

#ifdef CONFIG_OF
static const struct of_device_id m_tilt_pl_of_match[] = {
	{ .compatible = "mediatek,m_tilt_pl", },
	{},
};
#endif

static struct platform_driver tilt_driver =
{
	.probe      = tilt_probe,
	.remove     = tilt_remove,    
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend    = tilt_suspend,
	.resume     = tilt_resume,
#endif
	.driver     = 
	{
		.name = TILT_PL_DEV_NAME,
		#ifdef CONFIG_OF
		.of_match_table = m_tilt_pl_of_match,
		#endif
	}
};

static int __init tilt_init(void) 
{
	TILT_FUN();

	if(platform_driver_register(&tilt_driver))
	{
		TILT_ERR("failed to register tilt driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit tilt_exit(void)
{
	platform_driver_unregister(&tilt_driver); 
	platform_driver_unregister(&tilt_detector_driver);      
}

late_initcall(tilt_init);
//module_init(tilt_init);
//module_exit(tilt_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TILT device driver");
MODULE_AUTHOR("Mediatek");

