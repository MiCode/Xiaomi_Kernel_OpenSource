/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define DEBUG
#define LOG_FLAG	"sia81xx_sock"

#include <linux/slab.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include "sia81xx_tuning_if.h"
#include "sia81xx_socket.h"

#ifndef SO_RCVTIMEO
#define SO_RCVTIMEO SO_RCVTIMEO_NEW
#endif

#define SOCK_PORT_NUM		(5678)
#define RECV_WATI_TIME_MS	(10)
#define MAX_CAL_MAP_NUM		(16)

typedef enum task_state_e {
	TASK_STATE_INIT = 0,
	TASK_STATE_RUN,
	TASK_STATE_CLOSING,
	TASK_STATE_CLOSED,
}task_state_t;

struct cal_map {
	unsigned long cal_handle;	//afe handle(qcom) or cal module unit(mtk)
	uint32_t cal_id;			//afe port id(qcom) or task scene(mtk)
};

enum cal_packet_opt {
	OPT_SET_CAL_VAL,
	OPT_GET_CAL_VAL,
	OPT_ADD_CAL_ID,
	OPT_REM_CAL_ID,
};

enum cal_packet_rep_code {
	REP_CODE_OK,
	REP_CODE_NO_CAL_ID,
	REP_CODE_NO_CAL_HANDLE,
	REP_CODE_PARAM_SIZE_ERR,
	REP_CODE_UNKNOW_OPT,
	REP_CODE_EXEC_ERR,
	REP_CODE_CAL_ID_EXIST,
	REP_CODE_CAL_ID_NOT_EXIST,
	REP_CODE_EXEC_FUNC_MISSING
};

struct sia81xx_cal_packet {
	uint32_t packet_sn;
	uint32_t opt;
	uint32_t rep_code;
	uint32_t cal_id;
	uint32_t module_id;
	uint32_t param_id;
	uint32_t payload_size;
	uint8_t payload[];
} __packed;

#define CAL_PACKET_LEN(packet) \
	((packet)->payload_size + sizeof(struct sia81xx_cal_packet))

static volatile int sock_start_flag;
static volatile task_state_t sock_task_state = TASK_STATE_INIT;
static struct socket *sock_srv = NULL;

struct cal_map cal_map_table[MAX_CAL_MAP_NUM];

static struct cal_map *is_cal_id_exist(
	uint32_t cal_id)
{
	int i = 0;

	for(i = 0; i < MAX_CAL_MAP_NUM; i++) {
		if((cal_id == cal_map_table[i].cal_id) && 
			(0 != cal_map_table[i].cal_handle))
			return &cal_map_table[i];
	}

	return NULL;
}

static struct cal_map *get_one_can_use_cal_map(
	uint32_t cal_id) 
{
	struct cal_map *map = NULL;
	int i = 0;

	if(NULL != (map = is_cal_id_exist(cal_id))) {
		return map;
	}

	for(i = 0; i < MAX_CAL_MAP_NUM; i++) {
		if(0 == cal_map_table[i].cal_handle)
			return &cal_map_table[i];
	}

	return NULL;
}

static int record_cal_map(
	unsigned long cal_handle, 
	uint32_t cal_id)
{
	struct cal_map *map = NULL;

	map = get_one_can_use_cal_map(cal_id);
	if(NULL == map)
		return -EINVAL;

	map->cal_handle = cal_handle;
	map->cal_id = cal_id;

	return 0;
}

static void delete_all_acl_map(void)
{
	int i = 0;

	for(i = 0; i < MAX_CAL_MAP_NUM; i++) {
		if(0 != cal_map_table[i].cal_handle) {
////modify by gift 2019-7-16
#if 0
			if(0 == tuning_if_opt.close(cal_map_table[i].cal_handle)) {
				cal_map_table[i].cal_handle = 0;
				cal_map_table[i].cal_id = 0;
			} else {
				pr_err("[  err][%s] %s: tuning_if_opt.close err, "
					"id = %d \r\n",
					LOG_FLAG, __func__, cal_map_table[i].cal_id);
				continue;
			}
#else
			cal_map_table[i].cal_handle = 0;
			cal_map_table[i].cal_id = 0;
#endif
		}
	}
}

