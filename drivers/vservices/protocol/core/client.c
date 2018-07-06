
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 /*
  * This is the generated code for the core client protocol handling.
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
#include <vservices/protocol/core/types.h>
#include <vservices/protocol/core/common.h>
#include <vservices/protocol/core/client.h>
#include <vservices/service.h>

#include "../../transport.h"

#define VS_MBUF_SIZE(mbuf) mbuf->size
#define VS_MBUF_DATA(mbuf) mbuf->data
#define VS_STATE_SERVICE_PTR(state) state->service

/*** Linux driver model integration ***/
struct vs_core_client_driver {
	struct vs_client_core *client;
	struct list_head list;
	struct vs_service_driver vsdrv;
};

#define to_client_driver(d) \
        container_of(d, struct vs_core_client_driver, vsdrv)

static void core_handle_start(struct vs_service_device *service)
{

	struct vs_client_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (client->start)
		client->start(state);
	vs_service_state_unlock(service);
}

static void core_handle_reset(struct vs_service_device *service)
{

	struct vs_client_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (client->reset)
		client->reset(state);
	vs_service_state_unlock(service);
}

static void core_handle_start_bh(struct vs_service_device *service)
{

	struct vs_client_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock_bh(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (client->start)
		client->start(state);
	vs_service_state_unlock_bh(service);
}

static void core_handle_reset_bh(struct vs_service_device *service)
{

	struct vs_client_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client __maybe_unused =
	    to_client_driver(vsdrv)->client;

	vs_service_state_lock_bh(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (client->reset)
		client->reset(state);
	vs_service_state_unlock_bh(service);
}

static int core_client_probe(struct vs_service_device *service);
static int core_client_remove(struct vs_service_device *service);
static int core_handle_message(struct vs_service_device *service,
			       struct vs_mbuf *_mbuf);
static void core_handle_notify(struct vs_service_device *service,
			       uint32_t flags);
static void core_handle_start(struct vs_service_device *service);
static void core_handle_start_bh(struct vs_service_device *service);
static void core_handle_reset(struct vs_service_device *service);
static void core_handle_reset_bh(struct vs_service_device *service);
static int core_handle_tx_ready(struct vs_service_device *service);

int __vservice_core_client_register(struct vs_client_core *client,
				    const char *name, struct module *owner)
{
	int ret;
	struct vs_core_client_driver *driver;

	if (client->tx_atomic && !client->rx_atomic)
		return -EINVAL;

	driver = kzalloc(sizeof(*driver), GFP_KERNEL);
	if (!driver) {
		ret = -ENOMEM;
		goto fail_alloc_driver;
	}

	client->driver = &driver->vsdrv;
	driver->client = client;

	driver->vsdrv.protocol = VSERVICE_CORE_PROTOCOL_NAME;

	driver->vsdrv.is_server = false;
	driver->vsdrv.rx_atomic = client->rx_atomic;
	driver->vsdrv.tx_atomic = client->tx_atomic;

	driver->vsdrv.probe = core_client_probe;
	driver->vsdrv.remove = core_client_remove;
	driver->vsdrv.receive = core_handle_message;
	driver->vsdrv.notify = core_handle_notify;
	driver->vsdrv.start = client->tx_atomic ?
	    core_handle_start_bh : core_handle_start;
	driver->vsdrv.reset = client->tx_atomic ?
	    core_handle_reset_bh : core_handle_reset;
	driver->vsdrv.tx_ready = core_handle_tx_ready;
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

EXPORT_SYMBOL(__vservice_core_client_register);

int vservice_core_client_unregister(struct vs_client_core *client)
{
	struct vs_core_client_driver *driver;

	if (!client->driver)
		return 0;

	driver = to_client_driver(client->driver);
	driver_unregister(&driver->vsdrv.driver);

	client->driver = NULL;
	kfree(driver);

	return 0;
}

EXPORT_SYMBOL(vservice_core_client_unregister);

static int core_client_probe(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client = to_client_driver(vsdrv)->client;
	struct vs_client_core_state *state;

	state = client->alloc(service);
	if (!state)
		return -ENOMEM;
	else if (IS_ERR(state))
		return PTR_ERR(state);

	state->service = vs_get_service(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;

	dev_set_drvdata(&service->dev, state);

	return 0;
}

static int core_client_remove(struct vs_service_device *service)
{
	struct vs_client_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client = to_client_driver(vsdrv)->client;

	state->released = true;
	dev_set_drvdata(&service->dev, NULL);
	client->release(state);

	vs_put_service(service);

	return 0;
}

static int core_handle_tx_ready(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_client_core *client = to_client_driver(vsdrv)->client;
	struct vs_client_core_state *state = dev_get_drvdata(&service->dev);

	if (client->tx_ready)
		client->tx_ready(state);

	return 0;
}

int vs_client_core_core_getbufs_service_created(struct vs_client_core_state
						*_state,
						struct vs_string *service_name,
						struct vs_string *protocol_name,
						struct vs_mbuf *_mbuf)
{
	const vs_message_id_t _msg_id = VSERVICE_CORE_CORE_MSG_SERVICE_CREATED;
	const size_t _max_size =
	    sizeof(vs_message_id_t) + VSERVICE_CORE_SERVICE_NAME_SIZE +
	    VSERVICE_CORE_PROTOCOL_NAME_SIZE + 4UL;
	const size_t _min_size = _max_size - VSERVICE_CORE_PROTOCOL_NAME_SIZE;
	size_t _exact_size;

	if (*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) != _msg_id)
		return -EINVAL;
	if ((VS_MBUF_SIZE(_mbuf) > _max_size)
	    || (VS_MBUF_SIZE(_mbuf) < _min_size))
		return -EBADMSG;

	service_name->ptr =
	    (char *)(VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	service_name->max_size = VSERVICE_CORE_SERVICE_NAME_SIZE;

	protocol_name->ptr =
	    (char *)(VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
		     VSERVICE_CORE_SERVICE_NAME_SIZE + 4UL);
	protocol_name->max_size =
	    VS_MBUF_SIZE(_mbuf) - (sizeof(vs_message_id_t) +
				   VSERVICE_CORE_SERVICE_NAME_SIZE + 4UL);

	/* Now check the size received is the exact size expected */
	_exact_size =
	    _max_size - (VSERVICE_CORE_PROTOCOL_NAME_SIZE -
			 protocol_name->max_size);
	if (VS_MBUF_SIZE(_mbuf) != _exact_size)
		return -EBADMSG;

	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_getbufs_service_created);
int vs_client_core_core_free_service_created(struct vs_client_core_state
					     *_state,
					     struct vs_string *service_name,
					     struct vs_string *protocol_name,
					     struct vs_mbuf *_mbuf)
{
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);

	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_free_service_created);
int
vs_client_core_core_req_connect(struct vs_client_core_state *_state,
				gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_core *_client =
	    to_client_driver(vsdrv)->client;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_DISCONNECTED:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  flags);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) =
	    VSERVICE_CORE_CORE_REQ_CONNECT;

	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED__CONNECT;

	if (_client->core.state_change)
		_client->core.state_change(_state,
					   VSERVICE_CORE_STATE_DISCONNECTED,
					   VSERVICE_CORE_STATE_DISCONNECTED__CONNECT);

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

EXPORT_SYMBOL(vs_client_core_core_req_connect);
int
vs_client_core_core_req_disconnect(struct vs_client_core_state *_state,
				   gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_core *_client =
	    to_client_driver(vsdrv)->client;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  flags);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) =
	    VSERVICE_CORE_CORE_REQ_DISCONNECT;

	_state->state.core.statenum = VSERVICE_CORE_STATE_CONNECTED__DISCONNECT;

	if (_client->core.state_change)
		_client->core.state_change(_state,
					   VSERVICE_CORE_STATE_CONNECTED,
					   VSERVICE_CORE_STATE_CONNECTED__DISCONNECT);

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

EXPORT_SYMBOL(vs_client_core_core_req_disconnect);
static int
core_core_handle_ack_connect(const struct vs_client_core *_client,
			     struct vs_client_core_state *_state,
			     struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_DISCONNECTED__CONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}
	_state->state.core.statenum = VSERVICE_CORE_STATE_CONNECTED;

	if (_client->core.state_change)
		_client->core.state_change(_state,
					   VSERVICE_CORE_STATE_DISCONNECTED__CONNECT,
					   VSERVICE_CORE_STATE_CONNECTED);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.ack_connect)
		return _client->core.ack_connect(_state);
	return 0;
}

