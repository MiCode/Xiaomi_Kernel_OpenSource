#ifndef __LCT_BKL_SHOW_H__
#define __LCT_BKL_SHOW_H__

typedef int (*bkl_show_cb_t)(bool enable_tp);

extern int init_lct_bkl_show(bkl_show_cb_t callback);
extern void uninit_lct_bkl_show(void);
extern void set_lct_bkl_show_status(bool en);
extern bool get_lct_bkl_show_status(void);

#endif //__LCT_TP_WORK_H__

