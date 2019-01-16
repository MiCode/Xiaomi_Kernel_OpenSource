
#include "heart_rate.h"

struct hrm_context *hrm_context_obj = NULL;


static struct hrm_init_info* heart_rate_init_list[MAX_CHOOSE_HRM_NUM]= {0}; //modified
static void hrm_early_suspend(struct early_suspend *h);
static void hrm_late_resume(struct early_suspend *h);

static void hrm_work_func(struct work_struct *work)
{

	struct hrm_context *cxt = NULL;
	//int out_size;
	//hwm_sensor_data sensor_data;
	//u64 data64[6]; //for unify get_data parameter type
	u32 data32[6]; //for hwm_sensor_data.values as int
	//u64 data[4];
	int status;
	int64_t  nt;
	struct timespec time; 
	int err = 0;	

	cxt  = hrm_context_obj;
	
	if(NULL == cxt->hrm_data.get_data)
	{
		HRM_ERR("hrm driver not register data path\n");
	}

	
	time.tv_sec = time.tv_nsec = 0;    
	time = get_monotonic_coarse(); 
	nt = time.tv_sec*1000000000LL+time.tv_nsec;
	
	//add wake lock to make sure data can be read before system suspend
    	//initial data
	//data[0] = cxt->drv_data.hrm_data.values[0];
	//data[1] = cxt->drv_data.hrm_data.values[1];
	//data[2] = cxt->drv_data.hrm_data.values[2];
	//data[3] = cxt->drv_data.hrm_data.values[3];
	
	err = cxt->hrm_data.get_data(data32, &status);
	HRM_LOG("hrm get_data %d,%d,%d %d  \n" ,
	data32[0],data32[1],data32[2],data32[3]);
       
	if(err)
	{
		HRM_ERR("get hrm data fails!!\n" );
		goto hrm_loop;
	}
	else
	{
		if((data32[0] == cxt->drv_data.hrm_data.values[0])
		&& (data32[1] == cxt->drv_data.hrm_data.values[1])
		&& (data32[2] == cxt->drv_data.hrm_data.values[2])
		&& (data32[3] == cxt->drv_data.hrm_data.values[3]))
		{
			goto hrm_loop;
		}
		else
		{	
			cxt->drv_data.hrm_data.values[0] = data32[0];
			cxt->drv_data.hrm_data.values[1] = data32[1];
			cxt->drv_data.hrm_data.values[2] = data32[2];
			cxt->drv_data.hrm_data.values[3] = data32[3];
			HRM_LOG("hrm values %d,%d,%d,%d\n" ,
			cxt->drv_data.hrm_data.values[0],
			cxt->drv_data.hrm_data.values[1],
			cxt->drv_data.hrm_data.values[2],
			cxt->drv_data.hrm_data.values[3]);
			cxt->drv_data.hrm_data.status = status;
			cxt->drv_data.hrm_data.time = nt;
		}			
	 }
    
	if(true ==  cxt->is_first_data_after_enable)
	{
		cxt->is_first_data_after_enable = false;
		//filter -1 value
	    if(HRM_INVALID_VALUE == cxt->drv_data.hrm_data.values[0]||
		HRM_INVALID_VALUE == cxt->drv_data.hrm_data.values[1] ||
		HRM_INVALID_VALUE == cxt->drv_data.hrm_data.values[2] ||
		HRM_INVALID_VALUE == cxt->drv_data.hrm_data.values[3])
	    {
	        HRM_LOG(" read invalid data \n");
	       	goto hrm_loop;
			
	    }
	}
	//report data to input device
	//printk("new hrm work run....\n");
	HRM_LOG("hrm data %d,%d,%d %d  \n" ,cxt->drv_data.hrm_data.values[0],
	cxt->drv_data.hrm_data.values[1],cxt->drv_data.hrm_data.values[2],cxt->drv_data.hrm_data.values[3]);

	hrm_data_report(cxt->drv_data.hrm_data, cxt->drv_data.hrm_data.status);

	hrm_loop:
	if(true == cxt->is_polling_run)
	{
		{
		  mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ)); 
		}
	}
}

