/* BEGIN PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
//add Touch driver for G610-T11
/* BEGIN PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* BEGIN PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* BEGIN PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/*
 * cyttsp4_mt_common.c
 * Cypress TrueTouch(TM) Standard Product V4 Multi-touch module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp4_mt_common.h"

static void cyttsp4_lift_all(struct cyttsp4_mt_data *md)
{
  /* BEGIN PN:DTS2013041400018 ,Added by l00184147, 2013/4/12*/
  if (!md->si)
	return;
  /* END PN:DTS2013041400018 ,Added by l00184147, 2013/4/12*/
  
  if (md->num_prv_tch != 0) {
	if (md->mt_function.report_slot_liftoff)
	  md->mt_function.report_slot_liftoff(md);
	/* ICS Lift off button release signal and empty mt */
	if (md->prv_tch_type != CY_OBJ_HOVER)
	  input_report_key(md->input, BTN_TOUCH, CY_BTN_RELEASED);
	input_sync(md->input);
	md->num_prv_tch = 0;
  }
}

static void cyttsp4_get_touch_axis(struct cyttsp4_mt_data *md,
								   int *axis, int size, int max, u8 *xy_data, int bofs)
{
  int nbyte;
  int next;

  for (nbyte = 0, *axis = 0, next = 0; nbyte < size; nbyte++) {
	dev_vdbg(&md->ttsp->dev,
			 "%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
			 " xy_data[%d]=%02X(%d) bofs=%d\n",
			 __func__, *axis, *axis, size, max, xy_data, next,
			 xy_data[next], xy_data[next], bofs);
	*axis = (*axis * 256) + (xy_data[next] >> bofs);
	next++;
  }

  *axis &= max - 1;

  dev_vdbg(&md->ttsp->dev,
		   "%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
		   " xy_data[%d]=%02X(%d)\n",
		   __func__, *axis, *axis, size, max, xy_data, next,
		   xy_data[next], xy_data[next]);
}

static void cyttsp4_get_touch(struct cyttsp4_mt_data *md,
							  struct cyttsp4_touch *touch, u8 *xy_data)
{
  struct device *dev = &md->ttsp->dev;
  struct cyttsp4_sysinfo *si = md->si;
  enum cyttsp4_tch_abs abs;
  int tmp;
  bool flipped;

  for (abs = CY_TCH_X; abs < CY_TCH_NUM_ABS; abs++) {
	cyttsp4_get_touch_axis(md, &touch->abs[abs],
						   si->si_ofs.tch_abs[abs].size,
						   si->si_ofs.tch_abs[abs].max,
						   xy_data + si->si_ofs.tch_abs[abs].ofs,
						   si->si_ofs.tch_abs[abs].bofs);
	dev_vdbg(dev, "%s: get %s=%04X(%d)\n", __func__,
			 cyttsp4_tch_abs_string[abs],
			 touch->abs[abs], touch->abs[abs]);
  }

  if (md->pdata->flags & CY_FLAG_FLIP) {
	tmp = touch->abs[CY_TCH_X];
	touch->abs[CY_TCH_X] = touch->abs[CY_TCH_Y];
	touch->abs[CY_TCH_Y] = tmp;
	flipped = true;
  } else
	flipped = false;

  if (md->pdata->flags & CY_FLAG_INV_X) {
	if (flipped)
	  touch->abs[CY_TCH_X] = md->si->si_ofs.max_y -
		touch->abs[CY_TCH_X];
	else
	  touch->abs[CY_TCH_X] = md->si->si_ofs.max_x -
		touch->abs[CY_TCH_X];
  }
  if (md->pdata->flags & CY_FLAG_INV_Y) {
	if (flipped)
	  touch->abs[CY_TCH_Y] = md->si->si_ofs.max_x -
		touch->abs[CY_TCH_Y];
	else
	  touch->abs[CY_TCH_Y] = md->si->si_ofs.max_y -
		touch->abs[CY_TCH_Y];
  }

