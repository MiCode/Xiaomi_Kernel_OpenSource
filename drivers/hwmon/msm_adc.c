/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/msm_adc.h>
#include <linux/pmic8058-xoadc.h>
#include <linux/slab.h>
#include <linux/semaphore.h>

#include <mach/dal.h>

#define MSM_ADC_DRIVER_NAME		"msm_adc"
#define MSM_ADC_MAX_FNAME		15

#define MSM_ADC_DALRPC_DEVICEID		0x02000067
#define MSM_ADC_DALRPC_PORT_NAME	"DAL00"
#define MSM_ADC_DALRPC_CPU		SMD_APPS_MODEM

#define MSM_ADC_DALRPC_CMD_REQ_CONV	9
#define MSM_ADC_DALRPC_CMD_INPUT_PROP	11

#define MSM_ADC_DALRC_CONV_TIMEOUT	(5 * HZ)  /* 5 seconds */

enum dal_error {
	DAL_ERROR_INVALID_DEVICE_IDX = 1,
	DAL_ERROR_INVALID_CHANNEL_IDX,
	DAL_ERROR_NULL_POINTER,
	DAL_ERROR_DEVICE_QUEUE_FULL,
	DAL_ERROR_INVALID_PROPERTY_LENGTH,
	DAL_ERROR_REMOTE_EVENT_POOL_FULL
};

enum dal_result_status {
	DAL_RESULT_STATUS_INVALID,
	DAL_RESULT_STATUS_VALID
};

struct dal_conv_state {
	struct dal_conv_slot		context[MSM_ADC_DEV_MAX_INFLIGHT];
	struct list_head		slots;
	struct mutex			list_lock;
	struct semaphore		slot_count;
};

struct adc_dev {
	char				*name;
	uint32_t			nchans;
	struct dal_conv_state		conv;
	struct dal_translation		transl;
	struct sensor_device_attribute	*sens_attr;
	char				**fnames;
};

struct msm_adc_drv {
	/*  Common to both XOADC and EPM  */
	struct platform_device		*pdev;
	struct device			*hwmon;
	struct miscdevice		misc;
	/*  XOADC variables  */
	struct sensor_device_attribute	*sens_attr;
	struct workqueue_struct		*wq;
	atomic_t			online;
	atomic_t			total_outst;
	wait_queue_head_t		total_outst_wait;

	/*  EPM variables  */
	void				*dev_h;
	struct adc_dev			*devs[MSM_ADC_MAX_NUM_DEVS];
	struct mutex			prop_lock;
	atomic_t			rpc_online;
	atomic_t			rpc_total_outst;
	wait_queue_head_t		rpc_total_outst_wait;
};

static bool epm_init;
static bool epm_fluid_enabled;

/* Needed to support file_op interfaces */
static struct msm_adc_drv *msm_adc_drv;

static bool conv_first_request;

static ssize_t msm_adc_show_curr(struct device *dev,
				struct device_attribute *devattr, char *buf);

static int msm_rpc_adc_blocking_conversion(struct msm_adc_drv *msm_adc,
				uint32_t chan, struct adc_chan_result *result);

static int msm_adc_blocking_conversion(struct msm_adc_drv *msm_adc,
				uint32_t chan, struct adc_chan_result *result);

static int msm_adc_open(struct inode *inode, struct file *file)
{
	struct msm_client_data *client;
	struct msm_adc_drv *msm_adc = msm_adc_drv;
	struct platform_device *pdev = msm_adc->pdev;

	client = kzalloc(sizeof(struct msm_client_data), GFP_KERNEL);
	if (!client) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (!try_module_get(THIS_MODULE)) {
		kfree(client);
		return -EACCES;
	}

	mutex_init(&client->lock);
	INIT_LIST_HEAD(&client->complete_list);
	init_waitqueue_head(&client->data_wait);
	init_waitqueue_head(&client->outst_wait);

	client->online = 1;

	file->private_data = client;

	return nonseekable_open(inode, file);
}

static inline void msm_adc_restore_slot(struct dal_conv_state *conv_s,
					struct dal_conv_slot *slot)
{
	mutex_lock(&conv_s->list_lock);
	list_add(&slot->list, &conv_s->slots);
	mutex_unlock(&conv_s->list_lock);

	up(&conv_s->slot_count);
}

static int no_pending_client_requests(struct msm_client_data *client)
{
	mutex_lock(&client->lock);

	if (client->num_outstanding == 0) {
		mutex_unlock(&client->lock);
		return 1;
	}

	mutex_unlock(&client->lock);

	return 0;
}

static int data_avail(struct msm_client_data *client, uint32_t *pending)
{
	uint32_t completed;

	mutex_lock(&client->lock);
	completed = client->num_complete;
	mutex_unlock(&client->lock);

	if (completed > 0) {
		if (pending != NULL)
			*pending = completed;
		return 1;
	}

	return 0;
}

static int msm_adc_release(struct inode *inode, struct file *file)
{
	struct msm_client_data *client = file->private_data;
	struct adc_conv_slot *slot, *tmp;
	int rc;
	struct msm_adc_platform_data *pdata =
					msm_adc_drv->pdev->dev.platform_data;
	struct msm_adc_channels *channel = pdata->channel;

	module_put(THIS_MODULE);

	mutex_lock(&client->lock);

	/* prevent any further requests while we teardown the client */
	client->online = 0;

	mutex_unlock(&client->lock);

	/*
	 * We may still have outstanding transactions in flight from this
	 * client that have not completed. Make sure they're completed
	 * before removing the client.
	 */
	rc = wait_event_interruptible(client->outst_wait,
				      no_pending_client_requests(client));
	if (rc) {
		pr_err("%s: wait_event_interruptible failed rc = %d\n",
								__func__, rc);
		return rc;
	}

	/*
	 * All transactions have completed. Add slot resources back to the
	 * appropriate devices.
	 */
	list_for_each_entry_safe(slot, tmp, &client->complete_list, list) {
		slot->client = NULL;
		list_del(&slot->list);
		channel[slot->conv.result.chan].adc_access_fn->adc_restore_slot(
		channel[slot->conv.result.chan].adc_dev_instance, slot);
	}

	kfree(client);

	return 0;
}

