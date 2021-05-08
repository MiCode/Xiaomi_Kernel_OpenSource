#ifndef __LCT_TP_PALM_H__
#define __LCT_TP_PALM_H__

typedef int (*tp_palm_cb_t)(bool enable_tp);

extern int init_lct_tp_palm(tp_palm_cb_t callback);
extern void uninit_lct_tp_palm(void);
extern void set_lct_tp_palm_status(bool en);
extern bool get_lct_tp_palm_status(void);

#endif //__LCT_TP_PALM_H__

