/*
 * Copyright (C) 2016 Ingenic Semiconductor Co., Ltd.
 * Author: cli <chen.li@ingenic.com>
 *
 * X1000 Clock (kernel.4.4)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk-provider.h>
#include <dt-bindings/clock/ingenic-x1000.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/mfd/syscon.h>
#include <soc/cpm.h>

#include <linux/module.h>

#include "clk-comm.h"
#include "clk-pll.h"
#include "clk-cpccr.h"
#include "clk-cgu.h"
#include "clk-gate.h"

static const char *clk_name[NR_CLKS] = {
	[CLK_EXT] = "ext",
	[CLK_RTC] = "rtc",
	[CLK_PLL_APLL] = "apll",
	[CLK_PLL_MPLL] = "mpll",
	[CLK_MUX_SCLKA] = "sclka_mux",
	[CLK_MUX_CPLL] = "cpu_mux",
	[CLK_MUX_H0PLL] = "ahb0_mux",
	[CLK_MUX_H2PLL] = "ahb2_mux",
	[CLK_RATE_CPUCLK] = "cpu",
	[CLK_RATE_L2CCLK] = "l2cache",
	[CLK_RATE_H0CLK] = "ahb0",
	[CLK_RATE_H2CLK] = "ahb2",
	[CLK_RATE_PCLK] = "pclk",
	[CLK_CGU_DDR] = "cgu_ddr",
	[CLK_CGU_MAC] = "cgu_mac",
	[CLK_CGU_LCD] = "cgu_lcd",
	[CLK_CGU_MSC0] = "cgu_msc0",
	[CLK_CGU_MSC1] = "cgu_msc1",
	[CLK_CGU_USB] = "cgu_usb",
	[CLK_CGU_SFC] = "cgu_sfc",
	[CLK_CGU_SSI] = "cgu_ssi",
	[CLK_CGU_CIM] = "cgu_cim",
	[CLK_CGU_I2S] = "cgu_i2s",
	[CLK_CGU_PCM] = "cgu_pcm",
	[CLK_GATE_NEMC] = "gate_nemc",
	[CLK_GATE_EFUSE] = "gate_efuse",
	[CLK_GATE_SFC  ] = "gate_sfc",
	[CLK_GATE_OTG  ] = "gate_otg",
	[CLK_GATE_MSC0 ] = "gate_msc0",
	[CLK_GATE_MSC1 ] = "gate_msc1",
	[CLK_GATE_SCC  ] = "gate_scc",
	[CLK_GATE_I2C0 ] = "gate_i2c0",
	[CLK_GATE_I2C1 ] = "gate_i2c1",
	[CLK_GATE_I2C2 ] = "gate_i2c2",
	[CLK_GATE_I2C3 ] = "gate_i2c3",
	[CLK_GATE_AIC  ] = "gate_aic",
	[CLK_GATE_JPEG ] = "gate_jpeg",
	[CLK_GATE_SADC ] = "gate_sadc",
	[CLK_GATE_UART0] = "gate_uart0",
	[CLK_GATE_UART1] = "gate_uart1",
	[CLK_GATE_UART2] = "gate_uart2",
	[CLK_GATE_DMIC ] = "gate_dmic",
	[CLK_GATE_TCU  ] = "gate_tcu",
	[CLK_GATE_SSI  ] = "gate_ssi0",
	[CLK_GATE_OST  ] = "gate_ost",
	[CLK_GATE_PDMA ] = "gate_pdma",
	[CLK_GATE_CIM  ] = "gate_cim",
	[CLK_GATE_LCD  ] = "gate_lcd",
	[CLK_GATE_AES  ] = "gate_aes",
	[CLK_GATE_MAC  ] = "gate_mac",
	[CLK_GATE_PCM  ] = "gate_pcm",
	[CLK_GATE_RTC  ] = "gate_rtc",
	[CLK_GATE_APB0 ] = "gate_apb",
	[CLK_GATE_AHB0 ] = "gate_ahb0",
	[CLK_GATE_CPU  ] = "gate_cpu",
	[CLK_GATE_DDR  ] = "gate_ddr",
	[CLK_GATE_SCLKA] = "sclka",
	[CLK_GATE_USBPHY] = "gate_usbphy",
	[CLK_GATE_SCLKABUS] = "sclka_bus",
};

/*PLL HWDESC*/
static const s8 pll_od_encode[4] = {1, 2, 4, 8};
static struct ingenic_pll_hwdesc apll_hwdesc = \
	PLL_DESC(CPM_CPAPCR, 24, 7, 18, 5, 16, 2, 8, 10, 31, pll_od_encode, NULL, 1);

