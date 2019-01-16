#ifndef __CMDQ_STRUCT_H__
#define __CMDQ_STRUCT_H__

#include <linux/list.h>
#include <linux/spinlock.h>

typedef struct cmdqFileNodeStruct {
	pid_t userPID;
	pid_t userTGID;
	struct list_head taskList;
	spinlock_t nodeLock;
} cmdqFileNodeStruct;

#endif				/* __CMDQ_STRUCT_H__ */
