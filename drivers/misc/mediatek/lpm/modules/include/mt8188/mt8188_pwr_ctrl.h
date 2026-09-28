/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MT8188_PWR_CTRL_H__
#define __MT8188_PWR_CTRL_H__

/* SPM_WAKEUP_MISC */
#define WAKE_MISC_GIC_WAKEUP			0x3FF	/* bit0 ~ bit9 */
#define WAKE_MISC_DVFSRC_IRQ			(1U << 16)
#define WAKE_MISC_REG_CPU_WAKEUP		(1U << 17)
#define WAKE_MISC_PCM_TIMER_EVENT		(1U << 18)
#define WAKE_MISC_PMIC_OUT_B			((1U << 19) | (1U << 20))
#define WAKE_MISC_TWAM_IRQ_B			(1U << 21)
#define WAKE_MISC_PMSR_IRQ_B_SET0		(1U << 22)
#define WAKE_MISC_PMSR_IRQ_B_SET1		(1U << 23)
#define WAKE_MISC_PMSR_IRQ_B_SET2		(1U << 24)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_0		(1U << 25)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_1		(1U << 26)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_2		(1U << 27)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_3		(1U << 28)
#define WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL	(1U << 29)
#define WAKE_MISC_PMIC_IRQ_ACK			(1U << 30)
#define WAKE_MISC_PMIC_SCP_IRQ			(1U << 31)

struct pwr_ctrl {
	/* for SPM */
	uint32_t pcm_flags;
	/* can override pcm_flags */
	uint32_t pcm_flags_cust;
	/* set bit of pcm_flags, after pcm_flags_cust */
	uint32_t pcm_flags_cust_set;
	/* clr bit of pcm_flags, after pcm_flags_cust */
	uint32_t pcm_flags_cust_clr;
	uint32_t pcm_flags1;
	/* can override pcm_flags1 */
	uint32_t pcm_flags1_cust;
	/* set bit of pcm_flags1, after pcm_flags1_cust */
	uint32_t pcm_flags1_cust_set;
	/* clr bit of pcm_flags1, after pcm_flags1_cust */
	uint32_t pcm_flags1_cust_clr;
	/* @ 1T 32K */
	uint32_t timer_val;
	/* @ 1T 32K, can override timer_val */
	uint32_t timer_val_cust;
	/* stress for dpidle */
	uint32_t timer_val_ramp_en;
	/* stress for suspend */
	uint32_t timer_val_ramp_en_sec;
	uint32_t wake_src;
	/* can override wake_src */
	uint32_t wake_src_cust;
	/* disable wdt in suspend */
	uint8_t wdt_disable;

	/* SPM_AP_STANDBY_CON */
	/* [0] */
	uint8_t reg_wfi_op;
	/* [1] */
	uint8_t reg_wfi_type;
	/* [2] */
	uint8_t reg_mp0_cputop_idle_mask;
	/* [3] */
	uint8_t reg_mp1_cputop_idle_mask;
	/* [4] */
	uint8_t reg_mcusys_idle_mask;
	/* [25] */
	uint8_t reg_md_apsrc_1_sel;
	/* [26] */
	uint8_t reg_md_apsrc_0_sel;
	/* [29] */
	uint8_t reg_conn_apsrc_sel;

	/* SPM_SRC_REQ */
	/* [0] */
	uint8_t reg_spm_apsrc_req;
	/* [1] */
	uint8_t reg_spm_f26m_req;
	/* [3] */
	uint8_t reg_spm_infra_req;
	/* [4] */
	uint8_t reg_spm_vrf18_req;
	/* [7] */
	uint8_t reg_spm_ddr_en_req;
	/* [8] */
	uint8_t reg_spm_dvfs_req;
	/* [9] */
	uint8_t reg_spm_sw_mailbox_req;
	/* [10] */
	uint8_t reg_spm_sspm_mailbox_req;
	/* [11] */
	uint8_t reg_spm_adsp_mailbox_req;
	/* [12] */
	uint8_t reg_spm_scp_mailbox_req;