static int msm_adc_translate_dal_to_hwmon(struct msm_adc_drv *msm_adc,
					  uint32_t chan,
					  struct adc_dev_spec *dest)
{
	struct dal_translation *transl;
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;
	int i;

	for (i = 0; i < pdata->num_adc; i++) {
		transl = &msm_adc->devs[i]->transl;
		if (chan >= transl->hwmon_start &&
		    chan <= transl->hwmon_end) {
			dest->dal.dev_idx = transl->dal_dev_idx;
			dest->hwmon_dev_idx = transl->hwmon_dev_idx;
			dest->dal.chan_idx = chan - transl->hwmon_start;
			return 0;
		}
	}
	return -EINVAL;
}

static int msm_adc_translate_hwmon_to_dal(struct msm_adc_drv *msm_adc,
					  struct adc_dev_spec *source,
					  uint32_t *chan)
{
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;
	struct dal_translation *transl;
	int i;

	for (i = 0; i < pdata->num_adc; i++) {
		transl = &msm_adc->devs[i]->transl;
		if (source->dal.dev_idx != transl->dal_dev_idx)
			continue;
		*chan = transl->hwmon_start + source->dal.chan_idx;
		return 0;
	}
	return -EINVAL;
}

static int msm_adc_getinputproperties(struct msm_adc_drv *msm_adc,
					  const char *lookup_name,
					  struct adc_dev_spec *result)
{
	struct device *dev = &msm_adc->pdev->dev;
	int rc;

	mutex_lock(&msm_adc->prop_lock);

	rc = dalrpc_fcn_8(MSM_ADC_DALRPC_CMD_INPUT_PROP, msm_adc->dev_h,
			  lookup_name, strlen(lookup_name) + 1,
			  &result->dal, sizeof(struct dal_dev_spec));
	if (rc) {
		dev_err(dev, "DAL getprop request failed: rc = %d\n", rc);
		mutex_unlock(&msm_adc->prop_lock);
		return -EIO;
	}

	mutex_unlock(&msm_adc->prop_lock);
	return rc;
}

static int msm_adc_lookup(struct msm_adc_drv *msm_adc,
			  struct msm_adc_lookup *lookup)
{
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;
	struct adc_dev_spec target;
	int rc = 0, i = 0;
	uint32_t len = 0;

	len = strnlen(lookup->name, MSM_ADC_MAX_CHAN_STR);
	while (i < pdata->num_chan_supported) {
		if (strncmp(lookup->name, pdata->channel[i].name, len))
			i++;
		else
			break;
	}

	if (pdata->num_chan_supported > 0 && i < pdata->num_chan_supported) {
		lookup->chan_idx = i;
	} else if (msm_adc->dev_h) {
		rc = msm_adc_getinputproperties(msm_adc, lookup->name, &target);
		if (rc) {
			pr_err("%s: Lookup failed for %s\n", __func__,
				lookup->name);
			return rc;
		}
		rc = msm_adc_translate_hwmon_to_dal(msm_adc, &target,
						&lookup->chan_idx);
		if (rc)
			pr_err("%s: Translation failed for %s\n", __func__,
						lookup->name);
	} else {
		pr_err("%s: Lookup failed for %s\n", __func__, lookup->name);
		rc = -EINVAL;
	}
	return rc;
}

static int msm_adc_aio_conversion(struct msm_adc_drv *msm_adc,
				  struct adc_chan_result *request,
				  struct msm_client_data *client)
{
	struct msm_adc_platform_data *pdata =
					msm_adc_drv->pdev->dev.platform_data;
	struct msm_adc_channels *channel = &pdata->channel[request->chan];
	struct adc_conv_slot *slot;

	/* we could block here, but only for a bounded time */
	channel->adc_access_fn->adc_slot_request(channel->adc_dev_instance,
									&slot);

	if (slot) {
		atomic_inc(&msm_adc->total_outst);
		mutex_lock(&client->lock);
		client->num_outstanding++;
		mutex_unlock(&client->lock);

		/* indicates non blocking request to callback handler */
		slot->blocking = 0;
		slot->compk = NULL;/*For kernel space usage; n/a for usr space*/
		slot->conv.result.chan = client->adc_chan = request->chan;
		slot->client = client;
		slot->adc_request = START_OF_CONV;
		slot->chan_path = channel->chan_path_type;
		slot->chan_adc_config = channel->adc_config_type;
		slot->chan_adc_calib = channel->adc_calib_type;
		queue_work(msm_adc->wq, &slot->work);
		return 0;
	}
	return -EBUSY;
}

static int msm_adc_fluid_hw_deinit(struct msm_adc_drv *msm_adc)
{
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;

	if (!epm_init)
		return -EINVAL;

	if (pdata->gpio_config == APROC_CONFIG &&
		epm_fluid_enabled && pdata->adc_fluid_disable != NULL) {
		pdata->adc_fluid_disable();
		epm_fluid_enabled = false;
	}

	return 0;
}

static int msm_adc_fluid_hw_init(struct msm_adc_drv *msm_adc)
{
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;

	if (!epm_init)
		return -EINVAL;

	if (!pdata->adc_fluid_enable)
		return -ENODEV;

	printk(KERN_DEBUG "msm_adc_fluid_hw_init: Calling adc_fluid_enable.\n");

	if (pdata->gpio_config == APROC_CONFIG && !epm_fluid_enabled) {
		pdata->adc_fluid_enable();
		epm_fluid_enabled = true;
	}

  /* return success for now but check for errors from hw init configuration */
	return 0;
}