static struct ingenic_pll_hwdesc mpll_hwdesc = \
	PLL_DESC(CPM_CPMPCR, 24, 7, 18, 5, 16, 2, 7, 0, 31, pll_od_encode, NULL, 1);

/*CPCCR PARENTS*/
static const int sclk_a_p[] = { DUMMY_STOP, CLK_EXT, CLK_PLL_APLL, DUMMY_UNKOWN };
static const int cpccr_p[] = { DUMMY_STOP, CLK_MUX_SCLKA, CLK_PLL_MPLL, DUMMY_UNKOWN };

/*CPCCR HWDESC*/
#define INDEX_CPCCR_HWDESC(_id)  ((_id) - CLK_ID_CPCCR)
static struct ingenic_cpccr_hwdesc cpccr_hwdesc[] = {
	[INDEX_CPCCR_HWDESC(CLK_MUX_SCLKA)] = CPCCR_MUX_RODESC(CPM_CPCCR, 30, 0x3),	/*sclk a*/
	[INDEX_CPCCR_HWDESC(CLK_MUX_CPLL)] = CPCCR_MUX_RODESC(CPM_CPCCR, 28, 0x3),	/*cpll*/
	[INDEX_CPCCR_HWDESC(CLK_MUX_H0PLL)] = CPCCR_MUX_RODESC(CPM_CPCCR, 26, 0x3),	/*h0pll*/
	[INDEX_CPCCR_HWDESC(CLK_MUX_H2PLL)] = CPCCR_MUX_RODESC(CPM_CPCCR, 24, 0x3),	/*h2pll*/
	[INDEX_CPCCR_HWDESC(CLK_RATE_PCLK)] = CPCCR_RATE_RODESC(CPM_CPCCR, 16, 0xf),	/*pdiv*/
	[INDEX_CPCCR_HWDESC(CLK_RATE_H2CLK)] = CPCCR_RATE_RODESC(CPM_CPCCR, 12, 0xf),	/*h2div*/
	[INDEX_CPCCR_HWDESC(CLK_RATE_H0CLK)] = CPCCR_RATE_RODESC(CPM_CPCCR, 8, 0xf),	/*h0div*/
	[INDEX_CPCCR_HWDESC(CLK_RATE_L2CCLK)] = CPCCR_RATE_RODESC(CPM_CPCCR, 4, 0xf),	/*l2cdiv*/
	[INDEX_CPCCR_HWDESC(CLK_RATE_CPUCLK)] = CPCCR_RATE_RODESC(CPM_CPCCR, 0, 0xf),	/*cdiv*/
};
#define X1000_CPCCR_HWDESC(_id) &cpccr_hwdesc[INDEX_CPCCR_HWDESC((_id))]

/*CGU PARENTS*/
static const int cgu_ddr[] = { DUMMY_STOP, CLK_MUX_SCLKA, CLK_PLL_MPLL, DUMMY_UNKOWN };
/*mac, lcd pixel, msc, cim mclk, sfc*/
static const int cgu_sel_grp0[] = { CLK_MUX_SCLKA, CLK_PLL_MPLL };
static const int cgu_i2s[] = { CLK_EXT, CLK_MUX_SCLKA, CLK_EXT, CLK_PLL_MPLL };
static const int const cgu_usb[] = { CLK_EXT, CLK_EXT, CLK_MUX_SCLKA, CLK_PLL_MPLL };
static const int const cgu_ssi[] = { CLK_EXT, CLK_CGU_SFC };
static const int cgu_pcm[] = { CLK_MUX_SCLKA, CLK_EXT, CLK_PLL_MPLL, DUMMY_UNKOWN };

