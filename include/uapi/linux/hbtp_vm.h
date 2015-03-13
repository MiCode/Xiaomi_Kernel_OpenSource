#ifndef _HBTP_VM_H
#define _HBTP_VM_H

#include <linux/input.h>

struct hbtp_vm_click {
	int x;
	int y;
	int mask;
};

#define HBTP_VM_BUTTON_LEFT  0x00000001
#define HBTP_VM_BUTTON_RIGHT 0x00000002
#define HBTP_VM_BUTTON_DOWN  0x10000000
#define HBTP_VM_BUTTON_UP    0x20000000

/* ioctls */
#define HBTP_VM_IOCTL_BASE  'V'
#define HBTP_VM_ENABLE	        _IO(HBTP_VM_IOCTL_BASE, 200)
#define HBTP_VM_DISABLE	        _IO(HBTP_VM_IOCTL_BASE, 201)
#define HBTP_VM_SET_TOUCHDATA	_IOW(HBTP_INPUT_IOCTL_BASE, 202, \
					struct hbtp_input_mt)
#define HBTP_VM_SEND_CLICK      _IOW(HBTP_INPUT_IOCTL_BASE, 203, \
					struct hbtp_vm_click)

#endif	/* _HBTP_VM_H */

