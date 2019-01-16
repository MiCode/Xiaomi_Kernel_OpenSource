
#include "gyroscope.h"

struct gyro_context *gyro_context_obj = NULL;


static struct gyro_init_info* gyroscope_init_list[MAX_CHOOSE_GYRO_NUM]= {0}; //modified
static void gyro_early_suspend(struct early_suspend *h);
static void gyro_late_resume(struct early_suspend *h);

static void gyro_work_func(struct work_struct *work)
{

	struct gyro_context *cxt = NULL;
	//int out_size;
	//hwm_sensor_data sensor_data;
	int x,y,z,status;
	int64_t  nt;
	struct timespec time; 
	int err = 0;	

	cxt  = gyro_context_obj;
	
	if(NULL == cxt->gyro_data.get_data)
	{
		GYRO_ERR("gyro driver not register data path\n");
	}

	
	time.tv_sec = time.tv_nsec = 0;    
	time = get_monotonic_coarse(); 
	nt = time.tv_sec*1000000000LL+time.tv_nsec;
	
    //add wake lock to make sure data can be read before system suspend
	cxt->gyro_data.get_data(&x,&y,&z,&status);

	if(err)
	{
		GYRO_ERR("get gyro data fails!!\n" );
		goto gyro_loop;
	}
	else
	{
		if((x != cxt->drv_data.gyro_data.values[0]) 
					|| (y != cxt->drv_data.gyro_data.values[1])
					|| (z != cxt->drv_data.gyro_data.values[2]))
		{	
			if( 0 == x && 0==y 
						&& 0 == z)
			{
				    goto gyro_loop;
			}

			cxt->drv_data.gyro_data.values[0] = x+cxt->cali_sw[0];
			cxt->drv_data.gyro_data.values[1] = y+cxt->cali_sw[1];
			cxt->drv_data.gyro_data.values[2] = z+cxt->cali_sw[2];
			cxt->drv_data.gyro_data.status = status;
			cxt->drv_data.gyro_data.time = nt;
					
		}			
	 }
    
	if(true ==  cxt->is_first_data_after_enable)
	{
		cxt->is_first_data_after_enable = false;
		//filter -1 value
	    if(GYRO_INVALID_VALUE == cxt->drv_data.gyro_data.values[0] ||
		   	     GYRO_INVALID_VALUE == cxt->drv_data.gyro_data.values[1] ||
		   	     GYRO_INVALID_VALUE == cxt->drv_data.gyro_data.values[2])
	    {
	        GYRO_LOG(" read invalid data \n");
	       	goto gyro_loop;
			
	    }
	}
	//report data to input device
	//printk("new gyro work run....\n");
	//GYRO_LOG("gyro data[%d,%d,%d]  \n" ,cxt->drv_data.gyro_data.values[0],
	//cxt->drv_data.gyro_data.values[1],cxt->drv_data.gyro_data.values[2]);

	gyro_data_report(cxt->drv_data.gyro_data.values[0],
		cxt->drv_data.gyro_data.values[1],cxt->drv_data.gyro_data.values[2],
		cxt->drv_data.gyro_data.status);

	gyro_loop:
	if(true == cxt->is_polling_run)
	{
		{
		  mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ)); 
		}

	}
}

static void gyro_poll(unsigned long data)
{
	struct gyro_context *obj = (struct gyro_context *)data;
	if(obj != NULL)
	{
		schedule_work(&obj->report);
	}
}

static struct gyro_context *gyro_context_alloc_object(void)
{
	
	struct gyro_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    GYRO_LOG("gyro_context_alloc_object++++\n");
	if(!obj)
	{
		GYRO_ERR("Alloc gyro object error!\n");
		return NULL;
	}	
	atomic_set(&obj->delay, 200); /*5Hz*/// set work queue delay time 200ms
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, gyro_work_func);
	init_timer(&obj->timer);
	obj->timer.expires	= jiffies + atomic_read(&obj->delay)/(1000/HZ);
	obj->timer.function	= gyro_poll;
	obj->timer.data		= (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->is_batch_enable = false;
	obj->cali_sw[GYRO_AXIS_X]=0;
	obj->cali_sw[GYRO_AXIS_Y]=0;
	obj->cali_sw[GYRO_AXIS_Z]=0;
	mutex_init(&obj->gyro_op_mutex);
	GYRO_LOG("gyro_context_alloc_object----\n");
	return obj;
}

