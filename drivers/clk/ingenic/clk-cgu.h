#ifndef __INGENIC_CLK_CGU_H__
#define __INGENIC_CLK_CGU_H__
#include "clk-comm.h"

extern const struct clk_ops ingenic_cgu_mux_ops;
extern const struct clk_ops ingenic_cgu_ops;
extern const struct clk_ops ingenic_cgu_mux_ro_ops;
extern const struct clk_ops ingenic_cgu_nomux_ops;
extern const struct clk_ops ingenic_cgu_ro_ops;

/*reg, sel, selmsk, ce, busy ,stop, cdr, cdrmsk, parenttable, step, isnoglitch*/
#define CGU_DESC(_regoff, _cs, _csm, _ce, _busy, _stop, _cdr, _cdrm, _step)	\
{	\
	.regoff = _regoff, \
	.cs_off = _cs,	\
	.cs_msk = _csm,	\
	.bit_ce = _ce,	\
	.bit_stop = _stop,	\
	.bit_busy = _busy,	\
	.cdr_off = _cdr,	\
	.cdr_msk = _cdrm,	\
	.div_step = _step,	\
	.cs_exclk = -1,	\
}

/*use exclk_cs see struct ingenic_cgu_hwdesc*/
#define CGU_DESC_E(_regoff, _cs, _csm, _ce, _busy, _stop, _cdr, _cdrm, _step, _cs_exclk)	\
{	\
	.regoff = _regoff, \
	.cs_off = _cs,	\
	.cs_msk = _csm,	\
	.bit_ce = _ce,	\
	.bit_stop = _stop,	\
	.bit_busy = _busy,	\
	.cdr_off = _cdr,	\
	.cdr_msk = _cdrm,	\
	.div_step = _step,	\
	.cs_exclk = _cs_exclk,	\
}

struct ingenic_cgu_hwdesc {
	u32 regoff;
	s8 cs_off;
	u8 cs_msk;
	u8 bit_ce;
	u8 bit_stop;
	u8 bit_busy;
	u16 cdr_msk;
	u16 cdr_off;
	int div_step;
	int cs_exclk;     /*
			   * Some cgu clk, like usb,
			   * When they use exclk, they can stop the cgu for save power.
			   * if the cgu support this action above, fill the exclk cs here
			   * otherwise -1, if unsure -1.
			   */
};

int ingenic_cgu_set_parent(struct clk_hw *hw, u8 index);
u8 ingenic_cgu_get_parent(struct clk_hw *hw);
long ingenic_cgu_round_rate(struct clk_hw *hw, unsigned long rate, unsigned long *parent_rate);
unsigned long ingenic_cgu_recalc_rate(struct clk_hw *hw, unsigned long parent_rate);
int ingenic_cgu_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate);
int ingenic_cgu_enable(struct clk_hw *hw);
void ingenic_cpu_disable(struct clk_hw *hw);
int ingenic_cgu_is_enabled(struct clk_hw *hw);
#endif /*__INGENIC_CLK_CGU_H__*/
