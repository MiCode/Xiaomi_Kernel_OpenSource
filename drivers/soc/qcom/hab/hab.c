/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "hab.h"

#define HAB_DEVICE_CNSTR(__name__, __id__, __num__) { \
	.name = __name__,\
	.id = __id__,\
	.pchannels = LIST_HEAD_INIT(hab_devices[__num__].pchannels),\
	.pchan_lock = __MUTEX_INITIALIZER(hab_devices[__num__].pchan_lock),\
	.openq_list = LIST_HEAD_INIT(hab_devices[__num__].openq_list),\
	.openlock = __SPIN_LOCK_UNLOCKED(&hab_devices[__num__].openlock)\
	}

/*
 * The following has to match habmm definitions, order does not matter if
 * hab config does not care either. When hab config is not present, the default
 * is as guest VM all pchans are pchan opener (FE)
 */
static struct hab_device hab_devices[] = {
	HAB_DEVICE_CNSTR(DEVICE_AUD1_NAME, MM_AUD_1, 0),
	HAB_DEVICE_CNSTR(DEVICE_AUD2_NAME, MM_AUD_2, 1),
	HAB_DEVICE_CNSTR(DEVICE_AUD3_NAME, MM_AUD_3, 2),
	HAB_DEVICE_CNSTR(DEVICE_AUD4_NAME, MM_AUD_4, 3),
	HAB_DEVICE_CNSTR(DEVICE_CAM1_NAME, MM_CAM_1, 4),
	HAB_DEVICE_CNSTR(DEVICE_CAM2_NAME, MM_CAM_2, 5),
	HAB_DEVICE_CNSTR(DEVICE_DISP1_NAME, MM_DISP_1, 6),
	HAB_DEVICE_CNSTR(DEVICE_DISP2_NAME, MM_DISP_2, 7),
	HAB_DEVICE_CNSTR(DEVICE_DISP3_NAME, MM_DISP_3, 8),
	HAB_DEVICE_CNSTR(DEVICE_DISP4_NAME, MM_DISP_4, 9),
	HAB_DEVICE_CNSTR(DEVICE_DISP5_NAME, MM_DISP_5, 10),
	HAB_DEVICE_CNSTR(DEVICE_GFX_NAME, MM_GFX, 11),
	HAB_DEVICE_CNSTR(DEVICE_VID_NAME, MM_VID, 12),
	HAB_DEVICE_CNSTR(DEVICE_MISC_NAME, MM_MISC, 13),
	HAB_DEVICE_CNSTR(DEVICE_QCPE1_NAME, MM_QCPE_VM1, 14),
	HAB_DEVICE_CNSTR(DEVICE_QCPE2_NAME, MM_QCPE_VM2, 15),
	HAB_DEVICE_CNSTR(DEVICE_QCPE3_NAME, MM_QCPE_VM3, 16),
	HAB_DEVICE_CNSTR(DEVICE_QCPE4_NAME, MM_QCPE_VM4, 17),
	HAB_DEVICE_CNSTR(DEVICE_CLK1_NAME, MM_CLK_VM1, 18),
	HAB_DEVICE_CNSTR(DEVICE_CLK2_NAME, MM_CLK_VM2, 19),
	HAB_DEVICE_CNSTR(DEVICE_FDE1_NAME, MM_FDE_1, 20),
	HAB_DEVICE_CNSTR(DEVICE_BUFFERQ1_NAME, MM_BUFFERQ_1, 21),
};

struct hab_driver hab_driver = {
	.ndevices = ARRAY_SIZE(hab_devices),
	.devp = hab_devices,
};

struct uhab_context *hab_ctx_alloc(int kernel)
{
	struct uhab_context *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->closing = 0;
	INIT_LIST_HEAD(&ctx->vchannels);
	INIT_LIST_HEAD(&ctx->exp_whse);
	INIT_LIST_HEAD(&ctx->imp_whse);

	INIT_LIST_HEAD(&ctx->exp_rxq);
	init_waitqueue_head(&ctx->exp_wq);
	spin_lock_init(&ctx->expq_lock);

