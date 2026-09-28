// SPDX-License-Identifier: GPL-2.0
/*
 * mt8188-sunstone.c  --  MT8188 sunstone ALSA SoC machine driver
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <trace/hooks/sound.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
#include "mt8188-adsp-utils.h"
#endif
#include "mt8188-afe-common.h"
#include "../common/mtk-afe-platform-driver.h"
#if IS_ENABLED(CONFIG_SND_SOC_MT6359P_ACCDET_SUNSTONE)
#include "../../codecs/mt6359p-accdet.h"
#endif

/* FE */
SND_SOC_DAILINK_DEFS(playback2,
	DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback3,
	DAILINK_COMP_ARRAY(COMP_CPU("DL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback6,
	DAILINK_COMP_ARRAY(COMP_CPU("DL6")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback7,
	DAILINK_COMP_ARRAY(COMP_CPU("DL7")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback8,
	DAILINK_COMP_ARRAY(COMP_CPU("DL8")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback10,
	DAILINK_COMP_ARRAY(COMP_CPU("DL10")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback11,
	DAILINK_COMP_ARRAY(COMP_CPU("DL11")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture2,
	DAILINK_COMP_ARRAY(COMP_CPU("UL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture3,
	DAILINK_COMP_ARRAY(COMP_CPU("UL3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture4,
	DAILINK_COMP_ARRAY(COMP_CPU("UL4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture5,
	DAILINK_COMP_ARRAY(COMP_CPU("UL5")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture6,
	DAILINK_COMP_ARRAY(COMP_CPU("UL6")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture8,
	DAILINK_COMP_ARRAY(COMP_CPU("UL8")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture9,
	DAILINK_COMP_ARRAY(COMP_CPU("UL9")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(capture10,
	DAILINK_COMP_ARRAY(COMP_CPU("UL10")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
SND_SOC_DAILINK_DEFS(adsp_hostless,
	DAILINK_COMP_ARRAY(COMP_CPU("FE_HOSTLESS_VA")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(adsp_upload,
	DAILINK_COMP_ARRAY(COMP_CPU("FE_VA")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(adsp_record,
	DAILINK_COMP_ARRAY(COMP_CPU("FE_MICR")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif

SND_SOC_DAILINK_DEFS(hostless_adda,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA_HOSTLESS_LPBK")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(hostless_dmic,
	DAILINK_COMP_ARRAY(COMP_CPU("DMIC_HOSTLESS_RECORD")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(hostless_etdm1,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM1_HOSTLESS_LPBK")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* BE */
SND_SOC_DAILINK_DEFS(dl_src,
	DAILINK_COMP_ARRAY(COMP_CPU("DL_SRC")),
#if IS_ENABLED(CONFIG_SND_SOC_MT6359) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	DAILINK_COMP_ARRAY(COMP_CODEC("mt6359-sound", "mt6359-snd-codec-aif1")),
#else
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
#endif
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(virtual_source,
	DAILINK_COMP_ARRAY(COMP_CPU("DL_VIRTUAL_SOURCE")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(dmic,
	DAILINK_COMP_ARRAY(COMP_CPU("DMIC")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(dptx,
	DAILINK_COMP_ARRAY(COMP_CPU("DPTX")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(etdm1_in,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM1_IN")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(etdm2_in,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM2_IN")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(etdm1_out,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM1_OUT")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(etdm2_out,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM2_OUT")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(etdm3_out,
	DAILINK_COMP_ARRAY(COMP_CPU("ETDM3_OUT")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc0,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC0")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc1,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc2,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc3,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC3")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc4,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC4")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc5,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC5")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc6,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC6")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc7,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC7")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc8,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC8")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc9,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC9")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc10,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC10")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(gasrc11,
	DAILINK_COMP_ARRAY(COMP_CPU("GASRC11")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(hw_gain1,
	DAILINK_COMP_ARRAY(COMP_CPU("HW_GAIN1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(hw_gain2,
	DAILINK_COMP_ARRAY(COMP_CPU("HW_GAIN2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(multi_in1,
	DAILINK_COMP_ARRAY(COMP_CPU("MULTI_IN1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(multi_in2,
	DAILINK_COMP_ARRAY(COMP_CPU("MULTI_IN2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(pcm1,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(spdif_in,
	DAILINK_COMP_ARRAY(COMP_CPU("SPDIF_IN")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(spdif_out,
	DAILINK_COMP_ARRAY(COMP_CPU("SPDIF_OUT")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(ul_src1,
	DAILINK_COMP_ARRAY(COMP_CPU("UL_SRC1")),
#if IS_ENABLED(CONFIG_SND_SOC_MT6359) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	DAILINK_COMP_ARRAY(COMP_CODEC("mt6359-sound", "mt6359-snd-codec-aif1"),
			   COMP_CODEC("amic-codec", "amic-aif")),
#else
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
#endif
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
SND_SOC_DAILINK_DEFS(adsp_tdmin,
	DAILINK_COMP_ARRAY(COMP_CPU("BE_TDM_IN")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(adsp_ul9,
	DAILINK_COMP_ARRAY(COMP_CPU("BE_UL9_IN")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(adsp_ul2,
	DAILINK_COMP_ARRAY(COMP_CPU("BE_UL2_IN")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));
#endif

enum PINCTRL_PIN_STATE {
	PIN_STATE_DEFAULT = 0,
	PIN_STATE_EXTAMP_ON,
	PIN_STATE_EXTAMP_OFF,
	PIN_STATE_HP_EXTAMP_ON,
	PIN_STATE_HP_EXTAMP_OFF,
	PIN_STATE_MAX
};

#define SOC_DAPM_PIN_SWITCH_CUST(xname) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname "_Switch", \
	.info = snd_soc_dapm_info_pin_switch, \
	.get = snd_soc_dapm_get_pin_switch, \
	.put = snd_soc_dapm_put_pin_switch, \
	.private_value = (unsigned long)xname }

static const char * const mt8188_sunstone_pin_str[PIN_STATE_MAX] = {
	"default",
	"extamp_on",
	"extamp_off",
	"hp_extamp_on",
	"hp_extamp_off",
};

struct mt8188_sunstone_priv {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_states[PIN_STATE_MAX];
	struct gpio_desc *spk_pa_5v_en_gpio;
	unsigned int ext_spk_amp_vdd_on_time_us;
	struct device_node *platform_node;
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	struct device_node *adsp_platform_node;
#endif
#if IS_ENABLED(CONFIG_SND_SOC_HDMI_CODEC)
	struct snd_soc_jack hdmi_jack;
	struct snd_soc_jack dp_jack;
	struct device_node *hdmi_codec_node;
	struct device_node *dp_codec_node;
#endif
};

static int mt8188_snd_ext_hp_amp_wevent(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct mt8188_sunstone_priv *card_data = snd_soc_card_get_drvdata(card);
	int ret = 0;

	dev_info(card->dev, "%s event %d\n", __func__, event);

	switch (event) {
    	case SND_SOC_DAPM_POST_PMU:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_HP_EXTAMP_ON])) {
			ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_HP_EXTAMP_ON]);
			if (ret) {
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
			}
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8188_sunstone_pin_str[PIN_STATE_HP_EXTAMP_ON]);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_HP_EXTAMP_OFF])) {
			ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_HP_EXTAMP_OFF]);
			if (ret)
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8188_sunstone_pin_str[PIN_STATE_HP_EXTAMP_OFF]);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int mt8188_snd_ext_spk_amp_wevent(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_dapm_context *dapm = w->dapm;
	struct snd_soc_card *card = dapm->card;
	struct mt8188_sunstone_priv *card_data = snd_soc_card_get_drvdata(card);
	int ret = 0;

	dev_info(card->dev, "%s event %d\n", __func__, event);

	if (IS_ERR(card_data->spk_pa_5v_en_gpio)) {
		ret = PTR_ERR(card_data->spk_pa_5v_en_gpio);
		return ret;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		gpiod_set_value_cansleep(card_data->spk_pa_5v_en_gpio, 1);
		usleep_range(card_data->ext_spk_amp_vdd_on_time_us,
			card_data->ext_spk_amp_vdd_on_time_us + 1);
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXTAMP_ON])) {
			ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_EXTAMP_ON]);
			if (ret) {
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
				goto disable_spk_5v;
			}
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8188_sunstone_pin_str[PIN_STATE_EXTAMP_ON]);
			goto disable_spk_5v;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!IS_ERR(card_data->pin_states[PIN_STATE_EXTAMP_OFF])) {
			ret = pinctrl_select_state(card_data->pinctrl,
				card_data->pin_states[PIN_STATE_EXTAMP_OFF]);
			if (ret)
				dev_info(card->dev,
					"%s failed to select state %d\n",
					__func__, ret);
		} else {
			dev_info(card->dev,
				 "%s invalid pin state %s\n",
				 __func__,
				 mt8188_sunstone_pin_str[PIN_STATE_EXTAMP_OFF]);
		}
		gpiod_set_value_cansleep(card_data->spk_pa_5v_en_gpio, 0);
		break;
	default:
		break;
	}

	return 0;

disable_spk_5v:
	gpiod_set_value_cansleep(card_data->spk_pa_5v_en_gpio, 0);

	return ret;
}

#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
enum mt8188_volume_key_switch {
	VOLKEY_NORMAL = 0,
	VOLKEY_SWAP
};

static const char *const mt8188_volume_key_switch[] = { "VOLKEY_NORMAL", "VOLKEY_SWAP" };
extern void set_kpd_swap_vol_key(bool flag);
extern bool get_kpd_swap_vol_key(void);

static int mt8188_volkey_switch_get(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (get_kpd_swap_vol_key())
		ucontrol->value.integer.value[0] = VOLKEY_SWAP;
	else
		ucontrol->value.integer.value[0] = VOLKEY_NORMAL;

	return 0;
}

static int mt8188_volkey_switch_set(
				struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.integer.value[0] == VOLKEY_NORMAL)
		set_kpd_swap_vol_key(false);
	else
		set_kpd_swap_vol_key(true);

	return 0;
}

static const struct soc_enum mt8188_volkey_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8188_volume_key_switch),
		mt8188_volume_key_switch),
};
#endif
static int mt8188_afe_dl_playback_state_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol);

static const struct snd_soc_dapm_widget mt8188_sunstone_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk Amp", mt8188_snd_ext_spk_amp_wevent),
	SND_SOC_DAPM_HP("Ext Headphone Amp", mt8188_snd_ext_hp_amp_wevent),
	SND_SOC_DAPM_MIC("AP DMIC", NULL),
	SND_SOC_DAPM_MIC("Audio_MICBIAS0", NULL),
	SND_SOC_DAPM_OUTPUT("UL_VIRTUAL_OUTPUT"),
};

static const struct snd_kcontrol_new mt8188_sunstone_controls[] = {
	SOC_DAPM_PIN_SWITCH_CUST("Audio_MICBIAS0"),
	SOC_DAPM_PIN_SWITCH("Ext Spk Amp"),
	SOC_DAPM_PIN_SWITCH("Ext Headphone Amp"),
#ifdef CONFIG_KPD_VOLUME_KEY_SWAP
	SOC_ENUM_EXT("VOLKEY_SWITCH", mt8188_volkey_control_enum[0],
			mt8188_volkey_switch_get,
			mt8188_volkey_switch_set),
#endif
	SOC_SINGLE_BOOL_EXT("DL_Playback_State", 0,
			mt8188_afe_dl_playback_state_get,NULL),
};

static const struct snd_soc_dapm_route mt8188_sunstone_routes[] = {
	{ "Audio_MICBIAS0", NULL, "vaud18" },
	{ "Audio_MICBIAS0", NULL, "AUDGLB" },
	{ "Audio_MICBIAS0", NULL, "MIC_BIAS_0" },
	{ "UL_VIRTUAL_OUTPUT", NULL, "Audio_MICBIAS0" },
};

#if IS_ENABLED(CONFIG_SND_SOC_MT6359) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#include "../../codecs/mt6359.h"

#define CKSYS_AUD_TOP_CFG 0x032c
#define CKSYS_AUD_TOP_MON 0x0330

static int mt8188_mt6359_mtkaif_calibration(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *cmpnt_afe =
		snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	struct snd_soc_component *cmpnt_codec =
		asoc_rtd_to_codec(rtd, 0)->component;
	struct mtk_base_afe *afe;
	struct mt8188_afe_private *afe_priv;
	struct mtkaif_param *param;
	int chosen_phase_1, chosen_phase_2;
	int prev_cycle_1, prev_cycle_2;
	int test_done_1, test_done_2;
	int cycle_1, cycle_2;
	int mtkaif_chosen_phase[MT8188_MTKAIF_MISO_NUM];
	int mtkaif_phase_cycle[MT8188_MTKAIF_MISO_NUM];
	int mtkaif_calibration_num_phase;
	bool mtkaif_calibration_ok;
	unsigned int monitor = 0;
	int counter;
	int phase;
	int i;

	if (!cmpnt_afe)
		return -EINVAL;

	afe = snd_soc_component_get_drvdata(cmpnt_afe);
	afe_priv = afe->platform_priv;
	param = &afe_priv->mtkaif_params;

	dev_dbg(afe->dev, "%s(), start\n", __func__);

	param->mtkaif_calibration_ok = false;
	for (i = 0; i < MT8188_MTKAIF_MISO_NUM; i++) {
		param->mtkaif_chosen_phase[i] = -1;
		param->mtkaif_phase_cycle[i] = 0;
		mtkaif_chosen_phase[i] = -1;
		mtkaif_phase_cycle[i] = 0;
	}

	if (IS_ERR(afe_priv->topckgen)) {
		dev_info(afe->dev, "%s() Cannot find topckgen controller\n",
			 __func__);
		return 0;
	}

	pm_runtime_get_sync(afe->dev);
	mt6359_mtkaif_calibration_enable(cmpnt_codec);

	/* set test type to synchronizer pulse */
	regmap_update_bits(afe_priv->topckgen,
			   CKSYS_AUD_TOP_CFG, 0xffff, 0x4);
	mtkaif_calibration_num_phase = 42;	/* mt6359: 0 ~ 42 */
	mtkaif_calibration_ok = true;

	for (phase = 0;
	     phase <= mtkaif_calibration_num_phase && mtkaif_calibration_ok;
	     phase++) {
		mt6359_set_mtkaif_calibration_phase(cmpnt_codec,
						    phase, phase, phase);

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x1);

		test_done_1 = 0;
		test_done_2 = 0;

		cycle_1 = -1;
		cycle_2 = -1;

		counter = 0;
		while (!(test_done_1 & test_done_2)) {
			regmap_read(afe_priv->topckgen,
				    CKSYS_AUD_TOP_MON, &monitor);
			test_done_1 = (monitor >> 28) & 0x1;
			test_done_2 = (monitor >> 29) & 0x1;

			if (test_done_1 == 1)
				cycle_1 = monitor & 0xf;

			if (test_done_2 == 1)
				cycle_2 = (monitor >> 4) & 0xf;

			/* handle if never test done */
			if (++counter > 10000) {
				dev_info(afe->dev, "%s(), test fail, cycle_1 %d, cycle_2 %d, monitor 0x%x\n",
					 __func__,
					 cycle_1, cycle_2, monitor);
				mtkaif_calibration_ok = false;
				break;
			}
		}

		if (phase == 0) {
			prev_cycle_1 = cycle_1;
			prev_cycle_2 = cycle_2;
		}

		if (cycle_1 != prev_cycle_1 &&
		    mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] < 0) {
			mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] = phase - 1;
			mtkaif_phase_cycle[MT8188_MTKAIF_MISO_0] = prev_cycle_1;
		}

		if (cycle_2 != prev_cycle_2 &&
		    mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] < 0) {
			mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] = phase - 1;
			mtkaif_phase_cycle[MT8188_MTKAIF_MISO_1] = prev_cycle_2;
		}

		regmap_update_bits(afe_priv->topckgen,
				   CKSYS_AUD_TOP_CFG, 0x1, 0x0);

		if (mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] >= 0 &&
		    mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] >= 0)
			break;
	}

	if (mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] < 0) {
		mtkaif_calibration_ok = false;
		chosen_phase_1 = 0;
	} else {
		chosen_phase_1 = mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0];
	}

	if (mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] < 0) {
		mtkaif_calibration_ok = false;
		chosen_phase_2 = 0;
	} else {
		chosen_phase_2 = mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1];
	}


	mt6359_set_mtkaif_calibration_phase(cmpnt_codec,
					    chosen_phase_1,
					    chosen_phase_2,
					    0);

	mt6359_mtkaif_calibration_disable(cmpnt_codec);
	pm_runtime_put(afe->dev);

	param->mtkaif_calibration_ok = mtkaif_calibration_ok;
	param->mtkaif_chosen_phase[MT8188_MTKAIF_MISO_0] = chosen_phase_1;
	param->mtkaif_chosen_phase[MT8188_MTKAIF_MISO_1] = chosen_phase_2;

	for (i = 0; i < MT8188_MTKAIF_MISO_NUM; i++)
		param->mtkaif_phase_cycle[i] = mtkaif_phase_cycle[i];

	dev_info(afe->dev, "%s(), end, calibration ok %d\n",
		 __func__, param->mtkaif_calibration_ok);

	return 0;
}

static int mt8188_mt6359_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *cmpnt_codec =
		asoc_rtd_to_codec(rtd, 0)->component;

	/* set mtkaif protocol */
	mt6359_set_mtkaif_protocol(cmpnt_codec,
				   MT6359_MTKAIF_PROTOCOL_2_CLK_P2);

	/* mtkaif calibration */
	mt8188_mt6359_mtkaif_calibration(rtd);

#if IS_ENABLED(CONFIG_SND_SOC_MT6359P_ACCDET_SUNSTONE)
	mt6359p_accdet_init(cmpnt_codec, rtd->card);
#endif

	return 0;
}
#endif
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
static int mt8188_snd_multi_in_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	/* fix BE format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
		0, SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}
#endif

enum {
	DAI_LINK_DL2_FE,
	DAI_LINK_DL3_FE,
	DAI_LINK_DL6_FE,
	DAI_LINK_DL7_FE,
	DAI_LINK_DL8_FE,
	DAI_LINK_DL10_FE,
	DAI_LINK_DL11_FE,
	DAI_LINK_UL1_FE,
	DAI_LINK_UL2_FE,
	DAI_LINK_UL3_FE,
	DAI_LINK_UL4_FE,
	DAI_LINK_UL5_FE,
	DAI_LINK_UL6_FE,
	DAI_LINK_UL8_FE,
	DAI_LINK_UL9_FE,
	DAI_LINK_UL10_FE,
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	DAI_LINK_ADSP_HOSTLESS_VA_FE,
	DAI_LINK_ADSP_VA_FE,
	DAI_LINK_ADSP_MIC_RECORD_FE,
#endif
	DAI_LINK_ADDA_HOSTLESS_LPBK,
	DAI_LINK_DMIC_HOSTLESS_RECORD,
	DAI_LINK_ETDM1_HOSTLESS_LPBK,
	DAI_LINK_DL_SRC_BE,
	DAI_LINK_DL_VIRTUAL_SOURCE_BE,
	DAI_LINK_DMIC_BE,
	DAI_LINK_DPTX_BE,
	DAI_LINK_ETDM1_IN_BE,
	DAI_LINK_ETDM2_IN_BE,
	DAI_LINK_ETDM1_OUT_BE,
	DAI_LINK_ETDM2_OUT_BE,
	DAI_LINK_ETDM3_OUT_BE,
	DAI_LINK_GASRC0_BE,
	DAI_LINK_GASRC1_BE,
	DAI_LINK_GASRC2_BE,
	DAI_LINK_GASRC3_BE,
	DAI_LINK_GASRC4_BE,
	DAI_LINK_GASRC5_BE,
	DAI_LINK_GASRC6_BE,
	DAI_LINK_GASRC7_BE,
	DAI_LINK_GASRC8_BE,
	DAI_LINK_GASRC9_BE,
	DAI_LINK_GASRC10_BE,
	DAI_LINK_GASRC11_BE,
	DAI_LINK_HW_GAIN1_BE,
	DAI_LINK_HW_GAIN2_BE,
	DAI_LINK_MULTI_IN1_BE,
	DAI_LINK_MULTI_IN2_BE,
	DAI_LINK_PCM1_BE,
	DAI_LINK_SPDIF_IN_BE,
	DAI_LINK_SPDIF_OUT_BE,
	DAI_LINK_UL_SRC1_BE,
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	DAI_LINK_ADSP_TDM_IN_BE,
	DAI_LINK_ADSP_UL9_IN_BE,
	DAI_LINK_ADSP_UL2_IN_BE,
#endif
};

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
static int mt8188_adsp_hostless_va_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_card *card;
	struct snd_soc_pcm_runtime *afe_rtd;
	struct snd_soc_component *component;

	/* set ignore suspend for hostless path */
	component = asoc_rtd_to_cpu(rtd, 0)->component;
	dapm = snd_soc_component_get_dapm(component);
	snd_soc_dapm_ignore_suspend(dapm, "FE_HOSTLESS_VA");
	snd_soc_dapm_ignore_suspend(dapm, "VA UL2 In");
	snd_soc_dapm_ignore_suspend(dapm, "VA TDMIN In");
	snd_soc_dapm_ignore_suspend(dapm, "VA UL9 In");

	card = rtd->card;

	list_for_each_entry(afe_rtd, &card->rtd_list, list) {
		if (!strcmp(asoc_rtd_to_cpu(afe_rtd, 0)->name, "DMIC"))
			break;
	}
	if (afe_rtd) {
		component = asoc_rtd_to_cpu(afe_rtd, 0)->component;
		dapm = snd_soc_component_get_dapm(component);
		snd_soc_dapm_ignore_suspend(dapm, "DMIC_INPUT");
		snd_soc_dapm_ignore_suspend(dapm, "ETDM_INPUT");
	}
#if IS_ENABLED(CONFIG_SND_SOC_MT6359) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	list_for_each_entry(afe_rtd, &card->rtd_list, list) {
		if (!strcmp(asoc_rtd_to_cpu(afe_rtd, 0)->name, "UL_SRC1"))
			break;
	}
	if (afe_rtd) {
		component = asoc_rtd_to_cpu(afe_rtd, 0)->component;
		dapm = snd_soc_component_get_dapm(component);
		snd_soc_dapm_ignore_suspend(dapm, "AIN0");
		snd_soc_dapm_ignore_suspend(dapm, "AIN1");
		snd_soc_dapm_ignore_suspend(dapm, "AIN2");
		snd_soc_dapm_ignore_suspend(dapm, "AIN3");
		snd_soc_dapm_ignore_suspend(dapm, "AIN0_DMIC");
		snd_soc_dapm_ignore_suspend(dapm, "AIN2_DMIC");
		snd_soc_dapm_ignore_suspend(dapm, "AIN3_DMIC");
	}
#endif
	return 0;
}

static int mt8188_adsp_hostless_va_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_dpcm *dpcm;
	struct snd_soc_pcm_runtime *fe = asoc_substream_to_rtd(substream);
	struct snd_soc_pcm_runtime *be;

	/* should set all the connected be to ignore suspend */
	/* all there will be a substream->ops action in suspend_all */

	list_for_each_entry(dpcm,
			    &fe->dpcm[SNDRV_PCM_STREAM_CAPTURE].be_clients,
			    list_be) {
		be = dpcm->be;
		be->dai_link->ignore_suspend = 1;
	}
	return 0;
}

static void mt8188_adsp_hostless_va_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_dpcm *dpcm;
	struct snd_soc_pcm_runtime *fe = asoc_substream_to_rtd(substream);
	struct snd_soc_pcm_runtime *be;

	/* should resume all the connected be */
	list_for_each_entry(dpcm,
			    &fe->dpcm[SNDRV_PCM_STREAM_CAPTURE].be_clients,
			    list_be) {
		be = dpcm->be;
		be->dai_link->ignore_suspend = 0;
	}
}

static struct snd_soc_ops adsp_hostless_va_ops = {
	.startup = mt8188_adsp_hostless_va_startup,
	.shutdown = mt8188_adsp_hostless_va_shutdown,
};
#endif

static int mt8188_dptx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 256;
	unsigned int mclk_fs = rate * mclk_fs_ratio;
	struct snd_soc_dai *dai = asoc_rtd_to_cpu(rtd, 0);

	return snd_soc_dai_set_sysclk(dai, 0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static struct snd_soc_ops mt8188_sunstone_dptx_ops = {
	.hw_params = mt8188_dptx_hw_params,
};

static int mt8188_snd_dptx_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	/* fix BE i2s format to 32bit, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);

	return 0;
}

#if IS_ENABLED(CONFIG_SND_SOC_HDMI_CODEC) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
static int mt8188_snd_hdmi_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mt8188_sunstone_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_component *component = asoc_rtd_to_codec(rtd, 0)->component;
	int ret = 0;

	ret = snd_soc_card_jack_new(rtd->card, "HDMI Jack", SND_JACK_LINEOUT,
				    &priv->hdmi_jack, NULL, 0);
	if (ret) {
		dev_info(rtd->dev, "%s, new jack failed: %d\n", __func__, ret);
		return ret;
	}

	ret = snd_soc_component_set_jack(component, &priv->hdmi_jack, NULL);
	if (ret)
		dev_info(rtd->dev, "%s, set jack failed on %s (ret=%d)\n",
			 __func__, component->name, ret);

	return ret;
}

static int mt8188_snd_dp_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	struct mt8188_sunstone_priv *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_component *component = asoc_rtd_to_codec(rtd, 0)->component;
	int ret = 0;

	ret = snd_soc_card_jack_new(rtd->card, "DP Jack", SND_JACK_LINEOUT,
				    &priv->dp_jack, NULL, 0);
	if (ret) {
		dev_info(rtd->dev, "%s, new jack failed: %d\n", __func__, ret);
		return ret;
	}

	ret = snd_soc_component_set_jack(component, &priv->dp_jack, NULL);
	if (ret)
		dev_info(rtd->dev, "%s, set jack failed on %s (ret=%d)\n",
			 __func__, component->name, ret);

	return ret;
}
#endif
static struct snd_soc_dai_link mt8188_sunstone_dai_links[] = {
	/* FE */
	[DAI_LINK_DL2_FE] = {
		.name = "DL2_FE",
		.stream_name = "DL2 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	[DAI_LINK_DL3_FE] = {
		.name = "DL3_FE",
		.stream_name = "DL3 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback3),
	},
	[DAI_LINK_DL6_FE] = {
		.name = "DL6_FE",
		.stream_name = "DL6 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback6),
	},
	[DAI_LINK_DL7_FE] = {
		.name = "DL7_FE",
		.stream_name = "DL7 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback7),
	},
	[DAI_LINK_DL8_FE] = {
		.name = "DL8_FE",
		.stream_name = "DL8 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback8),
	},
	[DAI_LINK_DL10_FE] = {
		.name = "DL10_FE",
		.stream_name = "DL10 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback10),
	},
	[DAI_LINK_DL11_FE] = {
		.name = "DL11_FE",
		.stream_name = "DL11 Playback",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(playback11),
	},
	[DAI_LINK_UL1_FE] = {
		.name = "UL1_FE",
		.stream_name = "UL1 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture1),
	},
	[DAI_LINK_UL2_FE] = {
		.name = "UL2_FE",
		.stream_name = "UL2 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture2),
	},
	[DAI_LINK_UL3_FE] = {
		.name = "UL3_FE",
		.stream_name = "UL3 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture3),
	},
	[DAI_LINK_UL4_FE] = {
		.name = "UL4_FE",
		.stream_name = "UL4 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture4),
	},
	[DAI_LINK_UL5_FE] = {
		.name = "UL5_FE",
		.stream_name = "UL5 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture5),
	},
	[DAI_LINK_UL6_FE] = {
		.name = "UL6_FE",
		.stream_name = "UL6 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_PRE,
			SND_SOC_DPCM_TRIGGER_PRE,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture6),
	},
	[DAI_LINK_UL8_FE] = {
		.name = "UL8_FE",
		.stream_name = "UL8 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture8),
	},
	[DAI_LINK_UL9_FE] = {
		.name = "UL9_FE",
		.stream_name = "UL9 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture9),
	},
	[DAI_LINK_UL10_FE] = {
		.name = "UL10_FE",
		.stream_name = "UL10 Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST,
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(capture10),
	},
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	[DAI_LINK_ADSP_HOSTLESS_VA_FE] = {
		.name = "ADSP_HOSTLESS_VA",
		.stream_name = "HOSTLESS_VA",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		.init = mt8188_adsp_hostless_va_init,
		.ops = &adsp_hostless_va_ops,
		SND_SOC_DAILINK_REG(adsp_hostless),
	},
	[DAI_LINK_ADSP_VA_FE] = {
		.name = "ADSP_VA_FE",
		.stream_name = "VA_Capture",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(adsp_upload),
	},
	[DAI_LINK_ADSP_MIC_RECORD_FE] = {
		.name = "ADSP_MIC_RECORD",
		.stream_name = "MIC_RECORD",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(adsp_record),
	},
#endif
	[DAI_LINK_ADDA_HOSTLESS_LPBK] = {
		.name = "ADDA_HOSTLESS_LPBK_FE",
		.stream_name = "ADDA_HOSTLESS_LPBK",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_adda),
	},
	[DAI_LINK_DMIC_HOSTLESS_RECORD] = {
		.name = "DMIC_HOSTLESS_RECORD_FE",
		.stream_name = "DMIC_HOSTLESS_RECORD",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_dmic),
	},
	[DAI_LINK_ETDM1_HOSTLESS_LPBK] = {
		.name = "ETDM1_HOSTLESS_LPBK_FE",
		.stream_name = "ETDM1_HOSTLESS_LPBK",
		.trigger = {
			SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST
		},
		.dynamic = 1,
		.dpcm_playback = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hostless_etdm1),
	},
	/* BE */
	[DAI_LINK_DL_SRC_BE] = {
		.name = "DL_SRC_BE",
#if IS_ENABLED(CONFIG_SND_SOC_MT6359) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
		.init = mt8188_mt6359_init,
#endif
		.no_pcm = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(dl_src),
	},
	[DAI_LINK_DL_VIRTUAL_SOURCE_BE] = {
		.name = "DL_VIRTUAL_SOURCE BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(virtual_source),
	},
	[DAI_LINK_DMIC_BE] = {
		.name = "DMIC_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(dmic),
	},
	[DAI_LINK_DPTX_BE] = {
		.name = "DPTX_BE",
		.ops = &mt8188_sunstone_dptx_ops,
		.be_hw_params_fixup = mt8188_snd_dptx_hw_params_fixup,
		.no_pcm = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(dptx),
	},
	[DAI_LINK_ETDM1_IN_BE] = {
		.name = "ETDM1_IN_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(etdm1_in),
	},
	[DAI_LINK_ETDM2_IN_BE] = {
		.name = "ETDM2_IN_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(etdm2_in),
	},
	[DAI_LINK_ETDM1_OUT_BE] = {
		.name = "ETDM1_OUT_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(etdm1_out),
	},
	[DAI_LINK_ETDM2_OUT_BE] = {
		.name = "ETDM2_OUT_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(etdm2_out),
	},
	[DAI_LINK_ETDM3_OUT_BE] = {
		.name = "ETDM3_OUT_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(etdm3_out),
	},
	[DAI_LINK_GASRC0_BE] = {
		.name = "GASRC0_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc0),
	},
	[DAI_LINK_GASRC1_BE] = {
		.name = "GASRC1_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc1),
	},
	[DAI_LINK_GASRC2_BE] = {
		.name = "GASRC2_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc2),
	},
	[DAI_LINK_GASRC3_BE] = {
		.name = "GASRC3_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc3),
	},
	[DAI_LINK_GASRC4_BE] = {
		.name = "GASRC4_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc4),
	},
	[DAI_LINK_GASRC5_BE] = {
		.name = "GASRC5_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc5),
	},
	[DAI_LINK_GASRC6_BE] = {
		.name = "GASRC6_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc6),
	},
	[DAI_LINK_GASRC7_BE] = {
		.name = "GASRC7_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc7),
	},
	[DAI_LINK_GASRC8_BE] = {
		.name = "GASRC8_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc8),
	},
	[DAI_LINK_GASRC9_BE] = {
		.name = "GASRC9_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc9),
	},
	[DAI_LINK_GASRC10_BE] = {
		.name = "GASRC10_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(gasrc10),
	},
	[DAI_LINK_GASRC11_BE] = {
		.name = "GASRC11_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(gasrc11),
	},
	[DAI_LINK_HW_GAIN1_BE] = {
		.name = "HW_GAIN1_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain1),
	},
	[DAI_LINK_HW_GAIN2_BE] = {
		.name = "HW_GAIN2_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(hw_gain2),
	},
	[DAI_LINK_MULTI_IN1_BE] = {
		.name = "MULTI_IN1_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(multi_in1),
	},
	[DAI_LINK_MULTI_IN2_BE] = {
		.name = "MULTI_IN2_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(multi_in2),
	},
	[DAI_LINK_PCM1_BE] = {
		.name = "PCM1_BE",
		.no_pcm = 1,
		.dai_fmt = SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(pcm1),
	},
	[DAI_LINK_SPDIF_IN_BE] = {
		.name = "SPDIF_IN_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(spdif_in),
	},
	[DAI_LINK_SPDIF_OUT_BE] = {
		.name = "SPDIF_OUT_BE",
		.no_pcm = 1,
		.dpcm_playback = 1,
		SND_SOC_DAILINK_REG(spdif_out),
	},
	[DAI_LINK_UL_SRC1_BE] = {
		.name = "UL_SRC1_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		SND_SOC_DAILINK_REG(ul_src1),
	},
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	[DAI_LINK_ADSP_TDM_IN_BE] = {
		.name = "ADSP_TDM_IN_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_tdmin),
	},
	[DAI_LINK_ADSP_UL9_IN_BE] = {
		.name = "ADSP_UL9_IN_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_ul9),
	},
	[DAI_LINK_ADSP_UL2_IN_BE] = {
		.name = "ADSP_UL2_IN_BE",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_ul2),
	},
#endif
};

static int mt8188_afe_dl_playback_state_get(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct snd_soc_pcm_runtime *rtd = NULL;
	enum snd_soc_dpcm_state state;

	rtd = snd_soc_get_pcm_runtime(card, &mt8188_sunstone_dai_links[DAI_LINK_DL_SRC_BE]);
	if (!rtd) {
		ucontrol->value.integer.value[0] = 0;
		dev_dbg(card->dev, "%s rtd get fail\n", __func__);
		return 0;
	}

	state = rtd->dpcm[SNDRV_PCM_STREAM_PLAYBACK].state;
	if (state >= SND_SOC_DPCM_STATE_NEW && state <= SND_SOC_DPCM_STATE_START)
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;

	dev_dbg(card->dev, "%s+ enable %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	return 0;
}

static int mt8188_snd_gpio_probe(struct snd_soc_card *card)
{
	struct mt8188_sunstone_priv *priv = snd_soc_card_get_drvdata(card);
	int ret = 0;
	int i;

	priv->pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(priv->pinctrl)) {
		ret = PTR_ERR(priv->pinctrl);
		dev_info(card->dev, "%s devm_pinctrl_get failed %d\n",
			__func__, ret);
		return ret;
	}

	for (i = 0 ; i < PIN_STATE_MAX ; i++) {
		priv->pin_states[i] = pinctrl_lookup_state(priv->pinctrl,
			mt8188_sunstone_pin_str[i]);
		if (IS_ERR(priv->pin_states[i])) {
			ret = PTR_ERR(priv->pin_states[i]);
			dev_info(card->dev, "%s Can't find pin state %s %d\n",
				 __func__, mt8188_sunstone_pin_str[i], ret);
		}
	}

	if (IS_ERR(priv->pin_states[PIN_STATE_DEFAULT])) {
		dev_info(card->dev, "%s can't find default pin state\n",
			__func__);
		return 0;
	}

	/* default state */
	ret = pinctrl_select_state(priv->pinctrl,
				   priv->pin_states[PIN_STATE_DEFAULT]);
	if (ret)
		dev_info(card->dev, "%s failed to select state %d\n",
			__func__, ret);

	/* turn off hp ext amp if exist*/
	if (!IS_ERR(priv->pin_states[PIN_STATE_HP_EXTAMP_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
			priv->pin_states[PIN_STATE_HP_EXTAMP_OFF]);
		if (ret)
			dev_info(card->dev,
				"%s failed to select state %d\n",
				__func__, ret);
	}

	/* turn off ext amp if exist */
	if (!IS_ERR(priv->pin_states[PIN_STATE_EXTAMP_OFF])) {
		ret = pinctrl_select_state(priv->pinctrl,
			priv->pin_states[PIN_STATE_EXTAMP_OFF]);
		if (ret)
			dev_info(card->dev,
				"%s failed to select state %d\n",
				__func__, ret);
	}

	/*power control gpio for speaker PA*/
	priv->spk_pa_5v_en_gpio = devm_gpiod_get(card->dev, "spk-pa-5v-en",
					GPIOD_OUT_LOW);

	if (IS_ERR(priv->spk_pa_5v_en_gpio)) {
		ret = PTR_ERR(priv->spk_pa_5v_en_gpio);
		dev_info(card->dev, "failed to get spk_pa_5v_en_gpio: %d\n", ret);
	}

	return ret;
}

static int mt8188_snd_late_probe(struct snd_soc_card *card)
{
	int err;

	err = snd_soc_dapm_disable_pin(&card->dapm, "Audio_MICBIAS0");
	if (err)
		dev_err(card->dev, "Failed to disable Audio_MICBIAS0 : %d\n", err);

	err = snd_soc_dapm_disable_pin(&card->dapm, "Ext Spk Amp");
	if (err)
		dev_err(card->dev, "Failed to disable Ext Spk Amp : %d\n", err);

	err = snd_soc_dapm_disable_pin(&card->dapm, "Ext Headphone Amp");
	if (err)
		dev_err(card->dev, "Failed to disable Ext Headphone Amp : %d\n", err);

	err = snd_soc_dapm_ignore_suspend(&card->dapm, "Audio_MICBIAS0");
	if (err)
		dev_err(card->dev, "Failed to ignore Audio_MICBIAS0 : %d\n", err);

	err = snd_soc_dapm_ignore_suspend(&card->dapm, "UL_VIRTUAL_OUTPUT");
	if (err)
		dev_err(card->dev, "Failed to ignore UL_VIRTUAL_OUTPUT : %d\n", err);

	return err;
}

static struct snd_soc_card mt8188_sunstone_card = {
	.name = "mt8188-sound",
	.owner = THIS_MODULE,
	.dai_link = mt8188_sunstone_dai_links,
	.num_links = ARRAY_SIZE(mt8188_sunstone_dai_links),
	.dapm_widgets = mt8188_sunstone_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt8188_sunstone_widgets),
	.controls = mt8188_sunstone_controls,
	.num_controls = ARRAY_SIZE(mt8188_sunstone_controls),
	.dapm_routes = mt8188_sunstone_routes,
	.num_dapm_routes = ARRAY_SIZE(mt8188_sunstone_routes),
	.late_probe = mt8188_snd_late_probe,
};

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
static const unsigned int adsp_dai_links[] = {
	DAI_LINK_ADSP_HOSTLESS_VA_FE,
	DAI_LINK_ADSP_VA_FE,
	DAI_LINK_ADSP_MIC_RECORD_FE,
	DAI_LINK_ADSP_TDM_IN_BE,
	DAI_LINK_ADSP_UL9_IN_BE,
	DAI_LINK_ADSP_UL2_IN_BE
};

static bool is_adsp_dai_link(unsigned int dai)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(adsp_dai_links); i++)
		if (dai == adsp_dai_links[i])
			return true;

	return false;
}
#endif

static void mt8188_snd_component_chaining_hook(void *data, bool *ret)
{
	*ret = true;
}

static int mt8188_snd_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8188_sunstone_card;
	struct mt8188_sunstone_priv *priv;
	struct snd_soc_dai_link *dai_link;
	int ret, i;

	dev_info(&pdev->dev, "%s()+\n", __func__);

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return  -ENOMEM;

	card->dev = &pdev->dev;

	priv->platform_node = of_parse_phandle(pdev->dev.of_node,
					       "mediatek,platform", 0);
	if (!priv->platform_node) {
		dev_info(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	if (of_property_read_bool(pdev->dev.of_node, "audio-routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "audio-routing");
		if (ret)
			return ret;
	}

	of_property_read_u32(pdev->dev.of_node, "ext-spk-amp-vdd-on-time-us",
		&priv->ext_spk_amp_vdd_on_time_us);

	for_each_card_prelinks(card, i, dai_link) {
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
		if (is_adsp_dai_link(i))
			continue;
#endif
		if (dai_link->platforms->name)
			continue;
		dai_link->platforms->of_node = priv->platform_node;
	}

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	priv->adsp_platform_node = of_parse_phandle(pdev->dev.of_node,
						    "mediatek,adsp-platform", 0);
	if (!priv->adsp_platform_node) {
		dev_info(&pdev->dev,
			 "Property 'adsp-platform' missing or invalid\n");
		ret = -EINVAL;
		goto err_adsp_platform;
	}
	for_each_card_prelinks(card, i, dai_link) {
		if (!is_adsp_dai_link(i) || dai_link->platforms->name)
			continue;
		dai_link->platforms->of_node = priv->adsp_platform_node;
	}
#endif

#if IS_ENABLED(CONFIG_SND_SOC_HDMI_CODEC) && !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	priv->hdmi_codec_node = of_parse_phandle(pdev->dev.of_node,
						 "mediatek,hdmi-codec", 0);
	if (!priv->hdmi_codec_node) {
		dev_dbg(&pdev->dev, "No property 'hdmi-codec'\n");
	} else {
		mt8188_sunstone_dai_links[DAI_LINK_ETDM3_OUT_BE].codecs->of_node =
			priv->hdmi_codec_node;
		mt8188_sunstone_dai_links[DAI_LINK_ETDM3_OUT_BE].num_codecs = 1;
		mt8188_sunstone_dai_links[DAI_LINK_ETDM3_OUT_BE].codecs->name = NULL;
		mt8188_sunstone_dai_links[DAI_LINK_ETDM3_OUT_BE].codecs->dai_name =
			"i2s-hifi";
		mt8188_sunstone_dai_links[DAI_LINK_ETDM3_OUT_BE].init =
			mt8188_snd_hdmi_codec_init;
	}

	priv->dp_codec_node = of_parse_phandle(pdev->dev.of_node,
						 "mediatek,dptx-codec", 0);
	if (!priv->dp_codec_node) {
		dev_dbg(&pdev->dev, "No property 'dptx-codec'\n");
	} else {
		mt8188_sunstone_dai_links[DAI_LINK_DPTX_BE].codecs->of_node =
			priv->dp_codec_node;
		mt8188_sunstone_dai_links[DAI_LINK_DPTX_BE].num_codecs = 1;
		mt8188_sunstone_dai_links[DAI_LINK_DPTX_BE].codecs->name = NULL;
		mt8188_sunstone_dai_links[DAI_LINK_DPTX_BE].codecs->dai_name =
			"i2s-hifi";
		mt8188_sunstone_dai_links[DAI_LINK_DPTX_BE].init =
			mt8188_snd_dp_codec_init;
	}
#endif
#if IS_ENABLED(CONFIG_MTK_HDMI_RX)
	mt8188_sunstone_dai_links[DAI_LINK_MULTI_IN1_BE].be_hw_params_fixup =
		mt8188_snd_multi_in_hw_params_fixup;
#endif

	ret = register_trace_android_vh_snd_soc_card_get_comp_chain(
			mt8188_snd_component_chaining_hook, NULL);
	if (ret) {
		dev_dbg(&pdev->dev, "get_comp_chain register fail\n");
		goto err_get_comp_chain;
	}

	snd_soc_card_set_drvdata(card, priv);

	mt8188_snd_gpio_probe(card);

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret) {
		dev_dbg(&pdev->dev, "%s snd_soc_register_card fail %d\n",
			__func__, ret);
		goto err_register_card;
	}

	dev_info(&pdev->dev, "%s()-, success\n", __func__);

	return ret;

err_register_card:
	unregister_trace_android_vh_snd_soc_card_get_comp_chain(
		mt8188_snd_component_chaining_hook, NULL);
err_get_comp_chain:
#if IS_ENABLED(CONFIG_SND_SOC_HDMI_CODEC)
	of_node_put(priv->dp_codec_node);
	of_node_put(priv->hdmi_codec_node);
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	of_node_put(priv->adsp_platform_node);
err_adsp_platform:
#endif
	of_node_put(priv->platform_node);

	return ret;
}

static int mt8188_snd_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct mt8188_sunstone_priv *priv = snd_soc_card_get_drvdata(card);

#if IS_ENABLED(CONFIG_SND_SOC_HDMI_CODEC)
	of_node_put(priv->dp_codec_node);
	of_node_put(priv->hdmi_codec_node);
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	of_node_put(priv->adsp_platform_node);
#endif
	of_node_put(priv->platform_node);

	unregister_trace_android_vh_snd_soc_card_get_comp_chain(
		mt8188_snd_component_chaining_hook, NULL);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt8188_sunstone_dt_match[] = {
	{.compatible = "mediatek,mt8188-sound",},
	{}
};
#endif

static struct platform_driver mt8188_sunstone_driver = {
	.driver = {
		.name = "mt8188-sound",
#ifdef CONFIG_OF
		.of_match_table = mt8188_sunstone_dt_match,
#endif
#if !IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
		.pm = &snd_soc_pm_ops,
#endif
	},
	.probe = mt8188_snd_dev_probe,
	.remove = mt8188_snd_dev_remove,
};

module_platform_driver(mt8188_sunstone_driver);

/* Module information */
MODULE_DESCRIPTION("MT8188 sunstone ALSA SoC machine driver");
MODULE_AUTHOR("Chun-Chia Chiu <chun-chia.chiu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt8188 sunstone soc card");

