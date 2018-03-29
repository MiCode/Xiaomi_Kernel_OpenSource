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


#define DFT_TAG "[CONN_MD_EXP]"
#include "conn_md_log.h"

#include "conn_md_exp.h"

#include "conn_md.h"

CONN_MD_BRIDGE_OPS g_ops;

static int conn_md_test_rx_cb(ipc_ilm_t *ilm);

int conn_md_test(void)
{
#define PACKAGE_SIZE 100

	ipc_ilm_t ilm;
	local_para_struct *p_buf_str;
	int i = 0;
	int msg_len = 0;

	p_buf_str = kmalloc(sizeof(local_para_struct) + PACKAGE_SIZE, GFP_ATOMIC);
	if (NULL == p_buf_str) {
		CONN_MD_ERR_FUNC("kmalloc for local para ptr structure failed.\n");
		return -1;
	}
	p_buf_str->msg_len = PACKAGE_SIZE;
	for (i = 0; i < PACKAGE_SIZE; i++)
		p_buf_str->data[i] = i;

	ilm.local_para_ptr = p_buf_str;

	g_ops.rx_cb = conn_md_test_rx_cb;

	mtk_conn_md_bridge_reg(0x800001, &g_ops);
	mtk_conn_md_bridge_reg(0x800005, &g_ops);
	mtk_conn_md_bridge_reg(0x800009, &g_ops);

	ilm.dest_mod_id = 0x800005;
	ilm.src_mod_id = 0x800001;
	ilm.msg_id = 0;

	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x800005;
	ilm.src_mod_id = 0x800009;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x800001;
	ilm.src_mod_id = 0x800009;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x80000a;
	ilm.src_mod_id = 0x800009;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x800009;
	ilm.src_mod_id = 0x80000a;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x800005;
	ilm.src_mod_id = 0x800001;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x800001;
	ilm.src_mod_id = 0x800005;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	ilm.dest_mod_id = 0x800001;
	ilm.src_mod_id = 0x80000a;
	ilm.msg_id++;
	msg_len += 10;
	msg_len = ((msg_len >= PACKAGE_SIZE) ? PACKAGE_SIZE : msg_len);
	ilm.local_para_ptr->msg_len = msg_len;
	mtk_conn_md_bridge_send_msg(&ilm);

	kfree(p_buf_str);
	p_buf_str = NULL;

	conn_md_dmp_msg_queued(0, 0x80000a);

	mtk_conn_md_bridge_unreg(0x800001);
	conn_md_dmp_msg_queued(0, 0);
	conn_md_dmp_msg_active(0, 0);

	mtk_conn_md_bridge_unreg(0x800009);

	mtk_conn_md_bridge_unreg(0x80000a);
	conn_md_dmp_msg_queued(0, 0);
	conn_md_dmp_msg_active(0, 0);

	conn_md_dmp_msg_logged(0x800009, 0x800001);
	conn_md_dmp_msg_logged(0x800009, 0);
	conn_md_dmp_msg_logged(0, 0);
	conn_md_dmp_msg_logged(0x80000a, 0);

	return 0;
}

static int conn_md_test_rx_cb(ipc_ilm_t *ilm)
{
	int i = 0;

	pr_warn("%s, ilm:0x%p\n", __func__, ilm);
	pr_warn("%s, ilm:src_id(%d), dst_id(%d), msg_id(%d)\n", __func__,
		ilm->src_mod_id, ilm->dest_mod_id, ilm->msg_id);

	pr_warn("%s, local_para_ptr:0x%p, msg_len:%d\n", __func__, ilm->local_para_ptr,
		ilm->local_para_ptr->msg_len);

	for (i = 0; i < ilm->local_para_ptr->msg_len; i++) {
		pr_warn("%d ", ilm->local_para_ptr->data[i]);
		if ((0 != i) && (((1 + i) % 8) == 0))
			pr_warn("\n");
	}
	return 0;
}
