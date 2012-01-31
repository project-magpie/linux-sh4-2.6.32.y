/*****************************************************************************
 *
 * File name   : clock-common.c
 * Description : Low Level API - Common LLA functions (SOC independant)
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
25/nov/11 fabrice.charpentier@st.com
	  Functions rename to support several algos for a same PLL/FS.
28/oct/11 fabrice.charpentier@st.com
	  Added PLL1600 CMOS045 support for Lille
27/oct/11 fabrice.charpentier@st.com
	  PLL1200 functions revisited. API changed.
27/jul/11 fabrice.charpentier@st.com
	  FS660 algo enhancement.
14/mar/11 fabrice.charpentier@st.com
	  Added PLL1200 functions.
07/mar/11 fabrice.charpentier@st.com
	  clk_pll3200c32_get_params() revisited.
11/mar/10 fabrice.charpentier@st.com
	  clk_pll800c65_get_params() fully revisited.
10/dec/09 francesco.virlinzi@st.com
	  clk_pll1600c65_get_params() now same code for OS21 & Linux.
13/oct/09 fabrice.charpentier@st.com
	  clk_fs216c65_get_rate() API changed. Now returns error code.
30/sep/09 fabrice.charpentier@st.com
	  Introducing clk_pll800c65_get_rate() & clk_pll1600c65_get_rate() to
	  replace clk_pll800_freq() & clk_pll1600c65_freq().
*/

#include <linux/clk.h>
#include <asm-generic/div64.h>
#include <linux/clkdev.h>

int __init clk_register_table(struct clk *clks, int num, int enable)
{
	int i;

	for (i = 0; i < num; i++) {
		struct clk *clk = &clks[i];
		int ret;
		struct clk_lookup *cl;

		/*
		 * Some devices have clockgen outputs which are unused.
		 * In this case the LLA may still have an entry in its
		 * tables for that clock, and try and register that clock,
		 * so we need some way to skip it.
		 */
		if (!clk->name)
			continue;

		ret = clk_register(clk);
		if (ret)
			return ret;

		/*
		 * We must ignore the result of clk_enables as some of
		 * the LLA enables functions claim to support an
		 * enables function, but then fail if you call it!
		 */
		if (enable) {
			ret = clk_enable(clk);
			if (ret)
				pr_warning("Failed to enable clk %s, "
					   "ignoring\n", clk->name);
		}

		cl = clkdev_alloc(clk, clk->name, NULL);
		if (!cl)
			return -ENOMEM;
		clkdev_add(cl);
	}

	return 0;
}


/*
 * Linux specific function
 */

/* Return the number of set bits in x. */
static unsigned int population(unsigned int x)
{
	/* This is the traditional branch-less algorithm for population count */
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = x + (x << 8);
	x = x + (x << 16);

	return x >> 24;
}

/* Return the index of the most significant set in x.
 * The results are 'undefined' is x is 0 (0xffffffff as it happens
 * but this is a mere side effect of the algorithm. */
static unsigned int most_significant_set_bit(unsigned int x)
{
	/* propagate the MSSB right until all bits smaller than MSSB are set */
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);

	/* now count the number of set bits [clz is population(~x)] */
	return population(x) - 1;
}

#include "clock-oslayer.h"
#include "clock-common.h"


/*
 * PLL800
 */

/* ========================================================================
   Name:	clk_pll800c65_get_params()
   Description: Freq to parameters computation for PLL800 CMOS65
   Input:       input & output freqs (Hz)
   Output:      updated *mdiv, *ndiv & *pdiv (register values)
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * PLL800 in FS mode computation algo
 *
 *             2 * N * Fin Mhz
 * Fout Mhz = -----------------		[1]
 *                M * (2 ^ P)
 *
 * Rules:
 *   6.25Mhz <= output <= 800Mhz
 *   FS mode means 3 <= N <= 255
 *   1 <= M <= 255
 *   1Mhz <= PFDIN (input/M) <= 50Mhz
 *   200Mhz <= FVCO (input*2*N/M) <= 800Mhz
 *   For better long term jitter select M minimum && P maximum
 */

