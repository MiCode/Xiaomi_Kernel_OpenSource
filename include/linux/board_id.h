#ifndef __BOARD_ID_H__
#define __BOARD_ID_H__

extern void board_id_get_hwname(char *str);
extern int board_id_get_hwlevel(void);
extern int board_id_get_hwversion_product_num(void);
extern int board_id_get_hwversion_major_num(void);
extern int board_id_get_hwversion_minor_num(void);

#endif //__BOARD_ID_H__
