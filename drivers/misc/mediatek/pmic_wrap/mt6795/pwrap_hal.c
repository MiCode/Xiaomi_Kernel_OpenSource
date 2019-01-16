/******************************************************************************
 * pwrap_hal.c - Linux pmic_wrapper Driver,hardware_dependent driver
 *
 *
 * DESCRIPTION:
 *     This file provid the other drivers PMIC wrapper relative functions
 *
 ******************************************************************************/

#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/io.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <mach/mt_pmic_wrap.h>
#include <mach/mt_typedefs.h>
#include "pwrap_hal.h"

#define PMIC_WRAP_DEVICE "pmic_wrap"

/*-----start-- global variable-------------------------------------------------*/
#ifdef CONFIG_OF
void __iomem *pwrap_base;
static void __iomem *topckgen_base;
static void __iomem *infracfg_ao_base;
#endif
static struct mt_pmic_wrap_driver *mt_wrp;

static spinlock_t	wrp_lock = __SPIN_LOCK_UNLOCKED(lock);
//spinlock_t	wrp_lock = __SPIN_LOCK_UNLOCKED(lock);
//----------interral API ------------------------
static S32 	_pwrap_init_dio( U32 dio_en );
static S32 	_pwrap_init_cipher( void );
static S32 	_pwrap_init_reg_clock( U32 regck_sel );

static S32 	_pwrap_wacs2_nochk( U32 write, U32 adr, U32 wdata, U32 *rdata );
S32 	pwrap_write_nochk( U32  adr, U32  wdata );
S32 	pwrap_read_nochk( U32  adr, U32 *rdata );
#ifdef CONFIG_OF
static int pwrap_of_iomap(void);
static void pwrap_of_iounmap(void);
#endif

#ifdef PMIC_WRAP_NO_PMIC
/*-pwrap debug--------------------------------------------------------------------------*/
static inline void pwrap_dump_all_register(void)
{
	return;
}
/********************************************************************************************/
//extern API for PMIC driver, INT related control, this INT is for PMIC chip to AP
/********************************************************************************************/
U32 mt_pmic_wrap_eint_status(void)
{
	PWRAPLOG("[PMIC2]first of all PMIC_WRAP_STAUPD_GRPEN=0x%x\n", WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN));
	PWRAPLOG("[PMIC2]first of all PMIC_WRAP_EINT_STA=0x%x\n", WRAP_RD32(PMIC_WRAP_EINT_STA));
	return WRAP_RD32(PMIC_WRAP_EINT_STA);
}

void mt_pmic_wrap_eint_clr(int offset)
{
	if((offset<0)||(offset>3)) {
		PWRAPERR("clear EINT flag error, only 0-3 bit\n");
	}else {
		WRAP_WR32(PMIC_WRAP_EINT_CLR,(1<<offset));
	}
}

//--------------------------------------------------------
//    Function : pwrap_wacs2_hal()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
static S32 pwrap_wacs2_hal( U32  write, U32  adr, U32  wdata, U32 *rdata )
{
	PWRAPERR("there is no PMIC real chip,PMIC_WRAP do Nothing\n");
	return 0;
}
/*
 *pmic_wrap init,init wrap interface
 *
 */
S32 pwrap_init ( void )
{
	return 0;
}
//EXPORT_SYMBOL(pwrap_init);
/*Interrupt handler function*/
static irqreturn_t mt_pmic_wrap_irq(int irqno, void *dev_id)
{
	unsigned long flags=0;

	PWRAPFUC();
	PWRAPREG("dump pwrap register\n");
	spin_lock_irqsave(&wrp_lock,flags);
	//*-----------------------------------------------------------------------
	pwrap_dump_all_register();
	//raise the priority of WACS2 for AP
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO,1<<3);

	//*-----------------------------------------------------------------------
	//clear interrupt flag
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xffffffff);
	PWRAPREG("INT flag 0x%x\n",WRAP_RD32(PMIC_WRAP_INT_EN));
	//BUG_ON(1);
	spin_unlock_irqrestore(&wrp_lock,flags);
	return IRQ_HANDLED;
}

static U32 pwrap_read_test(void)
{
	return 0;
}
static U32 pwrap_write_test(void)
{
	return 0;
}

