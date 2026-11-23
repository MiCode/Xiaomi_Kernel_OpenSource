

#ifndef __PUB_STACK_H
#define __PUB_STACK_H

int xiaomi_save_stack_trace(struct task_struct *tsk, unsigned long *backtrace);
void xiaomi_save_stack_trace_user(unsigned long *backtrace);
int xiaomi_stack_trace_cmp(unsigned long *backtrace1, unsigned long *backtrace2);

#endif /* __PUB_STACK_H */

