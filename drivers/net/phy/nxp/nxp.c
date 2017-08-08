/*
 * Copyright 2017 NXP
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/delay.h>

#include "nxp.h"

/* Variable can be modified via parameter passed at load time
 * A nonzero value indicates that we should operate in managed mode
 */
static int managed_mode;
/* Permission: do not show up in sysfs */
module_param(managed_mode, int, 0000);
MODULE_PARM_DESC(managed_mode, "Use PHY in managed or autonomous mode");

/* Called to initialize the PHY,
 * including after a reset
 */
static int nxp_config_init(struct phy_device *phydev)
{
	int reg_val;
	int reg_name, reg_value, reg_mask;
	int err;

	/* set features of the PHY */
	reg_val = phy_read(phydev, MII_BMSR);
	if (reg_val < 0)
		goto phy_read_error;
	if (reg_val & BMSR_ESTATEN) {
		reg_val = phy_read(phydev, MII_ESTATUS);

		if (reg_val < 0)
			goto phy_read_error;

		if (reg_val & ESTATUS_100T1_FULL) {
			/* update phydev to include the supported features */
			phydev->supported |= SUPPORTED_100BASET1_FULL;
			phydev->advertising |= ADVERTISED_100BASET1_FULL;
		}
	}

	/* enable configuration register access once during initialization */
	err = phy_configure_bit(phydev, MII_ECTRL, ECTRL_CONFIG_EN, 1);
	if (err < 0)
		goto phy_configure_error;

	/* -enter managed or autonomous mode,
	 *  depending on the value of managed_mode.
	 *  The register layout changed between TJA1100 and TJA1102
	 * -configure LED mode (only tja1100 has LEDs)
	 */
	switch (phydev->phy_id & NXP_PHY_ID_MASK) {
	case NXP_PHY_ID_TJA1100:
		reg_name = MII_CFG1;
		reg_value = TJA1100_CFG1_LED_EN | CFG1_LED_LINKUP;
		if (!managed_mode)
			reg_value |= TJA1100_CFG1_AUTO_OP;
		reg_mask = TJA1100_CFG1_AUTO_OP |
		    TJA1100_CFG1_LED_EN | TJA1100_CFG1_LED_MODE;
		break;

	case NXP_PHY_ID_TJA1102P0:
		reg_name = MII_COMMCFG;
		reg_value = 0;
		if (!managed_mode)
			reg_value |= COMMCFG_AUTO_OP;
		reg_mask = COMMCFG_AUTO_OP;
		break;

	case NXP_PHY_ID_TJA1102P1:
		/* does not have an auto_op bit */
		break;

	default:
		goto unsupported_phy_error;
	}

	/* only configure the phys that have an auto_op bit or leds */
	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P0 ||
	    (phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1100) {
		err = phy_configure_bits(phydev, reg_name, reg_mask, reg_value);
		if (err < 0)
			goto phy_configure_error;
	}

	/* enable sleep confirm */
	err = phy_configure_bit(phydev, MII_CFG1, CFG1_SLEEP_CONFIRM, 1);
	if (err < 0)
		goto phy_configure_error;

	/* set sleep request timeout to 16ms */
	err = phy_configure_bits(phydev, MII_CFG2, CFG2_SLEEP_REQUEST_TO,
				 SLEEP_REQUEST_TO_16MS);
	if (err < 0)
		goto phy_configure_error;

	/* if in managed mode:
	 * -go to normal mode, if currently in standby
	 * (PHY might be pinstrapped to managed mode,
	 * and therefore not in normal mode yet)
	 * -enable link control
	 */
	if (managed_mode) {
		reg_val = phy_read(phydev, MII_ECTRL);
		if (reg_val < 0)
			goto phy_read_error;

		/* mask power mode bits */
		reg_val &= ECTRL_POWER_MODE;

		if (reg_val == POWER_MODE_STANDBY) {
			err = phydev->drv->resume(phydev);
			if (err < 0)
				goto phy_pmode_transit_error;
		}

		set_link_control(phydev, 1);
	}

	/* clear any pending interrupts */
	phydev->drv->ack_interrupt(phydev);

	/* enable all interrupts */
	phydev->interrupts = PHY_INTERRUPT_ENABLED;
	phydev->drv->config_intr(phydev);

	/* Setup and queue a polling function:
	 *
	 * The phy_queue is normally used to schedule the interrupt handler
	 * from interrupt context after an irq has been received.
	 * Here it is repurposed as scheduling mechanism for the poll function
	 */
	if (((struct nxp_specific_data *)phydev->priv)->poll_setup == 0) {
		cancel_work_sync(&phydev->phy_queue);
		INIT_WORK(&phydev->phy_queue, poll);
		queue_work(system_power_efficient_wq, &phydev->phy_queue);

		((struct nxp_specific_data *)phydev->priv)->poll_setup = 1;
	}

	return 0;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: config_init failed\n");
	return reg_val;

phy_pmode_transit_error:
	dev_err(&phydev->dev, "pmode error: config_init failed\n");
	return err;

phy_configure_error:
	dev_err(&phydev->dev, "read/write error: config_init failed\n");
	return err;

unsupported_phy_error:
	dev_err(&phydev->dev, "unsupported phy, config_init failed\n");
	return -1;
}

/* Called during discovery.
 * Used to set up device-specific structures
 */
static int nxp_probe(struct phy_device *phydev)
{
	int err;
	struct nxp_specific_data *nxp_specific;

	nxp_specific = kzalloc(sizeof(*nxp_specific), GFP_KERNEL);
	if (!nxp_specific)
		goto phy_allocation_error;

	phydev->priv = nxp_specific;

	/* detect the current master/slave setting and save it in phydev */
	((struct nxp_specific_data *)phydev->priv)->is_master =
	    get_master_cfg(phydev);

	((struct nxp_specific_data *)phydev->priv)->poll_setup = 0;

	/* init the phy. Should be called from phy framework but is not ? */
	err = phydev->drv->config_init(phydev);
	if (err < 0)
		return err;

	/* register sysfs files */
	err = sysfs_create_group(&phydev->dev.kobj, &nxp_attribute_group);
	if (err)
		goto register_sysfs_error;

	return 0;

/* error handling */
register_sysfs_error:
	dev_err(&phydev->dev, "sysfs file creation failed\n");
	return -ENOMEM;

phy_allocation_error:
	dev_err(&phydev->dev, "memory allocation for priv data failed\n");
	return -ENOMEM;
}

/* Clears up any memory, removes sysfs nodes and cancels polling */
static void nxp_remove(struct phy_device *phydev)
{
	/* unregister sysfs files */
	sysfs_remove_group(&phydev->dev.kobj, &nxp_attribute_group);

	/* free custom data struct */
	if (phydev->priv) {
		kzfree(phydev->priv);
		phydev->priv = NULL;
	}

	/* cancel scheduled work */
	cancel_work_sync(&phydev->phy_queue);
}

/* Clears any pending interrupts */
static int nxp_ack_interrupt(struct phy_device *phydev)
{
	int err;

	/* interrupts are acknowledged by reading, ie. clearing MII_INTSRC */
	err = phy_read(phydev, MII_INTSRC);
	if (err < 0)
		goto phy_read_error;
	return 0;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: ack_interrupt failed\n");
	return err;
}

/* Checks if the PHY generated an interrupt */
static int nxp_did_interrupt(struct phy_device *phydev)
{
	int reg_val;

	reg_val = phy_read(phydev, MII_INTSRC);
	if (reg_val < 0)
		goto phy_read_error;

	/* return bitmask of possible interrupts bits that are set */
	return (reg_val & INTERRUPT_ALL);

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: did_interrupt failed\n");
	return 0;
}

