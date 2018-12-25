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
		CONN_MD_ERR_FUNC("ERROR, p_ops(0x%08x), rx_cb(0x%08x)\n",
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
		CONN_MD_ERR_FUNC("ERROR, ilm(0x%08x),local_para_ptr(0x%08x)\n",
				ilm, ilm == NULL ? NULL : ilm->local_para_ptr);
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_conn_md_bridge_send_msg);