	spin_lock_init(&ctx->imp_lock);
	rwlock_init(&ctx->exp_lock);
	rwlock_init(&ctx->ctx_lock);

	kref_init(&ctx->refcount);
	ctx->import_ctx = habmem_imp_hyp_open();
	if (!ctx->import_ctx) {
		pr_err("habmem_imp_hyp_open failed\n");
		kfree(ctx);
		return NULL;
	}
	ctx->kernel = kernel;

	return ctx;
}

void hab_ctx_free(struct kref *ref)
{
	struct uhab_context *ctx =
		container_of(ref, struct uhab_context, refcount);
	struct hab_export_ack_recvd *ack_recvd, *tmp;

	habmem_imp_hyp_close(ctx->import_ctx, ctx->kernel);

	list_for_each_entry_safe(ack_recvd, tmp, &ctx->exp_rxq, node) {
		list_del(&ack_recvd->node);
		kfree(ack_recvd);
	}

	kfree(ctx);
}

struct virtual_channel *hab_get_vchan_fromvcid(int32_t vcid,
		struct uhab_context *ctx)
{
	struct virtual_channel *vchan;

	read_lock(&ctx->ctx_lock);
	list_for_each_entry(vchan, &ctx->vchannels, node) {
		if (vcid == vchan->id) {
			kref_get(&vchan->refcount);
			read_unlock(&ctx->ctx_lock);
			return vchan;
		}
	}
	read_unlock(&ctx->ctx_lock);
	return NULL;
}

static struct hab_device *find_hab_device(unsigned int mm_id)
{
	int i;

	for (i = 0; i < hab_driver.ndevices; i++) {
		if (hab_driver.devp[i].id == HAB_MMID_GET_MAJOR(mm_id))
			return &hab_driver.devp[i];
	}

	pr_err("find_hab_device failed: id=%d\n", mm_id);
	return NULL;
}
/*
 *   open handshake in FE and BE

 *   frontend            backend
 *  send(INIT)          wait(INIT)
 *  wait(INIT_ACK)      send(INIT_ACK)
 *  send(ACK)           wait(ACK)

 */
struct virtual_channel *frontend_open(struct uhab_context *ctx,
		unsigned int mm_id,
		int dom_id)
{
	int ret, open_id = 0;
	struct physical_channel *pchan = NULL;
	struct hab_device *dev;
	struct virtual_channel *vchan = NULL;
	static atomic_t open_id_counter = ATOMIC_INIT(0);
	struct hab_open_request request;
	struct hab_open_request *recv_request;
	int sub_id = HAB_MMID_GET_MINOR(mm_id);

	dev = find_hab_device(mm_id);
	if (dev == NULL) {
		pr_err("HAB device %d is not initialized\n", mm_id);
		ret = -EINVAL;
		goto err;
	}

	pchan = hab_pchan_find_domid(dev, dom_id);
	if (!pchan) {
		pr_err("hab_pchan_find_domid failed: dom_id=%d\n", dom_id);
		ret = -EINVAL;
		goto err;
	}

	vchan = hab_vchan_alloc(ctx, pchan);
	if (!vchan) {
		pr_err("vchan alloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	/* Send Init sequence */
	open_id = atomic_inc_return(&open_id_counter);
	hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT, pchan,
		vchan->id, sub_id, open_id);
	ret = hab_open_request_send(&request);
	if (ret) {
		pr_err("hab_open_request_send failed: %d\n", ret);
		goto err;
	}

	/* Wait for Init-Ack sequence */
	hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT_ACK, pchan,
		0, sub_id, open_id);
	ret = hab_open_listen(ctx, dev, &request, &recv_request, 0);
	if (ret || !recv_request) {
		pr_err("hab_open_listen failed: %d\n", ret);
		goto err;
	}

	vchan->otherend_id = recv_request->vchan_id;
	hab_open_request_free(recv_request);

	vchan->session_id = open_id;
	pr_debug("vchan->session_id:%d\n", vchan->session_id);

	/* Send Ack sequence */
	hab_open_request_init(&request, HAB_PAYLOAD_TYPE_ACK, pchan,
		0, sub_id, open_id);
	ret = hab_open_request_send(&request);
	if (ret)
		goto err;

	hab_pchan_put(pchan);

	return vchan;
err:
	if (vchan)
		hab_vchan_put(vchan);
	if (pchan)
		hab_pchan_put(pchan);

	return ERR_PTR(ret);
}