	/* SPM_SRC_MASK */
	/* [0] */
	uint8_t reg_sspm_srcclkena_0_mask_b;
	/* [1] */
	uint8_t reg_sspm_infra_req_0_mask_b;
	/* [2] */
	uint8_t reg_sspm_apsrc_req_0_mask_b;
	/* [3] */
	uint8_t reg_sspm_vrf18_req_0_mask_b;
	/* [4] */
	uint8_t reg_sspm_ddr_en_0_mask_b;
	/* [5] */
	uint8_t reg_scp_srcclkena_mask_b;
	/* [6] */
	uint8_t reg_scp_infra_req_mask_b;
	/* [7] */
	uint8_t reg_scp_apsrc_req_mask_b;
	/* [8] */
	uint8_t reg_scp_vrf18_req_mask_b;
	/* [9] */
	uint8_t reg_scp_ddr_en_mask_b;
	/* [10] */
	uint8_t reg_audio_dsp_srcclkena_mask_b;
	/* [11] */
	uint8_t reg_audio_dsp_infra_req_mask_b;
	/* [12] */
	uint8_t reg_audio_dsp_apsrc_req_mask_b;
	/* [13] */
	uint8_t reg_audio_dsp_vrf18_req_mask_b;
	/* [14] */
	uint8_t reg_audio_dsp_ddr_en_mask_b;
	/* [15] */
	uint8_t reg_apu_srcclkena_mask_b;
	/* [16] */
	uint8_t reg_apu_infra_req_mask_b;
	/* [17] */
	uint8_t reg_apu_apsrc_req_mask_b;
	/* [18] */
	uint8_t reg_apu_vrf18_req_mask_b;
	/* [19] */
	uint8_t reg_apu_ddr_en_mask_b;
	/* [20] */
	uint8_t reg_cpueb_srcclkena_mask_b;
	/* [21] */
	uint8_t reg_cpueb_infra_req_mask_b;
	/* [22] */
	uint8_t reg_cpueb_apsrc_req_mask_b;
	/* [23] */
	uint8_t reg_cpueb_vrf18_req_mask_b;
	/* [24] */
	uint8_t reg_cpueb_ddr_en_mask_b;
	/* [25] */
	uint8_t reg_bak_psri_srcclkena_mask_b;
	/* [26] */
	uint8_t reg_bak_psri_infra_req_mask_b;
	/* [27] */
	uint8_t reg_bak_psri_apsrc_req_mask_b;
	/* [28] */
	uint8_t reg_bak_psri_vrf18_req_mask_b;
	/* [29] */
	uint8_t reg_bak_psri_ddr_en_mask_b;
	/* [30] */
	uint8_t reg_cam_ddren_req_mask_b;
	/* [31] */
	uint8_t reg_img_ddren_req_mask_b;

