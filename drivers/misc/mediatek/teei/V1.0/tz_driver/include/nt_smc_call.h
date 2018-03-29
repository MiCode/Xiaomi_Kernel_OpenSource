/***********************************************************
 *  @ file : smc_call.h
 *  @ brief :  monitor call interface for user,
 * this implement  is updated to SMC Calling Convention doc
 * from arm ,Document number: ARM DEN 0028A 2013
 *  @ author: luocl
 *  @ author: Steven Meng
 *  @ copyright microtrust  Corporation
 *************************************************************/
#ifndef SMC_CALL_H_
#define SMC_CALL_H_

/*This field id is fixed by arm*/
#define ID_FIELD_F_FAST_SMC_CALL            1
#define ID_FIELD_F_STANDARD_SMC_CALL        0
#define ID_FIELD_W_64                       1
#define ID_FIELD_W_32                       0
#define ID_FIELD_T_ARM_SERVICE             0
#define ID_FIELD_T_CPU_SERVICE              1
#define ID_FIELD_T_SIP_SERVICE                2
#define ID_FIELD_T_OEM_SERVICE            3
#define ID_FIELD_T_STANDARD_SERVICE          4

/*TA Call 48-49*/
#define ID_FIELD_T_TA_SERVICE0              48
#define ID_FIELD_T_TA_SERVICE1              49
/*TOS Call 50-63*/
#define ID_FIELD_T_TRUSTED_OS_SERVICE0      50
#define ID_FIELD_T_TRUSTED_OS_SERVICE1      51

#define ID_FIELD_T_TRUSTED_OS_SERVICE2      52
#define ID_FIELD_T_TRUSTED_OS_SERVICE3      53

#define MAKE_SMC_CALL_ID(F, W, T, FN) (((F)<<31)|((W)<<30)|((T)<<24)|(FN))

#define SMC_CALL_RTC_OK                 0x0
#define SMC_CALL_RTC_UNKNOWN_FUN        0xFFFFFFFF
#define SMC_CALL_RTC_MONITOR_NOT_READY  0xFFFFFFFE

/*For t side  Fast Call*/
#define T_BOOT_NT_OS              \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32,  ID_FIELD_T_TRUSTED_OS_SERVICE0, 0)
#define T_ACK_N_OS_READY    \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32,  ID_FIELD_T_TRUSTED_OS_SERVICE0, 1)
#define T_GET_PARAM_IN       \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32,  ID_FIELD_T_TRUSTED_OS_SERVICE0, 2)
#define T_ACK_T_OS_FOREGROUND   \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE0, 3)
#define T_ACK_T_OS_BACKSTAGE   \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE0, 4)
#define T_ACK_N_FAST_CALL	  \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE0, 5)
#define T_DUMP_STATE	  \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE0, 6)
#define T_ACK_N_INIT_FC_BUF  \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE0, 7)
#define T_GET_BOOT_PARMS      \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE0, 8)

/*For t side  Standard Call*/
#define T_SCHED_NT			\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 0)
#define T_ACK_N_SYS_CTL		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 1)
#define T_ACK_N_NQ		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 2)
#define T_ACK_N_INVOKE_DRV	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 3)
#define T_INVOKE_N_DRV		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 4)
#define T_RAISE_N_EVENT		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 5)
#define T_ACK_N_BOOT_OK		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 6)
#define T_INVOKE_N_LOAD_IMG	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 7)
#define T_ACK_N_KERNEL_OK		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 8)
#define T_SCHED_NT_IRQ			\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE1, 9)

/*For nt side Fast Call*/
#define N_SWITCH_TO_T_OS_STAGE2	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 0)
#define N_CPU_CBOOT		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_CPU_SERVICE, 0)
#define N_CPU_SUSPEND		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_CPU_SERVICE, 1)
#define N_CPU_ON		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_CPU_SERVICE, 2)
#define N_CPU_OFF		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_CPU_SERVICE, 3)
#define N_GET_PARAM_IN		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 1)
#define N_INIT_T_FC_BUF		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 2)
#define N_INVOKE_T_FAST_CALL	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 3)
#define NT_DUMP_STATE		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 4)
#define N_ACK_T_FOREGROUND	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 5)
#define N_ACK_T_BACKSTAGE	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 6)
#define N_INIT_T_BOOT_STAGE1	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 7)
#define N_SWITCH_CORE \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 8)
#define N_GET_NON_IRQ_NUM      \
		MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE2, 9)


/*For nt side Standard Call*/
#define NT_SCHED_T		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 0)
#define N_INVOKE_T_SYS_CTL	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 1)
#define N_INVOKE_T_NQ		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 2)
#define N_INVOKE_T_DRV		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 3)
#define N_RAISE_T_EVENT		\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 4)
#define N_ACK_T_INVOKE_DRV	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 5)
#define N_INVOKE_T_LOAD_TEE	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 6)
#define N_ACK_T_LOAD_IMG	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 7)
#define NT_SCHED_T_FIQ	\
		MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_64, ID_FIELD_T_TRUSTED_OS_SERVICE3, 8)

