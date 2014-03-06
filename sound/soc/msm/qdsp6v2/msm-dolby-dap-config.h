/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef _MSM_DOLBY_DAP_CONFIG_H_
#define _MSM_DOLBY_DAP_CONFIG_H_

#ifdef CONFIG_DOLBY_DAP
/* DOLBY DOLBY GUIDS */
#define DOLBY_ADM_COPP_TOPOLOGY_ID	0x0001033B
#define DOLBY_BUNDLE_MODULE_ID		0x00010723
#define DOLBY_VISUALIZER_MODULE_ID	0x0001072B

#define DOLBY_PARAM_ID_VDHE		0x0001074D
#define DOLBY_PARAM_ID_VSPE		0x00010750
#define DOLBY_PARAM_ID_DSSF		0x00010753
#define DOLBY_PARAM_ID_DVLI		0x0001073E
#define DOLBY_PARAM_ID_DVLO		0x0001073F
#define DOLBY_PARAM_ID_DVLE		0x0001073C
#define DOLBY_PARAM_ID_DVMC		0x00010741
#define DOLBY_PARAM_ID_DVME		0x00010740
#define DOLBY_PARAM_ID_IENB		0x00010744
#define DOLBY_PARAM_ID_IEBF		0x00010745
#define DOLBY_PARAM_ID_IEON		0x00010743
#define DOLBY_PARAM_ID_DEON		0x00010738
#define DOLBY_PARAM_ID_NGON		0x00010736
#define DOLBY_PARAM_ID_GEON		0x00010748
#define DOLBY_PARAM_ID_GENB		0x00010749
#define DOLBY_PARAM_ID_GEBF		0x0001074A
#define DOLBY_PARAM_ID_AONB		0x0001075B
#define DOLBY_PARAM_ID_AOBF		0x0001075C
#define DOLBY_PARAM_ID_AOBG		0x0001075D
#define DOLBY_PARAM_ID_AOON		0x00010759
#define DOLBY_PARAM_ID_ARNB		0x0001075F
#define DOLBY_PARAM_ID_ARBF		0x00010760
#define DOLBY_PARAM_ID_PLB		0x00010768
#define DOLBY_PARAM_ID_PLMD		0x00010767
#define DOLBY_PARAM_ID_DHSB		0x0001074E
#define DOLBY_PARAM_ID_DHRG		0x0001074F
#define DOLBY_PARAM_ID_DSSB		0x00010751
#define DOLBY_PARAM_ID_DSSA		0x00010752
#define DOLBY_PARAM_ID_DVLA		0x0001073D
#define DOLBY_PARAM_ID_IEBT		0x00010746
#define DOLBY_PARAM_ID_IEA		0x0001076A
#define DOLBY_PARAM_ID_DEA		0x00010739
#define DOLBY_PARAM_ID_DED		0x0001073A
#define DOLBY_PARAM_ID_GEBG		0x0001074B
#define DOLBY_PARAM_ID_AOCC		0x0001075A
#define DOLBY_PARAM_ID_ARBI		0x00010761
#define DOLBY_PARAM_ID_ARBL		0x00010762
#define DOLBY_PARAM_ID_ARBH		0x00010763
#define DOLBY_PARAM_ID_AROD		0x00010764
#define DOLBY_PARAM_ID_ARTP		0x00010765
#define DOLBY_PARAM_ID_VMON		0x00010756
#define DOLBY_PARAM_ID_VMB		0x00010757
#define DOLBY_PARAM_ID_VCNB		0x00010733
#define DOLBY_PARAM_ID_VCBF		0x00010734
#define DOLBY_PARAM_ID_PREG		0x00010728
#define DOLBY_PARAM_ID_VEN		0x00010732
#define DOLBY_PARAM_ID_PSTG		0x00010729
#define DOLBY_PARAM_ID_INIT_ENDP		0x00010727

/* Not Used with Set Param kcontrol, only to query using Get Param */
#define DOLBY_PARAM_ID_VER                0x00010726

#define DOLBY_PARAM_ID_VCBG		0x00010730
#define DOLBY_PARAM_ID_VCBE		0x00010731