static int gyro_real_enable(int enable)
{
  int err =0;
  struct gyro_context *cxt = NULL;
  cxt = gyro_context_obj;
  if(1==enable)
  {
     
     if(true==cxt->is_active_data || true ==cxt->is_active_nodata)
     {
        err = cxt->gyro_ctl.enable_nodata(1);
        if(err)
        { 
           err = cxt->gyro_ctl.enable_nodata(1);
		   if(err)
		   {
		   		err = cxt->gyro_ctl.enable_nodata(1);
				if(err)
					GYRO_ERR("gyro enable(%d) err 3 timers = %d\n", enable, err);
		   }
        }
		GYRO_LOG("gyro real enable  \n" );
     }
	 
  }
  if(0==enable)
  {
     if(false==cxt->is_active_data && false ==cxt->is_active_nodata)
     {
        err = cxt->gyro_ctl.enable_nodata(0);
        if(err)
        { 
          GYRO_ERR("gyro enable(%d) err = %d\n", enable, err);
        }
		GYRO_LOG("gyro real disable  \n" );
     }
	 
  }

  return err;
}
static int gyro_enable_data(int enable)
{
    struct gyro_context *cxt = NULL;
	//int err =0;
	cxt = gyro_context_obj;
	if(NULL  == cxt->gyro_ctl.open_report_data)
	{
	  GYRO_ERR("no gyro control path\n");
	  return -1;
	}
	
    if(1 == enable)
    {
       GYRO_LOG("gyro enable data\n");
	   cxt->is_active_data =true;
       cxt->is_first_data_after_enable = true;
	   cxt->gyro_ctl.open_report_data(1);
       gyro_real_enable(enable);
	   if(false == cxt->is_polling_run && cxt->is_batch_enable == false)
	   {
	      if(false == cxt->gyro_ctl.is_report_input_direct)
	      {
	      	mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
		  	cxt->is_polling_run = true;
	      }
	   }
    }
	if(0 == enable)
	{
	   GYRO_LOG("gyro disable \n");
	   
	   cxt->is_active_data =false;
	   cxt->gyro_ctl.open_report_data(0);
	   if(true == cxt->is_polling_run)
	   {
	      if(false == cxt->gyro_ctl.is_report_input_direct)
	      {
	      	cxt->is_polling_run = false;
            smp_mb();
	      	del_timer_sync(&cxt->timer);
            smp_mb();
	      	cancel_work_sync(&cxt->report);
			cxt->drv_data.gyro_data.values[0] = GYRO_INVALID_VALUE;
	   		cxt->drv_data.gyro_data.values[1] = GYRO_INVALID_VALUE;
	   		cxt->drv_data.gyro_data.values[2] = GYRO_INVALID_VALUE;
	      }
	   }
       gyro_real_enable(enable);
	}
	return 0;
}



