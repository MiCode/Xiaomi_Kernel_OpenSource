/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

/*
 * usage:
 * mdla_qos_counter_init():
 *   create qos workqueue for count bandwidth
 *   @call at init hardware
 *
 * mdla_qos_counter_start():
 *   create timer to count current bandwidth of VPU each 16ms
 *   timer will schedule work to wq when time's up
 *   @call at bootup hardware
 *
 * mdla_cmd_qos_start():
 *   enque one cmd to counter's linked list and count bw of cmd
 *   @call at issue cmd
 *
 * mdla_cmd_qos_end():
 *  deque current cmd from counter's linked list
 *  @call at cmd return
 *
 * mdla_qos_counter_end():
 *  delete timer
 *  @call at shutdown hardware
 *
 * mdla_qos_counter_destroy():
 *  delete qos workqueue
 *  @call at destroy hardware
 */
#ifndef __MDLA_QOS_H__
#define __MDLA_QOS_H__

int mdla_cmd_qos_start(int core);
int mdla_cmd_qos_end(int core);

int mdla_qos_counter_start(unsigned int core);
int mdla_qos_counter_end(unsigned int core);
int mdla_qos_counter_init(void);
void mdla_qos_counter_destroy(void);

#endif