static int sia81xx_sock_write(
	struct socket *sock, 
	char *buf, 
	int len)
{
    struct kvec vec;
    struct msghdr msg;

	//pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);

	memset(&vec, 0, sizeof(vec));
	memset(&msg, 0, sizeof(msg));

	vec.iov_base = buf;
	vec.iov_len = len;

	return kernel_sendmsg(sock, &msg, &vec, 1, len);
}

static int sia81xx_sock_read(
	struct socket *sock, 
	char *buf, 
	int len)
{
    struct kvec vec;
    struct msghdr msg;

	//pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);

	memset(&vec, 0, sizeof(vec));
	memset(&msg, 0, sizeof(msg));
	
	vec.iov_base = buf;
	vec.iov_len = len;
		
	return kernel_recvmsg(sock, &msg, &vec, 1, len, 0);
}

static int sia81xx_sock_proc(
	struct socket *sock, 
	char *buf, 
	int len, 
	int max_len)
{
	int ret = 0;
	struct sia81xx_cal_packet *cal = NULL;
	struct cal_map *map = NULL;

	pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);
	
	cal = (struct sia81xx_cal_packet *)buf;
	if(CAL_PACKET_LEN(cal) > max_len) {
		pr_err("[  err][%s] %s: payload_size = %u, max_len = %d \r\n",
			LOG_FLAG, __func__, cal->payload_size, max_len);
	}

	switch(cal->opt) {
		case OPT_SET_CAL_VAL:
		{
			map = is_cal_id_exist(cal->cal_id);
			if(NULL == map) {
				pr_err("[  err][%s] %s: NULL == map, id = %d \r\n",
					LOG_FLAG, __func__, cal->cal_id);

				cal->rep_code = REP_CODE_NO_CAL_ID;
				ret = sia81xx_sock_write(sock, buf, len);
				
				break; 
			}

			if(CAL_PACKET_LEN(cal) > len) {
				pr_err("[  err][%s] %s: cal->payload_size = %u, len = %d \r\n",
					LOG_FLAG, __func__, cal->payload_size, len);
				
				cal->rep_code = REP_CODE_PARAM_SIZE_ERR;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}

			if(NULL == tuning_if_opt.write) {
				pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.write \r\n",
					LOG_FLAG, __func__);
				
				cal->rep_code = REP_CODE_EXEC_FUNC_MISSING;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}
			
			ret = tuning_if_opt.write(map->cal_handle, cal->module_id, 
				cal->param_id, cal->payload_size, cal->payload);
			if(0 > ret) {
				pr_err("[debug][%s] %s: tuning_if_opt.write failed "
					"ret = %d \r\n", 
					LOG_FLAG, __func__, ret);
				
				cal->rep_code = REP_CODE_EXEC_ERR;
				ret = sia81xx_sock_write(sock, buf, len);
			} else {
				pr_info("[ info][%s] %s: write module_id = 0x%08x, "
					"param_id = 0x%08x, payload_size = %u, "
					"payload = %u, ret = %d \r\n", 
					LOG_FLAG, __func__, cal->module_id, cal->param_id, 
					cal->payload_size, cal->payload[0], ret);

				cal->rep_code = REP_CODE_OK;
				ret = sia81xx_sock_write(sock, buf, len);
			}
			
			break;
		}
		case OPT_GET_CAL_VAL:
		{
			map = is_cal_id_exist(cal->cal_id);
			if(NULL == map) {
				pr_err("[  err][%s] %s: NULL == map, id = %d \r\n",
					LOG_FLAG, __func__, cal->cal_id);

				cal->rep_code = REP_CODE_NO_CAL_ID;
				ret = sia81xx_sock_write(sock, buf, len);
				
				break; 
			}

			if(NULL == tuning_if_opt.read) {
				pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.read \r\n",
					LOG_FLAG, __func__);
				
				cal->rep_code = REP_CODE_EXEC_FUNC_MISSING;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}
			
			ret = tuning_if_opt.read(map->cal_handle, cal->module_id, 
				cal->param_id, cal->payload_size, cal->payload);
			if(ret > cal->payload_size) {
				pr_err("[  err][%s] %s: tuning_if_opt.read failed "
					"ret = %d, payload_size = %u \r\n", 
					LOG_FLAG, __func__, ret, cal->payload_size);
				
				cal->rep_code = REP_CODE_EXEC_ERR;
				ret = sia81xx_sock_write(sock, buf, len);
			} else {
				pr_info("[ info][%s] %s: read module_id = 0x%08x, "
					"param_id = 0x%08x, payload_size = %u, "
					"payload = %u, ret = %d \r\n", 
					LOG_FLAG, __func__, cal->module_id, cal->param_id, 
					cal->payload_size, cal->payload[0], ret);

				cal->rep_code = REP_CODE_OK;
				ret = sia81xx_sock_write(
					sock, buf, CAL_PACKET_LEN(cal));
			}
			break;
		}
		case OPT_ADD_CAL_ID:
		{
			unsigned long cal_handle = 0;
			
			map = is_cal_id_exist(cal->cal_id);
			if(NULL != map) {
				pr_err("[  err][%s] %s: NULL != map, id = %d \r\n",
					LOG_FLAG, __func__, cal->cal_id);

				cal->rep_code = REP_CODE_CAL_ID_EXIST;
				ret = sia81xx_sock_write(sock, buf, len);
				
				break; 
			}
			
			if(NULL == tuning_if_opt.open) {
				pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.open \r\n",
					LOG_FLAG, __func__);
				
				cal->rep_code = REP_CODE_EXEC_FUNC_MISSING;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}

			cal_handle = tuning_if_opt.open(cal->cal_id);
			if(0 == cal_handle) {
				pr_err("[  err][%s] %s: NULL == cal_handle \r\n", 
					LOG_FLAG, __func__);

				cal->rep_code = REP_CODE_EXEC_ERR;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}
			
			if(0 != record_cal_map(cal_handle, cal->cal_id)) {
				pr_err("[  err][%s] %s: 0 != record_cal_map \r\n", 
					LOG_FLAG, __func__);

				cal->rep_code = REP_CODE_EXEC_ERR;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}

			cal->rep_code = REP_CODE_OK;
			ret = sia81xx_sock_write(sock, buf, len);
			
			break;
		}
		case OPT_REM_CAL_ID:
		{
			map = is_cal_id_exist(cal->cal_id);
			if(NULL == map) {
				pr_info("[ info][%s] %s: NULL == map, id = %d \r\n",
					LOG_FLAG, __func__, cal->cal_id);

				cal->rep_code = REP_CODE_CAL_ID_NOT_EXIST;
				ret = sia81xx_sock_write(sock, buf, len);
				
				break; 
			}
			
			if(NULL == tuning_if_opt.close) {
				pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.close \r\n",
					LOG_FLAG, __func__);
				
				cal->rep_code = REP_CODE_EXEC_FUNC_MISSING;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}
			
			if(0 != tuning_if_opt.close(map->cal_handle)) {
				pr_err("[  err][%s] %s: 0 != tuning_if_opt.close \r\n", 
					LOG_FLAG, __func__);

				cal->rep_code = REP_CODE_EXEC_ERR;
				ret = sia81xx_sock_write(sock, buf, len);

				break;
			}
			
			map->cal_handle = 0;
			map->cal_id = 0;

			cal->rep_code = REP_CODE_OK;
			ret = sia81xx_sock_write(sock, buf, len);
			
			break;
		}
		default :
		{
			cal->rep_code = REP_CODE_UNKNOW_OPT;
			ret = sia81xx_sock_write(sock, buf, len);

			break;
		}
	}

	pr_debug("[debug][%s] %s done with opt = 0x%08x, cal_id = 0x%08x, "
		"rep_code = 0x%08x module id = 0x%08x, param id = 0x%08x\r\n", 
		LOG_FLAG, __func__, cal->opt, cal->cal_id, 
		cal->rep_code, cal->module_id, cal->param_id);
	
	return ret;
}