static void pwrap_ut(U32 ut_test)
{
	switch(ut_test)
	{
		case 1:
			//pwrap_wacs2_para_test();
			pwrap_write_test();
			break;
		case 2:
			//pwrap_wacs2_para_test();
			pwrap_read_test();
			break;

		default:
			PWRAPREG ( "default test.\n" );
			break;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static S32 mt_pwrap_show_hal(char *buf)
{
	PWRAPFUC();
	return snprintf(buf, PAGE_SIZE, "%s\n","no implement" );
}
/*---------------------------------------------------------------------------*/
static S32 mt_pwrap_store_hal(const char *buf,size_t count)
{
	U32 reg_value=0;
	U32 reg_addr=0;
	U32 return_value=0;
	U32 ut_test=0;
	if(!strncmp(buf, "-h", 2))
	{
		PWRAPREG("PWRAP debug: [-dump_reg][-trace_wacs2][-init][-rdap][-wrap][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		PWRAPREG("PWRAP UT: [1][2]\n");
	}
	//--------------------------------------pwrap debug-------------------------------------------------------------
	else if(!strncmp(buf, "-dump_reg", 9))
	{
		pwrap_dump_all_register();
	}
	else if(!strncmp(buf, "-trace_wacs2", 12))
	{
		//pwrap_trace_wacs2();
	}
	else if(!strncmp(buf, "-init", 5))
	{
		return_value=pwrap_init();
		if(return_value==0)
			PWRAPREG("pwrap_init pass,return_value=%d\n",return_value);
		else
			PWRAPREG("pwrap_init fail,return_value=%d\n",return_value);
	}
	else if (!strncmp(buf, "-rdap", 5) && (1 == sscanf(buf+5, "%x", &reg_addr)))
	{
		//pwrap_read_reg_on_ap(reg_addr);
	}
	else if (!strncmp(buf, "-wrap", 5) && (2 == sscanf(buf+5, "%x %x", &reg_addr,&reg_value)))
	{
		//pwrap_write_reg_on_ap(reg_addr,reg_value);
	}
	else if (!strncmp(buf, "-rdpmic", 7) && (1 == sscanf(buf+7, "%x", &reg_addr)))
	{
		//pwrap_read_reg_on_pmic(reg_addr);
	}
	else if (!strncmp(buf, "-wrpmic", 7) && (2 == sscanf(buf+7, "%x %x", &reg_addr,&reg_value)))
	{
		//pwrap_write_reg_on_pmic(reg_addr,reg_value);
	}
	else if(!strncmp(buf, "-readtest", 9))
	{
		pwrap_read_test();
	}
	else if(!strncmp(buf, "-writetest", 10))
	{
		pwrap_write_test();
	}
	//--------------------------------------pwrap UT-------------------------------------------------------------
	else if (!strncmp(buf, "-ut", 3) && (1 == sscanf(buf+3, "%d", &ut_test)))
	{
		pwrap_ut(ut_test);
	}else{
		PWRAPREG("wrong parameter\n");
	}
	return count;
}
/*---------------------------------------------------------------------------*/
#else
/*-pwrap debug--------------------------------------------------------------------------*/
static inline void pwrap_dump_ap_register(void)
{
	U32 i=0;
	PWRAPREG("dump pwrap register, base=0x%p\n",PMIC_WRAP_BASE);
	PWRAPREG("address     :   3 2 1 0    7 6 5 4    B A 9 8    F E D C \n");
	for(i=0;i<=0x150;i+=16)
	{
		PWRAPREG("offset 0x%.3x:0x%.8x 0x%.8x 0x%.8x 0x%.8x \n",i,
				WRAP_RD32(PMIC_WRAP_BASE+i+0),
				WRAP_RD32(PMIC_WRAP_BASE+i+4),
				WRAP_RD32(PMIC_WRAP_BASE+i+8),
				WRAP_RD32(PMIC_WRAP_BASE+i+12));
	}
	//PWRAPREG("elapse_time=%llx(ns)\n",elapse_time);
	return;
}
static inline void pwrap_dump_pmic_register(void)
{
	//	U32 i=0;
	//	U32 reg_addr=0;
	//	U32 reg_value=0;
	//
	//	PWRAPREG("dump dewrap register\n");
	//	for(i=0;i<=14;i++)
	//	{
	//		reg_addr=(DEW_BASE+i*4);
	//		reg_value=pwrap_read_nochk(reg_addr,&reg_value);
	//		PWRAPREG("0x%x=0x%x\n",reg_addr,reg_value);
	//	}
	return;
}
static inline void pwrap_dump_all_register(void)
{
	pwrap_dump_ap_register();
	pwrap_dump_pmic_register();
	return;
}
static void __pwrap_soft_reset(void)
{
	PWRAPLOG("start reset wrapper\n");
	PWRAP_SOFT_RESET;
	PWRAPLOG("the reset register =%x\n",WRAP_RD32(INFRA_GLOBALCON_RST0));
	PWRAPLOG("PMIC_WRAP_STAUPD_GRPEN =0x%x,it should be equal to 0xc\n",WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN));
	//clear reset bit
	PWRAP_CLEAR_SOFT_RESET_BIT;
	return;
}
/******************************************************************************
  wrapper timeout
 ******************************************************************************/
#define PWRAP_TIMEOUT
#ifdef PWRAP_TIMEOUT
static U64 _pwrap_get_current_time(void)
{
	return sched_clock();   ///TODO: fix me
}
//U64 elapse_time=0;

static BOOL _pwrap_timeout_ns (U64 start_time_ns, U64 timeout_time_ns)
{
	U64 cur_time=0;
	U64 elapse_time=0;

	// get current tick
	cur_time = _pwrap_get_current_time();//ns

	//avoid timer over flow exiting in FPGA env
	if(cur_time < start_time_ns){
		PWRAPERR("@@@@Timer overflow! start%lld cur timer%lld\n",start_time_ns,cur_time);
		start_time_ns=cur_time;
		timeout_time_ns=2000*1000; //2000us
		PWRAPERR("@@@@reset timer! start%lld setting%lld\n",start_time_ns,timeout_time_ns);
	}
		
	elapse_time=cur_time-start_time_ns;

	// check if timeout
	if (timeout_time_ns <= elapse_time)
	{
		// timeout
		PWRAPERR("@@@@Timeout: elapse time%lld,start%lld setting timer%lld\n",
				elapse_time,start_time_ns,timeout_time_ns);
		return TRUE;
	}
	return FALSE;
}
static U64 _pwrap_time2ns (U64 time_us)
{
	return time_us*1000;
}

#else
static U64 _pwrap_get_current_time(void)
{
	return 0;
}
static BOOL _pwrap_timeout_ns (U64 start_time_ns, U64 elapse_time)//,U64 timeout_ns)
{
	return FALSE;
}
static U64 _pwrap_time2ns (U64 time_us)
{
	return 0;
}

#endif
//#####################################################################
//define macro and inline function (for do while loop)
//#####################################################################
typedef U32 (*loop_condition_fp)(U32);//define a function pointer

static inline U32 wait_for_fsm_idle(U32 x)
{
	return (GET_WACS0_FSM( x ) != WACS_FSM_IDLE );
}
static inline U32 wait_for_fsm_vldclr(U32 x)
{
	return (GET_WACS0_FSM( x ) != WACS_FSM_WFVLDCLR);
}
static inline U32 wait_for_sync(U32 x)
{
	return (GET_SYNC_IDLE0(x) != WACS_SYNC_IDLE);
}
static inline U32 wait_for_idle_and_sync(U32 x)
{
	return ((GET_WACS2_FSM(x) != WACS_FSM_IDLE) || (GET_SYNC_IDLE2(x) != WACS_SYNC_IDLE)) ;
}
static inline U32 wait_for_wrap_idle(U32 x)
{
	return ((GET_WRAP_FSM(x) != 0x0) || (GET_WRAP_CH_DLE_RESTCNT(x) != 0x0));
}
static inline U32 wait_for_wrap_state_idle(U32 x)
{
	return ( GET_WRAP_AG_DLE_RESTCNT( x ) != 0 ) ;
}
static inline U32 wait_for_man_idle_and_noreq(U32 x)
{
	return ( (GET_MAN_REQ(x) != MAN_FSM_NO_REQ ) || (GET_MAN_FSM(x) != MAN_FSM_IDLE) );
}
static inline U32 wait_for_man_vldclr(U32 x)
{
	return  (GET_MAN_FSM( x ) != MAN_FSM_WFVLDCLR) ;
}
static inline U32 wait_for_cipher_ready(U32 x)
{
	return (x!=3) ;
}
static inline U32 wait_for_stdupd_idle(U32 x)
{
	return ( GET_STAUPD_FSM(x) != 0x0) ;
}

static inline U32 wait_for_state_ready_init(loop_condition_fp fp,U32 timeout_us,void *wacs_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata=0x0;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait_for_state_ready_init timeout when waiting for idle\n");
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
	} while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}

static inline U32 wait_for_state_idle_init(loop_condition_fp fp,U32 timeout_us,void *wacs_register,void *wacs_vldclr_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait_for_state_idle_init timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			//pwrap_trace_wacs2();
			//BUG_ON(1);
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		//if last read command timeout,clear vldclr bit
		//read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle
		switch ( GET_WACS0_FSM( reg_rdata ) )
		{
			case WACS_FSM_WFVLDCLR:
				WRAP_WR32(wacs_vldclr_register , 1);
				PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
				break;
			case WACS_FSM_WFDLE:
				PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
				break;
			case WACS_FSM_REQ:
				PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
				break;
			default:
				break;
		}
	}while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}
static inline U32 wait_for_state_idle(loop_condition_fp fp,U32 timeout_us,void *wacs_register,void *wacs_vldclr_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait_for_state_idle timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			//pwrap_trace_wacs2();
			//BUG_ON(1);
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);
		if( GET_INIT_DONE0( reg_rdata ) != WACS_INIT_DONE)
		{
			PWRAPERR("initialization isn't finished \n");
			return E_PWR_NOT_INIT_DONE;
		}
		//if last read command timeout,clear vldclr bit
		//read command state machine:FSM_REQ-->wfdle-->WFVLDCLR;write:FSM_REQ-->idle
		switch ( GET_WACS0_FSM( reg_rdata ) )
		{
			case WACS_FSM_WFVLDCLR:
				WRAP_WR32(wacs_vldclr_register , 1);
				PWRAPERR("WACS_FSM = PMIC_WRAP_WACS_VLDCLR\n");
				break;
			case WACS_FSM_WFDLE:
				PWRAPERR("WACS_FSM = WACS_FSM_WFDLE\n");
				break;
			case WACS_FSM_REQ:
				PWRAPERR("WACS_FSM = WACS_FSM_REQ\n");
				break;
			default:
				break;
		}
	}while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}