static int
core_core_handle_nack_connect(const struct vs_client_core *_client,
			      struct vs_client_core_state *_state,
			      struct vs_mbuf *_mbuf)
{

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_DISCONNECTED__CONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}
	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED;

	if (_client->core.state_change)
		_client->core.state_change(_state,
					   VSERVICE_CORE_STATE_DISCONNECTED__CONNECT,
					   VSERVICE_CORE_STATE_DISCONNECTED);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.nack_connect)
		return _client->core.nack_connect(_state);
	return 0;
}

EXPORT_SYMBOL(core_core_handle_ack_connect);
static int
core_core_handle_ack_disconnect(const struct vs_client_core *_client,
				struct vs_client_core_state *_state,
				struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED__DISCONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}
	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED;

	if (_client->core.state_change)
		_client->core.state_change(_state,
					   VSERVICE_CORE_STATE_CONNECTED__DISCONNECT,
					   VSERVICE_CORE_STATE_DISCONNECTED);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.ack_disconnect)
		return _client->core.ack_disconnect(_state);
	return 0;
}

static int
core_core_handle_nack_disconnect(const struct vs_client_core *_client,
				 struct vs_client_core_state *_state,
				 struct vs_mbuf *_mbuf)
{

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED__DISCONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}
	_state->state.core.statenum = VSERVICE_CORE_STATE_CONNECTED;

	if (_client->core.state_change)
		_client->core.state_change(_state,
					   VSERVICE_CORE_STATE_CONNECTED__DISCONNECT,
					   VSERVICE_CORE_STATE_CONNECTED);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.nack_disconnect)
		return _client->core.nack_disconnect(_state);
	return 0;
}

