#include "in_pocket.h"

static struct inpk_context *inpk_context_obj = NULL;

static struct inpk_init_info* in_pocket_init= {0}; //modified
static void inpk_early_suspend(struct early_suspend *h);
static void inpk_late_resume(struct early_suspend *h);

static int resume_enable_status = 0;

static struct inpk_context *inpk_context_alloc_object(void)
{
	struct inpk_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    	INPK_LOG("inpk_context_alloc_object++++\n");
	if(!obj)
	{
		INPK_ERR("Alloc inpk object error!\n");
		return NULL;
	}	
	atomic_set(&obj->wake, 0);
	mutex_init(&obj->inpk_op_mutex);

	INPK_LOG("inpk_context_alloc_object----\n");
	return obj;
}

int inpk_notify()
{
	int err=0;
	int value=0;
	struct inpk_context *cxt = NULL;
  	cxt = inpk_context_obj;
	INPK_LOG("inpk_notify++++\n");
	
	value =1;
	input_report_rel(cxt->idev, EVENT_TYPE_INPK_VALUE, value);
	input_sync(cxt->idev); 
	
	return err;
}

static int inpk_real_enable(int enable)
{
	int err =0;
	struct inpk_context *cxt = NULL;
	cxt = inpk_context_obj;
	
	if(INPK_RESUME == enable)
	{
		enable = resume_enable_status;
	}
	
	if(1==enable)
	{
		resume_enable_status = 1;
		if(atomic_read(&(inpk_context_obj->early_suspend))) //not allow to enable under suspend
		{
			return 0;
		}
		if(false==cxt->is_active_data)
		{
			err = cxt->inpk_ctl.open_report_data(1);
			if(err)
			{ 
				err = cxt->inpk_ctl.open_report_data(1);
				if(err)
				{
					err = cxt->inpk_ctl.open_report_data(1);
					if(err)
					{
						INPK_ERR("enable_in_pocket enable(%d) err 3 timers = %d\n", enable, err);
						return err;
					}
				}
			}
			cxt->is_active_data = true;
			INPK_LOG("enable_in_pocket real enable  \n" );
		}
	}
	else if((0==enable) || (INPK_SUSPEND == enable))
	{
	if(0==enable)
			resume_enable_status = 0;
		if(true==cxt->is_active_data)
		{
			err = cxt->inpk_ctl.open_report_data(0);
			if(err)
			{ 
				INPK_ERR("enable_in_pocketenable(%d) err = %d\n", enable, err);
			}
			cxt->is_active_data =false;
			INPK_LOG("enable_in_pocket real disable  \n" );
		} 
	}
	return err;
}

int inpk_enable_nodata(int enable)
{
	struct inpk_context *cxt = NULL;
	cxt = inpk_context_obj;
	if(NULL  == cxt->inpk_ctl.open_report_data)
	{
		INPK_ERR("inpk_enable_nodata:inpk ctl path is NULL\n");
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
	inpk_real_enable(enable);
	return 0;
}

static ssize_t inpk_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct inpk_context *cxt = NULL;
	cxt = inpk_context_obj;
	
	INPK_LOG("inpk active: %d\n", cxt->is_active_nodata);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_nodata); 
}

static ssize_t inpk_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct inpk_context *cxt = NULL;
	INPK_LOG("inpk_store_enable nodata buf=%s\n",buf);
	mutex_lock(&inpk_context_obj->inpk_op_mutex);
	cxt = inpk_context_obj;
	if(NULL == cxt->inpk_ctl.open_report_data)
	{
		INPK_LOG("inpk_ctl enable nodata NULL\n");
		mutex_unlock(&inpk_context_obj->inpk_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		inpk_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
		inpk_enable_nodata(0);
	}
	else
	{
		INPK_ERR(" inpk_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&inpk_context_obj->inpk_op_mutex);
	return count;
}

static ssize_t inpk_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct inpk_context *cxt = NULL;
	int res =0;
	int en=0;
	INPK_LOG("inpk_store_active buf=%s\n",buf);
	mutex_lock(&inpk_context_obj->inpk_op_mutex);
	
	cxt = inpk_context_obj;
	if((res = sscanf(buf, "%d", &en))!=1)
	{
		INPK_LOG(" inpk_store_active param error: res = %d\n", res);
	}
	INPK_LOG(" inpk_store_active en=%d\n",en);
	if(1 == en)
	{
		inpk_real_enable(1);
	}
	else if(0 == en)
	{
		inpk_real_enable(0);
	}
	else
	{
		INPK_ERR(" inpk_store_active error !!\n");
	}
	mutex_unlock(&inpk_context_obj->inpk_op_mutex);
	INPK_LOG(" inpk_store_active done\n");
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t inpk_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct inpk_context *cxt = NULL;
	cxt = inpk_context_obj;

	INPK_LOG("inpk active: %d\n", cxt->is_active_data);
	return snprintf(buf, PAGE_SIZE, "%d\n", cxt->is_active_data); 
}

static ssize_t inpk_store_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	INPK_LOG(" not support now\n");
	return len;
}