/*CGU HWDESC*/
static struct ingenic_cgu_hwdesc cgu_hwdesc[] = {
	/*reg, sel, selmsk, ce, busy ,stop, cdr, cdrmsk, parenttable, step*/
	CGU_DESC(CPM_DDRCDR, 30, 0x2, 29, 28, 27, 0, 0xf, 1),	/*ddr*/		/*FIXME ddr now is ro mode*/
	CGU_DESC(CPM_MACCDR, 31, 0x1, 29, 28, 27, 0, 0xff, 1),	/*mac*/
	CGU_DESC(CPM_LPCDR, 31, 0x1, 29, 27, 26, 0, 0xff, 1),	/*lcd pixel*/
	CGU_DESC(CPM_MSC0CDR, 31, 0x1, 29, 28, 27, 0, 0xff, 2),	/*msc0*/
	CGU_DESC(CPM_MSC1CDR, -1, 0, 29, 28, 27, 0, 0xff, 2),	/*msc1*/	/*FIXME msc0 sel , msc1 must stop*/
	CGU_DESC_E(CPM_USBCDR, 30, 0x2, 29, 28, 27, 0, 0xff, 1, 0/*EXTCLK sel*/),	/*usb*/

	CGU_DESC(CPM_SFCCDR, 31, 0x1, 29, 28, 27, 0, 0xff, 1),	/*sfc*/
	CGU_DESC(CPM_CIMCDR, 31, 0x1, 29, 28, 27, 0, 0xff, 1),	/*cim mclk*/
	CGU_DESC(CPM_I2SCDR, 30, 0x3, 29, 0, 0, 0, 0, 1),	/*i2s*/
	CGU_DESC(CPM_PCMCDR, 30, 0x3, 29, 0, 0, 0, 0, 1),	/*pcm*/
};

/*SSI special OPS*/
static int ingenic_ssi_set_parent(struct clk_hw *hw, u8 index)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	unsigned long flags;
	uint32_t tmp;
	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	tmp = clkhw_readl(iclk, CPM_SFCCDR);
	tmp &=  ~BIT(30);
	tmp |= (index << 30);
	clkhw_writel(iclk, CPM_SFCCDR, tmp);

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return 0;
}

static u8 ingenic_ssi_get_parent(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	unsigned long flags;
	u8 idx;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	idx = (u8)(!!(clkhw_readl(iclk, CPM_SFCCDR) & BIT(30)));

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);

	return idx;
}

static unsigned long ingenic_ssi_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	unsigned long rate, flags;
	uint32_t tmp;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	tmp = clkhw_readl(iclk, CPM_SFCCDR);
	if (tmp & BIT(30))
		rate = parent_rate/2;
	else
		rate = parent_rate;

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return rate;
}

static const struct clk_ops ingenic_ssi_mux_ops = {
	.set_parent = ingenic_ssi_set_parent,
	.get_parent = ingenic_ssi_get_parent,
	.recalc_rate = ingenic_ssi_recalc_rate,
};

static unsigned long ingenic_audio_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	return 24000000UL;
}

static int ingenic_audio_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	return 0;
}

static long ingenic_audio_round_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long *parent_rate)
{
	return 24000000l;
}

static const struct clk_ops ingenic_audio_ops = {
	.set_parent = ingenic_cgu_set_parent,
	.get_parent = ingenic_cgu_get_parent,
	.recalc_rate = ingenic_audio_recalc_rate,
	.round_rate = ingenic_audio_round_rate,
	.set_rate = ingenic_audio_set_rate,
};

u8 ingenic_msc0_get_parent(struct clk_hw *hw)
{
	struct ingenic_clk *iclk = to_ingenic_clk(hw);
	uint32_t xcdr;
	unsigned long flags;
	u8 cs;

	if (iclk->spinlock)
		spin_lock_irqsave(iclk->spinlock, flags);
	else
		__acquire(iclk->spinlock);

	xcdr = clkhw_readl(iclk, 0x68);

	cs = (xcdr >> 31);

	if (iclk->spinlock)
		spin_unlock_irqrestore(iclk->spinlock, flags);
	else
		__release(iclk->spinlock);
	return cs;
}

const struct clk_ops ingenic_msc1_ro_ops = {
	.get_parent = ingenic_msc0_get_parent,
	.enable = ingenic_cgu_enable,
	.disable = ingenic_cgu_disable,
	.is_enabled = ingenic_cgu_is_enabled,
	.recalc_rate = ingenic_cgu_recalc_rate,
	.round_rate = ingenic_cgu_round_rate,
	.set_rate = ingenic_cgu_set_rate,
};

