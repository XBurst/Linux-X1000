/*
 * Ingenic JZ4780 SoC CGU driver
 *
 * Copyright (c) 2013-2015 Imagination Technologies
 * Author: Paul Burton <paul.burton@mips.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>//
#include <linux/delay.h>//
#include <linux/of.h>//
#include <dt-bindings/clock/x1000-cgu.h>//
#include "cgu.h"//

/* CGU register offsets */
#define CGU_REG_CPCCR		0x00//
#define CGU_REG_APLL		0x10//
#define CGU_REG_MPLL		0x14//
#define CGU_REG_CLKGR		0x20//
#define CGU_REG_OPCR		0x24//
#define CGU_REG_DDRCDR		0x2c//
#define CGU_REG_CPSPR		0x34//
#define CGU_REG_CPSPPR		0x38//
#define CGU_REG_USBPCR		0x3c//
#define CGU_REG_USBRDT		0x40//
#define CGU_REG_USBVBFIL	0x44//
#define CGU_REG_USBPCR1		0x48//
#define CGU_REG_USBCDR		0x50//
#define CGU_REG_MACPHYCDR	0x54//
#define CGU_REG_I2SCDR		0x60//
#define CGU_REG_LPCDR		0x64//
#define CGU_REG_MSC0CDR		0x68//
#define CGU_REG_I2SCDR1		0x70//
#define CGU_REG_SSICDR		0x74//
#define CGU_REG_CIMCDR		0x7c//
#define CGU_REG_PCMCDR		0x84//
#define CGU_REG_MSC1CDR		0xa4//
#define CGU_REG_CMP_INTR	0xb0//
#define CGU_REG_CMP_INTRE	0xb4//
#define CGU_REG_DRCG		0xd0//
#define CGU_REG_CLOCKSTATUS	0xd4//
#define CGU_REG_PCMCDR1		0xe0//
#define CGU_REG_MACPHYC		0xe8//

/* bits within the OPCR register */
#define OPCR_SPENDN0		(1 << 7)//
#define OPCR_SPENDN1		(1 << 6)//

/* bits within the USBPCR register */
#define USBPCR_USB_MODE		BIT(31)//
#define USBPCR_IDPULLUP_MASK	(0x3 << 28)//
#define USBPCR_COMMONONN	BIT(25)//
#define USBPCR_VBUSVLDEXT	BIT(24)//
#define USBPCR_VBUSVLDEXTSEL	BIT(23)//
#define USBPCR_POR		BIT(22)//
#define USBPCR_OTG_DISABLE	BIT(20)//
#define USBPCR_COMPDISTUNE_MASK	(0x7 << 17)//
#define USBPCR_OTGTUNE_MASK	(0x7 << 14)//
#define USBPCR_SQRXTUNE_MASK	(0x7 << 11)//
#define USBPCR_TXFSLSTUNE_MASK	(0xf << 7)//
#define USBPCR_TXPREEMPHTUNE	BIT(6)//
#define USBPCR_TXHSXVTUNE_MASK	(0x3 << 4)//
#define USBPCR_TXVREFTUNE_MASK	0xf//

/* bits within the USBPCR1 register */
#define USBPCR1_REFCLKSEL_SHIFT	26//
#define USBPCR1_REFCLKSEL_MASK	(0x3 << USBPCR1_REFCLKSEL_SHIFT)//
#define USBPCR1_REFCLKSEL_CORE	(0x2 << USBPCR1_REFCLKSEL_SHIFT)//
#define USBPCR1_REFCLKDIV_SHIFT	24//
#define USBPCR1_REFCLKDIV_MASK	(0x3 << USBPCR1_REFCLKDIV_SHIFT)//
#define USBPCR1_REFCLKDIV_48	(0x2 << USBPCR1_REFCLKDIV_SHIFT)//
#define USBPCR1_REFCLKDIV_24	(0x1 << USBPCR1_REFCLKDIV_SHIFT)//
#define USBPCR1_REFCLKDIV_12	(0x0 << USBPCR1_REFCLKDIV_SHIFT)//
#define USBPCR1_WORD_IF0	BIT(19)//