/********************************************************************************************/
//extern API for PMIC driver, INT related control, this INT is for PMIC chip to AP
/********************************************************************************************/
U32 mt_pmic_wrap_eint_status(void)
{
	return WRAP_RD32(PMIC_WRAP_EINT_STA);
}

void mt_pmic_wrap_eint_clr(int offset)
{
	if((offset<0)||(offset>3)) {
		PWRAPERR("clear EINT flag error, only 0-3 bit\n");
	}else {
		WRAP_WR32(PMIC_WRAP_EINT_CLR,(1<<offset));
	}
	PWRAPREG("clear EINT flag mt_pmic_wrap_eint_status=0x%x\n", WRAP_RD32(PMIC_WRAP_EINT_STA));
}

static inline U32 wait_for_state_ready(loop_condition_fp fp,U32 timeout_us,void *wacs_register,U32 *read_reg)
{

	U64 start_time_ns=0, timeout_ns=0;
	U32 reg_rdata;
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(timeout_us);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("timeout when waiting for idle\n");
			pwrap_dump_ap_register();
			//pwrap_trace_wacs2();
			return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		reg_rdata = WRAP_RD32(wacs_register);

		if( GET_INIT_DONE0( reg_rdata ) != WACS_INIT_DONE)
		{
			PWRAPERR("initialization isn't finished \n");
			return E_PWR_NOT_INIT_DONE;
		}
	} while( fp(reg_rdata)); //IDLE State
	if(read_reg)
		*read_reg=reg_rdata;
	return 0;
}

//--------------------------------------------------------
//    Function : pwrap_wacs2_hal()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
static S32 pwrap_wacs2_hal( U32  write, U32  adr, U32  wdata, U32 *rdata )
{
	//U64 wrap_access_time=0x0;
	U32 reg_rdata=0;
	U32 wacs_write=0;
	U32 wacs_adr=0;
	U32 wacs_cmd=0;
	U32 return_value=0;
	unsigned long flags=0;
	//PWRAPFUC();
	//#ifndef CONFIG_MTK_LDVT_PMIC_WRAP
	//PWRAPLOG("wrapper access,write=%x,add=%x,wdata=%x,rdata=%x\n",write,adr,wdata,rdata);
	//#endif
	// Check argument validation
	if( (write & ~(0x1))    != 0)  return E_PWR_INVALID_RW;
	if( (adr   & ~(0xffff)) != 0)  return E_PWR_INVALID_ADDR;
	if( (wdata & ~(0xffff)) != 0)  return E_PWR_INVALID_WDAT;

	spin_lock_irqsave(&wrp_lock,flags);
	// Check IDLE & INIT_DONE in advance
	return_value=wait_for_state_idle(wait_for_fsm_idle,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,PMIC_WRAP_WACS2_VLDCLR,0);
	if(return_value!=0)
	{
		PWRAPERR("wait_for_fsm_idle fail,return_value=%d\n",return_value);
		goto FAIL;
	}
	wacs_write  = write << 31;
	wacs_adr    = (adr >> 1) << 16;
	wacs_cmd= wacs_write | wacs_adr | wdata;

	WRAP_WR32(PMIC_WRAP_WACS2_CMD,wacs_cmd);
	if( write == 0 )
	{
		if (NULL == rdata)
		{
			PWRAPERR("rdata is a NULL pointer\n");
			return_value= E_PWR_INVALID_ARG;
			goto FAIL;
		}
		return_value=wait_for_state_ready(wait_for_fsm_vldclr,TIMEOUT_READ,PMIC_WRAP_WACS2_RDATA,&reg_rdata);
		if(return_value!=0)
		{
			PWRAPERR("wait_for_fsm_vldclr fail,return_value=%d\n",return_value);
			return_value+=1;//E_PWR_NOT_INIT_DONE_READ or E_PWR_WAIT_IDLE_TIMEOUT_READ
			goto FAIL;
		}
		*rdata = GET_WACS0_RDATA( reg_rdata );
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR , 1);
	}
	//spin_unlock_irqrestore(&wrp_lock,flags);
FAIL:
	spin_unlock_irqrestore(&wrp_lock,flags);
	if(return_value!=0)
	{
		PWRAPERR("pwrap_wacs2_hal fail,return_value=%d\n",return_value);
		PWRAPERR("timeout:BUG_ON here\n");
	//BUG_ON(1);
	}
	//wrap_access_time=sched_clock();
	//pwrap_trace(wrap_access_time,return_value,write, adr, wdata,(U32)rdata);
	return return_value;
}

//S32 pwrap_wacs2( U32  write, U32  adr, U32  wdata, U32 *rdata )
//{
//	return pwrap_wacs2_hal(write, adr,wdata,rdata );
//}
//EXPORT_SYMBOL(pwrap_wacs2);
//S32 pwrap_read( U32  adr, U32 *rdata )
//{
//	return pwrap_wacs2( PWRAP_READ, adr,0,rdata );
//}
//EXPORT_SYMBOL(pwrap_read);
//
//S32 pwrap_write( U32  adr, U32  wdata )
//{
//	return pwrap_wacs2( PWRAP_WRITE, adr,wdata,0 );
//}
//EXPORT_SYMBOL(pwrap_write);
//******************************************************************************
//--internal API for pwrap_init-------------------------------------------------
//******************************************************************************
//--------------------------------------------------------
//    Function : _pwrap_wacs2_nochk()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------

//static S32 pwrap_read_nochk( U32  adr, U32 *rdata )
S32 pwrap_read_nochk( U32  adr, U32 *rdata )
{
	return _pwrap_wacs2_nochk( 0, adr,  0, rdata );
}
/*EXPORT_SYMBOL(pwrap_read_nochk);*/

S32 pwrap_write_nochk( U32  adr, U32  wdata )
{
	return _pwrap_wacs2_nochk( 1, adr,wdata,0 );
}
/*EXPORT_SYMBOL(pwrap_write_nochk);*/

static S32 _pwrap_wacs2_nochk( U32 write, U32 adr, U32 wdata, U32 *rdata )
{
	U32 reg_rdata=0x0;
	U32 wacs_write=0x0;
	U32 wacs_adr=0x0;
	U32 wacs_cmd=0x0;
	U32 return_value=0x0;
	//PWRAPFUC();
	// Check argument validation
	if( (write & ~(0x1))    != 0)  return E_PWR_INVALID_RW;
	if( (adr   & ~(0xffff)) != 0)  return E_PWR_INVALID_ADDR;
	if( (wdata & ~(0xffff)) != 0)  return E_PWR_INVALID_WDAT;

	// Check IDLE
	return_value=wait_for_state_ready_init(wait_for_fsm_idle,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,0);
	if(return_value!=0)
	{
		PWRAPERR("_pwrap_wacs2_nochk write command fail,return_value=%x\n", return_value);
		return return_value;
	}

	wacs_write  = write << 31;
	wacs_adr    = (adr >> 1) << 16;
	wacs_cmd = wacs_write | wacs_adr | wdata;
	WRAP_WR32(PMIC_WRAP_WACS2_CMD,wacs_cmd);

	if( write == 0 )
	{
		if (NULL == rdata)
			return E_PWR_INVALID_ARG;
		// wait for read data ready
		return_value=wait_for_state_ready_init(wait_for_fsm_vldclr,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,&reg_rdata);
		if(return_value!=0)
		{
			PWRAPERR("_pwrap_wacs2_nochk read fail,return_value=%x\n", return_value);
			return return_value;
		}
		*rdata = GET_WACS0_RDATA( reg_rdata );
		WRAP_WR32(PMIC_WRAP_WACS2_VLDCLR , 1);
	}
	return 0;
}
//--------------------------------------------------------
//    Function : _pwrap_init_dio()
// Description :call it in pwrap_init,mustn't check init done
//   Parameter :
//      Return :
//--------------------------------------------------------
static S32 _pwrap_init_dio( U32 dio_en )
{
	U32 arb_en_backup=0x0;
	U32 rdata=0x0;
	U32 return_value=0;

	//PWRAPFUC();
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN ,WACS2 ); // only WACS2
	
