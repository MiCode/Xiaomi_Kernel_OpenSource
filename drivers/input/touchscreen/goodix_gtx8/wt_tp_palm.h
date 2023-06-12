#ifndef __WT_TP_PALM_H__
#define __WT_TP_PALM_H__

typedef int (*tp_palm_cb_t)(bool enable_tp);

extern int init_wt_tp_palm(tp_palm_cb_t callback);
extern void uninit_wt_tp_palm(void);
extern void set_wt_tp_palm_status(bool en);
extern bool get_wt_tp_palm_status(void);

#endif //__wt_TP_PALM_H__

