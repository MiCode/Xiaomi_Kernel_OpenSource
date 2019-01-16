
#include "mag.h"
#include "accel.h"

struct mag_context *mag_context_obj = NULL;

static struct mag_init_info* msensor_init_list[MAX_CHOOSE_G_NUM]= {0}; //modified

static void mag_early_suspend(struct early_suspend *h);
static void mag_late_resume(struct early_suspend *h);

static void mag_work_func(struct work_struct *work)
{

	struct mag_context *cxt = NULL;
	hwm_sensor_data sensor_data;
	int64_t  nt;
	struct timespec time; 
	int err;	
	int i;
	int x,y,z,status;
	static int flag=0;
	cxt  = mag_context_obj;
    memset(&sensor_data, 0, sizeof(sensor_data));	
	time.tv_sec = time.tv_nsec = 0;    
	time = get_monotonic_coarse(); 
	nt = time.tv_sec*1000000000LL+time.tv_nsec;
	
	for(i = 0; i < MAX_M_V_SENSOR; i++)
	{
	   
	   if(!(cxt->active_data_sensor&(0x01<<i)))
	   {
	       MAG_LOG("mag_type(%d)  enabled(%d)\n",i,cxt->active_data_sensor);
		   continue;
	   }

		if(ID_M_V_MAGNETIC ==i)
		{
			err = cxt->mag_dev_data.get_data_m(&x,&y,&z,&status);
			if(err)
	   		{
		  		MAG_ERR("get %d data fails!!\n" ,i);
		  		return;
	   		}
			cxt->drv_data[i].mag_data.values[0]=x;
			cxt->drv_data[i].mag_data.values[1]=y;
			cxt->drv_data[i].mag_data.values[2]=z;
			cxt->drv_data[i].mag_data.status = status;
			if(true ==  cxt->is_first_data_after_enable)
	    	{
		   		cxt->is_first_data_after_enable = false;
		   		//filter -1 value
	       		if(MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[0] ||
		   	     MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[1] ||
		   	     MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[2])
	       		{
	          		MAG_LOG(" read invalid data \n");
	       	  		continue;
			
	       		}
	    	}
			mag_data_report(MAGNETIC,cxt->drv_data[i].mag_data.values[0],
				cxt->drv_data[i].mag_data.values[1],
				cxt->drv_data[i].mag_data.values[2],
				cxt->drv_data[i].mag_data.status);
		
		  	//MAG_LOG("mag_type(%d) data[%d,%d,%d] \n" ,i,cxt->drv_data[i].mag_data.values[0],
	    	//cxt->drv_data[i].mag_data.values[1],cxt->drv_data[i].mag_data.values[2]);
		}
		
		if(ID_M_V_ORIENTATION ==i)
		{
			
			err = cxt->mag_dev_data.get_data_o(&x,&y,&z,&status);
			flag = !flag;
			if(flag)
				z++;
			else
				z--;
			if(err)
	   		{
		  		MAG_ERR("get %d data fails!!\n" ,i);
		  		return;
	   		}
			cxt->drv_data[i].mag_data.values[0]=x;
			cxt->drv_data[i].mag_data.values[1]=y;
			cxt->drv_data[i].mag_data.values[2]=z;
			cxt->drv_data[i].mag_data.status = status;
			if(true ==  cxt->is_first_data_after_enable)
	    	{
		   		cxt->is_first_data_after_enable = false;
		   		//filter -1 value
	       		if(MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[0] ||
		   	     MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[1] ||
		   	     MAG_INVALID_VALUE == cxt->drv_data[i].mag_data.values[2])
	       		{
	          		MAG_LOG(" read invalid data \n");
	       	  		continue;
			
	       		}
	    	}
			mag_data_report(ORIENTATION,cxt->drv_data[i].mag_data.values[0],
				cxt->drv_data[i].mag_data.values[1],
				cxt->drv_data[i].mag_data.values[2],
				cxt->drv_data[i].mag_data.status);
		
		  	//MAG_LOG("mag_type(%d) data[%d,%d,%d] \n" ,i,cxt->drv_data[i].mag_data.values[0],
	    	//cxt->drv_data[i].mag_data.values[1],cxt->drv_data[i].mag_data.values[2]);
		}
	    

	}

	//report data to input device
	//printk("new mag work run....\n");
	
	if(true == cxt->is_polling_run)
	{
		  mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ)); 
	}
}

