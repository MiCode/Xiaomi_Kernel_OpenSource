#ifndef __LCT_TP_WORK_H__
#define __LCT_TP_WORK_H__

typedef int (*tp_work_cb_t)(bool enable_tp);

extern int init_lct_tp_work(tp_work_cb_t callback);
extern bool get_lct_tp_work_status(void);

#endif //__LCT_TP_WORK_H__