/* Enables or disables interrupts */
static int nxp_config_intr(struct phy_device *phydev)
{
	int err;
	int interrupts;

	interrupts = phydev->interrupts;

	if (interrupts == PHY_INTERRUPT_ENABLED) {
		/* enable all interrupts
		 * PHY_INTERRUPT_ENABLED macro does not interfere with any
		 * of the possible interrupt source macros
		 */
		err = phy_write(phydev, MII_INTMASK, INTERRUPT_ALL);
	} else if (interrupts == PHY_INTERRUPT_DISABLED) {
		/* disable all interrupts */
		err = phy_write(phydev, MII_INTMASK, INTERRUPT_NONE);
	} else {
		/* interpret value of interrupts as interrupt mask */
		err = phy_write(phydev, MII_INTMASK, interrupts);
	}

	if (err < 0)
		goto phy_write_error;
	return 0;

phy_write_error:
	dev_err(&phydev->dev, "write error: config_intr failed\n");
	return err;
}

/* handler for link changes
 * if interrupts would be used, this would be handled by phy_change()
 */
static inline void handle_link_changes(struct phy_device *phydev)
{
	mutex_lock(&phydev->lock);
	if ((PHY_RUNNING == phydev->state) || (PHY_NOLINK == phydev->state))
		phydev->state = PHY_CHANGELINK;
	mutex_unlock(&phydev->lock);

	/* reschedule state queue work to run as soon as possible */
	cancel_delayed_work_sync(&phydev->state_queue);
	queue_delayed_work(system_power_efficient_wq, &phydev->state_queue, 0);
}

/* interrupt handler for pwon interrupts */
static inline void handle_pwon_interrupt(struct phy_device *phydev)
{
	/* after a power down reinitialize the phy */
	phydev->drv->config_init(phydev);

	/* update master/slave setting */
	((struct nxp_specific_data *)phydev->priv)->is_master =
	    get_master_cfg(phydev);

	/* For TJA1102, pwon interrupts only exist on TJA1102p0
	 * Find TJA1102p1 to reinitialize it too
	 */
	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P0) {
		struct phy_device *phydevp1 =
		    search_phy_by_addr(phydev->addr + 1);
		if (!phydevp1)
			return;

		phydevp1->drv->config_init(phydevp1);
		((struct nxp_specific_data *)phydevp1->priv)->is_master =
		    get_master_cfg(phydevp1);
	}
}

/* interrupt handler for undervoltage recovery interrupts */
static inline void handle_uvr_interrupt(struct phy_device *phydev)
{
	phydev->drv->resume(phydev);

	/* For TJA1102, UVR interrupts only exist on TJA1102p0
	 * Find TJA1102p1 to resume it too
	 */
	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P0) {
		struct phy_device *phydevp1 =
		    search_phy_by_addr(phydev->addr + 1);
		if (!phydevp1)
			return;

		phydevp1->drv->resume(phydevp1);
	}
}

/* polling function, that is executed regularly to handle phy interrupts */
static void poll(struct work_struct *work)
{
	int interrupts, interrupt_mask;
	struct phy_device *phydev =
	    container_of(work, struct phy_device, phy_queue);

	/* query phy for interrupts */
	interrupts = nxp_did_interrupt(phydev);

	/* mask out all disabled interrupts */
	interrupt_mask = phy_read(phydev, MII_INTMASK);
	interrupts &= interrupt_mask;

	/* handle interrupts
	 * - reinitialize after power down
	 * - resume PHY after an external WAKEUP was received
	 * - resume PHY after an undervoltage recovery
	 * - adjust state on link changes
	 * - check for some PHY errors
	 */

	/* SMI not disabled and read was successful */
	if ((interrupts != 0xffff) && (interrupt_mask >= 0)) {
		if (interrupts & INTERRUPT_PWON)
			handle_pwon_interrupt(phydev);
		else if (interrupts & INTERRUPT_UV_RECOVERY)
			handle_uvr_interrupt(phydev);
		else if (interrupts & INTERRUPT_WAKEUP)
			phydev->drv->resume(phydev);

		/* link changes */
		if (interrupts & INTERRUPT_LINK_STATUS_FAIL ||
		    interrupts & INTERRUPT_LINK_STATUS_UP)
			handle_link_changes(phydev);

		/* warnings */
		if (interrupts & INTERRUPT_PHY_INIT_FAIL)
			dev_err(&phydev->dev, "PHY initialization failed\n");
		if (interrupts & INTERRUPT_LINK_STATUS_FAIL)
			dev_err(&phydev->dev, "PHY link status failed\n");
		if (interrupts & INTERRUPT_SYM_ERR)
			dev_err(&phydev->dev, "PHY symbol error detected\n");
		if (interrupts & INTERRUPT_SNR_WARNING)
			dev_err(&phydev->dev, "PHY SNR warning\n");
		if (interrupts & INTERRUPT_CONTROL_ERROR)
			dev_err(&phydev->dev, "PHY control error\n");
		if (interrupts & INTERRUPT_UV_ERR)
			dev_err(&phydev->dev, "PHY undervoltage error\n");
		if (interrupts & INTERRUPT_TEMP_ERROR)
			dev_err(&phydev->dev, "PHY temperature error\n");
	}

	/* requeue poll function */
	msleep(POLL_PAUSE);	/* msleep is non-blocking */
	queue_work(system_power_efficient_wq, &phydev->phy_queue);
}

/* helper function, waits until a given condition is met
 *
 * The function delays until the part of the register at reg_addr,
 * defined by reg_mask equals cond, or a timeout (timeout*DELAY_LENGTH) occurs.
 * @return	0 if condition is met, <0 if timeout or read error occurred
 */
static int wait_on_condition(struct phy_device *phydev, int reg_addr,
			     int reg_mask, int cond, int timeout)
{
	int reg_val;

	do {
		udelay(DELAY_LENGTH);

		reg_val = phy_read(phydev, reg_addr);
		if (reg_val < 0)
			return reg_val;
	} while ((reg_val & reg_mask) != cond && --timeout);

	if (timeout)
		return 0;
	return -1;
}

/* helper function, enables or disables link control */
static void set_link_control(struct phy_device *phydev, int enable_link_control)
{
	int err;

	err = phy_configure_bit(phydev, MII_ECTRL, ECTRL_LINK_CONTROL,
				enable_link_control);
	if (err < 0)
		goto phy_configure_error;

	return;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: setting link control failed\n");
	return;
}

/* Helper function, configures phy as master or slave
 * @param  phydev    the phy to be configured
 * @param  setMaster ==0: set to slave
 *                   !=0: set to master
 * @return           0 on success, error code on failure
 */
static int set_master_cfg(struct phy_device *phydev, int setMaster)
{
	int err;

	/* disable link control prior to master/slave cfg */
	set_link_control(phydev, 0);

	/* write configuration to the phy */
	err = phy_configure_bit(phydev, MII_CFG1, CFG1_MASTER_SLAVE, setMaster);
	if (err < 0)
		goto phy_configure_error;

	/* enable link control after master/slave cfg was set */
	set_link_control(phydev, 1);

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: set_master_cfg failed\n");
	return err;
}

/* Helper function, reads master/slave configuration of phy
 * @param  phydev    the phy to be read
 *
 * @return           ==0: is slave
 *                   !=0: is master
 */