struct virtual_channel *backend_listen(struct uhab_context *ctx,
		unsigned int mm_id)
{
	int ret;
	int open_id;
	int sub_id = HAB_MMID_GET_MINOR(mm_id);
	struct physical_channel *pchan = NULL;
	struct hab_device *dev;
	struct virtual_channel *vchan = NULL;
	struct hab_open_request request;
	struct hab_open_request *recv_request;
	uint32_t otherend_vchan_id;

	dev = find_hab_device(mm_id);
	if (dev == NULL) {
		pr_err("failed to find dev based on id %d\n", mm_id);
		ret = -EINVAL;
		goto err;
	}

	while (1) {
		/* Wait for Init sequence */
		hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT,
			NULL, 0, sub_id, 0);
		ret = hab_open_listen(ctx, dev, &request, &recv_request, 0);
		if (ret || !recv_request) {
			pr_err("hab_open_listen failed: %d\n", ret);
			goto err;
		}

		otherend_vchan_id = recv_request->vchan_id;
		open_id = recv_request->open_id;
		pchan = recv_request->pchan;
		hab_pchan_get(pchan);
		hab_open_request_free(recv_request);

		vchan = hab_vchan_alloc(ctx, pchan);
		if (!vchan) {
			ret = -ENOMEM;
			goto err;
		}

		vchan->otherend_id = otherend_vchan_id;

		vchan->session_id = open_id;
		pr_debug("vchan->session_id:%d\n", vchan->session_id);

		/* Send Init-Ack sequence */
		hab_open_request_init(&request, HAB_PAYLOAD_TYPE_INIT_ACK,
				pchan, vchan->id, sub_id, open_id);
		ret = hab_open_request_send(&request);
		if (ret)
			goto err;

		/* Wait for Ack sequence */
		hab_open_request_init(&request, HAB_PAYLOAD_TYPE_ACK,
				pchan, 0, sub_id, open_id);
		ret = hab_open_listen(ctx, dev, &request, &recv_request, 0);

		if (ret != -EAGAIN)
			break;

		hab_vchan_put(vchan);
		vchan = NULL;
		hab_pchan_put(pchan);
		pchan = NULL;
	}

	if (ret || !recv_request) {
		pr_err("backend_listen failed: %d\n", ret);
		ret = -EINVAL;
		goto err;
	}

	hab_open_request_free(recv_request);
	hab_pchan_put(pchan);
	return vchan;
err:
	pr_err("listen on mmid %d failed\n", mm_id);
	if (vchan)
		hab_vchan_put(vchan);
	if (pchan)
		hab_pchan_put(pchan);
	return ERR_PTR(ret);
}

long hab_vchan_send(struct uhab_context *ctx,
		int vcid,
		size_t sizebytes,
		void *data,
		unsigned int flags)
{
	struct virtual_channel *vchan;
	int ret;
	struct hab_header header = HAB_HEADER_INITIALIZER;
	int nonblocking_flag = flags & HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING;

	if (sizebytes > HAB_MAX_MSG_SIZEBYTES) {
		pr_err("Message too large, %lu bytes\n", sizebytes);
		return -EINVAL;
	}

	vchan = hab_get_vchan_fromvcid(vcid, ctx);
	if (!vchan || vchan->otherend_closed) {
		ret = -ENODEV;
		goto err;
	}

	HAB_HEADER_SET_SIZE(header, sizebytes);
	if (flags & HABMM_SOCKET_SEND_FLAGS_XING_VM_STAT)
		HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_PROFILE);
	else
		HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_MSG);

	HAB_HEADER_SET_ID(header, vchan->otherend_id);
	HAB_HEADER_SET_SESSION_ID(header, vchan->session_id);

	while (1) {
		ret = physical_channel_send(vchan->pchan, &header, data);

		if (vchan->otherend_closed || nonblocking_flag ||
			ret != -EAGAIN)
			break;

		schedule();
	}


