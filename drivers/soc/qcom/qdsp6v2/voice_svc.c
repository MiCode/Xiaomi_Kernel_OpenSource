/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/qdsp6v2/apr_tal.h>
#include <linux/qdsp6v2/apr.h>
#include <sound/voice_svc.h>

#define MINOR_NUMBER 1
#define APR_MAX_RESPONSE 10
#define TIMEOUT_MS 1000

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

struct voice_svc_device {
	struct cdev *cdev;
	struct device *dev;
	int major;
};

struct voice_svc_prvt {
	void *apr_q6_mvm;
	void *apr_q6_cvs;
	uint16_t response_count;
	struct list_head response_queue;
	wait_queue_head_t response_wait;
	spinlock_t response_lock;
	/*
	 * This mutex ensures responses are processed in sequential order and
	 * that no two threads access and free the same response at the same
	 * time.
	 */
	struct mutex response_mutex_lock;
};

struct apr_data {
	struct apr_hdr hdr;
	__u8 payload[0];
} __packed;

struct apr_response_list {
	struct list_head list;
	struct voice_svc_cmd_response resp;
};

static struct voice_svc_device *voice_svc_dev;
static struct class *voice_svc_class;
static bool reg_dummy_sess;
static void *dummy_q6_mvm;
static void *dummy_q6_cvs;
dev_t device_num;

static spinlock_t voicesvc_lock;
static bool is_released;
static int voice_svc_dummy_reg(void);
static int voice_svc_dummy_dereg(void);

static int32_t qdsp_dummy_apr_callback(struct apr_client_data *data,
					void *priv);

static int32_t qdsp_apr_callback(struct apr_client_data *data, void *priv)
{
	struct voice_svc_prvt *prtd;
	struct apr_response_list *response_list;
	unsigned long spin_flags;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);

		return -EINVAL;
	}
	spin_lock(&voicesvc_lock);
	if (is_released) {
		spin_unlock(&voicesvc_lock);
		return 0;
	}

	prtd = (struct voice_svc_prvt *)priv;
	if (prtd == NULL) {
		pr_err("%s: private data is NULL\n", __func__);
		spin_unlock(&voicesvc_lock);

		return -EINVAL;
	}

	pr_debug("%s: data->opcode %x\n", __func__,
		 data->opcode);

	if (data->opcode == RESET_EVENTS) {
		if (data->reset_proc == APR_DEST_QDSP6) {
			pr_debug("%s: Received ADSP reset event\n", __func__);

			if (prtd->apr_q6_mvm != NULL) {
				apr_reset(prtd->apr_q6_mvm);
				prtd->apr_q6_mvm = NULL;
			}

			if (prtd->apr_q6_cvs != NULL) {
				apr_reset(prtd->apr_q6_cvs);
				prtd->apr_q6_cvs = NULL;
			}
		} else if (data->reset_proc == APR_DEST_MODEM) {
			pr_debug("%s: Received Modem reset event\n", __func__);
		}
		/* Set the remaining member variables to default values
			for RESET_EVENTS */
		data->payload_size = 0;
		data->payload = NULL;
		data->src_port = 0;
		data->dest_port = 0;
		data->token = 0;
	}

	spin_lock_irqsave(&prtd->response_lock, spin_flags);

	if (prtd->response_count < APR_MAX_RESPONSE) {
		response_list = kmalloc(sizeof(struct apr_response_list) +
					data->payload_size, GFP_ATOMIC);
		if (response_list == NULL) {
			pr_err("%s: kmalloc failed\n", __func__);

			spin_unlock_irqrestore(&prtd->response_lock,
					       spin_flags);
			spin_unlock(&voicesvc_lock);
			return -ENOMEM;
		}

		response_list->resp.src_port = data->src_port;

		/* Reverting the bit manipulation done in voice_svc_update_hdr
		 * to the src_port which is returned to us as dest_port.
		 */
		response_list->resp.dest_port = ((data->dest_port) >> 8);
		response_list->resp.token = data->token;
		response_list->resp.opcode = data->opcode;
		response_list->resp.payload_size = data->payload_size;
		if (data->payload != NULL && data->payload_size > 0) {
			memcpy(response_list->resp.payload, data->payload,
			       data->payload_size);
		}

		list_add_tail(&response_list->list, &prtd->response_queue);
		prtd->response_count++;
		spin_unlock_irqrestore(&prtd->response_lock, spin_flags);

		wake_up(&prtd->response_wait);
	} else {
		spin_unlock_irqrestore(&prtd->response_lock, spin_flags);
		pr_err("%s: Response dropped since the queue is full\n",
		       __func__);
	}

	spin_unlock(&voicesvc_lock);
	return 0;
}