#ifdef SLV_6331  
	  pwrap_write_nochk(MT6331_DEW_DIO_EN, (dio_en>>1));
#endif
#ifdef SLV_6332  
	  pwrap_write_nochk(MT6332_DEW_DIO_EN, (dio_en>>1));
#endif  

	// Check IDLE & INIT_DONE in advance
	return_value=wait_for_state_ready_init(wait_for_idle_and_sync,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,0);
	if(return_value!=0)
	{
		PWRAPERR("_pwrap_init_dio fail,return_value=%x\n", return_value);
		return return_value;
	}
	WRAP_WR32(PMIC_WRAP_DIO_EN , dio_en);
	// Read Test	
#ifdef SLV_6331  
	  pwrap_read_nochk(MT6331_DEW_READ_TEST, &rdata);
	  if( rdata != MT6331_DEFAULT_VALUE_READ_TEST )
	  {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0x5aa5\n", dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	  }
#endif
#ifdef SLV_6332  
	  pwrap_read_nochk(MT6332_DEW_READ_TEST, &rdata);
	  if( rdata != MT6332_DEFAULT_VALUE_READ_TEST )
	  {
		PWRAPERR("[Dio_mode][Read Test] fail,dio_en = %x, READ_TEST rdata=%x, exp=0xa55a\n", dio_en, rdata);
		return E_PWR_READ_TEST_FAIL;
	  }
#endif
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , arb_en_backup);
	return 0;
}

//--------------------------------------------------------
//    Function : _pwrap_init_cipher()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------
static S32 _pwrap_init_cipher( void )
{
	U32 arb_en_backup=0;
	U32 rdata=0;
	U32 return_value=0;
	U32 start_time_ns=0, timeout_ns=0;
	//PWRAPFUC();
	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN ,WACS2); // only WACS0

	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST ,  1);
	WRAP_WR32(PMIC_WRAP_CIPHER_SWRST ,  0);
	WRAP_WR32(PMIC_WRAP_CIPHER_KEY_SEL , 1);
	WRAP_WR32(PMIC_WRAP_CIPHER_IV_SEL  , 2);
	WRAP_WR32(PMIC_WRAP_CIPHER_EN   , 1);

	//Config CIPHER @ PMIC
#ifdef SLV_6331 
	pwrap_write_nochk(MT6331_DEW_CIPHER_SWRST, 0x1);
	pwrap_write_nochk(MT6331_DEW_CIPHER_SWRST, 0x0);
	pwrap_write_nochk(MT6331_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_nochk(MT6331_DEW_CIPHER_IV_SEL,  0x2);
	pwrap_write_nochk(MT6331_DEW_CIPHER_EN,  0x1);
#endif
#ifdef SLV_6332
	pwrap_write_nochk(MT6332_DEW_CIPHER_SWRST,   0x1);
	pwrap_write_nochk(MT6332_DEW_CIPHER_SWRST,   0x0);
	pwrap_write_nochk(MT6332_DEW_CIPHER_KEY_SEL, 0x1);
	pwrap_write_nochk(MT6332_DEW_CIPHER_IV_SEL,  0x2);
	pwrap_write_nochk(MT6332_DEW_CIPHER_EN,	0x1);
#endif	
	//wait for cipher data ready@AP
	return_value=wait_for_state_ready_init(wait_for_cipher_ready,TIMEOUT_WAIT_IDLE,PMIC_WRAP_CIPHER_RDY,0);
	if(return_value!=0)
	{
		PWRAPERR("wait for cipher data ready@AP fail,return_value=%x\n", return_value);
		return return_value;
	}

	//wait for cipher data ready@PMIC
#ifdef SLV_6331 
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait for cipher data ready@PMIC\n");
			//pwrap_dump_all_register();
			//return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		pwrap_read_nochk(MT6331_DEW_CIPHER_RDY,&rdata);
	} while( rdata != 0x1 ); //cipher_ready

	pwrap_write_nochk(MT6331_DEW_CIPHER_MODE, 0x1);
#endif
#ifdef SLV_6332
	start_time_ns = _pwrap_get_current_time();
	timeout_ns = _pwrap_time2ns(0xFFFFFF);
	do
	{
		if (_pwrap_timeout_ns(start_time_ns, timeout_ns))
		{
			PWRAPERR("wait for cipher data ready@PMIC\n");
			//pwrap_dump_all_register();
			//return E_PWR_WAIT_IDLE_TIMEOUT;
		}
		pwrap_read_nochk(MT6332_DEW_CIPHER_RDY,&rdata);
	} while( rdata != 0x1 ); //cipher_ready

	pwrap_write_nochk(MT6332_DEW_CIPHER_MODE, 0x1);
#endif
	//wait for cipher mode idle
	return_value=wait_for_state_ready_init(wait_for_idle_and_sync,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,0);
	if(return_value!=0)
	{
		PWRAPERR("wait for cipher mode idle fail,return_value=%x\n", return_value);
		return return_value;
	}
	WRAP_WR32(PMIC_WRAP_CIPHER_MODE , 1);

	// Read Test
#ifdef SLV_6331  
	  pwrap_read_nochk(MT6331_DEW_READ_TEST, &rdata);
	  if( rdata != MT6331_DEFAULT_VALUE_READ_TEST )
	  {
		PWRAPERR("_pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return E_PWR_READ_TEST_FAIL;
	  }
#endif
#ifdef SLV_6332  
	  pwrap_read_nochk(MT6332_DEW_READ_TEST, &rdata);
	  if( rdata != MT6332_DEFAULT_VALUE_READ_TEST )
	  {
		PWRAPERR("_pwrap_init_cipher,read test error,error code=%x, rdata=%x\n", 1, rdata);
		return E_PWR_READ_TEST_FAIL;
	  }
#endif  

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , arb_en_backup);
	return 0;
}