static void mag_poll(unsigned long data)
{
	struct mag_context *obj = (struct mag_context *)data;
	if(obj != NULL)
	{
		schedule_work(&obj->report);
	}
}

static struct mag_context *mag_context_alloc_object(void)
{
	
	struct mag_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL); 
    MAG_LOG("mag_context_alloc_object++++\n");
	if(!obj)
	{
		MAG_ERR("Alloc magel object error!\n");
		return NULL;
	}	
	atomic_set(&obj->delay, 200); /*5Hz*/// set work queue delay time 200ms
	atomic_set(&obj->wake, 0);
	INIT_WORK(&obj->report, mag_work_func);
	init_timer(&obj->timer);
	obj->timer.expires	= jiffies + atomic_read(&obj->delay)/(1000/HZ);
	obj->timer.function	= mag_poll;
	obj->timer.data		= (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	obj->active_data_sensor = 0;
	obj->active_nodata_sensor = 0;
	obj->is_batch_enable = false;
	mutex_init(&obj->mag_op_mutex);
	MAG_LOG("mag_context_alloc_object----\n");
	return obj;
}
static int mag_enable_data(int handle,int enable)
{
    struct mag_context *cxt = NULL;
	//int err =0;
	cxt = mag_context_obj;
	
	if(NULL  == cxt->drv_obj[handle] && NULL == cxt->mag_ctl.m_enable)
	{
	  MAG_ERR("no real mag driver\n");
	  return -1;
	}
    
    if(1 == enable)
    {
       MAG_LOG("MAG(%d) enable \n",handle);
       cxt->is_first_data_after_enable = true;
	   cxt->active_data_sensor |= 1<<handle;
	   
	   if(ID_M_V_ORIENTATION == handle)
	   {
	   		cxt->mag_ctl.o_enable(1);
		  	cxt->mag_ctl.o_open_report_data(1);
	   }
	   if(ID_M_V_MAGNETIC == handle)
	   {
	   		cxt->mag_ctl.m_enable(1);
	   		cxt->mag_ctl.m_open_report_data(1);
	   }
	   
	   if((0!=cxt->active_data_sensor) && (false == cxt->is_polling_run)&&(false == cxt->is_batch_enable))
	   {
	      if(false == cxt->mag_ctl.is_report_input_direct)
	      {
	       		MAG_LOG("MAG(%d)  mod timer \n",handle);
	      		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
		  		cxt->is_polling_run = true;
	      }
	   }
	  
	   
    }
	if(0 == enable)
	{
	   MAG_LOG("MAG(%d) disable \n",handle);
	   cxt->active_data_sensor &= ~(1<<handle);
	   if(ID_M_V_ORIENTATION == handle)
	   {
	   		cxt->mag_ctl.o_enable(0);
		  	cxt->mag_ctl.o_open_report_data(0);
	   }
	   if(ID_M_V_MAGNETIC == handle)
	   {
	   		cxt->mag_ctl.m_enable(0);
	   		cxt->mag_ctl.m_open_report_data(0);
	   }
	   
	   if(0 == cxt->active_data_sensor && true == cxt->is_polling_run)
	   {
	   		if(false == cxt->mag_ctl.is_report_input_direct)
	   		{
	   			MAG_LOG("MAG(%d)  del timer \n",handle);
	      		cxt->is_polling_run = false;
                  smp_mb();
	      		del_timer_sync(&cxt->timer);
                  smp_mb();
	      		cancel_work_sync(&cxt->report);
				cxt->drv_data[handle].mag_data.values[0] = MAG_INVALID_VALUE;
	   			cxt->drv_data[handle].mag_data.values[1] = MAG_INVALID_VALUE;
	   			cxt->drv_data[handle].mag_data.values[2] = MAG_INVALID_VALUE;
	   		}
	      
	   }

	}
	//mag_real_enable(handle,enable);
	return 0;
}
/*
static int m_enable_data(int en)
{
   int err;
   err=mag_enable_data(ID_M_V_MAGNETIC,en);
   return err;
}
static int o_enable_data(int en)
{
   int err;
   err=mag_enable_data(ID_M_V_ORIENTATION,en);
   return err;
}
*/
/*
static int m_set_delay(u64 delay)//ns
{
    int m_delay =0;
	m_delay = (int)delay/1000/1000;

	if(NULL  == mag_context_obj->drv_obj[ID_M_V_MAGNETIC])
	{
	  MAG_ERR("no real mag driver\n");
	  return -1;
	}
	if(mag_context_obj->drv_obj[ID_M_V_MAGNETIC]->mag_operate(mag_context_obj->drv_obj[ID_M_V_MAGNETIC]->self, OP_MAG_DELAY, &m_delay,sizeof(int), NULL, 0, NULL) != 0)
	{
	   MAG_ERR("mag sensor_operate set delay function error \r\n");
	}

    atomic_set(&mag_context_obj->delay, m_delay);
    MAG_LOG(" mag_delay %d ms\n",m_delay);
	return 0;
}

static int o_set_delay(u64 delay)//ns
{
    int m_delay =0;
	m_delay = (int)delay/1000/1000;
	
	if(NULL  == mag_context_obj->drv_obj[ID_M_V_ORIENTATION])
	{
		MAG_ERR("no real mag odriver\n");
		return -1;
	}

	if(mag_context_obj->drv_obj[ID_M_V_ORIENTATION]->mag_operate(mag_context_obj->drv_obj[ID_M_V_ORIENTATION]->self, OP_MAG_DELAY, &m_delay,sizeof(int), NULL, 0, NULL) != 0)
	{
		MAG_ERR("mag osensor_operate set delay function error \r\n");
	}
	atomic_set(&mag_context_obj->delay, m_delay);
    MAG_LOG(" mag_odelay %d ms\n",m_delay);
	return 0;

}
*/