/*For nt side Fast Call*/
#define N_SWITCH_TO_T_OS_STAGE2_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 0)
#define N_CPU_CBOOT_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_CPU_SERVICE, 0)
#define N_CPU_SUSPEND_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_CPU_SERVICE, 1)
#define N_CPU_ON_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_CPU_SERVICE, 2)
#define N_CPU_OFF_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_CPU_SERVICE, 3)
#define N_GET_PARAM_IN_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 1)
#define N_INIT_T_FC_BUF_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 2)
#define N_INVOKE_T_FAST_CALL_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 3)
#define NT_DUMP_STATE_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 4)
#define N_ACK_T_FOREGROUND_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 5)
#define N_ACK_T_BACKSTAGE_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 6)
#define N_INIT_T_BOOT_STAGE1_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 7)
#define N_SWITCH_CORE_32 \
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 8)
#define N_GET_NON_IRQ_NUM_32      \
	MAKE_SMC_CALL_ID(ID_FIELD_F_FAST_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE2, 9)


/*For nt side Standard Call*/
#define NT_SCHED_T_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 0)
#define N_INVOKE_T_SYS_CTL_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 1)
#define N_INVOKE_T_NQ_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 2)
#define N_INVOKE_T_DRV_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 3)
#define N_RAISE_T_EVENT_32		\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 4)
#define N_ACK_T_INVOKE_DRV_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 5)
#define N_INVOKE_T_LOAD_TEE_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 6)
#define N_ACK_T_LOAD_IMG_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 7)
#define NT_SCHED_T_FIQ_32	\
	MAKE_SMC_CALL_ID(ID_FIELD_F_STANDARD_SMC_CALL, ID_FIELD_W_32, ID_FIELD_T_TRUSTED_OS_SERVICE3, 8)


/* ////////////////////////////////////////////////////////////////////////////////////////////////////////////// */
#ifdef CONFIG_ARM64
/*  ==================  NT FAST CALL ================   */
static inline void n_init_t_boot_stage1(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INIT_T_BOOT_STAGE1), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_switch_to_t_os_stage2(void)
{
	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"mov x1, #0\n\t"
	"mov x2, #0\n\t"
	"mov x3, #0\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_SWITCH_TO_T_OS_STAGE2)
	: "x0", "x1", "x2", "x3",  "memory");
}

static inline void nt_dump_state(void)
{
	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"mov x1, #0\n\t"
	"mov x2, #0\n\t"
	"mov x3, #0\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (NT_DUMP_STATE)
	: "x0", "x1", "x2", "x3",  "memory");
}

static inline void n_get_param_in(
	uint64_t *rtc0,
	uint64_t *rtc1,
	uint64_t *rtc2,
	uint64_t *rtc3)
{
	uint64_t temp[4];

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov r0, %[fun_id]\n\t"
	"mov x1, #0\n\t"
	"mov x2, #0\n\t"
	"mov x3, #0\n\t"
	"smc 0\n\t"
	"nop"
	"str x0, [%[temp]]\n\t"
	"str x1, [%[temp], #8]\n\t"
	"str x2, [%[temp], #16]\n\t"
	"str x3, [%[temp], #24]\n\t"
	: :
	[fun_id] "r" (N_GET_PARAM_IN), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");

	*rtc0 = temp[0];
	*rtc1 = temp[1];
	*rtc2 = temp[2];
	*rtc3 = temp[3];

}
static inline void n_init_t_fc_buf(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INIT_T_FC_BUF), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}
static inline void n_invoke_t_fast_call(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INVOKE_T_FAST_CALL), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

/*  ==================  NT STANDARD CALL ================   */
static inline void nt_sched_t(void)
{
	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"mov x1, #0\n\t"
	"mov x2, #0\n\t"
	"mov x3, #0\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (NT_SCHED_T)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_invoke_t_sys_ctl(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INVOKE_T_SYS_CTL), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_invoke_t_nq(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INVOKE_T_NQ), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_invoke_t_drv(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INVOKE_T_DRV), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_raise_t_event(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_RAISE_T_EVENT), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_ack_t_invoke_drv(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_ACK_T_INVOKE_DRV), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_invoke_t_load_tee(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_INVOKE_T_LOAD_TEE), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void n_ack_t_load_img(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_ACK_T_LOAD_IMG), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void nt_sched_t_fiq(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (NT_SCHED_T_FIQ), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}


static inline void nt_sched_core(
	uint64_t p0,
	uint64_t p1,
	uint64_t p2)
{
	uint64_t temp[3];
	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;

	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"ldr x1, [%[temp], #0]\n\t"
	"ldr x2, [%[temp], #8]\n\t"
	"ldr x3, [%[temp], #16]\n\t"
	"smc 0\n\t"
	"nop"
	: :
	[fun_id] "r" (N_SWITCH_CORE), [temp] "r" (temp)
	: "x0", "x1", "x2", "x3", "memory");
}

