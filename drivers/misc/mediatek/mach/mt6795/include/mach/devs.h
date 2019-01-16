#ifndef __DEVS_H__
#define __DEVS_H__

#include <board-custom.h>
#include <mach/board.h>

#define CFG_DEV_UART0
#define CFG_DEV_UART1
#define CFG_DEV_UART2
#define CFG_DEV_UART3
//#define I2C_E2_ECO

/*
 * Define constants.
 */

#define MTK_UART_SIZE 0x100

/*
 * Define function prototype.
 */

extern int mt_board_init(void);

//extern unsigned int *get_modem_size_list(void);
//extern unsigned int get_nr_modem(void);

#endif  /* !__MT6575_DEVS_H__ */

