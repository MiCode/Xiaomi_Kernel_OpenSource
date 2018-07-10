
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 /*
  * This is the generated code for the block client protocol handling.
  */
#include <linux/types.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
#include <linux/export.h>
#endif

#include <vservices/types.h>
#include <vservices/buffer.h>
#include <vservices/protocol/block/types.h>
#include <vservices/protocol/block/common.h>
#include <vservices/protocol/block/client.h>
#include <vservices/service.h>

#include "../../transport.h"

#define VS_MBUF_SIZE(mbuf) mbuf->size
#define VS_MBUF_DATA(mbuf) mbuf->data
#define VS_STATE_SERVICE_PTR(state) state->service

static int _vs_client_block_req_open(struct vs_client_block_state *_state);

/*** Linux driver model integration ***/
struct vs_block_client_driver {
	struct vs_client_block *client;
	struct list_head list;
	struct vs_service_driver vsdrv;
};

#define to_client_driver(d) \
        container_of(d, struct vs_block_client_driver, vsdrv)

static void reset_nack_requests(struct vs_service_device *service)
{

	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	int i __maybe_unused;

	/* Clear out pending  read commands */
	for_each_set_bit(i, state->state.io.read_bitmask,
			 VSERVICE_BLOCK_IO_READ_MAX_PENDING) {
		void *tag = state->state.io.read_tags[i];

		if (client->io.nack_read)
			client->io.nack_read(state, tag,
					     VSERVICE_BLOCK_SERVICE_RESET);

		__clear_bit(i, state->state.io.read_bitmask);
	}

	/* Clear out pending  write commands */
	for_each_set_bit(i, state->state.io.write_bitmask,
			 VSERVICE_BLOCK_IO_WRITE_MAX_PENDING) {
		void *tag = state->state.io.write_tags[i];

		if (client->io.nack_write)
			client->io.nack_write(state, tag,
					      VSERVICE_BLOCK_SERVICE_RESET);

		__clear_bit(i, state->state.io.write_bitmask);
	}

}

static void block_handle_start(struct vs_service_device *service)
{

	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock(service);
	memset(&(state->state), 0, sizeof(state->state));

	_vs_client_block_req_open(state);

	vs_service_state_unlock(service);
}

static void block_handle_reset(struct vs_service_device *service)
{

	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock(service);
	if (!VSERVICE_BASE_STATE_IS_RUNNING(state->state.base)) {
		vs_service_state_unlock(service);
		return;
	}
	state->state.base = VSERVICE_BASE_RESET_STATE;
	reset_nack_requests(service);
	if (client->closed)
		client->closed(state);

	memset(&(state->state), 0, sizeof(state->state));

	vs_service_state_unlock(service);
}

static void block_handle_start_bh(struct vs_service_device *service)
{

	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock_bh(service);
	memset(&(state->state), 0, sizeof(state->state));

	_vs_client_block_req_open(state);

	vs_service_state_unlock_bh(service);
}

static void block_handle_reset_bh(struct vs_service_device *service)
{

	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock_bh(service);
	if (!VSERVICE_BASE_STATE_IS_RUNNING(state->state.base)) {
		vs_service_state_unlock_bh(service);
		return;
	}
	state->state.base = VSERVICE_BASE_RESET_STATE;
	reset_nack_requests(service);
	if (client->closed)
		client->closed(state);

	memset(&(state->state), 0, sizeof(state->state));

	vs_service_state_unlock_bh(service);
}

static int block_client_probe(struct vs_service_device *service);
static int block_client_remove(struct vs_service_device *service);
static int block_handle_message(struct vs_service_device *service,
				struct vs_mbuf *_mbuf);
static void block_handle_notify(struct vs_service_device *service,
				uint32_t flags);
static void block_handle_start(struct vs_service_device *service);
static void block_handle_start_bh(struct vs_service_device *service);
static void block_handle_reset(struct vs_service_device *service);
static void block_handle_reset_bh(struct vs_service_device *service);
static int block_handle_tx_ready(struct vs_service_device *service);