static struct ingenic_gate_hwdesc gate_hwdesc[] = {
	GATE_DESC(CPM_CLKGR, 0),
	GATE_DESC(CPM_CLKGR, 1),
	GATE_DESC(CPM_CLKGR, 2),
	GATE_DESC(CPM_CLKGR, 3),
	GATE_DESC(CPM_CLKGR, 4),
	GATE_DESC(CPM_CLKGR, 5),
	GATE_DESC(CPM_CLKGR, 6),
	GATE_DESC(CPM_CLKGR, 7),
	GATE_DESC(CPM_CLKGR, 8),
	GATE_DESC(CPM_CLKGR, 9),
	GATE_DESC(CPM_CLKGR, 10),
	GATE_DESC(CPM_CLKGR, 11),
	GATE_DESC(CPM_CLKGR, 12),
	GATE_DESC(CPM_CLKGR, 13),
	GATE_DESC(CPM_CLKGR, 14),
	GATE_DESC(CPM_CLKGR, 15),
	GATE_DESC(CPM_CLKGR, 16),
	GATE_DESC(CPM_CLKGR, 17),
	GATE_DESC(CPM_CLKGR, 18),
	GATE_DESC(CPM_CLKGR, 19),
	GATE_DESC(CPM_CLKGR, 20),
	GATE_DESC(CPM_CLKGR, 21),
	GATE_DESC(CPM_CLKGR, 22),
	GATE_DESC(CPM_CLKGR, 23),
	GATE_DESC(CPM_CLKGR, 24),
	GATE_DESC(CPM_CLKGR, 25),
	GATE_DESC(CPM_CLKGR, 26),
	GATE_DESC(CPM_CLKGR, 27),
	GATE_DESC(CPM_CLKGR, 28),
	GATE_DESC(CPM_CLKGR, 29),
	GATE_DESC(CPM_CLKGR, 30),
	GATE_DESC(CPM_CLKGR, 31),
	GATE_DESC(CPM_CPCCR, 23),
	GATE_DESC_REGMAP(CPM_OPCR, 23, 1),
	GATE_DESC_REGMAP(CPM_OPCR, 28, 1),
};
#define X1000_GATE_HWDESC(_id)	\
	 &gate_hwdesc[(_id) - CLK_ID_GATE]

#define X1000_CLK_INIT_UNLOCK(_id, _pids, _ops, _flags, _phwdesc)	\
		CLK_INIT_DATA_UNLOCK(_id, _ops, _pids, ARRAY_SIZE(_pids), _flags, _phwdesc)

#define X1000_CLK_INIT_LOCKED(_id, _pids, _ops, _flags, _phwdesc)	\
		CLK_INIT_DATA_LOCKED(_id, _ops, _pids, ARRAY_SIZE(_pids), _flags, _phwdesc)

#define X1000_CLK_INIT1_LOCKED(_id, _pid, _ops, _flags, _phwdesc)	\
		CLK_INIT_DATA1_LOCKED(_id, _ops, _pid, _flags, _phwdesc)

#define X1000_CLK_INIT1_UNLOCK(_id, _pid, _ops, _flags, _phwdesc)	\
		CLK_INIT_DATA1_UNLOCK(_id, _ops, _pid, _flags, _phwdesc)

