/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include "mpq_adapter.h"
#include "mpq_dvb_debug.h"


DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

/* data-structure holding MPQ adapter information */
static struct
{
	/* MPQ adapter registered to dvb-core */
	struct dvb_adapter adapter;

	/* mutex protect against the data-structure */
	struct mutex mutex;

	/* List of stream interfaces registered to the MPQ adapter */
	struct {
		/* pointer to the stream buffer using for data tunneling */
		struct mpq_streambuffer *stream_buffer;

		/* callback triggered when the stream interface is registered */
		mpq_adapter_stream_if_callback callback;

		/* parameter passed to the callback function */
		void *user_param;
	} interfaces[MPQ_ADAPTER_MAX_NUM_OF_INTERFACES];
} mpq_info;


/**
 * Initialize MPQ DVB adapter module.
 *
 * Return     error status
 */
static int __init mpq_adapter_init(void)
{
	int i;
	int result;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	mutex_init(&mpq_info.mutex);

	/* reset stream interfaces list */
	for (i = 0; i < MPQ_ADAPTER_MAX_NUM_OF_INTERFACES; i++) {
		mpq_info.interfaces[i].stream_buffer = NULL;
		mpq_info.interfaces[i].callback = NULL;
	}

	/* regsiter a new dvb-adapter to dvb-core */
	result = dvb_register_adapter(&mpq_info.adapter,
				      "Qualcomm technologies, inc. DVB adapter",
				      THIS_MODULE, NULL, adapter_nr);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: dvb_register_adapter failed, errno %d\n",
			__func__,
			result);
	}

	return result;
}


/**
 * Cleanup MPQ DVB adapter module.
 */
static void __exit mpq_adapter_exit(void)
{
	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* un-regsiter adapter from dvb-core */
	dvb_unregister_adapter(&mpq_info.adapter);
	mutex_destroy(&mpq_info.mutex);
}

struct dvb_adapter *mpq_adapter_get(void)
{
	return &mpq_info.adapter;
}
EXPORT_SYMBOL(mpq_adapter_get);


int mpq_adapter_register_stream_if(
		enum mpq_adapter_stream_if interface_id,
		struct mpq_streambuffer *stream_buffer)
{
	int ret;

	if (interface_id >= MPQ_ADAPTER_MAX_NUM_OF_INTERFACES) {
		ret = -EINVAL;
		goto register_failed;
	}

	if (mutex_lock_interruptible(&mpq_info.mutex)) {
		ret = -ERESTARTSYS;
		goto register_failed;
	}

	if (mpq_info.interfaces[interface_id].stream_buffer != NULL) {
		/* already registered interface */
		ret = -EINVAL;
		goto register_failed_unlock_mutex;
	}

	mpq_info.interfaces[interface_id].stream_buffer = stream_buffer;
	mutex_unlock(&mpq_info.mutex);

	/*
	 * If callback is installed, trigger it to notify that
	 * stream interface was registered.
	 */
	if (mpq_info.interfaces[interface_id].callback != NULL) {
		mpq_info.interfaces[interface_id].callback(
				interface_id,
				mpq_info.interfaces[interface_id].user_param);
	}

	return 0;

register_failed_unlock_mutex:
	mutex_unlock(&mpq_info.mutex);
register_failed:
	return ret;
}
EXPORT_SYMBOL(mpq_adapter_register_stream_if);


int mpq_adapter_unregister_stream_if(
		enum mpq_adapter_stream_if interface_id)
{
	if (interface_id >= MPQ_ADAPTER_MAX_NUM_OF_INTERFACES)
		return -EINVAL;

	if (mutex_lock_interruptible(&mpq_info.mutex))
		return -ERESTARTSYS;

	/* clear the registered interface */
	mpq_info.interfaces[interface_id].stream_buffer = NULL;

	mutex_unlock(&mpq_info.mutex);

	return 0;
}
EXPORT_SYMBOL(mpq_adapter_unregister_stream_if);


int mpq_adapter_get_stream_if(
		enum mpq_adapter_stream_if interface_id,
		struct mpq_streambuffer **stream_buffer)
{
	if ((interface_id >= MPQ_ADAPTER_MAX_NUM_OF_INTERFACES) ||
		(stream_buffer == NULL))
		return -EINVAL;

	if (mutex_lock_interruptible(&mpq_info.mutex))
		return -ERESTARTSYS;

	*stream_buffer = mpq_info.interfaces[interface_id].stream_buffer;

	mutex_unlock(&mpq_info.mutex);

	return 0;
}
EXPORT_SYMBOL(mpq_adapter_get_stream_if);


int mpq_adapter_notify_stream_if(
		enum mpq_adapter_stream_if interface_id,
		mpq_adapter_stream_if_callback callback,
		void *user_param)
{
	if (interface_id >= MPQ_ADAPTER_MAX_NUM_OF_INTERFACES)
		return -EINVAL;

	if (mutex_lock_interruptible(&mpq_info.mutex))
		return -ERESTARTSYS;

	mpq_info.interfaces[interface_id].callback = callback;
	mpq_info.interfaces[interface_id].user_param = user_param;

	mutex_unlock(&mpq_info.mutex);

	return 0;
}
EXPORT_SYMBOL(mpq_adapter_notify_stream_if);


module_init(mpq_adapter_init);
module_exit(mpq_adapter_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. MPQ adapter");
MODULE_LICENSE("GPL v2");
