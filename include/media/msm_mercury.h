#ifndef __LINUX_MSM_MERCURY_H
#define __LINUX_MSM_MERCURY_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define MSM_MERCURY_HW_VERSION_REG  0x0004/* this offset does not exist in HW*/

#define OUTPUT_H2V1  0
#define OUTPUT_H2V2  1
#define OUTPUT_BYTE  6

#define MSM_MERCURY_MODE_REALTIME_ENCODE 0
#define MSM_MERCURY_MODE_OFFLINE_ENCODE 1
#define MSM_MERCURY_MODE_REALTIME_ROTATION 2
#define MSM_MERCURY_MODE_OFFLINE_ROTATION 3

#define MSM_MERCURY_EVT_RESET       1
#define MSM_MERCURY_EVT_FRAMEDONE	2
#define MSM_MERCURY_EVT_ERR         3
#define MSM_MERCURY_EVT_UNBLOCK     4

#define MSM_MERCURY_HW_CMD_TYPE_READ      0
#define MSM_MERCURY_HW_CMD_TYPE_WRITE     1
#define MSM_MERCURY_HW_CMD_TYPE_WRITE_OR  2
#define MSM_MERCURY_HW_CMD_TYPE_UWAIT     3
#define MSM_MERCURY_HW_CMD_TYPE_MWAIT     4
#define MSM_MERCURY_HW_CMD_TYPE_MDELAY    5
#define MSM_MERCURY_HW_CMD_TYPE_UDELAY    6

#define MSM_MCR_IOCTL_MAGIC 'g'

#define MSM_MCR_IOCTL_GET_HW_VERSION \
	_IOW(MSM_MCR_IOCTL_MAGIC, 1, struct msm_mercury_hw_cmd *)

#define MSM_MCR_IOCTL_RESET \
	_IOW(MSM_MCR_IOCTL_MAGIC, 2, struct msm_mercury_ctrl_cmd *)

#define MSM_MCR_IOCTL_STOP \
	_IOW(MSM_MCR_IOCTL_MAGIC, 3, struct msm_mercury_hw_cmds *)

#define MSM_MCR_IOCTL_START \
	_IOW(MSM_MCR_IOCTL_MAGIC, 4, struct msm_mercury_hw_cmds *)

#define MSM_MCR_IOCTL_INPUT_BUF_CFG \
	_IOW(MSM_MCR_IOCTL_MAGIC, 5, struct msm_mercury_buf *)

#define MSM_MCR_IOCTL_INPUT_GET \
	_IOW(MSM_MCR_IOCTL_MAGIC, 6, struct msm_mercury_buf *)

#define MSM_MCR_IOCTL_INPUT_GET_UNBLOCK \
	_IOW(MSM_MCR_IOCTL_MAGIC, 7, int)

#define MSM_MCR_IOCTL_OUTPUT_BUF_CFG \
	_IOW(MSM_MCR_IOCTL_MAGIC, 8, struct msm_mercury_buf *)

#define MSM_MCR_IOCTL_OUTPUT_GET \
	_IOW(MSM_MCR_IOCTL_MAGIC, 9, struct msm_mercury_buf *)

#define MSM_MCR_IOCTL_OUTPUT_GET_UNBLOCK \
	_IOW(MSM_MCR_IOCTL_MAGIC, 10, int)

#define MSM_MCR_IOCTL_EVT_GET \
	_IOW(MSM_MCR_IOCTL_MAGIC, 11, struct msm_mercury_ctrl_cmd *)

#define MSM_MCR_IOCTL_EVT_GET_UNBLOCK \
	_IOW(MSM_MCR_IOCTL_MAGIC, 12, int)

#define MSM_MCR_IOCTL_HW_CMD \
	_IOW(MSM_MCR_IOCTL_MAGIC, 13, struct msm_mercury_hw_cmd *)

#define MSM_MCR_IOCTL_HW_CMDS \
	_IOW(MSM_MCR_IOCTL_MAGIC, 14, struct msm_mercury_hw_cmds *)

#define MSM_MCR_IOCTL_TEST_DUMP_REGION \
	_IOW(MSM_MCR_IOCTL_MAGIC, 15, unsigned long)

struct msm_mercury_ctrl_cmd {
	uint32_t type;
	uint32_t len;
	void     *value;
};

struct msm_mercury_buf {
	uint32_t type;
	int      fd;
	void     *vaddr;
	uint32_t y_off;
	uint32_t y_len;
	uint32_t framedone_len;
	uint32_t cbcr_off;
	uint32_t cbcr_len;
	uint32_t num_of_mcu_rows;
	uint32_t offset;
};

struct msm_mercury_hw_cmd {

	uint32_t type:4;
	/* n microseconds of timeout for WAIT */
	/* n microseconds of time for DELAY */
	/* repeat n times for READ/WRITE */
	/* max is 0xFFF, 4095 */
	uint32_t n:12;
	uint32_t offset:16;
	uint32_t mask;
	union {
		/* for single READ/WRITE/WAIT, n = 1 */
		uint32_t data;
		uint32_t *pdata;/* for multiple READ/WRITE/WAIT, n > 1 */
	};
};

struct msm_mercury_hw_cmds {
	uint32_t m;	/* number of elements in the hw_cmd array */
	struct msm_mercury_hw_cmd hw_cmd[1];
};

#endif /* __LINUX_MSM_MERCURY_H */