int __vservice_block_client_register(struct vs_client_block *client,
				     const char *name, struct module *owner)
{
	int ret;
	struct vs_block_client_driver *driver;

	if (client->tx_atomic && !client->rx_atomic)
		return -EINVAL;

	driver = kzalloc(sizeof(*driver), GFP_KERNEL);
	if (!driver) {
		ret = -ENOMEM;
		goto fail_alloc_driver;
	}

	client->driver = &driver->vsdrv;
	driver->client = client;

	driver->vsdrv.protocol = VSERVICE_BLOCK_PROTOCOL_NAME;

	driver->vsdrv.is_server = false;
	driver->vsdrv.rx_atomic = client->rx_atomic;
	driver->vsdrv.tx_atomic = client->tx_atomic;

	driver->vsdrv.probe = block_client_probe;
	driver->vsdrv.remove = block_client_remove;
	driver->vsdrv.receive = block_handle_message;
	driver->vsdrv.notify = block_handle_notify;
	driver->vsdrv.start = client->tx_atomic ?
	    block_handle_start_bh : block_handle_start;
	driver->vsdrv.reset = client->tx_atomic ?
	    block_handle_reset_bh : block_handle_reset;
	driver->vsdrv.tx_ready = block_handle_tx_ready;
	driver->vsdrv.out_notify_count = 0;
	driver->vsdrv.in_notify_count = 0;
	driver->vsdrv.driver.name = name;
	driver->vsdrv.driver.owner = owner;
	driver->vsdrv.driver.bus = &vs_client_bus_type;

	ret = driver_register(&driver->vsdrv.driver);

	if (ret) {
		goto fail_driver_register;
	}

	return 0;

 fail_driver_register:
	client->driver = NULL;
	kfree(driver);
 fail_alloc_driver:
	return ret;
}

EXPORT_SYMBOL(__vservice_block_client_register);

int vservice_block_client_unregister(struct vs_client_block *client)
{
	struct vs_block_client_driver *driver;

	if (!client->driver)
		return 0;

	driver = to_client_driver(client->driver);
	driver_unregister(&driver->vsdrv.driver);

	client->driver = NULL;
	kfree(driver);

	return 0;
}

EXPORT_SYMBOL(vservice_block_client_unregister);

static int block_client_probe(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client = to_client_driver(vsdrv)->client;
	struct vs_client_block_state *state;

	state = client->alloc(service);
	if (!state)
		return -ENOMEM;
	else if (IS_ERR(state))
		return PTR_ERR(state);

	state->service = vs_get_service(service);
	memset(&(state->state), 0, sizeof(state->state));

	dev_set_drvdata(&service->dev, state);

	return 0;
}

static int block_client_remove(struct vs_service_device *service)
{
	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client = to_client_driver(vsdrv)->client;

	state->released = true;
	dev_set_drvdata(&service->dev, NULL);
	client->release(state);

	vs_put_service(service);

	return 0;
}

static int block_handle_tx_ready(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_block *client = to_client_driver(vsdrv)->client;
	struct vs_client_block_state *state = dev_get_drvdata(&service->dev);

	if (!VSERVICE_BASE_STATE_IS_RUNNING(state->state.base))
		return 0;

	if (client->tx_ready)
		client->tx_ready(state);

	return 0;
}

static int _vs_client_block_req_open(struct vs_client_block_state *_state)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_block *_client =
	    to_client_driver(vsdrv)->client;

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_CLOSED:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  vs_service_has_atomic_rx(VS_STATE_SERVICE_PTR
							   (_state)) ?
				  GFP_ATOMIC : GFP_KERNEL);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) =
	    VSERVICE_BLOCK_BASE_REQ_OPEN;

	_state->state.base.statenum = VSERVICE_BASE_STATE_CLOSED__OPEN;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	return 0;
}

EXPORT_SYMBOL(_vs_client_block_req_open);
static int _vs_client_block_req_close(struct vs_client_block_state *_state)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_block *_client =
	    to_client_driver(vsdrv)->client;

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_RUNNING:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  vs_service_has_atomic_rx(VS_STATE_SERVICE_PTR
							   (_state)) ?
				  GFP_ATOMIC : GFP_KERNEL);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) =
	    VSERVICE_BLOCK_BASE_REQ_CLOSE;

	_state->state.base.statenum = VSERVICE_BASE_STATE_RUNNING__CLOSE;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	return 0;
}

EXPORT_SYMBOL(_vs_client_block_req_close);
static int _vs_client_block_req_reopen(struct vs_client_block_state *_state)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_block *_client =
	    to_client_driver(vsdrv)->client;

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_RUNNING:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  vs_service_has_atomic_rx(VS_STATE_SERVICE_PTR
							   (_state)) ?
				  GFP_ATOMIC : GFP_KERNEL);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) =
	    VSERVICE_BLOCK_BASE_REQ_REOPEN;

	_state->state.base.statenum = VSERVICE_BASE_STATE_RUNNING__REOPEN;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	return 0;
}