static void hrm_poll(unsigned long data)
{
	struct hrm_context *obj = (struct hrm_context *)data;
	if(obj != NULL)
	{
		schedule_work(&obj->report);
	}
}

static struct hrm_context *hrm_context_alloc_object(void)
{
	
	struct hrm_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    HRM_LOG("hrm_context_alloc_object++++\n");
	if(!obj)
	{
		HRM_ERR("Alloc hrm object error!\n");
		return NULL;
	}	
	atomic_set(&obj->delay, 200); /*5Hz*/// set work queue delay time 200ms
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, hrm_work_func);
	init_timer(&obj->timer);
	obj->timer.expires	= jiffies + atomic_read(&obj->delay)/(1000/HZ);
	obj->timer.function	= hrm_poll;
	obj->timer.data		= (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	mutex_init(&obj->hrm_op_mutex);
	HRM_LOG("hrm_context_alloc_object----\n");
	return obj;
}

static int hrm_real_enable(int enable)
{
  int err =0;
  struct hrm_context *cxt = NULL;
  cxt = hrm_context_obj;
  if(1==enable)
  {
     
     if(true==cxt->is_active_data || true ==cxt->is_active_nodata)
     {
        err = cxt->hrm_ctl.enable_nodata(1);
        if(err)
        { 
           err = cxt->hrm_ctl.enable_nodata(1);
		   if(err)
		   {
		   		err = cxt->hrm_ctl.enable_nodata(1);
				if(err)
					HRM_ERR("hrm enable(%d) err 3 timers = %d\n", enable, err);
		   }
        }
		HRM_LOG("hrm real enable  \n" );
     }
	 
  }
  if(0==enable)
  {
     if(false==cxt->is_active_data && false ==cxt->is_active_nodata)
     {
        err = cxt->hrm_ctl.enable_nodata(0);
        if(err)
        { 
          HRM_ERR("hrm enable(%d) err = %d\n", enable, err);
        }
		HRM_LOG("hrm real disable  \n" );
     }
	 
  }

  return err;
}
static int hrm_enable_data(int enable)
{
    struct hrm_context *cxt = NULL;
	cxt = hrm_context_obj;
	if(NULL  == cxt->hrm_ctl.open_report_data)
	{
	  HRM_ERR("no hrm control path\n");
	  return -1;
	}
	
    if(1 == enable)
    {
       HRM_LOG("hrm enable data\n");
	   cxt->is_active_data =true;
       cxt->is_first_data_after_enable = true;
	   cxt->hrm_ctl.open_report_data(1);
	   if(false == cxt->is_polling_run && cxt->is_batch_enable == false)
	   {
	      if(false == cxt->hrm_ctl.is_report_input_direct)
	      {
	      	mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
		  	cxt->is_polling_run = true;
	      }
	   }
    }
	if(0 == enable)
	{
	   HRM_LOG("hrm disable \n");
	   
	   cxt->is_active_data =false;
	   cxt->hrm_ctl.open_report_data(0);
	   if(true == cxt->is_polling_run)
	   {
	      if(false == cxt->hrm_ctl.is_report_input_direct)
	      {
	      	cxt->is_polling_run = false;
	      	del_timer_sync(&cxt->timer);
	      	cancel_work_sync(&cxt->report);
	      	cxt->drv_data.hrm_data.values[0] = HRM_INVALID_VALUE;
	      	cxt->drv_data.hrm_data.values[1] = HRM_INVALID_VALUE;
	      	cxt->drv_data.hrm_data.values[2] = HRM_INVALID_VALUE;
	      	cxt->drv_data.hrm_data.values[3] = HRM_INVALID_VALUE;
	      }
	   }
	   
	}
	hrm_real_enable(enable);
	return 0;
}



