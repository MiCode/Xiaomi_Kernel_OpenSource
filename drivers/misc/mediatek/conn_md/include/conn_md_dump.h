#ifndef __CONN_MD_DUMP_H_
#define __CONN_MD_DUMP_H_

#include "conn_md_log.h"
#include "conn_md_exp.h"

#define LENGTH_PER_PACKAGE 8
#define NUMBER_OF_MSG_LOGGED 16


typedef enum {
	MSG_ENQUEUE = 1,
	MSG_DEQUEUE = 2,
	MSG_EN_DE_QUEUE = 3,
} CONN_MD_MSG_TYPE;


typedef struct _CONN_MD_DMP_MSG_STR_ {
	unsigned int sec;
	unsigned int usec;
	CONN_MD_MSG_TYPE type;
	ipc_ilm_t ilm;
	uint16 msg_len;
	uint8 data[LENGTH_PER_PACKAGE];
} CONN_MD_DMP_MSG_STR, *P_CONN_MD_DMP_MSG_STR;


typedef struct _CONN_MD_DMP_MSG_LOG_ {

	CONN_MD_DMP_MSG_STR msg[NUMBER_OF_MSG_LOGGED];
	uint16 in;
	uint16 out;
	uint32 size;
	struct mutex lock;

} CONN_MD_DMP_MSG_LOG, *P_CONN_MD_DMP_MSG_LOG;


extern P_CONN_MD_DMP_MSG_LOG conn_md_dmp_init(void);
extern int conn_md_dmp_deinit(P_CONN_MD_DMP_MSG_LOG p_log);


extern int conn_md_dmp_in(ipc_ilm_t *p_ilm, CONN_MD_MSG_TYPE msg_type,
			  P_CONN_MD_DMP_MSG_LOG p_msg_log);
extern int conn_md_dmp_out(P_CONN_MD_DMP_MSG_LOG p_msg_log, uint32 src_id, uint32 dst_id);

#endif