static int msm_adc_poll_complete(struct msm_adc_drv *msm_adc,
			     struct msm_client_data *client, uint32_t *pending)
{
	int rc;

	/*
	 * Don't proceed if there there's nothing queued on this client.
	 * We could deadlock otherwise in a single threaded scenario.
	 */
	if (no_pending_client_requests(client) && !data_avail(client, pending))
		return -EDEADLK;

	rc = wait_event_interruptible(client->data_wait,
				data_avail(client, pending));
	if (rc)
		return rc;

	return 0;
}

static int msm_adc_read_result(struct msm_adc_drv *msm_adc,
			       struct msm_client_data *client,
			       struct adc_chan_result *result)
{
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;
	struct msm_adc_channels *channel = pdata->channel;
	struct adc_conv_slot *slot;
	int rc = 0;

	mutex_lock(&client->lock);

	slot = list_first_entry(&client->complete_list,
				struct adc_conv_slot, list);
	if (!slot) {
		mutex_unlock(&client->lock);
		return -ENOMSG;
	}

	slot->client = NULL;
	list_del(&slot->list);

	client->num_complete--;

	mutex_unlock(&client->lock);

	*result = slot->conv.result;

	/* restore this slot to reserve */
	channel[slot->conv.result.chan].adc_access_fn->adc_restore_slot(
		channel[slot->conv.result.chan].adc_dev_instance, slot);

	return rc;
}

static long msm_adc_ioctl(struct file *file, unsigned int cmd,
					     unsigned long arg)
{
	struct msm_client_data *client = file->private_data;
	struct msm_adc_drv *msm_adc = msm_adc_drv;
	struct platform_device *pdev = msm_adc->pdev;
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	uint32_t block_res = 0;

	int rc;

	switch (cmd) {
	case MSM_ADC_REQUEST:
		{
			struct adc_chan_result conv;

			if (copy_from_user(&conv, (void __user *)arg,
					sizeof(struct adc_chan_result)))
				return -EFAULT;

			if (conv.chan < pdata->num_chan_supported) {
				rc = msm_adc_blocking_conversion(msm_adc,
							conv.chan, &conv);
			} else {
				if (!msm_adc->dev_h)
					return -EAGAIN;

				rc = msm_rpc_adc_blocking_conversion(msm_adc,
							conv.chan, &conv);
			}
			if (rc) {
				dev_dbg(&pdev->dev, "BLK conversion failed\n");
				return rc;
			}

			if (copy_to_user((void __user *)arg, &conv,
					sizeof(struct adc_chan_result)))
				return -EFAULT;
			break;
		}
	case MSM_ADC_AIO_REQUEST_BLOCK_RES:
		block_res = 1;
	case MSM_ADC_AIO_REQUEST:
		{
			struct adc_chan_result conv;

			if (copy_from_user(&conv, (void __user *)arg,
					sizeof(struct adc_chan_result)))
				return -EFAULT;

			if (conv.chan >= pdata->num_chan_supported)
				return -EINVAL;

			rc = msm_adc_aio_conversion(msm_adc, &conv, client);
			if (rc) {
				dev_dbg(&pdev->dev, "AIO conversion failed\n");
				return rc;
			}
			if (copy_to_user((void __user *)arg, &conv,
					sizeof(struct adc_chan_result)))
				return -EFAULT;
			break;
		}
	case MSM_ADC_AIO_POLL:
		{
			uint32_t completed;

			rc = msm_adc_poll_complete(msm_adc, client, &completed);
			if (rc) {
				dev_dbg(&pdev->dev, "poll request failed\n");
				return rc;
			}

			if (copy_to_user((void __user *)arg, &completed,
					sizeof(uint32_t)))
				return -EFAULT;

			break;
		}
	case MSM_ADC_AIO_READ:
		{
			struct adc_chan_result result;

			rc = msm_adc_read_result(msm_adc, client, &result);
			if (rc) {
				dev_dbg(&pdev->dev, "read result failed\n");
				return rc;
			}

			if (copy_to_user((void __user *)arg, &result,
					sizeof(struct adc_chan_result)))
				return -EFAULT;
			break;
		}
	case MSM_ADC_LOOKUP:
		{
			struct msm_adc_lookup lookup;

			if (copy_from_user(&lookup, (void __user *)arg,
					sizeof(struct msm_adc_lookup)))
				return -EFAULT;

			rc = msm_adc_lookup(msm_adc, &lookup);
			if (rc) {
				dev_dbg(&pdev->dev, "No such channel: %s\n",
						lookup.name);
				return rc;
			}

			if (copy_to_user((void __user *)arg, &lookup,
					sizeof(struct msm_adc_lookup)))
				return -EFAULT;
			break;
		}
	case MSM_ADC_FLUID_INIT:
		{
			uint32_t result;

			result = msm_adc_fluid_hw_init(msm_adc);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))	{
				printk(KERN_ERR "MSM_ADC_FLUID_INIT: "
					"copy_to_user returned an error.\n");
				return -EFAULT;
			}
			printk(KERN_DEBUG "MSM_ADC_FLUID_INIT: Success.\n");
			break;
		}
	case MSM_ADC_FLUID_DEINIT:
		{
			uint32_t result;

			result = msm_adc_fluid_hw_deinit(msm_adc);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	default:
		return -EINVAL;
	}

	return 0;
}

const struct file_operations msm_adc_fops = {
	.open = msm_adc_open,
	.release = msm_adc_release,
	.unlocked_ioctl = msm_adc_ioctl,
};

