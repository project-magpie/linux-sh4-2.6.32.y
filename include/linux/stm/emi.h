/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

int emi_init(unsigned long memory_base, unsigned long control_base);
unsigned long emi_bank_base(int bank);
void emi_config_pata(int bank);