	/* SPM_SRC2_MASK */
	/* [0] */
	uint8_t reg_msdc0_srcclkena_mask_b;
	/* [1] */
	uint8_t reg_msdc0_infra_req_mask_b;
	/* [2] */
	uint8_t reg_msdc0_apsrc_req_mask_b;
	/* [3] */
	uint8_t reg_msdc0_vrf18_req_mask_b;
	/* [4] */
	uint8_t reg_msdc0_ddr_en_mask_b;
	/* [5] */
	uint8_t reg_msdc1_srcclkena_mask_b;
	/* [6] */
	uint8_t reg_msdc1_infra_req_mask_b;
	/* [7] */
	uint8_t reg_msdc1_apsrc_req_mask_b;
	/* [8] */
	uint8_t reg_msdc1_vrf18_req_mask_b;
	/* [9] */
	uint8_t reg_msdc1_ddr_en_mask_b;
	/* [10] */
	uint8_t reg_msdc2_srcclkena_mask_b;
	/* [11] */
	uint8_t reg_msdc2_infra_req_mask_b;
	/* [12] */
	uint8_t reg_msdc2_apsrc_req_mask_b;
	/* [13] */
	uint8_t reg_msdc2_vrf18_req_mask_b;
	/* [14] */
	uint8_t reg_msdc2_ddr_en_mask_b;
	/* [15] */
	uint8_t reg_ufs_srcclkena_mask_b;
	/* [16] */
	uint8_t reg_ufs_infra_req_mask_b;
	/* [17] */
	uint8_t reg_ufs_apsrc_req_mask_b;
	/* [18] */
	uint8_t reg_ufs_vrf18_req_mask_b;
	/* [19] */
	uint8_t reg_ufs_ddr_en_mask_b;
	/* [20] */
	uint8_t reg_usb_srcclkena_mask_b;
	/* [21] */
	uint8_t reg_usb_infra_req_mask_b;
	/* [22] */
	uint8_t reg_usb_apsrc_req_mask_b;
	/* [23] */
	uint8_t reg_usb_vrf18_req_mask_b;
	/* [24] */
	uint8_t reg_usb_ddr_en_mask_b;
	/* [25] */
	uint8_t reg_pextp_p0_srcclkena_mask_b;
	/* [26] */
	uint8_t reg_pextp_p0_infra_req_mask_b;
	/* [27] */
	uint8_t reg_pextp_p0_apsrc_req_mask_b;
	/* [28] */
	uint8_t reg_pextp_p0_vrf18_req_mask_b;
	/* [29] */
	uint8_t reg_pextp_p0_ddr_en_mask_b;

	/* SPM_SRC3_MASK */
	/* [0] */
	uint8_t reg_pextp_p1_srcclkena_mask_b;
	/* [1] */
	uint8_t reg_pextp_p1_infra_req_mask_b;
	/* [2] */
	uint8_t reg_pextp_p1_apsrc_req_mask_b;
	/* [3] */
	uint8_t reg_pextp_p1_vrf18_req_mask_b;
	/* [4] */
	uint8_t reg_pextp_p1_ddr_en_mask_b;
	/* [5] */
	uint8_t reg_gce0_infra_req_mask_b;
	/* [6] */
	uint8_t reg_gce0_apsrc_req_mask_b;
	/* [7] */
	uint8_t reg_gce0_vrf18_req_mask_b;
	/* [8] */
	uint8_t reg_gce0_ddr_en_mask_b;
	/* [9] */
	uint8_t reg_gce1_infra_req_mask_b;
	/* [10] */
	uint8_t reg_gce1_apsrc_req_mask_b;
	/* [11] */
	uint8_t reg_gce1_vrf18_req_mask_b;
	/* [12] */
	uint8_t reg_gce1_ddr_en_mask_b;
	/* [13] */
	uint8_t reg_spm_srcclkena_reserved_mask_b;
	/* [14] */
	uint8_t reg_spm_infra_req_reserved_mask_b;
	/* [15] */
	uint8_t reg_spm_apsrc_req_reserved_mask_b;
	/* [16] */
	uint8_t reg_spm_vrf18_req_reserved_mask_b;
	/* [17] */
	uint8_t reg_spm_ddr_en_reserved_mask_b;
	/* [18] */
	uint8_t reg_disp0_apsrc_req_mask_b;
	/* [19] */
	uint8_t reg_disp0_ddr_en_mask_b;
	/* [20] */
	uint8_t reg_disp1_apsrc_req_mask_b;
	/* [21] */
	uint8_t reg_disp1_ddr_en_mask_b;
	/* [22] */
	uint8_t reg_disp2_apsrc_req_mask_b;
	/* [23] */
	uint8_t reg_disp2_ddr_en_mask_b;
	/* [24] */
	uint8_t reg_disp3_apsrc_req_mask_b;
	/* [25] */
	uint8_t reg_disp3_ddr_en_mask_b;
	/* [26] */
	uint8_t reg_infrasys_apsrc_req_mask_b;
	/* [27] */
	uint8_t reg_infrasys_ddr_en_mask_b;
	/* [28] */
	uint8_t reg_cg_check_srcclkena_mask_b;
	/* [29] */
	uint8_t reg_cg_check_apsrc_req_mask_b;
	/* [30] */
	uint8_t reg_cg_check_vrf18_req_mask_b;
	/* [31] */
	uint8_t reg_cg_check_ddr_en_mask_b;

