#ifndef __CCCI_TTY_H__
#define __CCCI_TTY_H__

#define  CCCI_TTY_MODEM      0
#define  CCCI_TTY_META       1
#define  CCCI_TTY_IPC         2
#define  CCCI_TTY_ICUSB      3

typedef struct
{
    unsigned read;
    unsigned write;
    unsigned length;
} buffer_control_tty_t;


typedef struct
{
    buffer_control_tty_t    rx_control;
    buffer_control_tty_t    tx_control;
    unsigned char            buffer[0]; // [RX | TX]
    //unsigned char            *tx_buffer;
} shared_mem_tty_t;

extern void ccci_reset_buffers(shared_mem_tty_t *shared_mem, int size);
extern int __init ccci_tty_init(int);
extern void __exit ccci_tty_exit(int);


#endif // __CCCI_TTY_H__