int hrm_enable_nodata(int enable)
{
    struct hrm_context *cxt = NULL;
	cxt = hrm_context_obj;
	if(NULL  == cxt->hrm_ctl.enable_nodata)
	{
	  HRM_ERR("hrm_enable_nodata:hrm ctl path is NULL\n");
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
	hrm_real_enable(enable);
	return 0;
}


static ssize_t hrm_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	HRM_LOG(" not support now\n");
	return len;
}

static ssize_t hrm_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hrm_context *cxt = NULL;
	//int err =0;
	HRM_LOG("hrm_store_enable nodata buf=%s\n",buf);
	mutex_lock(&hrm_context_obj->hrm_op_mutex);
	cxt = hrm_context_obj;
	if(NULL == cxt->hrm_ctl.enable_nodata)
	{
		HRM_LOG("hrm_ctl enable nodata NULL\n");
		mutex_unlock(&hrm_context_obj->hrm_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		hrm_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
       	hrm_enable_nodata(0);
    	}
	else
	{
	  HRM_ERR(" hrm_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&hrm_context_obj->hrm_op_mutex);
	return count;
}

static ssize_t hrm_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hrm_context *cxt = NULL;
	HRM_LOG("hrm_store_active buf=%s\n",buf);
	mutex_lock(&hrm_context_obj->hrm_op_mutex);
	cxt = hrm_context_obj;
	if(NULL == cxt->hrm_ctl.open_report_data)
	{
		HRM_LOG("hrm_ctl enable NULL\n");
		mutex_unlock(&hrm_context_obj->hrm_op_mutex);
	 	return count;
	}
    if (!strncmp(buf, "1", 1)) 
	{
      hrm_enable_data(1);
       
    } 
	else if (!strncmp(buf, "0", 1))
	{
       hrm_enable_data(0);
    }
	else
	{
	  HRM_ERR(" hrm_store_active error !!\n");
	}
	mutex_unlock(&hrm_context_obj->hrm_op_mutex);
	HRM_LOG(" hrm_store_active done\n");
    return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t hrm_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct hrm_context *cxt = NULL;
	int div = 0;
	cxt = hrm_context_obj;	
    //int len = 0;
	HRM_LOG("hrm show active not support now\n");
	//div=cxt->hrm_data.vender_div;
	HRM_LOG("hrm vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div); 
	
	//return len;
}

static ssize_t hrm_store_delay(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
   // struct hrm_context *devobj = (struct hrm_context*)dev_get_drvdata(dev);
    int delay;
	int mdelay=0;
	struct hrm_context *cxt = NULL;
	//int err =0;
	mutex_lock(&hrm_context_obj->hrm_op_mutex);
	cxt = hrm_context_obj;
	if(NULL == cxt->hrm_ctl.set_delay)
	{
		HRM_LOG("hrm_ctl set_delay NULL\n");
		mutex_unlock(&hrm_context_obj->hrm_op_mutex);
	 	return count;
	}

    if (1 != sscanf(buf, "%d", &delay)) {
        HRM_ERR("invalid format!!\n");
		mutex_unlock(&hrm_context_obj->hrm_op_mutex);
        return count;
    }

    if(false == cxt->hrm_ctl.is_report_input_direct)
    {
    	mdelay = (int)delay/1000/1000;
    	atomic_set(&hrm_context_obj->delay, mdelay);
    }
    cxt->hrm_ctl.set_delay(delay);
	HRM_LOG(" hrm_delay %d ns\n",delay);
	mutex_unlock(&hrm_context_obj->hrm_op_mutex);
    return count;
}

static ssize_t hrm_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	HRM_LOG(" not support now\n");
	return len;
}

static ssize_t hrm_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hrm_context *cxt = NULL;
	//int err =0;
	HRM_LOG("hrm_store_batch buf=%s\n",buf);
	mutex_lock(&hrm_context_obj->hrm_op_mutex);
	cxt = hrm_context_obj;
	if(cxt->hrm_ctl.is_support_batch){

	    	if (!strncmp(buf, "1", 1)) 
		{
	    		cxt->is_batch_enable = true;
	    	} 
		else if (!strncmp(buf, "0", 1))
		{
			cxt->is_batch_enable = false;
	    	}
		else
		{
			HRM_ERR(" hrm_store_batch error !!\n");
		}
	}else{
		HRM_LOG(" hrm_store_batch not support\n");
	}
	mutex_unlock(&hrm_context_obj->hrm_op_mutex);
	HRM_LOG(" hrm_store_batch done: %d\n", cxt->is_batch_enable);
    	return count;

}