EXPORT_SYMBOL(_vs_client_block_req_reopen);
static int
block_base_handle_ack_open(const struct vs_client_block *_client,
			   struct vs_client_block_state *_state,
			   struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 28UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_CLOSED__OPEN:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}
	_state->state.base.statenum = VSERVICE_BASE_STATE_RUNNING;
	_state->io.sector_size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	_state->io.segment_size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 8UL);
	_state->readonly =
	    *(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	_state->sector_size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	_state->segment_size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 8UL);
	_state->device_sectors =
	    *(uint64_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
			   12UL);
	_state->flushable =
	    *(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 20UL);
	_state->committable =
	    *(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 24UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	_client->opened(_state);
	return 0;

}

static int
block_base_handle_nack_open(const struct vs_client_block *_client,
			    struct vs_client_block_state *_state,
			    struct vs_mbuf *_mbuf)
{

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_CLOSED__OPEN:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}
	_state->state.base.statenum = VSERVICE_BASE_STATE_CLOSED;
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	dev_err(&VS_STATE_SERVICE_PTR(_state)->dev,
		"Open operation failed for device %s\n",
		VS_STATE_SERVICE_PTR(_state)->name);

	return 0;

}

EXPORT_SYMBOL(block_base_handle_ack_open);
static int
block_base_handle_ack_close(const struct vs_client_block *_client,
			    struct vs_client_block_state *_state,
			    struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_RUNNING__CLOSE:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}
	_state->state.base.statenum = VSERVICE_BASE_STATE_CLOSED;
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	wake_up_all(&_state->service->quota_wq);
	_client->closed(_state);
	return 0;

}

static int
block_base_handle_nack_close(const struct vs_client_block *_client,
			     struct vs_client_block_state *_state,
			     struct vs_mbuf *_mbuf)
{

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_RUNNING__CLOSE:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}
	_state->state.base.statenum = VSERVICE_BASE_STATE_RUNNING;
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	wake_up_all(&_state->service->quota_wq);
	_client->closed(_state);
	return 0;

}

EXPORT_SYMBOL(block_base_handle_ack_close);
static int
block_base_handle_ack_reopen(const struct vs_client_block *_client,
			     struct vs_client_block_state *_state,
			     struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_RUNNING__REOPEN:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}
	_state->state.base.statenum = VSERVICE_BASE__RESET;
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->reopened) {
		_client->reopened(_state);
		return 0;
	}
	wake_up_all(&_state->service->quota_wq);
	_client->closed(_state);
	return _vs_client_block_req_open(_state);

}

static int
block_base_handle_nack_reopen(const struct vs_client_block *_client,
			      struct vs_client_block_state *_state,
			      struct vs_mbuf *_mbuf)
{

	switch (_state->state.base.statenum) {
	case VSERVICE_BASE_STATE_RUNNING__REOPEN:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.base.statenum,
			vservice_base_get_state_string(_state->state.base));

		return -EPROTO;

	}
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	return 0;

}

EXPORT_SYMBOL(block_base_handle_ack_reopen);
int vs_client_block_io_getbufs_ack_read(struct vs_client_block_state *_state,
					struct vs_pbuf *data,
					struct vs_mbuf *_mbuf)
{
	const vs_message_id_t _msg_id = VSERVICE_BLOCK_IO_ACK_READ;
	const size_t _max_size =
	    sizeof(vs_message_id_t) + _state->io.segment_size + 8UL;
	const size_t _min_size = _max_size - _state->io.segment_size;
	size_t _exact_size;

	if (*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) != _msg_id)
		return -EINVAL;
	if ((VS_MBUF_SIZE(_mbuf) > _max_size)
	    || (VS_MBUF_SIZE(_mbuf) < _min_size))
		return -EBADMSG;

	data->size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	data->data =
	    (uintptr_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL +
			   sizeof(uint32_t));
	data->max_size = data->size;

	/* Now check the size received is the exact size expected */
	_exact_size = _max_size - (_state->io.segment_size - data->size);
	if (VS_MBUF_SIZE(_mbuf) != _exact_size)
		return -EBADMSG;

	return 0;
}

EXPORT_SYMBOL(vs_client_block_io_getbufs_ack_read);
int vs_client_block_io_free_ack_read(struct vs_client_block_state *_state,
				     struct vs_pbuf *data,
				     struct vs_mbuf *_mbuf)
{
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);

	return 0;
}

