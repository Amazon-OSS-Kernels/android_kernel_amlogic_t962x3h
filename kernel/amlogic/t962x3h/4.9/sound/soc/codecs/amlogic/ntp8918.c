// SPDX-License-Identifier: GPL-2.0
/*
 * ALSA SoC ntp8918 codec driver
 *
 * Copyright (C) 2021 Amlogic,inc
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include "ntp8918.h"

#define DRV_NAME "ntp8918"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void ntp8918_early_suspend(struct early_suspend *h);
static void ntp8918_late_resume(struct early_suspend *h);
#endif

#define NTP8918_RATES (SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | \
	SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_96000)

#define NTP8918_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

#define NTP8918_DRC_PARAM_COUNT 29
#define NTP8919_EQ_LENGTH  58

char m_init_sequence[] = {
	0x0B, 0x01, // Soft Reset contorl
	0x02, 0x00, // MCLK ( 48k = 0x00, 96k = 0x01, 36k=0x02 )
	0x33, 0x03, // Soft Mute On Control
};

/* codec private data */
struct ntp8918_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct ntp8918_platform_data *pdata;

	int mute;
	unsigned char Ch1_vol;
	unsigned char Ch2_vol;
	unsigned char master_vol;
	int eq_enable;
	int drc_enable;
	unsigned char master_clk;
	char *m_reg_tab;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static int ntp8918_mute_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static void ntp8918_mute(struct snd_soc_codec *codec, int mute)
{
	if (mute) {
		snd_soc_write(codec, 0x33, 0x03);
		snd_soc_write(codec, 0x34, 0x03);
		snd_soc_write(codec, 0x35, 0x06);
	} else {
		snd_soc_write(codec, 0x35, 0x04);
		snd_soc_write(codec, 0x34, 0x00);
		snd_soc_write(codec, 0x33, 0x00);
	}
}

static int ntp8918_mute_locked_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);

	ntp8918->mute = ucontrol->value.integer.value[0];
	ntp8918_mute(codec, ntp8918->mute);

	return 0;
}

static int ntp8918_mute_locked_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = ntp8918->mute;

	return 0;
}

static int ntp8918_set_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);

	ntp8918->eq_enable = ucontrol->value.integer.value[0];

	if (ntp8918->eq_enable) {
		snd_soc_write(codec, 0x0E, 0x14);
		snd_soc_write(codec, 0x0F, 0x14);
		snd_soc_write(codec, 0x10, 0x07);
		snd_soc_write(codec, 0x11, 0x07);
		snd_soc_write(codec, 0x12, 0x55);
		snd_soc_write(codec, 0x13, 0x55);
		snd_soc_write(codec, 0x14, 0xF5);
		snd_soc_write(codec, 0x15, 0xF5);
	} else {
		snd_soc_write(codec, 0x0E, 0x00);
		snd_soc_write(codec, 0x0F, 0x00);
		snd_soc_write(codec, 0x10, 0x00);
		snd_soc_write(codec, 0x11, 0x00);
		snd_soc_write(codec, 0x12, 0x00);
		snd_soc_write(codec, 0x13, 0x00);
		snd_soc_write(codec, 0x14, 0x00);
		snd_soc_write(codec, 0x15, 0x00);
	}

	return 0;
}

static int ntp8918_get_EQ_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	bool enable = (bool)ntp8918->eq_enable & 0x1;

	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static int ntp8918_set_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);

	ntp8918->drc_enable = ucontrol->value.integer.value[0];

	return 0;
}

static int ntp8918_get_DRC_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	bool enable = (bool)ntp8918->drc_enable & 0x1;

	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static int ntp8918_set_DRC_param(struct snd_kcontrol *kcontrol,
				  const unsigned int __user *bytes,
				  unsigned int size)
{
	return 0;
}

static int ntp8918_get_DRC_param(struct snd_kcontrol *kcontrol,
			    unsigned int __user *bytes,
			    unsigned int size)
{
	return 0;
}