static int get_master_cfg(struct phy_device *phydev)
{
	int reg_val;

	/* read the current configuration */
	reg_val = phy_read(phydev, MII_CFG1);
	if (reg_val < 0)
		goto phy_read_error;

	return reg_val & CFG1_MASTER_SLAVE;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: get_master_cfg failed\n");
	return reg_val;
}

/* retrieves the link status from COMMSTAT register */
static int get_link_status(struct phy_device *phydev)
{
	int reg_val;

	reg_val = phy_read(phydev, MII_COMMSTAT);
	if (reg_val < 0)
		goto phy_read_error;

	return reg_val & COMMSTAT_LINK_UP;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: get_link_status failed\n");
	return reg_val;
}

/* issues a sleep request, if in managed mode */
static int nxp_sleep(struct phy_device *phydev)
{
	int err;

	if (!managed_mode)
		goto phy_auto_op_error;

	/* clear power mode bits and set them to sleep request */
	err = phy_configure_bits(phydev, MII_ECTRL, ECTRL_POWER_MODE,
				 POWER_MODE_SLEEPREQUEST);
	if (err < 0)
		goto phy_configure_error;

	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P0 ||
	    (phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P1) {
		/* tja1102 has an extra sleep state indicator in ECTRL
		 * If transition is successful this can be detected immediately,
		 * without waiting for SLEEP_REQUEST_TO to pass
		 */
		err = wait_on_condition(phydev, MII_ECTRL, ECTRL_POWER_MODE,
					POWER_MODE_SLEEP, SLEEP_REQUEST_TO);
		if (err < 0)
			goto phy_transition_error;
	} else if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1100) {
		/* TJA1100 disables SMI when entering SLEEP
		 * The SMI bus is pulled up, that means every
		 * SMI read will return 0xffff.
		 * We can use this to check if PHY entered SLEEP.
		 */
		err = wait_on_condition(phydev, MII_ECTRL,
					0xffff, 0xffff, SLEEP_REQUEST_TO);
		if (err < 0)
			goto phy_transition_error;
	}

	return 0;

/* error handling */
phy_auto_op_error:
	dev_err(&phydev->dev, "phy is in auto mode: sleep not possible\n");
	return 0;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: entering sleep failed\n");
	return err;

phy_transition_error:
	dev_err(&phydev->dev, "sleep request timed out\n");
	return err;
}

/* wakes up the phy from sleep mode */
static int wakeup_from_sleep(struct phy_device *phydev)
{
	int err;
	unsigned long wakeup_delay;

	if (!managed_mode)
		goto phy_auto_op_error;

	/* set power mode bits to standby mode */
	err = phy_configure_bits(phydev, MII_ECTRL, ECTRL_POWER_MODE,
				 POWER_MODE_STANDBY);
	if (err < 0)
		goto phy_configure_error;

	/* wait until power mode transition is completed */
	err = wait_on_condition(phydev, MII_ECTRL, ECTRL_POWER_MODE,
				POWER_MODE_STANDBY, POWER_MODE_TIMEOUT);
	if (err < 0)
		goto phy_transition_error;

	/* set power mode bits to normal mode */
	err = phy_configure_bits(phydev, MII_ECTRL, ECTRL_POWER_MODE,
				 POWER_MODE_NORMAL);
	if (err < 0)
		goto phy_configure_error;

	/* wait until the PLL is locked, indicating a completed transition */
	err = wait_on_condition(phydev, MII_GENSTAT, GENSTAT_PLL_LOCKED,
				GENSTAT_PLL_LOCKED, POWER_MODE_TIMEOUT);
	if (err < 0)
		goto phy_transition_error;
	/* if phy is configured as slave, also send a wakeup request
	 * to master
	 */
	if (!((struct nxp_specific_data *)phydev->priv)->is_master) {
		/* link control must be reset for wake request */
		set_link_control(phydev, 0);

		/* start sending bus wakeup signal */
		err = phy_configure_bit(phydev, MII_ECTRL,
					ECTRL_WAKE_REQUEST, 1);
		if (err < 0)
			goto phy_configure_error;

		switch (phydev->phy_id & NXP_PHY_ID_MASK) {
		case NXP_PHY_ID_TJA1100:
			wakeup_delay = TJA100_WAKE_REQUEST_TIMEOUT_US;
			break;
		case NXP_PHY_ID_TJA1102P0:
			/* fall through */
		case NXP_PHY_ID_TJA1102P1:
			wakeup_delay = TJA102_WAKE_REQUEST_TIMEOUT_US;
			break;
		default:
			goto unsupported_phy_error;
		}

		/* wait until link partner is guranteed to be awake */
		usleep_range(wakeup_delay, wakeup_delay + 1U);

		/* stop sending bus wakeup signal */
		err = phy_configure_bit(phydev, MII_ECTRL,
					ECTRL_WAKE_REQUEST, 0);
		if (err < 0)
			goto phy_configure_error;
	}

	/* reenable link control */
	set_link_control(phydev, 1);

	return 0;

/* error handling */
phy_auto_op_error:
	dev_err(&phydev->dev, "phy is in auto mode: wakeup not possible\n");
	return 0;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: wakeup failed\n");
	return err;

phy_transition_error:
	dev_err(&phydev->dev, "power mode transition failed\n");
	return err;

unsupported_phy_error:
	dev_err(&phydev->dev, "unsupported phy, wakeup failed\n");
	return -1;
}

/* send a wakeup request to the link partner */
static int wakeup_from_normal(struct phy_device *phydev)
{
	int err;

	/* start sending bus wakeup signal */
	err = phy_configure_bit(phydev, MII_ECTRL, ECTRL_WAKE_REQUEST, 1);
	if (err < 0)
		goto phy_configure_error;

	/* stop sending bus wakeup signal */
	err = phy_configure_bit(phydev, MII_ECTRL, ECTRL_WAKE_REQUEST, 0);
	if (err < 0)
		goto phy_configure_error;

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: wakeup_from_normal failed\n");
	return err;
}

/* wake up phy if is in sleep mode, send wakeup request if in normal mode */
static int nxp_wakeup(struct phy_device *phydev)
{
	int reg_val;
	int err = 0;

	reg_val = phy_read(phydev, MII_ECTRL);
	if (reg_val < 0)
		goto phy_read_error;

	reg_val &= ECTRL_POWER_MODE;
	switch (reg_val) {
	case POWER_MODE_NORMAL:
		err = wakeup_from_normal(phydev);
		break;
	case POWER_MODE_SLEEP:
		err = wakeup_from_sleep(phydev);
		break;
	case 0xffff & ECTRL_POWER_MODE:
		/* TJA1100 disables SMI during sleep */
		goto phy_SMI_disabled;
	default:
		break;
	}
	if (err < 0)
		goto phy_configure_error;

	return 0;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: nxp_wakeup failed\n");
	return reg_val;

phy_SMI_disabled:
	dev_err(&phydev->dev, "SMI interface disabled, cannot be woken up\n");
	return 0;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: wakeup_from_normal failed\n");
	return err;
}

/* power mode transition to standby */
static int nxp_suspend(struct phy_device *phydev)
{
	int err;

	if (!managed_mode)
		goto phy_auto_op_error;

	/* set BMCR_PDOWN bit in MII_BMCR register */
	err = phy_configure_bit(phydev, MII_BMCR, BMCR_PDOWN, 1);
	if (err < 0)
		goto phy_configure_error;

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: resume failed\n");
	return err;

phy_auto_op_error:
	dev_err(&phydev->dev, "phy is in auto mode: suspend not possible\n");
	return 0;
}