EXPORT_SYMBOL(vs_client_block_io_free_ack_read);
struct vs_mbuf *vs_client_block_io_alloc_req_write(struct vs_client_block_state
						   *_state,
						   struct vs_pbuf *data,
						   gfp_t flags)
{
	struct vs_mbuf *_mbuf;
	const vs_message_id_t _msg_id = VSERVICE_BLOCK_IO_REQ_WRITE;
	const uint32_t _msg_size =
	    sizeof(vs_message_id_t) + _state->io.segment_size + 32UL;
	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  flags);
	if (IS_ERR(_mbuf))
		return _mbuf;
	if (!_mbuf) {

		WARN_ON_ONCE(1);
		return ERR_PTR(-ENOMEM);
	}
	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) = _msg_id;

	if (!data)
		goto fail;
	data->data =
	    (uintptr_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
			   28UL + sizeof(uint32_t));
	data->size = _state->io.segment_size;
	data->max_size = data->size;
	return _mbuf;

 fail:
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	return NULL;
}

EXPORT_SYMBOL(vs_client_block_io_alloc_req_write);
int vs_client_block_io_free_req_write(struct vs_client_block_state *_state,
				      struct vs_pbuf *data,
				      struct vs_mbuf *_mbuf)
{
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);

	return 0;
}

EXPORT_SYMBOL(vs_client_block_io_free_req_write);
int
vs_client_block_io_req_read(struct vs_client_block_state *_state, void *_opaque,
			    uint64_t sector_index, uint32_t num_sects,
			    bool nodelay, bool flush, gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 24UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_block *_client =
	    to_client_driver(vsdrv)->client;
	uint32_t _opaque_tmp;
	if (_state->state.base.statenum != VSERVICE_BASE_STATE_RUNNING)
		return -EPROTO;
	_opaque_tmp =
	    find_first_zero_bit(_state->state.io.read_bitmask,
				VSERVICE_BLOCK_IO_READ_MAX_PENDING);
	if (_opaque_tmp >= VSERVICE_BLOCK_IO_READ_MAX_PENDING)
		return -EPROTO;

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  flags);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) = VSERVICE_BLOCK_IO_REQ_READ;

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    _opaque_tmp;
	*(uint64_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL) =
	    sector_index;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 12UL) =
	    num_sects;
	*(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 16UL) =
	    nodelay;
	*(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 20UL) =
	    flush;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.io.read_tags[_opaque_tmp] = _opaque;
	__set_bit(_opaque_tmp, _state->state.io.read_bitmask);

	return 0;
}

EXPORT_SYMBOL(vs_client_block_io_req_read);
int
vs_client_block_io_req_write(struct vs_client_block_state *_state,
			     void *_opaque, uint64_t sector_index,
			     uint32_t num_sects, bool nodelay, bool flush,
			     bool commit, struct vs_pbuf data,
			     struct vs_mbuf *_mbuf)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_block *_client =
	    to_client_driver(vsdrv)->client;
	uint32_t _opaque_tmp;
	if (_state->state.base.statenum != VSERVICE_BASE_STATE_RUNNING)
		return -EPROTO;
	_opaque_tmp =
	    find_first_zero_bit(_state->state.io.write_bitmask,
				VSERVICE_BLOCK_IO_WRITE_MAX_PENDING);
	if (_opaque_tmp >= VSERVICE_BLOCK_IO_WRITE_MAX_PENDING)
		return -EPROTO;

	if (*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) !=
	    VSERVICE_BLOCK_IO_REQ_WRITE)

		return -EINVAL;

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    _opaque_tmp;
	*(uint64_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL) =
	    sector_index;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 12UL) =
	    num_sects;
	*(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 16UL) =
	    nodelay;
	*(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 20UL) =
	    flush;
	*(bool *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 24UL) =
	    commit;
	if ((data.size + sizeof(vs_message_id_t) + 28UL) > VS_MBUF_SIZE(_mbuf))
		return -EINVAL;

	if (data.size < data.max_size)
		VS_MBUF_SIZE(_mbuf) -= (data.max_size - data.size);

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 28UL) =
	    data.size;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.io.write_tags[_opaque_tmp] = _opaque;
	__set_bit(_opaque_tmp, _state->state.io.write_bitmask);

	return 0;
}

