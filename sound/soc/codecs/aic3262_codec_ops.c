#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mfd/tlv320aic3262-core.h>
#include <linux/mfd/tlv320aic3262-registers.h>
#include <sound/soc.h>
#include "aic3xxx_cfw.h"
#include "aic3xxx_cfw_ops.h"
#include "tlv320aic326x.h"
#include "aic3262_codec_ops.h"

int aic3262_ops_reg_read(void *p, unsigned int reg)
{
	struct aic3262_priv *ps = p;
	union cfw_register *c = (union cfw_register *) &reg;
	union aic326x_reg_union mreg;

	mreg.aic326x_register.offset = c->offset;
	mreg.aic326x_register.page = c->page;
	mreg.aic326x_register.book = c->book;
	mreg.aic326x_register.reserved = 0;

	return aic3262_reg_read(ps->codec->control_data,
				mreg.aic326x_register_int);

}
int aic3262_ops_reg_write(void  *p, unsigned int reg, unsigned char mval)
{
	struct aic3262_priv *ps = p;
	union aic326x_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;

	mreg.aic326x_register.offset = c->offset;
	mreg.aic326x_register.page = c->page;
	mreg.aic326x_register.book = c->book;
	mreg.aic326x_register.reserved = 0;
	mval = c->data;

	return aic3262_reg_write(ps->codec->control_data,
				mreg.aic326x_register_int, mval);
}

int aic3262_ops_set_bits(void *p, unsigned int reg,
			 unsigned char mask, unsigned char val)
{
	struct aic3262_priv *ps = p;

	union aic326x_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;
	mreg.aic326x_register.offset = c->offset;
	mreg.aic326x_register.page = c->page;
	mreg.aic326x_register.book = c->book;
	mreg.aic326x_register.reserved = 0;

	return aic3262_set_bits(ps->codec->control_data,
				mreg.aic326x_register_int, mask, val);

}

int aic3262_ops_bulk_read(void *p, unsigned int reg, int count, u8 *buf)
{
	struct aic3262_priv *ps = p;

	union aic326x_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;
	mreg.aic326x_register.offset = c->offset;
	mreg.aic326x_register.page = c->page;
	mreg.aic326x_register.book = c->book;
	mreg.aic326x_register.reserved = 0;

	return aic3262_bulk_read(ps->codec->control_data,
				 mreg.aic326x_register_int, count, buf);
}

int aic3262_ops_bulk_write(void *p, unsigned int reg, int count, const u8 *buf)
{
	struct aic3262_priv *ps = p;
	union aic326x_reg_union mreg;
	union cfw_register *c = (union cfw_register *) &reg;

	mreg.aic326x_register.offset = c->offset;
	mreg.aic326x_register.page = c->page;
	mreg.aic326x_register.book = c->book;
	mreg.aic326x_register.reserved = 0;

	return aic3262_bulk_write(ps->codec->control_data,
				  mreg.aic326x_register_int, count, buf);
}
/*****************************************************************************
Function Name	: aic3262_ops_dlock_lock
Argument	: pointer argument to the codec
Return value	: Integer
Purpose		: To Read the run state of the DAC and ADC
by reading the codec and returning the run state

Run state Bit format

------------------------------------------------------
D31|..........| D7 | D6|  D5  |  D4  | D3 | D2 | D1  |   D0  |
R               R    R   LADC   RADC    R    R   LDAC   RDAC
------------------------------------------------------

*******************************************************************************/

int aic3262_ops_lock(void *pv)
{
	int run_state = 0;
	struct aic3262_priv *aic3262 = (struct aic3262_priv *)pv;
	mutex_lock(&aic3262->codec->mutex);

	/* Reading the run state of adc and dac */
	run_state = get_runstate(aic3262->codec->control_data);

	return run_state;
}
/*******************************************************************************
Function name	: aic3262_ops_dlock_unlock
Argument	: pointer argument to the codec
Return Value	: integer returning 0
Purpose		: To unlock the mutex acqiured for reading
run state of the codec
 ******************************************************************************/
int aic3262_ops_unlock(void *pv)
{
	/*Releasing the lock of mutex */
	struct aic3262_priv *aic3262 = (struct aic3262_priv *)pv;

	mutex_unlock(&aic3262->codec->mutex);
	return 0;
}
/*******************************************************************************
Function Name	: aic3262_ops_dlock_stop
Argument	: pointer Argument to the codec
mask tells us the bit format of the
codec running state

Bit Format:
------------------------------------------------------
D31|..........| D7 | D6| D5 | D4 | D3 | D2 | D1 | D0 |
R               R    R   AL   AR    R    R   DL   DR
------------------------------------------------------
R  - Reserved
A  - minidsp_A
D  - minidsp_D
 ******************************************************************************/
