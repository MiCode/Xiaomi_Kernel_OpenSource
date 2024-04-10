#ifndef __LCT_TP_PROXIMITY_SWITCH_H__
#define __LCT_TP_PROXIMITY_SWITCH_H__

typedef int (*tp_proximity_switch_cb_t)(bool enable_tp);

extern int init_lct_tp_proximity_switch(tp_proximity_switch_cb_t callback);
extern void uninit_lct_tp_proximity_switch(void);
extern void set_lct_tp_proximity_switch_status(bool en);
extern bool get_lct_tp_proximity_switch_status(void);

#endif //__LCT_TP_PROXIMITY_SWITCH_H__
