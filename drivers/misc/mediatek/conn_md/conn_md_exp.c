// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define DFT_TAG "[CONN_MD_EXP]"

#include "conn_md_exp.h"
#include "conn_md_log.h"

#include "conn_md.h"

int mtk_conn_md_bridge_reg(uint32 u_id, struct conn_md_bridge_ops *p_ops)
{

	int i_ret = -1;
	/*sanity check */
	if (NULL != p_ops && NULL != p_ops->rx_cb) {
		/*add user */
		i_ret = conn_md_add_user(u_id, p_ops);
	} else {
		CONN_MD_ERR_FUNC("ERROR, u_id(0x%08x)\n", u_id);
		CONN_MD_ERR_FUNC("ERROR, p_ops(%p), rx_cb(%p)\n",
				p_ops, NULL == p_ops ? NULL : p_ops->rx_cb);
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	}

	return i_ret;
}
EXPORT_SYMBOL(mtk_conn_md_bridge_reg);

int mtk_conn_md_bridge_unreg(uint32 u_id)
{

	int i_ret = -1;

	/*delete user */
	i_ret = conn_md_del_user(u_id);
	return 0;
}
EXPORT_SYMBOL(mtk_conn_md_bridge_unreg);
int mtk_conn_md_bridge_send_msg(struct ipc_ilm *ilm)
{
	int i_ret = -1;
	/*sanity check */
	if (NULL != ilm && NULL != ilm->local_para_ptr) {
		/*send data */
		i_ret = conn_md_send_msg(ilm);
	} else {
		CONN_MD_ERR_FUNC("ERROR, ilm(%p),local_para_ptr(%p)\n",
				ilm, ilm == NULL ? NULL : ilm->local_para_ptr);
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_conn_md_bridge_send_msg);