/*----------------------------------------------------------------------------*/
static ssize_t mag_show_magdev(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	printk("sensor test: mag function!\n");
    return len;
}
/*----------------------------------------------------------------------------*/

static ssize_t mag_store_oactive(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;
	//int err =0;
	MAG_LOG("mag_store_oactive buf=%s\n",buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if(NULL == cxt->mag_ctl.o_enable)
	{
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl o-enable NULL\n");
	 	return count;
	}
	
    if (!strncmp(buf, "1", 1)) 
	{
       mag_enable_data(ID_M_V_ORIENTATION,1);
       //cxt->mag_ctl.o_enable(1);
    } 
	else if (!strncmp(buf, "0", 1))
	{
       mag_enable_data(ID_M_V_ORIENTATION,0);    
       //cxt->mag_ctl.o_enable(0);
    }
	else
	{
	  MAG_ERR(" mag_store_oactive error !!\n");
	}
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_oactive done\n");
    return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t mag_show_oactive(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct mag_context *cxt = NULL;
	int div = 0;
	cxt = mag_context_obj;
	div = cxt->mag_dev_data.div_o;
	ACC_LOG("acc mag_dev_data o_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);

}

static ssize_t mag_store_active(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;
	//int err =0;
    MAG_LOG("mag_store_active buf=%s\n",buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if(NULL == cxt->mag_ctl.m_enable)
	{
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl path is NULL\n");
	 	return count;
	}
	
    if (!strncmp(buf, "1", 1)) 
	{
       mag_enable_data(ID_M_V_MAGNETIC,1);
       //cxt->mag_ctl.m_enable(1);
    } 
	else if (!strncmp(buf, "0", 1))
	{
       mag_enable_data(ID_M_V_MAGNETIC,0);    
       //cxt->mag_ctl.m_enable(0);
    }
	else
	{
	  MAG_ERR(" mag_store_active error !!\n");
	}
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_active done\n");
    return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t mag_show_active(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct mag_context *cxt = NULL;
	int div = 0;
	cxt = mag_context_obj;
	div = cxt->mag_dev_data.div_m;
	ACC_LOG("acc mag_dev_data m_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div); 
}

static ssize_t mag_store_odelay(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
  //  struct mag_context *devobj = (struct mag_context*)dev_get_drvdata(dev);
    int delay=0;
	int mdelay=0;
	struct mag_context *cxt = NULL;
	//int err =0;
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if(NULL == cxt->mag_ctl.o_set_delay)
	{
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl o_delay NULL\n");
	 	return count;
	}
	
    MAG_LOG(" mag_odelay ++ \n");
 

    if (1 != sscanf(buf, "%d", &delay)) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_ERR("invalid format!!\n");
        return count;
    }
	if(false == cxt->mag_ctl.is_report_input_direct)
    {
    	mdelay = (int)delay/1000/1000;
    	atomic_set(&mag_context_obj->delay, mdelay);
    }
    cxt->mag_ctl.o_set_delay(delay);
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_odelay %d ns done\n",delay);
    return count;
}

static ssize_t mag_show_odelay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_delay(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
   // struct mag_context *devobj = (struct mag_context*)dev_get_drvdata(dev);
    int delay=0;
	int mdelay=0;
	struct mag_context *cxt = NULL;
	//int err =0;
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if(NULL == cxt->mag_ctl.m_set_delay)
	{
		mutex_unlock(&mag_context_obj->mag_op_mutex);
		MAG_LOG("mag_ctl m_delay NULL\n");
	 	return count;
	}
	
    MAG_LOG(" mag_delay ++ \n");
  
	if (1 != sscanf(buf, "%d", &delay)) {
		mutex_unlock(&mag_context_obj->mag_op_mutex);
        MAG_ERR("invalid format!!\n");
        return count;
    }
	if(false == cxt->mag_ctl.is_report_input_direct)
    {
    	mdelay = (int)delay/1000/1000;
    	atomic_set(&mag_context_obj->delay, mdelay);
    }
	cxt->mag_ctl.m_set_delay(delay);
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_delay %d ns done\n",delay);
    return count;
}

static ssize_t mag_show_delay(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    int len = 0;
	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_batch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;
	//int err =0;
	MAG_LOG("mag_store_batch buf=%s\n",buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if(cxt->mag_ctl.is_support_batch){
	    	if (!strncmp(buf, "1", 1)) 
		{
	    		cxt->is_batch_enable = true;
                if(true == cxt->is_polling_run)
                {
                    cxt->is_polling_run = false;
                    del_timer_sync(&cxt->timer);
                    cancel_work_sync(&cxt->report);
                    cxt->drv_data[ID_M_V_MAGNETIC].mag_data.values[0] = MAG_INVALID_VALUE;
                    cxt->drv_data[ID_M_V_MAGNETIC].mag_data.values[1] = MAG_INVALID_VALUE;
                    cxt->drv_data[ID_M_V_MAGNETIC].mag_data.values[2] = MAG_INVALID_VALUE;
                }
	   	 } 
		else if (!strncmp(buf, "0", 1))
		{
			cxt->is_batch_enable = false;
                if(false == cxt->is_polling_run)
                {
                    if(false == cxt->mag_ctl.is_report_input_direct)
                    {
                        mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
                        cxt->is_polling_run = true;
                    }
                }
	    	}
		else
		{
			MAG_ERR(" mag_store_batch error !!\n");
		}
	}else{
		MAG_LOG(" mag_store_batch not supported\n");
	}
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_batch done: %d\n", cxt->is_batch_enable);
    	return count;
}


static ssize_t mag_show_batch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    	int len = 0;
	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_flush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    return count;
}
static ssize_t mag_show_sensordevnum(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
	struct mag_context *cxt = NULL;
	const char *devname = NULL;
	cxt = mag_context_obj;
	devname = dev_name(&cxt->idev->dev);

 return snprintf(buf, PAGE_SIZE, "%s\n", devname+5); 
}


static ssize_t mag_show_flush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    	int len = 0;
	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_obatch(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct mag_context *cxt = NULL;
	//int err =0;
	MAG_LOG("mag_store_obatch buf=%s\n",buf);
	mutex_lock(&mag_context_obj->mag_op_mutex);
	cxt = mag_context_obj;
	if(cxt->mag_ctl.is_support_batch){
	    	if (!strncmp(buf, "1", 1)) 
		{
	    		cxt->is_batch_enable = true;
                if(true == cxt->is_polling_run)
                {
                    cxt->is_polling_run = false;
                    del_timer_sync(&cxt->timer);
                    cancel_work_sync(&cxt->report);
                    cxt->drv_data[ID_M_V_ORIENTATION].mag_data.values[0] = MAG_INVALID_VALUE;
                    cxt->drv_data[ID_M_V_ORIENTATION].mag_data.values[1] = MAG_INVALID_VALUE;
                    cxt->drv_data[ID_M_V_ORIENTATION].mag_data.values[2] = MAG_INVALID_VALUE;
                }
	   	 } 
		else if (!strncmp(buf, "0", 1))
		{
			cxt->is_batch_enable = false;
                if(false == cxt->is_polling_run)
                {
                    if(false == cxt->mag_ctl.is_report_input_direct)
                    {
                        mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay)/(1000/HZ));
                        cxt->is_polling_run = true;
                    }
                }
	    	}
		else
		{
			MAG_ERR(" mag_store_obatch error !!\n");
		}
	}else{
		MAG_LOG(" mag_store_obatch not supported\n");		
	}
	mutex_unlock(&mag_context_obj->mag_op_mutex);
	MAG_LOG(" mag_store_obatch done: %d\n", cxt->is_batch_enable);
    	return count;

}


static ssize_t mag_show_obatch(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    	int len = 0;
	MAG_LOG(" not support now\n");
	return len;
}

static ssize_t mag_store_oflush(struct device* dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    	return count;
}


static ssize_t mag_show_oflush(struct device* dev, 
                                 struct device_attribute *attr, char *buf) 
{
    	int len = 0;
	MAG_LOG(" not support now\n");
	return len;
}


int mag_attach(int sensor,struct mag_drv_obj *obj)
{
	int err = 0;
	MAG_FUN();
	//mag_context_obj->drv_obj[sensor] =  obj;
	mag_context_obj->drv_obj[sensor] = kzalloc(sizeof(struct mag_drv_obj), GFP_KERNEL);
	if(mag_context_obj->drv_obj[sensor] == NULL)
	{
		err = -EPERM;
		MAG_ERR(" mag attatch alloc fail \n");
		return err;
	}				
		
	memcpy(mag_context_obj->drv_obj[sensor], obj, sizeof(*obj));
	if(NULL == mag_context_obj->drv_obj[sensor])
	{
	  err  =-1;
	  MAG_ERR(" mag attatch fail \n");
	}
	return err;
}
/*----------------------------------------------------------------------------*/
EXPORT_SYMBOL_GPL(mag_attach);


static int msensor_remove(struct platform_device *pdev)
{
	MAG_LOG("msensor_remove\n");
	return 0;
}

static int msensor_probe(struct platform_device *pdev) 
{
	MAG_LOG("msensor_probe\n");
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id msensor_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver msensor_driver = {
	.probe      = msensor_probe,
	.remove     = msensor_remove,    
	.driver     = 
	{
		.name  = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = msensor_of_match,
		#endif
	}
};

static int mag_real_driver_init(void) 
{
    int i =0;
	int err=0;
	MAG_LOG(" mag_real_driver_init +\n");
	for(i = 0; i < MAX_CHOOSE_G_NUM; i++)
	{
	  MAG_LOG(" i=%d\n",i);
	  if(0 != msensor_init_list[i])
	  {
	    MAG_LOG(" mag try to init driver %s\n", msensor_init_list[i]->name);
	    err = msensor_init_list[i]->init();
		if(0 == err)
		{
		   MAG_LOG(" mag real driver %s probe ok\n", msensor_init_list[i]->name);
		   break;
		}
	  }
	}

	if(i == MAX_CHOOSE_G_NUM)
	{
	   MAG_LOG(" mag_real_driver_init fail\n");
	   err =-1;
	}
	return err;
}

 int mag_driver_add(struct mag_init_info* obj) 
{
    int err=0;
	int i =0;
	
	MAG_FUN();
	if (!obj) {
		MAG_ERR("MAG driver add fail, mag_init_info is NULL \n");
		return -1;
	}

	for(i =0; i < MAX_CHOOSE_G_NUM; i++ )
	{
        if((i == 0) && (NULL == msensor_init_list[0])){
			MAG_LOG("register mensor driver for the first time\n");
			if(platform_driver_register(&msensor_driver))
			{
				MAG_ERR("failed to register msensor driver already exist\n");
			}
		}
	    if(NULL == msensor_init_list[i])
	    {
	    	obj->platform_diver_addr = &msensor_driver;
	      	msensor_init_list[i] = obj;
		  	break;
	    }
	}
	if(i >= MAX_CHOOSE_G_NUM)
	{
	   MAG_ERR("MAG driver add err \n");
	   err =-1;
	}
		
	return err;
}
EXPORT_SYMBOL_GPL(mag_driver_add);

static int mag_misc_init(struct mag_context *cxt)
{

    int err=0;
    cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name  = MAG_MISC_DEV_NAME;
	
	if((err = misc_register(&cxt->mdev)))
	{
		MAG_ERR("unable to register mag misc device!!\n");
	}
	return err;
}

static void mag_input_destroy(struct mag_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int mag_input_init(struct mag_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = MAG_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_Y); 
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_MAGEL_STATUS);

	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_O_STATUS);
	
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_X, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_Y, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_Z, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_MAGEL_STATUS, MAG_STATUS_MIN, MAG_STATUS_MAX, 0, 0);
	
	input_set_abs_params(dev, EVENT_TYPE_O_X, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_Y, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_Z, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_O_STATUS, MAG_STATUS_MIN, MAG_STATUS_MAX, 0, 0);
	
	input_set_drvdata(dev, cxt);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev= dev;

	return 0;
}

DEVICE_ATTR(magdev,        S_IWUSR | S_IRUGO, mag_show_magdev, NULL);
DEVICE_ATTR(magactive,     S_IWUSR | S_IRUGO, mag_show_active, mag_store_active);
DEVICE_ATTR(magdelay,      S_IWUSR | S_IRUGO, mag_show_delay,  mag_store_delay);
DEVICE_ATTR(magoactive,     S_IWUSR | S_IRUGO, mag_show_oactive, mag_store_oactive);
DEVICE_ATTR(magodelay,      S_IWUSR | S_IRUGO, mag_show_odelay,  mag_store_odelay);
DEVICE_ATTR(magbatch,      	S_IWUSR | S_IRUGO, mag_show_batch,  mag_store_batch);
DEVICE_ATTR(magflush,      		S_IWUSR | S_IRUGO, mag_show_flush,  mag_store_flush);
DEVICE_ATTR(magobatch,      	S_IWUSR | S_IRUGO, mag_show_obatch,  mag_store_obatch);
DEVICE_ATTR(magoflush,      	S_IWUSR | S_IRUGO, mag_show_oflush,  mag_store_oflush);
DEVICE_ATTR(magdevnum,      	S_IWUSR | S_IRUGO, mag_show_sensordevnum,  NULL);

static struct attribute *mag_attributes[] = {
	&dev_attr_magdev.attr,
	&dev_attr_magactive.attr,
	&dev_attr_magdelay.attr,
	&dev_attr_magbatch.attr,
	&dev_attr_magflush.attr,
	&dev_attr_magoactive.attr,
	&dev_attr_magodelay.attr,
	&dev_attr_magobatch.attr,	
	&dev_attr_magoflush.attr,
	&dev_attr_magdevnum.attr,
	NULL
};

static struct attribute_group mag_attribute_group = {
	.attrs = mag_attributes
};


int mag_register_data_path(struct mag_data_path *data)
{
	struct mag_context *cxt = NULL;
	//int err =0;
	cxt = mag_context_obj;
	cxt->mag_dev_data.div_m = data->div_m;
	cxt->mag_dev_data.div_o = data->div_o;
	cxt->mag_dev_data.get_data_m = data->get_data_m;
	cxt->mag_dev_data.get_data_o = data->get_data_o;
	cxt->mag_dev_data.get_raw_data = data->get_raw_data;
	MAG_LOG("mag register data path div_o: %d\n", cxt->mag_dev_data.div_o);
	MAG_LOG("mag register data path div_m: %d\n", cxt->mag_dev_data.div_m);

	return 0;
}

int mag_register_control_path(struct mag_control_path *ctl)
{
	struct mag_context *cxt = NULL;
	int err =0;
	cxt = mag_context_obj;

	cxt->mag_ctl.m_set_delay = ctl->m_set_delay;
	cxt->mag_ctl.m_enable= ctl->m_enable;
	cxt->mag_ctl.m_open_report_data= ctl->m_open_report_data;
	cxt->mag_ctl.o_set_delay = ctl->o_set_delay;
	cxt->mag_ctl.o_open_report_data= ctl->o_open_report_data;
	cxt->mag_ctl.o_enable= ctl->o_enable;
	cxt->mag_ctl.is_report_input_direct = ctl->is_report_input_direct;
	cxt->mag_ctl.is_support_batch = ctl->is_support_batch;
	cxt->mag_ctl.is_use_common_factory = ctl->is_use_common_factory;
	
	if(NULL==cxt->mag_ctl.m_set_delay || NULL==cxt->mag_ctl.m_enable
		|| NULL==cxt->mag_ctl.m_open_report_data
		|| NULL==cxt->mag_ctl.o_set_delay || NULL==cxt->mag_ctl.o_open_report_data
		|| NULL==cxt->mag_ctl.o_enable)
	{
		MAG_LOG("mag register control path fail \n");
	 	return -1;
	}

	//add misc dev for sensor hal control cmd
	err = mag_misc_init(mag_context_obj);
	if(err)
	{
	   MAG_ERR("unable to register acc misc device!!\n");
	   return -2;
	}
	err = sysfs_create_group(&mag_context_obj->mdev.this_device->kobj,
			&mag_attribute_group);
	if (err < 0)
	{
	  MAG_ERR("unable to create acc attribute file\n");
	   return -3;
	}

	kobject_uevent(&mag_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;	
}
static int x1,y1,z1=0;
static long pc=0;
static long count =0;

static int check_repeat_data(int x, int y, int z)
{
    if ((x1==x) && (y1==y) &&(z1==z))
        pc++;
    else
        pc = 0;

    x1=x;y1=y;z1=z;

    if (pc >100) {
        MAG_ERR("Mag sensor output repeat data\n");
        pc=0;
    }
    return 0;
}

static int check_abnormal_data(int x, int y, int z, int status)
{
    long total;
    total = (x*x + y*y + z*z)/16;
    if ((total <100) || (total >10000)) {
        if (count %10 == 0) {
             MAG_ERR("mag sensor abnormal data: x=%d,y=%d,z=%d, status=%d \n",x,y,z,status);
        }
        count++;
        if (count >1000)
            count=0;
    }
    return 0;
}
int mag_data_report(MAG_TYPE type,int x, int y, int z, int status)
{
	//MAG_LOG("update!valus: %d, %d, %d, %d\n" , x, y, z, status);
    struct mag_context *cxt = NULL;
    check_repeat_data(x,y,z);
    check_abnormal_data(x,y,z,status);
	//int err =0;
	cxt = mag_context_obj;
	if(MAGNETIC==type)
	{
        input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_STATUS, status);
		input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_X, x);
	    input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_Y, y);
	    input_report_abs(cxt->idev, EVENT_TYPE_MAGEL_Z, z);
		input_sync(cxt->idev);  	
	}
	if(ORIENTATION==type)
	{
   		input_report_abs(cxt->idev, EVENT_TYPE_O_STATUS, status);
		input_report_abs(cxt->idev, EVENT_TYPE_O_X, x);
	  	input_report_abs(cxt->idev, EVENT_TYPE_O_Y, y);
	   	input_report_abs(cxt->idev, EVENT_TYPE_O_Z, z);
		input_sync(cxt->idev); 
	}

	return 0;
}