EXPORT_SYMBOL(vs_client_block_io_req_write);
static int
block_io_handle_ack_read(const struct vs_client_block *_client,
			 struct vs_client_block_state *_state,
			 struct vs_mbuf *_mbuf)
{
	const size_t _max_size =
	    sizeof(vs_message_id_t) + _state->io.segment_size + 8UL;
	void *_opaque;
	struct vs_pbuf data;
	const size_t _min_size = _max_size - _state->io.segment_size;
	size_t _exact_size;
	uint32_t _opaque_tmp;

	/* The first check is to ensure the message isn't complete garbage */
	if ((VS_MBUF_SIZE(_mbuf) > _max_size)
	    || (VS_MBUF_SIZE(_mbuf) < _min_size))
		return -EBADMSG;
	_opaque_tmp =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	if (_opaque_tmp >= VSERVICE_BLOCK_IO_READ_MAX_PENDING)
		return -EPROTO;
	if (!VSERVICE_BASE_STATE_IS_RUNNING(_state->state.base))
		return -EPROTO;
	if (!test_bit(_opaque_tmp, _state->state.io.read_bitmask))
		return -EPROTO;
	_opaque = _state->state.io.read_tags[_opaque_tmp];
	__clear_bit(_opaque_tmp, _state->state.io.read_bitmask);

	data.size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	data.data =
	    (uintptr_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL +
			   sizeof(uint32_t));
	data.max_size = data.size;

	/* Now check the size received is the exact size expected */
	_exact_size = _max_size - (_state->io.segment_size - data.size);
	if (VS_MBUF_SIZE(_mbuf) != _exact_size)
		return -EBADMSG;
	if (_client->io.ack_read)
		return _client->io.ack_read(_state, _opaque, data, _mbuf);
	return 0;
}

static int
block_io_handle_nack_read(const struct vs_client_block *_client,
			  struct vs_client_block_state *_state,
			  struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 8UL;
	void *_opaque;
	vservice_block_block_io_error_t err;
	uint32_t _opaque_tmp;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	_opaque_tmp =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	if (_opaque_tmp >= VSERVICE_BLOCK_IO_READ_MAX_PENDING)
		return -EPROTO;
	if (!VSERVICE_BASE_STATE_IS_RUNNING(_state->state.base))
		return -EPROTO;
	if (!test_bit(_opaque_tmp, _state->state.io.read_bitmask))
		return -EPROTO;
	_opaque = _state->state.io.read_tags[_opaque_tmp];
	__clear_bit(_opaque_tmp, _state->state.io.read_bitmask);
	err =
	    *(vservice_block_block_io_error_t *) (VS_MBUF_DATA(_mbuf) +
						  sizeof(vs_message_id_t) +
						  4UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->io.nack_read)
		return _client->io.nack_read(_state, _opaque, err);
	return 0;
}

EXPORT_SYMBOL(block_io_handle_ack_read);
static int
block_io_handle_ack_write(const struct vs_client_block *_client,
			  struct vs_client_block_state *_state,
			  struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 4UL;
	void *_opaque;
	uint32_t _opaque_tmp;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	_opaque_tmp =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	if (_opaque_tmp >= VSERVICE_BLOCK_IO_WRITE_MAX_PENDING)
		return -EPROTO;
	if (!VSERVICE_BASE_STATE_IS_RUNNING(_state->state.base))
		return -EPROTO;
	if (!test_bit(_opaque_tmp, _state->state.io.write_bitmask))
		return -EPROTO;
	_opaque = _state->state.io.write_tags[_opaque_tmp];
	__clear_bit(_opaque_tmp, _state->state.io.write_bitmask);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->io.ack_write)
		return _client->io.ack_write(_state, _opaque);
	return 0;
}

static int
block_io_handle_nack_write(const struct vs_client_block *_client,
			   struct vs_client_block_state *_state,
			   struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 8UL;
	void *_opaque;
	vservice_block_block_io_error_t err;
	uint32_t _opaque_tmp;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	_opaque_tmp =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	if (_opaque_tmp >= VSERVICE_BLOCK_IO_WRITE_MAX_PENDING)
		return -EPROTO;
	if (!VSERVICE_BASE_STATE_IS_RUNNING(_state->state.base))
		return -EPROTO;
	if (!test_bit(_opaque_tmp, _state->state.io.write_bitmask))
		return -EPROTO;
	_opaque = _state->state.io.write_tags[_opaque_tmp];
	__clear_bit(_opaque_tmp, _state->state.io.write_bitmask);
	err =
	    *(vservice_block_block_io_error_t *) (VS_MBUF_DATA(_mbuf) +
						  sizeof(vs_message_id_t) +
						  4UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->io.nack_write)
		return _client->io.nack_write(_state, _opaque, err);
	return 0;
}