/* DOLBY DAP control params */
#define DOLBY_COMMIT_ALL_TO_DSP		0x70000001
#define DOLBY_COMMIT_TO_DSP		0x70000002
#define DOLBY_USE_CACHE			0x70000003
#define DOLBY_AUTO_ENDP			0x70000004
#define DOLBY_AUTO_ENDDEP_PARAMS		0x70000005

/* DOLBY DAP offsets start */
#define DOLBY_PARAM_VDHE_LENGTH   1
#define DOLBY_PARAM_VDHE_OFFSET   0
#define DOLBY_PARAM_VSPE_LENGTH   1
#define DOLBY_PARAM_VSPE_OFFSET   (DOLBY_PARAM_VDHE_OFFSET + \
					DOLBY_PARAM_VDHE_LENGTH)
#define DOLBY_PARAM_DSSF_LENGTH   1
#define DOLBY_PARAM_DSSF_OFFSET   (DOLBY_PARAM_VSPE_OFFSET + \
					DOLBY_PARAM_VSPE_LENGTH)
#define DOLBY_PARAM_DVLI_LENGTH   1
#define DOLBY_PARAM_DVLI_OFFSET   (DOLBY_PARAM_DSSF_OFFSET + \
					DOLBY_PARAM_DSSF_LENGTH)
#define DOLBY_PARAM_DVLO_LENGTH   1
#define DOLBY_PARAM_DVLO_OFFSET   (DOLBY_PARAM_DVLI_OFFSET + \
					DOLBY_PARAM_DVLI_LENGTH)
#define DOLBY_PARAM_DVLE_LENGTH   1
#define DOLBY_PARAM_DVLE_OFFSET   (DOLBY_PARAM_DVLO_OFFSET + \
					DOLBY_PARAM_DVLO_LENGTH)
#define DOLBY_PARAM_DVMC_LENGTH   1
#define DOLBY_PARAM_DVMC_OFFSET   (DOLBY_PARAM_DVLE_OFFSET + \
					DOLBY_PARAM_DVLE_LENGTH)
#define DOLBY_PARAM_DVME_LENGTH   1
#define DOLBY_PARAM_DVME_OFFSET   (DOLBY_PARAM_DVMC_OFFSET + \
					DOLBY_PARAM_DVMC_LENGTH)
#define DOLBY_PARAM_IENB_LENGTH   1
#define DOLBY_PARAM_IENB_OFFSET   (DOLBY_PARAM_DVME_OFFSET + \
					DOLBY_PARAM_DVME_LENGTH)
#define DOLBY_PARAM_IEBF_LENGTH   40
#define DOLBY_PARAM_IEBF_OFFSET   (DOLBY_PARAM_IENB_OFFSET + \
					DOLBY_PARAM_IENB_LENGTH)
#define DOLBY_PARAM_IEON_LENGTH   1
#define DOLBY_PARAM_IEON_OFFSET   (DOLBY_PARAM_IEBF_OFFSET + \
					DOLBY_PARAM_IEBF_LENGTH)
#define DOLBY_PARAM_DEON_LENGTH   1
#define DOLBY_PARAM_DEON_OFFSET   (DOLBY_PARAM_IEON_OFFSET + \
					DOLBY_PARAM_IEON_LENGTH)
#define DOLBY_PARAM_NGON_LENGTH   1
#define DOLBY_PARAM_NGON_OFFSET   (DOLBY_PARAM_DEON_OFFSET + \
					DOLBY_PARAM_DEON_LENGTH)
#define DOLBY_PARAM_GEON_LENGTH   1
#define DOLBY_PARAM_GEON_OFFSET   (DOLBY_PARAM_NGON_OFFSET + \
					DOLBY_PARAM_NGON_LENGTH)
#define DOLBY_PARAM_GENB_LENGTH   1
#define DOLBY_PARAM_GENB_OFFSET   (DOLBY_PARAM_GEON_OFFSET + \
					DOLBY_PARAM_GEON_LENGTH)
#define DOLBY_PARAM_GEBF_LENGTH   40
#define DOLBY_PARAM_GEBF_OFFSET   (DOLBY_PARAM_GENB_OFFSET + \
					DOLBY_PARAM_GENB_LENGTH)
#define DOLBY_PARAM_AONB_LENGTH   1
#define DOLBY_PARAM_AONB_OFFSET   (DOLBY_PARAM_GEBF_OFFSET + \
					DOLBY_PARAM_GEBF_LENGTH)