static int mag_probe(struct platform_device *pdev) 
{

	int err;
	MAG_LOG("+++++++++++++mag_probe!!\n");
	mag_context_obj = mag_context_alloc_object();
	if (!mag_context_obj)
	{
		err = -ENOMEM;
		MAG_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	//init real mageleration driver
    err = mag_real_driver_init();
	if(err)
	{
		goto mag_real_driver_init_fail;
		MAG_ERR("mag_real_driver_init fail\n");
	}

	err = mag_factory_device_init();
	if(err)
	{
		MAG_ERR("mag_factory_device_init fail\n");
	}
    //init input dev
	err = mag_input_init(mag_context_obj);
	if(err)
	{
		MAG_ERR("unable to register mag input device!\n");
		goto exit_alloc_input_dev_failed;
	}

#if defined(CONFIG_HAS_EARLYSUSPEND)
    atomic_set(&(mag_context_obj->early_suspend), 0);
	mag_context_obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
	mag_context_obj->early_drv.suspend  = mag_early_suspend,
	mag_context_obj->early_drv.resume   = mag_late_resume,    
	register_early_suspend(&mag_context_obj->early_drv);
#endif

	MAG_LOG("----magel_probe OK !!\n");
	return 0;

	//exit_hwmsen_create_attr_failed:
	//exit_misc_register_failed:    

	//exit_err_sysfs:
	
	if (err)
	{
	   MAG_ERR("sysfs node creation error \n");
	   mag_input_destroy(mag_context_obj);
	}

	mag_real_driver_init_fail:
	exit_alloc_input_dev_failed:    
	kfree(mag_context_obj);
	
	exit_alloc_data_failed:

	MAG_LOG("----magel_probe fail !!!\n");
	return err;
}



static int mag_remove(struct platform_device *pdev)
{
	int err=0;
	MAG_FUN(f);
	input_unregister_device(mag_context_obj->idev);        
	sysfs_remove_group(&mag_context_obj->idev->dev.kobj,
				&mag_attribute_group);
	
	if((err = misc_deregister(&mag_context_obj->mdev)))
	{
		MAG_ERR("misc_deregister fail: %d\n", err);
	}
	kfree(mag_context_obj);

	return 0;
}

static void mag_early_suspend(struct early_suspend *h) 
{
   atomic_set(&(mag_context_obj->early_suspend), 1);
   MAG_LOG(" mag_context_obj ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(mag_context_obj->early_suspend)));
   return ;
}
/*----------------------------------------------------------------------------*/
static void mag_late_resume(struct early_suspend *h)
{
   atomic_set(&(mag_context_obj->early_suspend), 0);
   MAG_LOG(" mag_context_obj ok------->hwm_obj->early_suspend=%d \n",atomic_read(&(mag_context_obj->early_suspend)));
   return ;
}

static int mag_suspend(struct platform_device *dev, pm_message_t state) 
{
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mag_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_mag_pl_of_match[] = {
	{ .compatible = "mediatek,m_mag_pl", },
	{},
};
#endif

static struct platform_driver mag_driver =
{
	.probe      = mag_probe,
	.remove     = mag_remove,    
	.suspend    = mag_suspend,
	.resume     = mag_resume,
	.driver     = 
	{
		.name = MAG_PL_DEV_NAME,
        #ifdef CONFIG_OF
		.of_match_table = m_mag_pl_of_match,
		#endif
	}
};

static int __init mag_init(void) 
{
	MAG_FUN();

	if(platform_driver_register(&mag_driver))
	{
		MAG_ERR("failed to register mag driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit mag_exit(void)
{
	platform_driver_unregister(&mag_driver); 
   	platform_driver_unregister(&msensor_driver); 
}

late_initcall(mag_init);
//module_init(mag_init);
//module_exit(mag_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAGELEROMETER device driver");
MODULE_AUTHOR("Mediatek");