int clk_pll800c65_get_params(unsigned long input, unsigned long output,
	unsigned long *mdiv, unsigned long *ndiv, unsigned long *pdiv)
{
	unsigned long m, n, pfdin, fvco;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation, pi;

	/* Output clock range: 6.25Mhz to 800Mhz */
	if (output < 6250000 || output > 800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (pi = 5; pi >= 0 && deviation; pi--) {
		for (m = 1; (m < 255) && deviation; m++) {
			n = m * (1 << pi) * output / (input * 2);

			/* Checks */
			if (n < 3)
				continue;
			if (n > 255)
				break;
			pfdin = input / m; /* 1Mhz <= PFDIN <= 50Mhz */
			if (pfdin < 1000 || pfdin > 50000)
				continue;
			/* 200Mhz <= FVCO <= 800Mhz */
			fvco = (input * 2 * n) / m;
			if (fvco > 800000)
				continue;
			if (fvco < 200000)
				break;

			new_freq = (input * 2 * n) / (m * (1 << pi));
			new_deviation = new_freq - output;
			if (new_deviation < 0)
				new_deviation = -new_deviation;
			if (!new_deviation || new_deviation < deviation) {
				*mdiv	= m;
				*ndiv	= n;
				*pdiv	= pi;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll800c65_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:      'input' freq (Hz), mdiv/ndiv/pvid values
   Output:      '*rate' updated
   Return:      Error code.
   ======================================================================== */

int clk_pll800c65_get_rate(unsigned long input, unsigned long mdiv,
	unsigned long ndiv, unsigned long pdiv, unsigned long *rate)
{
	if (!mdiv)
		mdiv++; /* mdiv=0 or 1 => MDIV=1 */

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((2 * (input/1000) * ndiv) / mdiv) / (1 << pdiv)) * 1000;

	return 0;
}

/*
 * PLL1200
 */

/* ========================================================================
   Name:	clk_pll1200c32_get_params()
   Description: PHI freq to parameters computation for PLL1200.
   Input:       input=input freq (Hz),output=output freq (Hz)
		WARNING: Output freq is given for PHI (FVCO/ODF).
   Output:      updated *idf, *ldf, & *odf
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 *   FVCO >> Divider (ODF) >> PHI
 *
 * PHI = (INFF * LDF) / (ODF * IDF) when BYPASS = L
 *
 * Rules:
 *   9.6Mhz <= input (INFF) <= 350Mhz
 *   600Mhz <= FVCO <= 1200Mhz
 *   9.52Mhz <= PHI output <= 1200Mhz
 *   1 <= i (register value for IDF) <= 7
 *   8 <= l (register value for LDF) <= 127
 *   1 <= odf (register value for ODF) <= 63
 */

int clk_pll1200c32_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ldf,
			   unsigned long *odf)
{
	unsigned long i, l, o; /* IDF, LDF, ODF values */
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;

	/* Output clock range: 9.52Mhz to 1200Mhz */
	if (output < 9520000 || output > 1200000000)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing Output Division Factor */
	if (output < 600000000) {
		o = 600000000 / output;
		if (600000000 % output)
			o = o + 1;
	} else
		o = 1;

	input /= 1000;
	output /= 1000;

	for (i = 1; (i <= 7) && deviation; i++) {
		l = i * output * o / input;

		/* Checks */
		if (l < 8)
			continue;
		if (l > 127)
			break;

		new_freq = (input * l) / (i * o);
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*idf = i;
			*ldf = l;
			*odf = o;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll1200c32_get_rate()
   Description: Convert input/idf/ldf/odf values to PHI output freq.
		WARNING: Assuming NOT BYPASS.
   Params:      'input' freq (Hz), idf/ldf/odf REGISTERS values
   Output:      '*rate' updated with value of PHI output (FVCO/ODF).
   Return:      Error code.
   ======================================================================== */

int clk_pll1200c32_get_rate(unsigned long input, unsigned long idf,
			    unsigned long ldf, unsigned long odf,
			    unsigned long *rate)
{
	if (!idf)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((input / 1000) * ldf) / (odf * idf)) * 1000;

	return 0;
}

/*
 * PLL1600
 * WARNING: 2 types currently supported; CMOS065 & CMOS045
 */

/* ========================================================================
   Name:	clk_pll1600c45_get_params(), PL1600 CMOS45
   Description: FVCO output freq to parameters computation function.
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *idf, *ndiv and *cp
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * Spec used: CMOS045_PLL_PG_1600X_A_SSCG_FR_LSHOD25_7M4X0Y2Z_SPECS_1.1.pdf
 *
 * Rules:
 *   4Mhz <= input (INFF) <= 350Mhz
 *   800Mhz <= VCO freq (FVCO) <= 1800Mhz
 *   6.35Mhz <= output (PHI) <= 900Mhz
 *   1 <= IDF (Input Div Factor) <= 7
 *   8 <= NDIV (Loop Div Factor) <= 225
 *   1 <= ODF (Output Div Factor) <= 63
 *
 * PHI = (INFF*LDF) / (2*IDF*ODF)
 * FVCO = (INFF*LDF) / (IDF)
 * LDF = 2*NDIV (if FRAC_CONTROL=L)
 * => FVCO = INFF * 2 * NDIV / IDF
 */

int clk_pll1600c45_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv,
			   unsigned long *cp)
{
	unsigned long i, n = 0; /* IDF, NDIV values */
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;
	/* Charge pump table: highest ndiv value for cp=7 to 27 */
	static const unsigned char cp_table[] = {
		71, 79, 87, 95, 103, 111, 119, 127, 135, 143,
		151, 159, 167, 175, 183, 191, 199, 207, 215,
		223, 225
	};

	/* Output clock range: 800Mhz to 1800Mhz */
	if (output < 800000000 || output > 1800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (i = 1; (i <= 7) && deviation; i++) {
		n = (i * output) / (2 * input);

		/* Checks */
		if (n < 8)
			continue;
		if (n > 225)
			break;

		new_freq = (input * 2 * n) / i;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*idf	= i;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	/* Computing recommended charge pump value */
	for (*cp = 7; n > cp_table[*cp - 7]; (*cp)++)
		;

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c45_get_phi_params()
   Description: PLL1600 C45 PHI freq computation function
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *idf, *ndiv, *odf and *cp
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll1600c45_get_phi_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv,
			   unsigned long *odf, unsigned long *cp)
{
	unsigned long o; /* ODF value */

	/* Output clock range: 6.35Mhz to 900Mhz */
	if (output < 6350000 || output > 900000000)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing Output Division Factor */
	if (output < 400000000) {
		o = 400000000 / output;
		if (400000000 % output)
			o = o + 1;
	} else
		o = 1;
	*odf = o;

	return clk_pll1600c45_get_params(input, output, idf, ndiv, cp);
}

/* ========================================================================
   Name:	clk_pll1600c45_get_rate()
   Description: Convert input/idf/ndiv REGISTERS values to FVCO frequency
   Params:      'input' freq (Hz), idf/ndiv REGISTERS values
   Output:      '*rate' updated with value of FVCO output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600c45_get_rate(unsigned long input, unsigned long idf,
			    unsigned long ndiv, unsigned long *rate)
{
	if (!idf)
		return CLK_ERR_BAD_PARAMETER;

	/* FVCO = (INFF*LDF) / (IDF)
	   LDF = 2*NDIV (if FRAC_CONTROL=L)
	   => FVCO = INFF * 2 * NDIV / IDF */

	/* Note: input is divided to avoid overflow */
	*rate = (((input / 1000) * 2 * ndiv) / idf) * 1000;

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c45_get_phi_rate()
   Description: Convert input/idf/ndiv/odf REGISTERS values to frequency
   Params:      'input' freq (Hz), idf/ndiv/odf REGISTERS values
   Output:      '*rate' updated with value of PHI output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600c45_get_phi_rate(unsigned long input, unsigned long idf,
			    unsigned long ndiv, unsigned long odf,
			    unsigned long *rate)
{
	if (!idf || !odf)
		return CLK_ERR_BAD_PARAMETER;

	/* PHI = (INFF*LDF) / (2*IDF*ODF)
	   LDF = 2*NDIV (if FRAC_CONTROL=L)
	   => PHI = (INFF*NDIV) / (IDF*ODF) */

	/* Note: input is divided to avoid overflow */
	*rate = (((input/1000) * ndiv) / (idf * odf)) * 1000;

	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c65_get_params()
   Description: Freq to parameters computation for PLL1600 CMOS65
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv (rdiv) & *ndiv (ddiv)
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * Rules:
 *   600Mhz <= output (FVCO) <= 1800Mhz
 *   1 <= M (also called R) <= 7
 *   4 <= N <= 255
 *   4Mhz <= PFDIN (input/M) <= 75Mhz
 */

int clk_pll1600c65_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv)
{
	unsigned long m, n, pfdin;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;

	/* Output clock range: 600Mhz to 1800Mhz */
	if (output < 600000000 || output > 1800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (m = 1; (m <= 7) && deviation; m++) {
		n = m * output / (input * 2);

		/* Checks */
		if (n < 4)
			continue;
		if (n > 255)
			break;
		pfdin = input / m; /* 4Mhz <= PFDIN <= 75Mhz */
		if (pfdin < 4000 || pfdin > 75000)
			continue;

		new_freq = (input * 2 * n) / m;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*mdiv	= m;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll1600c65_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:      'input' freq (Hz), mdiv/ndiv values
		Info: mdiv also called rdiv, ndiv also called ddiv
   Output:      '*rate' updated with value of HS output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600c65_get_rate(unsigned long input, unsigned long mdiv,
			    unsigned long ndiv, unsigned long *rate)
{
	if (!mdiv)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = ((2 * (input/1000) * ndiv) / mdiv) * 1000;

	return 0;
}

/*
 * PLL3200
 */

/* ========================================================================
   Name:	clk_pll3200c32_get_params()
   Description: Freq to parameters computation for PLL3200 CMOS32
   Input:       input=input freq (Hz), output=FVCOBY2 freq (Hz)
   Output:      updated *idf & *ndiv, plus *cp value (charge pump)
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 * VCO >> /2 >> FVCOBY2
 *                 |> Divider (ODF0) >> PHI0
 *                 |> Divider (ODF1) >> PHI1
 *                 |> Divider (ODF2) >> PHI2
 *                 |> Divider (ODF3) >> PHI3
 *
 * FVCOby2 output = (input*4*NDIV) / (2*IDF) (assuming FRAC_CONTROL==L)
 *
 * Rules:
 *   4Mhz <= input <= 350Mhz
 *   800Mhz <= output (FVCOby2) <= 1600Mhz
 *   1 <= i (register value for IDF) <= 7
 *   8 <= n (register value for NDIV) <= 200
 */

int clk_pll3200c32_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv,
			   unsigned long *cp)
{
	unsigned long i, n = 0;
	unsigned long deviation = 0xffffffff;
	unsigned long new_freq;
	long new_deviation;
	/* Charge pump table: highest ndiv value for cp=6 to 25 */
	static const unsigned char cp_table[] = {
		48, 56, 64, 72, 80, 88, 96, 104, 112, 120,
		128, 136, 144, 152, 160, 168, 176, 184, 192
	};

	/* Output clock range: 800Mhz to 1600Mhz */
	if (output < 800000000 || output > 1600000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	for (i = 1; (i <= 7) && deviation; i++) {
		n = i * output / (2 * input);

		/* Checks */
		if (n < 8)
			continue;
		if (n > 200)
			break;

		new_freq = (input * 2 * n) / i;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*idf	= i;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	/* Computing recommended charge pump value */
	for (*cp = 6; n > cp_table[*cp-6]; (*cp)++)
		;

	return 0;
}

/* ========================================================================
   Name:	clk_pll3200c32_get_rate()
   Description: Convert input/idf/ndiv values to FVCOby2 frequency for PLL3200
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCOby2 output (PHIx / 1).
   Return:      Error code.
   ======================================================================== */

int clk_pll3200c32_get_rate(unsigned long input, unsigned long idf,
			unsigned long ndiv, unsigned long *rate)
{
	if (!idf)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided to avoid overflow */
	*rate = ((2 * (input/1000) * ndiv) / idf) * 1000;

	return 0;
}

/*
 * FS216
 */

/* ========================================================================
   Name:	clk_fs216c65_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types.
   Currently based on C090_4FS216_25. */

int clk_fs216c65_get_params(unsigned long input, unsigned long output,
			    unsigned long *md, unsigned long *pe,
			    unsigned long *sdiv)
{
	unsigned long long p, q;
	unsigned int predivide;
	int preshift; /* always +ve but used in subtraction */
	unsigned int lsdiv;
	int lmd;
	unsigned int lpe = 1 << 14;

	/* pre-divide the frequencies */
	p = 1048576ull * input * 8;    /* <<20? */
	q = output;

	predivide = (unsigned int)div64_u64(p, q);

	/* determine an appropriate value for the output divider using eqn. #4
	 * with md = -16 and pe = 32768 (and round down) */
	lsdiv = predivide / 524288;
	if (lsdiv > 1) {
		/* sdiv = fls(sdiv) - 1; // this doesn't work
		 * for some unknown reason */
		lsdiv = most_significant_set_bit(lsdiv);
	} else
		lsdiv = 1;

	/* pre-shift a common sub-expression of later calculations */
	preshift = predivide >> lsdiv;

	/* determine an appropriate value for the coarse selection using eqn. #5
	 * with pe = 32768 (and round down which for signed values means away
	 * from zero) */
	lmd = ((preshift - 1048576) / 32768) - 1;	 /* >>15? */

	/* calculate a value for pe that meets the output target */
	lpe = -1 * (preshift - 1081344 - (32768 * lmd));  /* <<15? */

	/* finally give sdiv its true hardware form */
	lsdiv--;
	/* special case for 58593.75Hz and harmonics...
	* can't quite seem to get the rounding right */
	if (lmd == -17 && lpe == 0) {
		lmd = -16;
		lpe = 32767;
	}

	/* update the outgoing arguments */
	*sdiv = lsdiv;
	*md = lmd;
	*pe = lpe;

	/* return 0 if all variables meet their contraints */
	return (lsdiv <= 7 && -16 <= lmd && lmd <= -1 && lpe <= 32767) ? 0 : -1;
}

/* ========================================================================
   Name:	clk_fs216c65_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

int clk_fs216c65_get_rate(unsigned long input, unsigned long pe,
		unsigned long md, unsigned long sd, unsigned long *rate)
{
	int md2 = md;
	long long p, q, r, s, t;
	if (md & 0x10)
		md2 = md | 0xfffffff0;/* adjust the md sign */

	input *= 8;

	p = 1048576ll * input;
	q = 32768 * md2;
	r = 1081344 - pe;
	s = r + q;
	t = (1 << (sd + 1)) * s;
	*rate = div64_u64(p, t);

	return 0;
}

/*
   FS660
   Based on C32_4FS_660MHZ_LR_EG_5U1X2T8X_um spec.

   This FSYN embed a programmable PLL which then serve the 4 digital blocks

   clkin => PLL660 => DIG660_0 => clkout0
		   => DIG660_1 => clkout1
		   => DIG660_2 => clkout2
		   => DIG660_3 => clkout3
   For this reason the PLL660 is programmed separately from digital parts.
*/

/* ========================================================================
   Name:	clk_fs660c32_vco_get_params()
   Description: Compute params for embeded PLL660
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *ndiv (register value). Note that PDIV is frozen to 1.
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fs660c32_vco_get_params(unsigned long input, unsigned long output,
			     unsigned long *ndiv)
{
/* Formula
   VCO frequency = (fin x ndiv) / pdiv
   ndiv = VCOfreq * pdiv / fin
   */
	unsigned long pdiv = 1, n;

	/* Output clock range: 384Mhz to 660Mhz */
	if (output < 384000000 || output > 660000000)
		return CLK_ERR_BAD_PARAMETER;

	if (input > 40000000)
		/* This means that PDIV would be 2 instead of 1.
		   Not supported today. */
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	n = output * pdiv / input;
	/* FCh: opened point. Min value is 16. To be clarified */
	if (n < 16)
		n = 16;
	*ndiv = n - 16; /* Converting formula value to reg value */

	return 0;
}
/* ========================================================================
   Name:	clk_fs660c32_dig_get_params()
   Description: Compute params for digital part of FS660
   Input:       input=VCO freq, output=requested freq (Hz) & nsdiv
   Output:      updated *md, *pe & *sdiv registers values.
   Return:      'clk_err_t' error code
   ======================================================================== */
#define p20		(1 << 20)

/* We use Fixed-point arithmetic in order to avoid "float" functions.*/
#define SCALING_FACTOR	2048LL

int clk_fs660c32_dig_get_params(unsigned long input, unsigned long output,
			     unsigned long nsdiv, unsigned long *md,
			     unsigned long *pe, unsigned long *sdiv)
{
	int si;
	unsigned long ns; /* nsdiv value (1 or 3) */
	unsigned long s; /* sdiv value = 1 << sdiv_reg_value */
	unsigned long p; /* pe value */
	unsigned long m; /* md value */
	unsigned long new_freq, new_deviation;
	/* initial condition to say: "infinite deviation" */
	unsigned long deviation = 0xffffffff;

	/*
	 * nsdiv is a register value ('BIN') which is translated
	 * to a decimal value following the below table:
	 *
	 *            ns.bin         ns.dec
	 *              0               3
	 *              1               1
	 */
	ns = (nsdiv ? 1 : 3);

	/* Reduce freq to prevent overflows */
	input /= 10000;
	output /= 10000;

	for (si = 0; (si < 9) && deviation; si++) {
		s = (1 << si);
		for (m = 0; (m < 32) && deviation; m++) {
			p = (input * 2048) ;
			p = p - 2048 * (s * ns * output) - (s * ns * output) * (m * (2048 / 32));
			p = p * (p20 / 2048);
			p = p / (s * ns * output);
			if (p > 32767)
				continue;
			new_freq = (input * 2048) / (s * ns * (2048 + (m * (2048 / 32)) + ((p * 2048) / p20)));
			if (new_freq < output)
				new_deviation = output - new_freq;
			else
				new_deviation = new_freq - output;
			/* Check if this is a better solution */
			if (new_deviation < deviation) {
				*pe = p;
				*md = m;
				*sdiv = si;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:	clk_fs660liege_dig_get_params()
   Description: Compute params for digital part of FS660
   Input:       input=VCO freq, output=requested freq (Hz) & nsdiv
   Output:      updated *md, *pe & *sdiv registers values.
   Return:      'clk_err_t' error code
   ======================================================================== */
#define P20		(uint64_t)(1 << 20)
int clk_fs660liege_dig_get_params(unsigned long input, unsigned long output,
			     unsigned long *nsdiv, unsigned long *md,
			     unsigned long *pe, unsigned long *sdiv)
{
	int si;
	unsigned long ns; /* nsdiv value (1 or 3) */
	unsigned long s; /* sdiv value = 1 << sdiv_reg_value */
	unsigned long m; /* md value */
	unsigned long new_freq, new_deviation;
	/* initial condition to say: "infinite deviation" */
	unsigned long deviation = 0xffffffff;
	uint64_t p; /* pe value */

	/*
	 * nsdiv is a register value ('BIN') which is translated
	 * to a decimal value
	 * moreover on some chip this register is totally hard-wired on silicon
	 * while on other chip it's programmable
	 *  following the below table:
	 *
	 *    *nsdiv        ns.bin       	  ns.dec
	 * 	-1	  programmable
	 *       0          0-silicon               3
	 *       1          1-silicon               1
	 */
	if (*nsdiv != -1) {
		ns = (*nsdiv ? 1 : 3);
		goto skip_ns_programming;
	}

	for (ns = 1; ns < 4; ns += 2)

skip_ns_programming:

	for (si = 0; (si < 9) && deviation; si++) {
		s = (1 << si);
		for (m = 0; (m < 32) && deviation; m++) {
			p = (uint64_t)input * SCALING_FACTOR;
			p = p - SCALING_FACTOR * ((uint64_t)s * (uint64_t)ns * (uint64_t)output) -
				 ((uint64_t)s * (uint64_t)ns * (uint64_t)output) *
				 ((uint64_t)m * (SCALING_FACTOR / 32LL));
			p = p * (P20 / SCALING_FACTOR);
			p = div64_u64(p, (uint64_t)((uint64_t)s * (uint64_t)ns * (uint64_t)output));

			if (p > 32767LL)
				continue;

			clk_fs660c32_get_rate(input, (ns == 1) ? 1 : 0, m,
					(unsigned long)p, si, &new_freq);

			if (new_freq < output)
				new_deviation = output - new_freq;
			else
				new_deviation = new_freq - output;
			/* Check if this is a better solution */
			if (new_deviation < deviation) {
				*pe = (unsigned long)p;
				*md = m;
				*sdiv = si;
				*nsdiv = (ns == 1) ? 1 : 0;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == 0xffffffff) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:	clk_fs660c32_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   Inputs:	input=VCO frequency, nsdiv, md, pe, & sdivregisters values.
   Outputs:	*rate updated
   ======================================================================== */

int clk_fs660c32_get_rate(unsigned long input, unsigned long nsdiv,
			unsigned long md, unsigned long pe,
			unsigned long sdiv, unsigned long *rate)
{

	unsigned long s = (1 << sdiv); /* sdiv value = 1 << sdiv_reg_value */
	unsigned long ns;  /* nsdiv value (1 or 3) */

	/*
	 * ns is a binary value which is translated to a decimal value
	 * following the below table:
	 *
	 *	    nsdiv.bin	     ns.dec
	 *		0		3
	 *		1		1
	 */

	ns = (nsdiv == 1) ? 1 : 3;

	*rate = (unsigned long) div64_u64(((uint64_t)input * SCALING_FACTOR),
		   (uint64_t)((uint64_t)s * (uint64_t)ns *
		    (SCALING_FACTOR + ((uint64_t)md * SCALING_FACTOR / 32LL) +
		    ((uint64_t)pe * SCALING_FACTOR / P20))));
	return 0;
}

/* ========================================================================
   Name:	clk_fs660c32_vco_get_rate()
   Description: Compute VCO frequency of FS660 embeded PLL (PLL660)
   Input: ndiv & pdiv registers values
   Output: updated *rate (Hz)
   ======================================================================== */

int clk_fs660c32_vco_get_rate(unsigned long input, unsigned long ndiv,
			   unsigned long *rate)
{
	unsigned long nd = ndiv + 16; /* ndiv value */
	unsigned long pdiv = 1; /* Frozen. Not configurable so far */

	*rate = (input * nd) / pdiv;

	return 0;
}