//--------------------------------------------------------
//    Function : _pwrap_init_sistrobe()
// Description : Initialize SI_CK_CON and SIDLY
//   Parameter :
//      Return :
//--------------------------------------------------------
static S32 _pwrap_init_sistrobe( void )
{
	U32 arb_en_backup=0;
	U32 rdata=0;
	U32 i=0;
	S32 ind=0; 
	U32 tmp1=0;
	U32 tmp2=0;
	U32 result_faulty=0;
	U32 result[2]={0,0};
	S32 leading_one[2]={-1,-1};
	S32 tailing_one[2]={-1,-1};

	arb_en_backup = WRAP_RD32(PMIC_WRAP_HIPRIO_ARB_EN);

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN ,WACS2); // only WACS2

	//---------------------------------------------------------------------
	// Scan all possible input strobe by READ_TEST
	//---------------------------------------------------------------------
	for( ind=0; ind<24; ind++)  // 24 sampling clock edge
	{
		WRAP_WR32(PMIC_WRAP_SI_CK_CON , (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY ,0x3 - (ind & 0x3));
#ifdef SLV_6331    
			_pwrap_wacs2_nochk(0, MT6331_DEW_READ_TEST, 0, &rdata);
			if( rdata == MT6331_DEFAULT_VALUE_READ_TEST ) {
				 PWRAPLOG("_pwrap_init_sistrobe [Read Test of MT6331] pass,index=%d rdata=%x\n", ind,rdata);
     			 result[0] |= (0x1 << ind);
    		}else {
				 PWRAPLOG("_pwrap_init_sistrobe [Read Test of MT6331] tuning,index=%d rdata=%x\n", ind,rdata);
			}
#endif
#ifdef SLV_6332
			_pwrap_wacs2_nochk(0, MT6332_DEW_READ_TEST, 0, &rdata);
			if( rdata == MT6332_DEFAULT_VALUE_READ_TEST ) {
				PWRAPLOG("_pwrap_init_sistrobe [Read Test of MT6332] pass,index=%d rdata=%x\n", ind,rdata);
				result[1] |= (0x1 << ind);
			}else {
				 PWRAPLOG("_pwrap_init_sistrobe [Read Test of MT6332] tuning,index=%d rdata=%x\n", ind,rdata);
			}
#endif  
	 }
#ifndef SLV_6331
	  result[0] = result[1];
#endif
#ifndef SLV_6332
	  result[1] = result[0];
#endif
	 //---------------------------------------------------------------------
  	// Locate the leading one and trailing one of PMIC 1/2
  	//---------------------------------------------------------------------
	for( ind=23 ; ind>=0 ; ind-- )
	{
	  if( (result[0] & (0x1 << ind)) && leading_one[0] == -1){
		  leading_one[0] = ind;
	  }
	  if(leading_one[0] > 0) { break;}
	}
	for( ind=23 ; ind>=0 ; ind-- )
	{
	  if( (result[1] & (0x1 << ind)) && leading_one[1] == -1){
		  leading_one[1] = ind;
	  }
	  if(leading_one[1] > 0) { break;}
	}  
	
	for( ind=0 ; ind<24 ; ind++ )
	{
	  if( (result[0] & (0x1 << ind)) && tailing_one[0] == -1){
		  tailing_one[0] = ind;
	  }
	  if(tailing_one[0] > 0) { break;}
	}
	for( ind=0 ; ind<24 ; ind++ )
	{
	  if( (result[1] & (0x1 << ind)) && tailing_one[1] == -1){
		  tailing_one[1] = ind;
	  }
	  if(tailing_one[1] > 0) { break;}
	}  

  	//---------------------------------------------------------------------
  	// Check the continuity of pass range
  	//---------------------------------------------------------------------
  	for( i=0; i<2; i++)
  	{
    	tmp1 = (0x1 << (leading_one[i]+1)) - 1;
    	tmp2 = (0x1 << tailing_one[i]) - 1;
    	if( (tmp1 - tmp2) != result[i] )
    	{
    		/*TERR = "[DrvPWRAP_InitSiStrobe] Fail at PMIC %d, result = %x, leading_one:%d, tailing_one:%d"
    	         	 , i+1, result[i], leading_one[i], tailing_one[i]*/
    	    PWRAPERR("_pwrap_init_sistrobe Fail at PMIC %d, result = %x, leading_one:%d, tailing_one:%d\n", i+1, result[i], leading_one[i], tailing_one[i]);
      		result_faulty = 0x1;
    	}
  	}
	
	//---------------------------------------------------------------------
	// Config SICK and SIDLY to the middle point of pass range
	//---------------------------------------------------------------------
	if( result_faulty == 0 )
    {
  		// choose the best point in the interaction of PMIC1's pass range and PMIC2's pass range
    	ind = ( (leading_one[0] + tailing_one[0])/2 + (leading_one[1] + tailing_one[1])/2 )/2;
        /*TINFO = "The best point in the interaction area is %d, ind"*/ 
		WRAP_WR32(PMIC_WRAP_SI_CK_CON , (ind >> 2) & 0x7);
		WRAP_WR32(PMIC_WRAP_SIDLY , 0x3 - (ind & 0x3));		
		//---------------------------------------------------------------------
		// Restore
		//---------------------------------------------------------------------
		WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , arb_en_backup);
		return 0;
	}
	else
	{
		PWRAPERR("_pwrap_init_sistrobe Fail,result_faulty=%x\n", result_faulty);
		return result_faulty;
	}
}

//--------------------------------------------------------
//    Function : _pwrap_reset_spislv()
// Description :
//   Parameter :
//      Return :
//--------------------------------------------------------

static S32 _pwrap_reset_spislv( void )
{
	U32 ret=0;
	U32 return_value=0;
	//PWRAPFUC();
	// This driver does not using _pwrap_switch_mux
	// because the remaining requests are expected to fail anyway

	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN , DISABLE_ALL);
	WRAP_WR32(PMIC_WRAP_WRAP_EN , DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL , MANUAL_MODE);
	WRAP_WR32(PMIC_WRAP_MAN_EN ,ENABLE);
	WRAP_WR32(PMIC_WRAP_DIO_EN ,DISABLE);

	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_CSL  << 8));//0x2100
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8)); //0x2800//to reset counter
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_CSH  << 8));//0x2000
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));
	WRAP_WR32(PMIC_WRAP_MAN_CMD , (OP_WR << 13) | (OP_OUTS << 8));

	return_value=wait_for_state_ready_init(wait_for_sync,TIMEOUT_WAIT_IDLE,PMIC_WRAP_WACS2_RDATA,0);
	if(return_value!=0)
	{
		PWRAPERR("_pwrap_reset_spislv fail,return_value=%x\n", return_value);
		ret=E_PWR_TIMEOUT;
		goto timeout;
	}

	WRAP_WR32(PMIC_WRAP_MAN_EN ,DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL ,WRAPPER_MODE);

timeout:
	WRAP_WR32(PMIC_WRAP_MAN_EN ,DISABLE);
	WRAP_WR32(PMIC_WRAP_MUX_SEL ,WRAPPER_MODE);
	return ret;
}

static S32 _pwrap_init_reg_clock( U32 regck_sel )
{
	//U32 wdata=0;
	//U32 rdata=0;
	PWRAPFUC();
	
	// Set Dummy cycle 6331 and 6332 (assume 12MHz)
#ifdef SLV_6331
	  pwrap_write_nochk(MT6331_DEW_RDDMY_NO, 0x8);
#endif
#ifdef SLV_6332  
	  pwrap_write_nochk(MT6332_DEW_RDDMY_NO, 0x8);
#endif  
	WRAP_WR32(PMIC_WRAP_RDDMY ,0x88);

	// Config SPI Waveform according to reg clk
	if( regck_sel == 1 ) { // 6MHz in 6323  => no support ; 18MHz in 6320
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE   , 0x4);  // wait data written into register => 3T_PMIC
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ    , 0x5);  // for 6320, slave need enough time (4T of PMIC reg_ck) to back idle state
		WRAP_WR32(PMIC_WRAP_CSLEXT_START   , 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END   , 0x0);
	} else if( regck_sel == 2 ) { // 12MHz in 6323; 36MHz in 6320
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ  , 0x0);
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE   , 0x6);  // wait data written into register => 3T_PMIC: consists of CSLEXT_END(1T) + CSHEXT(6T)
		WRAP_WR32(PMIC_WRAP_CSLEXT_START   , 0x0);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END   ,0x0);
	} else { //Safe mode
		WRAP_WR32(PMIC_WRAP_CSHEXT_WRITE   , 0xf);
		WRAP_WR32(PMIC_WRAP_CSHEXT_READ    , 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_START   , 0xf);
		WRAP_WR32(PMIC_WRAP_CSLEXT_END   , 0xf);
	}

	return 0;
}

