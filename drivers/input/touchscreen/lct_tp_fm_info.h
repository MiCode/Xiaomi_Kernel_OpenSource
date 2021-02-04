
#ifndef LCT_TP_FM_INFO_H
#define LCT_TP_FM_INFO_H

#define SUPPORT_READ_TP_VERSION // *#87# can read tp version

extern int init_tp_fm_info(u16 version_info_num, char* version_info_str, char *name);
extern void update_tp_fm_info(char  *version_info_str);

#endif
