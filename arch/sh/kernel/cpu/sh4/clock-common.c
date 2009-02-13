/************************************************************************
File  : Low Level clock API
        Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008 STMicroelectronics
************************************************************************/

#if defined(ST_OS21)
#include <math.h>
#include "clock.h"

#else   /* Linux */

#include <linux/clk.h>
#include <linux/math64.h>

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
#endif

#include "clock-oslayer.h"
#include "clock-common.h"

/* ========================================================================
   Name:        clk_pll800_freq()
   Description: Convert PLLx_CFG to freq for PLL800
   Params:      'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */

unsigned long clk_pll800_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, pdiv, mdiv;

	mdiv = (cfg >>  0) & 0xff;
	ndiv = (cfg >>  8) & 0xff;
	pdiv = (cfg >> 16) & 0x7;
	freq = (((2 * (input/1000) * ndiv) / mdiv) / (1 << pdiv)) * 1000;

	return freq;
}

/* ========================================================================
   Name:        clk_pll1600_freq()
   Description: Convert PLLx_CFG to freq for PLL1600
                Always returns HS output value.
   Params:      'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */

unsigned long clk_pll1600_freq(unsigned long input, unsigned long cfg)
{
	unsigned long freq, ndiv, mdiv;

	mdiv = (cfg >>  0) & 0x7;
	ndiv = (cfg >>  8) & 0xff;
	freq = ((2 * (input/1000) * ndiv) / mdiv) * 1000;

	return freq;
}

/* ========================================================================
   Name:        clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

unsigned long clk_fsyn_get_rate(unsigned long input, unsigned long pe,
		unsigned long md, unsigned long sd)
{
#ifdef ST_OS21

    unsigned long rate,ref;
    int pediv,mddiv,sddiv;

    pediv = (pe&0xffff);
    mddiv =(md&0x1f);
    mddiv = ( mddiv - 32 );
    sddiv =(sd&0x7);
    sddiv = pow(2,(sddiv+1));

    ref = input / 1000000;

    rate = ((pow(2,15)*(ref*8))/(sddiv*((pediv*(1.0+mddiv/32.0))-((pediv-pow(2,15))*(1.0+(mddiv+1.0)/32.0)))))*1000000;

    return(rate);

#else   /* Linux: does not allow use of FPU in kernel space */

	long long p, q, r, s, t, u;

	p = 1048576ll * input;
	q = 32768 * md;
	r = 1081344 - pe;
	s = r + q;
	t = (1 << (sd + 1)) * s;
	u = div64_u64(p, t);

	return u;

 #endif
}

/* ========================================================================
   Name:        clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_params(int input, int output, int *md, int *pe, int *sdiv)
{
#ifdef ST_OS21

    double fr, Tr, Td1, Tx, fmx, nd1, nd2, Tdif;
    int NTAP, msdiv, mfmx, ndiv, fout;

    NTAP = 32;
    mfmx = 0;

    ndiv = 1.0;

    fr = input * 8.0;
    Tr = 1.0 / fr;
    Td1 = 1.0 / (NTAP * fr);
    msdiv = 0;

    /* Looking for SDIV */
    while (! ((mfmx >= (input*8)) && (mfmx <= (input*16))) && (msdiv < 7))
    {
        msdiv = msdiv + 1;
        mfmx = pow(2,msdiv) * output;
    }

    *sdiv = msdiv - 1;
    fmx = mfmx / (float)1000000.0;
    if ((fmx < (8*input)) || (fmx > (16*input)))
    {
        return(CLK_ERR_BAD_PARAMETER);
    }

    Tx = 1 / (fmx * 1000000.0);

    Tdif = Tr - Tx;

    /* Looking for MD */
    nd1 = floor((32.0 * (mfmx - fr) / mfmx));
    nd2 = nd1 + 1.0;

    *md = 32.0 - nd2;

    /* Looking for PE */
    *pe = ceil((32.0 * (mfmx - fr) / mfmx - nd1) * 32768.0);

    return(0);

#else   /* Linux */

	unsigned long long p, q;
	unsigned int predivide;
	int preshift; /* always +ve but used in subtraction */
	unsigned int lsdiv;
	int lmd;
	unsigned int lpe = 1 << 14;

	/* pre-divide the frequencies */
	p = 1048576ull * input;    /* <<20? */
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
	lmd = ((preshift - 1048576) / 32768) - 1;         /* >>15? */

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

#endif
}

/* ========================================================================
   Name:        clk_err_string
   Description: Convert LLA error code to string.
   Returns:     const char *ErrMessage
   ======================================================================== */

const char *clk_err_string(int err)
{
    static const char *errors[]={"unknown error","feature not supported","bad parameter","fatal error"};
    if ( err > CLK_ERR_INTERNAL ) return(errors[0]);

    return(errors[err]);
}

/* ========================================================================
   Name:        clk_short_name
   Description: Returns clock name with prefix skipped (XXX_).
                "CLKA_DISP_200" becomes "DISP_200".
   Returns:     const char *ShortName
   ======================================================================== */

const char *clk_short_name(const char *name)
{
    const char *Ptr;

    for( Ptr = name; *Ptr && (*Ptr!='_'); Ptr++ );
    Ptr++;  /* Skipping '_' */

    return(Ptr);
}

#if !defined(ST_OS21)
#define TOLLERANCE	5
#define tollerance	((rate * (TOLLERANCE))/100)

int get_ratio_field(unsigned long rate, unsigned long prate, int *ratios)
{
	int idx;
	unsigned long h_threshold = rate + tollerance;
	unsigned long l_threshold = rate - tollerance;

	if (!prate || !rate || !ratios)
		return NO_MORE_RATIO;
	if (rate > prate)
		return NO_MORE_RATIO;
	for (idx = 0; ratios[idx] != NO_MORE_RATIO; ++idx) {
		if (ratios[idx] == RATIO_RESERVED)
			continue;
		if (!ratios[idx])
			continue;
		if (prate/ratios[idx] >= l_threshold &&
		    prate/ratios[idx] <= h_threshold)
			return idx;
	}
	return NO_MORE_RATIO;
}

int get_xratio_field(unsigned long rate, unsigned long prate,
	struct xratio *ratios)
{
	int idx;
	unsigned long h_threshold = rate + tollerance;
	unsigned long l_threshold = rate - tollerance;
	if (!prate || !rate || !ratios)
		return NO_MORE_RATIO;
	if (rate > prate)
		return NO_MORE_RATIO;
	for (idx = 0; ratios[idx].ratio != NO_MORE_RATIO; ++idx) {
		if (ratios[idx].ratio == RATIO_RESERVED)
			continue;
		if (!ratios[idx].ratio)
			continue;
		if (prate/ratios[idx].ratio >= l_threshold &&
		    prate/ratios[idx].ratio <= h_threshold)
			return idx;
	}
	return NO_MORE_RATIO;
}
#endif
