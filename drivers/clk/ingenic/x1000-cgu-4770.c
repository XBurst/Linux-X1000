// SPDX-License-Identifier: GPL-2.0
/*
 * JZ4770 SoC CGU driver
 * Copyright 2018, Paul Cercueil <paul@crapouillou.net>
 */

#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/syscore_ops.h>
#include <dt-bindings/clock/jz4770-cgu.h>
#include "cgu.h"

/*
 * CPM registers offset address definition
 */
#define CGU_REG_CPCCR		0x00
#define CGU_REG_LCR		0x04
#define CGU_REG_CPPCR0		0x10
#define CGU_REG_CLKGR0		0x20
#define CGU_REG_OPCR		0x24
#define CGU_REG_CPPCR1		0x14
#define CGU_REG_USBPCR1		0x48
#define CGU_REG_USBCDR		0x50
#define CGU_REG_I2SCDR		0x60
#define CGU_REG_LPCDR		0x64
#define CGU_REG_MSC0CDR		0x68
#define CGU_REG_UHCCDR		0x6c
#define CGU_REG_SSICDR		0x74
#define CGU_REG_CIMCDR		0x7c
#define CGU_REG_GPSCDR		0x80
#define CGU_REG_PCMCDR		0x84
#define CGU_REG_GPUCDR		0x88
#define CGU_REG_MSC1CDR		0xA4

/* bits within the LCR register */
#define LCR_LPM			BIT(0)		/* Low Power Mode */

/* bits within the OPCR register */
#define OPCR_SPENDH		BIT(5)		/* UHC PHY suspend */

/* bits within the USBPCR1 register */
#define USBPCR1_UHC_POWER	BIT(5)		/* UHC PHY power down */

static struct ingenic_cgu *cgu;

static const s8 pll_od_encoding[8] = {
	0x0, 0x1, -1, 0x2, -1, -1, -1, 0x3,
};

static const struct ingenic_cgu_clk_info jz4770_cgu_clocks[] = {

	/* External clocks */

	[JZ4770_CLK_EXT] = { "ext", CGU_CLK_EXT },
	[JZ4770_CLK_OSC32K] = { "osc32k", CGU_CLK_EXT },

	/* PLLs */

	[JZ4770_CLK_PLL0] = {
		"pll0", CGU_CLK_PLL,
		.parents = { JZ4770_CLK_EXT },
		.pll = {
			.reg = CGU_REG_CPPCR0,
			.m_shift = 24,
			.m_bits = 7,
			.m_offset = 1,
			.n_shift = 18,
			.n_bits = 5,
			.n_offset = 1,
			.od_shift = 16,
			.od_bits = 2,
			.od_max = 8,
			.od_encoding = pll_od_encoding,
			.bypass_bit = 9,
			.enable_bit = 8,
			.stable_bit = 10,
		},
	},

	[JZ4770_CLK_PLL1] = {
		/* TODO: PLL1 can depend on PLL0 */
		"pll1", CGU_CLK_PLL,
		.parents = { JZ4770_CLK_EXT },
		.pll = {
			.reg = CGU_REG_CPPCR1,
			.m_shift = 24,
			.m_bits = 7,
			.m_offset = 1,
			.n_shift = 18,
			.n_bits = 5,
			.n_offset = 1,
			.od_shift = 16,
			.od_bits = 2,
			.od_max = 8,
			.od_encoding = pll_od_encoding,
			.enable_bit = 7,
			.stable_bit = 6,
			.no_bypass_bit = true,
		},
	},

	/* Main clocks */

	[JZ4770_CLK_CCLK] = {
		"cclk", CGU_CLK_DIV,
		.parents = { JZ4770_CLK_PLL0, },
		.div = { CGU_REG_CPCCR, 0, 1, 4, 22, -1, -1 },
	},
	[JZ4770_CLK_H0CLK] = {
		"h0clk", CGU_CLK_DIV,
		.parents = { JZ4770_CLK_PLL0, },
		.div = { CGU_REG_CPCCR, 4, 1, 4, 22, -1, -1 },
	},
	[JZ4770_CLK_H2CLK] = {
		"h2clk", CGU_CLK_DIV,
		.parents = { JZ4770_CLK_PLL0, },
		.div = { CGU_REG_CPCCR, 16, 1, 4, 22, -1, -1 },
	},
	[JZ4770_CLK_C1CLK] = {
		"c1clk", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { JZ4770_CLK_PLL0, },
		.div = { CGU_REG_CPCCR, 12, 1, 4, 22, -1, -1 },
		.gate = { CGU_REG_OPCR, 31, true }, // disable CCLK stop on idle
	},
	[JZ4770_CLK_PCLK] = {
		"pclk", CGU_CLK_DIV,
		.parents = { JZ4770_CLK_PLL0, },
		.div = { CGU_REG_CPCCR, 8, 1, 4, 22, -1, -1 },
	},

	/* Those divided clocks can connect to PLL0 or PLL1 */


	/* Those divided clocks can connect to EXT, PLL0 or PLL1 */


	/* Gate-only clocks */

	[JZ4770_CLK_UART0] = {
		"uart0", CGU_CLK_GATE,
		.parents = { JZ4770_CLK_EXT, },
		.gate = { CGU_REG_CLKGR0, 15 },
	},
	[JZ4770_CLK_UART1] = {
		"uart1", CGU_CLK_GATE,
		.parents = { JZ4770_CLK_EXT, },
		.gate = { CGU_REG_CLKGR, 16 },
	},
	[X1000_CLK_UART2] = {
		"uart2", CGU_CLK_GATE,
		.parents = { X1000_CLK_EXT, },
		.gate = { CGU_REG_CLKGR, 17 },
	},

	/* Custom clocks */

	[JZ4770_CLK_EXT512] = {
		"ext/512", CGU_CLK_FIXDIV,
		.parents = { JZ4770_CLK_EXT },
		.fixdiv = { 512 },
	},

	[JZ4770_CLK_RTC] = {
		"rtc", CGU_CLK_MUX,
		.parents = { JZ4770_CLK_EXT512, JZ4770_CLK_OSC32K, },
		.mux = { CGU_REG_OPCR, 2, 1},
	},
};

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int jz4770_cgu_pm_suspend(void)
{
	u32 val;

	val = readl(cgu->base + CGU_REG_LCR);
	writel(val | LCR_LPM, cgu->base + CGU_REG_LCR);
	return 0;
}

static void jz4770_cgu_pm_resume(void)
{
	u32 val;

	val = readl(cgu->base + CGU_REG_LCR);
	writel(val & ~LCR_LPM, cgu->base + CGU_REG_LCR);
}

static struct syscore_ops jz4770_cgu_pm_ops = {
	.suspend = jz4770_cgu_pm_suspend,
	.resume = jz4770_cgu_pm_resume,
};
#endif /* CONFIG_PM_SLEEP */

static void __init jz4770_cgu_init(struct device_node *np)
{
	int retval;

	cgu = ingenic_cgu_new(jz4770_cgu_clocks,
			      ARRAY_SIZE(jz4770_cgu_clocks), np);
	if (!cgu)
		pr_err("%s: failed to initialise CGU\n", __func__);

	retval = ingenic_cgu_register_clocks(cgu);
	if (retval)
		pr_err("%s: failed to register CGU Clocks\n", __func__);

#if IS_ENABLED(CONFIG_PM_SLEEP)
	register_syscore_ops(&jz4770_cgu_pm_ops);
#endif
}

/* We only probe via devicetree, no need for a platform driver */
CLK_OF_DECLARE(jz4770_cgu, "ingenic,jz4770-cgu", jz4770_cgu_init);