  dev_vdbg(dev, "%s: flip=%s inv-x=%s inv-y=%s x=%04X(%d) y=%04X(%d)\n",
		   __func__, flipped ? "true" : "false",
		   md->pdata->flags & CY_FLAG_INV_X ? "true" : "false",
		   md->pdata->flags & CY_FLAG_INV_Y ? "true" : "false",
		   touch->abs[CY_TCH_X], touch->abs[CY_TCH_X],
		   touch->abs[CY_TCH_Y], touch->abs[CY_TCH_Y]);
}

static void cyttsp4_get_mt_touches(struct cyttsp4_mt_data *md, int num_cur_tch)
{
  struct device *dev = &md->ttsp->dev;
  struct cyttsp4_sysinfo *si = md->si;
  struct cyttsp4_touch tch;
  int sig;
  int i, j, t = 0;
  int ids[max(CY_TMA1036_MAX_TCH + 1,
			  CY_TMA4XX_MAX_TCH + 1)]; /* add one for hover */
  int mt_sync_count = 0;

  memset(ids, 0, (si->si_ofs.max_tchs + 1) * sizeof(int));
  memset(&tch, 0, sizeof(struct cyttsp4_touch));
  for (i = 0; i < num_cur_tch; i++) {
	cyttsp4_get_touch(md, &tch, si->xy_data +
					  (i * si->si_ofs.tch_rec_size));
	if ((tch.abs[CY_TCH_T] < md->pdata->frmwrk->abs
		 [(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MIN_OST]) ||
		(tch.abs[CY_TCH_T] > md->pdata->frmwrk->abs
		 [(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MAX_OST])) {
	  dev_err(dev, "%s: tch=%d -> bad trk_id=%d max_id=%d\n",
			  __func__, i, tch.abs[CY_TCH_T],
			  md->pdata->frmwrk->abs[(CY_ABS_ID_OST *
									  CY_NUM_ABS_SET) + CY_MAX_OST]);
	  if (md->mt_function.input_sync)
		md->mt_function.input_sync(md->input);
	  mt_sync_count++;
	  continue;
	}

	/*
	 * if any touch is hover, then there is only one touch
	 * so it is OK to check the first touch for hover condition
	 */
	if ((md->num_prv_tch == 0 && tch.abs[CY_TCH_O] != CY_OBJ_HOVER)
		|| (md->prv_tch_type == CY_OBJ_HOVER
			&& tch.abs[CY_TCH_O] != CY_OBJ_HOVER))
	  input_report_key(md->input, BTN_TOUCH, CY_BTN_PRESSED);

	/* use 0 based track id's */
	sig = md->pdata->frmwrk->abs
	  [(CY_ABS_ID_OST * CY_NUM_ABS_SET) + 0];
	if (sig != CY_IGNORE_VALUE) {
	  t = tch.abs[CY_TCH_T] - md->pdata->frmwrk->abs
		[(CY_ABS_ID_OST * CY_NUM_ABS_SET) + CY_MIN_OST];
	  if (tch.abs[CY_TCH_E] == CY_EV_LIFTOFF) {
		dev_dbg(dev, "%s: t=%d e=%d lift-off\n",
				__func__, t, tch.abs[CY_TCH_E]);
		goto cyttsp4_get_mt_touches_pr_tch;
	  }
	  if (md->mt_function.input_report)
		md->mt_function.input_report(md->input, sig, t);
	  ids[t] = true;
	}

	/* Check if hover on this touch */
	dev_vdbg(dev, "%s: t=%d z=%d\n", __func__, t,
			 tch.abs[CY_TCH_P]);
	if (t == CY_ACTIVE_STYLUS_ID) {
	  tch.abs[CY_TCH_P] = 0;
	  dev_dbg(dev, "%s: t=%d z=%d force zero\n", __func__, t,
			  tch.abs[CY_TCH_P]);
	}

	/* all devices: position and pressure fields */
	for (j = 0; j <= CY_ABS_W_OST ; j++) {
	  sig = md->pdata->frmwrk->abs[((CY_ABS_X_OST + j) *
									CY_NUM_ABS_SET) + 0];
	  if (sig != CY_IGNORE_VALUE)
		input_report_abs(md->input, sig,
						 tch.abs[CY_TCH_X + j]);
	}
	if (si->si_ofs.tch_rec_size > CY_TMA1036_TCH_REC_SIZE) {
	  /*
	   * TMA400 size and orientation fields:
	   * if pressure is non-zero and major touch
	   * signal is zero, then set major and minor touch
	   * signals to minimum non-zero value
	   */
	  if (tch.abs[CY_TCH_P] > 0 && tch.abs[CY_TCH_MAJ] == 0)
		tch.abs[CY_TCH_MAJ] = tch.abs[CY_TCH_MIN] = 1;

	  /* Get the extended touch fields */
	  for (j = 0; j < CY_NUM_EXT_TCH_FIELDS; j++) {
		sig = md->pdata->frmwrk->abs
		  [((CY_ABS_MAJ_OST + j) *
			CY_NUM_ABS_SET) + 0];
		if (sig != CY_IGNORE_VALUE)
		  input_report_abs(md->input, sig,
						   tch.abs[CY_TCH_MAJ + j]);
	  }
	}
	if (md->mt_function.input_sync)
	  md->mt_function.input_sync(md->input);
	mt_sync_count++;

  cyttsp4_get_mt_touches_pr_tch:
	if (si->si_ofs.tch_rec_size > CY_TMA1036_TCH_REC_SIZE)
	  dev_dbg(dev,
			  "%s: t=%d x=%d y=%d z=%d M=%d m=%d o=%d e=%d\n",
			  __func__, t,
			  tch.abs[CY_TCH_X],
			  tch.abs[CY_TCH_Y],
			  tch.abs[CY_TCH_P],
			  tch.abs[CY_TCH_MAJ],
			  tch.abs[CY_TCH_MIN],
			  tch.abs[CY_TCH_OR],
			  tch.abs[CY_TCH_E]);
	else
	  dev_dbg(dev,
			  "%s: t=%d x=%d y=%d z=%d e=%d\n", __func__,
			  t,
			  tch.abs[CY_TCH_X],
			  tch.abs[CY_TCH_Y],
			  tch.abs[CY_TCH_P],
			  tch.abs[CY_TCH_E]);
  }

  if (md->mt_function.final_sync)
	md->mt_function.final_sync(md->input, si->si_ofs.max_tchs,
							   mt_sync_count, ids);

  md->num_prv_tch = num_cur_tch;
  md->prv_tch_type = tch.abs[CY_TCH_O];

  return;
}

/* read xy_data for all current touches */
static int cyttsp4_xy_worker(struct cyttsp4_mt_data *md)
{
  struct device *dev = &md->ttsp->dev;
  struct cyttsp4_sysinfo *si = md->si;
  u8 num_cur_tch;
  u8 hst_mode;
  u8 rep_len;
  u8 rep_stat;
  u8 tt_stat;
  int rc = 0;

  /*
   * Get event data from cyttsp4 device.
   * The event data includes all data
   * for all active touches.
   * Event data also includes button data
   */
  /*
   * Use 2 reads:
   * 1st read to get mode + button bytes + touch count (core)
   * 2nd read (optional) to get touch 1 - touch n data
   */
  hst_mode = si->xy_mode[CY_REG_BASE];
  rep_len = si->xy_mode[si->si_ofs.rep_ofs];
  rep_stat = si->xy_mode[si->si_ofs.rep_ofs + 1];
  tt_stat = si->xy_mode[si->si_ofs.tt_stat_ofs];
  dev_vdbg(dev, "%s: %s%02X %s%d %s%02X %s%02X\n", __func__,
		   "hst_mode=", hst_mode, "rep_len=", rep_len,
		   "rep_stat=", rep_stat, "tt_stat=", tt_stat);

  num_cur_tch = GET_NUM_TOUCHES(tt_stat);
  dev_vdbg(dev, "%s: num_cur_tch=%d\n", __func__, num_cur_tch);

  if (rep_len == 0 && num_cur_tch > 0) {
	dev_err(dev, "%s: report length error rep_len=%d num_tch=%d\n",
			__func__, rep_len, num_cur_tch);
	goto cyttsp4_xy_worker_exit;
  }

  /* read touches */
  if (num_cur_tch > 0) {
	rc = cyttsp4_read(md->ttsp, CY_MODE_OPERATIONAL,
					  si->si_ofs.tt_stat_ofs + 1, si->xy_data,
					  num_cur_tch * si->si_ofs.tch_rec_size);
	if (rc < 0) {
	  dev_err(dev, "%s: read fail on touch regs r=%d\n",
			  __func__, rc);
	  goto cyttsp4_xy_worker_exit;
	}
  }

  /* print xy data */
  cyttsp4_pr_buf(dev, md->pr_buf, si->xy_data, num_cur_tch *
				 si->si_ofs.tch_rec_size, "xy_data");

#ifdef SHOK_SENSOR_DATA_MODE
  if (si->monitor.mntr_status == CY_MNTR_STARTED) {
	int offset = (si->si_ofs.max_tchs * si->si_ofs.tch_rec_size)
	  + (si->si_ofs.num_btns
		 * si->si_ofs.btn_rec_size)
	  + (si->si_ofs.tt_stat_ofs + 1);
	rc = cyttsp4_read(md->ttsp, CY_MODE_OPERATIONAL,
					  offset, &(si->monitor.sensor_data[0]), 150);
	if (rc < 0) {
	  dev_err(dev, "%s: %s r=%d\n", __func__,
			  "read fail on sensor monitor regs",
			  rc);
	  goto cyttsp4_xy_worker_exit;
	}
	cyttsp4_pr_buf(dev, md->pr_buf, si->monitor.sensor_data,
				   150, "sensor_monitor");
  }
#endif
  /* check any error conditions */
  if (IS_BAD_PKT(rep_stat)) {
	dev_dbg(dev, "%s: Invalid buffer detected\n", __func__);
	rc = 0;
	goto cyttsp4_xy_worker_exit;
  }

	if (IS_LARGE_AREA(tt_stat)) {
		dev_dbg(dev, "%s: Large area detected\n", __func__);
		if (!(md->pdata->flags & CY_FLAG_REPORT_ON_LO))
			num_cur_tch = 0;
	}

  if (num_cur_tch > si->si_ofs.max_tchs) {
	if (num_cur_tch > max(CY_TMA1036_MAX_TCH, CY_TMA4XX_MAX_TCH)) {
	  /* terminate all active tracks */
	  dev_err(dev, "%s: Num touch err detected (n=%d)\n",
			  __func__, num_cur_tch);
	  num_cur_tch = 0;
	} else {
	  dev_err(dev, "%s: %s (n=%d c=%d)\n", __func__,
			  "too many tch; set to max tch",
			  num_cur_tch, si->si_ofs.max_tchs);
	  num_cur_tch = si->si_ofs.max_tchs;
	}
  }

  /* extract xy_data for all currently reported touches */
  dev_vdbg(dev, "%s: extract data num_cur_tch=%d\n", __func__,
		   num_cur_tch);
  if (num_cur_tch)
	cyttsp4_get_mt_touches(md, num_cur_tch);
  else
	cyttsp4_lift_all(md);

  dev_vdbg(dev, "%s: done\n", __func__);
  rc = 0;

 cyttsp4_xy_worker_exit:
  return rc;
}

static int cyttsp4_mt_attention(struct cyttsp4_device *ttsp)
{
  struct device *dev = &ttsp->dev;
  struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
  int rc = 0;

  dev_vdbg(dev, "%s\n", __func__);

  /* BEGIN PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/
  mutex_lock(&md->report_lock);
  if (!md->is_suspended) {
  /* core handles handshake */
  rc = cyttsp4_xy_worker(md);
  } else {
	dev_vdbg(dev, "%s: Ignoring report while suspended\n",
			__func__);
  }
  mutex_unlock(&md->report_lock);
  /* END PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/

  if (rc < 0)
	dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

  return rc;
}

static int cyttsp4_startup_attention(struct cyttsp4_device *ttsp)
{
  struct device *dev = &ttsp->dev;
  struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
  int rc = 0;

  dev_vdbg(dev, "%s\n", __func__);

  /* BEGIN PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/
  mutex_lock(&md->report_lock);
  cyttsp4_lift_all(md);
  mutex_unlock(&md->report_lock);
  /* END PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/  
  
  return rc;
}

static int cyttsp4_mt_open(struct input_dev *input)
{
  struct device *dev = input->dev.parent;
  struct cyttsp4_device *ttsp =
	container_of(dev, struct cyttsp4_device, dev);

  dev_dbg(dev, "%s\n", __func__);

	pm_runtime_get(dev);

  dev_vdbg(dev, "%s: setup subscriptions\n", __func__);

  /* set up touch call back */
  cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
							  cyttsp4_mt_attention, CY_MODE_OPERATIONAL);

  /* set up startup call back */
  cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
							  cyttsp4_startup_attention, 0);

  return 0;
}

