// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <asm-generic/io.h>

#include <linux/amlogic/cpu_version.h>
#include <soc/amlogic/aml_ddr_pmu.h>

#define DMC_QOS_IRQ                     BIT(30)

#define DMC_MON_G12_CTRL0		(0x20  << 2)
#define DMC_MON_G12_CTRL1		(0x21  << 2)
#define DMC_MON_G12_CTRL2		(0x22  << 2)
#define DMC_MON_G12_CTRL3		(0x23  << 2)
#define DMC_MON_G12_CTRL4		(0x24  << 2)
#define DMC_MON_G12_CTRL5		(0x25  << 2)
#define DMC_MON_G12_CTRL6		(0x26  << 2)
#define DMC_MON_G12_CTRL7		(0x27  << 2)
#define DMC_MON_G12_CTRL8		(0x28  << 2)

#define DMC_MON_G12_ALL_REQ_CNT		(0x29  << 2)
#define DMC_MON_G12_ALL_GRANT_CNT	(0x2a  << 2)
#define DMC_MON_G12_ONE_GRANT_CNT	(0x2b  << 2)
#define DMC_MON_G12_SEC_GRANT_CNT	(0x2c  << 2)
#define DMC_MON_G12_THD_GRANT_CNT	(0x2d  << 2)
#define DMC_MON_G12_FOR_GRANT_CNT	(0x2e  << 2)
#define DMC_MON_G12_TIMER		(0x2f  << 2)

#define DMC_VERSION			(0x05  << 2)

static const u32 dmc_version_supported_list[] = {0xa0002, 0x1000003};

static void dmc_g12_set_timer(struct dmc_hw_info *info)
{
	unsigned long clock_count = info->timer_value;

	pr_debug("ddr clock is %lu\n", clock_count);

	/* set timer trigger clock_cnt 1s*/
	writel(clock_count, info->ddr_reg + DMC_MON_G12_TIMER);
}

static void dmc_g12_counter_enable(struct dmc_hw_info *info)
{
	unsigned int val;
	unsigned long clk = readl(info->ddr_reg + DMC_MON_G12_TIMER);

	/* timer maybe modified by ddr bandwidth, reset it */
	if (clk != info->timer_value)
		writel(info->timer_value, info->ddr_reg + DMC_MON_G12_TIMER);

	/* enable all channel */
	val =  (0x01 << 31) |	/* enable bit */
	       (0x01 << 20) |	/* use timer  */
	       (0x0f <<  0);
	writel(val, info->ddr_reg + DMC_MON_G12_CTRL0);
}

static void dmc_g12_config_fiter(struct dmc_hw_info *info,
		int port, int channel);

static void dmc_g12_counter_disable(struct dmc_hw_info *info)
{
	unsigned int val;
	int i;

	/* clear counters */
	val = readl(info->ddr_reg + DMC_MON_G12_CTRL0);
	writel(val, info->ddr_reg + DMC_MON_G12_CTRL0);

	/* clear port channal mapping */
	for (i = 0; i < info->chann_nr; i++)
		dmc_g12_config_fiter(info, -1, i);
}

static void dmc_g12_config_fiter(struct dmc_hw_info *info,
		int port, int channel)
{
	unsigned int val;
	unsigned int rp[MAX_CHANNEL] = {DMC_MON_G12_CTRL1, DMC_MON_G12_CTRL3,
					DMC_MON_G12_CTRL5, DMC_MON_G12_CTRL7};
	unsigned int rs[MAX_CHANNEL] = {DMC_MON_G12_CTRL2, DMC_MON_G12_CTRL4,
					DMC_MON_G12_CTRL6, DMC_MON_G12_CTRL8};
	int subport = -1;

	/* clear all port mask */
	if (port < 0) {
		writel(0, info->ddr_reg + rp[channel]);
		writel(0, info->ddr_reg + rs[channel]);
		return;
	}

	if (port >= PORT_MAJOR)
		subport = port - PORT_MAJOR;

	if (subport < 0) {
		val = readl(info->ddr_reg + rp[channel]);
		val |=  (1 << port);
		writel(val, info->ddr_reg + rp[channel]);
		val = 0xffff;
		writel(val, info->ddr_reg + rs[channel]);
	} else {
		val = (0x1 << 23);	/* select device */
		writel(val, info->ddr_reg + rp[channel]);
		val = readl(info->ddr_reg + rs[channel]);
		val |= (1 << subport);
		writel(val, info->ddr_reg + rs[channel]);
	}
}

