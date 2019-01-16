#ifndef __CONN_MD_H_
#define __CONN_MD_H_



#include "conn_md_exp.h"
#include "conn_md_dump.h"


/*-----------------------Data Structure Definition-----------------------*/

typedef enum {
	USER_MIN,
	USER_REGED,
	USER_ENABLED,
	USER_DISABLED,
	USER_UNREGED,
	USER_MAX,
} USER_STATE;

typedef struct _CONN_MD_USER_ {
	uint32 u_id;
	USER_STATE state;
	CONN_MD_BRIDGE_OPS ops;
	struct list_head entry;
} CONN_MD_USER, *P_CONN_MD_USER;


typedef struct _CONN_MD_MSG_ {
	ipc_ilm_t ilm;
	struct list_head entry;
	local_para_struct local_para;
} CONN_MD_MSG, *P_CONN_MD_MSG;


typedef struct _CONN_MD_QUEUE_ {
	struct list_head list;
	struct mutex lock;
	uint32 counter;
} CONN_MD_QUEUE, *P_CONN_MD_QUEUE;


typedef struct _CONN_MD_USER_LIST_ {
	uint32 counter;
	struct list_head list;
	struct mutex lock;	/*lock for user add/delete/check */
} CONN_MD_USER_LIST, *P_CONN_MD_USER_LIST;

typedef struct _CONN_MD_STRUCT_ {
	/*con-md-thread used for tx queue handle */
	struct task_struct *p_task;
	struct completion tx_comp;

	CONN_MD_USER_LIST user_list;
	CONN_MD_QUEUE act_queue;
	CONN_MD_QUEUE msg_queue;
	P_CONN_MD_DMP_MSG_LOG p_msg_dmp_sys;

} CONN_MD_STRUCT, *P_CONN_MD_STRUCT;

extern int conn_md_send_msg(ipc_ilm_t *ilm);
extern int conn_md_del_user(uint32 u_id);
extern int conn_md_add_user(uint32 u_id, CONN_MD_BRIDGE_OPS *p_ops);
extern int conn_md_dmp_msg_logged(uint32 src_id, uint32 dst_id);
extern int conn_md_dmp_msg_active(uint32 src_id, uint32 dst_id);
extern int conn_md_dmp_msg_queued(uint32 src_id, uint32 dst_id);


#endif
