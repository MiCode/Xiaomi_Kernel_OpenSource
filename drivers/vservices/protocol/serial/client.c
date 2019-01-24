
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 /*
  * This is the generated code for the serial client protocol handling.
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
#include <vservices/protocol/serial/types.h>
#include <vservices/protocol/serial/common.h>
#include <vservices/protocol/serial/client.h>
#include <vservices/service.h>

#include "../../transport.h"

#define VS_MBUF_SIZE(mbuf) mbuf->size
#define VS_MBUF_DATA(mbuf) mbuf->data
#define VS_STATE_SERVICE_PTR(state) state->service

static int _vs_client_serial_req_open(struct vs_client_serial_state *_state);

/*** Linux driver model integration ***/
struct vs_serial_client_driver {
	struct vs_client_serial *client;
	struct list_head list;
	struct vs_service_driver vsdrv;
};

#define to_client_driver(d) \
        container_of(d, struct vs_serial_client_driver, vsdrv)

static void reset_nack_requests(struct vs_service_device *service)
{

}

static void serial_handle_start(struct vs_service_device *service)
{

	struct vs_client_serial_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock(service);
	state->state = VSERVICE_SERIAL_PROTOCOL_RESET_STATE;

	_vs_client_serial_req_open(state);

	vs_service_state_unlock(service);
}

static void serial_handle_reset(struct vs_service_device *service)
{

	struct vs_client_serial_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client __maybe_unused =
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

	state->state = VSERVICE_SERIAL_PROTOCOL_RESET_STATE;

	vs_service_state_unlock(service);
}

static void serial_handle_start_bh(struct vs_service_device *service)
{

	struct vs_client_serial_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock_bh(service);
	state->state = VSERVICE_SERIAL_PROTOCOL_RESET_STATE;

	_vs_client_serial_req_open(state);

	vs_service_state_unlock_bh(service);
}

static void serial_handle_reset_bh(struct vs_service_device *service)
{

	struct vs_client_serial_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client __maybe_unused =
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

	state->state = VSERVICE_SERIAL_PROTOCOL_RESET_STATE;

	vs_service_state_unlock_bh(service);
}

static int serial_client_probe(struct vs_service_device *service);
static int serial_client_remove(struct vs_service_device *service);
static int serial_handle_message(struct vs_service_device *service,
				 struct vs_mbuf *_mbuf);
static void serial_handle_notify(struct vs_service_device *service,
				 uint32_t flags);
static void serial_handle_start(struct vs_service_device *service);
static void serial_handle_start_bh(struct vs_service_device *service);
static void serial_handle_reset(struct vs_service_device *service);
static void serial_handle_reset_bh(struct vs_service_device *service);
static int serial_handle_tx_ready(struct vs_service_device *service);