static int ntp8918_set_EQ_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int ntp8918_get_EQ_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int ntp8918_set_reg(struct snd_kcontrol *kcontrol,
			    const unsigned int __user *bytes,
			    unsigned int size)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	char *p = ntp8918->m_reg_tab;
	int ret = 0, i = 0;
	bool i2c_4byte_on = false;

	ret = copy_from_user(p, val, size);
	if (ret)
		return -EFAULT;

	while (i < size) {
		if (p[0] == 0x7e)
			i2c_4byte_on = false;

		if (!i2c_4byte_on) {
			snd_soc_write(codec, p[0], p[1]);
			i += 2;
		} else {
			regmap_raw_write(ntp8918->regmap, p[0],
				p+1, 4);
			i += 5;
		}

		if (p[0] == 0x7e && (p[1] == 0x03 || p[1] == 0x08))
			i2c_4byte_on = true;
		else if (p[0] == 0x7e && (p[1] == 0x00))
			i2c_4byte_on = false;
		p = ntp8918->m_reg_tab + i;
	}

	return 0;
}

static int ntp8918_get_reg(struct snd_kcontrol *kcontrol,
			    unsigned int __user *bytes,
			    unsigned int size)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	char *p = ntp8918->m_reg_tab;
	int ret = 0;

	ret = copy_to_user(val, p, size);
	if (ret)
		return -EFAULT;

	return 0;
}

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -12550, 50, 0);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -7900, 50, 0);

static const struct snd_kcontrol_new ntp8918_snd_controls[] = {
	SOC_SINGLE_TLV("Master Volume", MVOL, 0,
		0xff, 0, mvol_tlv),
	SOC_SINGLE_TLV("Ch1 Volume", C1VOL, 0,
		0xff, 0, chvol_tlv),
	SOC_SINGLE_TLV("Ch2 Volume", C2VOL, 0,
		0xff, 0, chvol_tlv),
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Maser Volume Mute",
		.info = ntp8918_mute_info,
		.put = ntp8918_mute_locked_put,
		.get = ntp8918_mute_locked_get,
	},
	SOC_SINGLE_BOOL_EXT("Set EQ Enable", 0,
			   ntp8918_get_EQ_enum, ntp8918_set_EQ_enum),
	SOC_SINGLE_BOOL_EXT("Set DRC Enable", 0,
			   ntp8918_get_DRC_enum, ntp8918_set_DRC_enum),
	SND_SOC_BYTES_EXT("EQ table", NTP8919_EQ_LENGTH,
			   ntp8918_get_EQ_param, ntp8918_set_EQ_param),
	SND_SOC_BYTES_TLV("DRC table", NTP8918_DRC_PARAM_COUNT,
			   ntp8918_get_DRC_param, ntp8918_set_DRC_param),
	SND_SOC_BYTES_TLV("Reg table", NTP8918_REGISTER_COUNT,
			ntp8918_get_reg, ntp8918_set_reg),
};

static int ntp8918_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);
	unsigned char master_clk = 0x00;
	int rate = params_rate(params);

	if (rate == 32000)
		master_clk = 0x10;
	else if (rate == 96000)
		master_clk = 0x01;
	else
		master_clk = 0x00;

	if (master_clk != ntp8918->master_clk) {
		ntp8918->master_clk = master_clk;
		snd_soc_write(codec, MCLK, ntp8918->master_clk);
	}
	return 0;
}

static int ntp8918_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int ntp8918_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return 0;
	}

	return 0;
}

static int ntp8918_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	pr_debug("level = %d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	codec->component.dapm.bias_level = level;

	return 0;
}

static int ntp8918_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *codec_dai)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			if (!ntp8918->mute)
				ntp8918_mute(codec, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			if (!ntp8918->mute)
				ntp8918_mute(codec, 1);
			break;
		}
	}
	return 0;
}

static const struct snd_soc_dai_ops ntp8918_dai_ops = {
	.hw_params  = ntp8918_hw_params,
	.set_sysclk = ntp8918_set_dai_sysclk,
	.set_fmt    = ntp8918_set_dai_fmt,
	.trigger    = ntp8918_trigger,
};

static struct snd_soc_dai_driver ntp8918_dai = {
	.name     = DRV_NAME,
	.playback = {
		.stream_name  = "HIFI Playback",
		.channels_min = 2,
		.channels_max = 16,
		.rates        = NTP8918_RATES,
		.formats      = NTP8918_FORMATS,
	},
	.ops      = &ntp8918_dai_ops,
};

