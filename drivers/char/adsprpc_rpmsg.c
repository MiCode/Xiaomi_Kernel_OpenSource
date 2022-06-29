// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/rpmsg.h>
#include <linux/of.h>
#include <linux/module.h>
#include <trace/events/fastrpc.h>
#include <trace/events/rproc_qcom.h>
#include "adsprpc_shared.h"

struct frpc_transport_session_control {
	struct rpmsg_device *rpdev;
	struct mutex rpmsg_mutex;
	char *subsys;
	/* Flags for DSP up mutex */
	wait_queue_head_t wait_for_rpmsg_ch;
	atomic_t is_rpmsg_ch_up;
};

static struct frpc_transport_session_control rpmsg_session_control[NUM_CHANNELS];

inline int verify_transport_device(int cid, bool trusted_vm)
{
	int err = 0;
	struct frpc_transport_session_control *rpmsg_session = &rpmsg_session_control[cid];

	mutex_lock(&rpmsg_session->rpmsg_mutex);
	VERIFY(err, NULL != rpmsg_session->rpdev);
	if (err) {
		err = -ENODEV;
		mutex_unlock(&rpmsg_session->rpmsg_mutex);
		goto bail;
	}
	mutex_unlock(&rpmsg_session->rpmsg_mutex);
bail:
	return err;
}

static inline int get_cid_from_rpdev(struct rpmsg_device *rpdev)
{
	int err = 0, cid = -1;
	const char *label = 0;

	VERIFY(err, !IS_ERR_OR_NULL(rpdev));
	if (err)
		return -ENODEV;

	err = of_property_read_string(rpdev->dev.parent->of_node, "label",
					&label);

	if (err)
		label = rpdev->dev.parent->of_node->name;

	if (!strcmp(label, "cdsp"))
		cid = CDSP_DOMAIN_ID;
	else if (!strcmp(label, "adsp"))
		cid = ADSP_DOMAIN_ID;
	else if (!strcmp(label, "slpi"))
		cid = SDSP_DOMAIN_ID;
	else if (!strcmp(label, "mdsp"))
		cid = MDSP_DOMAIN_ID;

	return cid;
}

static int fastrpc_rpmsg_probe(struct rpmsg_device *rpdev)
{
	int err = 0;
	int cid = -1;
	struct frpc_transport_session_control *transport_session_control = NULL;

	VERIFY(err, !IS_ERR_OR_NULL(rpdev));
	if (err)
		return -ENODEV;

	cid = get_cid_from_rpdev(rpdev);
	VERIFY(err, VALID_FASTRPC_CID(cid));
	if (err) {
		err = -ECHRNG;
		goto bail;
	}
	transport_session_control = &rpmsg_session_control[cid];
	mutex_lock(&transport_session_control->rpmsg_mutex);
	transport_session_control->rpdev = rpdev;
	mutex_unlock(&transport_session_control->rpmsg_mutex);

	/*
	 * Set atomic variable to 1 when rpmsg channel is up
	 * and wake up all threads waiting for rpmsg channel
	 */
	atomic_set(&transport_session_control->is_rpmsg_ch_up, 1);
	wake_up_interruptible(&transport_session_control->wait_for_rpmsg_ch);

	ADSPRPC_INFO("opened rpmsg channel for %s\n",
		rpmsg_session_control[cid].subsys);
bail:
	if (err)
		ADSPRPC_ERR("rpmsg probe of %s cid %d failed\n",
			rpdev->dev.parent->of_node->name, cid);
	return err;
}

static void fastrpc_rpmsg_remove(struct rpmsg_device *rpdev)
{
	int err = 0;
	int cid = -1;
	struct frpc_transport_session_control *transport_session_control = NULL;

	VERIFY(err, !IS_ERR_OR_NULL(rpdev));
	if (err) {
		err = -ENODEV;
		return;
	}

	cid = get_cid_from_rpdev(rpdev);
	VERIFY(err, VALID_FASTRPC_CID(cid));
	if (err) {
		err = -ECHRNG;
		goto bail;
	}
	transport_session_control = &rpmsg_session_control[cid];
	mutex_lock(&transport_session_control->rpmsg_mutex);
	transport_session_control->rpdev = NULL;
	mutex_unlock(&transport_session_control->rpmsg_mutex);

	/*
	 * Set atomic variable to 0 when rpmsg channel is down and
	 * make threads wait on is_rpmsg_ch_up
	 */
	atomic_set(&transport_session_control->is_rpmsg_ch_up, 0);

	ADSPRPC_INFO("closed rpmsg channel of %s\n",
		rpmsg_session_control[cid].subsys);
bail:
	if (err)
		ADSPRPC_ERR("rpmsg remove of %s cid %d failed\n",
			rpdev->dev.parent->of_node->name, cid);
}