static ssize_t hrm_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0); 
}

static ssize_t hrm_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	//mutex_lock(&hrm_context_obj->hrm_op_mutex);
    	//struct hrm_context *devobj = (struct hrm_context*)dev_get_drvdata(dev);
	//do read FIFO data function and report data immediately
	//mutex_unlock(&hrm_context_obj->hrm_op_mutex);
    return count;
}

static ssize_t hrm_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0); 
}

static ssize_t hrm_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	const char *devname = NULL;
	devname = dev_name(&hrm_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5); 
}
static int heart_rate_remove(struct platform_device *pdev)
{
	HRM_LOG("heart_rate_remove\n");
	return 0;
}

static int heart_rate_probe(struct platform_device *pdev) 
{
	HRM_LOG("heart_rate_probe\n");
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id heart_rate_of_match[] = {
	{ .compatible = "mediatek,heart_rate", },
	{},
};
#endif

static struct platform_driver heart_rate_driver = {
	.probe      = heart_rate_probe,
	.remove     = heart_rate_remove,    
	.driver     = 
	{
		.name  = "heart_rate",
        #ifdef CONFIG_OF
		.of_match_table = heart_rate_of_match,
		#endif
	}
};

static int hrm_real_driver_init(void) 
{
    int i =0;
	int err=0;
	HRM_LOG(" hrm_real_driver_init +\n");
	for(i = 0; i < MAX_CHOOSE_HRM_NUM; i++)
	{
	  HRM_LOG(" i=%d\n",i);
	  if(0 != heart_rate_init_list[i])
	  {
	    	HRM_LOG(" hrm try to init driver %s\n", heart_rate_init_list[i]->name);
	    	err = heart_rate_init_list[i]->init();
		if(0 == err)
		{
		   HRM_LOG(" hrm real driver %s probe ok\n", heart_rate_init_list[i]->name);
		   break;
		}
	  }
	}

	if(i == MAX_CHOOSE_HRM_NUM)
	{
	   HRM_LOG(" hrm_real_driver_init fail\n");
	   err=-1;
	}
	return err;
}

  int hrm_driver_add(struct hrm_init_info* obj) 
{
    int err=0;
	int i =0;
	
	HRM_FUN(f);

	for(i =0; i < MAX_CHOOSE_HRM_NUM; i++ )
	{
		if(i == 0){
			HRM_LOG("register hrm driver for the first time\n");
			if(platform_driver_register(&heart_rate_driver))
			{
				HRM_ERR("failed to register hrm driver already exist\n");
			}
		}
		
	    if(NULL == heart_rate_init_list[i])
	    {
	      obj->platform_diver_addr = &heart_rate_driver;
	      heart_rate_init_list[i] = obj;
		  break;
	    }
	}
	if(NULL==heart_rate_init_list[i])
	{
	   HRM_ERR("hrm driver add err \n");
	   err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(hrm_driver_add);

static int hrm_misc_init(struct hrm_context *cxt)
{

    int err=0;
    //kernel-3.10\include\linux\Miscdevice.h
    //use MISC_DYNAMIC_MINOR exceed 64
    cxt->mdev.minor = M_HRM_MISC_MINOR;
	cxt->mdev.name  = HRM_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		HRM_ERR("unable to register hrm misc device!!\n");
	}
	return err;
}

static void hrm_input_destroy(struct hrm_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int hrm_input_init(struct hrm_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = HRM_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_HRM_BPM);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_HRM_STATUS);
	
	input_set_abs_params(dev, EVENT_TYPE_HRM_BPM,    HRM_VALUE_MIN, HRM_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_HRM_STATUS, HRM_VALUE_MIN, HRM_VALUE_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev= dev;

	return 0;
}

DEVICE_ATTR(hrmenablenodata,     S_IWUSR | S_IRUGO, hrm_show_enable_nodata, hrm_store_enable_nodata);
DEVICE_ATTR(hrmactive,     S_IWUSR | S_IRUGO, hrm_show_active, hrm_store_active);
DEVICE_ATTR(hrmdelay,      S_IWUSR | S_IRUGO, hrm_show_delay,  hrm_store_delay);
DEVICE_ATTR(hrmbatch,     S_IWUSR | S_IRUGO, hrm_show_batch, hrm_store_batch);
DEVICE_ATTR(hrmflush,      S_IWUSR | S_IRUGO, hrm_show_flush,  hrm_store_flush);
DEVICE_ATTR(hrmdevnum,      S_IWUSR | S_IRUGO, hrm_show_devnum,  NULL);

static struct attribute *hrm_attributes[] = {
	&dev_attr_hrmenablenodata.attr,
	&dev_attr_hrmactive.attr,
	&dev_attr_hrmdelay.attr,
	&dev_attr_hrmbatch.attr,
	&dev_attr_hrmflush.attr,
	&dev_attr_hrmdevnum.attr,
	NULL
};

static struct attribute_group hrm_attribute_group = {
	.attrs = hrm_attributes
};

int hrm_register_data_path(struct hrm_data_path *data)
{
	struct hrm_context *cxt = NULL;
	//int err =0;
	cxt = hrm_context_obj;
	cxt->hrm_data.get_data = data->get_data;
	//cxt->hrm_data.vender_div = data->vender_div;
	//cxt->hrm_data.get_raw_data = data->get_raw_data;
	//HRM_LOG("hrm register data path vender_div: %d\n", cxt->hrm_data.vender_div);
	if(NULL == cxt->hrm_data.get_data)
	{
		HRM_LOG("hrm register data path fail \n");
	 	return -1;
	}
	return 0;
}

int hrm_register_control_path(struct hrm_control_path *ctl)
{
	struct hrm_context *cxt = NULL;
	int err =0;
	cxt = hrm_context_obj;
	cxt->hrm_ctl.set_delay = ctl->set_delay;
	cxt->hrm_ctl.open_report_data= ctl->open_report_data;
	cxt->hrm_ctl.enable_nodata = ctl->enable_nodata;
	cxt->hrm_ctl.is_support_batch = ctl->is_support_batch;
	
	if(NULL==cxt->hrm_ctl.set_delay || NULL==cxt->hrm_ctl.open_report_data
		|| NULL==cxt->hrm_ctl.enable_nodata)
	{
		HRM_LOG("hrm register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = hrm_misc_init(hrm_context_obj);
	if(err)
	{
	   HRM_ERR("unable to register hrm misc device!!\n");
	   return -2;
	}
	err = sysfs_create_group(&hrm_context_obj->mdev.this_device->kobj,
			&hrm_attribute_group);
	if (err < 0)
	{
	   HRM_ERR("unable to create hrm attribute file\n");
	   return -3;
	}

	kobject_uevent(&hrm_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;	
}

int hrm_data_report(hwm_sensor_data data,int status)
{
	HRM_LOG("+hrm_data_report! %d, %d, %d, %d\n",data.values[0], data.values[1], data.values[2], data.values[3]);
  struct hrm_context *cxt = NULL;
	int err =0;
	cxt = hrm_context_obj;
  input_report_abs(cxt->idev, EVENT_TYPE_HRM_BPM,    data.values[0]);
  input_report_abs(cxt->idev, EVENT_TYPE_HRM_STATUS, data.values[1]);
	input_sync(cxt->idev); 
	return err;
}

static int hrm_probe(struct platform_device *pdev) 
{

	int err;
	HRM_LOG("+++++++++++++hrm_probe!!\n");

	hrm_context_obj = hrm_context_alloc_object();
	if (!hrm_context_obj)
	{
		err = -ENOMEM;
		HRM_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	//init real hrmeleration driver
    err = hrm_real_driver_init();
	if(err)
	{
		HRM_ERR("hrm real driver init fail\n");
		goto real_driver_init_fail;
	}

	//err = hrm_factory_device_init();
	//if(err)
	//{
	//	HRM_ERR("hrm_factory_device_init fail\n");
	//}

	//init input dev
	err = hrm_input_init(hrm_context_obj);
	if(err)
	{
		HRM_ERR("unable to register hrm input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
  atomic_set(&(hrm_context_obj->early_suspend), 0);
	hrm_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	hrm_context_obj->early_drv.suspend  = hrm_early_suspend,
	hrm_context_obj->early_drv.resume   = hrm_late_resume,    
	register_early_suspend(&hrm_context_obj->early_drv);
#endif //#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)

	HRM_LOG("----hrm_probe OK !!\n");
	return 0;

	//exit_hwmsen_create_attr_failed:
	//exit_misc_register_failed:    

	//exit_err_sysfs:
	
	if (err)
	{
	   HRM_ERR("sysfs node creation error \n");
	   hrm_input_destroy(hrm_context_obj);
	}
	
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(hrm_context_obj);
	
	exit_alloc_data_failed:
	

	HRM_LOG("----hrm_probe fail !!!\n");
	return err;
}



static int hrm_remove(struct platform_device *pdev)
{
	int err=0;
	HRM_FUN(f);
	input_unregister_device(hrm_context_obj->idev);        
	sysfs_remove_group(&hrm_context_obj->idev->dev.kobj,
				&hrm_attribute_group);
	
	if((err = misc_deregister(&hrm_context_obj->mdev)))
	{
		HRM_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(hrm_context_obj);

	return 0;
}

static void hrm_early_suspend(struct early_suspend *h) 
{
   atomic_set(&(hrm_context_obj->early_suspend), 1);
   HRM_LOG(" hrm_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(hrm_context_obj->early_suspend)));
   return ;
}
/*----------------------------------------------------------------------------*/
static void hrm_late_resume(struct early_suspend *h)
{
   atomic_set(&(hrm_context_obj->early_suspend), 0);
   HRM_LOG(" hrm_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(hrm_context_obj->early_suspend)));
   return ;
}

static int hrm_suspend(struct platform_device *dev, pm_message_t state) 
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static int hrm_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_hrm_pl_of_match[] = {
	{ .compatible = "mediatek,m_hrm_pl", },
	{},
};
#endif

static struct platform_driver hrm_driver =
{
	.probe      = hrm_probe,
	.remove     = hrm_remove,    
	.suspend    = hrm_suspend,
	.resume     = hrm_resume,
	.driver     = 
	{
		.name = HRM_PL_DEV_NAME,//mt_hrm_pl
		#ifdef CONFIG_OF
		.of_match_table = m_hrm_pl_of_match,
		#endif
	}
};

static int __init hrm_init(void) 
{
	HRM_FUN(f);

	if(platform_driver_register(&hrm_driver))
	{
		HRM_ERR("failed to register hrm driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit hrm_exit(void)
{
	platform_driver_unregister(&hrm_driver); 
	platform_driver_unregister(&heart_rate_driver);      
}

late_initcall(hrm_init);
//module_init(hrm_init);
//module_exit(hrm_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HEART_RATE device driver");
MODULE_AUTHOR("Mediatek");