static ssize_t msm_adc_show_curr(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct msm_adc_drv *msm_adc = dev_get_drvdata(dev);
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;
	struct adc_chan_result result;
	int rc;

#ifdef CONFIG_PMIC8058_XOADC
	rc = pm8058_xoadc_registered();
	if (rc <= 0)
		return -ENODEV;
#endif
	if (attr->index < pdata->num_chan_supported) {
		rc = msm_adc_blocking_conversion(msm_adc,
					attr->index, &result);
	} else {
		if (pdata->gpio_config == APROC_CONFIG && !epm_fluid_enabled
					&& pdata->adc_fluid_enable != NULL) {
			printk(KERN_DEBUG "This is to read ADC value for "
				"Fluid EPM and init. Do it only once.\n");
			pdata->adc_fluid_enable();
			epm_fluid_enabled = true;
		}
		rc = msm_rpc_adc_blocking_conversion(msm_adc,
					attr->index, &result);
	}
	if (rc)
		return 0;

	return sprintf(buf, "Result: %lld Raw: %d\n", result.physical,
		result.adc_code);
}

static int msm_rpc_adc_blocking_conversion(struct msm_adc_drv *msm_adc,
		uint32_t hwmon_chan, struct adc_chan_result *result)
{
	struct msm_adc_platform_data *pdata = msm_adc->pdev->dev.platform_data;
	struct dal_conv_request params;
	struct device *dev = &msm_adc->pdev->dev;
	struct adc_dev *adc_dev;
	struct dal_conv_state *conv_s;
	struct dal_conv_slot *slot;
	struct adc_dev_spec dest;
	int timeout, rc = 0;

	if (pdata->gpio_config == APROC_CONFIG &&
			pdata->adc_gpio_enable != NULL)
		pdata->adc_gpio_enable(hwmon_chan-pdata->num_chan_supported);

	rc = msm_adc_translate_dal_to_hwmon(msm_adc, hwmon_chan, &dest);
	if (rc) {
		dev_err(dev, "%s: translation from chan %u failed\n",
							__func__, hwmon_chan);
		if (pdata->gpio_config == APROC_CONFIG &&
				pdata->adc_gpio_disable != NULL)
			pdata->adc_gpio_disable(hwmon_chan
					-pdata->num_chan_supported);
		return -EINVAL;
	}

	adc_dev = msm_adc->devs[dest.hwmon_dev_idx];
	conv_s = &adc_dev->conv;

	down(&conv_s->slot_count);

	mutex_lock(&conv_s->list_lock);

	slot = list_first_entry(&conv_s->slots, struct dal_conv_slot, list);
	list_del(&slot->list);
	BUG_ON(!slot);

	mutex_unlock(&conv_s->list_lock);

	/* indicates blocking request to callback handler */
	slot->blocking = 1;

	params.target.dev_idx = dest.dal.dev_idx;
	params.target.chan_idx = dest.dal.chan_idx;
	params.cb_h = slot->cb_h;

	rc = dalrpc_fcn_8(MSM_ADC_DALRPC_CMD_REQ_CONV, msm_adc->dev_h,
			&params, sizeof(params), NULL, 0);
	if (rc) {
		dev_err(dev, "%s: Conversion for device = %u channel = %u"
			     " failed\n", __func__, params.target.dev_idx,
						    params.target.chan_idx);

		rc = -EIO;
		goto blk_conv_err;
	}

	timeout = wait_for_completion_interruptible_timeout(&slot->comp,
					      MSM_ADC_DALRC_CONV_TIMEOUT);
	if (timeout == 0) {
		dev_err(dev, "read for device = %u channel = %u timed out\n",
				params.target.dev_idx, params.target.chan_idx);
		rc = -ETIMEDOUT;
		goto blk_conv_err;
	} else if (timeout < 0) {
		rc = -EINTR;
		goto blk_conv_err;
	}

	result->physical = (int64_t)slot->result.physical;

	if (slot->result.status == DAL_RESULT_STATUS_INVALID)
		rc = -ENODATA;

blk_conv_err:
	if (pdata->gpio_config == APROC_CONFIG &&
			pdata->adc_gpio_disable != NULL)
		pdata->adc_gpio_disable(hwmon_chan-pdata->num_chan_supported);
	msm_adc_restore_slot(conv_s, slot);

	return rc;
}

static int msm_adc_blocking_conversion(struct msm_adc_drv *msm_adc,
			uint32_t hwmon_chan, struct adc_chan_result *result)
{
	struct adc_conv_slot *slot;
	struct msm_adc_platform_data *pdata =
					msm_adc_drv->pdev->dev.platform_data;
	struct msm_adc_channels *channel = &pdata->channel[hwmon_chan];
	int ret = 0;

	if (conv_first_request) {
		ret = pm8058_xoadc_calib_device(channel->adc_dev_instance);
		if (ret) {
			pr_err("pmic8058 xoadc calibration failed, retry\n");
			return ret;
		}
		conv_first_request = false;
	}

	channel->adc_access_fn->adc_slot_request(channel->adc_dev_instance,
									&slot);
	if (slot) {
		slot->conv.result.chan = hwmon_chan;
		/* indicates blocking request to callback handler */
		slot->blocking = 1;
		slot->adc_request = START_OF_CONV;
		slot->chan_path = channel->chan_path_type;
		slot->chan_adc_config = channel->adc_config_type;
		slot->chan_adc_calib = channel->adc_calib_type;
		queue_work(msm_adc_drv->wq, &slot->work);

		wait_for_completion_interruptible(&slot->comp);
		*result = slot->conv.result;
		channel->adc_access_fn->adc_restore_slot(
					channel->adc_dev_instance, slot);
		return 0;
	}
	return -EBUSY;
}

