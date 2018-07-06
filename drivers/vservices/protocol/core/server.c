
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 /*
  * This is the generated code for the core server protocol handling.
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
#include <vservices/protocol/core/server.h>
#include <vservices/service.h>

#include "../../transport.h"

#define VS_MBUF_SIZE(mbuf) mbuf->size
#define VS_MBUF_DATA(mbuf) mbuf->data
#define VS_STATE_SERVICE_PTR(state) state->service

/*** Linux driver model integration ***/
struct vs_core_server_driver {
	struct vs_server_core *server;
	struct list_head list;
	struct vs_service_driver vsdrv;
};

#define to_server_driver(d) \
        container_of(d, struct vs_core_server_driver, vsdrv)

static void core_handle_start(struct vs_service_device *service)
{

	struct vs_server_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server __maybe_unused =
	    to_server_driver(vsdrv)->server;

	vs_service_state_lock(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (server->start)
		server->start(state);
	vs_service_state_unlock(service);
}

static void core_handle_reset(struct vs_service_device *service)
{

	struct vs_server_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server __maybe_unused =
	    to_server_driver(vsdrv)->server;

	vs_service_state_lock(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (server->reset)
		server->reset(state);
	vs_service_state_unlock(service);
}

static void core_handle_start_bh(struct vs_service_device *service)
{

	struct vs_server_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server __maybe_unused =
	    to_server_driver(vsdrv)->server;

	vs_service_state_lock_bh(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (server->start)
		server->start(state);
	vs_service_state_unlock_bh(service);
}

static void core_handle_reset_bh(struct vs_service_device *service)
{

	struct vs_server_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server __maybe_unused =
	    to_server_driver(vsdrv)->server;

	vs_service_state_lock_bh(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;
	if (server->reset)
		server->reset(state);
	vs_service_state_unlock_bh(service);
}

static int core_server_probe(struct vs_service_device *service);
static int core_server_remove(struct vs_service_device *service);
static int core_handle_message(struct vs_service_device *service,
			       struct vs_mbuf *_mbuf);
static void core_handle_notify(struct vs_service_device *service,
			       uint32_t flags);
static void core_handle_start(struct vs_service_device *service);
static void core_handle_start_bh(struct vs_service_device *service);
static void core_handle_reset(struct vs_service_device *service);
static void core_handle_reset_bh(struct vs_service_device *service);
static int core_handle_tx_ready(struct vs_service_device *service);

int __vservice_core_server_register(struct vs_server_core *server,
				    const char *name, struct module *owner)
{
	int ret;
	struct vs_core_server_driver *driver;

	if (server->tx_atomic && !server->rx_atomic)
		return -EINVAL;

	driver = kzalloc(sizeof(*driver), GFP_KERNEL);
	if (!driver) {
		ret = -ENOMEM;
		goto fail_alloc_driver;
	}

	server->driver = &driver->vsdrv;
	driver->server = server;

	driver->vsdrv.protocol = VSERVICE_CORE_PROTOCOL_NAME;

	driver->vsdrv.is_server = true;
	driver->vsdrv.rx_atomic = server->rx_atomic;
	driver->vsdrv.tx_atomic = server->tx_atomic;
	/* FIXME Jira ticket SDK-2835 - philipd. */
	driver->vsdrv.in_quota_min = 1;
	driver->vsdrv.in_quota_best = server->in_quota_best ?
	    server->in_quota_best : driver->vsdrv.in_quota_min;
	/* FIXME Jira ticket SDK-2835 - philipd. */
	driver->vsdrv.out_quota_min = 1;
	driver->vsdrv.out_quota_best = server->out_quota_best ?
	    server->out_quota_best : driver->vsdrv.out_quota_min;
	driver->vsdrv.in_notify_count = VSERVICE_CORE_NBIT_IN__COUNT;
	driver->vsdrv.out_notify_count = VSERVICE_CORE_NBIT_OUT__COUNT;

	driver->vsdrv.probe = core_server_probe;
	driver->vsdrv.remove = core_server_remove;
	driver->vsdrv.receive = core_handle_message;
	driver->vsdrv.notify = core_handle_notify;
	driver->vsdrv.start = server->tx_atomic ?
	    core_handle_start_bh : core_handle_start;
	driver->vsdrv.reset = server->tx_atomic ?
	    core_handle_reset_bh : core_handle_reset;
	driver->vsdrv.tx_ready = core_handle_tx_ready;
	driver->vsdrv.out_notify_count = 0;
	driver->vsdrv.in_notify_count = 0;
	driver->vsdrv.driver.name = name;
	driver->vsdrv.driver.owner = owner;
	driver->vsdrv.driver.bus = &vs_server_bus_type;

	ret = driver_register(&driver->vsdrv.driver);

	if (ret) {
		goto fail_driver_register;
	}

	return 0;

 fail_driver_register:
	server->driver = NULL;
	kfree(driver);
 fail_alloc_driver:
	return ret;
}

EXPORT_SYMBOL(__vservice_core_server_register);

int vservice_core_server_unregister(struct vs_server_core *server)
{
	struct vs_core_server_driver *driver;

	if (!server->driver)
		return 0;

	driver = to_server_driver(server->driver);
	driver_unregister(&driver->vsdrv.driver);

	server->driver = NULL;
	kfree(driver);

	return 0;
}

EXPORT_SYMBOL(vservice_core_server_unregister);

static int core_server_probe(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server = to_server_driver(vsdrv)->server;
	struct vs_server_core_state *state;

	state = server->alloc(service);
	if (!state)
		return -ENOMEM;
	else if (IS_ERR(state))
		return PTR_ERR(state);

	state->service = vs_get_service(service);
	state->state = VSERVICE_CORE_PROTOCOL_RESET_STATE;

	dev_set_drvdata(&service->dev, state);

	return 0;
}

static int core_server_remove(struct vs_service_device *service)
{
	struct vs_server_core_state *state = dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server = to_server_driver(vsdrv)->server;

	state->released = true;
	dev_set_drvdata(&service->dev, NULL);
	server->release(state);

	vs_put_service(service);

	return 0;
}

static int core_handle_tx_ready(struct vs_service_device *service)
{
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	struct vs_server_core *server = to_server_driver(vsdrv)->server;
	struct vs_server_core_state *state = dev_get_drvdata(&service->dev);

	if (server->tx_ready)
		server->tx_ready(state);

	return 0;
}

struct vs_mbuf *vs_server_core_core_alloc_service_created(struct
							  vs_server_core_state
							  *_state,
							  struct vs_string
							  *service_name,
							  struct vs_string
							  *protocol_name,
							  gfp_t flags)
{
	struct vs_mbuf *_mbuf;
	const vs_message_id_t _msg_id = VSERVICE_CORE_CORE_MSG_SERVICE_CREATED;
	const uint32_t _msg_size =
	    sizeof(vs_message_id_t) + VSERVICE_CORE_SERVICE_NAME_SIZE +
	    VSERVICE_CORE_PROTOCOL_NAME_SIZE + 4UL;
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

	if (!service_name)
		goto fail;
	service_name->ptr =
	    (char *)(VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL);
	service_name->max_size = VSERVICE_CORE_SERVICE_NAME_SIZE;
	if (!protocol_name)
		goto fail;
	protocol_name->ptr =
	    (char *)(VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) +
		     VSERVICE_CORE_SERVICE_NAME_SIZE + 4UL);
	protocol_name->max_size = VSERVICE_CORE_PROTOCOL_NAME_SIZE;

	return _mbuf;

 fail:
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	return NULL;
}

EXPORT_SYMBOL(vs_server_core_core_alloc_service_created);
int vs_server_core_core_free_service_created(struct vs_server_core_state
					     *_state,
					     struct vs_string *service_name,
					     struct vs_string *protocol_name,
					     struct vs_mbuf *_mbuf)
{
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_free_service_created);
int
vs_server_core_core_send_ack_connect(struct vs_server_core_state *_state,
				     gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_ACK_CONNECT;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.core.statenum = VSERVICE_CORE_STATE_CONNECTED;

	if (_server->core.state_change)
		_server->core.state_change(_state,
					   VSERVICE_CORE_STATE_DISCONNECTED__CONNECT,
					   VSERVICE_CORE_STATE_CONNECTED);

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_send_ack_connect);
int
vs_server_core_core_send_nack_connect(struct vs_server_core_state *_state,
				      gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_NACK_CONNECT;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED;

	if (_server->core.state_change)
		_server->core.state_change(_state,
					   VSERVICE_CORE_STATE_DISCONNECTED__CONNECT,
					   VSERVICE_CORE_STATE_DISCONNECTED);

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_send_nack_connect);
int
vs_server_core_core_send_ack_disconnect(struct vs_server_core_state *_state,
					gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_ACK_DISCONNECT;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED;

	if (_server->core.state_change)
		_server->core.state_change(_state,
					   VSERVICE_CORE_STATE_CONNECTED__DISCONNECT,
					   VSERVICE_CORE_STATE_DISCONNECTED);

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_send_ack_disconnect);
int
vs_server_core_core_send_nack_disconnect(struct vs_server_core_state *_state,
					 gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_NACK_DISCONNECT;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.core.statenum = VSERVICE_CORE_STATE_CONNECTED;

	if (_server->core.state_change)
		_server->core.state_change(_state,
					   VSERVICE_CORE_STATE_CONNECTED__DISCONNECT,
					   VSERVICE_CORE_STATE_CONNECTED);

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_send_nack_disconnect);
static int
vs_server_core_core_handle_req_connect(const struct vs_server_core *_server,
				       struct vs_server_core_state *_state,
				       struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

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
	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED__CONNECT;

	if (_server->core.state_change)
		_server->core.state_change(_state,
					   VSERVICE_CORE_STATE_DISCONNECTED,
					   VSERVICE_CORE_STATE_DISCONNECTED__CONNECT);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_server->core.req_connect)
		return _server->core.req_connect(_state);
	else
		dev_warn(&_state->service->dev,
			 "[%s:%d] Protocol warning: No handler registered for _server->core.req_connect, command will never be acknowledged\n",
			 __func__, __LINE__);
	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_handle_req_connect);
static int
vs_server_core_core_handle_req_disconnect(const struct vs_server_core *_server,
					  struct vs_server_core_state *_state,
					  struct vs_mbuf *_mbuf)
{
	const size_t _expected_size = sizeof(vs_message_id_t) + 0UL;

	if (VS_MBUF_SIZE(_mbuf) < _expected_size)
		return -EBADMSG;

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
	_state->state.core.statenum = VSERVICE_CORE_STATE_CONNECTED__DISCONNECT;

	if (_server->core.state_change)
		_server->core.state_change(_state,
					   VSERVICE_CORE_STATE_CONNECTED,
					   VSERVICE_CORE_STATE_CONNECTED__DISCONNECT);
	vs_service_free_mbuf(VS_STATE_SERVICE_PTR(_state), _mbuf);
	if (_server->core.req_disconnect)
		return _server->core.req_disconnect(_state);
	else
		dev_warn(&_state->service->dev,
			 "[%s:%d] Protocol warning: No handler registered for _server->core.req_disconnect, command will never be acknowledged\n",
			 __func__, __LINE__);
	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_handle_req_disconnect);
int
vs_server_core_core_send_startup(struct vs_server_core_state *_state,
				 uint32_t core_in_quota,
				 uint32_t core_out_quota, gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 8UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_MSG_STARTUP;

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    core_in_quota;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL) =
	    core_out_quota;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	_state->state.core.statenum = VSERVICE_CORE_STATE_DISCONNECTED;

	if (_server->core.state_change)
		_server->core.state_change(_state, VSERVICE_CORE_STATE_OFFLINE,
					   VSERVICE_CORE_STATE_DISCONNECTED);

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_send_startup);
int
vs_server_core_core_send_shutdown(struct vs_server_core_state *_state,
				  gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 0UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_MSG_SHUTDOWN;

	{
		int err = vs_service_send(VS_STATE_SERVICE_PTR(_state), _mbuf);
		if (err) {
			dev_warn(&_state->service->dev,
				 "[%s:%d] Protocol warning: Error %d sending message on transport.\n",
				 __func__, __LINE__, err);

			return err;
		}
	}

	switch (_state->state.core.statenum) {
	case VSERVICE_CORE_STATE_DISCONNECTED:
		_state->state.core.statenum = VSERVICE_CORE_STATE_OFFLINE;

		if (_server->core.state_change)
			_server->core.state_change(_state,
						   VSERVICE_CORE_STATE_DISCONNECTED,
						   VSERVICE_CORE_STATE_OFFLINE);
		break;
	case VSERVICE_CORE_STATE_CONNECTED:
		_state->state.core.statenum = VSERVICE_CORE_STATE_OFFLINE;

		if (_server->core.state_change)
			_server->core.state_change(_state,
						   VSERVICE_CORE_STATE_CONNECTED,
						   VSERVICE_CORE_STATE_OFFLINE);
		break;

	default:
		break;
	}

	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_send_shutdown);
int
vs_server_core_core_send_service_created(struct vs_server_core_state *_state,
					 uint32_t service_id,
					 struct vs_string service_name,
					 struct vs_string protocol_name,
					 struct vs_mbuf *_mbuf)
{

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	if (*(vs_message_id_t *) (VS_MBUF_DATA(_mbuf)) !=
	    VSERVICE_CORE_CORE_MSG_SERVICE_CREATED)

		return -EINVAL;

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    service_id;
	{
		size_t _size = strnlen(service_name.ptr, service_name.max_size);
		if ((_size + sizeof(vs_message_id_t) + 4UL) >
		    VS_MBUF_SIZE(_mbuf))
			return -EINVAL;

		memset(service_name.ptr + _size, 0,
		       service_name.max_size - _size);
	}
	{
		size_t _size =
		    strnlen(protocol_name.ptr, protocol_name.max_size);
		if ((_size + sizeof(vs_message_id_t) +
		     VSERVICE_CORE_SERVICE_NAME_SIZE + 4UL) >
		    VS_MBUF_SIZE(_mbuf))
			return -EINVAL;

		if (_size < protocol_name.max_size)
			VS_MBUF_SIZE(_mbuf) -= (protocol_name.max_size - _size);

	}

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

EXPORT_SYMBOL(vs_server_core_core_send_service_created);
int
vs_server_core_core_send_service_removed(struct vs_server_core_state *_state,
					 uint32_t service_id, gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 4UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_MSG_SERVICE_REMOVED;

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

EXPORT_SYMBOL(vs_server_core_core_send_service_removed);
int
vs_server_core_core_send_server_ready(struct vs_server_core_state *_state,
				      uint32_t service_id, uint32_t in_quota,
				      uint32_t out_quota,
				      uint32_t in_bit_offset,
				      uint32_t in_num_bits,
				      uint32_t out_bit_offset,
				      uint32_t out_num_bits, gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 28UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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
	    VSERVICE_CORE_CORE_MSG_SERVER_READY;

	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 0UL) =
	    service_id;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 4UL) =
	    in_quota;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 8UL) =
	    out_quota;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 12UL) =
	    in_bit_offset;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 16UL) =
	    in_num_bits;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 20UL) =
	    out_bit_offset;
	*(uint32_t *) (VS_MBUF_DATA(_mbuf) + sizeof(vs_message_id_t) + 24UL) =
	    out_num_bits;

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

