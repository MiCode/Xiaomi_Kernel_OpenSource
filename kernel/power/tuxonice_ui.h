/*
 * kernel/power/tuxonice_ui.h
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 */

enum {
	DONT_CLEAR_BAR,
	CLEAR_BAR
};

enum {
	/* Userspace -> Kernel */
	USERUI_MSG_ABORT = 0x11,
	USERUI_MSG_SET_STATE = 0x12,
	USERUI_MSG_GET_STATE = 0x13,
	USERUI_MSG_GET_DEBUG_STATE = 0x14,
	USERUI_MSG_SET_DEBUG_STATE = 0x15,
	USERUI_MSG_SPACE = 0x18,
	USERUI_MSG_GET_POWERDOWN_METHOD = 0x1A,
	USERUI_MSG_SET_POWERDOWN_METHOD = 0x1B,
	USERUI_MSG_GET_LOGLEVEL = 0x1C,
	USERUI_MSG_SET_LOGLEVEL = 0x1D,
	USERUI_MSG_PRINTK = 0x1E,

	/* Kernel -> Userspace */
	USERUI_MSG_MESSAGE = 0x21,
	USERUI_MSG_PROGRESS = 0x22,
	USERUI_MSG_POST_ATOMIC_RESTORE = 0x25,

	USERUI_MSG_MAX,
};

struct userui_msg_params {
	u32 a, b, c, d;
	char text[255];
};

struct ui_ops {
	char (*wait_for_key) (int timeout);
	 u32(*update_status) (u32 value, u32 maximum, const char *fmt, ...);
	void (*prepare_status) (int clearbar, const char *fmt, ...);
	void (*cond_pause) (int pause, char *message);
	void (*abort) (int result_code, const char *fmt, ...);
	void (*prepare) (void);
	void (*cleanup) (void);
	void (*message) (u32 section, u32 level, u32 normally_logged, const char *fmt, ...);
};

extern struct ui_ops *toi_current_ui;

#define toi_update_status(val, max, fmt, args...) \
 (toi_current_ui ? (toi_current_ui->update_status) (val, max, fmt, ##args) : \
	max)

#define toi_prepare_console(void) \
	do { if (toi_current_ui) \
		(toi_current_ui->prepare)(); \
	} while (0)

#define toi_cleanup_console(void) \
	do { if (toi_current_ui) \
		(toi_current_ui->cleanup)(); \
	} while (0)

#define abort_hibernate(result, fmt, args...) \
	do { if (toi_current_ui) \
		(toi_current_ui->abort)(result, fmt, ##args); \
	     else { \
		set_abort_result(result); \
	     } \
	} while (0)

#define toi_cond_pause(pause, message) \
	do { if (toi_current_ui) \
		(toi_current_ui->cond_pause)(pause, message); \
	} while (0)

#define toi_prepare_status(clear, fmt, args...) \
	do { if (toi_current_ui) \
		(toi_current_ui->prepare_status)(clear, fmt, ##args); \
	     else \
		printk(KERN_INFO fmt "%s", ##args, "\n"); \
	} while (0)

#define toi_message(sn, lev, log, fmt, a...) \
do { \
	if (toi_current_ui && (!sn || test_debug_state(sn))) \
		toi_current_ui->message(sn, lev, log, fmt, ##a); \
} while (0)

__exit void toi_ui_cleanup(void);
extern int toi_ui_init(void);
extern void toi_ui_exit(void);
extern int toi_register_ui_ops(struct ui_ops *this_ui);
extern void toi_remove_ui_ops(struct ui_ops *this_ui);