static int sia81xx_sock_server_task(
	void *data)
{
	int ret = 0;

	static struct socket *sock_clt = NULL;
	static char comm_buf[4096];

	pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);

	sock_task_state = TASK_STATE_RUN;

	if((NULL == sock_srv) || (1 != sock_start_flag)) {
		goto end;
	}

	/*listen*/  
	ret = kernel_listen(sock_srv, 10);
	if(ret < 0){
		pr_err("[  err][%s] %s: server: listen error ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		goto end;
    }
    pr_debug("[debug][%s] %s: listen ok! ret = %d \r\n", 
		LOG_FLAG, __func__, ret);

	while(TASK_STATE_RUN == sock_task_state) {
		if(kernel_accept(sock_srv, &sock_clt, 10) >= 0)
			break;
	}

	if(TASK_STATE_RUN == sock_task_state) {
		pr_debug("[debug][%s] %s: accept ok, Connection "
			"Established addr<0x%08u>\r\n", 
			LOG_FLAG, __func__, sock_clt->sk->sk_rcv_saddr);
	}

	while(TASK_STATE_RUN == sock_task_state) {
		ret = sia81xx_sock_read(
			sock_clt, comm_buf, sizeof(comm_buf));
		if((ret <= 0) || (ret > sizeof(comm_buf))) {
			msleep(RECV_WATI_TIME_MS);
			continue;
		}
		pr_debug("[debug][%s] %s: recv len = %d \r\n", 
			LOG_FLAG, __func__, ret);

		ret = sia81xx_sock_proc(sock_clt, comm_buf, ret, sizeof(comm_buf));
		if(0 > ret) {
			pr_err("[  err][%s] %s: err !! process ret = %d \r\n", 
				LOG_FLAG, __func__, ret);
		} else {
			pr_debug("[debug][%s] %s: process = %d \r\n", 
				LOG_FLAG, __func__, ret);
		}
	}

end :
	pr_debug("[debug][%s] %s: sia81xx_sock_server_task close \r\n", 
		LOG_FLAG, __func__);
	
	if(NULL != sock_clt) {
		pr_debug("[debug][%s] %s: sock_release sock_clt \r\n", 
			LOG_FLAG, __func__);
		sock_release(sock_clt);
	}
	
	sock_task_state = TASK_STATE_CLOSED;

	return 0;
}