static inline void nt_get_non_irq_num (uint64_t *p0)
{
	uint64_t temp[3];
	__asm__ volatile(
	/* ".arch_extension sec\n" */
	"mov x0, %[fun_id]\n\t"
	"smc 0\n\t"
	"str x1, [%[temp], #0]\n\t"
	"nop"
	: :
	[fun_id] "r" (N_GET_NON_IRQ_NUM), [temp] "r" (temp)
	: "x0", "x1", "memory");
	*p0 = temp[0];
}





/* ///////////////////////////////////////////////////////////////////////////////////////////////////////////// */


#else

static inline void smc_out(uint32_t id,
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	uint32_t fun_id = (uint32_t)id;
	uint32_t temp[3];

	temp[0] = p0;
	temp[1] = p1;
	temp[2] = p2;


	__asm__ volatile(
			".arch_extension sec\n"
			"mov r0, %[fun_id]\n\t"
			"ldr r1, [%[temp], #0]\n\t"
			"ldr r2, [%[temp], #4]\n\t"
			"ldr r3, [%[temp], #8]\n\t"
			"smc 0\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop"
			: :
			[fun_id] "r" (fun_id), [temp] "r" (temp)
			: "r0", "r1", "r2", "r3",  "memory");
}


static inline void smc_in(
		uint32_t id,
		uint32_t *rtc0,
		uint32_t *rtc1,
		uint32_t *rtc2,
		uint32_t *rtc3)
{
	uint32_t fun_id = (uint32_t)id;
	uint32_t temp[4];
	*rtc0 =  0;
	*rtc1 =  0;
	*rtc2 =  0;
	*rtc3 =  0;

	__asm__ volatile(
			".arch_extension sec\n"
			"mov r0, %[fun_id]\n\t"
			"mov r1, #0\n\t"
			"mov r2, #0\n\t"
			"mov r3, #0\n\t"
			"smc 0\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"str r0, [%[temp]]\n\t"
			"str r1, [%[temp], #4]\n\t"
			"str r2, [%[temp], #8]\n\t"
			"str r3, [%[temp], #12]\n\t"
			: :
			[fun_id] "r" (fun_id), [temp] "r" (temp)
			: "r0", "r1", "r2", "r3", "memory");
	*rtc0 =  temp[0];
	*rtc1 =  temp[1];
	*rtc2 =  temp[2];
	*rtc3 =  temp[3];


}



/*  ==================  NT FAST CALL ================   */
static inline void n_init_t_boot_stage1(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INIT_T_BOOT_STAGE1_32, p0, p1, p2);

}

static inline void n_switch_to_t_os_stage2(void)
{
	smc_out(N_SWITCH_TO_T_OS_STAGE2_32, 0, 0, 0);
}

static inline void nt_dump_state(void)
{

	smc_out(NT_DUMP_STATE_32, 0, 0, 0);
}

static inline void n_get_param_in(
		uint32_t *rtc0,
		uint32_t *rtc1,
		uint32_t *rtc2,
		uint32_t *rtc3)
{
	smc_in(N_GET_PARAM_IN_32, rtc0, rtc1, rtc2, rtc3);

}


static inline void n_init_t_fc_buf(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INIT_T_FC_BUF_32, p0, p1, p2);

}
static inline void n_invoke_t_fast_call(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INVOKE_T_FAST_CALL_32, p0, p1, p2);
}

/*  ==================  NT STANDARD CALL ================   */
static inline void nt_sched_t(void)
{
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;

	smc_out(NT_SCHED_T_32, p0, p1, p2);

}

static inline void n_invoke_t_sys_ctl(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INVOKE_T_SYS_CTL_32, p0, p1, p2);

}

static inline void n_invoke_t_nq(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INVOKE_T_NQ_32, p0, p1, p2);

}

static inline void n_invoke_t_drv(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INVOKE_T_DRV_32, p0, p1, p2);

}

static inline void n_raise_t_event(
		uint64_t p0,
		uint64_t p1,
		uint64_t p2)
{
	smc_out(N_RAISE_T_EVENT_32, p0, p1, p2);

}

static inline void n_ack_t_invoke_drv(
		uint64_t p0,
		uint64_t p1,
		uint64_t p2)
{
	smc_out(N_ACK_T_INVOKE_DRV_32, p0, p1, p2);

}

static inline void n_invoke_t_load_tee(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_INVOKE_T_LOAD_TEE_32, p0, p1, p2);

}

static inline void n_ack_t_load_img(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_ACK_T_LOAD_IMG_32, p0, p1, p2);

}

static inline void nt_sched_t_fiq(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(NT_SCHED_T_FIQ_32, p0, p1, p2);

}


static inline void nt_sched_core(
		uint32_t p0,
		uint32_t p1,
		uint32_t p2)
{
	smc_out(N_SWITCH_CORE_32, p0, p1, p2);

}

static inline void nt_get_non_irq_num(uint32_t *p0)
{
	uint32_t rtc1;
	uint32_t rtc2;
	uint32_t rtc3;

	smc_in(N_GET_NON_IRQ_NUM_32, &rtc1, p0, &rtc2, &rtc3);
}


#endif
#endif /* SMC_CALL_H_ */
