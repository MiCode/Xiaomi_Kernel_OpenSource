
#include "barometer.h"

struct baro_context *baro_context_obj = NULL;


static struct baro_init_info* barometer_init_list[MAX_CHOOSE_BARO_NUM]= {0}; //modified
static void baro_early_suspend(struct early_suspend *h);
static void baro_late_resume(struct early_suspend *h);

static void baro_work_func(struct work_struct *work)
{

	struct baro_context *cxt = NULL;
	//hwm_sensor_data sensor_data;
	int value,status;
	int64_t  nt;
	struct timespec time; 
	int err;

	cxt  = baro_context_obj;
	
	if(NULL == cxt->baro_data.get_data)
	{
		BARO_LOG("baro driver not register data path\n");
	}

	
	time.tv_sec = time.tv_nsec = 0;    
	time = get_monotonic_coarse(); 
	nt = time.tv_sec*1000000000LL+time.tv_nsec;
	
    	//add wake lock to make sure data can be read before system suspend
	err = cxt->baro_data.get_data(&value,&status);

	if(err)
	{
		BARO_ERR("get baro data fails!!\n" );
		goto baro_loop;
	}
	else
	{
		{	
			cxt->drv_data.baro_data.values[0] = value;
			cxt->drv_data.baro_data.status = status;
			cxt->drv_data.baro_data.time = nt;
		}			
	 }
    
	if(true ==  cxt->is_first_data_after_enable)
	{
		cxt->is_first_data_after_enable = false;
		//filter -1 value
	    if(BARO_INVALID_VALUE == cxt->drv_data.baro_data.values[0])
	    {
	        	BARO_LOG(" read invalid data \n");
	       	goto baro_loop;
			
	    }
	}
	//report data to input device
	//printk("new baro work run....\n");
	//BARO_LOG("baro data[%d]  \n" ,cxt->drv_data.baro_data.values[0]);

	baro_data_report(cxt->idev,
		cxt->drv_data.baro_data.values[0],
		cxt->drv_data.baro_data.status);

	baro_loop:
	if(true == cxt->is_polling_run)
	{
		{
		  mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ)); 
		}
	}
}

static void baro_poll(unsigned long data)
{
	struct baro_context *obj = (struct baro_context *)data;
	if(obj != NULL)
	{
		schedule_work(&obj->report);
	}
}

static struct baro_context *baro_context_alloc_object(void)
{
	
	struct baro_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    	BARO_LOG("baro_context_alloc_object++++\n");
	if(!obj)
	{
		BARO_ERR("Alloc baro object error!\n");
		return NULL;
	}	
	atomic_set(&obj->delay, 200); /*5Hz*/// set work queue delay time 200ms
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, baro_work_func);
	init_timer(&obj->timer);
	obj->timer.expires	= jiffies + atomic_read(&obj->delay)/(1000/HZ);
	obj->timer.function	= baro_poll;
	obj->timer.data	= (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->baro_op_mutex);
	obj->is_batch_enable = false;//for batch mode init

	BARO_LOG("baro_context_alloc_object----\n");
	return obj;
}

