#ifndef __QG_H__
#define __QG_H__

#define MAX_FIFO_LENGTH		16

enum qg {
	QG_SOC,
	QG_OCV_UV,
	QG_RBAT_MOHM,
	QG_PON_OCV_UV,
	QG_GOOD_OCV_UV,
	QG_ESR,
	QG_CHARGE_COUNTER,
	QG_FIFO_TIME_DELTA,
	QG_RESERVED_1,
	QG_RESERVED_2,
	QG_RESERVED_3,
	QG_RESERVED_4,
	QG_RESERVED_5,
	QG_RESERVED_6,
	QG_RESERVED_7,
	QG_RESERVED_8,
	QG_RESERVED_9,
	QG_RESERVED_10,
	QG_MAX,
};

struct fifo_data {
	unsigned int			v;
	unsigned int			i;
	unsigned int			count;
	unsigned int			interval;
};

struct qg_param {
	unsigned int			data;
	bool				valid;
};

struct qg_kernel_data {
	unsigned int			fifo_length;
	struct fifo_data		fifo[MAX_FIFO_LENGTH];
	struct qg_param			param[QG_MAX];
};

struct qg_user_data {
	struct qg_param			param[QG_MAX];
};

#endif