static int fastrpc_rpmsg_callback(struct rpmsg_device *rpdev, void *data,
	int len, void *priv, u32 addr)
{
	int err = 0;
	int rpmsg_err = 0;
	int cid = -1;

	trace_fastrpc_msg("rpmsg_callback: begin");
	cid = get_cid_from_rpdev(rpdev);
	VERIFY(err, VALID_FASTRPC_CID(cid));
	if (err) {
		err = -ECHRNG;
		goto bail;
	}

	rpmsg_err = fastrpc_handle_rpc_response(data, len, cid);
bail:
	if (err) {
		err = -ENOKEY;
		ADSPRPC_ERR(
			"invalid response data %pK, len %d from remote subsystem err %d\n",
			data, len, err);
	} else
		err = rpmsg_err;

	trace_fastrpc_msg("rpmsg_callback: end");
	return err;
}

/*
 * This function is called from fastrpc_channel open to wait
 * for rpmsg channel in the respective domain. The wait in this
 * function is done only for CDSP, Audio and Sensors Daemons.
 */
int fastrpc_wait_for_transport_interrupt(int cid,
					unsigned int flags)
{
	struct frpc_transport_session_control *transport_session_control = NULL;
	int err = 0;

	/*
	 * The flags which are applicable only for daemons are checked.
	 * Dynamic PDs will fail and return immediately if the
	 * remote subsystem is not up.
	 */
	if (flags == FASTRPC_INIT_ATTACH || flags == FASTRPC_INIT_ATTACH_SENSORS
		|| flags == FASTRPC_INIT_CREATE_STATIC) {
		transport_session_control = &rpmsg_session_control[cid];
		ADSPRPC_DEBUG("Thread waiting for cid %d rpmsg channel", cid);
		err = wait_event_interruptible(transport_session_control->wait_for_rpmsg_ch,
				atomic_read(&transport_session_control->is_rpmsg_ch_up));
		ADSPRPC_DEBUG("Thread received signal for cid %d rpmsg channel (interrupted %d)",
			cid, err);
	}

	return err;
}

int fastrpc_transport_send(int cid, void *rpc_msg, uint32_t rpc_msg_size, bool trusted_vm)
{
	int err = 0;
	struct frpc_transport_session_control *rpmsg_session = &rpmsg_session_control[cid];

	mutex_lock(&rpmsg_session->rpmsg_mutex);
	VERIFY(err, !IS_ERR_OR_NULL(rpmsg_session->rpdev));
	if (err) {
		err = -ENODEV;
		ADSPRPC_ERR("No rpmsg device for %s, err %d\n", current->comm, err);
		mutex_unlock(&rpmsg_session->rpmsg_mutex);
		goto bail;
	}
	err = rpmsg_send(rpmsg_session->rpdev->ept, rpc_msg, rpc_msg_size);
	mutex_unlock(&rpmsg_session->rpmsg_mutex);
bail:
	return err;
}

static const struct rpmsg_device_id fastrpc_rpmsg_match[] = {
	{ FASTRPC_GLINK_GUID },
	{ },
};

static const struct of_device_id fastrpc_rpmsg_of_match[] = {
	{ .compatible = "qcom,msm-fastrpc-rpmsg" },
	{ },
};
MODULE_DEVICE_TABLE(of, fastrpc_rpmsg_of_match);

static struct rpmsg_driver fastrpc_rpmsg_client = {
	.id_table = fastrpc_rpmsg_match,
	.probe = fastrpc_rpmsg_probe,
	.remove = fastrpc_rpmsg_remove,
	.callback = fastrpc_rpmsg_callback,
	.drv = {
		.name = "qcom,msm_fastrpc_rpmsg",
		.of_match_table = fastrpc_rpmsg_of_match,
	},
};

void fastrpc_rproc_trace_events(const char *name, const char *event,
				const char *subevent)
{
	trace_rproc_qcom_event(name, event, subevent);
}

inline void fastrpc_transport_session_init(int cid, char *subsys)
{
	rpmsg_session_control[cid].subsys = subsys;
	mutex_init(&rpmsg_session_control[cid].rpmsg_mutex);
	init_waitqueue_head(&rpmsg_session_control[cid].wait_for_rpmsg_ch);
}

inline void fastrpc_transport_session_deinit(int cid)
{
	mutex_destroy(&rpmsg_session_control[cid].rpmsg_mutex);
}

int fastrpc_transport_init(void)
{
	int err = 0;

	err = register_rpmsg_driver(&fastrpc_rpmsg_client);
	if (err) {
		pr_err("Error: adsprpc: %s: register_rpmsg_driver failed with err %d\n",
			__func__, err);
		goto bail;
	}
bail:
	return err;
}

void fastrpc_transport_deinit(void)
{
	unregister_rpmsg_driver(&fastrpc_rpmsg_client);
}