int gyro_enable_nodata(int enable)
{
    struct gyro_context *cxt = NULL;
	//int err =0;
	cxt = gyro_context_obj;
	if(NULL  == cxt->gyro_ctl.enable_nodata)
	{
	  GYRO_ERR("gyro_enable_nodata:gyro ctl path is NULL\n");
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
	gyro_real_enable(enable);
	return 0;
}


static ssize_t gyro_show_enable_nodata(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	GYRO_LOG(" not support now\n");
	return len;
}

static ssize_t gyro_store_enable_nodata(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct gyro_context *cxt = NULL;
	//int err =0;
	GYRO_LOG("gyro_store_enable nodata buf=%s\n",buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if(NULL == cxt->gyro_ctl.enable_nodata)
	{
		GYRO_LOG("gyro_ctl enable nodata NULL\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	 	return count;
	}
	if (!strncmp(buf, "1", 1))
	{
		gyro_enable_nodata(1);
	}
	else if (!strncmp(buf, "0", 1))
	{
       	gyro_enable_nodata(0);
    	}
	else
	{
	  GYRO_ERR(" gyro_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	return count;
}

static ssize_t gyro_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct gyro_context *cxt = NULL;
	//int err =0;
	GYRO_LOG("gyro_store_active buf=%s\n",buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if(NULL == cxt->gyro_ctl.open_report_data)
	{
		GYRO_LOG("gyro_ctl enable NULL\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	 	return count;
	}
    if (!strncmp(buf, "1", 1)) 
	{
      // cxt->gyro_ctl.enable(1);
      gyro_enable_data(1);
       
    } 
	else if (!strncmp(buf, "0", 1))
	{
        
       //cxt->gyro_ctl.enable(0);
       gyro_enable_data(0);
    }
	else
	{
	  GYRO_ERR(" gyro_store_active error !!\n");
	}
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	GYRO_LOG(" gyro_store_active done\n");
    return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t gyro_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct gyro_context *cxt = NULL;
	int div = 0;
	cxt = gyro_context_obj;	
    //int len = 0;
	GYRO_LOG("gyro show active not support now\n");
	div=cxt->gyro_data.vender_div;
	GYRO_LOG("gyro vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div); 
	
	//return len;
}

static ssize_t gyro_store_delay(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
   // struct gyro_context *devobj = (struct gyro_context*)dev_get_drvdata(dev);
    int delay;
	int mdelay=0;
	struct gyro_context *cxt = NULL;
	//int err =0;
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if(NULL == cxt->gyro_ctl.set_delay)
	{
		GYRO_LOG("gyro_ctl set_delay NULL\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	 	return count;
	}

    if (1 != sscanf(buf, "%d", &delay)) {
        GYRO_ERR("invalid format!!\n");
		mutex_unlock(&gyro_context_obj->gyro_op_mutex);
        return count;
    }

    if(false == cxt->gyro_ctl.is_report_input_direct)
    {
    	mdelay = (int)delay/1000/1000;
    	atomic_set(&gyro_context_obj->delay, mdelay);
    }
    cxt->gyro_ctl.set_delay(delay);
	GYRO_LOG(" gyro_delay %d ns\n",delay);
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
    return count;
}

static ssize_t gyro_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	GYRO_LOG(" not support now\n");
	return len;
}

static ssize_t gyro_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct gyro_context *cxt = NULL;
	//int err =0;
	GYRO_LOG("gyro_store_batch buf=%s\n",buf);
	mutex_lock(&gyro_context_obj->gyro_op_mutex);
	cxt = gyro_context_obj;
	if(cxt->gyro_ctl.is_support_batch){

	    	if (!strncmp(buf, "1", 1)) 
		{
	    		cxt->is_batch_enable = true;
                if(true == cxt->is_polling_run)
                {
                    cxt->is_polling_run = false;
                    del_timer_sync(&cxt->timer);
                    cancel_work_sync(&cxt->report);
                    cxt->drv_data.gyro_data.values[0] = GYRO_INVALID_VALUE;
                    cxt->drv_data.gyro_data.values[1] = GYRO_INVALID_VALUE;
                    cxt->drv_data.gyro_data.values[2] = GYRO_INVALID_VALUE;
                }
	    	} 
		else if (!strncmp(buf, "0", 1))
		{
			cxt->is_batch_enable = false;
                if(false == cxt->is_polling_run)
                {
                    if(false == cxt->gyro_ctl.is_report_input_direct)
                    {
                        mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
                        cxt->is_polling_run = true;
                    }
                }
	    	}
		else
		{
			GYRO_ERR(" gyro_store_batch error !!\n");
		}
	}else{
		GYRO_LOG(" gyro_store_batch not support\n");
	}
	mutex_unlock(&gyro_context_obj->gyro_op_mutex);
	GYRO_LOG(" gyro_store_batch done: %d\n", cxt->is_batch_enable);
    	return count;

}

static ssize_t gyro_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0); 
}

static ssize_t gyro_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	//mutex_lock(&gyro_context_obj->gyro_op_mutex);
    	//struct gyro_context *devobj = (struct gyro_context*)dev_get_drvdata(dev);
	//do read FIFO data function and report data immediately
	//mutex_unlock(&gyro_context_obj->gyro_op_mutex);
    return count;
}

static ssize_t gyro_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0); 
}

static ssize_t gyro_show_devnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	const char *devname = NULL;
	devname = dev_name(&gyro_context_obj->idev->dev);
	return snprintf(buf, PAGE_SIZE, "%s\n", devname+5); 
}
static int gyroscope_remove(struct platform_device *pdev)
{
	GYRO_LOG("gyroscope_remove\n");
	return 0;
}