/* bits within the USBRDT register */
#define USBRDT_VBFIL_LD_EN	BIT(25)//
#define USBRDT_USBRDT_MASK	0x7fffff//

/* bits within the USBVBFIL register */
#define USBVBFIL_IDDIGFIL_SHIFT	16//
#define USBVBFIL_IDDIGFIL_MASK	(0xffff << USBVBFIL_IDDIGFIL_SHIFT)//
#define USBVBFIL_USBVBFIL_MASK	(0xffff)//

static struct ingenic_cgu *cgu;//

static const s8 pll_od_encoding[8] = {//??
	0x0, 0x1, -1, 0x2, -1, -1, -1, 0x3,//??
};//??

static const struct ingenic_cgu_clk_info x1000_cgu_clocks[] = {//

	/* External clocks */

	[X1000_CLK_EXCLK] = { "ext", CGU_CLK_EXT },/////
	[X1000_CLK_RTCLK] = { "rtc", CGU_CLK_EXT },/////

	/* PLLs */

	[X1000_CLK_APLL] = {//
		"apll", CGU_CLK_PLL,//
		.parents = { X1000_CLK_EXCLK, -1, -1, -1 },//
		.pll = {//
			.reg = CGU_REG_APLL,//
			.m_shift = 24,//
			.m_bits = 7,//
			.m_offset = 1,//
			.n_shift = 18,//
			.n_bits = 5,//
			.n_offset = 1,//
			.od_shift = 16,//
			.od_bits = 2,//
			.od_max = 8,//
			.od_encoding = pll_od_encoding,//
			.bypass_bit = 9,//
			.enable_bit = 8,//
			.stable_bit = 10,//
		},
	},//

	[X1000_CLK_MPLL] = {//
		"mpll", CGU_CLK_PLL,//
		.parents = { X1000_CLK_EXCLK, -1, -1, -1 },//
		.pll = {//
			.reg = CGU_REG_MPLL,//
			.m_shift = 24,//
			.m_bits = 7,//
			.m_offset = 1,//
			.n_shift = 18,//
			.n_bits = 5,//
			.n_offset = 1,//
			.od_shift = 16,//
			.od_bits = 2,//
			.od_max = 8,//
			.od_encoding = pll_od_encoding,//
			.enable_bit = 7,//
			.stable_bit = 6,//
			.no_bypass_bit = true,//
		},
	},//

	/* Muxes & dividers */

	[X1000_CLK_SCLKA] = {//
		"sclk_a", CGU_CLK_MUX,//
		.parents = { -1, X1000_CLK_EXCLK, X1000_CLK_APLL, -1 },//
		.mux = { CGU_REG_CPCCR, 30, 2 },//??
	},//

	[X1000_CLK_CPUMUX] = {//
		"cpumux", CGU_CLK_MUX,//
		.parents = { -1, X1000_CLK_SCLKA, X1000_CLK_MPLL, -1 },//
		.mux = { CGU_REG_CPCCR, 28, 2 },//??
	},//

	[X1000_CLK_CPU] = {//
		"cpu", CGU_CLK_DIV,//
		.parents = { X1000_CLK_CPUMUX, -1, -1, -1 },//
		.div = { CGU_REG_CPCCR, 0, 1, 4, 22, -1, -1 },//
	},//

	[X1000_CLK_L2CACHE] = {//
		"l2cache", CGU_CLK_DIV,//
		.parents = { X1000_CLK_CPUMUX, -1, -1, -1 },//
		.div = { CGU_REG_CPCCR, 4, 1, 4, -1, -1, -1 },//
	},//

	[X1000_CLK_AHB0] = {//
		"ahb0", CGU_CLK_MUX | CGU_CLK_DIV,//
		.parents = { -1, X1000_CLK_SCLKA, X1000_CLK_MPLL, -1 },//
		.mux = { CGU_REG_CPCCR, 26, 2 },//
		.div = { CGU_REG_CPCCR, 8, 1, 4, 21, -1, -1 },//
	},//

	[X1000_CLK_AHB2PMUX] = {//
		"ahb2_apb_mux", CGU_CLK_MUX,//
		.parents = { -1, X1000_CLK_SCLKA, X1000_CLK_MPLL, -1 },//
		.mux = { CGU_REG_CPCCR, 24, 2 },//
	},//

	[X1000_CLK_AHB2] = {//
		"ahb2", CGU_CLK_DIV,//
		.parents = { X1000_CLK_AHB2PMUX, -1, -1, -1 },
		.div = { CGU_REG_CPCCR, 12, 1, 4, 20, -1, -1 },//
	},//

	[X1000_CLK_PCLK] = {//
		"pclk", CGU_CLK_DIV,//
		.parents = { X1000_CLK_AHB2PMUX, -1, -1, -1 },//
		.div = { CGU_REG_CPCCR, 16, 1, 4, 20, -1, -1 },//
	},//

	[X1000_CLK_DDR] = {//
		"ddr", CGU_CLK_MUX | CGU_CLK_DIV | CGU_CLK_GATE,//
		.parents = { -1, X1000_CLK_SCLKA, X1000_CLK_MPLL, -1 },//
		.mux = { CGU_REG_DDRCDR, 30, 2 },//
		.div = { CGU_REG_DDRCDR, 0, 1, 4, 29, 28, 27 },//??25/26?
		.gate = { CGU_REG_CLKGR, 31 },
	},//

	[X1000_CLK_MSCMUX] = {
		"msc_mux", CGU_CLK_MUX,
		.parents = { X1000_CLK_SCLKA, X1000_CLK_MPLL},
		.mux = { CGU_REG_MSC0CDR, 31, 1 },
	},

	[X1000_CLK_MSC0] = {
		"msc0", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { X1000_CLK_MSCMUX, -1, -1, -1 },
		.div = { CGU_REG_MSC0CDR, 0, 2, 8, 29, 28, 27 },
		.gate = { CGU_REG_CLKGR, 4 },
	},

	[X1000_CLK_MSC1] = {
		"msc1", CGU_CLK_DIV | CGU_CLK_GATE,
		.parents = { X1000_CLK_MSCMUX, -1, -1, -1 },
		.div = { CGU_REG_MSC1CDR, 0, 2, 8, 29, 28, 27 },
		.gate = { CGU_REG_CLKGR, 5 },
	},

	/* Gate-only clocks */

	[X1000_CLK_UART0] = {//
		"uart0", CGU_CLK_GATE,//
		.parents = { X1000_CLK_EXCLK, -1, -1, -1 },//
		.gate = { CGU_REG_CLKGR, 14 },//
	},//

	[X1000_CLK_UART1] = {//
		"uart1", CGU_CLK_GATE,//
		.parents = { X1000_CLK_EXCLK, -1, -1, -1 },//
		.gate = { CGU_REG_CLKGR, 15 },//
	},//

	[X1000_CLK_UART2] = {//
		"uart2", CGU_CLK_GATE,//
		.parents = { X1000_CLK_EXCLK, -1, -1, -1 },//
		.gate = { CGU_REG_CLKGR, 16 },//
	},//

	[X1000_CLK_PDMA] = {
		"pdma", CGU_CLK_GATE,
		.parents = { X1000_CLK_EXCLK, -1, -1, -1 },
		.gate = { CGU_REG_CLKGR, 21 },
	},
};

static void __init x1000_cgu_init(struct device_node *np)//
{//
	int retval;//

	cgu = ingenic_cgu_new(x1000_cgu_clocks,//
			      ARRAY_SIZE(x1000_cgu_clocks), np);//
	if (!cgu) {//
		pr_err("%s: failed to initialise CGU\n", __func__);//
		return;//
	}//

	retval = ingenic_cgu_register_clocks(cgu);//
	if (retval) {//
		pr_err("%s: failed to register CGU Clocks\n", __func__);//
		return;//
	}//
}//
CLK_OF_DECLARE(x1000_cgu, "ingenic,x1000-cgu", x1000_cgu_init);//