/* power mode transition from standby to normal */
static int nxp_resume(struct phy_device *phydev)
{
	int err;

	/* clear BMCR_PDOWN bit in MII_BMCR register */
	err = phy_configure_bit(phydev, MII_BMCR, BMCR_PDOWN, 0);
	if (err < 0)
		goto phy_configure_error;

	/* transit to normal mode */
	err = phy_configure_bits(phydev, MII_ECTRL, ECTRL_POWER_MODE,
				 POWER_MODE_NORMAL);
	if (err < 0)
		goto phy_configure_error;

	/* wait until power mode transition is completed */
	err = wait_on_condition(phydev, MII_ECTRL, ECTRL_POWER_MODE,
				POWER_MODE_NORMAL, POWER_MODE_TIMEOUT);
	if (err < 0)
		goto phy_transition_error;

	/* wait until the PLL is locked, indicating a completed transition */
	err = wait_on_condition(phydev, MII_GENSTAT, GENSTAT_PLL_LOCKED,
				GENSTAT_PLL_LOCKED, POWER_MODE_TIMEOUT);
	if (err < 0)
		goto phy_pll_error;

	/* reenable link control */
	set_link_control(phydev, 1);

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: resume failed\n");
	return err;

phy_transition_error:
	dev_err(&phydev->dev, "power mode transition failed\n");
	return err;

phy_pll_error:
	dev_err(&phydev->dev, "Error: PLL is unstable and not locked\n");
	return err;
}

/* Configures the autonegotiation capabilities */
static int nxp_config_aneg(struct phy_device *phydev)
{
	/* disable autoneg and manually configure speed, duplex, pause frames */
	phydev->autoneg = 0;

	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;

	phydev->pause = 0;
	phydev->asym_pause = 0;

	return 0;
}

/* helper function, enters the test mode specified by tmode
 * @return          0 if test mode was entered, <0 on read or write error
 */
static int enter_test_mode(struct phy_device *phydev, enum test_mode tmode)
{
	int reg_val = 0;
	int err;

	switch (tmode) {
	case NO_TMODE:
		reg_val = ECTRL_NO_TMODE;
		break;
	case TMODE1:
		reg_val = ECTRL_TMODE1;
		break;
	case TMODE2:
		reg_val = ECTRL_TMODE2;
		break;
	case TMODE3:
		reg_val = ECTRL_TMODE3;
		break;
	case TMODE4:
		reg_val = ECTRL_TMODE4;
		break;
	case TMODE5:
		reg_val = ECTRL_TMODE5;
		break;
	case TMODE6:
		reg_val = ECTRL_TMODE6;
		break;
	default:
		break;
	}

	if (reg_val > 0) {
		/* set test mode bits accordingly */
		err = phy_configure_bits(phydev, MII_ECTRL, ECTRL_TEST_MODE,
					 reg_val);
		if (err < 0)
			goto phy_configure_error;
	}

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: setting test mode failed\n");
	return err;
}

/* helper function, enables or disables loopback mode
 * @return	0 if loopback mode was configured, <0 on read or write error
 */
static int set_loopback(struct phy_device *phydev, int enable_loopback)
{
	int err;

	err = phy_configure_bit(phydev, MII_BMCR, BMCR_LOOPBACK,
				enable_loopback);
	if (err < 0)
		goto phy_configure_error;

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: configuring loopback failed\n");
	return err;
}

/* helper function, enters the loopback mode specified by lmode
 * @return          0 if loopback mode was entered, <0 on read or write error
 */
static int enter_loopback_mode(struct phy_device *phydev,
			       enum loopback_mode lmode)
{
	int reg_val = 0;
	int err;

	/* disable link control prior to loopback cfg */
	set_link_control(phydev, 0);

	switch (lmode) {
	case NO_LMODE:
		/* disable loopback */
		err = set_loopback(phydev, 0);
		if (err < 0)
			goto phy_set_loopback_error;
		break;
	case INTERNAL_LMODE:
		reg_val = ECTRL_INTERNAL_LMODE;
		break;
	case EXTERNAL_LMODE:
		reg_val = ECTRL_EXTERNAL_LMODE;
		break;
	case REMOTE_LMODE:
		reg_val = ECTRL_REMOTE_LMODE;
		break;
	default:
		break;
	}

	if (reg_val > 0) {
		err = phy_configure_bits(phydev, MII_ECTRL,
					 ECTRL_LOOPBACK_MODE, reg_val);
		if (err < 0)
			goto phy_configure_error;

		/* enable loopback */
		err = set_loopback(phydev, 1);
		if (err < 0)
			goto phy_set_loopback_error;
	}

	/* enable link control after loopback cfg was set */
	set_link_control(phydev, 1);

	return 0;

/* error handling */
phy_set_loopback_error:
	dev_err(&phydev->dev, "error: enable/disable loopback failed\n");
	return err;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: setting loopback mode failed\n");
	return err;
}

/* helper function, enters the led mode specified by lmode
 * @return          0 if led mode was entered, <0 on read or write error
 */
static int enter_led_mode(struct phy_device *phydev, enum led_mode lmode)
{
	int reg_val = 0;
	int err;

	switch (lmode) {
	case NO_LED_MODE:
		/* disable led */
		err = phy_configure_bit(phydev, MII_CFG1,
					TJA1100_CFG1_LED_EN, 0);
		if (err < 0)
			goto phy_configure_error;
		break;
	case LINKUP_LED_MODE:
		reg_val = CFG1_LED_LINKUP;
		break;
	case FRAMEREC_LED_MODE:
		reg_val = CFG1_LED_FRAMEREC;
		break;
	case SYMERR_LED_MODE:
		reg_val = CFG1_LED_SYMERR;
		break;
	case CRSSIG_LED_MODE:
		reg_val = CFG1_LED_CRSSIG;
		break;
	default:
		break;
	}

	if (reg_val > 0) {
		err = phy_configure_bits(phydev, MII_CFG1,
					 TJA1100_CFG1_LED_MODE, reg_val);
		if (err < 0)
			goto phy_configure_error;

		/* enable led */
		err = phy_configure_bit(phydev, MII_CFG1,
					TJA1100_CFG1_LED_EN, 1);
		if (err < 0)
			goto phy_configure_error;
	}

	return 0;

/* error handling */
phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: setting led mode failed\n");
	return err;
}

/* This function handles read accesses to the node 'master_cfg' in
 * sysfs.
 * Depending on current configuration of the phy, the node reads
 * 'master' or 'slave'
 */
static ssize_t sysfs_get_master_cfg(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int is_master;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	is_master = get_master_cfg(phydev);

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 is_master ? "master" : "slave");
}

/* This function handles write accesses to the node 'master_cfg' in sysfs.
 * Depending on the value written to it, the phy is configured as
 * master or slave
 */
static ssize_t sysfs_set_master_cfg(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int err;
	int setMaster;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	/* parse the buffer */
	err = kstrtoint(buf, 10, &setMaster);
	if (err < 0)
		goto phy_parse_error;

	/* write configuration to the phy */
	err = set_master_cfg(phydev, setMaster);
	if (err < 0)
		goto phy_cfg_error;

	/* update phydev */
	((struct nxp_specific_data *)phydev->priv)->is_master = setMaster;

	return count;

/* error handling */
phy_parse_error:
	dev_err(&phydev->dev, "parse error: sysfs_set_master_cfg failed\n");
	return err;

phy_cfg_error:
	dev_err(&phydev->dev, "phy cfg error: sysfs_set_master_cfg failed\n");
	return err;
}

/* This function handles read accesses to the node 'power_cfg' in sysfs.
 * Reading the node returns the current power state
 */