static ssize_t inpk_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	INPK_LOG(" not support now\n");
	return len;
}


static ssize_t inpk_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	INPK_LOG(" not support now\n");
	return len;
}

static ssize_t inpk_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	int len = 0;
	INPK_LOG(" not support now\n");
	return len;
}

static ssize_t inpk_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	int len = 0;
	INPK_LOG(" not support now\n");
	return len;
}

static ssize_t inpk_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	INPK_LOG(" not support now\n");
	return len;
}

static ssize_t inpk_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	char *devname = NULL;
	devname = dev_name(&inpk_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5);  //TODO: why +5?
}
static int in_pocket_remove(struct platform_device *pdev)
{
	INPK_LOG("in_pocket_remove\n");
	return 0;
}

static int in_pocket_probe(struct platform_device *pdev) 
{
	INPK_LOG("in_pocket_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id in_pocket_of_match[] = {
	{ .compatible = "mediatek,in_pocket", },
	{},
};
#endif

static struct platform_driver in_pocket_driver = {
	.probe      = in_pocket_probe,
	.remove     = in_pocket_remove,    
	.driver     = 
	{
		.name  = "in_pocket",
		#ifdef CONFIG_OF
		.of_match_table = in_pocket_of_match,
		#endif
	}
};

static int inpk_real_driver_init(void) 
{
	int err=0;
	INPK_LOG(" inpk_real_driver_init +\n");
	if(0 != in_pocket_init)
	{
		INPK_LOG(" inpk try to init driver %s\n", in_pocket_init->name);
		err = in_pocket_init->init();
		if(0 == err)
		{
			INPK_LOG(" inpk real driver %s probe ok\n", in_pocket_init->name);
		}
	}
	return err;
}

int inpk_driver_add(struct inpk_init_info* obj) 
{
	int err=0;
	
	INPK_FUN();
	INPK_LOG("register in_pocket driver for the first time\n");
	if(platform_driver_register(&in_pocket_driver))
	{
		INPK_ERR("failed to register gensor driver already exist\n");
	}
	if(NULL == in_pocket_init)
	{
		obj->platform_diver_addr = &in_pocket_driver;
		in_pocket_init = obj;
	}

	if(NULL==in_pocket_init)
	{
		INPK_ERR("INPK driver add err \n");
		err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(inpk_driver_add);

static int inpk_misc_init(struct inpk_context *cxt)
{
	int err=0;
    //kernel-3.10\include\linux\Miscdevice.h
    //use MISC_DYNAMIC_MINOR exceed 64
	cxt->mdev.minor = M_INPK_MISC_MINOR;
	cxt->mdev.name  = INPK_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		INPK_ERR("unable to register inpk misc device!!\n");
	}
	return err;
}

static void inpk_input_destroy(struct inpk_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int inpk_input_init(struct inpk_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = INPK_INPUTDEV_NAME;
	input_set_capability(dev, EV_REL, EVENT_TYPE_INPK_VALUE);
	
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

DEVICE_ATTR(inpkenablenodata,     	S_IWUSR | S_IRUGO, inpk_show_enable_nodata, inpk_store_enable_nodata);
DEVICE_ATTR(inpkactive,     		S_IWUSR | S_IRUGO, inpk_show_active, inpk_store_active);
DEVICE_ATTR(inpkdelay,      		S_IWUSR | S_IRUGO, inpk_show_delay,  inpk_store_delay);
DEVICE_ATTR(inpkbatch,      		S_IWUSR | S_IRUGO, inpk_show_batch,  inpk_store_batch);
DEVICE_ATTR(inpkflush,      			S_IWUSR | S_IRUGO, inpk_show_flush,  inpk_store_flush);
DEVICE_ATTR(inpkdevnum,      			S_IWUSR | S_IRUGO, inpk_show_devnum,  NULL);


static struct attribute *inpk_attributes[] = {
	&dev_attr_inpkenablenodata.attr,
	&dev_attr_inpkactive.attr,
	&dev_attr_inpkdelay.attr,
	&dev_attr_inpkbatch.attr,
	&dev_attr_inpkflush.attr,
	&dev_attr_inpkdevnum.attr,
	NULL
};

static struct attribute_group inpk_attribute_group = {
	.attrs = inpk_attributes
};

int inpk_register_data_path(struct inpk_data_path *data)
{
	struct inpk_context *cxt = NULL;
	cxt = inpk_context_obj;
	cxt->inpk_data.get_data = data->get_data;
	if(NULL == cxt->inpk_data.get_data)
	{
		INPK_LOG("inpk register data path fail \n");
	 	return -1;
	}
	return 0;
}

int inpk_register_control_path(struct inpk_control_path *ctl)
{
	struct inpk_context *cxt = NULL;
	int err =0;
	cxt = inpk_context_obj;
//	cxt->inpk_ctl.enable = ctl->enable;
//	cxt->inpk_ctl.enable_nodata = ctl->enable_nodata;
	cxt->inpk_ctl.open_report_data = ctl->open_report_data;
	
	if(NULL==cxt->inpk_ctl.open_report_data)
	{
		INPK_LOG("inpk register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = inpk_misc_init(inpk_context_obj);
	if(err)
	{
		INPK_ERR("unable to register inpk misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&inpk_context_obj->mdev.this_device->kobj,
			&inpk_attribute_group);
	if (err < 0)
	{
		INPK_ERR("unable to create inpk attribute file\n");
		return -3;
	}
	kobject_uevent(&inpk_context_obj->mdev.this_device->kobj, KOBJ_ADD);
	return 0;	
}

static int inpk_probe(struct platform_device *pdev) 
{
	int err;
	INPK_LOG("+++++++++++++inpk_probe!!\n");

	inpk_context_obj = inpk_context_alloc_object();
	if (!inpk_context_obj)
	{
		err = -ENOMEM;
		INPK_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}
	//init real inpk driver
    	err = inpk_real_driver_init();
	if(err)
	{
		INPK_ERR("inpk real driver init fail\n");
		goto real_driver_init_fail;
	}

	//init input dev
	err = inpk_input_init(inpk_context_obj);
	if(err)
	{
		INPK_ERR("unable to register inpk input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
    	atomic_set(&(inpk_context_obj->early_suspend), 0);
	inpk_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	inpk_context_obj->early_drv.suspend  = inpk_early_suspend,
	inpk_context_obj->early_drv.resume   = inpk_late_resume,    
	register_early_suspend(&inpk_context_obj->early_drv);
#endif //#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
  
	INPK_LOG("----inpk_probe OK !!\n");
	return 0;


	if (err)
	{
	   INPK_ERR("sysfs node creation error \n");
	   inpk_input_destroy(inpk_context_obj);
	}
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(inpk_context_obj);
	exit_alloc_data_failed:
	INPK_LOG("----inpk_probe fail !!!\n");
	return err;
}

static int inpk_remove(struct platform_device *pdev)
{
	int err=0;
	INPK_FUN(f);
	input_unregister_device(inpk_context_obj->idev);        
	sysfs_remove_group(&inpk_context_obj->idev->dev.kobj,
				&inpk_attribute_group);
	
	if((err = misc_deregister(&inpk_context_obj->mdev)))
	{
		INPK_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(inpk_context_obj);
	return 0;
}

static void inpk_early_suspend(struct early_suspend *h) 
{
	atomic_set(&(inpk_context_obj->early_suspend), 1);
	if(!atomic_read(&inpk_context_obj->wake)) //not wake up, disable in early suspend
	{
		inpk_real_enable(INPK_SUSPEND);
	}
	INPK_LOG(" inpk_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(inpk_context_obj->early_suspend)));
	return ;
}
/*----------------------------------------------------------------------------*/
static void inpk_late_resume(struct early_suspend *h)
{
	atomic_set(&(inpk_context_obj->early_suspend), 0);
	if(!atomic_read(&inpk_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		inpk_real_enable(INPK_RESUME);
	}
	INPK_LOG(" inpk_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(inpk_context_obj->early_suspend)));
	return ;
}

#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
static int inpk_suspend(struct platform_device *dev, pm_message_t state) 
{
	atomic_set(&(inpk_context_obj->suspend), 1);
	if(!atomic_read(&inpk_context_obj->wake)) //not wake up, disable in early suspend
	{
		inpk_real_enable(INPK_SUSPEND);
	}
	INPK_LOG(" inpk_suspend ok------->hwm_obj->suspend=%d \n",atomic_read(&(inpk_context_obj->suspend)));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int inpk_resume(struct platform_device *dev)
{
	atomic_set(&(inpk_context_obj->suspend), 0);
	if(!atomic_read(&inpk_context_obj->wake) && resume_enable_status) //not wake up, disable in early suspend
	{
		inpk_real_enable(INPK_RESUME);
	}
	INPK_LOG(" inpk_resume ok------->hwm_obj->suspend=%d \n",atomic_read(&(inpk_context_obj->suspend)));
	return 0;
}
#endif //#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)

#ifdef CONFIG_OF
static const struct of_device_id m_inpk_pl_of_match[] = {
	{ .compatible = "mediatek,m_inpk_pl", },
	{},
};
#endif

static struct platform_driver inpk_driver =
{
	.probe      = inpk_probe,
	.remove     = inpk_remove,    
#if !defined(CONFIG_HAS_EARLYSUSPEND) || !defined(USE_EARLY_SUSPEND)
	.suspend    = inpk_suspend,
	.resume     = inpk_resume,
#endif
	.driver     = 
	{
		.name = INPK_PL_DEV_NAME,
		#ifdef CONFIG_OF
		.of_match_table = m_inpk_pl_of_match,
		#endif
	}
};

static int __init inpk_init(void) 
{
	INPK_FUN();

	if(platform_driver_register(&inpk_driver))
	{
		INPK_ERR("failed to register inpk driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit inpk_exit(void)
{
	platform_driver_unregister(&inpk_driver); 
	platform_driver_unregister(&in_pocket_driver);      
}

late_initcall(inpk_init);
//module_init(inpk_init);
//module_exit(inpk_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("INPK device driver");
MODULE_AUTHOR("Mediatek");

