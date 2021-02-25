#ifndef UAPI_MILLET_H
#define UAPI_MILLET_H

#ifndef NETLINK_MILLET
#define NETLINK_MILLET		29
#endif

#define MILLET_KERNEL_ID	0x12341234
#define MILLET_USER_ID		0xabcddcba
#define UID_MIN_VALUE		10000
#define NAME_MAXLEN		32
#define EXT_LEN			6

#ifndef ENUM_BINDER_STAT
#define ENUM_BINDER_STAT
enum BINDER_STAT {
	BINDER_IN_IDLE,
	BINDER_IN_BUSY,
	BINDER_THREAD_IN_BUSY,
	BINDER_PROC_IN_BUSY,
	BINDER_IN_TRANSACTION,
	BINDER_ST_NUM,
};
#endif

enum MILLET_VERSION {
	VERSION_1_0 = 1,
	VERSION_2_0,
};

enum SIG_STAT {
	KILLED_BY_PRO,
	KILLED_BY_LMK,
};

enum MSG_TYPE {
	O_MSG,
	LOOPBACK_MSG,
	MSG_TO_USER,
	MSG_TO_KERN,
	MSG_END,
};

#define MSG_VALID(x) ((x > O_MSG) && (x < MSG_END))

enum MILLET_TYPE {
	O_TYPE,
	SIG_TYPE,
	BINDER_TYPE,
	BINDER_ST_TYPE,
	MEM_TYPE,
	PKG_TYPE,
	HANDSHK_TYPE,
	MILLET_TYPES_NUM,
};

#define TYPE_VALID(x) ((x > O_TYPE) && (x < MILLET_TYPES_NUM))
static const char *NAME_ARRAY[NAME_MAXLEN] = {
	"NULL",
	"SIG",
	"BINDER",
	"BINDER_STAT",
	"MEM",
	"PKG",
	"HANDSHK",
	"invalid",
};

enum BINDER_EXTRA_MSG {
	BINDER_BUFF_WARN,
	BINDER_REPLY,
	BINDER_TRANS,
	BINDER_THREAD_HAS_WORK,
};

enum PKG_CMD {
	CMD_NOP,
	ADD_UID,
	DEL_UID,
	CLEAR_ALL_UID,
	CMD_END,
};

struct time_stamp {
	unsigned long long sec;
	long nsec;
};

struct millet_data {
	enum MILLET_TYPE owner;
	enum MILLET_TYPE monitor;
	enum MSG_TYPE msg_type;
	unsigned long src_port;
	unsigned long dst_port;
	int uid;
	struct time_stamp tm;
	unsigned long pri[EXT_LEN];
	union {
		union {
			struct sig_s {
				void *caller_task;
				void *killed_task;
				int killed_pid;
				enum SIG_STAT reason;
			} sig;

			union {
				struct stat_s {
					void *task;
					int pid;
					int tid;
					enum BINDER_STAT reason;
				} stat;
				struct trans_s {
					void *src_task;
					void *dst_task;
					int caller_uid;
					int caller_pid;
					int caller_tid;
					int dst_pid;
					bool tf_oneway;
					unsigned int	code;
				} trans;
			} binder;

			struct pkg_s {
				int pkg_owner;
				int owner_pid;
			} pkg;

			struct mem_s {
			} mem;

			struct game_s {
			} game;

		} k_priv;

		union {
			unsigned long data;
		} comm;
	} mod;

};

struct millet_userconf{
	enum MILLET_TYPE owner;
	enum MSG_TYPE msg_type;
	unsigned long src_port;
	unsigned long dst_port;
	unsigned long pri[EXT_LEN];
	union {
		union {
			unsigned long data;
		} comm;

		union {
			struct pkg_user {
				enum PKG_CMD cmd;
				int uid;
			} pkg;

			struct binder_st_user {
				int uid;
			} binder_st;
		} u_priv;
	} mod;


};
#endif