static ssize_t sysfs_get_power_cfg(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int reg_val;
	char *pmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	reg_val = phy_read(phydev, MII_ECTRL);
	if (reg_val < 0)
		goto phy_read_error;

	/* mask power mode bits */
	reg_val &= ECTRL_POWER_MODE;

	switch (reg_val) {
	case POWER_MODE_NORMAL:
		pmode = "POWER_MODE_NORMAL\n";
		break;
	case POWER_MODE_SLEEPREQUEST:
		pmode = "POWER_MODE_SLEEPREQUEST\n";
		break;
	case POWER_MODE_SLEEP:
		pmode = "POWER_MODE_SLEEP\n";
		break;
	case POWER_MODE_SILENT:
		pmode = "POWER_MODE_SILENT\n";
		break;
	case POWER_MODE_STANDBY:
		pmode = "POWER_MODE_STANDBY\n";
		break;
	case POWER_MODE_NOCHANGE:
		pmode = "POWER_MODE_NOCHANGE\n";
		break;
	default:
		pmode = "unknown\n";
	}

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, pmode);

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: sysfs_get_power_cfg failed\n");
	return reg_val;
}

/* This function handles write accesses to the node 'power_cfg' in
 * sysfs.
 * Depending on the value written to it, the phy enters a certain
 * power state.
 */
static ssize_t sysfs_set_power_cfg(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int err;
	int pmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	/* parse the buffer */
	err = kstrtoint(buf, 10, &pmode);
	if (err < 0)
		goto phy_parse_error;

	switch (pmode) {
	case 0:
		err = phydev->drv->suspend(phydev);
		break;
	case 1:
		err = phydev->drv->resume(phydev);
		break;
	case 2:
		err = nxp_sleep(phydev);
		break;
	case 3:
		err = nxp_wakeup(phydev);
		break;
	default:
		break;
	}

	if (err)
		goto phy_pmode_transit_error;

	return count;

/* error handling */
phy_parse_error:
	dev_err(&phydev->dev, "parse error: sysfs_set_power_cfg failed\n");
	return err;

phy_pmode_transit_error:
	dev_err(&phydev->dev, "pmode error: sysfs_set_power_cfg failed\n");
	return err;
}

/* This function handles read accesses to the node 'loopback_cfg' in sysfs
 * Reading the node returns the current loopback configuration
 */
static ssize_t sysfs_get_loopback_cfg(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int reg_val;
	char *lmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	reg_val = phy_read(phydev, MII_BMCR);
	if (reg_val < 0)
		goto phy_read_error;

	if (reg_val & BMCR_LOOPBACK) {
		/* loopback enabled */
		reg_val = phy_read(phydev, MII_ECTRL);
		if (reg_val < 0)
			goto phy_read_error;

		/* mask loopback mode bits */
		reg_val &= ECTRL_LOOPBACK_MODE;

		switch (reg_val) {
		case ECTRL_INTERNAL_LMODE:
			lmode = "INTERNAL_LOOPBACK\n";
			break;
		case ECTRL_EXTERNAL_LMODE:
			lmode = "EXTERNAL_LOOPBACK\n";
			break;
		case ECTRL_REMOTE_LMODE:
			lmode = "REMOTE_LOOPBACK\n";
			break;
		default:
			lmode = "unknown\n";
		}
	} else {
		/* loopback disabled */
		lmode = "LOOPBACK_DISABLED\n";
	}

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, lmode);

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: sysfs_get_loopback_cfg failed\n");
	return reg_val;
}

/* This function handles write accesses to the node 'loopback_cfg'
 * in sysfs.
 * Depending on the value written to it, the phy enters a certain
 * loopback state.
 */
static ssize_t sysfs_set_loopback_cfg(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int err;
	int lmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	if (!managed_mode)
		goto phy_auto_op_error;

	/* parse the buffer */
	err = kstrtoint(buf, 10, &lmode);
	if (err < 0)
		goto phy_parse_error;

	switch (lmode) {
	case 0:
		err = enter_loopback_mode(phydev, NO_LMODE);
		break;
	case 1:
		err = enter_loopback_mode(phydev, INTERNAL_LMODE);
		break;
	case 2:
		err = enter_loopback_mode(phydev, EXTERNAL_LMODE);
		break;
	case 3:
		err = enter_loopback_mode(phydev, REMOTE_LMODE);
		break;
	default:
		break;
	}

	if (err)
		goto phy_lmode_transit_error;

	return count;

/* error handling */
phy_auto_op_error:
	dev_err(&phydev->dev, "phy is in auto mode: loopback not available\n");
	return count;

phy_parse_error:
	dev_err(&phydev->dev, "parse error: sysfs_set_loopback_cfg failed\n");
	return err;

phy_lmode_transit_error:
	dev_err(&phydev->dev, "lmode error: sysfs_set_loopback_cfg failed\n");
	return err;
}

/* This function handles read accesses to the node 'cable_test' in sysfs
 * Reading the node executes a cable test and returns the result
 */
static ssize_t sysfs_get_cable_test(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int reg_val;
	int err;
	char *c_test_result;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	if (!managed_mode)
		goto phy_auto_op_error;

	/* disable link control prior to cable test */
	set_link_control(phydev, 0);

	/* execute a cable test */
	err = phy_configure_bit(phydev, MII_ECTRL, ECTRL_CABLE_TEST, 1);
	if (err < 0)
		goto phy_configure_error;

	/* wait until test is completed */
	err = wait_on_condition(phydev, MII_ECTRL, ECTRL_CABLE_TEST,
				0, CABLE_TEST_TIMEOUT);
	if (err < 0)
		goto phy_transition_error;

	/* evaluate the test results */
	reg_val = phy_read(phydev, MII_EXTERNAL_STATUS);
	if (reg_val < 0)
		goto phy_read_error;

	if (reg_val & EXTSTAT_SHORT_DETECT)
		c_test_result = "SHORT_DETECT\n";
	else if (reg_val & EXTSTAT_OPEN_DETECT)
		c_test_result = "OPEN_DETECT\n";
	else
		c_test_result = "NO_ERROR\n";

	/* reenable link control after cable test */
	set_link_control(phydev, 1);

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, c_test_result);

/* error handling */
phy_auto_op_error:
	dev_err(&phydev->dev, "phy is in auto mode: cabletest not available\n");
	return 0;

phy_read_error:
	dev_err(&phydev->dev, "read error: sysfs_get_cable_test failed\n");
	return reg_val;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: sysfs_get_cable_test failed\n");
	return err;

phy_transition_error:
	dev_err(&phydev->dev, "Timeout: cable test failed to finish in time\n");
	return err;
}

/* This function handles read accesses to the node 'test_mode' in sysfs
 * Reading the node returns the current test mode configuration
 */
static ssize_t sysfs_get_test_mode(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int reg_val;
	char *tmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	reg_val = phy_read(phydev, MII_ECTRL);
	if (reg_val < 0)
		goto phy_read_error;

	/* mask test mode bits */
	reg_val &= ECTRL_TEST_MODE;

	switch (reg_val) {
	case ECTRL_NO_TMODE:
		tmode = "NO_TMODE\n";
		break;
	case ECTRL_TMODE1:
		tmode = "TMODE1\n";
		break;
	case ECTRL_TMODE2:
		tmode = "TMODE2\n";
		break;
	case ECTRL_TMODE3:
		tmode = "TMODE3\n";
		break;
	case ECTRL_TMODE4:
		tmode = "TMODE4\n";
		break;
	case ECTRL_TMODE5:
		tmode = "TMODE5\n";
		break;
	case ECTRL_TMODE6:
		tmode = "TMODE6\n";
		break;
	default:
		tmode = "unknown\n";
	}

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, tmode);

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: sysfs_get_test_mode failed\n");
	return reg_val;
}

