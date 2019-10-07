#ifndef __INGENIC_CLK_PLLV1_H__
#define __INGENIC_CLK_PLLV1_H__
#include "clk-comm.h"


extern const struct clk_ops ingenic_pll_ro_ops;

#define PLL_DESC(_regoff, _m, _ml, _n, _nl, _od, _odl, _en, _on, _bs, _od_code, _table, _step)	\
{	\
	.regoff = _regoff, \
	.m_sft = _m, \
	.m_width = _ml,	\
	.n_sft = _n, \
	.n_width = _nl,	\
	.od_sft = _od, \
	.od_width = _odl, \
	.en_bit = _en, \
	.on_bit = _on, \
	.od_encode = _od_code,	\
	.bs_bit	= _bs,	\
	.table = _table,	\
	.div_step = _step	\
}

struct ingenic_pll_hwdesc {
	u32 regoff;
	u8 m_sft;
	u8 m_width;
	u8 n_sft;
	u8 n_width;
	u8 od_sft;
	u8 od_width;
	u8 on_bit;
	u8 en_bit;
	s8 bs_bit;
	const s8 *od_encode;		/*od rules, -1 not support*/
	const void *table;		/*clk rate table, -r not support*/
	u8 div_step;
};

#endif /*__INGENIC_CLK_PLLV1_H__*/
