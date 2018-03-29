/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include "pdr_sensor.h"

static struct pdr_context *pdr_context_obj;


static struct pdr_init_info *pdr_init_list[MAX_CHOOSE_PDR_NUM] = { 0 };	/* modified */

#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
static void pdr_early_suspend(struct early_suspend *h);
static void pdr_late_resume(struct early_suspend *h);
#endif

static void pdr_work_func(struct work_struct *work)
{
	struct timespec time;
	int sensor_data[4];
	int status = 0;
	int err = 0;
	struct pdr_context *cxt = NULL;
	int64_t nt = 0;

	cxt = pdr_context_obj;
	if (NULL == cxt->pdr_data.get_data)
		PDR_LOG("pdr driver not register data path\n");

	time.tv_sec = time.tv_nsec = 0;
	time = get_monotonic_coarse();
	nt = time.tv_sec * 1000000000LL + time.tv_nsec;

	err = cxt->pdr_data.get_data(sensor_data, &status);
	/* PDR_ERR("pdr data:%d,%d,%d,status:%d\n", sensor_data[0], sensor_data[1], sensor_data[2], status); */
	if (err) {
		PDR_ERR("get pdr data fails!!\n");
		goto pdr_loop;
	} else {
		if (0 == sensor_data[0] && 0 == sensor_data[1] && 0 == sensor_data[2])
			goto pdr_loop;

		cxt->drv_data.pdr_data.values[0] = sensor_data[0];	/*x axis */
		cxt->drv_data.pdr_data.values[1] = sensor_data[1];	/*y axis */
		cxt->drv_data.pdr_data.values[2] = sensor_data[2];	/*z axis */
		cxt->drv_data.pdr_data.values[3] = sensor_data[3];	/*scalar */
		cxt->drv_data.pdr_data.status = status;	/*status */
		cxt->drv_data.pdr_data.time = nt;
	}

	if (true == cxt->is_first_data_after_enable) {
		cxt->is_first_data_after_enable = false;
		/* filter -1 value */
		if (PDR_INVALID_VALUE == cxt->drv_data.pdr_data.values[0] ||
		    PDR_INVALID_VALUE == cxt->drv_data.pdr_data.values[1] ||
		    PDR_INVALID_VALUE == cxt->drv_data.pdr_data.values[2] ||
		    PDR_INVALID_VALUE == cxt->drv_data.pdr_data.values[3]) {
			PDR_LOG(" read invalid data\n");
			goto pdr_loop;
		}
	}
	/* report data to input device */
	/* printk("new pdr work run....\n"); */
	/* PDR_LOG("pdr data[%d,%d,%d]\n" ,cxt->drv_data.pdr_data.values[0], */
	/* cxt->drv_data.pdr_data.values[1],cxt->drv_data.pdr_data.values[2]); */
	pdr_data_report(cxt->drv_data.pdr_data.values[0],
			cxt->drv_data.pdr_data.values[1],
			cxt->drv_data.pdr_data.values[2],
			cxt->drv_data.pdr_data.values[3], cxt->drv_data.pdr_data.status);

pdr_loop:
	if (true == cxt->is_polling_run)
		mod_timer(&cxt->timer, jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
}

static void pdr_poll(unsigned long data)
{
	struct pdr_context *obj = (struct pdr_context *)data;

	if (obj != NULL)
		schedule_work(&obj->report);
}

static struct pdr_context *pdr_context_alloc_object(void)
{

	struct pdr_context *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

	/*PDR_ERR("[Bai]>>pdr_context_alloc_object++++\n"); */
	if (!obj) {
		PDR_ERR("Alloc pdr object error!\n");
		return NULL;
	}
	atomic_set(&obj->delay, 200);	/*5Hz  set work queue delay time 200ms */
	atomic_set(&obj->wake, 0);

	INIT_WORK(&obj->report, pdr_work_func);
	init_timer(&obj->timer);
	obj->timer.expires = jiffies + atomic_read(&obj->delay) / (1000 / HZ);
	obj->timer.function = pdr_poll;
	obj->timer.data = (unsigned long)obj;
	obj->is_first_data_after_enable = false;
	obj->is_polling_run = false;
	mutex_init(&obj->pdr_op_mutex);
	obj->is_batch_enable = false;	/* for batch mode init */
	/*PDR_ERR("[Bai]>>pdr_context_alloc_object end\n"); */
	return obj;
}

static int pdr_real_enable(int enable)
{
	int err = 0;
	struct pdr_context *cxt = NULL;

	cxt = pdr_context_obj;
	if (1 == enable) {

		if (true == cxt->is_active_data || true == cxt->is_active_nodata) {
			err = cxt->pdr_ctl.enable_nodata(1);
			if (err) {
				err = cxt->pdr_ctl.enable_nodata(1);
				if (err) {
					err = cxt->pdr_ctl.enable_nodata(1);
					if (err)
						PDR_ERR("pdr enable(%d) err 3 timers = %d\n",
							enable, err);
				}
			}
			PDR_LOG("pdr real enable\n");
		}

	}
	if (0 == enable) {
		if (false == cxt->is_active_data && false == cxt->is_active_nodata) {
			err = cxt->pdr_ctl.enable_nodata(0);
			if (err)
				PDR_ERR("pdr enable(%d) err = %d\n", enable, err);
			PDR_LOG("pdr real disable\n");
		}
	}

	return err;
}

static int pdr_enable_data(int enable)
{
	struct pdr_context *cxt = NULL;

	/* int err =0; */
	cxt = pdr_context_obj;
	if (NULL == cxt->pdr_ctl.open_report_data) {
		PDR_ERR("no pdr control path\n");
		return -1;
	}

	if (1 == enable) {
		PDR_LOG("PDR enable data\n");
		cxt->is_active_data = true;
		cxt->is_first_data_after_enable = true;
		cxt->pdr_ctl.open_report_data(1);
		if (false == cxt->is_polling_run && cxt->is_batch_enable == false) {
			if (false == cxt->pdr_ctl.is_report_input_direct) {
				mod_timer(&cxt->timer,
					  jiffies + atomic_read(&cxt->delay) / (1000 / HZ));
				cxt->is_polling_run = true;
			}
		}
	}
	if (0 == enable) {
		PDR_LOG("PDR disable\n");

		cxt->is_active_data = false;
		cxt->pdr_ctl.open_report_data(0);
		if (true == cxt->is_polling_run) {
			if (false == cxt->pdr_ctl.is_report_input_direct) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
				cxt->drv_data.pdr_data.values[0] = PDR_INVALID_VALUE;
				cxt->drv_data.pdr_data.values[1] = PDR_INVALID_VALUE;
				cxt->drv_data.pdr_data.values[2] = PDR_INVALID_VALUE;
			}
		}

	}
	pdr_real_enable(enable);
	return 0;
}