static void cyttsp4_mt_close(struct input_dev *input)
{
  struct device *dev = input->dev.parent;
  /* BEGIN PN:DTS2013041400018 ,Deleted by l00184147, 2013/4/12*/
  /* END PN:DTS2013041400018 ,Deleted by l00184147, 2013/4/12*/
  struct cyttsp4_device *ttsp =
	container_of(dev, struct cyttsp4_device, dev);

  dev_dbg(dev, "%s\n", __func__);
  /* BEGIN PN:DTS2013041400018 ,Deleted by l00184147, 2013/4/12*/
  /* END PN:DTS2013041400018 ,Deleted by l00184147, 2013/4/12*/
  cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
								cyttsp4_mt_attention, CY_MODE_OPERATIONAL);

  cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
								cyttsp4_startup_attention, 0);

  pm_runtime_put(dev);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp4_mt_early_suspend(struct early_suspend *h)
{
  struct cyttsp4_mt_data *md =
	container_of(h, struct cyttsp4_mt_data, es);
  struct device *dev = &md->ttsp->dev;
  /* BEGIN PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/
  dev_dbg(dev, "%s\n", __func__);
  
  if (md->si){
	mutex_lock(&md->report_lock);
	md->is_suspended = true;
	cyttsp4_lift_all(md);
	mutex_unlock(&md->report_lock);
  }
  /* END PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/
}

static void cyttsp4_mt_late_resume(struct early_suspend *h)
{
  struct cyttsp4_mt_data *md =
	container_of(h, struct cyttsp4_mt_data, es);
  struct device *dev = &md->ttsp->dev;

  dev_dbg(dev, "%s\n", __func__);
  /* BEGIN PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/
  mutex_lock(&md->report_lock);
  md->is_suspended = false;
  mutex_unlock(&md->report_lock);
  /* END PN:DTS2013041400018 ,Modified by l00184147, 2013/4/12*/
}

