// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * usage:
 * vpu_qos_counter_init():
 *   create qos workqueue for count bandwidth
 *   @call at init hardware
 *
 * vpu_qos_counter_start():
 *   create timer to count current bandwidth of VPU each 16ms
 *   timer will schedule work to wq when time's up
 *   @call at bootup hardware
 *
 * vpu_cmd_qos_start():
 *   enque one cmd to counter's linked list and count bw of cmd
 *   @call at issue cmd
 *
 * vpu_cmd_qos_end():
 *  deque current cmd from counter's linked list
 *  @call at cmd return
 *
 * vpu_qos_counter_end():
 *  delete timer
 *  @call at shutdown hardware
 *
 * vpu_qos_counter_destroy():
 *  delete qos workqueue
 *  @call at destroy hardware
 */


#ifndef _MT6779_VPU_QOS_H_
#define _MT6779_VPU_QOS_H_

int vpu_cmd_qos_start(int core);
int vpu_cmd_qos_end(int core);

int vpu_qos_counter_start(unsigned int core);
int vpu_qos_counter_end(unsigned int core);
int vpu_qos_counter_init(void);
void vpu_qos_counter_destroy(void);

#endif