EXPORT_SYMBOL(block_io_handle_ack_write);
static int
block_handle_message(struct vs_service_device *service, struct vs_mbuf *_mbuf)
{
	vs_message_id_t message_id;
	__maybe_unused struct vs_client_block_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_client_block *client =
	    to_client_driver(vsdrv)->client;

	int ret;

	/* Extract the message ID */
	if (VS_MBUF_SIZE(_mbuf) < sizeof(message_id)) {
		dev_err(&state->service->dev,
			"[%s:%d] Protocol error: Invalid message size %zd\n",
			__func__, __LINE__, VS_MBUF_SIZE(_mbuf));

		return -EBADMSG;
	}

	message_id = *(vs_message_id_t *) (VS_MBUF_DATA(_mbuf));

	switch (message_id) {

/** interface base **/
/* command in sync open */
	case VSERVICE_BLOCK_BASE_ACK_OPEN:
		ret = block_base_handle_ack_open(client, state, _mbuf);
		break;
	case VSERVICE_BLOCK_BASE_NACK_OPEN:
		ret = block_base_handle_nack_open(client, state, _mbuf);
		break;

/* command in sync close */
	case VSERVICE_BLOCK_BASE_ACK_CLOSE:
		ret = block_base_handle_ack_close(client, state, _mbuf);
		break;
	case VSERVICE_BLOCK_BASE_NACK_CLOSE:
		ret = block_base_handle_nack_close(client, state, _mbuf);
		break;

/* command in sync reopen */
	case VSERVICE_BLOCK_BASE_ACK_REOPEN:
		ret = block_base_handle_ack_reopen(client, state, _mbuf);
		break;
	case VSERVICE_BLOCK_BASE_NACK_REOPEN:
		ret = block_base_handle_nack_reopen(client, state, _mbuf);
		break;

/** interface block_io **/
/* command in parallel read */
	case VSERVICE_BLOCK_IO_ACK_READ:
		ret = block_io_handle_ack_read(client, state, _mbuf);
		break;
	case VSERVICE_BLOCK_IO_NACK_READ:
		ret = block_io_handle_nack_read(client, state, _mbuf);
		break;

/* command in parallel write */
	case VSERVICE_BLOCK_IO_ACK_WRITE:
		ret = block_io_handle_ack_write(client, state, _mbuf);
		break;
	case VSERVICE_BLOCK_IO_NACK_WRITE:
		ret = block_io_handle_nack_write(client, state, _mbuf);
		break;

	default:
		dev_err(&state->service->dev,
			"[%s:%d] Protocol error: Unknown message type %d\n",
			__func__, __LINE__, (int)message_id);

		ret = -EPROTO;
		break;
	}

	if (ret) {
		dev_err(&state->service->dev,
			"[%s:%d] Protocol error: Handler for message type %d returned %d\n",
			__func__, __LINE__, (int)message_id, ret);

	}

	return ret;
}

static void block_handle_notify(struct vs_service_device *service,
				uint32_t notify_bits)
{
	__maybe_unused struct vs_client_block_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_client_block *client =
	    to_client_driver(vsdrv)->client;

	uint32_t bits = notify_bits;
	int ret;

	while (bits) {
		uint32_t not = __ffs(bits);
		switch (not) {

    /** interface block_io **/

		default:
			dev_err(&state->service->dev,
				"[%s:%d] Protocol error: Unknown notification %d\n",
				__func__, __LINE__, (int)not);

			ret = -EPROTO;
			break;

		}
		bits &= ~(1 << not);
		if (ret) {
			dev_err(&state->service->dev,
				"[%s:%d] Protocol error: Handler for notification %d returned %d\n",
				__func__, __LINE__, (int)not, ret);

		}
	}
}

int vs_client_block_reopen(struct vs_client_block_state *_state)
{
	return _vs_client_block_req_reopen(_state);
}

EXPORT_SYMBOL(vs_client_block_reopen);

int vs_client_block_close(struct vs_client_block_state *_state)
{
	return _vs_client_block_req_close(_state);
}

EXPORT_SYMBOL(vs_client_block_close);

MODULE_DESCRIPTION("OKL4 Virtual Services blockClient Protocol Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
