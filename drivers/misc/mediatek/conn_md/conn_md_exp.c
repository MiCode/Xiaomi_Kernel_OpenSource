
#define DFT_TAG "[CONN_MD_EXP]"

#include "conn_md_exp.h"
#include "conn_md_log.h"

#include "conn_md.h"




int mtk_conn_md_bridge_reg(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops)
{

	int i_ret = -1;
	/*sanity check */
	if (NULL != p_ops && NULL != p_ops->rx_cb) {
		/*add user */
		i_ret = conn_md_add_user(u_id, p_ops);
	} else {
		CONN_MD_ERR_FUNC("invalid parameter, u_id (0x%08x), p_ops(0x08x), rx_cb(0x%08x)\n",
				 u_id, p_ops, NULL == p_ops ? NULL : p_ops->rx_cb);
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	}


	return i_ret;
}

int mtk_conn_md_bridge_unreg(uint32 u_id)
{

	int i_ret = -1;

	/*delete user */
	i_ret = conn_md_del_user(u_id);
	return 0;
}

int mtk_conn_md_bridge_send_msg(ipc_ilm_t *ilm)
{
	int i_ret = -1;
	/*sanity check */
	if (NULL != ilm && NULL != ilm->local_para_ptr) {
		/*send data */
		i_ret = conn_md_send_msg(ilm);
	} else {
		CONN_MD_ERR_FUNC("invalid parameter, ilm(0x08x), ilm local_para_ptr(0x%08x)\n", ilm,
				 NULL == ilm ? NULL : ilm->local_para_ptr);
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_conn_md_bridge_reg);
EXPORT_SYMBOL(mtk_conn_md_bridge_unreg);
EXPORT_SYMBOL(mtk_conn_md_bridge_send_msg);