static int sia81xx_open_sock_task(void)
{
	struct task_struct *socket_task = NULL;

	if((TASK_STATE_RUN == sock_task_state) || 
		(TASK_STATE_CLOSING == sock_task_state))
		return -EINVAL;

	socket_task = kthread_create(
		sia81xx_sock_server_task, NULL, "sia81xx_socket");
	if(NULL == socket_task) {
		pr_err("[  err][%s] %s: socket_task create error \r\n", 
			LOG_FLAG, __func__);
		return -ECHILD;
	}
	
	if(wake_up_process(socket_task) < 0) {
		return -EFAULT;
	}

	return 0;
}

static int sia81xx_close_sock_task(void)
{
	int max_wait_time_ms = RECV_WATI_TIME_MS * 10;

	pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);

	if(TASK_STATE_RUN != sock_task_state)
		goto end;

	sock_task_state = TASK_STATE_CLOSING;

	while(TASK_STATE_CLOSED != sock_task_state) {
		
		if(0 >= max_wait_time_ms) {
			pr_err("[  err][%s] %s: sia81xx_wait_sock_task_close "
				"time out\r\n", 
				LOG_FLAG, __func__);
			return -EBUSY;
		}
		
		msleep(1);
		max_wait_time_ms --;
	}

end :
	pr_debug("[debug][%s] %s: use wait time %d ms \r\n", 
		LOG_FLAG, __func__, (RECV_WATI_TIME_MS * 10) - max_wait_time_ms);

	return 0;
}

