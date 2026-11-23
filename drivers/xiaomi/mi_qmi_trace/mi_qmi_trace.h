// SPDX-License-Identifier: GPL-2.0
#ifndef MI_QMI_TRACE_H
#define MI_QMI_TRACE_H
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
extern int device_state; // 设备状态变量
// 设备状态定义
#define DEVICE_STATE_DEFAULT 0
#define DEVICE_STATE_SLEEP   1
#define DEVICE_STATE_NIGHT   2
#define DEVICE_STATE_DAY     3
#define DEVICE_STATE_CLEAR   9
typedef struct Node {
    char *qmi;
    int count;
    struct Node *next;
} Node;
extern Node *sleep_head;
extern Node *night_head;
extern Node *day_head;
// 函数声明
void appendNode(const char *hex_service_id, const char *qmi_message_id);
#endif // MI_QMI_TRACE_H