int __vservice_serial_client_register(struct vs_client_serial *client,
				      const char *name, struct module *owner)
{
	int ret;
	struct vs_serial_client_driver *driver;

	if (client->tx_atomic && !client->rx_atomic)
		return -EINVAL;

	driver = kzalloc(sizeof(*driver), GFP_KERNEL);
	if (!driver) {
		ret = -ENOMEM;
		goto fail_alloc_driver;
	}

	client->driver = &driver->vsdrv;
	driver->client = client;

	driver->vsdrv.protocol = VSERVICE_SERIAL_PROTOCOL_NAME;

	driver->vsdrv.is_server = false;
	driver->vsdrv.rx_atomic = client->rx_atomic;
	driver->vsdrv.tx_atomic = client->tx_atomic;

	driver->vsdrv.probe = serial_client_probe;
	driver->vsdrv.remove = serial_client_remove;
	driver->vsdrv.receive = serial_handle_message;
	driver->vsdrv.notify = serial_handle_notify;
	driver->vsdrv.start = client->tx_atomic ?
	    serial_handle_start_bh : serial_handle_start;
	driver->vsdrv.reset = client->tx_atomic ?
	    serial_handle_reset_bh : serial_handle_reset;
	driver->vsdrv.tx_ready = serial_handle_tx_ready;
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

EXPORT_SYMBOL(__vservice_serial_client_register);

int vservice_serial_client_unregister(struct vs_client_serial *client)
{
	struct vs_serial_client_driver *driver;

	if (!client->driver)
		return 0;

	driver = to_client_driver(client->driver);
	driver_unregister(&driver->vsdrv.driver);

	client->driver = NULL;
	kfree(driver);

	return 0;
}

EXPORT_SYMBOL(vservice_serial_client_unregister);

static int serial_client_probe(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client = to_client_driver(vsdrv)->client;
	struct vs_client_serial_state *state;

	state = client->alloc(service);
	if (!state)
		return -ENOMEM;
	else if (IS_ERR(state))
		return PTR_ERR(state);

	state->service = vs_get_service(service);
	state->state = VSERVICE_SERIAL_PROTOCOL_RESET_STATE;

	dev_set_drvdata(&service->dev, state);

	return 0;
}

static int serial_client_remove(struct vs_service_device *service)
{
	struct vs_client_serial_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client = to_client_driver(vsdrv)->client;

	state->released = true;
	dev_set_drvdata(&service->dev, NULL);
	client->release(state);

	vs_put_service(service);

	return 0;
}

static int serial_handle_tx_ready(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_serial *client = to_client_driver(vsdrv)->client;
	struct vs_client_serial_state *state = dev_get_drvdata(&service->dev);

	if (!VSERVICE_BASE_STATE_IS_RUNNING(state->state.base))
		return 0;

	if (client->tx_ready)
		client->tx_ready(state);

	return 0;
}

static int _vs_client_serial_req_open(struct vs_client_serial_state *_state)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_serial *_client =
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
	    VSERVICE_SERIAL_BASE_REQ_OPEN;

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

EXPORT_SYMBOL(_vs_client_serial_req_open);
static int _vs_client_serial_req_close(struct vs_client_serial_state *_state)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_serial *_client =
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
	    VSERVICE_SERIAL_BASE_REQ_CLOSE;

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

EXPORT_SYMBOL(_vs_client_serial_req_close);
static int _vs_client_serial_req_reopen(struct vs_client_serial_state *_state)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_serial *_client =
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
	    VSERVICE_SERIAL_BASE_REQ_REOPEN;

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

EXPORT_SYMBOL(_vs_client_serial_req_reopen);
static int
serial_base_handle_ack_open(const struct vs_client_serial *_client,
			    struct vs_client_serial_state *_state,
			    struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 4UL;

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
	_state->serial.packet_size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	_state->packet_size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	_client->opened(_state);
	return 0;

}

static int
serial_base_handle_nack_open(const struct vs_client_serial *_client,
			     struct vs_client_serial_state *_state,
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

EXPORT_SYMBOL(serial_base_handle_ack_open);
static int
serial_base_handle_ack_close(const struct vs_client_serial *_client,
			     struct vs_client_serial_state *_state,
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
serial_base_handle_nack_close(const struct vs_client_serial *_client,
			      struct vs_client_serial_state *_state,
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

EXPORT_SYMBOL(serial_base_handle_ack_close);
static int
serial_base_handle_ack_reopen(const struct vs_client_serial *_client,
			      struct vs_client_serial_state *_state,
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
	return _vs_client_serial_req_open(_state);

}

static int
serial_base_handle_nack_reopen(const struct vs_client_serial *_client,
			       struct vs_client_serial_state *_state,
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

EXPORT_SYMBOL(serial_base_handle_ack_reopen);
struct vs_mbuf *vs_client_serial_serial_alloc_msg(struct vs_client_serial_state
						  *_state, struct vs_pbuf *b,
						  gfp_t flags)
{
	struct vs_mbuf *_mbuf;
	const vs_message_id_t _msg_id = VSERVICE_SERIAL_SERIAL_MSG_MSG;
	const uint32_t _msg_size =
	    sizeof(vs_message_id_t) + _state->serial.packet_size + 4UL;
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

	if (!b)
		goto fail;
	b->data =
	    (uintptr_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL +
			   sizeof(uint32_t));
	b->size = _state->serial.packet_size;
	b->max_size = b->size;
	return _mbuf;

 fail:
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	return NULL;
}

EXPORT_SYMBOL(vs_client_serial_serial_alloc_msg);
int vs_client_serial_serial_getbufs_msg(struct vs_client_serial_state *_state,
					struct vs_pbuf *b,
					struct vs_mbuf *_mbuf)
{
	const vs_message_id_t _msg_id = VSERVICE_SERIAL_SERIAL_MSG_MSG;
	const size_t _max_size =
	    sizeof(vs_message_id_t) + _state->serial.packet_size + 4UL;
	const size_t _min_size = _max_size - _state->serial.packet_size;
	size_t _exact_size;

	if (*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) != _msg_id)
		return -EINVAL;
	if ((VS_MBUF_SIZE(_mbuf) > _max_size)
	    || (VS_MBUF_SIZE(_mbuf) < _min_size))
		return -EBADMSG;

	b->size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	b->data =
	    (uintptr_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL +
			   sizeof(uint32_t));
	b->max_size = b->size;

	/* Now check the size received is the exact size expected */
	_exact_size = _max_size - (_state->serial.packet_size - b->size);
	if (VS_MBUF_SIZE(_mbuf) != _exact_size)
		return -EBADMSG;

	return 0;
}

EXPORT_SYMBOL(vs_client_serial_serial_getbufs_msg);
int vs_client_serial_serial_free_msg(struct vs_client_serial_state *_state,
				     struct vs_pbuf *b, struct vs_mbuf *_mbuf)
{
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);

	return 0;
}

EXPORT_SYMBOL(vs_client_serial_serial_free_msg);
static int
vs_client_serial_serial_handle_msg(const struct vs_client_serial *_client,
				   struct vs_client_serial_state *_state,
				   struct vs_mbuf *_mbuf)
{
	const size_t _max_size =
	    sizeof(vs_message_id_t) + _state->serial.packet_size + 4UL;
	struct vs_pbuf b;
	const size_t _min_size = _max_size - _state->serial.packet_size;
	size_t _exact_size;
	if (!VSERVICE_BASE_STATE_IS_RUNNING(_state->state.base))
		return -EPROTO;

	/* The first check is to ensure the message isn't complete garbage */
	if ((VS_MBUF_SIZE(_mbuf) > _max_size)
	    || (VS_MBUF_SIZE(_mbuf) < _min_size))
		return -EBADMSG;

	b.size =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	b.data =
	    (uintptr_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL +
			   sizeof(uint32_t));
	b.max_size = b.size;

	/* Now check the size received is the exact size expected */
	_exact_size = _max_size - (_state->serial.packet_size - b.size);
	if (VS_MBUF_SIZE(_mbuf) != _exact_size)
		return -EBADMSG;
	if (_client->serial.msg_msg)
		return _client->serial.msg_msg(_state, b, _mbuf);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_serial_serial_handle_msg);
int
vs_client_serial_serial_send_msg(struct vs_client_serial_state *_state,
				 struct vs_pbuf b, struct vs_mbuf *_mbuf)
{

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_serial *_client =
	    to_client_driver(vsdrv)->client;
	if (_state->state.base.statenum != VSERVICE_BASE_STATE_RUNNING)
		return -EPROTO;
	if (*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) !=
	    VSERVICE_SERIAL_SERIAL_MSG_MSG)

		return -EINVAL;

	if ((b.size + sizeof(vs_message_id_t) + 0UL) > VS_MBUF_SIZE(_mbuf))
		return -EINVAL;

	if (b.size < b.max_size)
		VS_MBUF_SIZE(_mbuf) -= (b.max_size - b.size);

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    b.size;

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

EXPORT_SYMBOL(vs_client_serial_serial_send_msg);
static int
serial_handle_message(struct vs_service_device *service, struct vs_mbuf *_mbuf)
{
	vs_message_id_t message_id;
	__maybe_unused struct vs_client_serial_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_client_serial *client =
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
	case VSERVICE_SERIAL_BASE_ACK_OPEN:
		ret = serial_base_handle_ack_open(client, state, _mbuf);
		break;
	case VSERVICE_SERIAL_BASE_NACK_OPEN:
		ret = serial_base_handle_nack_open(client, state, _mbuf);
		break;

/* command in sync close */
	case VSERVICE_SERIAL_BASE_ACK_CLOSE:
		ret = serial_base_handle_ack_close(client, state, _mbuf);
		break;
	case VSERVICE_SERIAL_BASE_NACK_CLOSE:
		ret = serial_base_handle_nack_close(client, state, _mbuf);
		break;

/* command in sync reopen */
	case VSERVICE_SERIAL_BASE_ACK_REOPEN:
		ret = serial_base_handle_ack_reopen(client, state, _mbuf);
		break;
	case VSERVICE_SERIAL_BASE_NACK_REOPEN:
		ret = serial_base_handle_nack_reopen(client, state, _mbuf);
		break;

/** interface serial **/
/* message msg */
	case VSERVICE_SERIAL_SERIAL_MSG_MSG:
		ret = vs_client_serial_serial_handle_msg(client, state, _mbuf);
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

static void serial_handle_notify(struct vs_service_device *service,
				 uint32_t notify_bits)
{
	__maybe_unused struct vs_client_serial_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_client_serial *client =
	    to_client_driver(vsdrv)->client;

	uint32_t bits = notify_bits;
	int ret;

	while (bits) {
		uint32_t not = __ffs(bits);
		switch (not) {

    /** interface serial **/

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

int vs_client_serial_reopen(struct vs_client_serial_state *_state)
{
	return _vs_client_serial_req_reopen(_state);
}

EXPORT_SYMBOL(vs_client_serial_reopen);

int vs_client_serial_close(struct vs_client_serial_state *_state)
{
	return _vs_client_serial_req_close(_state);
}

EXPORT_SYMBOL(vs_client_serial_close);

MODULE_DESCRIPTION("OKL4 Virtual Services serialClient Protocol Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