err:
	if (vchan)
		hab_vchan_put(vchan);

	return ret;
}

struct hab_message *hab_vchan_recv(struct uhab_context *ctx,
				int vcid,
				unsigned int flags)
{
	struct virtual_channel *vchan;
	struct hab_message *message;
	int ret = 0;
	int nonblocking_flag = flags & HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING;

	vchan = hab_get_vchan_fromvcid(vcid, ctx);
	if (!vchan)
		return ERR_PTR(-ENODEV);

	if (nonblocking_flag) {
		/*
		 * Try to pull data from the ring in this context instead of
		 * IRQ handler. Any available messages will be copied and queued
		 * internally, then fetched by hab_msg_dequeue()
		 */
		physical_channel_rx_dispatch((unsigned long) vchan->pchan);
	}

	message = hab_msg_dequeue(vchan, flags);
	if (!message) {
		if (nonblocking_flag)
			ret = -EAGAIN;
		else if (vchan->otherend_closed)
			ret = -ENODEV;
		else
			ret = -EPIPE;
	}

	hab_vchan_put(vchan);
	return ret ? ERR_PTR(ret) : message;
}

bool hab_is_loopback(void)
{
	return hab_driver.b_loopback;
}

int hab_vchan_open(struct uhab_context *ctx,
		unsigned int mmid,
		int32_t *vcid,
		uint32_t flags)
{
	struct virtual_channel *vchan = NULL;
	struct hab_device *dev;

	pr_debug("Open mmid=%d, loopback mode=%d, loopback num=%d\n",
		mmid, hab_driver.b_loopback, hab_driver.loopback_num);

	if (!vcid)
		return -EINVAL;

	if (hab_is_loopback()) {
		if (!hab_driver.loopback_num) {
			hab_driver.loopback_num = 1;
			vchan = backend_listen(ctx, mmid);
		} else {
			hab_driver.loopback_num = 0;
			vchan = frontend_open(ctx, mmid, LOOPBACK_DOM);
		}
	} else {
		dev = find_hab_device(mmid);

		if (dev) {
			struct physical_channel *pchan =
			hab_pchan_find_domid(dev, HABCFG_VMID_DONT_CARE);

			if (pchan->is_be)
				vchan = backend_listen(ctx, mmid);
			else
				vchan = frontend_open(ctx, mmid,
						HABCFG_VMID_DONT_CARE);
		} else {
			pr_err("failed to find device, mmid %d\n", mmid);
		}
	}

	if (IS_ERR(vchan)) {
		pr_err("vchan open failed over mmid=%d\n", mmid);
		return PTR_ERR(vchan);
	}

	pr_debug("vchan id %x, remote id %x\n",
		vchan->id, vchan->otherend_id);

	write_lock(&ctx->ctx_lock);
	list_add_tail(&vchan->node, &ctx->vchannels);
	write_unlock(&ctx->ctx_lock);

	*vcid = vchan->id;

	return 0;
}

void hab_send_close_msg(struct virtual_channel *vchan)
{
	struct hab_header header = {0};

	if (vchan && !vchan->otherend_closed) {
		HAB_HEADER_SET_SIZE(header, 0);
		HAB_HEADER_SET_TYPE(header, HAB_PAYLOAD_TYPE_CLOSE);
		HAB_HEADER_SET_ID(header, vchan->otherend_id);
		HAB_HEADER_SET_SESSION_ID(header, vchan->session_id);
		physical_channel_send(vchan->pchan, &header, NULL);
	}
}

static void hab_vchan_close_impl(struct kref *ref)
{
	struct virtual_channel *vchan =
		container_of(ref, struct virtual_channel, usagecnt);

	list_del(&vchan->node);
	hab_vchan_stop_notify(vchan);
	hab_vchan_put(vchan);
}