	/* SPM_SRC4_MASK */
	/* [8:0] */
	uint32_t reg_mcusys_merge_apsrc_req_mask_b;
	/* [17:9] */
	uint32_t reg_mcusys_merge_ddr_en_mask_b;
	/* [19:18] */
	uint8_t reg_dramc_md32_infra_req_mask_b;
	/* [21:20] */
	uint8_t reg_dramc_md32_vrf18_req_mask_b;
	/* [23:22] */
	uint8_t reg_dramc_md32_ddr_en_mask_b;
	/* [24] */
	uint8_t reg_dvfsrc_event_trigger_mask_b;

	/* SPM_WAKEUP_EVENT_MASK2 */
	/* [3:0] */
	uint8_t reg_sc_sw2spm_wakeup_mask_b;
	/* [4] */
	uint8_t reg_sc_adsp2spm_wakeup_mask_b;
	/* [8:5] */
	uint8_t reg_sc_sspm2spm_wakeup_mask_b;
	/* [9] */
	uint8_t reg_sc_scp2spm_wakeup_mask_b;
	/* [10] */
	uint8_t reg_csyspwrup_ack_mask;
	/* [11] */
	uint8_t reg_csyspwrup_req_mask;

	/* SPM_WAKEUP_EVENT_MASK */
	/* [31:0] */
	uint32_t reg_wakeup_event_mask;

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	/* [31:0] */
	uint32_t reg_ext_wakeup_event_mask;
};

/* code gen by spm_pwr_ctrl_atf.pl, need struct pwr_ctrl */
enum pwr_ctrl_enum {
	PW_PCM_FLAGS,
	PW_PCM_FLAGS_CUST,
	PW_PCM_FLAGS_CUST_SET,
	PW_PCM_FLAGS_CUST_CLR,
	PW_PCM_FLAGS1,
	PW_PCM_FLAGS1_CUST,
	PW_PCM_FLAGS1_CUST_SET,
	PW_PCM_FLAGS1_CUST_CLR,
	PW_TIMER_VAL,
	PW_TIMER_VAL_CUST,
	PW_TIMER_VAL_RAMP_EN,
	PW_TIMER_VAL_RAMP_EN_SEC,
	PW_WAKE_SRC,
	PW_WAKE_SRC_CUST,
	PW_WDT_DISABLE,

	/* SPM_AP_STANDBY_CON */
	PW_REG_WFI_OP,
	PW_REG_WFI_TYPE,
	PW_REG_MP0_CPUTOP_IDLE_MASK,
	PW_REG_MP1_CPUTOP_IDLE_MASK,
	PW_REG_MCUSYS_IDLE_MASK,
	PW_REG_MD_APSRC_1_SEL,
	PW_REG_MD_APSRC_0_SEL,
	PW_REG_CONN_APSRC_SEL,

	/* SPM_SRC_REQ */
	PW_REG_SPM_APSRC_REQ,
	PW_REG_SPM_F26M_REQ,
	PW_REG_SPM_INFRA_REQ,
	PW_REG_SPM_VRF18_REQ,
	PW_REG_SPM_DDR_EN_REQ,
	PW_REG_SPM_DVFS_REQ,
	PW_REG_SPM_SW_MAILBOX_REQ,
	PW_REG_SPM_SSPM_MAILBOX_REQ,
	PW_REG_SPM_ADSP_MAILBOX_REQ,
	PW_REG_SPM_SCP_MAILBOX_REQ,