/* This function handles write accesses to the node 'test_mode' in sysfs
 * Depending on the value written to it, the phy enters a certain test mode
 */
static ssize_t sysfs_set_test_mode(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int err;
	int tmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	if (!managed_mode)
		goto phy_auto_op_error;

	/* parse the buffer */
	err = kstrtoint(buf, 10, &tmode);
	if (err < 0)
		goto phy_parse_error;

	switch (tmode) {
	case 0:
		err = enter_test_mode(phydev, NO_TMODE);
		/* enable link control after exiting test */
		set_link_control(phydev, 1);
		break;
	case 1:
		/* disbale link control before entering test */
		set_link_control(phydev, 0);
		err = enter_test_mode(phydev, TMODE1);
		break;
	case 2:
		/* disbale link control before entering test */
		set_link_control(phydev, 0);
		err = enter_test_mode(phydev, TMODE2);
		break;
	case 3:
		/* disbale link control before entering test */
		set_link_control(phydev, 0);
		err = enter_test_mode(phydev, TMODE3);
		break;
	case 4:
		/* disbale link control before entering test */
		set_link_control(phydev, 0);
		err = enter_test_mode(phydev, TMODE4);
		break;
	case 5:
		/* disbale link control before entering test */
		set_link_control(phydev, 0);
		err = enter_test_mode(phydev, TMODE5);
		break;
	case 6:
		/* disbale link control before entering test */
		set_link_control(phydev, 0);
		err = enter_test_mode(phydev, TMODE6);
		break;
	default:
		break;
	}

	if (err)
		goto phy_tmode_transit_error;

	return count;

/* error handling */
phy_auto_op_error:
	dev_err(&phydev->dev, "phy is in auto mode: testmodes not available\n");
	return count;

phy_parse_error:
	dev_err(&phydev->dev, "parse error: sysfs_get_test_mode failed\n");
	return err;

phy_tmode_transit_error:
	dev_err(&phydev->dev, "tmode error: sysfs_get_test_mode failed\n");
	return err;
}

/* This function handles read accesses to the node 'led_cfg' in sysfs.
 * Reading the node returns the current led configuration
 */
static ssize_t sysfs_get_led_cfg(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int reg_val;
	char *lmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	lmode = "DISABLED\n";

	/* only TJA1100 has leds */
	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1100) {
		reg_val = phy_read(phydev, MII_CFG1);
		if (reg_val < 0)
			goto phy_read_error;

		if (reg_val & TJA1100_CFG1_LED_EN) {
			/* mask led mode bits */
			reg_val &= TJA1100_CFG1_LED_MODE;

			switch (reg_val) {
			case CFG1_LED_LINKUP:
				lmode = "LINKUP\n";
				break;
			case CFG1_LED_FRAMEREC:
				lmode = "FRAMEREC\n";
				break;
			case CFG1_LED_SYMERR:
				lmode = "SYMERR\n";
				break;
			case CFG1_LED_CRSSIG:
				lmode = "CRSSIG\n";
				break;
			default:
				lmode = "unknown\n";
			}
		}
	}

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, lmode);

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: sysfs_get_led_cfg failed\n");
	return reg_val;
}

/* This function handles write accesses to the node 'led_cfg' in sysfs
 * Depending on the value written to it, the led mode is configured
 * accordingly.
 */
static ssize_t sysfs_set_led_cfg(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int err;
	int lmode;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	/* parse the buffer */
	err = kstrtoint(buf, 10, &lmode);
	if (err < 0)
		goto phy_parse_error;

	switch (lmode) {
	case 0:
		err = enter_led_mode(phydev, NO_LED_MODE);
		break;
	case 1:
		err = enter_led_mode(phydev, LINKUP_LED_MODE);
		break;
	case 2:
		err = enter_led_mode(phydev, FRAMEREC_LED_MODE);
		break;
	case 3:
		err = enter_led_mode(phydev, SYMERR_LED_MODE);
		break;
	case 4:
		err = enter_led_mode(phydev, CRSSIG_LED_MODE);
		break;
	default:
		break;
	}

	if (err)
		goto phy_lmode_transit_error;

	return count;

/* error handling */
phy_parse_error:
	dev_err(&phydev->dev, "parse error: sysfs_set_led_cfg failed\n");
	return err;

phy_lmode_transit_error:
	dev_err(&phydev->dev, "lmode error: sysfs_set_led_cfg failed\n");
	return err;
}

/* This function handles read accesses to the node 'link_status' in sysfs
 * Depending on current link status of the phy, the node reads
 * 'up' or 'down'
 */
static ssize_t sysfs_get_link_status(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int linkup;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	linkup = get_link_status(phydev);

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE, "%s\n", linkup ? "up" : "down");
}

/* This function handles read accesses to the node 'wakeup_cfg' in sysfs
 * Reading the node returns the current status of the bits
 * FWDPHYLOC, REMWUPHY, LOCWUPHY, FWDPHYREM
 */
static ssize_t sysfs_get_wakeup_cfg(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int reg_val;
	int fwdphyloc_en, remwuphy_en, locwuphy_en, fwdphyrem_en;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P0 ||
	    (phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P1) {
		reg_val = phy_read(phydev, MII_CFG1);
		if (reg_val < 0)
			goto phy_read_error;

		fwdphyloc_en = 0;
		remwuphy_en = 0;
		locwuphy_en = 0;
		fwdphyrem_en = 0;

		if (reg_val & TJA1102_CFG1_FWDPHYLOC)
			fwdphyloc_en = 1;
		if (reg_val & CFG1_REMWUPHY)
			remwuphy_en = 1;
		if (reg_val & CFG1_LOCWUPHY)
			locwuphy_en = 1;
		if (reg_val & CFG1_FWDPHYREM)
			fwdphyrem_en = 1;
	} else if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1100) {
		remwuphy_en = 1;	/* not configurable, always enabled */
		fwdphyloc_en = 0;	/* not supported */

		/* The status LED and WAKE input share a pin, so ultimately
		 * configuration depends on the hardware setup.
		 * If LED is disabled, we assume the pin is used for WAKE.
		 * In this case, the phy wakes up upon local wakeup event
		 * via the WAKE pin and also forwards it.
		 */
		reg_val = phy_read(phydev, MII_CFG1);
		if (reg_val < 0)
			goto phy_read_error;

		if (reg_val & TJA1100_CFG1_LED_EN) {
			locwuphy_en = 0;
			fwdphyrem_en = 0;
		} else {
			locwuphy_en = 1;
			fwdphyrem_en = 1;
		}
	} else {
		goto unsupported_phy_error;
	}

	/* write result into the buffer */
	return scnprintf(buf, PAGE_SIZE,
			 "fwdphyloc[%s], remwuphy[%s], locwuphy[%s], fwdphyrem[%s]\n",
			 (fwdphyloc_en ? "on" : "off"),
			 (remwuphy_en ? "on" : "off"),
			 (locwuphy_en ? "on" : "off"),
			 (fwdphyrem_en ? "on" : "off")
	    );

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: sysfs_get_wakeup_cfg failed\n");
	return reg_val;

unsupported_phy_error:
	dev_err(&phydev->dev, "unsupported phy, sysfs_get_wakeup_cfg failed\n");
	return -1;
}

/* This function handles write accesses to the node 'wakeup_cfg' in sysfs
 * Depending on the hexadecimal value written, the bits
 * FWDPHYLOC, REMWUPHY, LOCWUPHY, FWDPHYREM are configured
 */
