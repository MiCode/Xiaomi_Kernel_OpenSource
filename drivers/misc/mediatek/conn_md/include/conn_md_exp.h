#ifndef __CONN_MD_EXP_H_
#define __CONN_MD_EXP_H_


#if defined(CONFIG_MTK_ECCCI_DRIVER) || defined(CONFIG_MTK_ECCCI_DRIVER_MODULE)
#include "port_ipc.h"		/*data structure is defined here, mediatek/kernel/drivers/eccci */
#include "ccci_ipc_task_ID.h"	/*IPC task id is defined here, mediatek/kernel/drivers/eccci */
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;
#ifdef CHAR
#undef CHAR
#endif
#define IPC_HEADER_FILE_INCLUDED 1
#endif

#ifndef IPC_HEADER_FILE_INCLUDED
/* #if defined(CONFIG_MTK_EEMCS_DEVICES) || defined(CONFIG_MTK_EEMCS_DEVICES_MODULE) */
#include "eemcs_ipc.h"		/*data structure is defined here, mediatek/kernel/drivers/eemcs */
#include "eemcs_ipc_task_ID.h"	/*IPC task id is defined here, mediatek/kernel/drivers/eemcs */
/* #endif */
#endif
typedef enum {
	CONN_MD_ERR_NO_ERR = 0,
	CONN_MD_ERR_DEF_ERR = -1,
	CONN_MD_ERR_INVALID_PARAM = -2,
	CONN_MD_ERR_OTHERS = -4,

} CONN_MD_ERR_CODE;


/*For IDC test*/
typedef int (*CONN_MD_MSG_RX_CB) (ipc_ilm_t *ilm);

typedef struct {
	CONN_MD_MSG_RX_CB rx_cb;
} CONN_MD_BRIDGE_OPS, *P_CONN_MD_BRIDGE_OPS;

extern int mtk_conn_md_bridge_reg(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops);
extern int mtk_conn_md_bridge_unreg(uint32 u_id);
extern int mtk_conn_md_bridge_send_msg(ipc_ilm_t *ilm);

#if 0
static int __weak mtk_conn_md_bridge_reg(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops)
{
	pr_err(KERN_ERR "MTK_CONN Weak FUNCTION~~~\n");
	return 0;
}

static int __weak mtk_conn_md_bridge_unreg(uint32 u_id)
{
	pr_err(KERN_ERR "MTK_CONN Weak FUNCTION~~~\n");
	return 0;
}

static int __weak mtk_conn_md_bridge_send_msg(ipc_ilm_t *ilm)
{
	pr_err(KERN_ERR "MTK_CONN Weak FUNCTION~~~\n");
	return 0;
}
#endif

#endif /*__CONN_MD_EXP_H_*/