static int ntp8918_GPIO_enable(struct snd_soc_codec *codec, bool enable)
{
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);
	struct ntp8918_platform_data *pdata = ntp8918->pdata;

	if (pdata->reset_pin <= 0)
		return 0;

	if (enable) {
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		usleep_range(10 * 1000, 11 * 1000);
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
		dev_info(codec->dev, "ntp8918 start status = %d\n",
			gpio_get_value(pdata->reset_pin));
		usleep_range(1 * 1000, 2 * 1000);
	} else {
		gpio_set_value(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		dev_info(codec->dev, "ntp8918 stop status = %d\n",
			gpio_get_value(pdata->reset_pin));
		usleep_range(1 * 1000, 2 * 1000);
	}

	return 0;
}

static int ntp8918_reg_init(struct snd_soc_codec *codec)
{
	int i = 0;
	bool i2c_4byte_on = false;
	struct ntp8918_priv *ntp8918 = snd_soc_codec_get_drvdata(codec);
	int reg_len = sizeof(m_init_sequence);
	char *p = m_init_sequence;

	while (i < reg_len) {
		if (p[0] == 0x7e)
			i2c_4byte_on = false;

		if (!i2c_4byte_on) {
			snd_soc_write(codec, p[0], p[1]);
			i += 2;
		} else {
			regmap_raw_write(ntp8918->regmap, p[0], p+1, 4);
			i += 5;
		}

		if (p[0] == 0x7e && (p[1] == 0x03 || p[1] == 0x08))
			i2c_4byte_on = true;
		else if (p[0] == 0x7e && (p[1] == 0x00))
			i2c_4byte_on = false;

		p = m_init_sequence + i;
	}

	return 0;
}

static int ntp8918_init(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);

	ntp8918_GPIO_enable(codec, true);

	dev_info(codec->dev, "%s\n", __func__);

	ntp8918_reg_init(codec);

	ntp8918->master_clk = 0;//for 48K or 44.1k;

	/* set default volume*/ /*unmute */
	ntp8918->Ch1_vol = ntp8918->Ch2_vol = 0xFF;//0db
	snd_soc_write(codec, C1VOL, ntp8918->Ch1_vol);
	snd_soc_write(codec, C2VOL, ntp8918->Ch2_vol);
	ntp8918->master_vol = 0xf0;//-7.5db
	snd_soc_write(codec, MVOL, ntp8918->master_vol);
	/*unmute */
	ntp8918->eq_enable = 1;
	ntp8918->mute = 0;
	ntp8918_mute(codec, ntp8918->mute);

	return 0;
}

static int ntp8918_probe(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);
	struct ntp8918_platform_data *pdata = ntp8918->pdata;
	int ret = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ntp8918->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ntp8918->early_suspend.suspend = ntp8918_early_suspend;
	ntp8918->early_suspend.resume = ntp8918_late_resume;
	ntp8918->early_suspend.param = codec;
	register_early_suspend(&ntp8918->early_suspend);
#endif

	if (pdata->reset_pin > 0) {
		ret = devm_gpio_request_one(codec->dev, pdata->reset_pin,
						GPIOF_OUT_INIT_LOW,
						"ntp8918-reset-pin");

		if (ret < 0) {
			dev_err(codec->dev, "ntp8918 get gpio error!\n");
			return -1;
		}
	}

	ntp8918_init(codec);

	return 0;
}

static int ntp8918_remove(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);
	struct ntp8918_platform_data *pdata = ntp8918->pdata;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ntp8918->early_suspend);
#endif

	devm_gpio_free(codec->dev, pdata->reset_pin);

	return 0;
}

#ifdef CONFIG_PM
static int ntp8918_suspend(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s!\n", __func__);

	/* save volume */
	ntp8918->Ch1_vol = snd_soc_read(codec, C1VOL);
	ntp8918->Ch2_vol = snd_soc_read(codec, C2VOL);
	ntp8918->master_vol = snd_soc_read(codec, MVOL);

	ntp8918_mute(codec, 1);
	usleep_range(1 * 1000, 2 * 1000);
	ntp8918_GPIO_enable(codec, false);
	return 0;
}