	/* SPM_SRC_MASK */
	PW_REG_SSPM_SRCCLKENA_0_MASK_B,
	PW_REG_SSPM_INFRA_REQ_0_MASK_B,
	PW_REG_SSPM_APSRC_REQ_0_MASK_B,
	PW_REG_SSPM_VRF18_REQ_0_MASK_B,
	PW_REG_SSPM_DDR_EN_0_MASK_B,
	PW_REG_SCP_SRCCLKENA_MASK_B,
	PW_REG_SCP_INFRA_REQ_MASK_B,
	PW_REG_SCP_APSRC_REQ_MASK_B,
	PW_REG_SCP_VRF18_REQ_MASK_B,
	PW_REG_SCP_DDR_EN_MASK_B,
	PW_REG_AUDIO_DSP_SRCCLKENA_MASK_B,
	PW_REG_AUDIO_DSP_INFRA_REQ_MASK_B,
	PW_REG_AUDIO_DSP_APSRC_REQ_MASK_B,
	PW_REG_AUDIO_DSP_VRF18_REQ_MASK_B,
	PW_REG_AUDIO_DSP_DDR_EN_MASK_B,
	PW_REG_APU_SRCCLKENA_MASK_B,
	PW_REG_APU_INFRA_REQ_MASK_B,
	PW_REG_APU_APSRC_REQ_MASK_B,
	PW_REG_APU_VRF18_REQ_MASK_B,
	PW_REG_APU_DDR_EN_MASK_B,
	PW_REG_CPUEB_SRCCLKENA_MASK_B,
	PW_REG_CPUEB_INFRA_REQ_MASK_B,
	PW_REG_CPUEB_APSRC_REQ_MASK_B,
	PW_REG_CPUEB_VRF18_REQ_MASK_B,
	PW_REG_CPUEB_DDR_EN_MASK_B,
	PW_REG_BAK_PSRI_SRCCLKENA_MASK_B,
	PW_REG_BAK_PSRI_INFRA_REQ_MASK_B,
	PW_REG_BAK_PSRI_APSRC_REQ_MASK_B,
	PW_REG_BAK_PSRI_VRF18_REQ_MASK_B,
	PW_REG_BAK_PSRI_DDR_EN_MASK_B,
	PW_REG_CAM_DDREN_REQ_MASK_B,
	PW_REG_IMG_DDREN_REQ_MASK_B,

	/* SPM_SRC2_MASK */
	PW_REG_MSDC0_SRCCLKENA_MASK_B,
	PW_REG_MSDC0_INFRA_REQ_MASK_B,
	PW_REG_MSDC0_APSRC_REQ_MASK_B,
	PW_REG_MSDC0_VRF18_REQ_MASK_B,
	PW_REG_MSDC0_DDR_EN_MASK_B,
	PW_REG_MSDC1_SRCCLKENA_MASK_B,
	PW_REG_MSDC1_INFRA_REQ_MASK_B,
	PW_REG_MSDC1_APSRC_REQ_MASK_B,
	PW_REG_MSDC1_VRF18_REQ_MASK_B,
	PW_REG_MSDC1_DDR_EN_MASK_B,
	PW_REG_MSDC2_SRCCLKENA_MASK_B,
	PW_REG_MSDC2_INFRA_REQ_MASK_B,
	PW_REG_MSDC2_APSRC_REQ_MASK_B,
	PW_REG_MSDC2_VRF18_REQ_MASK_B,
	PW_REG_MSDC2_DDR_EN_MASK_B,
	PW_REG_UFS_SRCCLKENA_MASK_B,
	PW_REG_UFS_INFRA_REQ_MASK_B,
	PW_REG_UFS_APSRC_REQ_MASK_B,
	PW_REG_UFS_VRF18_REQ_MASK_B,
	PW_REG_UFS_DDR_EN_MASK_B,
	PW_REG_USB_SRCCLKENA_MASK_B,
	PW_REG_USB_INFRA_REQ_MASK_B,
	PW_REG_USB_APSRC_REQ_MASK_B,
	PW_REG_USB_VRF18_REQ_MASK_B,
	PW_REG_USB_DDR_EN_MASK_B,
	PW_REG_PEXTP_P0_SRCCLKENA_MASK_B,
	PW_REG_PEXTP_P0_INFRA_REQ_MASK_B,
	PW_REG_PEXTP_P0_APSRC_REQ_MASK_B,
	PW_REG_PEXTP_P0_VRF18_REQ_MASK_B,
	PW_REG_PEXTP_P0_DDR_EN_MASK_B,