int32_t adc_channel_open(uint32_t channel, void **h)
{
	struct msm_client_data *client;
	struct msm_adc_drv *msm_adc = msm_adc_drv;
	struct msm_adc_platform_data *pdata;
	struct platform_device *pdev;
	int i = 0;

	if (!msm_adc_drv)
		return -EFAULT;

#ifdef CONFIG_PMIC8058_XOADC
	if (pm8058_xoadc_registered() <= 0)
		return -ENODEV;
#endif
	pdata = msm_adc->pdev->dev.platform_data;
	pdev = msm_adc->pdev;

	while (i < pdata->num_chan_supported) {
		if (channel == pdata->channel[i].channel_name)
			break;
		else
			i++;
	}

	if (i == pdata->num_chan_supported)
		return -EBADF; /* unknown channel */

	client = kzalloc(sizeof(struct msm_client_data), GFP_KERNEL);
	if (!client) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (!try_module_get(THIS_MODULE)) {
		kfree(client);
		return -EACCES;
	}

	mutex_init(&client->lock);
	INIT_LIST_HEAD(&client->complete_list);
	init_waitqueue_head(&client->data_wait);
	init_waitqueue_head(&client->outst_wait);

	client->online = 1;
	client->adc_chan = i;
	*h = (void *)client;
	return 0;
}

int32_t adc_channel_close(void *h)
{
	struct msm_client_data *client = (struct msm_client_data *)h;

	kfree(client);
	return 0;
}

int32_t adc_channel_request_conv(void *h, struct completion *conv_complete_evt)
{
	struct msm_client_data *client = (struct msm_client_data *)h;
	struct msm_adc_platform_data *pdata =
					msm_adc_drv->pdev->dev.platform_data;
	struct msm_adc_channels *channel = &pdata->channel[client->adc_chan];
	struct adc_conv_slot *slot;
	int ret;

	if (conv_first_request) {
		ret = pm8058_xoadc_calib_device(channel->adc_dev_instance);
		if (ret) {
			pr_err("pmic8058 xoadc calibration failed, retry\n");
			return ret;
		}
		conv_first_request = false;
	}

	channel->adc_access_fn->adc_slot_request(channel->adc_dev_instance,
									&slot);

	if (slot) {
		atomic_inc(&msm_adc_drv->total_outst);
		mutex_lock(&client->lock);
		client->num_outstanding++;
		mutex_unlock(&client->lock);

		slot->conv.result.chan = client->adc_chan;
		slot->blocking = 0;
		slot->compk = conv_complete_evt;
		slot->client = client;
		slot->adc_request = START_OF_CONV;
		slot->chan_path = channel->chan_path_type;
		slot->chan_adc_config = channel->adc_config_type;
		slot->chan_adc_calib = channel->adc_calib_type;
		queue_work(msm_adc_drv->wq, &slot->work);
		return 0;
	}
	return -EBUSY;
}

int32_t adc_channel_read_result(void *h, struct adc_chan_result *chan_result)
{
	struct msm_client_data *client = (struct msm_client_data *)h;
	struct msm_adc_platform_data *pdata =
					msm_adc_drv->pdev->dev.platform_data;
	struct msm_adc_channels *channel = pdata->channel;
	struct adc_conv_slot *slot;
	int rc = 0;

	mutex_lock(&client->lock);

	slot = list_first_entry(&client->complete_list,
				struct adc_conv_slot, list);
	if (!slot) {
		mutex_unlock(&client->lock);
		return -ENOMSG;
	}

	slot->client = NULL;
	list_del(&slot->list);

	client->num_complete--;

	mutex_unlock(&client->lock);

	*chan_result = slot->conv.result;

	/* restore this slot to reserve */
	channel[slot->conv.result.chan].adc_access_fn->adc_restore_slot(
		channel[slot->conv.result.chan].adc_dev_instance, slot);

	return rc;
}

static void msm_rpc_adc_conv_cb(void *context, u32 param,
			    void *evt_buf, u32 len)
{
	struct dal_adc_result *result = evt_buf;
	struct dal_conv_slot *slot = context;
	struct msm_adc_drv *msm_adc = msm_adc_drv;

	memcpy(&slot->result, result, sizeof(slot->result));

	/* for blocking requests, signal complete */
	if (slot->blocking)
		complete(&slot->comp);

	/* for non-blocking requests, add slot to the client completed list */
	else {
		struct msm_client_data *client = slot->client;

		mutex_lock(&client->lock);

		list_add(&slot->list, &client->complete_list);
		client->num_complete++;
		client->num_outstanding--;

		/*
		 * if the client release has been invoked and this is call
		 * corresponds to the last request, then signal release
		 * to complete.
		 */
		if (slot->client->online == 0 && client->num_outstanding == 0)
			wake_up_interruptible_all(&client->outst_wait);

		mutex_unlock(&client->lock);

		wake_up_interruptible_all(&client->data_wait);

		atomic_dec(&msm_adc->total_outst);

		/* verify driver remove has not been invoked */
		if (atomic_read(&msm_adc->online) == 0 &&
				atomic_read(&msm_adc->total_outst) == 0)
			wake_up_interruptible_all(&msm_adc->total_outst_wait);
	}
}

void msm_adc_conv_cb(void *context, u32 param,
			    void *evt_buf, u32 len)
{
	struct adc_conv_slot *slot = context;
	struct msm_adc_drv *msm_adc = msm_adc_drv;

	switch (slot->adc_request) {
	case START_OF_CONV:
		slot->adc_request = END_OF_CONV;
	break;
	case START_OF_CALIBRATION:
		slot->adc_request = END_OF_CALIBRATION;
	break;
	case END_OF_CALIBRATION:
	case END_OF_CONV:
	break;
	}
	queue_work(msm_adc->wq, &slot->work);
}

static void msm_adc_teardown_device_conv(struct platform_device *pdev,
				    struct adc_dev *adc_dev)
{
	struct dal_conv_state *conv_s = &adc_dev->conv;
	struct msm_adc_drv *msm_adc = platform_get_drvdata(pdev);
	struct dal_conv_slot *slot;
	int i;

	for (i = 0; i < MSM_ADC_DEV_MAX_INFLIGHT; i++) {
		slot = &conv_s->context[i];
		if (slot->cb_h) {
			dalrpc_dealloc_cb(msm_adc->dev_h, slot->cb_h);
			slot->cb_h = NULL;
		}
	}
}

