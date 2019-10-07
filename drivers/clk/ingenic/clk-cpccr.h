#ifndef __INGENIC_CLK_CPCCR_H__
#define __INGENIC_CLK_CPCCR_H__
#include "clk-comm.h"

extern const struct clk_ops ingenic_cpccr_rate_ro_ops;
extern const struct clk_ops ingenic_cpccr_mux_ro_ops;

#define CPCCR_MUX_RODESC(_ctrloff, _cs, _csm)	\
	CPCCR_DESC(_ctrloff, _cs, _csm, 0, 0, 0, 0, 0, 0, 0, 0)

#define CPCCR_RATE_RODESC(_ctrloff, _div, _divm)	\
	CPCCR_DESC(_ctrloff, 0, 0, 0, _div, _divm, 0, 0, 0, 0, 0)

#define CPCCR_DESC(_ctrloff, _cs, _csm, _ce, _div, _divm, _div1, _div1m, _stoff, _busy, _csb)	\
{	\
	.ctrloff = _ctrloff, \
	.cs_off = _cs,	\
	.cs_msk = _csm,	\
	.bit_ce = _ce,	\
	.div_off = _div,	\
	.div_msk = _divm,	\
	.div1_off = _div1,	\
	.div1_msk = _div1m,	\
	.statoff = _stoff,	\
	.bit_rate_busy = _busy, \
	.bit_cs_stable = _csb,	\
}

struct ingenic_cpccr_hwdesc {
	u32 ctrloff;	/*Clock Control Register*/
	u8 cs_off;	/*Clock Source select shift in register*/
	u8 cs_msk;	/*Clock Source select mask(no shifted)*/
	u8 bit_ce;	/*clock freq Change Enable bit*/
	u8 div_off;	/*clock rate DIV shift in register*/
	u16 div_msk;	/*clock rate DIV mask(no shifted)*/
	s8 div1_off;	/*associated div1, like l2div associate with cdiv, pdiv associate with h2div,
			  -1 means no associated div*/
	s8 div1_msk;
	u32 statoff;	/*Clock Status Register*/
	u8 bit_rate_busy;  /*clock Rate change Busy bit*/
	s8 bit_cs_stable;  /*clock Clock Source change statble, -1 means no stable*/
};
#endif /*__INGENIC_CLK_CPCCR_H__*/