static const struct ingenic_clk_init __initdata x1000_clk_init_data[] = {
	/*pll*/
	X1000_CLK_INIT1_UNLOCK(CLK_PLL_APLL, CLK_EXT, &ingenic_pll_ro_ops, CLK_IGNORE_UNUSED, &apll_hwdesc),
	X1000_CLK_INIT1_UNLOCK(CLK_PLL_MPLL, CLK_EXT, &ingenic_pll_ro_ops, CLK_IGNORE_UNUSED, &mpll_hwdesc),
	/*cpccr*/
	X1000_CLK_INIT_UNLOCK(CLK_MUX_SCLKA, sclk_a_p, &ingenic_cpccr_mux_ro_ops, 0, X1000_CPCCR_HWDESC(CLK_MUX_SCLKA)),
	X1000_CLK_INIT_UNLOCK(CLK_MUX_CPLL, cpccr_p, &ingenic_cpccr_mux_ro_ops, 0, X1000_CPCCR_HWDESC(CLK_MUX_CPLL)),
	X1000_CLK_INIT_UNLOCK(CLK_MUX_H0PLL, cpccr_p, &ingenic_cpccr_mux_ro_ops, 0, X1000_CPCCR_HWDESC(CLK_MUX_H0PLL)),
	X1000_CLK_INIT_UNLOCK(CLK_MUX_H2PLL, cpccr_p, &ingenic_cpccr_mux_ro_ops, 0, X1000_CPCCR_HWDESC(CLK_MUX_H2PLL)),
	X1000_CLK_INIT1_UNLOCK(CLK_RATE_PCLK, CLK_MUX_H2PLL, &ingenic_cpccr_rate_ro_ops,0, X1000_CPCCR_HWDESC(CLK_RATE_PCLK)),
	X1000_CLK_INIT1_UNLOCK(CLK_RATE_H2CLK, CLK_MUX_H2PLL, &ingenic_cpccr_rate_ro_ops,0, X1000_CPCCR_HWDESC(CLK_RATE_H2CLK)),
	X1000_CLK_INIT1_UNLOCK(CLK_RATE_H0CLK, CLK_MUX_H0PLL, &ingenic_cpccr_rate_ro_ops,0, X1000_CPCCR_HWDESC(CLK_RATE_H0CLK)),
	X1000_CLK_INIT1_UNLOCK(CLK_RATE_CPUCLK, CLK_MUX_CPLL, &ingenic_cpccr_rate_ro_ops,0, X1000_CPCCR_HWDESC(CLK_RATE_CPUCLK)),
	X1000_CLK_INIT1_UNLOCK(CLK_RATE_L2CCLK, CLK_MUX_CPLL, &ingenic_cpccr_rate_ro_ops,0, X1000_CPCCR_HWDESC(CLK_RATE_L2CCLK)),
	/*cgu*/
	X1000_CLK_INIT_LOCKED(CLK_CGU_DDR, cgu_ddr, &ingenic_cgu_ro_ops, CLK_IGNORE_UNUSED, &cgu_hwdesc[0]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_MAC, cgu_sel_grp0, &ingenic_cgu_ops, CLK_SET_PARENT_GATE, &cgu_hwdesc[1]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_LCD, cgu_sel_grp0, &ingenic_cgu_ops, CLK_SET_PARENT_GATE, &cgu_hwdesc[2]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_MSC0, cgu_sel_grp0, &ingenic_cgu_ops, CLK_SET_PARENT_GATE, &cgu_hwdesc[3]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_MSC1, cgu_sel_grp0, &ingenic_msc1_ro_ops, CLK_GET_RATE_NOCACHE, &cgu_hwdesc[4]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_USB, cgu_usb, &ingenic_cgu_ops, CLK_SET_PARENT_GATE, &cgu_hwdesc[5]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_SFC, cgu_sel_grp0, &ingenic_cgu_ops, CLK_SET_PARENT_GATE, &cgu_hwdesc[6]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_CIM, cgu_sel_grp0, &ingenic_cgu_ops, CLK_SET_PARENT_GATE, &cgu_hwdesc[7]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_SSI, cgu_ssi, &ingenic_ssi_mux_ops, 0, NULL),
	X1000_CLK_INIT_LOCKED(CLK_CGU_I2S, cgu_i2s, &ingenic_audio_ops, 0, &cgu_hwdesc[8]),
	X1000_CLK_INIT_LOCKED(CLK_CGU_PCM, cgu_pcm, &ingenic_audio_ops, 0, &cgu_hwdesc[9]),
	/*gate*/
	X1000_CLK_INIT1_LOCKED(CLK_GATE_NEMC, CLK_RATE_H2CLK, &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_NEMC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_EFUSE, CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_EFUSE)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_SFC  , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_SFC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_OTG  , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_OTG)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_MSC0 , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_MSC0)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_MSC1 , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_MSC1)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_SCC  , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_SCC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_I2C0 , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_I2C0)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_I2C1 , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_I2C1)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_I2C2 , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_I2C2)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_I2C3 , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_I2C3)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_AIC  , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_AIC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_JPEG , CLK_RATE_H0CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_JPEG)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_SADC , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_SADC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_UART0, CLK_EXT, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_UART0)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_UART1, CLK_EXT, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_UART1)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_UART2, CLK_EXT, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_UART2)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_DMIC , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_DMIC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_TCU  , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_TCU)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_SSI  , CLK_RATE_PCLK,&ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_SSI)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_OST  , CLK_RATE_L2CCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_OST)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_PDMA , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_PDMA)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_CIM  , CLK_RATE_H0CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_CIM)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_LCD  , CLK_RATE_H0CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_LCD)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_AES  , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_AES)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_MAC  , CLK_RATE_H2CLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_MAC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_PCM  , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_PCM)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_RTC  , CLK_RATE_PCLK, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_RTC)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_APB0 , CLK_RATE_PCLK, &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_APB0)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_AHB0 , CLK_RATE_H0CLK, &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_AHB0)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_CPU  , CLK_RATE_CPUCLK, &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_CPU)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_DDR  , CLK_RATE_H0CLK, &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_DDR)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_SCLKA, CLK_MUX_SCLKA, &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_SCLKA)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_USBPHY, CLK_CGU_USB, &ingenic_gate_ops, 0,
			X1000_GATE_HWDESC(CLK_GATE_USBPHY)),
	X1000_CLK_INIT1_LOCKED(CLK_GATE_SCLKABUS, CLK_MUX_SCLKA,  &ingenic_gate_ops, CLK_IGNORE_UNUSED,
			X1000_GATE_HWDESC(CLK_GATE_SCLKABUS)),
};