static int32_t qdsp_dummy_apr_callback(struct apr_client_data *data, void *priv)
{
	/* Do Nothing */
	return 0;
}

static void voice_svc_update_hdr(struct voice_svc_cmd_request *apr_req_data,
				 struct apr_data *aprdata)
{

	aprdata->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				       APR_HDR_LEN(sizeof(struct apr_hdr)),
				       APR_PKT_VER);
	/* Bit manipulation is done on src_port so that a unique ID is sent.
	 * This manipulation can be used in the future where the same service
	 * is tried to open multiple times with the same src_port. At that
	 * time 0x0001 can be replaced with other values depending on the
	 * count.
	 */
	aprdata->hdr.src_port = ((apr_req_data->src_port) << 8 | 0x0001);
	aprdata->hdr.dest_port = apr_req_data->dest_port;
	aprdata->hdr.token = apr_req_data->token;
	aprdata->hdr.opcode = apr_req_data->opcode;
	aprdata->hdr.pkt_size  = APR_PKT_SIZE(APR_HDR_SIZE,
					apr_req_data->payload_size);
	memcpy(aprdata->payload, apr_req_data->payload,
	       apr_req_data->payload_size);
}

static int voice_svc_send_req(struct voice_svc_cmd_request *apr_request,
			      struct voice_svc_prvt *prtd)
{
	int ret = 0;
	void *apr_handle = NULL;
	struct apr_data *aprdata = NULL;
	uint32_t user_payload_size;
	uint32_t payload_size;

	pr_debug("%s\n", __func__);