static void msm_rpc_adc_teardown_device(struct platform_device *pdev,
				    struct adc_dev *adc_dev)
{
	struct dal_translation *transl = &adc_dev->transl;
	int i, num_chans = transl->hwmon_end - transl->hwmon_start + 1;

	if (adc_dev->sens_attr)
		for (i = 0; i < num_chans; i++)
			device_remove_file(&pdev->dev,
					&adc_dev->sens_attr[i].dev_attr);

	msm_adc_teardown_device_conv(pdev, adc_dev);

	kfree(adc_dev->fnames);
	kfree(adc_dev->sens_attr);
	kfree(adc_dev);
}

static void msm_rpc_adc_teardown_devices(struct platform_device *pdev)
{
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	struct msm_adc_drv *msm_adc = platform_get_drvdata(pdev);
	int i, rc = 0;

	for (i = 0; i < pdata->num_adc; i++) {
		if (msm_adc->devs[i]) {
			msm_rpc_adc_teardown_device(pdev, msm_adc->devs[i]);
			msm_adc->devs[i] = NULL;
		} else
			break;
	}

	if (msm_adc->dev_h) {
		rc = daldevice_detach(msm_adc->dev_h);
		if (rc)
			dev_err(&pdev->dev, "Cannot detach from dal device\n");
		msm_adc->dev_h = NULL;
	}

}

static void msm_adc_teardown_device(struct platform_device *pdev,
				    struct msm_adc_drv *msm_adc)
{
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	int i, num_chans = pdata->num_chan_supported;

	if (pdata->num_chan_supported > 0) {
		if (msm_adc->sens_attr)
			for (i = 0; i < num_chans; i++)
				device_remove_file(&pdev->dev,
					&msm_adc->sens_attr[i].dev_attr);
		kfree(msm_adc->sens_attr);
	}
}

static void msm_adc_teardown(struct platform_device *pdev)
{
	struct msm_adc_drv *msm_adc = platform_get_drvdata(pdev);

	if (!msm_adc)
		return;

	misc_deregister(&msm_adc->misc);

	if (msm_adc->hwmon)
		hwmon_device_unregister(msm_adc->hwmon);

	msm_rpc_adc_teardown_devices(pdev);
	msm_adc_teardown_device(pdev, msm_adc);

	kfree(msm_adc);
	platform_set_drvdata(pdev, NULL);
}

static int __devinit msm_adc_device_conv_init(struct msm_adc_drv *msm_adc,
					      struct adc_dev *adc_dev)
{
	struct platform_device *pdev = msm_adc->pdev;
	struct dal_conv_state *conv_s = &adc_dev->conv;
	struct dal_conv_slot *slot = conv_s->context;
	int rc, i;

	sema_init(&conv_s->slot_count, MSM_ADC_DEV_MAX_INFLIGHT);
	mutex_init(&conv_s->list_lock);
	INIT_LIST_HEAD(&conv_s->slots);

	for (i = 0; i < MSM_ADC_DEV_MAX_INFLIGHT; i++) {
		list_add(&slot->list, &conv_s->slots);
		slot->cb_h = dalrpc_alloc_cb(msm_adc->dev_h,
					     msm_rpc_adc_conv_cb, slot);
		if (!slot->cb_h) {
			dev_err(&pdev->dev, "Unable to allocate DAL callback"
							" for slot %d\n", i);
			rc = -ENOMEM;
			goto dal_err_cb;
		}
		init_completion(&slot->comp);
		slot->idx = i;
		slot++;
	}

	return 0;

dal_err_cb:
	msm_adc_teardown_device_conv(pdev, adc_dev);

	return rc;
}

static struct sensor_device_attribute msm_rpc_adc_curr_in_attr =
	SENSOR_ATTR(NULL, S_IRUGO, msm_adc_show_curr, NULL, 0);

static int __devinit msm_rpc_adc_device_init_hwmon(struct platform_device *pdev,
						struct adc_dev *adc_dev)
{
	struct dal_translation *transl = &adc_dev->transl;
	int i, rc, num_chans = transl->hwmon_end - transl->hwmon_start + 1;
	const char prefix[] = "curr", postfix[] = "_input";
	char tmpbuf[5];

	adc_dev->fnames = kzalloc(num_chans * MSM_ADC_MAX_FNAME +
				  num_chans * sizeof(char *), GFP_KERNEL);
	if (!adc_dev->fnames) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_dev->sens_attr = kzalloc(num_chans *
			    sizeof(struct sensor_device_attribute), GFP_KERNEL);
	if (!adc_dev->sens_attr) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		rc = -ENOMEM;
		goto hwmon_err_fnames;
	}

	for (i = 0; i < num_chans; i++) {
		adc_dev->fnames[i] = (char *)adc_dev->fnames +
			i * MSM_ADC_MAX_FNAME + num_chans * sizeof(char *);
		strcpy(adc_dev->fnames[i], prefix);
		sprintf(tmpbuf, "%d", transl->hwmon_start + i);
		strcat(adc_dev->fnames[i], tmpbuf);
		strcat(adc_dev->fnames[i], postfix);

		msm_rpc_adc_curr_in_attr.index = transl->hwmon_start + i;
		msm_rpc_adc_curr_in_attr.dev_attr.attr.name =
					adc_dev->fnames[i];
		memcpy(&adc_dev->sens_attr[i], &msm_rpc_adc_curr_in_attr,
					sizeof(msm_rpc_adc_curr_in_attr));

		rc = device_create_file(&pdev->dev,
				&adc_dev->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&pdev->dev, "device_create_file failed for "
						"dal dev %u chan %d\n",
					    adc_dev->transl.dal_dev_idx, i);
			goto hwmon_err_sens;
		}
	}

	return 0;

hwmon_err_sens:
	kfree(adc_dev->sens_attr);
