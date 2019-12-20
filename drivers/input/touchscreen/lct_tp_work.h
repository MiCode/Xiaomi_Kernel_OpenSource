#ifndef __GGG_TP_WORK_H__
#define __GGG_TP_WORK_H__

typedef int (*tp_work_cb_t)(bool enable_tp);

extern int init_lct_tp_work(tp_work_cb_t callback);
extern void uninit_lct_tp_work(void);
extern void set_lct_tp_work_status(bool en);
extern bool get_lct_tp_work_status(void);

#endif //__GGG_TP_WORK_H__