static int baro_real_enable(int enable)
{
  int err =0;
  struct baro_context *cxt = NULL;
  cxt = baro_context_obj;
  if(1==enable)
  {
     
     if(true==cxt->is_active_data || true ==cxt->is_active_nodata)
     {
        err = cxt->baro_ctl.enable_nodata(1);
        if(err)
        { 
           err = cxt->baro_ctl.enable_nodata(1);
		   if(err)
		   {
		   		err = cxt->baro_ctl.enable_nodata(1);
				if(err)
					BARO_ERR("baro enable(%d) err 3 timers = %d\n", enable, err);
		   }
        }
		BARO_LOG("baro real enable  \n" );
     }
	 
  }
  if(0==enable)
  {
     if(false==cxt->is_active_data && false ==cxt->is_active_nodata)
     {
        err = cxt->baro_ctl.enable_nodata(0);
        if(err)
        { 
        	BARO_ERR("baro enable(%d) err = %d\n", enable, err);
        }
	 BARO_LOG("baro real disable  \n" );
     }
	 
  }

  return err;
}
static int baro_enable_data(int enable)
{
    struct baro_context *cxt = NULL;
	cxt = baro_context_obj;
	if(NULL  == cxt->baro_ctl.open_report_data)
	{
	  BARO_ERR("no baro control path\n");
	  return -1;
	}
	
    if(1 == enable)
    {
       BARO_LOG("BARO enable data\n");
	   cxt->is_active_data =true;
       	cxt->is_first_data_after_enable = true;
	   cxt->baro_ctl.open_report_data(1);
        baro_real_enable(enable);
	   if(false == cxt->is_polling_run && cxt->is_batch_enable == false)
	   {
	      if(false == cxt->baro_ctl.is_report_input_direct)
	      {
	      		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
		  	cxt->is_polling_run = true;
	      }
	   }
    }
	if(0 == enable)
	{
	   BARO_LOG("BARO disable \n");
	   cxt->is_active_data =false;
	   cxt->baro_ctl.open_report_data(0);
	   if(true == cxt->is_polling_run)
	   {
	      if(false == cxt->baro_ctl.is_report_input_direct )
	      {
		      	cxt->is_polling_run = false;
		      	del_timer_sync(&cxt->timer);
		      	cancel_work_sync(&cxt->report);
			cxt->drv_data.baro_data.values[0] = BARO_INVALID_VALUE;
	      }
	   }
       baro_real_enable(enable);
	}
	return 0;
}



int baro_enable_nodata(int enable)
{
    struct baro_context *cxt = NULL;
	cxt = baro_context_obj;
	if(NULL  == cxt->baro_ctl.enable_nodata)
	{
	  BARO_ERR("baro_enable_nodata:baro ctl path is NULL\n");
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
	baro_real_enable(enable);
	return 0;
}


static ssize_t baro_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	BARO_LOG(" not support now\n");
	return len;
}

static ssize_t baro_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct baro_context *cxt = NULL;
    
	BARO_LOG("baro_store_enable nodata buf=%s\n",buf);
	mutex_lock(&baro_context_obj->baro_op_mutex);

	cxt = baro_context_obj;
	if(NULL == cxt->baro_ctl.enable_nodata)
	{
		BARO_LOG("baro_ctl enable nodata NULL\n");
		mutex_unlock(&baro_context_obj->baro_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		baro_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
       	baro_enable_nodata(0);
    	}
	else
	{
	  	BARO_ERR(" baro_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&baro_context_obj->baro_op_mutex);

    return count;
}

static ssize_t baro_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct baro_context *cxt = NULL;
    
   	BARO_LOG("baro_store_active buf=%s\n",buf);
	mutex_lock(&baro_context_obj->baro_op_mutex);
	
	cxt = baro_context_obj;
	if(NULL == cxt->baro_ctl.open_report_data)
	{
		BARO_LOG("baro_ctl enable NULL\n");
		mutex_unlock(&baro_context_obj->baro_op_mutex);
	 	return count;
	}
    if (!strncmp(buf, "1", 1)) 
	{
      		baro_enable_data(1);
       } 
	else if (!strncmp(buf, "0", 1))
	{
       	baro_enable_data(0);
    	}
	else
	{
	  	BARO_ERR(" baro_store_active error !!\n");
	}
	mutex_unlock(&baro_context_obj->baro_op_mutex);
	BARO_LOG(" baro_store_active done\n");
    return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t baro_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct baro_context *cxt = NULL;
    int div;

	cxt = baro_context_obj;
	div=cxt->baro_data.vender_div;

	BARO_LOG("baro vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div); 
}

static ssize_t baro_store_delay(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)

{
    	int delay;
	int mdelay=0;
	struct baro_context *cxt = NULL;

    mutex_lock(&baro_context_obj->baro_op_mutex);
    
	cxt = baro_context_obj;
	if(NULL == cxt->baro_ctl.set_delay)
	{
		BARO_LOG("baro_ctl set_delay NULL\n");
		mutex_unlock(&baro_context_obj->baro_op_mutex);
	 	return count;
	}

    if (1 != sscanf(buf, "%d", &delay)) {
        BARO_ERR("invalid format!!\n");
		mutex_unlock(&baro_context_obj->baro_op_mutex);
        return count;
    }

    if(false == cxt->baro_ctl.is_report_input_direct)
    {
    	mdelay = (int)delay/1000/1000;
    	atomic_set(&baro_context_obj->delay, mdelay);
    }
    cxt->baro_ctl.set_delay(delay);
	BARO_LOG(" baro_delay %d ns\n",delay);
	mutex_unlock(&baro_context_obj->baro_op_mutex);
    return count;

}

static ssize_t baro_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	BARO_LOG(" not support now\n");
	return len;
}


static ssize_t baro_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    struct baro_context *cxt = NULL;
    
    BARO_LOG("baro_store_batch buf=%s\n",buf);
	mutex_lock(&baro_context_obj->baro_op_mutex);

	cxt = baro_context_obj;

    if (!strncmp(buf, "1", 1)) 
	{
    	cxt->is_batch_enable = true;
        if(true == cxt->is_polling_run)
        {
            cxt->is_polling_run = false;
            del_timer_sync(&cxt->timer);
            cancel_work_sync(&cxt->report);
            cxt->drv_data.baro_data.values[0] = BARO_INVALID_VALUE;
            cxt->drv_data.baro_data.values[1] = BARO_INVALID_VALUE;
            cxt->drv_data.baro_data.values[2] = BARO_INVALID_VALUE;
        }
    	} 
	else if (!strncmp(buf, "0", 1))
	{
	cxt->is_batch_enable = false;
        if(false == cxt->is_polling_run)
        {
            if(false == cxt->baro_ctl.is_report_input_direct)
            {
                mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
                cxt->is_polling_run = true;
            }
        }
    	}
	else
	{
	BARO_ERR(" baro_store_batch error !!\n");
	}
	mutex_unlock(&baro_context_obj->baro_op_mutex);
	BARO_LOG(" baro_store_batch done: %d\n", cxt->is_batch_enable);
    return count;

}