hwmon_err_fnames:
	kfree(adc_dev->fnames);

	return rc;
}

static int __devinit msm_rpc_adc_device_init(struct platform_device *pdev)
{
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	struct msm_adc_drv *msm_adc = platform_get_drvdata(pdev);
	struct adc_dev *adc_dev;
	struct adc_dev_spec target;
	int i, rc;
	int hwmon_cntr = pdata->num_chan_supported;

	for (i = 0; i < pdata->num_adc; i++) {
		adc_dev = kzalloc(sizeof(struct adc_dev), GFP_KERNEL);
		if (!adc_dev) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto dev_init_err;
		}

		msm_adc->devs[i] = adc_dev;
		adc_dev->name = pdata->dev_names[i];

		rc = msm_adc_device_conv_init(msm_adc, adc_dev);
		if (rc) {
			dev_err(&pdev->dev, "DAL device[%s] failed conv init\n",
							adc_dev->name);
			goto dev_init_err;
		}

		/* DAL device lookup */
		rc = msm_adc_getinputproperties(msm_adc, adc_dev->name,
								&target);
		if (rc) {
			dev_err(&pdev->dev, "No such DAL device[%s]\n",
							adc_dev->name);
			goto dev_init_err;
		}

		adc_dev->transl.dal_dev_idx = target.dal.dev_idx;
		adc_dev->transl.hwmon_dev_idx = i;
		adc_dev->nchans = target.dal.chan_idx;
		adc_dev->transl.hwmon_start = hwmon_cntr;
		adc_dev->transl.hwmon_end = hwmon_cntr + adc_dev->nchans - 1;
		hwmon_cntr += adc_dev->nchans;

		rc = msm_rpc_adc_device_init_hwmon(pdev, adc_dev);
		if (rc)
			goto dev_init_err;
	}

	return 0;

dev_init_err:
	msm_rpc_adc_teardown_devices(pdev);
	return rc;
}

static int __devinit msm_rpc_adc_init(struct platform_device *pdev1)
{
	struct msm_adc_drv *msm_adc = msm_adc_drv;
	struct platform_device *pdev = msm_adc->pdev;
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	int rc = 0;

	dev_dbg(&pdev->dev, "msm_rpc_adc_init called\n");

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -EINVAL;
	}

	mutex_init(&msm_adc->prop_lock);

	rc = daldevice_attach(MSM_ADC_DALRPC_DEVICEID,
			MSM_ADC_DALRPC_PORT_NAME,
			MSM_ADC_DALRPC_CPU,
			&msm_adc->dev_h);
	if (rc) {
		dev_err(&pdev->dev, "Cannot attach to dal device\n");
		return rc;
	}

	dev_dbg(&pdev->dev, "Attach to dal device Succeeded\n");

	rc = msm_rpc_adc_device_init(pdev);
	if (rc) {
		dev_err(&pdev->dev, "msm_adc_dev_init failed\n");
		goto err_cleanup;
	}

	init_waitqueue_head(&msm_adc->rpc_total_outst_wait);
	atomic_set(&msm_adc->rpc_online, 1);
	atomic_set(&msm_adc->rpc_total_outst, 0);
	epm_init = true;
	pr_info("msm_adc successfully registered\n");

	return 0;

err_cleanup:
	msm_rpc_adc_teardown_devices(pdev);

	return rc;
}

/*
 * Process the deferred job
 */
void msm_adc_wq_work(struct work_struct *work)
{
	struct adc_properties *adc_properties;
	struct adc_conv_slot *slot = container_of(work,
						struct adc_conv_slot, work);
	uint32_t idx = slot->conv.result.chan;
	struct msm_adc_platform_data *pdata =
					msm_adc_drv->pdev->dev.platform_data;
	struct msm_adc_channels *channel = &pdata->channel[idx];
	int32_t adc_code;

	switch (slot->adc_request) {
	case START_OF_CONV:
			channel->adc_access_fn->adc_select_chan_and_start_conv(
					channel->adc_dev_instance, slot);
	break;
	case END_OF_CONV:
		adc_properties = channel->adc_access_fn->adc_get_properties(
						channel->adc_dev_instance);
		if (channel->adc_access_fn->adc_read_adc_code)
			channel->adc_access_fn->adc_read_adc_code(
					channel->adc_dev_instance, &adc_code);
		if (channel->chan_processor)
			channel->chan_processor(adc_code, adc_properties,
				&slot->chan_properties, &slot->conv.result);
		/* Intentionally a fall thru here.  Calibraton does not need
		to perform channel processing, etc.  However, both
		end of conversion and end of calibration requires the below
		fall thru code to be executed. */
	case END_OF_CALIBRATION:
		/* for blocking requests, signal complete */
		if (slot->blocking)
			complete(&slot->comp);
		else {
			struct msm_client_data *client = slot->client;

			mutex_lock(&client->lock);

			if (slot->adc_request == END_OF_CONV) {
				list_add(&slot->list, &client->complete_list);
				client->num_complete++;
			}
			client->num_outstanding--;

		/*
		 * if the client release has been invoked and this is call
		 * corresponds to the last request, then signal release
		 * to complete.
		 */
			if (slot->client->online == 0 &&
						client->num_outstanding == 0)
				wake_up_interruptible_all(&client->outst_wait);

			mutex_unlock(&client->lock);

			wake_up_interruptible_all(&client->data_wait);

			atomic_dec(&msm_adc_drv->total_outst);

			/* verify driver remove has not been invoked */
			if (atomic_read(&msm_adc_drv->online) == 0 &&
				atomic_read(&msm_adc_drv->total_outst) == 0)
				wake_up_interruptible_all(
					&msm_adc_drv->total_outst_wait);

			if (slot->compk) /* Kernel space request */
				complete(slot->compk);
			if (slot->adc_request == END_OF_CALIBRATION)
				channel->adc_access_fn->adc_restore_slot(
					channel->adc_dev_instance, slot);
		}
	break;
	case START_OF_CALIBRATION: /* code here to please code reviewers
					to satisfy silly compiler warnings */
	break;
	}
}