EXPORT_SYMBOL(core_core_handle_ack_disconnect);
static int
vs_client_core_core_handle_startup(const struct vs_client_core *_client,
				   struct vs_client_core_state *_state,
				   struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 8UL;
	uint32_t core_in_quota;
	uint32_t core_out_quota;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_OFFLINE:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED;

	if (_client->core.state_change)
		_client->core.state_change(_state, VSERVICE_CORE_STATE_OFFLINE,
					   VSERVICE_CORE_STATE_DISCONNECTED);
	core_in_quota =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	core_out_quota =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.msg_startup)
		return _client->core.msg_startup(_state, core_in_quota,
						 core_out_quota);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_handle_startup);
static int
vs_client_core_core_handle_shutdown(const struct vs_client_core *_client,
				    struct vs_client_core_state *_state,
				    struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_DISCONNECTED:
	case VSERVICE_CORE_STATE_DISCONNECTED__CONNECT:
	case VSERVICE_CORE_STATE_CONNECTED:
	case VSERVICE_CORE_STATE_CONNECTED__DISCONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_DISCONNECTED:
		_state->state.core.statenum = VSERVICE_CORE_STATE_OFFLINE;

		if (_client->core.state_change)
			_client->core.state_change(_state,
						   VSERVICE_CORE_STATE_DISCONNECTED,
						   VSERVICE_CORE_STATE_OFFLINE);
		break;
	case VSERVICE_CORE_STATE_CONNECTED:
		_state->state.core.statenum = VSERVICE_CORE_STATE_OFFLINE;

		if (_client->core.state_change)
			_client->core.state_change(_state,
						   VSERVICE_CORE_STATE_CONNECTED,
						   VSERVICE_CORE_STATE_OFFLINE);
		break;

	default:
		break;
	}
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.msg_shutdown)
		return _client->core.msg_shutdown(_state);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_handle_shutdown);