int pdr_enable_nodata(int enable)
{
	struct pdr_context *cxt = NULL;

	/* int err =0; */
	cxt = pdr_context_obj;
	if (NULL == cxt->pdr_ctl.enable_nodata) {
		PDR_ERR("pdr_enable_nodata:pdr ctl path is NULL\n");
		return -1;
	}

	if (1 == enable)
		cxt->is_active_nodata = true;

	if (0 == enable)
		cxt->is_active_nodata = false;

	pdr_real_enable(enable);
	return 0;
}


static ssize_t pdr_show_enable_nodata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PDR_LOG(" not support now\n");
	return len;
}

static ssize_t pdr_store_enable_nodata(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct pdr_context *cxt = NULL;
	/* int err =0; */

	PDR_LOG("pdr_store_enable nodata buf=%s\n", buf);
	mutex_lock(&pdr_context_obj->pdr_op_mutex);

	cxt = pdr_context_obj;
	if (NULL == cxt->pdr_ctl.enable_nodata) {
		PDR_LOG("pdr_ctl enable nodata NULL\n");
		mutex_unlock(&pdr_context_obj->pdr_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->pdr_ctl.enable_nodata(1); */
		pdr_enable_nodata(1);
	} else if (!strncmp(buf, "0", 1)) {
		/* cxt->pdr_ctl.enable_nodata(0); */
		pdr_enable_nodata(0);
	} else {
		PDR_ERR(" pdr_store enable nodata cmd error !!\n");
	}
	mutex_unlock(&pdr_context_obj->pdr_op_mutex);

	return 0;
}

static ssize_t pdr_store_active(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct pdr_context *cxt = NULL;
	/* int err =0; */

	/*PDR_ERR("pdr_store_active buf=%s\n", buf);*/
	mutex_lock(&pdr_context_obj->pdr_op_mutex);
	cxt = pdr_context_obj;
	if (NULL == cxt->pdr_ctl.open_report_data) {
		PDR_LOG("pdr_ctl enable NULL\n");
		mutex_unlock(&pdr_context_obj->pdr_op_mutex);
		return count;
	}
	if (!strncmp(buf, "1", 1)) {
		/* cxt->pdr_ctl.enable(1); */
		pdr_enable_data(1);

	} else if (!strncmp(buf, "0", 1)) {

		/* cxt->pdr_ctl.enable(0); */
		pdr_enable_data(0);
	} else {
		PDR_ERR(" pdr_store_active error !!\n");
	}
	mutex_unlock(&pdr_context_obj->pdr_op_mutex);
	PDR_LOG(" pdr_store_active done\n");
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t pdr_show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pdr_context *cxt = NULL;
	int div;

	cxt = pdr_context_obj;
	div = cxt->pdr_data.vender_div;

	PDR_LOG("pdr vender_div value: %d\n", div);
	return snprintf(buf, PAGE_SIZE, "%d\n", div);
}

static ssize_t pdr_store_delay(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	/* struct pdr_context *devobj = (struct pdr_context*)dev_get_drvdata(dev); */
	int delay = 0;
	int mdelay = 0;
	struct pdr_context *cxt = NULL;
	int err = 0;

	mutex_lock(&pdr_context_obj->pdr_op_mutex);
	cxt = pdr_context_obj;
	if (NULL == cxt->pdr_ctl.set_delay) {
		PDR_LOG("pdr_ctl set_delay NULL\n");
		mutex_unlock(&pdr_context_obj->pdr_op_mutex);
		return count;
	}

	err = kstrtoint(buf, 10, &delay);	/*ns */
	if (err != 0) {
		PDR_ERR("invalid format!!\n");
		mutex_unlock(&pdr_context_obj->pdr_op_mutex);
		return count;
	}

	if (false == cxt->pdr_ctl.is_report_input_direct) {
		mdelay = (int)delay / 1000 / 1000;
		atomic_set(&pdr_context_obj->delay, mdelay);
	}
	cxt->pdr_ctl.set_delay(delay);
	mutex_unlock(&pdr_context_obj->pdr_op_mutex);

	/*PDR_ERR("pdr_delay %d(ns) = %d(ms)\n", delay, mdelay);*/
	return count;
}

static ssize_t pdr_show_delay(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	PDR_LOG(" not support now\n");
	return len;
}

static ssize_t pdr_show_sensordevnum(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pdr_context *cxt = NULL;
	char *devname = NULL;
	struct input_handle *handle;

	cxt = pdr_context_obj;
	list_for_each_entry(handle, &cxt->idev->h_list, d_node)
		if (strncmp(handle->name, "event", 5) == 0) {
			devname = handle->name;
			break;
		}
	return snprintf(buf, PAGE_SIZE, "%s\n", devname + 5);
}


static ssize_t pdr_store_batch(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct pdr_context *cxt = NULL;

	/* int err =0; */
	PDR_LOG("pdr_store_batch buf=%s\n", buf);
	mutex_lock(&pdr_context_obj->pdr_op_mutex);
	cxt = pdr_context_obj;
	if (cxt->pdr_ctl.is_support_batch) {
		if (!strncmp(buf, "1", 1)) {
			cxt->is_batch_enable = true;
			/* MTK problem fix - start */
			if (cxt->is_active_data && cxt->is_polling_run) {
				cxt->is_polling_run = false;
				del_timer_sync(&cxt->timer);
				cancel_work_sync(&cxt->report);
			}
			/* MTK problem fix - end */
		} else if (!strncmp(buf, "0", 1)) {
			cxt->is_batch_enable = false;
			/* MTK problem fix - start */
			if (cxt->is_active_data)
				pdr_enable_data(true);
			/* MTK problem fix - end */
		} else {
			PDR_ERR(" pdr_store_batch error !!\n");
		}
	} else {
		PDR_LOG(" pdr_store_batch mot supported\n");
	}
	mutex_unlock(&pdr_context_obj->pdr_op_mutex);
	PDR_LOG(" pdr_store_batch done: %d\n", cxt->is_batch_enable);
	return count;

}

static ssize_t pdr_show_batch(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t pdr_store_flush(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	/* struct pdr_context *devobj = (struct pdr_context*)dev_get_drvdata(dev); */
	mutex_lock(&pdr_context_obj->pdr_op_mutex);
	/* do read FIFO data function and report data immediately */
	mutex_unlock(&pdr_context_obj->pdr_op_mutex);
	return count;
}

static ssize_t pdr_show_flush(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static int pdrsensor_remove(struct platform_device *pdev)
{
	PDR_LOG("pdrsensor_remove\n");
	return 0;
}

static int pdrsensor_probe(struct platform_device *pdev)
{
	PDR_LOG("pdrsensor_probe\n");
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id pdrsensor_of_match[] = {
	{.compatible = "mediatek,pdrsensor",},
	{},
};
#endif

static struct platform_driver pdrsensor_driver = {
	.probe = pdrsensor_probe,
	.remove = pdrsensor_remove,
	.driver = {
		   .name = "pdrsensor",
#ifdef CONFIG_OF
		   .of_match_table = pdrsensor_of_match,
#endif
		   }
};

static int pdr_real_driver_init(void)
{
	int i = 0;
	int err = 0;

	PDR_LOG(" pdr_real_driver_init +\n");
	for (i = 0; i < MAX_CHOOSE_PDR_NUM; i++) {
		PDR_LOG(" i=%d\n", i);
		if (0 != pdr_init_list[i]) {
			PDR_LOG(" pdr try to init driver %s\n", pdr_init_list[i]->name);
			err = pdr_init_list[i]->init();
			if (0 == err) {
				PDR_LOG(" pdr real driver %s probe ok\n", pdr_init_list[i]->name);
				break;
			}
		}
	}

	if (i == MAX_CHOOSE_PDR_NUM) {
		PDR_LOG(" pdr_real_driver_init fail\n");
		err = -1;
	}
	return err;
}

static int pdr_misc_init(struct pdr_context *cxt)
{

	int err = 0;

	cxt->mdev.minor = MISC_DYNAMIC_MINOR;
	cxt->mdev.name = PDR_MISC_DEV_NAME;

	err = misc_register(&cxt->mdev);
	if (err)
		PDR_ERR("unable to register pdr misc device!!\n");

	/* dev_set_drvdata(cxt->mdev.this_device, cxt); */
	return err;
}

static void pdr_input_destroy(struct pdr_context *cxt)
{
	struct input_dev *dev = cxt->idev;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int pdr_input_init(struct pdr_context *cxt)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = PDR_INPUTDEV_NAME;

	input_set_capability(dev, EV_ABS, EVENT_TYPE_PDR_X);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_PDR_Y);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_PDR_Z);
	input_set_capability(dev, EV_ABS, EVENT_TYPE_PDR_SCALAR);
	input_set_capability(dev, EV_REL, EVENT_TYPE_PDR_STATUS);

	input_set_abs_params(dev, EVENT_TYPE_PDR_X, PDR_VALUE_MIN, PDR_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_PDR_Y, PDR_VALUE_MIN, PDR_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_PDR_Z, PDR_VALUE_MIN, PDR_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, EVENT_TYPE_PDR_SCALAR, PDR_VALUE_MIN, PDR_VALUE_MAX, 0, 0);
	input_set_drvdata(dev, cxt);

	input_set_events_per_packet(dev, 32);	/* test */

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	cxt->idev = dev;

	return 0;
}

DEVICE_ATTR(pdrenablenodata, S_IWUSR | S_IRUGO, pdr_show_enable_nodata, pdr_store_enable_nodata);
DEVICE_ATTR(pdractive, S_IWUSR | S_IRUGO, pdr_show_active, pdr_store_active);
DEVICE_ATTR(pdrdelay, S_IWUSR | S_IRUGO, pdr_show_delay, pdr_store_delay);
DEVICE_ATTR(pdrbatch, S_IWUSR | S_IRUGO, pdr_show_batch, pdr_store_batch);
DEVICE_ATTR(pdrflush, S_IWUSR | S_IRUGO, pdr_show_flush, pdr_store_flush);
DEVICE_ATTR(pdrdevnum, S_IWUSR | S_IRUGO, pdr_show_sensordevnum, NULL);

static struct attribute *pdr_attributes[] = {
	&dev_attr_pdrenablenodata.attr,
	&dev_attr_pdractive.attr,
	&dev_attr_pdrdelay.attr,
	&dev_attr_pdrbatch.attr,
	&dev_attr_pdrflush.attr,
	&dev_attr_pdrdevnum.attr,
	NULL
};

static struct attribute_group pdr_attribute_group = {
	.attrs = pdr_attributes
};

int pdr_register_data_path(struct pdr_data_path *data)
{
	struct pdr_context *cxt = NULL;

	cxt = pdr_context_obj;
	cxt->pdr_data.get_data = data->get_data;
	cxt->pdr_data.vender_div = data->vender_div;
	PDR_LOG("pdr register data path vender_div: %d\n", cxt->pdr_data.vender_div);
	if (NULL == cxt->pdr_data.get_data) {
		PDR_LOG("pdr register data path fail\n");
		return -1;
	}
	return 0;
}

int pdr_register_control_path(struct pdr_control_path *ctl)
{
	struct pdr_context *cxt = NULL;
	int err = 0;

	cxt = pdr_context_obj;
	cxt->pdr_ctl.set_delay = ctl->set_delay;
	cxt->pdr_ctl.open_report_data = ctl->open_report_data;
	cxt->pdr_ctl.enable_nodata = ctl->enable_nodata;
	cxt->pdr_ctl.is_support_batch = ctl->is_support_batch;
	cxt->pdr_ctl.is_report_input_direct = ctl->is_report_input_direct;	/*reserve */

	if (NULL == cxt->pdr_ctl.set_delay || NULL == cxt->pdr_ctl.open_report_data
	    || NULL == cxt->pdr_ctl.enable_nodata) {
		PDR_LOG("pdr register control path fail\n");
		return -1;
	}
	/* add misc dev for sensor hal control cmd */
	err = pdr_misc_init(pdr_context_obj);
	if (err) {
		PDR_ERR("unable to register pdr misc device!!\n");
		return -2;
	}
	err = sysfs_create_group(&pdr_context_obj->mdev.this_device->kobj, &pdr_attribute_group);
	if (err < 0) {
		PDR_ERR("unable to create pdr attribute file\n");
		return -3;
	}

	kobject_uevent(&pdr_context_obj->mdev.this_device->kobj, KOBJ_ADD);

	return 0;
}

int pdr_data_report(int x, int y, int z, int scalar, int status)
{
	/* PDR_LOG("+pdr_data_report! %d, %d, %d, %d\n",x,y,z,status); */
	struct pdr_context *cxt = NULL;

	cxt = pdr_context_obj;
	input_report_abs(cxt->idev, EVENT_TYPE_PDR_X, x);
	input_report_abs(cxt->idev, EVENT_TYPE_PDR_Y, y);
	input_report_abs(cxt->idev, EVENT_TYPE_PDR_Z, z);
	input_report_abs(cxt->idev, EVENT_TYPE_PDR_SCALAR, scalar);
	/* input_report_rel(cxt->idev, EVENT_TYPE_PDR_STATUS, status); */
	input_sync(cxt->idev);
	return 0;
}

static int pdr_probe(void)
{
	int err = 0;

	/*PDR_ERR("[Bai]>> pdr_probe start==>!!\n"); */

	pdr_context_obj = pdr_context_alloc_object();
	if (!pdr_context_obj) {
		err = -ENOMEM;
		PDR_ERR("unable to allocate devobj!\n");
		goto exit_alloc_data_failed;
	}

	/* init real pdreleration driver */
	err = pdr_real_driver_init();
	if (err) {
		PDR_ERR("pdr real driver init fail\n");
		goto real_driver_init_fail;
	}

	/* init input dev */
	err = pdr_input_init(pdr_context_obj);
	if (err) {
		PDR_ERR("unable to register pdr input device!\n");
		goto exit_alloc_input_dev_failed;
	}

	PDR_ERR("[Bai]>> pdr_probe OK !!\n");
	return 0;

exit_alloc_input_dev_failed:
	PDR_ERR("[Bai]>> sysfs node creation error\n");
	pdr_input_destroy(pdr_context_obj);
real_driver_init_fail:
	kfree(pdr_context_obj);
exit_alloc_data_failed:
	PDR_ERR("[Bai]>> pdr_probe fail !!!\n");
	return err;
}

static int pdr_remove(void)
{
	int err = 0;

	PDR_FUN(f);
	input_unregister_device(pdr_context_obj->idev);
	sysfs_remove_group(&pdr_context_obj->idev->dev.kobj, &pdr_attribute_group);

	err = misc_deregister(&pdr_context_obj->mdev);
	if (err)
		PDR_ERR("misc_deregister fail: %d\n", err);

	kfree(pdr_context_obj);

	return 0;
}

#if 0
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_EARLYSUSPEND)
static void pdr_early_suspend(struct early_suspend *h)
{
	atomic_set(&(pdr_context_obj->early_suspend), 1);
	PDR_LOG(" pdr_early_suspend ok------->hwm_obj->early_suspend=%d\n",
		atomic_read(&(pdr_context_obj->early_suspend)));
}

/*----------------------------------------------------------------------------*/

static void pdr_late_resume(struct early_suspend *h)
{
	atomic_set(&(pdr_context_obj->early_suspend), 0);
	PDR_LOG(" pdr_late_resume ok------->hwm_obj->early_suspend=%d\n",
		atomic_read(&(pdr_context_obj->early_suspend)));
}
#endif
#endif
#if 0
static int pdr_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int pdr_resume(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id m_pdr_pl_of_match[] = {
	{.compatible = "mediatek,m_pdr_pl",},
	{},
};
#endif

static struct platform_driver pdr_driver = {
	.probe = pdr_probe,
	.remove = pdr_remove,
	.suspend = pdr_suspend,
	.resume = pdr_resume,
	.driver = {
		   .name = PDR_PL_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = m_pdr_pl_of_match,
#endif
		   }
};
#endif

int pdr_driver_add(struct pdr_init_info *obj)
{
	int err = 0;
	int i = 0;

	/*PDR_FUN(); */

	for (i = 0; i < MAX_CHOOSE_PDR_NUM; i++) {
		if ((i == 0) && (NULL == pdr_init_list[0])) {
			PDR_LOG("register gensor driver for the first time\n");
			if (platform_driver_register(&pdrsensor_driver))
				PDR_ERR("failed to register pdrsensor driver already exist\n");
			else
				PDR_ERR("[Bai]>> register pdrsensor driver OK\n");
		}

		if (NULL == pdr_init_list[i]) {
			obj->platform_diver_addr = &pdrsensor_driver;
			pdr_init_list[i] = obj;
			break;
		}
	}
	if (i >= MAX_CHOOSE_PDR_NUM) {
		PDR_ERR("PDR driver Full\n");
		err = -1;
	}
	return err;
}
EXPORT_SYMBOL_GPL(pdr_driver_add);

static int __init pdr_init(void)
{
	/*PDR_FUN(); */

	PDR_ERR("[Bai]>> pdr_init\n");

	if (pdr_probe()) {
		PDR_ERR("failed to register pdr driver\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit pdr_exit(void)
{
	pdr_remove();
	platform_driver_unregister(&pdrsensor_driver);
}

late_initcall(pdr_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PDR device driver");
MODULE_AUTHOR("Mediatek");