#define DOLBY_PARAM_AOBF_LENGTH   40
#define DOLBY_PARAM_AOBF_OFFSET   (DOLBY_PARAM_AONB_OFFSET + \
					DOLBY_PARAM_AONB_LENGTH)
#define DOLBY_PARAM_AOBG_LENGTH   329
#define DOLBY_PARAM_AOBG_OFFSET   (DOLBY_PARAM_AOBF_OFFSET + \
					DOLBY_PARAM_AOBF_LENGTH)
#define DOLBY_PARAM_AOON_LENGTH   1
#define DOLBY_PARAM_AOON_OFFSET   (DOLBY_PARAM_AOBG_OFFSET + \
					DOLBY_PARAM_AOBG_LENGTH)
#define DOLBY_PARAM_ARNB_LENGTH   1
#define DOLBY_PARAM_ARNB_OFFSET   (DOLBY_PARAM_AOON_OFFSET + \
					DOLBY_PARAM_AOON_LENGTH)
#define DOLBY_PARAM_ARBF_LENGTH   40
#define DOLBY_PARAM_ARBF_OFFSET   (DOLBY_PARAM_ARNB_OFFSET + \
					DOLBY_PARAM_ARNB_LENGTH)
#define DOLBY_PARAM_PLB_LENGTH    1
#define DOLBY_PARAM_PLB_OFFSET    (DOLBY_PARAM_ARBF_OFFSET + \
					DOLBY_PARAM_ARBF_LENGTH)
#define DOLBY_PARAM_PLMD_LENGTH   1
#define DOLBY_PARAM_PLMD_OFFSET   (DOLBY_PARAM_PLB_OFFSET + \
					DOLBY_PARAM_PLB_LENGTH)
#define DOLBY_PARAM_DHSB_LENGTH   1
#define DOLBY_PARAM_DHSB_OFFSET   (DOLBY_PARAM_PLMD_OFFSET + \
					DOLBY_PARAM_PLMD_LENGTH)
#define DOLBY_PARAM_DHRG_LENGTH   1
#define DOLBY_PARAM_DHRG_OFFSET   (DOLBY_PARAM_DHSB_OFFSET + \
					DOLBY_PARAM_DHSB_LENGTH)
#define DOLBY_PARAM_DSSB_LENGTH   1
#define DOLBY_PARAM_DSSB_OFFSET   (DOLBY_PARAM_DHRG_OFFSET + \
					DOLBY_PARAM_DHRG_LENGTH)
#define DOLBY_PARAM_DSSA_LENGTH   1
#define DOLBY_PARAM_DSSA_OFFSET   (DOLBY_PARAM_DSSB_OFFSET + \
					DOLBY_PARAM_DSSB_LENGTH)
#define DOLBY_PARAM_DVLA_LENGTH   1
#define DOLBY_PARAM_DVLA_OFFSET   (DOLBY_PARAM_DSSA_OFFSET + \
					DOLBY_PARAM_DSSA_LENGTH)
#define DOLBY_PARAM_IEBT_LENGTH   40
#define DOLBY_PARAM_IEBT_OFFSET   (DOLBY_PARAM_DVLA_OFFSET + \
					DOLBY_PARAM_DVLA_LENGTH)
#define DOLBY_PARAM_IEA_LENGTH    1
#define DOLBY_PARAM_IEA_OFFSET    (DOLBY_PARAM_IEBT_OFFSET + \
					DOLBY_PARAM_IEBT_LENGTH)
#define DOLBY_PARAM_DEA_LENGTH    1
#define DOLBY_PARAM_DEA_OFFSET    (DOLBY_PARAM_IEA_OFFSET + \
					DOLBY_PARAM_IEA_LENGTH)
#define DOLBY_PARAM_DED_LENGTH    1
#define DOLBY_PARAM_DED_OFFSET    (DOLBY_PARAM_DEA_OFFSET + \
					DOLBY_PARAM_DEA_LENGTH)
#define DOLBY_PARAM_GEBG_LENGTH   40
#define DOLBY_PARAM_GEBG_OFFSET   (DOLBY_PARAM_DED_OFFSET + \
					DOLBY_PARAM_DED_LENGTH)