static int
vs_client_core_core_handle_service_created(const struct vs_client_core *_client,
					   struct vs_client_core_state *_state,
					   struct vs_mbuf *_mbuf)
{
	const size_t _max_size =
	    sizeof(vs_message_id_t) + VSERVICE_CORE_SERVICE_NAME_SIZE +
	    VSERVICE_CORE_PROTOCOL_NAME_SIZE + 4UL;
	uint32_t service_id;
	struct vs_string service_name;
	struct vs_string protocol_name;
	const size_t _min_size = _max_size - VSERVICE_CORE_PROTOCOL_NAME_SIZE;
	size_t _exact_size;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED:
	case VSERVICE_CORE_STATE_CONNECTED__DISCONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	/* The first check is to ensure the message isn't complete garbage */
	if ((VS_MBUF_SIZE(_mbuf) > _max_size)
	    || (VS_MBUF_SIZE(_mbuf) < _min_size))
		return -EBADMSG;
	service_id =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	service_name.ptr =
	    (char *)(VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	service_name.max_size = VSERVICE_CORE_SERVICE_NAME_SIZE;

	protocol_name.ptr =
	    (char *)(VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
		     VSERVICE_CORE_SERVICE_NAME_SIZE + 4UL);
	protocol_name.max_size =
	    VS_MBUF_SIZE(_mbuf) - (sizeof(vs_message_id_t) +
				   VSERVICE_CORE_SERVICE_NAME_SIZE + 4UL);

	/* Now check the size received is the exact size expected */
	_exact_size =
	    _max_size - (VSERVICE_CORE_PROTOCOL_NAME_SIZE -
			 protocol_name.max_size);
	if (VS_MBUF_SIZE(_mbuf) != _exact_size)
		return -EBADMSG;
	if (_client->core.msg_service_created)
		return _client->core.msg_service_created(_state, service_id,
							 service_name,
							 protocol_name, _mbuf);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_handle_service_created);
static int
vs_client_core_core_handle_service_removed(const struct vs_client_core *_client,
					   struct vs_client_core_state *_state,
					   struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 4UL;
	uint32_t service_id;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED:
	case VSERVICE_CORE_STATE_CONNECTED__DISCONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	service_id =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.msg_service_removed)
		return _client->core.msg_service_removed(_state, service_id);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_handle_service_removed);
static int
vs_client_core_core_handle_server_ready(const struct vs_client_core *_client,
					struct vs_client_core_state *_state,
					struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 28UL;
	uint32_t service_id;
	uint32_t in_quota;
	uint32_t out_quota;
	uint32_t in_bit_offset;
	uint32_t in_num_bits;
	uint32_t out_bit_offset;
	uint32_t out_num_bits;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED:
	case VSERVICE_CORE_STATE_CONNECTED__DISCONNECT:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	service_id =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	in_quota =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	out_quota =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 8UL);
	in_bit_offset =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
			   12UL);
	in_num_bits =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
			   16UL);
	out_bit_offset =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
			   20UL);
	out_num_bits =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
			   24UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.msg_server_ready)
		return _client->core.msg_server_ready(_state, service_id,
						      in_quota, out_quota,
						      in_bit_offset,
						      in_num_bits,
						      out_bit_offset,
						      out_num_bits);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_handle_server_ready);