static void dmc_g12_get_counters(struct dmc_hw_info *info,
					struct ddr_grant_info *dg)
{
	int i;
	unsigned int reg;

	dg->all_grant = readl(info->ddr_reg + DMC_MON_G12_ALL_GRANT_CNT);
	dg->all_req   = readl(info->ddr_reg + DMC_MON_G12_ALL_REQ_CNT);
	for (i = 0; i < info->chann_nr; i++) {
		reg = DMC_MON_G12_ONE_GRANT_CNT + (i << 2);
		dg->channel_grant[i] = readl(info->ddr_reg + reg);
	}
}

static unsigned long dmc_g12_get_freq_quick(struct dmc_hw_info *info)
{
	unsigned int val;
	unsigned int n, m, od1;
	unsigned int od_div = 0xfff;
	unsigned long freq = 0;

	val = readl(info->pll_reg);
	val = val & 0xfffff;
	switch ((val >> 16) & 7) {
	case 0:
		od_div = 2;
		break;

	case 1:
		od_div = 3;
		break;

	case 2:
		od_div = 4;
		break;

	case 3:
		od_div = 6;
		break;

	case 4:
		od_div = 8;
		break;

	default:
		break;
	}

	m = val & 0x1ff;
	n = ((val >> 10) & 0x1f);
	od1 = (((val >> 19) & 0x1)) == 1 ? 2 : 1;
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	if (n)
		freq = ((((freq * m) / n) >> od1) / od_div) * 1000;

	return freq;
}

static int dmc_g12_irq_identify(struct dmc_hw_info *info)
{
	/* check if ddr bandwidth enable the irq
	 * ddr bandwidth set dmc with high freq
	 */
	if (info->timer_value != readl(info->ddr_reg + DMC_MON_G12_TIMER))
		return 0;

	return 1;
}

static int dmc_g12_irq_handler(struct dmc_hw_info *info,
				struct ddr_grant_info *dg)
{
	unsigned int val;
	int ret = -1;

	val = readl(info->ddr_reg + DMC_MON_G12_CTRL0);
	if (val & DMC_QOS_IRQ) {
		dmc_g12_get_counters(info, dg);
		/* clear irq flags */
		writel(val, info->ddr_reg + DMC_MON_G12_CTRL0);
		ret = 0;
	}
	return ret;
}

static struct dmc_pmu_hw_ops g12_ops = {
	.enable		= dmc_g12_counter_enable,
	.disable	= dmc_g12_counter_disable,
	.irq_handler	= dmc_g12_irq_handler,
	.irq_identify	= dmc_g12_irq_identify,
	.get_counters	= dmc_g12_get_counters,
	.config_port	= dmc_g12_config_fiter,
};

/* the event list which t7 supports */
static const int event_support[] = {
	CYCLE_COUNTER_ID,
	ALL_CHAN_COUNTER_ID,
	CHAN1_COUNTER_ID,
	CHAN2_COUNTER_ID,
	CHAN3_COUNTER_ID,
	CHAN4_COUNTER_ID,
};

struct dmc_pmu_hw_ops *dmc_g12_pmu_init(struct dmc_hw_info *info)
{
	unsigned int i;
	unsigned int version = readl(info->ddr_reg + DMC_VERSION);

	pr_info("readl(info->ddr_reg + DMC_VERSION) %x\n", version);

	for (i = 0; i < ARRAY_SIZE(dmc_version_supported_list); i++)
		if (dmc_version_supported_list[i] == version) {
			pr_info("find the supported dmc %x\n", version);
			break;
		}

	if (i == ARRAY_SIZE(dmc_version_supported_list))
		return ERR_PTR(-ENODEV);

	info->chann_nr = 4;

	for (i = 0; i < ARRAY_SIZE(event_support); i++)
		info->event_flag[event_support[i]] = EVENT_SUPPORT;

	info->timer_value = dmc_g12_get_freq_quick(info);

	dmc_g12_set_timer(info);

	dmc_g12_counter_disable(info);

	return &g12_ops;
}