void cyttsp4_setup_early_suspend(struct cyttsp4_mt_data *md)
{
  md->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
  md->es.suspend = cyttsp4_mt_early_suspend;
  md->es.resume = cyttsp4_mt_late_resume;

  register_early_suspend(&md->es);
}
#endif

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM_RUNTIME)
static int cyttsp4_mt_suspend(struct device *dev)
{
  struct cyttsp4_mt_data *md = dev_get_drvdata(dev);

  dev_dbg(dev, "%s\n", __func__);

	if (md->si)
		cyttsp4_lift_all(md);
	return 0;
}

static int cyttsp4_mt_resume(struct device *dev)
{
  dev_dbg(dev, "%s\n", __func__);

  return 0;
}
#endif

/* BEGIN PN:SPBB-1257 ,Deteled by l00184147, 2013/2/21*/
//Don't use the pm operation with PM sleep and runtime sleep
//const struct dev_pm_ops cyttsp4_mt_pm_ops = {
//  SET_SYSTEM_SLEEP_PM_OPS(cyttsp4_mt_suspend, cyttsp4_mt_resume)
//  SET_RUNTIME_PM_OPS(cyttsp4_mt_suspend, cyttsp4_mt_resume, NULL)
//};
/* END PN:SPBB-1257 ,Deteled by l00184147, 2013/2/21*/