static ssize_t sysfs_set_wakeup_cfg(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int err, reg_val, reg_mask, wakeup_cfg;
	struct phy_device *phydev = container_of(dev, struct phy_device, dev);

	/* parse the buffer */
	err = kstrtoint(buf, 16, &wakeup_cfg);
	if (err < 0)
		goto phy_parse_error;

	if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P0 ||
	    (phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1102P1) {
		reg_val = 0;

		/* the first 4 bits of the supplied hexadecimal value
		 * are interpreted as the wakeup configuration
		 */
		if (wakeup_cfg & SYSFS_FWDPHYLOC)
			reg_val |= TJA1102_CFG1_FWDPHYLOC;
		if (wakeup_cfg & SYSFS_REMWUPHY)
			reg_val |= CFG1_REMWUPHY;
		if (wakeup_cfg & SYSFS_LOCWUPHY)
			reg_val |= CFG1_LOCWUPHY;
		if (wakeup_cfg & SYSFS_FWDPHYREM)
			reg_val |= CFG1_FWDPHYREM;

		reg_mask = (TJA1102_CFG1_FWDPHYLOC | CFG1_REMWUPHY |
			    CFG1_LOCWUPHY | CFG1_FWDPHYREM);

		err = phy_configure_bits(phydev, MII_CFG1, reg_mask, reg_val);
		if (err < 0)
			goto phy_configure_error;
	} else if ((phydev->phy_id & NXP_PHY_ID_MASK) == NXP_PHY_ID_TJA1100) {
		/* FWDPHYLOC MUST be off
		 * REMWUPHY MUST be on
		 * only LOCWUPHY and FWDPHYREM are configurable
		 * Possible configurations:
		 * - BOTH enabled (then led MUST be off)
		 * - BOTH disabled (then led CAN be on)
		 * all other configurations are invalid.
		 *
		 * Therefore valid values to write to sysfs are:
		 * - 2 (LOCWUPHY and FWDPHYREM off)
		 * - E (LOCWUPHY and FWDPHYREM on)
		 */
		if (((wakeup_cfg & SYSFS_LOCWUPHY) !=
		     (wakeup_cfg & SYSFS_FWDPHYREM)) ||
		    wakeup_cfg & SYSFS_FWDPHYLOC ||
		    !(wakeup_cfg & SYSFS_REMWUPHY)) {
			dev_alert(&phydev->dev, "Invalid configuration\n");
		} else if (wakeup_cfg & SYSFS_LOCWUPHY &&
			   wakeup_cfg & SYSFS_FWDPHYREM) {
			err = enter_led_mode(phydev, NO_LED_MODE);
			if (err)
				goto phy_lmode_transit_error;
		}
	}

	return count;

/* error handling */
phy_parse_error:
	dev_err(&phydev->dev, "parse error: sysfs_set_wakeup_cfg failed\n");
	return err;

phy_configure_error:
	dev_err(&phydev->dev, "phy r/w error: sysfs_set_wakeup_cfg failed\n");
	return err;

phy_lmode_transit_error:
	dev_err(&phydev->dev, "lmode error: sysfs_set_wakeup_cfg failed\n");
	return err;
}

/* r/w access for everyone */
static DEVICE_ATTR(master_cfg, S_IWUSR | S_IRUSR,
		   sysfs_get_master_cfg, sysfs_set_master_cfg);
static DEVICE_ATTR(power_cfg, S_IWUSR | S_IRUSR,
		   sysfs_get_power_cfg, sysfs_set_power_cfg);
static DEVICE_ATTR(loopback_cfg, S_IWUSR | S_IRUSR,
		   sysfs_get_loopback_cfg, sysfs_set_loopback_cfg);
static DEVICE_ATTR(cable_test, S_IRUSR, sysfs_get_cable_test, NULL);
static DEVICE_ATTR(test_mode, S_IWUSR | S_IRUSR,
		   sysfs_get_test_mode, sysfs_set_test_mode);
static DEVICE_ATTR(led_cfg, S_IWUSR | S_IRUSR,
		   sysfs_get_led_cfg, sysfs_set_led_cfg);
static DEVICE_ATTR(link_status, S_IRUSR, sysfs_get_link_status, NULL);
static DEVICE_ATTR(wakeup_cfg, S_IWUSR | S_IRUSR,
		   sysfs_get_wakeup_cfg, sysfs_set_wakeup_cfg);

static struct attribute *nxp_sysfs_entries[] = {
	&dev_attr_master_cfg.attr,
	&dev_attr_power_cfg.attr,
	&dev_attr_loopback_cfg.attr,
	&dev_attr_cable_test.attr,
	&dev_attr_test_mode.attr,
	&dev_attr_led_cfg.attr,
	&dev_attr_link_status.attr,
	&dev_attr_wakeup_cfg.attr,
	NULL
};

static struct attribute_group nxp_attribute_group = {
	.name = "configuration",
	.attrs = nxp_sysfs_entries,
};

/* helper function, configures a register of phydev
 *
 * The function sets the bit of register reg_name,
 * defined by bit_mask to 0 if (bit_value == 0), else to 1
 * @return	0 if configuration completed, <0 if read/write
 *		error occurred
 */
static inline int phy_configure_bit(struct phy_device *phydev, int reg_name,
				    int bit_mask, int bit_value)
{
	int reg_val, err;

	reg_val = phy_read(phydev, reg_name);
	if (reg_val < 0)
		goto phy_read_error;

	if (bit_value)
		reg_val |= bit_mask;
	else
		reg_val &= ~bit_mask;

	err = phy_write(phydev, reg_name, reg_val);
	if (err < 0)
		goto phy_write_error;

	return 0;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: phy config failed\n");
	return reg_val;

phy_write_error:
	dev_err(&phydev->dev, "write error: phy config failed\n");
	return err;
}

/* helper function, configures a register of phydev
 *
 * The function sets the bits of register reg_name,
 * defined by bit_mask to bit_value
 * @return	0 if configuration completed, <0 if read/write
 *		error occurred
 */
static inline int phy_configure_bits(struct phy_device *phydev, int reg_name,
				     int bit_mask, int bit_value)
{
	int reg_val, err;

	reg_val = phy_read(phydev, reg_name);
	if (reg_val < 0)
		goto phy_read_error;

	reg_val &= ~bit_mask;
	reg_val |= bit_value;

	err = phy_write(phydev, reg_name, reg_val);
	if (err < 0)
		goto phy_write_error;

	return 0;

/* error handling */
phy_read_error:
	dev_err(&phydev->dev, "read error: phy config failed\n");
	return reg_val;

phy_write_error:
	dev_err(&phydev->dev, "write error: phy config failed\n");
	return err;
}