#define DOLBY_PARAM_AOCC_LENGTH   1
#define DOLBY_PARAM_AOCC_OFFSET   (DOLBY_PARAM_GEBG_OFFSET + \
					DOLBY_PARAM_GEBG_LENGTH)
#define DOLBY_PARAM_ARBI_LENGTH   40
#define DOLBY_PARAM_ARBI_OFFSET   (DOLBY_PARAM_AOCC_OFFSET + \
					DOLBY_PARAM_AOCC_LENGTH)
#define DOLBY_PARAM_ARBL_LENGTH   40
#define DOLBY_PARAM_ARBL_OFFSET   (DOLBY_PARAM_ARBI_OFFSET + \
					DOLBY_PARAM_ARBI_LENGTH)
#define DOLBY_PARAM_ARBH_LENGTH   40
#define DOLBY_PARAM_ARBH_OFFSET   (DOLBY_PARAM_ARBL_OFFSET + \
					DOLBY_PARAM_ARBL_LENGTH)
#define DOLBY_PARAM_AROD_LENGTH   1
#define DOLBY_PARAM_AROD_OFFSET   (DOLBY_PARAM_ARBH_OFFSET + \
					DOLBY_PARAM_ARBH_LENGTH)
#define DOLBY_PARAM_ARTP_LENGTH   1
#define DOLBY_PARAM_ARTP_OFFSET   (DOLBY_PARAM_AROD_OFFSET + \
					DOLBY_PARAM_AROD_LENGTH)
#define DOLBY_PARAM_VMON_LENGTH   1
#define DOLBY_PARAM_VMON_OFFSET   (DOLBY_PARAM_ARTP_OFFSET + \
					DOLBY_PARAM_ARTP_LENGTH)
#define DOLBY_PARAM_VMB_LENGTH    1
#define DOLBY_PARAM_VMB_OFFSET    (DOLBY_PARAM_VMON_OFFSET + \
					DOLBY_PARAM_VMON_LENGTH)
#define DOLBY_PARAM_VCNB_LENGTH   1
#define DOLBY_PARAM_VCNB_OFFSET   (DOLBY_PARAM_VMB_OFFSET + \
					DOLBY_PARAM_VMB_LENGTH)
#define DOLBY_PARAM_VCBF_LENGTH   20
#define DOLBY_PARAM_VCBF_OFFSET   (DOLBY_PARAM_VCNB_OFFSET + \
					DOLBY_PARAM_VCNB_LENGTH)
#define DOLBY_PARAM_PREG_LENGTH   1
#define DOLBY_PARAM_PREG_OFFSET   (DOLBY_PARAM_VCBF_OFFSET + \
					DOLBY_PARAM_VCBF_LENGTH)
#define DOLBY_PARAM_VEN_LENGTH    1
#define DOLBY_PARAM_VEN_OFFSET    (DOLBY_PARAM_PREG_OFFSET + \
					DOLBY_PARAM_PREG_LENGTH)
#define DOLBY_PARAM_PSTG_LENGTH   1
#define DOLBY_PARAM_PSTG_OFFSET   (DOLBY_PARAM_VEN_OFFSET + \
					DOLBY_PARAM_VEN_LENGTH)

#define DOLBY_PARAM_INT_ENDP_LENGTH		1
#define DOLBY_PARAM_PAYLOAD_SIZE		3
#define DOLBY_MAX_LENGTH_INDIVIDUAL_PARAM	329

#define DOLBY_NUM_ENDP_DEPENDENT_PARAMS	  3
#define DOLBY_ENDDEP_PARAM_DVLO_OFFSET	  0
#define DOLBY_ENDDEP_PARAM_DVLO_LENGTH	  1
#define DOLBY_ENDDEP_PARAM_DVLI_OFFSET    (DOLBY_ENDDEP_PARAM_DVLO_OFFSET + \
						DOLBY_ENDDEP_PARAM_DVLO_LENGTH)
#define DOLBY_ENDDEP_PARAM_DVLI_LENGTH    1
#define DOLBY_ENDDEP_PARAM_VMB_OFFSET     (DOLBY_ENDDEP_PARAM_DVLI_OFFSET + \
						DOLBY_ENDDEP_PARAM_DVLI_LENGTH)