static int cyttsp4_setup_input_device(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
	int signal = CY_IGNORE_VALUE;
	int max_x, max_y, max_p, min, max;
	int max_x_tmp, max_y_tmp;
	int i;
	int rc;

	dev_vdbg(dev, "%s: Initialize event signals\n", __func__);
	__set_bit(EV_ABS, md->input->evbit);
	__set_bit(EV_REL, md->input->evbit);
	__set_bit(INPUT_PROP_DIRECT,md->input->propbit); /*Add by huawei to choose direct mode for JellyBean*/
	__set_bit(EV_KEY, md->input->evbit);
	bitmap_fill(md->input->absbit, ABS_MAX);
	__set_bit(BTN_TOUCH, md->input->keybit);

  /* If virtualkeys enabled, don't use all screen */
  if (md->pdata->flags & CY_FLAG_VKEYS) {
	max_x_tmp = CY_VKEYS_X;
	max_y_tmp = CY_VKEYS_Y;
  } else {
      /* BEGIN PN:DTS2013031401505  ,Modified by F00184246, 2013/3/14*/ 
	max_x_tmp = CY_G610_NOVKEYS_X;
	max_y_tmp = CY_G610_NOVKEYS_Y;
     /* END PN:DTS2013031401505  ,Modified by F00184246, 2013/3/14*/ 
  }

  /* get maximum values from the sysinfo data */
  if (md->pdata->flags & CY_FLAG_FLIP) {
	max_x = max_y_tmp - 1;
	max_y = max_x_tmp - 1;
  } else {
	max_x = max_x_tmp - 1;
	max_y = max_y_tmp - 1;
  }
  max_p = md->si->si_ofs.max_p;

  /* set event signal capabilities */
  for (i = 0; i < (md->pdata->frmwrk->size / CY_NUM_ABS_SET); i++) {
	signal = md->pdata->frmwrk->abs
	  [(i * CY_NUM_ABS_SET) + CY_SIGNAL_OST];
	if (signal != CY_IGNORE_VALUE) {
	  min = md->pdata->frmwrk->abs
		[(i * CY_NUM_ABS_SET) + CY_MIN_OST];
	  max = md->pdata->frmwrk->abs
		[(i * CY_NUM_ABS_SET) + CY_MAX_OST];
	  if (i == CY_ABS_ID_OST) {
		/* shift track ids down to start at 0 */
		max = max - min;
		min = min - min;
	  } else if (i == CY_ABS_X_OST)
		max = max_x;
	  else if (i == CY_ABS_Y_OST)
		max = max_y;
	  else if (i == CY_ABS_P_OST)
		max = max_p;
	  input_set_abs_params(md->input, signal, min, max,
						   md->pdata->frmwrk->abs
						   [(i * CY_NUM_ABS_SET) + CY_FUZZ_OST],
						   md->pdata->frmwrk->abs
						   [(i * CY_NUM_ABS_SET) + CY_FLAT_OST]);
	  dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
			  __func__, signal, min, max);
	  if ((i == CY_ABS_ID_OST) &&
		  (md->si->si_ofs.tch_rec_size <
		   CY_TMA4XX_TCH_REC_SIZE))
		break;
	}
  }

	rc = md->mt_function.input_register_device(md->input,
			md->si->si_ofs.max_tchs);
	if (rc < 0)
		dev_err(dev, "%s: Error, failed register input device r=%d\n",
			__func__, rc);
	else
		md->input_device_registered = true;

	return rc;
}