//--------------------------------------------------------
//    Function : DrvPWRAP_Switch_Strobe_Setting()
// Description : used to switch input data calibration setting before system sleep or after system wakeup
//               no use since SPI_CK (26MHz) is always kept unchanged
//   Parameter :
//      Return :
//--------------------------------------------------------
//void DrvPWRAP_Switch_Strobe_Setting (int si_ck_con, int sidly)
//{
//  int reg_rdata;
//  // turn off spi_wrap
//  *PMIC_WRAP_WRAP_EN = 0;
//  // wait for WRAP to be in idle state
//  // and no remaining rdata to be received
//  do
//  {
//    reg_rdata = *PMIC_WRAP_WRAP_STA;
//  } while ( (GET_WRAP_FSM(reg_rdata) != 0) ||
//            (GET_WRAP_CH_DLE_RESTCNT(reg_rdata)) != 0 );
//
//  *PMIC_WRAP_SI_CK_CON = si_ck_con;
//  *PMIC_WRAP_SIDLY = sidly;
//
//  // turn on spi_wrap
//  *PMIC_WRAP_WRAP_EN = 1;
//}


static S32 _pwrap_init_signature( U8 path )
{
	int ret;
	U32 rdata=0x0;
	PWRAPFUC();

	if(path == 1){
		//###############################
		// Signature Checking - Using Write Test Register
		// should be the last to modify WRITE_TEST
		//###############################
		_pwrap_wacs2_nochk(1, MT6331_DEW_WRITE_TEST, 0x5678, &rdata);
		WRAP_WR32(PMIC_WRAP_SIG_ADR,MT6331_DEW_WRITE_TEST);
		WRAP_WR32(PMIC_WRAP_SIG_VALUE,0x5678);
		WRAP_WR32(PMIC_WRAP_SIG_MODE, 0x1);
	}else{
		//###############################
		 // Signature Checking using CRC and EINT update
		// should be the last to modify WRITE_TEST
		//###############################
#ifdef SLV_6331  
		  ret = pwrap_write_nochk(MT6331_DEW_CRC_EN, ENABLE);
		  if( ret != 0 )
		  {
			PWRAPERR("MT6331 enable CRC fail,ret=%x\n", ret);
			return E_PWR_INIT_ENABLE_CRC;
		  }
		  WRAP_WR32(PMIC_WRAP_SIG_ADR,WRAP_RD32(PMIC_WRAP_SIG_ADR)|MT6331_DEW_CRC_VAL);
		  WRAP_WR32(PMIC_WRAP_EINT_STA0_ADR,MT6331_INT_STA);
		  WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN)|0x5);
#endif  
#ifdef SLV_6332  
		  ret = pwrap_write_nochk(MT6332_DEW_CRC_EN, ENABLE);
		  if( ret != 0 )
		  {
			 PWRAPERR("MT6332 enable CRC fail,ret=%x\n", ret);
			 return E_PWR_INIT_ENABLE_CRC;
		  }
		  WRAP_WR32(PMIC_WRAP_SIG_ADR,WRAP_RD32(PMIC_WRAP_SIG_ADR)|(MT6332_DEW_CRC_VAL<<16));
		  WRAP_WR32(PMIC_WRAP_EINT_STA1_ADR,MT6332_INT_STA);
		  WRAP_WR32(PMIC_WRAP_STAUPD_GRPEN,WRAP_RD32(PMIC_WRAP_STAUPD_GRPEN)|0xa);
#endif  
	}
	WRAP_WR32(PMIC_WRAP_CRC_EN, ENABLE);
	return 0;
}
/*
 *pmic_wrap init,init wrap interface
 *
 */
S32 pwrap_init ( void )
{
	S32 sub_return=0;
	S32 sub_return1=0;
	//S32 ret=0;
	U32 rdata=0x0;
	//U32 regValue;
	//U32 timeout=0;
	u32 cg_mask = 0;
	u32 backup = 0;
	PWRAPFUC();
#ifdef CONFIG_OF
	sub_return = pwrap_of_iomap();
	if (sub_return)
		return sub_return;
#endif
	//###############################
	//toggle PMIC_WRAP and pwrap_spictl reset
	//###############################
	//   WRAP_SET_BIT(0x80,INFRA_GLOBALCON_RST0);
	//   WRAP_CLR_BIT(0x80,INFRA_GLOBALCON_RST0);
	__pwrap_soft_reset();

	//###############################
	// Set SPI_CK_freq = 26MHz 
	//###############################
	WRAP_WR32(CLK_CFG_5_CLR,CLK_SPI_CK_26M);

	//###############################
	//toggle PERI_PWRAP_BRIDGE reset
	//###############################
	//WRAP_SET_BIT(0x04,PERI_GLOBALCON_RST1);
	//WRAP_CLR_BIT(0x04,PERI_GLOBALCON_RST1);

	//###############################
	//Enable DCM
	//###############################
	//WRAP_WR32(PMIC_WRAP_DCM_EN , ENABLE);
	WRAP_WR32(PMIC_WRAP_DCM_EN , 3);//enable CRC DCM and Pwrap DCM
	WRAP_WR32(PMIC_WRAP_DCM_DBC_PRD ,DISABLE); //no debounce

	//###############################
	//Reset SPISLV
	//###############################
	sub_return=_pwrap_reset_spislv();
	if( sub_return != 0 )
	{
		PWRAPERR("error,_pwrap_reset_spislv fail,sub_return=%x\n",sub_return);
		return E_PWR_INIT_RESET_SPI;
	}
	//###############################
	// Enable WACS2
	//###############################
	WRAP_WR32(PMIC_WRAP_WRAP_EN,ENABLE);//enable wrap
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,WACS2); //Only WACS2
	WRAP_WR32(PMIC_WRAP_WACS2_EN,ENABLE);

	//###############################
	// Input data calibration flow;
	//###############################
	sub_return = _pwrap_init_sistrobe();
	if( sub_return != 0 )
	{
		PWRAPERR("error,DrvPWRAP_InitSiStrobe fail,sub_return=%x\n",sub_return);
		return E_PWR_INIT_SIDLY;
	}

	//###############################
	// SPI Waveform Configuration
	//###############################
	//0:safe mode, 1:6MHz, 2:12MHz => no support 6MHz since the clock is too slow to transmit data (due to RDDMY's limit -> only 4'hf)
	sub_return = _pwrap_init_reg_clock(3);
	if( sub_return != 0)
	{
		PWRAPERR("error,_pwrap_init_reg_clock fail,sub_return=%x\n",sub_return);
		return E_PWR_INIT_REG_CLOCK;
	}

	//###############################
	// Enable DIO mode
	//###############################
	//PMIC2 dual io not ready
	sub_return = _pwrap_init_dio(3);
	if( sub_return != 0 )
	{
		PWRAPERR("_pwrap_init_dio test error,error code=%x, sub_return=%x\n", 0x11, sub_return);
		return E_PWR_INIT_DIO;
	}

	//###############################
	// Enable Encryption
	//###############################
	sub_return = _pwrap_init_cipher();
	if( sub_return != 0 )
	{
		PWRAPERR("Enable Encryption fail, return=%x\n", sub_return);
		return E_PWR_INIT_CIPHER;
	}

	//###############################
	// Write test using WACS2
	//###############################
	//check Wtiet test default value
	
