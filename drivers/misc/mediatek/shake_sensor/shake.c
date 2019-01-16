#include "shake.h"

static struct shk_context *shk_context_obj = NULL;

static struct shk_init_info* shake_init= {0}; //modified
static void shk_early_suspend(struct early_suspend *h);
static void shk_late_resume(struct early_suspend *h);

static int resume_enable_status = 0;

static struct shk_context *shk_context_alloc_object(void)
{
	struct shk_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    	SHK_LOG("shk_context_alloc_object++++\n");
	if(!obj)
	{
		SHK_ERR("Alloc shk object error!\n");
		return NULL;
	}	
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->shk_op_mutex);

	SHK_LOG("shk_context_alloc_object----\n");
	return obj;
}

int shk_notify()
{
	int err=0;
	int value=0;
	struct shk_context *cxt = NULL;
  	cxt = shk_context_obj;
	SHK_LOG("shk_notify++++\n");
	
	value = 1;
	input_report_rel(cxt->idev, EVENT_TYPE_SHK_VALUE, value);
	input_sync(cxt->idev); 
	
	return err;
}

static int shk_real_enable(int enable)
{
	int err =0;
	struct shk_context *cxt = NULL;
	cxt = shk_context_obj;

	if(SHK_RESUME == enable)
	{
		enable = resume_enable_status;
	}

	if(1==enable)
	{
		resume_enable_status = 1;
		if(atomic_read(&(shk_context_obj->early_suspend))) //not allow to enable under suspend
		{
			return 0;
		}
		if(false==cxt->is_active_data)
		{
			err = cxt->shk_ctl.open_report_data(1);
			if(err)
			{ 
				err = cxt->shk_ctl.open_report_data(1);
				if(err)
				{
					err = cxt->shk_ctl.open_report_data(1);
					if(err)
					{
						SHK_ERR("enable_shake enable(%d) err 3 timers = %d\n", enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			SHK_LOG("enable_shake real enable  \n" );
		}
	}
	else if((0==enable) || (SHK_SUSPEND == enable))
	{
		if(0==enable)
			resume_enable_status = 0;
		if(true==cxt->is_active_data)
		{
			err = cxt->shk_ctl.open_report_data(0);
			if(err)
			{ 
				SHK_ERR("enable_shakeenable(%d) err = %d\n", enable, err);
			}
			cxt->is_active_data =false;
			SHK_LOG("enable_shake real disable  \n" );
		} 
	}
	return err;
}

int shk_enable_nodata(int enable)
{
	struct shk_context *cxt = NULL;
	cxt = shk_context_obj;
	if(NULL  == cxt->shk_ctl.open_report_data)
	{
		SHK_ERR("shk_enable_nodata:shk ctl path is NULL\n");
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
	shk_real_enable(enable);
	return 0;
}

static ssize_t shk_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct shk_context *cxt = NULL;
	cxt = shk_context_obj;
	
	SHK_LOG("shk active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata); 
}

static ssize_t shk_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct shk_context *cxt = NULL;
	SHK_LOG("shk_store_enable nodata buf=%s\n",buf);
	mutex_lock(&shk_context_obj->shk_op_mutex);
	cxt = shk_context_obj;
	if(NULL == cxt->shk_ctl.open_report_data)
	{
		SHK_LOG("shk_ctl enable nodata NULL\n");
		mutex_unlock(&shk_context_obj->shk_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		shk_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
		shk_enable_nodata(0);
    	}
	else
	{
		SHK_ERR(" shk_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&shk_context_obj->shk_op_mutex);
	return count;
}

static ssize_t shk_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct shk_context *cxt = NULL;
	int res =0;
	int en=0;
	SHK_LOG("shk_store_active buf=%s\n",buf);
	mutex_lock(&shk_context_obj->shk_op_mutex);
	
	cxt = shk_context_obj;
	if((res = sscanf(buf, "%d", &en))!=1)
	{
		SHK_LOG(" shk_store_active param error: res = %d\n", res);
	}
	SHK_LOG(" shk_store_active en=%d\n",en);
	if(1 == en)
	{
		shk_real_enable(1);
	}
	else if(0 == en)
	{
		shk_real_enable(0);
	}
	else
	{
		SHK_ERR(" shk_store_active error !!\n");
	}
	mutex_unlock(&shk_context_obj->shk_op_mutex);
	SHK_LOG(" shk_store_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t shk_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct shk_context *cxt = NULL;
	cxt = shk_context_obj;

	SHK_LOG("shk active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data); 
}

static ssize_t shk_store_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	SHK_LOG(" not support now\n");
	return len;
}


static ssize_t shk_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	SHK_LOG(" not support now\n");
	return len;
}


static ssize_t shk_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	SHK_LOG(" not support now\n");
	return len;
}

static ssize_t shk_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	SHK_LOG(" not support now\n");
	return len;
}

static ssize_t shk_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	SHK_LOG(" not support now\n");
	return len;
}

static ssize_t shk_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	SHK_LOG(" not support now\n");
	return len;
}

static ssize_t shk_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	char *devname = NULL;
	devname = dev_name(&shk_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5);  //TODO: why +5?
}
static int shake_remove(struct platform_device *pdev)
{
	SHK_LOG("shake_remove\n");
	return 0;
}

static int shake_probe(struct platform_device *pdev) 
{
	SHK_LOG("shake_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id shake_of_match[] = {
	{ .compatible = "mediatek,shake", },
	{},
};
#endif

static struct platform_driver shake_driver = {
	.probe      = shake_probe,
	.remove     = shake_remove,    
	.driver     = 
	{
		.name  = "shake",
		#ifdef CONFIG_OF
		.of_match_table = shake_of_match,
		#endif
	}
};

static int shk_real_driver_init(void) 
{
	int err=0;
	SHK_LOG(" shk_real_driver_init +\n");
	if(0 != shake_init)
	{
		SHK_LOG(" shk try to init driver %s\n", shake_init->name);
		err = shake_init->init();
		if(0 == err)
		{
			SHK_LOG(" shk real driver %s probe ok\n", shake_init->name);
		}
	}
	return err;
}

int shk_driver_add(struct shk_init_info* obj) 
{
	int err=0;
	
	SHK_FUN();
	SHK_LOG("register shake driver for the first time\n");
	if(platform_driver_register(&shake_driver))
	{
		SHK_ERR("failed to register gensor driver already exist\n");
	}
	if(NULL == shake_init)
	{
		obj->platform_diver_addr = &shake_driver;
		shake_init = obj;
	}

	if(NULL==shake_init)
	{
		SHK_ERR("SHK driver add err \n");
		err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(shk_driver_add);

static int shk_misc_init(struct shk_context *cxt)
{
	int err=0;
    //kernel-3.10\include\linux\Miscdevice.h
    //use MISC_DYNAMIC_MINOR exceed 64
	cxt->mdev.minor = M_SHK_MISC_MINOR;
	cxt->mdev.name  = SHK_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		SHK_ERR("unable to register shk misc device!!\n");
	}
	return err;
}

static void shk_input_destroy(struct shk_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int shk_input_init(struct shk_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = SHK_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_SHK_VALUE);
	
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

DEVICE_ATTR(shkenablenodata,     	S_IWUSR | S_IRUGO, shk_show_enable_nodata, shk_store_enable_nodata);
DEVICE_ATTR(shkactive,     		S_IWUSR | S_IRUGO, shk_show_active, shk_store_active);
DEVICE_ATTR(shkdelay,      		S_IWUSR | S_IRUGO, shk_show_delay,  shk_store_delay);
DEVICE_ATTR(shkbatch,      		S_IWUSR | S_IRUGO, shk_show_batch,  shk_store_batch);
DEVICE_ATTR(shkflush,      			S_IWUSR | S_IRUGO, shk_show_flush,  shk_store_flush);
DEVICE_ATTR(shkdevnum,      			S_IWUSR | S_IRUGO, shk_show_devnum,  NULL);


static struct attribute *shk_attributes[] = {
	&dev_attr_shkenablenodata.attr,
	&dev_attr_shkactive.attr,
	&dev_attr_shkdelay.attr,
	&dev_attr_shkbatch.attr,
	&dev_attr_shkflush.attr,
	&dev_attr_shkdevnum.attr,
	NULL
};

static struct attribute_group shk_attribute_group = {
	.attrs = shk_attributes
};

int shk_register_data_path(struct shk_data_path *data)
{
	struct shk_context *cxt = NULL;
	cxt = shk_context_obj;
	cxt->shk_data.get_data = data->get_data;
	if(NULL == cxt->shk_data.get_data)
	{
		SHK_LOG("shk register data path fail \n");
	 	return -1;
	}
	return 0;
}

int shk_register_control_path(struct shk_control_path *ctl)
{
	struct shk_context *cxt = NULL;
	int err =0;
	cxt = shk_context_obj;
//	cxt->shk_ctl.enable = ctl->enable;
//	cxt->shk_ctl.enable_nodata = ctl->enable_nodata;
	cxt->shk_ctl.open_report_data = ctl->open_report_data;
	
	if(NULL==cxt->shk_ctl.open_report_data)
	{
		SHK_LOG("shk register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = shk_misc_init(shk_context_obj);
	if(err)
	{
		SHK_ERR("unable to register shk misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&shk_context_obj->mdev.this_device->kobj,
			&shk_attribute_group);
	if (err < 0)
	{
		SHK_ERR("unable to create shk attribute file\n");
		return -3;
	}
	kobject_uevent(&shk_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;	
}

static int shk_probe(struct platform_device *pdev) 
{
	int err;
	SHK_LOG("+++++++++++++shk_probe!!\n");

	shk_context_obj = shk_context_alloc_object();
	if (!shk_context_obj)
	{
		err = -ENOMEM;
		SHK_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	//init real shk driver
    	err = shk_real_driver_init();
	if(err)
	{
		SHK_ERR("shk real driver init fail\n");
		goto real_driver_init_fail;
	}

	//init input dev
	err = shk_input_init(shk_context_obj);
	if(err)
	{
		SHK_ERR("unable to register shk input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
    	atomic_set(&(shk_context_obj->early_suspend), 0);
	shk_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	shk_context_obj->early_drv.suspend  = shk_early_suspend,
	shk_context_obj->early_drv.resume   = shk_late_resume,    
	register_early_suspend(&shk_context_obj->early_drv);
#endif //#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
  
	SHK_LOG("----shk_probe OK !!\n");
	return 0;


	if (err)
	{
	   SHK_ERR("sysfs node creation error \n");
	   shk_input_destroy(shk_context_obj);
	}
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(shk_context_obj);
	exit_alloc_data_failed:
	SHK_LOG("----shk_probe fail !!!\n");
	return err;
}

static int shk_remove(struct platform_device *pdev)
{
	int err=0;
	SHK_FUN(f);
	input_unregister_device(shk_context_obj->idev);        
	sysfs_remove_group(&shk_context_obj->idev->dev.kobj,
				&shk_attribute_group);
	
	if((err = misc_deregister(&shk_context_obj->mdev)))
	{
		SHK_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(shk_context_obj);
	return 0;
}

static void shk_early_suspend(struct early_suspend *h) 
{
	atomic_set(&(shk_context_obj->early_suspend), 1);
	if(!atomic_read(&shk_context_obj->wake)) //not wake up, disable in early suspend
	{
		shk_real_enable(SHK_SUSPEND);
	}
	SHK_LOG(" shk_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(shk_context_obj->early_suspend)));
	return ;
}
/*----------------------------------------------------------------------------*/
static void shk_late_resume(struct early_suspend *h)
{
	atomic_set(&(shk_context_obj->early_suspend), 0);
	if(!atomic_read(&shk_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		shk_real_enable(SHK_RESUME);
	}
	SHK_LOG(" shk_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(shk_context_obj->early_suspend)));
	return ;
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int shk_suspend(struct platform_device *dev, pm_message_t state) 
{
	atomic_set(&(shk_context_obj->suspend), 1);
	if(!atomic_read(&shk_context_obj->wake)) //not wake up, disable in early suspend
	{
		shk_real_enable(SHK_SUSPEND);
	}
	SHK_LOG(" shk_early_suspend ok------->hwm_obj->suspend=%d \n",atomic_read(&(shk_context_obj->suspend)));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int shk_resume(struct platform_device *dev)
{
	atomic_set(&(shk_context_obj->suspend), 0);
	if(!atomic_read(&shk_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		shk_real_enable(SHK_RESUME);
	}
	SHK_LOG(" shk_resume ok------->hwm_obj->suspend=%d \n",atomic_read(&(shk_context_obj->suspend)));
	return 0;
}
#endif //#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)

#ifdef CONFIG_OF
static const struct of_device_id m_shk_pl_of_match[] = {
	{ .compatible = "mediatek,m_shk_pl", },
	{},
};
#endif

static struct platform_driver shk_driver =
{
	.probe      = shk_probe,
	.remove     = shk_remove,    
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend    = shk_suspend,
	.resume     = shk_resume,
#endif
	.driver     = 
	{
		.name = SHK_PL_DEV_NAME,
		#ifdef CONFIG_OF
		.of_match_table = m_shk_pl_of_match,
		#endif
	}
};

static int __init shk_init(void) 
{
	SHK_FUN();

	if(platform_driver_register(&shk_driver))
	{
		SHK_ERR("failed to register shk driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit shk_exit(void)
{
	platform_driver_unregister(&shk_driver); 
	platform_driver_unregister(&shake_driver);      
}

late_initcall(shk_init);
//module_init(shk_init);
//module_exit(shk_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SHK device driver");
MODULE_AUTHOR("Mediatek");