void hab_vchan_close(struct uhab_context *ctx, int32_t vcid)
{
	struct virtual_channel *vchan, *tmp;

	if (!ctx)
		return;

	write_lock(&ctx->ctx_lock);
	list_for_each_entry_safe(vchan, tmp, &ctx->vchannels, node) {
		if (vchan->id == vcid) {
			kref_put(&vchan->usagecnt, hab_vchan_close_impl);
			break;
		}
	}

	write_unlock(&ctx->ctx_lock);
}

/*
 * To name the pchan - the pchan has two ends, either FE or BE locally.
 * if is_be is true, then this is listener for BE. pchane name use remote
 * FF's vmid from the table.
 * if is_be is false, then local is FE as opener. pchan name use local FE's
 * vmid (self)
 */
static int hab_initialize_pchan_entry(struct hab_device *mmid_device,
				int vmid_local, int vmid_remote, int is_be)
{
	char pchan_name[MAX_VMID_NAME_SIZE];
	struct physical_channel *pchan = NULL;
	int ret;
	int vmid = is_be ? vmid_remote : vmid_local;

	if (!mmid_device) {
		pr_err("habdev %pK, vmid local %d, remote %d, is be %d\n",
				mmid_device, vmid_local, vmid_remote, is_be);
		return -EINVAL;
	}

	snprintf(pchan_name, MAX_VMID_NAME_SIZE, "vm%d-", vmid);
	strlcat(pchan_name, mmid_device->name, MAX_VMID_NAME_SIZE);

	ret = habhyp_commdev_alloc((void **)&pchan, is_be, pchan_name,
					vmid_remote, mmid_device);
	if (ret) {
		pr_err("failed %d to allocate pchan %s, vmid local %d, remote %d, is_be %d, total %d\n",
				ret, pchan_name, vmid_local, vmid_remote,
				is_be, mmid_device->pchan_cnt);
	} else {
		/* local/remote id setting should be kept in lower level */
		pchan->vmid_local = vmid_local;
		pchan->vmid_remote = vmid_remote;
		pr_debug("pchan %s mmid %s local %d remote %d role %d\n",
				pchan_name, mmid_device->name,
				pchan->vmid_local, pchan->vmid_remote,
				pchan->dom_id);
	}

	return ret;
}