static struct phy_driver nxp_drivers[] = {
	{
	 .phy_id = NXP_PHY_ID_TJA1100,
	 .name = "TJA1100",
	 .phy_id_mask = NXP_PHY_ID_MASK,
	 .features = (SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_100BASET1_FULL),
	 .flags = 0,
	 .probe = &nxp_probe,
	 .remove = &nxp_remove,
	 .config_init = &nxp_config_init,
	 .config_aneg = &nxp_config_aneg,
	 .read_status = &genphy_read_status,
	 .resume = &nxp_resume,
	 .suspend = &nxp_suspend,
	 .config_intr = &nxp_config_intr,
	 .ack_interrupt = &nxp_ack_interrupt,
	 .did_interrupt = &nxp_did_interrupt,
	 .driver = {.owner = THIS_MODULE},
	 },
	{
	 .phy_id = NXP_PHY_ID_TJA1102P0,
	 .name = "TJA1102_p0",
	 .phy_id_mask = NXP_PHY_ID_MASK,
	 .features = (SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_100BASET1_FULL),
	 .flags = 0,
	 .probe = &nxp_probe,
	 .remove = &nxp_remove,
	 .config_init = &nxp_config_init,
	 .config_aneg = &nxp_config_aneg,
	 .read_status = &genphy_read_status,
	 .resume = &nxp_resume,
	 .suspend = &nxp_suspend,
	 .config_intr = &nxp_config_intr,
	 .ack_interrupt = &nxp_ack_interrupt,
	 .did_interrupt = &nxp_did_interrupt,
	 .driver = {.owner = THIS_MODULE},
	 },
	{
	 .phy_id = NXP_PHY_ID_TJA1102S,
	 .name = "TJA1102S",
	 .phy_id_mask = NXP_PHY_ID_MASK,
	 .features = (SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_100BASET1_FULL),
	 .flags = 0,
	 .probe = &nxp_probe,
	 .remove = &nxp_remove,
	 .config_init = &nxp_config_init,
	 .config_aneg = &nxp_config_aneg,
	 .read_status = &genphy_read_status,
	 .resume = &nxp_resume,
	 .suspend = &nxp_suspend,
	 .config_intr = &nxp_config_intr,
	 .ack_interrupt = &nxp_ack_interrupt,
	 .did_interrupt = &nxp_did_interrupt,
	 .driver = {.owner = THIS_MODULE},
	 }
};

/* This driver is only registered if a TJA1102p0 is detected */
static struct phy_driver nxp_TJA1102p1_fixup_driver = {
	.phy_id = NXP_PHY_ID_TJA1102P1,
	.name = "TJA1102_p1",
	.phy_id_mask = NXP_PHY_ID_MASK,
	.features = (SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_100BASET1_FULL),
	.flags = 0,
	.probe = &nxp_probe,
	.remove = &nxp_remove,
	.config_init = &nxp_config_init,
	.config_aneg = &nxp_config_aneg,
	.read_status = &genphy_read_status,
	.resume = &nxp_resume,
	.suspend = &nxp_suspend,
	.config_intr = &nxp_config_intr,
	.ack_interrupt = &nxp_ack_interrupt,
	.did_interrupt = &nxp_did_interrupt,
	.driver = {.owner = THIS_MODULE},
};

/* Helper function: Search net devices for a specific phy with given
 * address.
 *
 * @return        pyhdev if found, NULL else
 */
static struct phy_device *search_phy_by_addr(int phy_addr)
{
	struct net_device *dev;
	struct phy_device *phydev;

	/* search the net devices for attached phy */
	dev = first_net_device(&init_net);
	while (dev) {
		if (dev->phydev) {
			/* check if there is a phy at given addr */
			phydev = dev->phydev->bus->phy_map[phy_addr];
			if (phydev)
				return phydev;
		}
		dev = next_net_device(dev);
	}
	return NULL;
}

/* Helper function: Search phy_map of mdio bus for a specific phy with
 * given id.
 *
 * @return        pyhdev if found, NULL else
 */
static struct phy_device *search_mdio_by_id(struct mii_bus *bus, int phy_id)
{
	int addr;
	struct phy_device *phydev;

	/* search the bus for a phy with the specified phy_id */
	for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
		if (bus->phy_map[addr]) {
			phydev = bus->phy_map[addr];
			if ((phydev->phy_id & NXP_PHY_ID_MASK) == phy_id) {
				pr_alert("found the given phy\n");

				return phydev;
			}
		}
	}
	return NULL;
}

/* Helper function: Search net devices for a specific phy with given id.
 *
 * @return        pyhdev if found, NULL else
 */
static struct phy_device *search_phy_by_id(int phy_id)
{
	struct net_device *dev;

	/* search the net devices for attached phy */
	dev = first_net_device(&init_net);
	while (dev) {
		if (dev->phydev)
			return search_mdio_by_id(dev->phydev->bus, phy_id);
		dev = next_net_device(dev);
	}
	return NULL;
}

/* Register the nxp_TJA1102p1_fixup_driver, if TJA1102p0 is present
 *
 * @return        0 if registration complete, errorcode on failure
 */
static int TJA1102p1_fixup_register(void)
{
	int err;
	struct device_driver *drv;
	struct phy_device *phydev;

	/* The ID of the TJA1102_p1 is always zero, however we cannot simply set
	 * the driver ID to zero  and leave binding to the framework.
	 * Instead, we check if a TJA1102_p0 is present.
	 * If so we can safely assume that there
	 * also is a TJA1102_p1 and dynamically load the driver for it.
	 */

	/* retrieve phydev */
	phydev = search_phy_by_id(NXP_PHY_ID_TJA1102P0);
	if (!phydev)
		goto phy_not_found;

	/* check if the fixup drv is already loaded */
	drv = driver_find("TJA1102_p1", phydev->dev.bus);
	if (drv) {
		dev_alert(&phydev->dev, "fixup driver already loaded\n");
	} else {
		err = phy_driver_register(&nxp_TJA1102p1_fixup_driver);
		if (err)
			goto drv_registration_error;

		pr_alert("Successfully registered fixup: %s\n",
			 nxp_TJA1102p1_fixup_driver.name);
	}

	return 0;

/* error handling */
drv_registration_error:
	return err;

phy_not_found:
	return 0;
}

/* Unregister the nxp_TJA1102p1_fixup_driver, if it is present */
static void unregister_TJA1102p1_fixup(void)
{
	struct device_driver *drv;
	struct phy_device *phydev;

	/* retrieve phydev */
	phydev = search_phy_by_id(NXP_PHY_ID_TJA1102P0);
	if (!phydev)
		return;

	/* check if the fixup drv was previously loaded */
	drv = driver_find("TJA1102_p1", phydev->dev.bus);
	if (drv) {
		dev_alert(&phydev->dev, "unloading fixup driver\n");
		phy_driver_unregister(&nxp_TJA1102p1_fixup_driver);
	}
}

/* module init function */
static int __init nxp_init(void)
{
	int err;

	pr_alert("loading NXP PHY driver: [%s]\n",
		 (managed_mode ? "managed mode" : "autonomous mode"));

	err = phy_drivers_register(nxp_drivers, ARRAY_SIZE(nxp_drivers));
	if (err)
		goto drv_registration_error;

	err = TJA1102p1_fixup_register();
	if (err)
		goto drv_registration_error;

	return 0;

/* error handling */
drv_registration_error:
	pr_err("NXP PHY: driver registration failed\n");
	return err;
}

module_init(nxp_init);

/* module exit function */
static void __exit nxp_exit(void)
{
	pr_alert("unloading NXP PHY driver\n");
	unregister_TJA1102p1_fixup();
	phy_drivers_unregister(nxp_drivers, ARRAY_SIZE(nxp_drivers));
}

module_exit(nxp_exit);

/* use module device table for hotplugging support */
static struct mdio_device_id __maybe_unused nxp_tbl[] = {
	{NXP_PHY_ID_TJA1100, NXP_PHY_ID_MASK},
	{NXP_PHY_ID_TJA1102P0, NXP_PHY_ID_MASK},
	{NXP_PHY_ID_TJA1102S, NXP_PHY_ID_MASK},
	{}
};

MODULE_DEVICE_TABLE(mdio, nxp_tbl);

MODULE_DESCRIPTION("NXP PHY driver");
MODULE_AUTHOR("Marco Hartmann");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