int aic3262_ops_stop(void *pv, int mask)
{
	int run_state = 0;
	int limask = 0;
	struct aic3262_priv *aic3262 = (struct aic3262_priv *)pv;
	int ret_wbits = 0;

	mutex_lock(&aic3262->codec->mutex);
	run_state = get_runstate(aic3262->codec->control_data);

	limask = mask & AIC3XX_COPS_MDSP_A;
	if (limask != 0)
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_ADC_DATAPATH_SETUP, 0xC0, 0);

	limask = mask & AIC3XX_COPS_MDSP_D;
	if (limask != 0)
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_DAC_DATAPATH_SETUP, 0xC0, 0);

	limask = mask & AIC3XX_COPS_MDSP_A;
	if (limask != 0) {
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_ADC_FLAG, AIC3262_ADC_POWER_MASK,
				      0, TIME_DELAY, DELAY_COUNTER);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"at line %d function %s, ADC powerdown"
				"wait_bits timedout\n",
				__LINE__, __func__);
	}

	limask = mask & AIC3XX_COPS_MDSP_D;
	if (limask != 0) {
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_DAC_FLAG, AIC3262_DAC_POWER_MASK,
				      0, TIME_DELAY, DELAY_COUNTER);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"at line %d function %s, DAC powerdown"
				"wait_bits timedout\n",
				__LINE__, __func__);
	}

	return run_state;

}
/****************************************************************************
Function name	: aic3262_ops_dlock_restore
Argument	: pointer argument to the codec,run_state
Return Value	: integer returning 0
Purpose		: To unlock the mutex acqiured for reading
run state of the codec and to restore the states of the dsp
******************************************************************************/
int aic3262_ops_restore(void *pv, int run_state)
{
	int sync_state;
	struct aic3262_priv *aic3262 = (struct aic3262_priv *)pv;

	/*      This is for read the sync mode register state  */
	sync_state = SYNC_STATE(aic3262);

	/*checking whether the sync mode has been set or
	   not and checking the current state */
	if (((run_state & 0x30) && (run_state & 0x03)) && (sync_state & 0x80))
		aic3262_restart_dsps_sync(pv, run_state);
	else
		aic3262_dsp_pwrup(pv, run_state);

	mutex_unlock(&aic3262->codec->mutex);

	return 0;
}

/*****************************************************************************
Function name	: aic3262_ops_adaptivebuffer_swap
Argument	: pointer argument to the codec,mask tells us which dsp has to
be chosen for swapping
Return Value	: integer returning 0
Purpose		: To swap the coefficient buffers of minidsp according to mask
******************************************************************************/

int aic3262_ops_adaptivebuffer_swap(void *pv, int mask)
{
	struct aic3262_priv *aic3262 = (struct aic3262_priv *)pv;
	int ret_wbits = 0;

	if (mask & AIC3XX_ABUF_MDSP_A) {
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_ADC_ADAPTIVE_CRAM_REG, 0x1, 0x1);
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_ADC_ADAPTIVE_CRAM_REG, 0x1, 0, 15,
				      1);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"at line %d function %s, miniDSP_A buffer swap failed\n",
				__LINE__, __func__);
	}

	if (mask & AIC3XX_ABUF_MDSP_D1) {
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_DAC_ADAPTIVE_BANK1_REG, 0x1, 0x1);
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_DAC_ADAPTIVE_BANK1_REG, 0x1, 0,
				      15, 1);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"at line %d function %s, miniDSP_D buffer1 swap failed\n",
				__LINE__, __func__);
	}

	if (mask & AIC3XX_ABUF_MDSP_D2) {
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_DAC_ADAPTIVE_BANK2_REG, 0x1, 0x1);
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_DAC_ADAPTIVE_BANK2_REG, 0x1, 0,
				      15, 1);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"at line %d function %s, miniDSP_D buffer2 swap failed\n",
				__LINE__, __func__);
	}

	return 0;
}

/*****************************************************************************
Function name	: get_runstate
Argument	: pointer argument to the codec
Return Value	: integer returning the runstate
Purpose		: To read the current state of the dac's and adc's
******************************************************************************/