#define DOLBY_ENDDEP_PARAM_VMB_LENGTH     1
#define DOLBY_ENDDEP_PARAM_LENGTH         (DOLBY_ENDDEP_PARAM_DVLO_LENGTH + \
		DOLBY_ENDDEP_PARAM_DVLI_LENGTH + DOLBY_ENDDEP_PARAM_VMB_LENGTH)

#define MAX_DOLBY_PARAMS			47
#define MAX_DOLBY_CTRL_PARAMS			5
#define ALL_DOLBY_PARAMS			(MAX_DOLBY_PARAMS + \
							MAX_DOLBY_CTRL_PARAMS)
#define DOLBY_COMMIT_ALL_IDX			MAX_DOLBY_PARAMS
#define DOLBY_COMMIT_IDX			(MAX_DOLBY_PARAMS+1)
#define DOLBY_USE_CACHE_IDX			(MAX_DOLBY_PARAMS+2)
#define DOLBY_AUTO_ENDP_IDX			(MAX_DOLBY_PARAMS+3)
#define DOLBY_AUTO_ENDDEP_IDX			(MAX_DOLBY_PARAMS+4)

#define TOTAL_LENGTH_DOLBY_PARAM		745
#define NUM_DOLBY_ENDP_DEVICE			24
#define DOLBY_VIS_PARAM_HEADER_SIZE		 25

#define DOLBY_INVALID_PORT_ID			-1
/* DOLBY device definitions */
enum {
	DOLBY_ENDP_INT_SPEAKERS = 0,
	DOLBY_ENDP_EXT_SPEAKERS,
	DOLBY_ENDP_HEADPHONES,
	DOLBY_ENDP_HDMI,
	DOLBY_ENDP_SPDIF,
	DOLBY_ENDP_DLNA,
	DOLBY_ENDP_ANALOG,
};

enum {
	DEVICE_NONE			= 0x0,
	/* output devices */
	EARPIECE			= 0x1,
	SPEAKER				= 0x2,
	WIRED_HEADSET			= 0x4,
	WIRED_HEADPHONE			= 0x8,
	BLUETOOTH_SCO			= 0x10,
	BLUETOOTH_SCO_HEADSET		= 0x20,
	BLUETOOTH_SCO_CARKIT		= 0x40,
	BLUETOOTH_A2DP			= 0x80,
	BLUETOOTH_A2DP_HEADPHONES	= 0x100,
	BLUETOOTH_A2DP_SPEAKER		= 0x200,
	AUX_DIGITAL			= 0x400,
	ANLG_DOCK_HEADSET		= 0x800,
	DGTL_DOCK_HEADSET		= 0x1000,
	USB_ACCESSORY			= 0x2000,
	USB_DEVICE			= 0x4000,
	REMOTE_SUBMIX			= 0x8000,
	ANC_HEADSET			= 0x10000,
	ANC_HEADPHONE			= 0x20000,
	PROXY				= 0x40000,
	FM				= 0x80000,
	FM_TX				= 0x100000,
	DEVICE_OUT_ALL			= 0x7FFFFFFF,
};
/* DOLBY device definitions end */

struct dolby_dap_params {
	uint32_t value[TOTAL_LENGTH_DOLBY_PARAM + MAX_DOLBY_PARAMS];
} __packed;
int dolby_dap_init(int port_id, int channels);
int msm_routing_get_dolby_dap_param_to_set_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_put_dolby_dap_param_to_set_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_get_dolby_dap_param_to_get_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_put_dolby_dap_param_to_get_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_get_dolby_dap_param_visualizer_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_put_dolby_dap_param_visualizer_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_get_dolby_dap_endpoint_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
int msm_routing_put_dolby_dap_endpoint_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol);
void dolby_dap_deinit(int port_id);
/* Dolby DOLBY end */
#else
int dolby_dap_init(int port_id, int channels) { return 0; }
int msm_routing_get_dolby_dap_param_to_set_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_put_dolby_dap_param_to_set_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_get_dolby_dap_param_to_get_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_put_dolby_dap_param_to_get_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_get_dolby_dap_param_visualizer_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_put_dolby_dap_param_visualizer_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_get_dolby_dap_endpoint_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
int msm_routing_put_dolby_dap_endpoint_control(
			struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol) { return 0; }
void dolby_dap_deinit(int port_id) { return; }
#endif

#endif