	if (apr_request == NULL) {
		pr_err("%s: apr_request is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	user_payload_size = apr_request->payload_size;
	payload_size = sizeof(struct apr_data) + user_payload_size;

	if (payload_size <= user_payload_size) {
		pr_err("%s: invalid payload size ( 0x%x ).\n",
			__func__, user_payload_size);
		ret = -EINVAL;
		goto done;
	} else {
		aprdata = kmalloc(payload_size, GFP_KERNEL);
		if (aprdata == NULL) {
			ret = -ENOMEM;
			goto done;
		}
	}

	voice_svc_update_hdr(apr_request, aprdata);

	if (!strcmp(apr_request->svc_name, VOICE_SVC_CVS_STR)) {
		apr_handle = prtd->apr_q6_cvs;
	} else if (!strcmp(apr_request->svc_name, VOICE_SVC_MVM_STR)) {
		apr_handle = prtd->apr_q6_mvm;
	} else {
		pr_err("%s: Invalid service %.*s\n", __func__,
			MAX_APR_SERVICE_NAME_LEN, apr_request->svc_name);

		ret = -EINVAL;
		goto done;
	}

	ret = apr_send_pkt(apr_handle, (uint32_t *)aprdata);

	if (ret < 0) {
		pr_err("%s: Fail in sending request %d\n",
			__func__, ret);
		ret = -EINVAL;
	} else {
		pr_debug("%s: apr packet sent successfully %d\n",
			 __func__, ret);
		ret = 0;
	}

done:
	kfree(aprdata);
	return ret;
}
static int voice_svc_reg(char *svc, uint32_t src_port,
			 struct voice_svc_prvt *prtd, void **handle)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (handle == NULL) {
		pr_err("%s: handle is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (*handle != NULL) {
		pr_err("%s: svc handle not NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (src_port == (APR_MAX_PORTS - 1)) {
		pr_err("%s: SRC port reserved for dummy session\n", __func__);
		pr_err("%s: Unable to register %s\n", __func__, svc);
		ret = -EINVAL;
		goto done;
	}

	*handle = apr_register("ADSP",
			       svc, qdsp_apr_callback,
			       ((src_port) << 8 | 0x0001),
			       prtd);

	if (*handle == NULL) {
		pr_err("%s: Unable to register %s\n",
		       __func__, svc);

		ret = -EFAULT;
		goto done;
	}
	pr_debug("%s: Register %s successful\n",
		__func__, svc);
done:
	return ret;
}

static int voice_svc_dereg(char *svc, void **handle)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (handle == NULL) {
		pr_err("%s: handle is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (*handle == NULL) {
		pr_err("%s: svc handle is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	ret = apr_deregister(*handle);
	if (ret) {
		pr_err("%s: Unable to deregister service %s; error: %d\n",
		       __func__, svc, ret);

		goto done;
	}
	*handle = NULL;
	pr_debug("%s: deregister %s successful\n", __func__, svc);

done:
	return ret;
}

static int process_reg_cmd(struct voice_svc_register *apr_reg_svc,
			   struct voice_svc_prvt *prtd)
{
	int ret = 0;
	char *svc = NULL;
	void **handle = NULL;

	pr_debug("%s\n", __func__);

	if (!strcmp(apr_reg_svc->svc_name, VOICE_SVC_MVM_STR)) {
		svc = VOICE_SVC_MVM_STR;
		handle = &prtd->apr_q6_mvm;
	} else if (!strcmp(apr_reg_svc->svc_name, VOICE_SVC_CVS_STR)) {
		svc = VOICE_SVC_CVS_STR;
		handle = &prtd->apr_q6_cvs;
	} else {
		pr_err("%s: Invalid Service: %.*s\n", __func__,
			MAX_APR_SERVICE_NAME_LEN, apr_reg_svc->svc_name);
		ret = -EINVAL;
		goto done;
	}

	if (apr_reg_svc->reg_flag) {
		ret = voice_svc_reg(svc, apr_reg_svc->src_port, prtd,
				    handle);
	} else if (!apr_reg_svc->reg_flag) {
		ret = voice_svc_dereg(svc, handle);
	}

done:
	return ret;
}

static ssize_t voice_svc_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	int ret = 0;
	struct voice_svc_prvt *prtd;
	struct voice_svc_write_msg *data = NULL;
	uint32_t cmd;
	struct voice_svc_register *register_data = NULL;
	struct voice_svc_cmd_request *request_data = NULL;
	uint32_t request_payload_size;

	pr_debug("%s\n", __func__);

	/*
	 * Check if enough memory is allocated to parse the message type.
	 * Will check there is enough to hold the payload later.
	 */
	if (count >= sizeof(struct voice_svc_write_msg)) {
		data = kmalloc(count, GFP_KERNEL);
	} else {
		pr_debug("%s: invalid data size\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (data == NULL) {
		pr_err("%s: data kmalloc failed.\n", __func__);

		ret = -ENOMEM;
		goto done;
	}

	ret = copy_from_user(data, buf, count);
	if (ret) {
		pr_err("%s: copy_from_user failed %d\n", __func__, ret);

		ret = -EPERM;
		goto done;
	}

	cmd = data->msg_type;
	prtd = (struct voice_svc_prvt *) file->private_data;
	if (prtd == NULL) {
		pr_err("%s: prtd is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	switch (cmd) {
	case MSG_REGISTER:
		/*
		 * Check that count reflects the expected size to ensure
		 * sufficient memory was allocated. Since voice_svc_register
		 * has a static size, this should be exact.
		 */
		if (count == (sizeof(struct voice_svc_write_msg) +
			      sizeof(struct voice_svc_register))) {
			register_data =
				(struct voice_svc_register *)data->payload;
			if (register_data == NULL) {
				pr_err("%s: register data is NULL", __func__);
				ret = -EINVAL;
				goto done;
			}
			ret = process_reg_cmd(register_data, prtd);
			if (!ret)
				ret = count;
		} else {
			pr_err("%s: invalid data payload size for register command\n",
				__func__);
			ret = -EINVAL;
			goto done;
		}
		break;
	case MSG_REQUEST:
		/*
		 * Check that count reflects the expected size to ensure
		 * sufficient memory was allocated. Since voice_svc_cmd_request
		 * has a variable size, check the minimum value count must be to
		 * parse the message request then check the minimum size to hold
		 * the payload of the message request.
		 */
		if (count >= (sizeof(struct voice_svc_write_msg) +
			      sizeof(struct voice_svc_cmd_request))) {
			request_data =
				(struct voice_svc_cmd_request *)data->payload;
			if (request_data == NULL) {
				pr_err("%s: request data is NULL", __func__);
				ret = -EINVAL;
				goto done;
			}

			request_payload_size = request_data->payload_size;

			if (count >= (sizeof(struct voice_svc_write_msg) +
				      sizeof(struct voice_svc_cmd_request) +
				      request_payload_size)) {
				ret = voice_svc_send_req(request_data, prtd);
				if (!ret)
					ret = count;
			} else {
				pr_err("%s: invalid request payload size\n",
					__func__);
				ret = -EINVAL;
				goto done;
			}
		} else {
			pr_err("%s: invalid data payload size for request command\n",
				__func__);
			ret = -EINVAL;
			goto done;
		}
		break;
	default:
		pr_debug("%s: Invalid command: %u\n", __func__, cmd);
		ret = -EINVAL;
	}

done:
	kfree(data);
	return ret;
}

static ssize_t voice_svc_read(struct file *file, char __user *arg,
			      size_t count, loff_t *ppos)
{
	int ret = 0;
	struct voice_svc_prvt *prtd;
	struct apr_response_list *resp;
	unsigned long spin_flags;
	int size;

	pr_debug("%s\n", __func__);

	prtd = (struct voice_svc_prvt *)file->private_data;
	if (prtd == NULL) {
		pr_err("%s: prtd is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&prtd->response_mutex_lock);
	spin_lock_irqsave(&prtd->response_lock, spin_flags);

	if (list_empty(&prtd->response_queue)) {
		spin_unlock_irqrestore(&prtd->response_lock, spin_flags);
		pr_debug("%s: wait for a response\n", __func__);

		ret = wait_event_interruptible_timeout(prtd->response_wait,
					!list_empty(&prtd->response_queue),
					msecs_to_jiffies(TIMEOUT_MS));
		if (ret == 0) {
			pr_debug("%s: Read timeout\n", __func__);

			ret = -ETIMEDOUT;
			goto unlock;
		} else if (ret > 0 && !list_empty(&prtd->response_queue)) {
			pr_debug("%s: Interrupt recieved for response\n",
				 __func__);
		} else if (ret < 0) {
			pr_debug("%s: Interrupted by SIGNAL %d\n",
				 __func__, ret);

			goto unlock;
		}

		spin_lock_irqsave(&prtd->response_lock, spin_flags);
	}

	resp = list_first_entry(&prtd->response_queue,
				struct apr_response_list, list);

	spin_unlock_irqrestore(&prtd->response_lock, spin_flags);

	size = resp->resp.payload_size +
	       sizeof(struct voice_svc_cmd_response);

	if (count < size) {
		pr_err("%s: Invalid payload size %zd, %d\n",
		       __func__, count, size);

		ret = -ENOMEM;
		goto unlock;
	}

	if (!access_ok(VERIFY_WRITE, arg, size)) {
		pr_err("%s: Access denied to write\n",
		       __func__);

		ret = -EPERM;
		goto unlock;
	}

	ret = copy_to_user(arg, &resp->resp,
			 sizeof(struct voice_svc_cmd_response) +
			 resp->resp.payload_size);
	if (ret) {
		pr_err("%s: copy_to_user failed %d\n", __func__, ret);

		ret = -EPERM;
		goto unlock;
	}

	spin_lock_irqsave(&prtd->response_lock, spin_flags);

	list_del(&resp->list);
	prtd->response_count--;
	kfree(resp);

	spin_unlock_irqrestore(&prtd->response_lock,
				spin_flags);

	ret = count;

unlock:
	mutex_unlock(&prtd->response_mutex_lock);
done:
	return ret;
}

static int voice_svc_dummy_reg()
{
	uint32_t src_port = APR_MAX_PORTS - 1;

	pr_debug("%s\n", __func__);
	dummy_q6_mvm = apr_register("ADSP", "MVM",
				qdsp_dummy_apr_callback,
				src_port,
				NULL);
	if (dummy_q6_mvm == NULL) {
		pr_err("%s: Unable to register dummy MVM\n", __func__);
		goto err;
	}

	dummy_q6_cvs = apr_register("ADSP", "CVS",
				qdsp_dummy_apr_callback,
				src_port,
				NULL);
	if (dummy_q6_cvs == NULL) {
		pr_err("%s: Unable to register dummy CVS\n", __func__);
		goto err;
	}
	return 0;
err:
	if (dummy_q6_mvm != NULL) {
		apr_deregister(dummy_q6_mvm);
		dummy_q6_mvm = NULL;
	}
	return -EINVAL;
}

static int voice_svc_dummy_dereg(void)
{
	pr_debug("%s\n", __func__);
	if (dummy_q6_mvm != NULL) {
		apr_deregister(dummy_q6_mvm);
		dummy_q6_mvm = NULL;
	}

	if (dummy_q6_cvs != NULL) {
		apr_deregister(dummy_q6_cvs);
		dummy_q6_cvs = NULL;
	}
	return 0;
}

static int voice_svc_open(struct inode *inode, struct file *file)
{
	struct voice_svc_prvt *prtd = NULL;

	pr_debug("%s\n", __func__);

	prtd = kmalloc(sizeof(struct voice_svc_prvt), GFP_KERNEL);

	if (prtd == NULL) {
		pr_err("%s: kmalloc failed\n", __func__);
		return -ENOMEM;
	}

	memset(prtd, 0, sizeof(struct voice_svc_prvt));
	prtd->apr_q6_cvs = NULL;
	prtd->apr_q6_mvm = NULL;
	prtd->response_count = 0;
	INIT_LIST_HEAD(&prtd->response_queue);
	init_waitqueue_head(&prtd->response_wait);
	spin_lock_init(&prtd->response_lock);
	mutex_init(&prtd->response_mutex_lock);
	file->private_data = (void *)prtd;

	is_released = 0;
	/* Current APR implementation doesn't support session based
	 * multiple service registrations. The apr_deregister()
	 * function sets the destination and client IDs to zero, if
	 * deregister is called for a single service instance.
	 * To avoid this, register for additional services.
	 */
	if (!reg_dummy_sess) {
		voice_svc_dummy_reg();
		reg_dummy_sess = 1;
	}
	return 0;
}

static int voice_svc_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct apr_response_list *resp = NULL;
	unsigned long spin_flags;
	struct voice_svc_prvt *prtd = NULL;
	char *svc_name = NULL;
	void **handle = NULL;

	pr_debug("%s\n", __func__);

	prtd = (struct voice_svc_prvt *)file->private_data;
	if (prtd == NULL) {
		pr_err("%s: prtd is NULL\n", __func__);

		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&prtd->response_mutex_lock);
	if (reg_dummy_sess) {
		voice_svc_dummy_dereg();
		reg_dummy_sess = 0;
	}
	if (prtd->apr_q6_cvs != NULL) {
		svc_name = VOICE_SVC_MVM_STR;
		handle = &prtd->apr_q6_cvs;
		ret = voice_svc_dereg(svc_name, handle);
		if (ret)
			pr_err("%s: Failed to dereg CVS %d\n", __func__, ret);
	}

	if (prtd->apr_q6_mvm != NULL) {
		svc_name = VOICE_SVC_MVM_STR;
		handle = &prtd->apr_q6_mvm;
		ret = voice_svc_dereg(svc_name, handle);
		if (ret)
			pr_err("%s: Failed to dereg MVM %d\n", __func__, ret);
	}

	spin_lock_irqsave(&prtd->response_lock, spin_flags);

	while (!list_empty(&prtd->response_queue)) {
		pr_debug("%s: Remove item from response queue\n", __func__);

		resp = list_first_entry(&prtd->response_queue,
					struct apr_response_list, list);
		list_del(&resp->list);
		prtd->response_count--;
		kfree(resp);
	}

	spin_unlock_irqrestore(&prtd->response_lock, spin_flags);
	mutex_unlock(&prtd->response_mutex_lock);

	mutex_destroy(&prtd->response_mutex_lock);

	spin_lock(&voicesvc_lock);
	kfree(file->private_data);
	file->private_data = NULL;
	is_released = 1;
	spin_unlock(&voicesvc_lock);
done:
	return ret;
}

static const struct file_operations voice_svc_fops = {
	.owner =                THIS_MODULE,
	.open =                 voice_svc_open,
	.read =                 voice_svc_read,
	.write =                voice_svc_write,
	.release =              voice_svc_release,
};


static int voice_svc_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

	voice_svc_dev = devm_kzalloc(&pdev->dev,
				  sizeof(struct voice_svc_device), GFP_KERNEL);
	if (!voice_svc_dev) {
		pr_err("%s: kzalloc failed\n", __func__);
		ret = -ENOMEM;
		goto done;
	}

	ret = alloc_chrdev_region(&device_num, 0, MINOR_NUMBER,
				  VOICE_SVC_DRIVER_NAME);
	if (ret) {
		pr_err("%s: Failed to alloc chrdev\n", __func__);
		ret = -ENODEV;
		goto chrdev_err;
	}

	voice_svc_dev->major = MAJOR(device_num);
	voice_svc_class = class_create(THIS_MODULE, VOICE_SVC_DRIVER_NAME);
	if (IS_ERR(voice_svc_class)) {
		ret = PTR_ERR(voice_svc_class);
		pr_err("%s: Failed to create class; err = %d\n", __func__,
			ret);
		goto class_err;
	}

	voice_svc_dev->dev = device_create(voice_svc_class, NULL, device_num,
					   NULL, VOICE_SVC_DRIVER_NAME);
	if (IS_ERR(voice_svc_dev->dev)) {
		ret = PTR_ERR(voice_svc_dev->dev);
		pr_err("%s: Failed to create device; err = %d\n", __func__,
			ret);
		goto dev_err;
	}

	voice_svc_dev->cdev = cdev_alloc();
	if (!voice_svc_dev->cdev) {
		pr_err("%s: Failed to alloc cdev\n", __func__);
		ret = -ENOMEM;
		goto cdev_alloc_err;
	}

	cdev_init(voice_svc_dev->cdev, &voice_svc_fops);
	ret = cdev_add(voice_svc_dev->cdev, device_num, MINOR_NUMBER);
	if (ret) {
		pr_err("%s: Failed to register chrdev; err = %d\n", __func__,
			ret);
		goto add_err;
	}
	pr_debug("%s: Device created\n", __func__);
	spin_lock_init(&voicesvc_lock);
	goto done;

add_err:
	cdev_del(voice_svc_dev->cdev);
cdev_alloc_err:
	device_destroy(voice_svc_class, device_num);
dev_err:
	class_destroy(voice_svc_class);
class_err:
	unregister_chrdev_region(0, MINOR_NUMBER);
chrdev_err:
	kfree(voice_svc_dev);
done:
	return ret;
}

static int voice_svc_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	cdev_del(voice_svc_dev->cdev);
	kfree(voice_svc_dev->cdev);
	device_destroy(voice_svc_class, device_num);
	class_destroy(voice_svc_class);
	unregister_chrdev_region(0, MINOR_NUMBER);
	kfree(voice_svc_dev);

	return 0;
}

static struct of_device_id voice_svc_of_match[] = {
	{.compatible = "qcom,msm-voice-svc"},
	{ }
};
MODULE_DEVICE_TABLE(of, voice_svc_of_match);

static struct platform_driver voice_svc_driver = {
	.probe          = voice_svc_probe,
	.remove         = voice_svc_remove,
	.driver         = {
		.name   = "msm-voice-svc",
		.owner  = THIS_MODULE,
		.of_match_table = voice_svc_of_match,
	},
};

static int __init voice_svc_init(void)
{
	pr_debug("%s\n", __func__);

	return platform_driver_register(&voice_svc_driver);
}

static void __exit voice_svc_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&voice_svc_driver);
}

module_init(voice_svc_init);
module_exit(voice_svc_exit);

MODULE_DESCRIPTION("Soc QDSP6v2 Voice Service driver");
MODULE_LICENSE("GPL v2");