int get_runstate(void *ps)
{
	struct aic3262 *pr = ps;
	int run_state = 0;
	int DAC_state = 0, ADC_state = 0;
	/* Read the run state */
	DAC_state = aic3262_reg_read(pr, AIC3262_DAC_FLAG);
	ADC_state = aic3262_reg_read(pr, AIC3262_ADC_FLAG);

	DSP_STATUS(run_state, ADC_state, 6, 5);
	DSP_STATUS(run_state, ADC_state, 2, 4);
	DSP_STATUS(run_state, DAC_state, 7, 1);
	DSP_STATUS(run_state, DAC_state, 3, 0);

	return run_state;

}
/****************************************************************************
Function name	: aic3262_dsp_pwrdwn_status
Argument	: pointer argument to the codec , cur_state of dac's and adc's
Return Value	: integer returning 0
Purpose		: To read the status of dsp's
******************************************************************************/

int aic3262_dsp_pwrdwn_status(void *pv)
{
	struct aic3262_priv *aic3262 = pv;
	int ret_wbits = 0;

	aic3262_set_bits(aic3262->codec->control_data,
			 AIC3262_ADC_DATAPATH_SETUP, 0XC0, 0);
	aic3262_set_bits(aic3262->codec->control_data,
			 AIC3262_DAC_DATAPATH_SETUP, 0XC0, 0);

	ret_wbits =
	    aic3262_wait_bits(aic3262->codec->control_data, AIC3262_ADC_FLAG,
			      AIC3262_ADC_POWER_MASK, 0, TIME_DELAY,
			      DELAY_COUNTER);
	if (!ret_wbits)
		dev_err(aic3262->codec->dev, "ADC Power down timedout\n");

	aic3262_wait_bits(aic3262->codec->control_data, AIC3262_DAC_FLAG,
			  AIC3262_DAC_POWER_MASK, 0, TIME_DELAY, DELAY_COUNTER);
	if (!ret_wbits)
		dev_err(aic3262->codec->dev, "DAC Power down timedout\n");

	return 0;
}

int aic3262_dsp_pwrup(void *pv, int state)
{
	struct aic3262_priv *aic3262 = (struct aic3262_priv *)pv;
	int adc_reg_mask = 0;
	int adc_power_mask = 0;
	int dac_reg_mask = 0;
	int dac_power_mask = 0;
	int ret_wbits;

	if (state & AIC3262_COPS_MDSP_A_L) {
		adc_reg_mask |= 0x80;
		adc_power_mask |= AIC3262_LADC_POWER_MASK;
	}
	if (state & AIC3262_COPS_MDSP_A_R) {
		adc_reg_mask |= 0x40;
		adc_power_mask |= AIC3262_RADC_POWER_MASK;
	}

	if (state & AIC3262_COPS_MDSP_A)
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_ADC_DATAPATH_SETUP, 0XC0,
				 adc_reg_mask);

	if (state & AIC3262_COPS_MDSP_D_L) {
		dac_reg_mask |= 0x80;
		dac_power_mask |= AIC3262_LDAC_POWER_MASK;
	}
	if (state & AIC3262_COPS_MDSP_D_R) {
		dac_reg_mask |= 0x40;
		dac_power_mask |= AIC3262_RDAC_POWER_MASK;
	}

	if (state & AIC3262_COPS_MDSP_D)
		aic3262_set_bits(aic3262->codec->control_data,
				 AIC3262_DAC_DATAPATH_SETUP, 0XC0,
				 dac_reg_mask);

	if (state & AIC3262_COPS_MDSP_A) {
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_ADC_FLAG, AIC3262_ADC_POWER_MASK,
				      adc_power_mask, TIME_DELAY,
				      DELAY_COUNTER);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"ADC Power down timedout\n");
	}

	if (state & AIC3262_COPS_MDSP_D) {
		ret_wbits =
		    aic3262_wait_bits(aic3262->codec->control_data,
				      AIC3262_DAC_FLAG, AIC3262_DAC_POWER_MASK,
				      dac_power_mask, TIME_DELAY,
				      DELAY_COUNTER);
		if (!ret_wbits)
			dev_err(aic3262->codec->dev,
				"ADC Power down timedout\n");
	}

	return 0;
}

int aic3262_restart_dsps_sync(void *pv, int run_state)
{

	aic3262_dsp_pwrdwn_status(pv);
	aic3262_dsp_pwrup(pv, run_state);

	return 0;
}

const struct aic3xxx_codec_ops aic3262_cfw_codec_ops = {
	.reg_read  =	aic3262_ops_reg_read,
	.reg_write =	aic3262_ops_reg_write,
	.set_bits  =	aic3262_ops_set_bits,
	.bulk_read =	aic3262_ops_bulk_read,
	.bulk_write =	aic3262_ops_bulk_write,
	.lock      =	aic3262_ops_lock,
	.unlock    =	aic3262_ops_unlock,
	.stop      =	aic3262_ops_stop,
	.restore   =	aic3262_ops_restore,
	.bswap     =	aic3262_ops_adaptivebuffer_swap,
};