static void hab_generate_pchan(struct local_vmid *settings, int i, int j)
{
	int k, ret = 0;

	pr_debug("%d as mmid %d in vmid %d\n",
			HABCFG_GET_MMID(settings, i, j), j, i);

	switch (HABCFG_GET_MMID(settings, i, j)) {
	case MM_AUD_START/100:
		for (k = MM_AUD_START + 1; k < MM_AUD_END; k++) {
			/*
			 * if this local pchan end is BE, then use
			 * remote FE's vmid. If local end is FE, then
			 * use self vmid
			 */
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_CAM_START/100:
		for (k = MM_CAM_START + 1; k < MM_CAM_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_DISP_START/100:
		for (k = MM_DISP_START + 1; k < MM_DISP_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_GFX_START/100:
		for (k = MM_GFX_START + 1; k < MM_GFX_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_VID_START/100:
		for (k = MM_VID_START + 1; k < MM_VID_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_MISC_START/100:
		for (k = MM_MISC_START + 1; k < MM_MISC_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_QCPE_START/100:
		for (k = MM_QCPE_START + 1; k < MM_QCPE_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;

	case MM_CLK_START/100:
		for (k = MM_CLK_START + 1; k < MM_CLK_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;
	case MM_FDE_START/100:
		for (k = MM_FDE_START + 1; k < MM_FDE_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;
	case MM_BUFFERQ_START/100:
		for (k = MM_BUFFERQ_START + 1; k < MM_BUFFERQ_END; k++) {
			ret += hab_initialize_pchan_entry(
					find_hab_device(k),
					settings->self,
					HABCFG_GET_VMID(settings, i),
					HABCFG_GET_BE(settings, i, j));
		}
		break;
	default:
		pr_err("failed to find mmid %d, i %d, j %d\n",
			HABCFG_GET_MMID(settings, i, j), i, j);

		break;
	}
}

/*
 * generate pchan list based on hab settings table.
 * return status 0: success, otherwise failure
 */
static int hab_generate_pchan_list(struct local_vmid *settings)
{
	int i, j;

	/* scan by valid VMs, then mmid */
	pr_debug("self vmid is %d\n", settings->self);
	for (i = 0; i < HABCFG_VMID_MAX; i++) {
		if (HABCFG_GET_VMID(settings, i) != HABCFG_VMID_INVALID &&
			HABCFG_GET_VMID(settings, i) != settings->self) {
			pr_debug("create pchans for vm %d\n", i);

			for (j = 1; j <= HABCFG_MMID_AREA_MAX; j++) {
				if (HABCFG_GET_MMID(settings, i, j)
						!= HABCFG_VMID_INVALID)
					hab_generate_pchan(settings, i, j);
			}
		}
	}

	return 0;
}

/*
 * This function checks hypervisor plug-in readiness, read in hab configs,
 * and configure pchans
 */
int do_hab_parse(void)
{
	int result;
	int i;
	struct hab_device *device;
	int pchan_total = 0;

	/* first check if hypervisor plug-in is ready */
	result = hab_hypervisor_register();
	if (result) {
		pr_err("register HYP plug-in failed, ret %d\n", result);
		return result;
	}

	/* Initialize open Q before first pchan starts */
	for (i = 0; i < hab_driver.ndevices; i++) {
		device = &hab_driver.devp[i];
		init_waitqueue_head(&device->openq);
	}

	/* read in hab config and create pchans*/
	memset(&hab_driver.settings, HABCFG_VMID_INVALID,
				sizeof(hab_driver.settings));

	result = hab_parse(&hab_driver.settings);
	if (result) {
		pr_warn("hab_parse failed and use the default settings\n");
		fill_default_gvm_settings(&hab_driver.settings, 2,
					MM_AUD_START, MM_ID_MAX);
	}

	/* now generate hab pchan list */
	result  = hab_generate_pchan_list(&hab_driver.settings);
	if (result) {
		pr_err("generate pchan list failed, ret %d\n", result);
	} else {
		for (i = 0; i < hab_driver.ndevices; i++) {
			device = &hab_driver.devp[i];
			pchan_total += device->pchan_cnt;
		}
		pr_debug("ret %d, total %d pchans added, ndevices %d\n",
				 result, pchan_total, hab_driver.ndevices);
	}

	return result;
}

static int hab_open(struct inode *inodep, struct file *filep)
{
	int result = 0;
	struct uhab_context *ctx;

	ctx = hab_ctx_alloc(0);

	if (!ctx) {
		pr_err("hab_ctx_alloc failed\n");
		filep->private_data = NULL;
		return -ENOMEM;
	}

	filep->private_data = ctx;

	return result;
}

static int hab_release(struct inode *inodep, struct file *filep)
{
	struct uhab_context *ctx = filep->private_data;
	struct virtual_channel *vchan, *tmp;

	if (!ctx)
		return 0;

	pr_debug("inode %pK, filep %pK\n", inodep, filep);

	write_lock(&ctx->ctx_lock);

	list_for_each_entry_safe(vchan, tmp, &ctx->vchannels, node) {
		list_del(&vchan->node);
		hab_vchan_stop_notify(vchan);
		hab_vchan_put(vchan);
	}

	write_unlock(&ctx->ctx_lock);

	hab_ctx_put(ctx);
	filep->private_data = NULL;

	return 0;
}

static long hab_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	struct uhab_context *ctx = (struct uhab_context *)filep->private_data;
	struct hab_open *open_param;
	struct hab_close *close_param;
	struct hab_recv *recv_param;
	struct hab_send *send_param;
	struct hab_info *info_param;
	struct hab_message *msg;
	void *send_data;
	unsigned char data[256] = { 0 };
	long ret = 0;
	char names[30];

	if (_IOC_SIZE(cmd) && (cmd & IOC_IN)) {
		if (_IOC_SIZE(cmd) > sizeof(data))
			return -EINVAL;

		if (copy_from_user(data, (void __user *)arg, _IOC_SIZE(cmd))) {
			pr_err("copy_from_user failed cmd=%x size=%d\n",
				cmd, _IOC_SIZE(cmd));
			return -EFAULT;
		}
	}

	switch (cmd) {
	case IOCTL_HAB_VC_OPEN:
		open_param = (struct hab_open *)data;
		ret = hab_vchan_open(ctx, open_param->mmid,
			&open_param->vcid, open_param->flags);
		break;
	case IOCTL_HAB_VC_CLOSE:
		close_param = (struct hab_close *)data;
		hab_vchan_close(ctx, close_param->vcid);
		break;
	case IOCTL_HAB_SEND:
		send_param = (struct hab_send *)data;
		if (send_param->sizebytes > HAB_MAX_MSG_SIZEBYTES) {
			ret = -EINVAL;
			break;
		}

		send_data = kzalloc(send_param->sizebytes, GFP_TEMPORARY);
		if (!send_data) {
			ret = -ENOMEM;
			break;
		}

		if (copy_from_user(send_data, (void __user *)send_param->data,
				send_param->sizebytes)) {
			ret = -EFAULT;
		} else {
			ret = hab_vchan_send(ctx, send_param->vcid,
						send_param->sizebytes,
						send_data,
						send_param->flags);
		}
		kfree(send_data);
		break;
	case IOCTL_HAB_RECV:
		recv_param = (struct hab_recv *)data;
		if (!recv_param->data) {
			ret = -EINVAL;
			break;
		}

		msg = hab_vchan_recv(ctx, recv_param->vcid, recv_param->flags);

		if (IS_ERR(msg)) {
			recv_param->sizebytes = 0;
			ret = PTR_ERR(msg);
			break;
		}

		if (recv_param->sizebytes < msg->sizebytes) {
			recv_param->sizebytes = 0;
			ret = -EINVAL;
		} else if (copy_to_user((void __user *)recv_param->data,
					msg->data,
					msg->sizebytes)) {
			pr_err("copy_to_user failed: vc=%x size=%d\n",
				recv_param->vcid, (int)msg->sizebytes);
			recv_param->sizebytes = 0;
			ret = -EFAULT;
		} else {
			recv_param->sizebytes = msg->sizebytes;
		}

		hab_msg_free(msg);
		break;
	case IOCTL_HAB_VC_EXPORT:
		ret = hab_mem_export(ctx, (struct hab_export *)data, 0);
		break;
	case IOCTL_HAB_VC_IMPORT:
		ret = hab_mem_import(ctx, (struct hab_import *)data, 0);
		break;
	case IOCTL_HAB_VC_UNEXPORT:
		ret = hab_mem_unexport(ctx, (struct hab_unexport *)data, 0);
		break;
	case IOCTL_HAB_VC_UNIMPORT:
		ret = hab_mem_unimport(ctx, (struct hab_unimport *)data, 0);
		break;
	case IOCTL_HAB_VC_QUERY:
		info_param = (struct hab_info *)data;
		if (!info_param->names || !info_param->namesize ||
			info_param->namesize > sizeof(names)) {
			pr_err("wrong vm info vcid %X, names %llX, sz %d\n",
				info_param->vcid, info_param->names,
				info_param->namesize);
			ret = -EINVAL;
			break;
		}
		ret = hab_vchan_query(ctx, info_param->vcid,
				(uint64_t *)&info_param->ids,
				names, info_param->namesize, 0);
		if (!ret) {
			if (copy_to_user((void __user *)info_param->names,
						 names,
						 info_param->namesize)) {
				pr_err("copy_to_user failed: vc=%x size=%d\n",
					info_param->vcid,
					info_param->namesize*2);
				info_param->namesize = 0;
				ret = -EFAULT;
			}
		}
		break;
	default:
		ret = -ENOIOCTLCMD;
	}

	if (ret == 0 && _IOC_SIZE(cmd) && (cmd & IOC_OUT))
		if (copy_to_user((void __user *) arg, data, _IOC_SIZE(cmd))) {
			pr_err("copy_to_user failed: cmd=%x\n", cmd);
			ret = -EFAULT;
		}

	return ret;
}

static long hab_compat_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	return hab_ioctl(filep, cmd, arg);
}

static const struct file_operations hab_fops = {
	.owner = THIS_MODULE,
	.open = hab_open,
	.release = hab_release,
	.mmap = habmem_imp_hyp_mmap,
	.unlocked_ioctl = hab_ioctl,
	.compat_ioctl = hab_compat_ioctl
};

/*
 * These map sg functions are pass through because the memory backing the
 * sg list is already accessible to the kernel as they come from a the
 * dedicated shared vm pool
 */

static int hab_map_sg(struct device *dev, struct scatterlist *sgl,
	int nelems, enum dma_data_direction dir,
	struct dma_attrs *attrs)
{
	/* return nelems directly */
	return nelems;
}

static void hab_unmap_sg(struct device *dev,
	struct scatterlist *sgl, int nelems,
	enum dma_data_direction dir,
	struct dma_attrs *attrs)
{
	/*Do nothing */
}

static const struct dma_map_ops hab_dma_ops = {
	.map_sg		= hab_map_sg,
	.unmap_sg	= hab_unmap_sg,
};

static int __init hab_init(void)
{
	int result;
	dev_t dev;

	result = alloc_chrdev_region(&hab_driver.major, 0, 1, "hab");

	if (result < 0) {
		pr_err("alloc_chrdev_region failed: %d\n", result);
		return result;
	}

	cdev_init(&hab_driver.cdev, &hab_fops);
	hab_driver.cdev.owner = THIS_MODULE;
	hab_driver.cdev.ops = &hab_fops;
	dev = MKDEV(MAJOR(hab_driver.major), 0);

	result = cdev_add(&hab_driver.cdev, dev, 1);

	if (result < 0) {
		unregister_chrdev_region(dev, 1);
		pr_err("cdev_add failed: %d\n", result);
		return result;
	}

	hab_driver.class = class_create(THIS_MODULE, "hab");

	if (IS_ERR(hab_driver.class)) {
		result = PTR_ERR(hab_driver.class);
		pr_err("class_create failed: %d\n", result);
		goto err;
	}

	hab_driver.dev = device_create(hab_driver.class, NULL,
					dev, &hab_driver, "hab");

	if (IS_ERR(hab_driver.dev)) {
		result = PTR_ERR(hab_driver.dev);
		pr_err("device_create failed: %d\n", result);
		goto err;
	}

	/* read in hab config, then configure pchans */
	result = do_hab_parse();

	if (!result) {
		hab_driver.kctx = hab_ctx_alloc(1);
		if (!hab_driver.kctx) {
			pr_err("hab_ctx_alloc failed");
			result = -ENOMEM;
			hab_hypervisor_unregister();
			goto err;
		}

		set_dma_ops(hab_driver.dev, &hab_dma_ops);

		return result;
	}

err:
	if (!IS_ERR_OR_NULL(hab_driver.dev))
		device_destroy(hab_driver.class, dev);
	if (!IS_ERR_OR_NULL(hab_driver.class))
		class_destroy(hab_driver.class);
	cdev_del(&hab_driver.cdev);
	unregister_chrdev_region(dev, 1);

	pr_err("Error in hab init, result %d\n", result);
	return result;
}

static void __exit hab_exit(void)
{
	dev_t dev;

	hab_hypervisor_unregister();
	hab_ctx_put(hab_driver.kctx);
	dev = MKDEV(MAJOR(hab_driver.major), 0);
	device_destroy(hab_driver.class, dev);
	class_destroy(hab_driver.class);
	cdev_del(&hab_driver.cdev);
	unregister_chrdev_region(dev, 1);
}

subsys_initcall(hab_init);
module_exit(hab_exit);

MODULE_DESCRIPTION("Hypervisor abstraction layer");
MODULE_LICENSE("GPL v2");