static int ntp8918_resume(struct snd_soc_codec *codec)
{
	struct ntp8918_priv *ntp8918 =
		snd_soc_codec_get_drvdata(codec);

	dev_info(codec->dev, "%s!\n", __func__);

	ntp8918_GPIO_enable(codec, true);

	ntp8918_reg_init(codec);

	snd_soc_write(codec, C1VOL, ntp8918->Ch1_vol);
	snd_soc_write(codec, C2VOL, ntp8918->Ch2_vol);
	snd_soc_write(codec, MVOL, ntp8918->master_vol);
	ntp8918_mute(codec, ntp8918->mute);
	return 0;
}
#else
#define ntp8918_suspend NULL
#define ntp8918_resume NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ntp8918_early_suspend(struct early_suspend *h)
{
}

static void ntp8918_late_resume(struct early_suspend *h)
{
}
#endif

static const struct snd_soc_dapm_widget ntp8918_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HIFI Playback", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_codec_driver soc_codec_dev_ntp8918 = {
	.probe            = ntp8918_probe,
	.remove           = ntp8918_remove,
	.suspend          = ntp8918_suspend,
	.resume           = ntp8918_resume,
	.set_bias_level   = ntp8918_set_bias_level,
	.component_driver = {
		.controls         = ntp8918_snd_controls,
		.num_controls     = ARRAY_SIZE(ntp8918_snd_controls),
		.dapm_widgets     = ntp8918_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(ntp8918_dapm_widgets),
	}
};


static bool ntp8918_reg_is_volatile(struct device *dev, unsigned int reg)
{
	return reg >= 0x00 && reg <= 0x7E;
}

static const struct regmap_config ntp8918_regmap = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.cache_type       = REGCACHE_RBTREE,
	.volatile_reg = ntp8918_reg_is_volatile,
};

static int ntp8918_parse_dt(struct ntp8918_priv *ntp8918,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		ret = -1;
	} else {
		pr_info("%s pdata->reset_pin = %d!\n", __func__,
			reset_pin);
	}
	ntp8918->pdata->reset_pin = reset_pin;

	return ret;
}

static int ntp8918_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct ntp8918_priv *ntp8918;
	struct ntp8918_platform_data *pdata;
	int ret;

	ntp8918 = devm_kzalloc(&i2c->dev, sizeof(struct ntp8918_priv),
		GFP_KERNEL);
	if (!ntp8918)
		return -ENOMEM;

	ntp8918->regmap = devm_regmap_init_i2c(i2c, &ntp8918_regmap);
	if (IS_ERR(ntp8918->regmap)) {
		ret = PTR_ERR(ntp8918->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	i2c_set_clientdata(i2c, ntp8918);

	pdata = devm_kzalloc(&i2c->dev,
		sizeof(struct ntp8918_platform_data),
		GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for ntp8918 pdata\n", __func__);
		return -ENOMEM;
	}
	ntp8918->pdata = pdata;

	ntp8918_parse_dt(ntp8918, i2c->dev.of_node);

	ret = snd_soc_register_codec(&i2c->dev,
		&soc_codec_dev_ntp8918,
		&ntp8918_dai, 1);

	ntp8918->m_reg_tab =
		kzalloc(sizeof(char) * NTP8918_REGISTER_COUNT,
			GFP_KERNEL);
	if (!ntp8918->m_reg_tab)
		return -ENOMEM;

	memset(ntp8918->m_reg_tab, 0, NTP8918_REGISTER_COUNT);

	if (ret != 0)
		dev_err(&i2c->dev, "%s, Failed to register codec (%d)\n", __func__, ret);

	return ret;
}

static int ntp8918_i2c_remove(struct i2c_client *client)
{
	struct ntp8918_priv *ntp8918 =
		(struct ntp8918_priv *)i2c_get_clientdata(client);

	if (ntp8918)
		kfree(ntp8918->m_reg_tab);

	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id ntp8918_i2c_id[] = {
	{ "ntp8918", 0 },
	{}
};

static const struct of_device_id ntp8918_of_id[] = {
	{ .compatible = "NTP, ntp8918", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, ntp8918_of_id);

static struct i2c_driver ntp8918_i2c_driver = {
	.driver   = {
		.name           = "ntp8918",
		.owner          = THIS_MODULE,
		.of_match_table = ntp8918_of_id,
	},
	.probe = ntp8918_i2c_probe,
	.remove = ntp8918_i2c_remove,
	.id_table = ntp8918_i2c_id,
};

module_i2c_driver(ntp8918_i2c_driver);

MODULE_DESCRIPTION("ASoC ntp8918 driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");