static ssize_t baro_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0); 
}

static ssize_t baro_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	mutex_lock(&baro_context_obj->baro_op_mutex);
	//struct baro_context *devobj = (struct baro_context*)dev_get_drvdata(dev);
	//do read FIFO data function and report data immediately
	mutex_unlock(&baro_context_obj->baro_op_mutex);
    return count;
}

static ssize_t baro_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0); 
}

static ssize_t baro_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	const char *devname = NULL;
	devname = dev_name(&baro_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5); 
}
static int barometer_remove(struct platform_device *pdev)
{
	BARO_LOG("barometer_remove\n");
	return 0;
}

static int barometer_probe(struct platform_device *pdev) 
{
	BARO_LOG("barometer_probe\n");
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id barometer_of_match[] = {
	{ .compatible = "mediatek,barometer", },
	{},
};
#endif

static struct platform_driver barometer_driver = {
	.probe      = barometer_probe,
	.remove     = barometer_remove,    
	.driver     = 
	{
		.name  = "barometer",
        #ifdef CONFIG_OF
		.of_match_table = barometer_of_match,
		#endif
	}
};

static int baro_real_driver_init(void) 
{
    int i =0;
	int err=0;
	BARO_LOG(" baro_real_driver_init +\n");
	for(i = 0; i < MAX_CHOOSE_BARO_NUM; i++)
	{
	  BARO_LOG(" i=%d\n",i);
	  if(0 != barometer_init_list[i])
	  {
	    BARO_LOG(" baro try to init driver %s\n", barometer_init_list[i]->name);
	    err = barometer_init_list[i]->init();
		if(0 == err)
		{
		   BARO_LOG(" baro real driver %s probe ok\n", barometer_init_list[i]->name);
		   break;
		}
	  }
	}

	if(i == MAX_CHOOSE_BARO_NUM)
	{
	   BARO_LOG(" baro_real_driver_init fail\n");
	   err=-1;
	}
	return err;
}

  int baro_driver_add(struct baro_init_info* obj) 
{
    int err=0;
	int i =0;
	
	BARO_FUN();
	if (!obj) {
		BARO_ERR("BARO driver add fail, baro_init_info is NULL \n");
		return -1;
	}
	
	for(i =0; i < MAX_CHOOSE_BARO_NUM; i++ )
	{
		if(i == 0){
			BARO_LOG("register barometer driver for the first time\n");
			if(platform_driver_register(&barometer_driver))
			{
				BARO_ERR("failed to register gensor driver already exist\n");
			}
		}
		
	    if(NULL == barometer_init_list[i])
	    {
	      obj->platform_diver_addr = &barometer_driver;
	      barometer_init_list[i] = obj;
		  break;
	    }
	}
	if(i >= MAX_CHOOSE_BARO_NUM)
	{
	   BARO_ERR("BARO driver add err \n");
	   err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(baro_driver_add);

static int baro_misc_init(struct baro_context *cxt)
{

    int err=0;
    cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = BARO_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		BARO_ERR("unable to register baro misc device!!\n");
	}
	return err;
}

static void baro_input_destroy(struct baro_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int baro_input_init(struct baro_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = BARO_INPUTDEV_NAME;

    set_bit(EV_REL, dev->evbit);
	input_set_capability(dev, EV_REL, EVENT_TYPE_BARO_VALUE);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_BARO_STATUS);
	
	//input_set_abs_params(dev, EVENT_TYPE_BARO_VALUE, BARO_VALUE_MIN, BARO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_BARO_STATUS, BARO_STATUS_MIN, BARO_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev= dev;

	return 0;
}

DEVICE_ATTR(baroenablenodata,     	S_IWUSR | S_IRUGO, baro_show_enable_nodata, baro_store_enable_nodata);
DEVICE_ATTR(baroactive,     		S_IWUSR | S_IRUGO, baro_show_active, baro_store_active);
DEVICE_ATTR(barodelay,      		S_IWUSR | S_IRUGO, baro_show_delay,  baro_store_delay);
DEVICE_ATTR(barobatch,      		S_IWUSR | S_IRUGO, baro_show_batch,  baro_store_batch);
DEVICE_ATTR(baroflush,      			S_IWUSR | S_IRUGO, baro_show_flush,  baro_store_flush);
DEVICE_ATTR(barodevnum,      			S_IWUSR | S_IRUGO, baro_show_devnum,  NULL);


static struct attribute *baro_attributes[] = {
	&dev_attr_baroenablenodata.attr,
	&dev_attr_baroactive.attr,
	&dev_attr_barodelay.attr,
	&dev_attr_barobatch.attr,
	&dev_attr_baroflush.attr,
	&dev_attr_barodevnum.attr,
	NULL
};

static struct attribute_group baro_attribute_group = {
	.attrs = baro_attributes
};

int baro_register_data_path(struct baro_data_path *data)
{
	struct baro_context *cxt = NULL;
	cxt = baro_context_obj;
	cxt->baro_data.get_data = data->get_data;
	cxt->baro_data.vender_div = data->vender_div;
	cxt->baro_data.get_raw_data = data->get_raw_data;
	BARO_LOG("baro register data path vender_div: %d\n", cxt->baro_data.vender_div);
	if(NULL == cxt->baro_data.get_data)
	{
		BARO_LOG("baro register data path fail \n");
	 	return -1;
	}
	return 0;
}

int baro_register_control_path(struct baro_control_path *ctl)
{
	struct baro_context *cxt = NULL;
	int err =0;
	cxt = baro_context_obj;
	cxt->baro_ctl.set_delay = ctl->set_delay;
	cxt->baro_ctl.open_report_data= ctl->open_report_data;
	cxt->baro_ctl.enable_nodata = ctl->enable_nodata;
	cxt->baro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->baro_ctl.is_report_input_direct= ctl->is_report_input_direct;
	cxt->baro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->baro_ctl.is_use_common_factory = ctl->is_use_common_factory;
	
	if(NULL==cxt->baro_ctl.set_delay || NULL==cxt->baro_ctl.open_report_data
		|| NULL==cxt->baro_ctl.enable_nodata)
	{
		BARO_LOG("baro register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = baro_misc_init(baro_context_obj);
	if(err)
	{
	   BARO_ERR("unable to register baro misc device!!\n");
	   return -2;
	}
	err = sysfs_create_group(&baro_context_obj->mdev.this_device->kobj,
			&baro_attribute_group);
	if (err < 0)
	{
	   BARO_ERR("unable to create baro attribute file\n");
	   return -3;
	}

	kobject_uevent(&baro_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;	
}

int baro_data_report(struct input_dev *dev, int value, int status)
{
	//BARO_LOG("+baro_data_report! %d, %d, %d, %d\n",x,y,z,status);
  	input_report_rel(dev, EVENT_TYPE_BARO_VALUE, value);
	input_report_abs(dev, EVENT_TYPE_BARO_STATUS, status);
	input_sync(dev);

    return 0;
}

static int baro_probe(struct platform_device *pdev) 
{

	int err;
	BARO_LOG("+++++++++++++baro_probe!!\n");

	baro_context_obj = baro_context_alloc_object();
	if (!baro_context_obj)
	{
		err = -ENOMEM;
		BARO_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	//init real baro driver
    	err = baro_real_driver_init();
	if(err)
	{
		BARO_ERR("baro real driver init fail\n");
		goto real_driver_init_fail;
	}
	
	//init baro common factory mode misc device
	err = baro_factory_device_init();
	if(err)
	{
		BARO_ERR("baro factory device already registed\n");
	}

	//init input dev
	err = baro_input_init(baro_context_obj);
	if(err)
	{
		BARO_ERR("unable to register baro input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND)
    atomic_set(&(baro_context_obj->early_suspend), 0);
	baro_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	baro_context_obj->early_drv.suspend  = baro_early_suspend,
	baro_context_obj->early_drv.resume   = baro_late_resume,    
	register_early_suspend(&baro_context_obj->early_drv);
#endif

  
	BARO_LOG("----baro_probe OK !!\n");
	return 0;

	//exit_hwmsen_create_attr_failed:
	//exit_misc_register_failed:    

	//exit_err_sysfs:
	
	if (err)
	{
	   BARO_ERR("sysfs node creation error \n");
	   baro_input_destroy(baro_context_obj);
	}
	
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(baro_context_obj);
	baro_context_obj = NULL;
	exit_alloc_data_failed:
	

	BARO_LOG("----baro_probe fail !!!\n");
	return err;
}



static int baro_remove(struct platform_device *pdev)
{
    int err=0;
    
	BARO_FUN(f);
	
	input_unregister_device(baro_context_obj->idev);        
	sysfs_remove_group(&baro_context_obj->idev->dev.kobj,
				&baro_attribute_group);
	
	if((err = misc_deregister(&baro_context_obj->mdev)))
	{
		BARO_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(baro_context_obj);

	return 0;
}

static void baro_early_suspend(struct early_suspend *h) 
{
   atomic_set(&(baro_context_obj->early_suspend), 1);
   BARO_LOG(" baro_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(baro_context_obj->early_suspend)));
   return ;
}
/*----------------------------------------------------------------------------*/
static void baro_late_resume(struct early_suspend *h)
{
   atomic_set(&(baro_context_obj->early_suspend), 0);
   BARO_LOG(" baro_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(baro_context_obj->early_suspend)));
   return ;
}

static int baro_suspend(struct platform_device *dev, pm_message_t state) 
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static int baro_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_baro_pl_of_match[] = {
	{ .compatible = "mediatek,m_baro_pl", },
	{},
};
#endif

static struct platform_driver baro_driver =
{
	.probe      = baro_probe,
	.remove     = baro_remove,    
	.suspend    = baro_suspend,
	.resume     = baro_resume,
	.driver     = 
	{
		.name = BARO_PL_DEV_NAME,
        #ifdef CONFIG_OF
		.of_match_table = m_baro_pl_of_match,
		#endif
	}
};

static int __init baro_init(void) 
{
	BARO_FUN();

	if(platform_driver_register(&baro_driver))
	{
		BARO_ERR("failed to register baro driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit baro_exit(void)
{
	platform_driver_unregister(&baro_driver); 
	platform_driver_unregister(&barometer_driver);      
}

late_initcall(baro_init);
//module_init(baro_init);
//module_exit(baro_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BAROMETER device driver");
MODULE_AUTHOR("Mediatek");