static int cyttsp4_setup_input_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);
	int rc = 0;

	dev_vdbg(dev, "%s\n", __func__);

	md->si = cyttsp4_request_sysinfo(ttsp);
	if (!md->si)
		return -1;

	rc = cyttsp4_setup_input_device(ttsp);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
		cyttsp4_setup_input_attention, 0);

	return rc;
}

int cyttsp4_mt_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*
	 * This check is to prevent pm_runtime usage_count drop below zero
	 * because of removing the module while in suspended state
	 */
	if (md->is_suspended)
		pm_runtime_get_noresume(dev);

	unregister_early_suspend(&md->es);
#endif

	if (md->input_device_registered) {
		input_unregister_device(md->input);
	} else {
		input_free_device(md->input);
		cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp4_setup_input_attention, 0);
	}

	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);

	dev_set_drvdata(dev, NULL);
	kfree(md);
	return 0;
}

static int cyttsp4_mt_probe(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_mt_data *md;
	struct cyttsp4_mt_platform_data *pdata = dev_get_platdata(dev);
	int rc = 0;

	dev_info(dev, "%s\n", __func__);
	dev_dbg(dev, "%s: debug on\n", __func__);
	dev_vdbg(dev, "%s: verbose debug on\n", __func__);

	if (pdata == NULL) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}

	md = kzalloc(sizeof(*md), GFP_KERNEL);
	if (md == NULL) {
		dev_err(dev, "%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}

	cyttsp4_init_function_ptrs(md);

	/* BEGIN PN:DTS2013041400018 ,Added by l00184147, 2013/4/12*/
	mutex_init(&md->report_lock);
	/* END PN:DTS2013041400018 ,Added by l00184147, 2013/4/12*/

	md->prv_tch_type = CY_OBJ_STANDARD_FINGER;
	md->ttsp = ttsp;
	md->pdata = pdata;
	dev_set_drvdata(dev, md);
	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	md->input = input_allocate_device();
	if (md->input == NULL) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	md->input->name = ttsp->name;
	scnprintf(md->phys, sizeof(md->phys)-1, "%s", dev_name(dev));
	md->input->phys = md->phys;
	md->input->dev.parent = &md->ttsp->dev;
	md->input->open = cyttsp4_mt_open;
	md->input->close = cyttsp4_mt_close;
	input_set_drvdata(md->input, md);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	/* get sysinfo */
	md->si = cyttsp4_request_sysinfo(ttsp);
	pm_runtime_put(dev);

	if (md->si) {
		rc = cyttsp4_setup_input_device(ttsp);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, md->si);
		cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
			cyttsp4_setup_input_attention, 0);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
  cyttsp4_setup_early_suspend(md);
#endif
  dev_dbg(dev, "%s: OK\n", __func__);
  return 0;

error_init_input:
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
	input_free_device(md->input);
error_alloc_failed:
	dev_set_drvdata(dev, NULL);
	kfree(md);
error_alloc_data_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);
	return rc;
}

struct cyttsp4_driver cyttsp4_mt_driver = {
  .probe = cyttsp4_mt_probe,
  .remove = cyttsp4_mt_release,
  .driver = {
	.name = CYTTSP4_MT_NAME,
	.bus = &cyttsp4_bus_type,
	/* BEGIN PN:SPBB-1257 ,Deteled by l00184147, 2013/2/21*/
	//no longer to use pm operation
	//.pm = &cyttsp4_mt_pm_ops,
	/* END PN:SPBB-1257 ,Deteled by l00184147, 2013/2/21*/
  },
};
/* END PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/* END PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* END PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* END PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