static int gyroscope_probe(struct platform_device *pdev) 
{
	GYRO_LOG("gyroscope_probe\n");
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id gyroscope_of_match[] = {
	{ .compatible = "mediatek,gyroscope", },
	{},
};
#endif

static struct platform_driver gyroscope_driver = {
	.probe      = gyroscope_probe,
	.remove     = gyroscope_remove,    
	.driver     = 
	{
		.name  = "gyroscope",
        #ifdef CONFIG_OF
		.of_match_table = gyroscope_of_match,
		#endif
	}
};

static int gyro_real_driver_init(void) 
{
    int i =0;
	int err=0;
	GYRO_LOG(" gyro_real_driver_init +\n");
	for(i = 0; i < MAX_CHOOSE_GYRO_NUM; i++)
	{
	  GYRO_LOG(" i=%d\n",i);
	  if(0 != gyroscope_init_list[i])
	  {
	    	GYRO_LOG(" gyro try to init driver %s\n", gyroscope_init_list[i]->name);
	    	err = gyroscope_init_list[i]->init();
		if(0 == err)
		{
		   GYRO_LOG(" gyro real driver %s probe ok\n", gyroscope_init_list[i]->name);
		   break;
		}
	  }
	}

	if(i == MAX_CHOOSE_GYRO_NUM)
	{
	   GYRO_LOG(" gyro_real_driver_init fail\n");
	   err=-1;
	}
	return err;
}

  int gyro_driver_add(struct gyro_init_info* obj) 
{
    int err=0;
	int i =0;
	
	GYRO_FUN(f);
	if (!obj) {
		GYRO_ERR("gyro driver add fail, gyro_init_info is NULL \n");
		return -1;
	}

	for(i =0; i < MAX_CHOOSE_GYRO_NUM; i++ )
	{
        if((i == 0) && (NULL == gyroscope_init_list[0])){
			GYRO_LOG("register gyro driver for the first time\n");
			if(platform_driver_register(&gyroscope_driver))
			{
				GYRO_ERR("failed to register gyro driver already exist\n");
			}
		}
		
	    if(NULL == gyroscope_init_list[i])
	    {
	      obj->platform_diver_addr = &gyroscope_driver;
	      gyroscope_init_list[i] = obj;
		  break;
	    }
	}
	if(i >= MAX_CHOOSE_GYRO_NUM)
	{
	   GYRO_ERR("gyro driver add err \n");
	   err=-1;
	}
	
	return err;
}
EXPORT_SYMBOL_GPL(gyro_driver_add);

static int gyro_misc_init(struct gyro_context *cxt)
{

    int err=0;
    cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = GYRO_MISC_DEV_NAME;
	if((err = misc_register(&cxt->mdev)))
	{
		GYRO_ERR("unable to register gyro misc device!!\n");
	}
	return err;
}

static void gyro_input_destroy(struct gyro_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int gyro_input_init(struct gyro_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = GYRO_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_GYRO_STATUS);
	input_set_capability(dev, EV_REL, EVENT_TYPE_GYRO_UPDATE);
	
	input_set_abs_params(dev, EVENT_TYPE_GYRO_X, GYRO_VALUE_MIN, GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GYRO_Y, GYRO_VALUE_MIN, GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GYRO_Z, GYRO_VALUE_MIN, GYRO_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_GYRO_STATUS, GYRO_STATUS_MIN, GYRO_STATUS_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev= dev;

	return 0;
}

DEVICE_ATTR(gyroenablenodata,     S_IWUSR | S_IRUGO, gyro_show_enable_nodata, gyro_store_enable_nodata);
DEVICE_ATTR(gyroactive,     S_IWUSR | S_IRUGO, gyro_show_active, gyro_store_active);
DEVICE_ATTR(gyrodelay,      S_IWUSR | S_IRUGO, gyro_show_delay,  gyro_store_delay);
DEVICE_ATTR(gyrobatch,     S_IWUSR | S_IRUGO, gyro_show_batch, gyro_store_batch);
DEVICE_ATTR(gyroflush,      S_IWUSR | S_IRUGO, gyro_show_flush,  gyro_store_flush);
DEVICE_ATTR(gyrodevnum,      S_IWUSR | S_IRUGO, gyro_show_devnum,  NULL);

static struct attribute *gyro_attributes[] = {
	&dev_attr_gyroenablenodata.attr,
	&dev_attr_gyroactive.attr,
	&dev_attr_gyrodelay.attr,
	&dev_attr_gyrobatch.attr,
	&dev_attr_gyroflush.attr,
	&dev_attr_gyrodevnum.attr,
	NULL
};

static struct attribute_group gyro_attribute_group = {
	.attrs = gyro_attributes
};

int gyro_register_data_path(struct gyro_data_path *data)
{
	struct gyro_context *cxt = NULL;
	//int err =0;
	cxt = gyro_context_obj;
	cxt->gyro_data.get_data = data->get_data;
	cxt->gyro_data.vender_div = data->vender_div;
	cxt->gyro_data.get_raw_data = data->get_raw_data;
	GYRO_LOG("gyro register data path vender_div: %d\n", cxt->gyro_data.vender_div);
	if(NULL == cxt->gyro_data.get_data)
	{
		GYRO_LOG("gyro register data path fail \n");
	 	return -1;
	}
	return 0;
}

int gyro_register_control_path(struct gyro_control_path *ctl)
{
	struct gyro_context *cxt = NULL;
	int err =0;
	cxt = gyro_context_obj;
	cxt->gyro_ctl.set_delay = ctl->set_delay;
	cxt->gyro_ctl.open_report_data= ctl->open_report_data;
	cxt->gyro_ctl.enable_nodata = ctl->enable_nodata;
	cxt->gyro_ctl.is_support_batch = ctl->is_support_batch;
	cxt->gyro_ctl.gyro_calibration = ctl->gyro_calibration;
	cxt->gyro_ctl.is_use_common_factory = ctl->is_use_common_factory;
	
	if(NULL==cxt->gyro_ctl.set_delay || NULL==cxt->gyro_ctl.open_report_data
		|| NULL==cxt->gyro_ctl.enable_nodata)
	{
		GYRO_LOG("gyro register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = gyro_misc_init(gyro_context_obj);
	if(err)
	{
	   GYRO_ERR("unable to register gyro misc device!!\n");
	   return -2;
	}
	err = sysfs_create_group(&gyro_context_obj->mdev.this_device->kobj,
			&gyro_attribute_group);
	if (err < 0)
	{
	   GYRO_ERR("unable to create gyro attribute file\n");
	   return -3;
	}

	kobject_uevent(&gyro_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;	
}

static int x1,y1,z1=0;
static long pc=0;

static int check_repeat_data(int x, int y, int z)
{
    if ((x1==x) && (y1==y) &&(z1==z))
        pc++;
    else
        pc = 0;

    x1=x;y1=y;z1=z;

    if (pc >100) {
        GYRO_ERR("Gyro sensor output repeat data\n");
        pc=0;
    }
    return 0;
}
int gyro_data_report(int x, int y, int z,int status)
{
	//GYRO_LOG("+gyro_data_report! %d, %d, %d, %d\n",x,y,z,status);
    	struct gyro_context *cxt = NULL;
	int err =0;
    check_repeat_data(x,y,z);
	cxt = gyro_context_obj;
  	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_X, x);
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_Y, y);
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_Z, z);
	input_report_abs(cxt->idev, EVENT_TYPE_GYRO_STATUS, status);
	input_report_rel(cxt->idev, EVENT_TYPE_GYRO_UPDATE, 1);
	input_sync(cxt->idev); 
	return err;
}

static int gyro_probe(struct platform_device *pdev) 
{

	int err;
	GYRO_LOG("+++++++++++++gyro_probe!!\n");

	gyro_context_obj = gyro_context_alloc_object();
	if (!gyro_context_obj)
	{
		err = -ENOMEM;
		GYRO_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	//init real gyroeleration driver
    err = gyro_real_driver_init();
	if(err)
	{
		GYRO_ERR("gyro real driver init fail\n");
		goto real_driver_init_fail;
	}

	err = gyro_factory_device_init();
	if(err)
	{
		GYRO_ERR("gyro_factory_device_init fail\n");
	}

	//init input dev
	err = gyro_input_init(gyro_context_obj);
	if(err)
	{
		GYRO_ERR("unable to register gyro input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
    	atomic_set(&(gyro_context_obj->early_suspend), 0);
	gyro_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	gyro_context_obj->early_drv.suspend  = gyro_early_suspend,
	gyro_context_obj->early_drv.resume   = gyro_late_resume,    
	register_early_suspend(&gyro_context_obj->early_drv);
#endif //#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)

	GYRO_LOG("----gyro_probe OK !!\n");
	return 0;

	//exit_hwmsen_create_attr_failed:
	//exit_misc_register_failed:    

	//exit_err_sysfs:
	
	if (err)
	{
	   GYRO_ERR("sysfs node creation error \n");
	   gyro_input_destroy(gyro_context_obj);
	}
	
	real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(gyro_context_obj);
	
	exit_alloc_data_failed:
	

	GYRO_LOG("----gyro_probe fail !!!\n");
	return err;
}



static int gyro_remove(struct platform_device *pdev)
{
	int err=0;
	GYRO_FUN(f);
	input_unregister_device(gyro_context_obj->idev);        
	sysfs_remove_group(&gyro_context_obj->idev->dev.kobj,
				&gyro_attribute_group);
	
	if((err = misc_deregister(&gyro_context_obj->mdev)))
	{
		GYRO_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(gyro_context_obj);

	return 0;
}

static void gyro_early_suspend(struct early_suspend *h) 
{
   atomic_set(&(gyro_context_obj->early_suspend), 1);
   GYRO_LOG(" gyro_early_suspend ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(gyro_context_obj->early_suspend)));
   return ;
}
/*----------------------------------------------------------------------------*/
static void gyro_late_resume(struct early_suspend *h)
{
   atomic_set(&(gyro_context_obj->early_suspend), 0);
   GYRO_LOG(" gyro_late_resume ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(gyro_context_obj->early_suspend)));
   return ;
}

static int gyro_suspend(struct platform_device *dev, pm_message_t state) 
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static int gyro_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_gyro_pl_of_match[] = {
	{ .compatible = "mediatek,m_gyro_pl", },
	{},
};
#endif

static struct platform_driver gyro_driver =
{
	.probe      = gyro_probe,
	.remove     = gyro_remove,    
	.suspend    = gyro_suspend,
	.resume     = gyro_resume,
	.driver     = 
	{
		.name = GYRO_PL_DEV_NAME,//mt_gyro_pl
		#ifdef CONFIG_OF
		.of_match_table = m_gyro_pl_of_match,
		#endif
	}
};

static int __init gyro_init(void) 
{
	GYRO_FUN(f);

	if(platform_driver_register(&gyro_driver))
	{
		GYRO_ERR("failed to register gyro driver\n");
		return -ENODEV;
	}
	
	return 0;
}

static void __exit gyro_exit(void)
{
	platform_driver_unregister(&gyro_driver); 
	platform_driver_unregister(&gyroscope_driver);      
}

late_initcall(gyro_init);
//module_init(gyro_init);
//module_exit(gyro_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GYROSCOPE device driver");
MODULE_AUTHOR("Mediatek");

