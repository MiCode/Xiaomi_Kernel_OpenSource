#ifndef __LCT_TP_GRIP_AREA_H__
#define __LCT_TP_GRIP_AREA_H__

typedef int (*get_screen_angle_callback)(void);
typedef int (*set_screen_angle_callback)(int angle);

extern int init_lct_tp_grip_area(set_screen_angle_callback set_fun, get_screen_angle_callback get_fun);
extern void uninit_lct_tp_grip_area(void);
extern int set_tp_grip_area_angle(int screen_angle);
extern int get_tp_grip_area_angle(void);

#endif//__LCT_TP_GRIP_AREA_H__