EXPORT_SYMBOL(vs_server_core_core_send_server_ready);
int
vs_server_core_core_send_service_reset(struct vs_server_core_state *_state,
				       uint32_t service_id, gfp_t flags)
{
	struct vs_mbuf *_mbuf;

	const size_t _msg_size = sizeof(vs_message_id_t) + 4UL;

	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(VS_STATE_SERVICE_PTR(_state)->dev.driver);
	__maybe_unused struct vs_server_core *_server =
	    to_server_driver(vsdrv)->server;

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

EXPORT_SYMBOL(vs_server_core_core_send_service_reset);
static int
vs_server_core_core_handle_service_reset(const struct vs_server_core *_server,
					 struct vs_server_core_state *_state,
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
	if (_server->core.msg_service_reset)
		return _server->core.msg_service_reset(_state, service_id);
	return 0;
	return 0;
}

EXPORT_SYMBOL(vs_server_core_core_handle_service_reset);
static int
core_handle_message(struct vs_service_device *service, struct vs_mbuf *_mbuf)
{
	vs_message_id_t message_id;
	__maybe_unused struct vs_server_core_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_server_core *server =
	    to_server_driver(vsdrv)->server;

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
	case VSERVICE_CORE_CORE_REQ_CONNECT:
		ret =
		    vs_server_core_core_handle_req_connect(server, state,
							   _mbuf);
		break;

/* command in sync disconnect */
	case VSERVICE_CORE_CORE_REQ_DISCONNECT:
		ret =
		    vs_server_core_core_handle_req_disconnect(server, state,
							      _mbuf);
		break;

/* message service_reset */
	case VSERVICE_CORE_CORE_MSG_SERVICE_RESET:
		ret =
		    vs_server_core_core_handle_service_reset(server, state,
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
	__maybe_unused struct vs_server_core_state *state =
	    dev_get_drvdata(&service->dev);
	struct vs_service_driver *vsdrv =
	    to_vs_service_driver(service->dev.driver);
	__maybe_unused struct vs_server_core *server =
	    to_server_driver(vsdrv)->server;

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

MODULE_DESCRIPTION("OKL4 Virtual Services coreServer Protocol Driver");
MODULE_AUTHOR("Open Kernel Labs, Inc");