int sia81xx_open_sock_server()
{
	int ret = 0;
	struct sockaddr_in s_addr;
	struct timeval tv;

	pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);

	if(0 != sock_start_flag) {
		pr_err("[  err][%s] %s: flag = %d, sock_srv = %d \r\n", 
			LOG_FLAG, __func__, sock_start_flag, (NULL == sock_srv));
		return 0;
	}

	memset(&s_addr, 0, sizeof(s_addr));
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(SOCK_PORT_NUM);
	s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*create a socket*/
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,0,0))
	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, 0, &sock_srv);
#else
	ret = sock_create_kern(AF_INET, SOCK_STREAM, 0, &sock_srv);
#endif
	if(ret){
		pr_err("[  err][%s] %s: server:socket_create err! ret = %d \r\n", 
			LOG_FLAG, __func__, ret);
		goto err;
	}
	pr_debug("[debug][%s] %s: server:socket_create ok! ret = %d \r\n", 
		LOG_FLAG, __func__, ret);

	tv.tv_sec = 0;
	tv.tv_usec = 1000 * RECV_WATI_TIME_MS;
	kernel_setsockopt(
		sock_srv, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

	/*bind the socket*/
	ret = kernel_bind(sock_srv, (struct sockaddr *)&s_addr, 
			sizeof(struct sockaddr_in));
	if(ret < 0){
		pr_err("[  err][%s] %s: server: bind error ret = %d \r\n", 
			LOG_FLAG, __func__, ret);
		goto err_uncreate;
	}
	pr_debug("[debug][%s] %s: server:bind ok! ret = %d \r\n", 
		LOG_FLAG, __func__, ret);

	ret = sia81xx_open_sock_task();
	if(0 != ret) {
		pr_err("[  err][%s] %s: socket_task create error ret = %d \r\n", 
			LOG_FLAG, __func__, ret);
		goto err_uncreate;
	}

	sock_start_flag = 1;

	/* before socket server start, 
	 * cal_map_table should not has any data */
	memset(cal_map_table, 0, sizeof(cal_map_table));
	
	pr_debug("[debug][%s] %s: socket server start ok !!! \r\n", 
		LOG_FLAG, __func__);

	return 0;

err_uncreate :
	sock_release(sock_srv);
err :
	sock_srv = NULL;
	sock_start_flag = 0;
	
	return ret;
}
//EXPORT_SYMBOL(sia81xx_open_sock_server);

int sia81xx_close_sock_server(void)
{
	int ret = 0;

	pr_debug("[debug][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	sia81xx_close_sock_task();

	if((NULL != sock_srv) && (0 != sock_start_flag)) {
		pr_debug("[debug][%s] %s: sock_release sock_srv !! \r\n", 
			LOG_FLAG, __func__);
		sock_release(sock_srv);
	}

	sock_srv = NULL;
	sock_start_flag = 0;

	/* when socket server closed, must be delete all 
	 * cal_handle by this function call */
	delete_all_acl_map();

	return ret;
}
//EXPORT_SYMBOL(sia81xx_close_sock_server);

int sia81xx_sock_init(void)
{
	int ret = 0;

	pr_info("[ info][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	sia81xx_close_sock_server();

	sock_task_state = TASK_STATE_INIT;

	memset(cal_map_table, 0, sizeof(cal_map_table));
	
	return ret;
}
//EXPORT_SYMBOL(sia81xx_sock_init);

void sia81xx_sock_exit(void)
{
	pr_info("[ info][%s] %s: run !! \r\n", LOG_FLAG, __func__);

	sia81xx_close_sock_server();

	sock_task_state = TASK_STATE_INIT;

	memset(cal_map_table, 0, sizeof(cal_map_table));
}
//EXPORT_SYMBOL(sia81xx_sock_exit);