void ingenic_gate_clk_associated_init(struct ingenic_clk_provide *ctx)
{
	int i, id;
	struct clk *clk;
	struct ingenic_gate_hwdesc *hwdesc;
	struct ingenic_clk *iclk;


	for (i = 0; i < CLK_NR_GATE; i++) {
		id = CLK_ID_GATE + i;
		clk = ctx->data.clks[id];
		if (!clk)
			continue;
		iclk = to_ingenic_clk(__clk_get_hw(clk));
		hwdesc = iclk->hwdesc;
		if (hwdesc->assoc_id >= CLK_ID_GATE &&
				hwdesc->assoc_id < CLK_ID_GATE + CLK_NR_GATE);
			hwdesc->assoc_clk = ctx->data.clks[hwdesc->assoc_id];
	}
	return;
}

static int x1000_clk_suspend(void)
{
	return 0;
}

static void x1000_clk_resume(void)
{
	return;
}

static struct syscore_ops x1000_syscore_ops = {
	.suspend = x1000_clk_suspend,
	.resume = x1000_clk_resume,
};

static void __init x1000_clk_init(struct device_node *np)
{
	struct ingenic_clk_provide *ctx = kzalloc(sizeof(struct ingenic_clk_provide), GFP_ATOMIC);

	ctx->regbase = of_io_request_and_map(np, 0, "cpm");
	if (!ctx->regbase)
		return;

	ctx->np = np;
	ctx->data.clks = (struct clk**)kzalloc(sizeof(struct clk*) * NR_CLKS, GFP_ATOMIC);
	ctx->data.clk_num = NR_CLKS;
	ctx->clk_name = clk_name;
	ctx->pm_regmap = syscon_node_to_regmap(np);
	if (IS_ERR(ctx->pm_regmap)) {
		pr_err("Cannot find regmap for %s: %ld\n", np->full_name,
				PTR_ERR(ctx->pm_regmap));
		return;
	}

	ingenic_fixclk_register(ctx, clk_name[CLK_EXT], CLK_EXT);
	ingenic_fixclk_register(ctx, clk_name[CLK_RTC], CLK_RTC);

	ingenic_clks_register(ctx, x1000_clk_init_data, ARRAY_SIZE(x1000_clk_init_data));

	ingenic_gate_clk_associated_init(ctx);

	register_syscore_ops(&x1000_syscore_ops);

	if (of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->data))
		panic("could not register clk provider\n");

	{
		char *name[] = {"cpu", "l2cache", "ahb0", "ahb2" , "pclk"};
		unsigned long rate[] = {0, 0, 0, 0, 0};
		int i;
		struct clk *clk;
		for (i = 0; i < 5; i++)  {
			clk = clk_get(NULL, name[i]);
			if (!IS_ERR(clk))
				rate[i] = clk_get_rate(clk);
			clk_put(clk);
			rate[i] /= 1000000;
		}
		printk("CPU:[%dM]L2CACHE:[%dM]AHB0:[%dM]AHB2:[%dM]PCLK:[%dM]\n",
				(int)rate[0], (int)rate[1], (int)rate[2],
				(int)rate[3], (int)rate[4]);
	}

	return;
}
CLK_OF_DECLARE(x1000_clk, "ingenic,x1000-clocks", x1000_clk_init);
