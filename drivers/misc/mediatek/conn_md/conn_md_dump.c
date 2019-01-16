

#define DFT_TAG "[CONN_MD_DMP]"

#include "conn_md_log.h"
#include "conn_md_dump.h"


P_CONN_MD_DMP_MSG_LOG conn_md_dmp_init(void)
{
	uint32 msg_log_size = sizeof(CONN_MD_DMP_MSG_LOG);
	P_CONN_MD_DMP_MSG_LOG p_msg_log = vmalloc(msg_log_size);
	if (NULL != p_msg_log) {
		CONN_MD_INFO_FUNC("alloc memory for msg log system done, size:0x%08x\n",
				  msg_log_size);
		memset(p_msg_log, 0, msg_log_size);

		mutex_init(&p_msg_log->lock);

	} else {
		CONN_MD_ERR_FUNC("alloc memory for msg log system failed\n");
	}
	return p_msg_log;
}

int conn_md_dmp_deinit(P_CONN_MD_DMP_MSG_LOG p_log)
{
	int i_ret = -1;
	if (NULL != p_log) {
		CONN_MD_INFO_FUNC("vaild log buffer pointer:0x%08x, free it.\n", p_log);
		mutex_destroy(&p_log->lock);
		vfree(p_log);
		i_ret = 0;
	} else {
		CONN_MD_WARN_FUNC("invalid log buffer pointer\n");
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	}
	return 0;
}

int __conn_md_dmp_in(ipc_ilm_t *p_ilm, CONN_MD_MSG_TYPE msg_type, P_CONN_MD_DMP_MSG_LOG p_msg_log)
{
	struct timeval now;
	P_CONN_MD_DMP_MSG_STR p_msg = NULL;

	/*get current time */
	do_gettimeofday(&now);

	mutex_lock(&p_msg_log->lock);

	p_msg = &p_msg_log->msg[p_msg_log->in];

	/*Log timestamp */
	p_msg->sec = now.tv_sec;
	p_msg->usec = now.tv_usec;
	p_msg->type = msg_type;

	/*Log p_ilm */
	memcpy(&p_msg->ilm, p_ilm, sizeof(ipc_ilm_t));

	/*Log msg length */
	p_msg->msg_len = p_ilm->local_para_ptr->msg_len;

	/*Log msg content */
	memcpy(&p_msg->data, p_ilm->local_para_ptr->data,
	       p_msg->msg_len > LENGTH_PER_PACKAGE ? LENGTH_PER_PACKAGE : p_msg->msg_len);

	/*update in size and index */

	if (NUMBER_OF_MSG_LOGGED <= p_msg_log->size)
		p_msg_log->size = NUMBER_OF_MSG_LOGGED;
	else
		p_msg_log->size++;

	p_msg_log->in++;
	p_msg_log->in %= NUMBER_OF_MSG_LOGGED;

	mutex_unlock(&p_msg_log->lock);
	CONN_MD_WARN_FUNC("msg type:%d enqueued succeed\n", msg_type);
	return 0;
}


int conn_md_dmp_in(ipc_ilm_t *p_ilm, CONN_MD_MSG_TYPE msg_type, P_CONN_MD_DMP_MSG_LOG p_msg_log)
{
	int i_ret = -1;

	if (NULL == p_ilm ||
	    NULL == p_ilm->local_para_ptr ||
	    0 == p_ilm->local_para_ptr->msg_len ||
	    (msg_type != MSG_ENQUEUE && msg_type != MSG_DEQUEUE)) {
		CONN_MD_WARN_FUNC("invalid parameter\n");
		i_ret = CONN_MD_ERR_INVALID_PARAM;
	} else {
		i_ret = __conn_md_dmp_in(p_ilm, msg_type, p_msg_log);
	}
	return i_ret;
}

int __conn_md_dmp_msg_filter(P_CONN_MD_DMP_MSG_STR p_msg, uint32 src_id, uint32 dst_id)
{
	ipc_ilm_t *p_ilm = &p_msg->ilm;
	int i = 0;

	if (((0 == src_id) || (src_id == p_ilm->src_mod_id)) &&
	    ((0 == dst_id) || (dst_id == p_ilm->dest_mod_id))) {
		__conn_md_log_print(DFT_TAG
				    "%d.%d s, <%s> src_id:0x%08x, dst_id:0x%08x, msg_len:%d, dump_len:%d:\n",
				    p_msg->sec, p_msg->usec,
				    (MSG_ENQUEUE == p_msg->type ? "enqueue" : "dequeue"),
				    p_msg->ilm.src_mod_id, p_msg->ilm.dest_mod_id, p_msg->msg_len,
				    (LENGTH_PER_PACKAGE >=
				     p_msg->msg_len ? p_msg->msg_len : LENGTH_PER_PACKAGE));

		for (i = 0; (i < p_msg->msg_len) && (i < LENGTH_PER_PACKAGE); i++) {
			__conn_md_log_print("%02x ", p_msg->data[i]);
			if (7 == (i % 8))
				__conn_md_log_print("\n");
		}
		__conn_md_log_print("\n");
	}
	return 0;
}


int conn_md_dmp_out(P_CONN_MD_DMP_MSG_LOG p_msg_log, uint32 src_id, uint32 dst_id)
{
	int i_ret = 0;
	int size = 0;
	int in = 0;
	int out = 0;
	P_CONN_MD_DMP_MSG_STR p_msg = NULL;

	mutex_lock(&p_msg_log->lock);

	size = p_msg_log->size;

	if (NULL == p_msg_log) {
		mutex_unlock(&p_msg_log->lock);

		CONN_MD_WARN_FUNC("invalid parameter, p_msg_log:0x%08x\n", p_msg_log);
		return CONN_MD_ERR_INVALID_PARAM;
	}
	CONN_MD_INFO_FUNC("dump msg for <src_id:0x%08x, dst_id:0x%08x> start\n", src_id, dst_id);
	if (NUMBER_OF_MSG_LOGGED == size) {
		out = in;
	} else {
		out = 0;
	}

	while (size--) {
		p_msg = &p_msg_log->msg[out];

		__conn_md_dmp_msg_filter(p_msg, src_id, dst_id);

		out++;
		out %= NUMBER_OF_MSG_LOGGED;
	}
	mutex_unlock(&p_msg_log->lock);
	CONN_MD_INFO_FUNC("dump msg for <src_id:0x%08x, dst_id:0x%08x> finished\n", src_id, dst_id);
	return i_ret;
}
