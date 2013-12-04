#ifndef _DEMOD_H_
#define _DEMOD_H_
#include <linux/ioctl.h>
/*
 * demod_rw ioctl() argument to R/W 32 bit value
 * from demod intrnal registers
 */
struct demod_rw {
	unsigned int addr;
	unsigned int value;
	int dir;/* 1 -read;0 - write */
};

/*
 * demod_set_region ioctl() argument
 * sets base for read/write operations
 */
struct demod_set_region {
	unsigned int base;
	int dir; /* 1 -read;0 - write */
};

/*
 * defines for IOCTL functions
 * read Documentation/ioctl-number.txt
 * some random number to avoid coinciding with other ioctl numbers
 */
#define DEMOD_IOCTL_BASE					0xBB
#define DEMOD_IOCTL_RW		\
	_IOWR(DEMOD_IOCTL_BASE, 0, struct demod_rw)
#define DEMOD_IOCTL_SET_REGION		\
	_IOW(DEMOD_IOCTL_BASE, 1, struct demod_set_region)
#define DEMOD_IOCTL_RESET		\
	_IO(DEMOD_IOCTL_BASE, 2)
#endif /* _DEMOD_H_ */