static struct sensor_device_attribute msm_adc_curr_in_attr =
	SENSOR_ATTR(NULL, S_IRUGO, msm_adc_show_curr, NULL, 0);

static int __devinit msm_adc_init_hwmon(struct platform_device *pdev,
					       struct msm_adc_drv *msm_adc)
{
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	struct msm_adc_channels *channel = pdata->channel;
	int i, rc, num_chans = pdata->num_chan_supported;

	if (!channel)
		return -EINVAL;

	msm_adc->sens_attr = kzalloc(num_chans *
			    sizeof(struct sensor_device_attribute), GFP_KERNEL);
	if (!msm_adc->sens_attr) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		rc = -ENOMEM;
		goto hwmon_err_sens;
	}

	for (i = 0; i < num_chans; i++) {
		msm_adc_curr_in_attr.index = i;
		msm_adc_curr_in_attr.dev_attr.attr.name = channel[i].name;
		memcpy(&msm_adc->sens_attr[i], &msm_adc_curr_in_attr,
						sizeof(msm_adc_curr_in_attr));

		rc = device_create_file(&pdev->dev,
				&msm_adc->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&pdev->dev, "device_create_file failed for "
					    "dal dev %s\n",
					    channel[i].name);
			goto hwmon_err_sens;
		}
	}

	return 0;

hwmon_err_sens:
	kfree(msm_adc->sens_attr);

	return rc;
}

static struct platform_driver msm_adc_rpcrouter_remote_driver = {
	.probe          = msm_rpc_adc_init,
	.driver         = {
		.name   = MSM_ADC_DALRPC_PORT_NAME,
		.owner  = THIS_MODULE,
	},
};

static int msm_adc_probe(struct platform_device *pdev)
{
	struct msm_adc_platform_data *pdata = pdev->dev.platform_data;
	struct msm_adc_drv *msm_adc;
	int rc = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -EINVAL;
	}

	msm_adc = kzalloc(sizeof(struct msm_adc_drv), GFP_KERNEL);
	if (!msm_adc) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, msm_adc);
	msm_adc_drv = msm_adc;
	msm_adc->pdev = pdev;

	if (pdata->target_hw == MSM_8x60 || pdata->target_hw == FSM_9xxx) {
		rc = msm_adc_init_hwmon(pdev, msm_adc);
		if (rc) {
			dev_err(&pdev->dev, "msm_adc_dev_init failed\n");
			goto err_cleanup;
		}
	}

	msm_adc->hwmon = hwmon_device_register(&pdev->dev);
	if (IS_ERR(msm_adc->hwmon)) {
		dev_err(&pdev->dev, "hwmon_device_register failed\n");
		rc = PTR_ERR(msm_adc->hwmon);
		goto err_cleanup;
	}

	msm_adc->misc.name = MSM_ADC_DRIVER_NAME;
	msm_adc->misc.minor = MISC_DYNAMIC_MINOR;
	msm_adc->misc.fops = &msm_adc_fops;

	if (misc_register(&msm_adc->misc)) {
		dev_err(&pdev->dev, "Unable to register misc device!\n");
		goto err_cleanup;
	}

	init_waitqueue_head(&msm_adc->total_outst_wait);
	atomic_set(&msm_adc->online, 1);
	atomic_set(&msm_adc->total_outst, 0);

	msm_adc->wq = create_singlethread_workqueue("msm_adc");
	if (!msm_adc->wq)
		goto err_cleanup;

	if (pdata->num_adc > 0) {
		if (pdata->target_hw == MSM_8x60)
			platform_driver_register(
				&msm_adc_rpcrouter_remote_driver);
		else
			msm_rpc_adc_init(pdev);
	}
	conv_first_request = true;

	pr_info("msm_adc successfully registered\n");

	return 0;

err_cleanup:
	msm_adc_teardown(pdev);

	return rc;
}

static int __devexit msm_adc_remove(struct platform_device *pdev)
{
	int rc;

	struct msm_adc_drv *msm_adc = platform_get_drvdata(pdev);

	atomic_set(&msm_adc->online, 0);

	atomic_set(&msm_adc->rpc_online, 0);

	misc_deregister(&msm_adc->misc);

	hwmon_device_unregister(msm_adc->hwmon);
	msm_adc->hwmon = NULL;

	/*
	 * We may still have outstanding transactions in flight that have not
	 * completed. Make sure they're completed before tearing down.
	 */
	rc = wait_event_interruptible(msm_adc->total_outst_wait,
				      atomic_read(&msm_adc->total_outst) == 0);
	if (rc) {
		pr_err("%s: wait_event_interruptible failed rc = %d\n",
								__func__, rc);
		return rc;
	}

	rc = wait_event_interruptible(msm_adc->rpc_total_outst_wait,
	      atomic_read(&msm_adc->rpc_total_outst) == 0);
	if (rc) {
		pr_err("%s: wait_event_interruptible failed rc = %d\n",
								__func__, rc);
		return rc;
	}

	msm_adc_teardown(pdev);

	pr_info("msm_adc unregistered\n");

	return 0;
}

static struct platform_driver msm_adc_driver = {
	.probe = msm_adc_probe,
	.remove = __devexit_p(msm_adc_remove),
	.driver = {
		.name = MSM_ADC_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_adc_init(void)
{
	return platform_driver_register(&msm_adc_driver);
}
module_init(msm_adc_init);

static void __exit msm_adc_exit(void)
{
	platform_driver_unregister(&msm_adc_driver);
}
module_exit(msm_adc_exit);

MODULE_DESCRIPTION("MSM ADC Driver");
MODULE_ALIAS("platform:msm_adc");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