#ifdef SLV_6331
	  sub_return = pwrap_write_nochk(MT6331_DEW_WRITE_TEST, MT6331_WRITE_TEST_VALUE);
	  sub_return1 = pwrap_read_nochk(MT6331_DEW_WRITE_TEST, &rdata);
	  if( rdata != MT6331_WRITE_TEST_VALUE )  {
		PWRAPERR("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n", rdata,sub_return,sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	  }
#endif
#ifdef SLV_6332
	  sub_return = pwrap_write_nochk(MT6332_DEW_WRITE_TEST, MT6332_WRITE_TEST_VALUE);
	  sub_return1 = pwrap_read_nochk(MT6332_DEW_WRITE_TEST, &rdata);
	  if( rdata != MT6332_WRITE_TEST_VALUE )  {
		PWRAPERR("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n", rdata,sub_return,sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	  }
#endif
	
	//###############################
	// Signature Checking - Using CRC
	// should be the last to modify WRITE_TEST
	//###############################
	sub_return = _pwrap_init_signature(0);
	if( sub_return != 0 )
	{
		PWRAPERR("Enable CRC fail, return=%x\n", sub_return);
		return E_PWR_INIT_ENABLE_CRC;
	}

	//###############################
	// PMIC_WRAP enables
	//###############################
	WRAP_WR32(PMIC_WRAP_HIPRIO_ARB_EN,0x3f);
	WRAP_WR32(PMIC_WRAP_WACS0_EN,ENABLE);
	WRAP_WR32(PMIC_WRAP_WACS1_EN,ENABLE);
	WRAP_WR32(PMIC_WRAP_STAUPD_PRD, 0x5);  //0x1:20us,for concurrence test,MP:0x5;  //100us
	WRAP_WR32(PMIC_WRAP_WDT_UNIT,0xf);
	WRAP_WR32(PMIC_WRAP_WDT_SRC_EN,0xffffffff);
	WRAP_WR32(PMIC_WRAP_TIMER_EN,0x1);
	WRAP_WR32(PMIC_WRAP_INT_EN,0x7ffffff9); 

	//###############################
	// Initialization Done
	//###############################
	WRAP_WR32(PMIC_WRAP_INIT_DONE2 , ENABLE);

	//###############################
	//TBD: Should be configured by MD MCU
	//###############################
	WRAP_WR32(PMIC_WRAP_INIT_DONE0 ,ENABLE);
	WRAP_WR32(PMIC_WRAP_INIT_DONE1 , ENABLE);
#ifdef CONFIG_OF
	pwrap_of_iounmap();
#endif

	return 0;
}
//EXPORT_SYMBOL(pwrap_init);
/*Interrupt handler function*/
static irqreturn_t mt_pmic_wrap_irq(int irqno, void *dev_id)
{
	unsigned long flags=0;

	PWRAPFUC();
	PWRAPREG("dump pwrap register\n");
	spin_lock_irqsave(&wrp_lock,flags);
	//*-----------------------------------------------------------------------
	pwrap_dump_all_register();
	//raise the priority of WACS2 for AP
	WRAP_WR32(PMIC_WRAP_HARB_HPRIO,1<<3);

	//*-----------------------------------------------------------------------
	//clear interrupt flag
	WRAP_WR32(PMIC_WRAP_INT_CLR, 0xffffffff);
	PWRAPREG("INT flag 0x%x\n",WRAP_RD32(PMIC_WRAP_INT_EN));
	if(((WRAP_RD32(PMIC_WRAP_WDT_FLG)&(1<<1))==0x10) || ((WRAP_RD32(PMIC_WRAP_WACS0_RDATA)&(1<<22))==0x0)) 
       /*do nothing if WACS0 wait for idle timeout*/
	  PWRAPERR("PWRAP WACS0 wait for idle timeout and no need to trigger BUG_ON\n");
	else{
       pwrap_of_iomap();
	   PWRAPLOG("the infra clock register =0x%x\n",WRAP_RD32(INFRACFG_AO_REG_BASE+0x048));
	   pwrap_of_iounmap();
	  BUG_ON(1);
	}
	spin_unlock_irqrestore(&wrp_lock,flags);
	return IRQ_HANDLED;
}

static U32 pwrap_read_test(void)
{
	U32 rdata=0;
	U32 return_value=0;
	// Read Test
#ifdef SLV_6331
	return_value=pwrap_read(MT6331_DEW_READ_TEST,&rdata);
	if( rdata != MT6331_DEFAULT_VALUE_READ_TEST )
	{
		PWRAPREG("Read Test fail,rdata=0x%x, exp=0x5aa5,return_value=0x%x\n", rdata,return_value);
		return E_PWR_READ_TEST_FAIL;
	}
	else
	{
		PWRAPREG("Read Test pass,return_value=%d\n",return_value);
		//return 0;
	}
#endif
#ifdef SLV_6332
	return_value=pwrap_read(MT6332_DEW_READ_TEST,&rdata);
	if( rdata != MT6332_DEFAULT_VALUE_READ_TEST )
	{
		PWRAPREG("Read Test fail,rdata=0x%x, exp=0x5aa5,return_value=0x%x\n", rdata,return_value);
		return E_PWR_READ_TEST_FAIL;
	}
	else
	{
		PWRAPREG("Read Test pass,return_value=%d\n",return_value);
		return 0;
	}
#endif
	return 0;
}
static U32 pwrap_write_test(void)
{
	U32 rdata=0;
	U32 sub_return=0;
	U32 sub_return1=0;
	//###############################
	// Write test using WACS2
	//###############################
#ifdef SLV_6331
	sub_return = pwrap_write(MT6331_DEW_WRITE_TEST, MT6331_WRITE_TEST_VALUE);
	PWRAPREG("after MT6331 pwrap_write\n");
	sub_return1 = pwrap_read(MT6331_DEW_WRITE_TEST,&rdata);
	if(( rdata != MT6331_WRITE_TEST_VALUE )||( sub_return != 0 )||( sub_return1 != 0 ))
	{
		PWRAPREG("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n", rdata,sub_return,sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
	else
	{
		PWRAPREG("write MT6331 Test pass\n");
		//return 0;
	}
#endif
#ifdef SLV_6332
	sub_return = pwrap_write(MT6332_DEW_WRITE_TEST, MT6332_WRITE_TEST_VALUE);
	PWRAPREG("after MT6332 pwrap_write\n");
	sub_return1 = pwrap_read(MT6332_DEW_WRITE_TEST,&rdata);
	if(( rdata != MT6332_WRITE_TEST_VALUE )||( sub_return != 0 )||( sub_return1 != 0 ))
	{
		PWRAPREG("write test error,rdata=0x%x,exp=0xa55a,sub_return=0x%x,sub_return1=0x%x\n", rdata,sub_return,sub_return1);
		return E_PWR_INIT_WRITE_TEST;
	}
	else
	{
		PWRAPREG("write MT6332 Test pass\n");
		return 0;
	}
#endif
	return 0;
}
static void pwrap_int_test(void)
{
	U32 rdata1=0;
	U32 rdata2=0;
	while(1) {
	   #ifdef SLV_6331
		rdata1 = WRAP_RD32(PMIC_WRAP_EINT_STA);
		pwrap_read(MT6331_INT_STA,&rdata2);
		PWRAPREG("Pwrap INT status check,PMIC_WRAP_EINT_STA=0x%x,MT6331_INT_STA[0x01B4]=0x%x\n", rdata1,rdata2);
	   #endif
	   #ifdef SLV_6332
		rdata1 = WRAP_RD32(PMIC_WRAP_EINT_STA);
		pwrap_read(MT6332_INT_STA,&rdata2);
		PWRAPREG("Pwrap INT status check,PMIC_WRAP_EINT_STA=0x%x,MT6332_INT_STA[0x8112]=0x%x\n", rdata1,rdata2);
	   #endif
	   msleep(500);
	}
}
static void pwrap_ut(U32 ut_test)
{
	switch(ut_test)
	{
		case 1:
			//pwrap_wacs2_para_test();
			pwrap_write_test();
			break;
		case 2:
			//pwrap_wacs2_para_test();
			pwrap_read_test();
			break;

		default:
			PWRAPREG ( "default test.\n" );
			break;
	}
	return;
}
/*---------------------------------------------------------------------------*/
static S32 mt_pwrap_show_hal(char *buf)
{
	PWRAPFUC();
	return snprintf(buf, PAGE_SIZE, "%s\n","no implement" );
}
/*---------------------------------------------------------------------------*/
static S32 mt_pwrap_store_hal(const char *buf,size_t count)
{
	U32 reg_value=0;
	U32 reg_addr=0;
	U32 return_value=0;
	U32 ut_test=0;
	if(!strncmp(buf, "-h", 2))
	{
		PWRAPREG("PWRAP debug: [-dump_reg][-trace_wacs2][-init][-rdap][-wrap][-rdpmic][-wrpmic][-readtest][-writetest]\n");
		PWRAPREG("PWRAP UT: [1][2]\n");
	}
	//--------------------------------------pwrap debug-------------------------------------------------------------
	else if(!strncmp(buf, "-dump_reg", 9))
	{
		pwrap_dump_all_register();
	}
	else if(!strncmp(buf, "-trace_wacs2", 12))
	{
		//pwrap_trace_wacs2();
	}
	else if(!strncmp(buf, "-init", 5))
	{
		return_value=pwrap_init();
		if(return_value==0)
			PWRAPREG("pwrap_init pass,return_value=%d\n",return_value);
		else
			PWRAPREG("pwrap_init fail,return_value=%d\n",return_value);
	}
	else if (!strncmp(buf, "-rdap", 5) && (1 == sscanf(buf+5, "%x", &reg_addr)))
	{
		//pwrap_read_reg_on_ap(reg_addr);
	}
	else if (!strncmp(buf, "-wrap", 5) && (2 == sscanf(buf+5, "%x %x", &reg_addr,&reg_value)))
	{
		//pwrap_write_reg_on_ap(reg_addr,reg_value);
	}
	else if (!strncmp(buf, "-rdpmic", 7) && (1 == sscanf(buf+7, "%x", &reg_addr)))
	{
		//pwrap_read_reg_on_pmic(reg_addr);
	}
	else if (!strncmp(buf, "-wrpmic", 7) && (2 == sscanf(buf+7, "%x %x", &reg_addr,&reg_value)))
	{
		//pwrap_write_reg_on_pmic(reg_addr,reg_value);
	}
	else if(!strncmp(buf, "-readtest", 9))
	{
		pwrap_read_test();
	}
	else if(!strncmp(buf, "-writetest", 10))
	{
		pwrap_write_test();
	}
	else if(!strncmp(buf, "-int", 4))
	{
		pwrap_int_test();
	}
	//--------------------------------------pwrap UT-------------------------------------------------------------
	else if (!strncmp(buf, "-ut", 3) && (1 == sscanf(buf+3, "%d", &ut_test)))
	{
		pwrap_ut(ut_test);
	}else{
		PWRAPREG("wrong parameter\n");
	}
	return count;
}
/*---------------------------------------------------------------------------*/
#endif //endif PMIC_WRAP_NO_PMIC
#ifdef CONFIG_OF
static int pwrap_of_iomap(void)
{
	/*
	 * Map the address of the following register base:
	 * INFRACFG_AO, TOPCKGEN, SCP_CLK_CTRL, SCP_PMICWP2P
	 */

	struct device_node *infracfg_ao_node;
	struct device_node *topckgen_node;

	infracfg_ao_node =
		of_find_compatible_node(NULL, NULL, "mediatek,INFRACFG_AO");
	if (!infracfg_ao_node) {
		pr_warn("get INFRACFG_AO failed\n");
		return -ENODEV;
	}

	infracfg_ao_base = of_iomap(infracfg_ao_node, 0);
	if (!infracfg_ao_base) {
		pr_warn("INFRACFG_AO iomap failed\n");
		return -ENOMEM;
	}

	topckgen_node =
		of_find_compatible_node(NULL, NULL, "mediatek,CKSYS");
	if (!topckgen_node) {
		pr_warn("get TOPCKGEN failed\n");
		return -ENODEV;
	}

	topckgen_base = of_iomap(topckgen_node, 0);
	if (!topckgen_base) {
		pr_warn("TOPCKGEN iomap failed\n");
		return -ENOMEM;
	}
	return 0;
}
static void pwrap_of_iounmap(void)
{
	iounmap(infracfg_ao_base);
	iounmap(topckgen_base);
}
#endif

#define VERSION     "$Revision$"
static int is_pwrap_init_done(void)
{
	int ret=0;
	ret = WRAP_RD32(PMIC_WRAP_INIT_DONE2);
	if(ret!=0)
		return 0;

	ret = pwrap_init();
	if(ret!=0){
		PWRAPERR("init error (%d)\n", ret);
		pwrap_dump_all_register();
		return ret;
	}
	PWRAPLOG("init successfully done (%d)\n\n", ret);
	return ret;
}
int __init mt_pwrap_hal_init(void)
{
	S32 ret = 0;
#ifdef CONFIG_OF
	u32 pwrap_irq;
	struct device_node *pwrap_node;

	pwrap_node = of_find_compatible_node(NULL, NULL, "mediatek,PMIC_WRAP");
	if (!pwrap_node) {
		pr_warn("PWRAP get node failed\n");
		return -ENODEV;
	}

	pwrap_base = of_iomap(pwrap_node, 0);
	if (!pwrap_base) {
		pr_warn("PWRAP iomap failed\n");
		return -ENOMEM;
	}

	pwrap_irq = irq_of_parse_and_map(pwrap_node, 0);
	if (!pwrap_irq) {
		pr_warn("PWRAP get irq fail\n");
		return -ENODEV;
	}
	pr_warn("PWRAP reg: 0x%p,  irq: %d\n", pwrap_base, pwrap_irq);
#endif
	PWRAPLOG("HAL init: version %s\n", VERSION);
	mt_wrp = get_mt_pmic_wrap_drv();	
	mt_wrp->store_hal = mt_pwrap_store_hal;
	mt_wrp->show_hal = mt_pwrap_show_hal;
	mt_wrp->wacs2_hal = pwrap_wacs2_hal;

	if(is_pwrap_init_done()==0){
	  #ifdef PMIC_WRAP_NO_PMIC
	  #else
		ret = request_irq(MT_PMIC_WRAP_IRQ_ID, mt_pmic_wrap_irq, IRQF_TRIGGER_HIGH, PMIC_WRAP_DEVICE,0);
	  #endif
		if (ret) {
			PWRAPERR("register IRQ failed (%d)\n", ret);
			return ret;
		}
	}else{
		PWRAPERR("not init (%d)\n", ret);
	}

	//PWRAPERR("not init (%x)\n", is_pwrap_init_done);

	return ret;
}
postcore_initcall(mt_pwrap_hal_init);
