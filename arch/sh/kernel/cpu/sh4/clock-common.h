/************************************************************************
File  : Low Level clock API
        Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008 STMicroelectronics
************************************************************************/

#ifndef __CLKLLA_COMMON_H
#define __CLKLLA_COMMON_H

#ifndef ST_OS21
#define NO_MORE_RATIO		-1
#define RATIO_RESERVED		-2

int get_ratio_field(unsigned long rate, unsigned long prate, int *ratios);

struct xratio {
	unsigned long ratio;
	unsigned long field;
};

int get_xratio_field(unsigned long rate, unsigned long prate,
	struct xratio *ratios);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
   Name:        clk_pll800_freq()
   Description: Convert PLLx_CFG to freq for PLL800
   Params:      'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */

unsigned long clk_pll800_freq( unsigned long input, unsigned long cfg );

/* ========================================================================
   Name:        clk_pll1600_freq()
   Description: Convert PLLx_CFG to freq for PLL1600
                Always returns HS output value.
   Params:      'input' freq (Hz), 'cfg'=PLLx_CFG register value
   ======================================================================== */

unsigned long clk_pll1600_freq( unsigned long input, unsigned long cfg );

/* ========================================================================
   Name:        clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

unsigned long clk_fsyn_get_rate( unsigned long input, unsigned long pe, unsigned long md, unsigned long sd );

/* ========================================================================
   Name:        clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_params( int input, int output, int *md, int *pe, int *sdiv );

/* ========================================================================
   Name:        clk_err_string
   Description: Convert LLA error code to string.
   Returns:     const char *ErrMessage
   ======================================================================== */

const char *clk_err_string( int err );


#ifdef __cplusplus
}
#endif

#endif /* #ifndef __CLKLLA_COMMON_H */