	/* SPM_SRC3_MASK */
	PW_REG_PEXTP_P1_SRCCLKENA_MASK_B,
	PW_REG_PEXTP_P1_INFRA_REQ_MASK_B,
	PW_REG_PEXTP_P1_APSRC_REQ_MASK_B,
	PW_REG_PEXTP_P1_VRF18_REQ_MASK_B,
	PW_REG_PEXTP_P1_DDR_EN_MASK_B,
	PW_REG_GCE0_INFRA_REQ_MASK_B,
	PW_REG_GCE0_APSRC_REQ_MASK_B,
	PW_REG_GCE0_VRF18_REQ_MASK_B,
	PW_REG_GCE0_DDR_EN_MASK_B,
	PW_REG_GCE1_INFRA_REQ_MASK_B,
	PW_REG_GCE1_APSRC_REQ_MASK_B,
	PW_REG_GCE1_VRF18_REQ_MASK_B,
	PW_REG_GCE1_DDR_EN_MASK_B,
	PW_REG_SPM_SRCCLKENA_RESERVED_MASK_B,
	PW_REG_SPM_INFRA_REQ_RESERVED_MASK_B,
	PW_REG_SPM_APSRC_REQ_RESERVED_MASK_B,
	PW_REG_SPM_VRF18_REQ_RESERVED_MASK_B,
	PW_REG_SPM_DDR_EN_RESERVED_MASK_B,
	PW_REG_DISP0_APSRC_REQ_MASK_B,
	PW_REG_DISP0_DDR_EN_MASK_B,
	PW_REG_DISP1_APSRC_REQ_MASK_B,
	PW_REG_DISP1_DDR_EN_MASK_B,
	PW_REG_DISP2_APSRC_REQ_MASK_B,
	PW_REG_DISP2_DDR_EN_MASK_B,
	PW_REG_DISP3_APSRC_REQ_MASK_B,
	PW_REG_DISP3_DDR_EN_MASK_B,
	PW_REG_INFRASYS_APSRC_REQ_MASK_B,
	PW_REG_INFRASYS_DDR_EN_MASK_B,
	PW_REG_CG_CHECK_SRCCLKENA_MASK_B,
	PW_REG_CG_CHECK_APSRC_REQ_MASK_B,
	PW_REG_CG_CHECK_VRF18_REQ_MASK_B,
	PW_REG_CG_CHECK_DDR_EN_MASK_B,

	/* SPM_SRC4_MASK */
	PW_REG_MCUSYS_MERGE_APSRC_REQ_MASK_B,
	PW_REG_MCUSYS_MERGE_DDR_EN_MASK_B,
	PW_REG_DRAMC_MD32_INFRA_REQ_MASK_B,
	PW_REG_DRAMC_MD32_VRF18_REQ_MASK_B,
	PW_REG_DRAMC_MD32_DDR_EN_MASK_B,
	PW_REG_DVFSRC_EVENT_TRIGGER_MASK_B,

	/* SPM_WAKEUP_EVENT_MASK2 */
	PW_REG_SC_SW2SPM_WAKEUP_MASK_B,
	PW_REG_SC_ADSP2SPM_WAKEUP_MASK_B,
	PW_REG_SC_SSPM2SPM_WAKEUP_MASK_B,
	PW_REG_SC_SCP2SPM_WAKEUP_MASK_B,
	PW_REG_CSYSPWRUP_ACK_MASK,
	PW_REG_CSYSPWRUP_REQ_MASK,

	/* SPM_WAKEUP_EVENT_MASK */
	PW_REG_WAKEUP_EVENT_MASK,

	/* SPM_WAKEUP_EVENT_EXT_MASK */
	PW_REG_EXT_WAKEUP_EVENT_MASK,

	PW_MAX_COUNT,
};

#endif /* __PWR_CTRL_H__ */