static int
vs_client_core_core_handle_service_reset(const struct vs_client_core *_client,
					 struct vs_client_core_state *_state,
					 struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 4UL;
	uint32_t service_id;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

	service_id =
	    *(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_client->core.msg_service_reset)
		return _client->core.msg_service_reset(_state, service_id);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_client_core_core_handle_service_reset);
int
vs_client_core_core_send_service_reset(struct vs_client_core_state *_state,
				       uint32_t service_id, gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 4UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_client_core *_client =
	    to_client_driver(vsdrv)->client;

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_CONNECTED:

		break;

	default:
		dev_err(&_state->service->dev,
			"[%s:%d] Protocol error: In wrong protocol state %d - %s\n",
			__func__, __LINE__, _state->state.core.statenum,
			vservice_core_get_state_string(_state->state.core));

		return -EPROTO;

	}

	_mbuf =
	    vs_service_alloc_mbuf(VS_STATE_SERVICE_PTR(_state), _msg_size,
				  flags);
	if (IS_ERR(_mbuf))
		return PTR_ERR(_mbuf);
	if (!_mbuf) {

		WARN_ON_ONCE(1);

		return -ENOMEM;
	}

	*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) =
	    VSERVICE_CORE_CORE_MSG_SERVICE_RESET;

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    service_id;

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

EXPORT_SYMBOL(vs_client_core_core_send_service_reset);
static int
core_handle_message(struct vs_service_device *service, struct vs_mbuf *_mbuf)
{
	vs_message_id_t message_id;
	__maybe_unused struct vs_client_core_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_client_core *client =
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

/** interface core **/
/* command in sync connect */
	case VSERVICE_CORE_CORE_ACK_CONNECT:
		ret = core_core_handle_ack_connect(client, state, _mbuf);
		break;
	case VSERVICE_CORE_CORE_NACK_CONNECT:
		ret = core_core_handle_nack_connect(client, state, _mbuf);
		break;

/* command in sync disconnect */
	case VSERVICE_CORE_CORE_ACK_DISCONNECT:
		ret = core_core_handle_ack_disconnect(client, state, _mbuf);
		break;
	case VSERVICE_CORE_CORE_NACK_DISCONNECT:
		ret = core_core_handle_nack_disconnect(client, state, _mbuf);
		break;

/* message startup */
	case VSERVICE_CORE_CORE_MSG_STARTUP:
		ret = vs_client_core_core_handle_startup(client, state, _mbuf);
		break;

/* message shutdown */
	case VSERVICE_CORE_CORE_MSG_SHUTDOWN:
		ret = vs_client_core_core_handle_shutdown(client, state, _mbuf);
		break;

/* message service_created */
	case VSERVICE_CORE_CORE_MSG_SERVICE_CREATED:
		ret =
		    vs_client_core_core_handle_service_created(client, state,
							       _mbuf);
		break;

/* message service_removed */
	case VSERVICE_CORE_CORE_MSG_SERVICE_REMOVED:
		ret =
		    vs_client_core_core_handle_service_removed(client, state,
							       _mbuf);
		break;

/* message server_ready */
	case VSERVICE_CORE_CORE_MSG_SERVER_READY:
		ret =
		    vs_client_core_core_handle_server_ready(client, state,
							    _mbuf);
		break;

/* message service_reset */
	case VSERVICE_CORE_CORE_MSG_SERVICE_RESET:
		ret =
		    vs_client_core_core_handle_service_reset(client, state,
							     _mbuf);
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

static void core_handle_notify(struct vs_service_device *service,
			       uint32_t notify_bits)
{
	__maybe_unused struct vs_client_core_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_client_core *client =
	    to_client_driver(vsdrv)->client;

	uint32_t bits = notify_bits;
	int ret;

	while (bits) {
		uint32_t not = __ffs(bits);
		switch (not) {

    /** interface core **/

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

MODULE_DESCRIPTION("OKL4 Virtual Services coreClient Protocol Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
