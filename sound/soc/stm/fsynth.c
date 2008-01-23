/*
 *   STMicroelectronics System-on-Chips' audio oversampling frequency
 *   synthesizers driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Authors: Pawel MOLL <pawel.moll@st.com>
 *            Daniel THOMPSON <daniel.thompson@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/registers.h>
#include <asm/div64.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/info.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 4 /* See common.h debug features */
#include "common.h"



/*
 * Hardware constants
 */

#ifdef CONFIG_CPU_SUBTYPE_STB7100
#	define CHANNELS 3
#endif
#ifdef CONFIG_CPU_SUBTYPE_STX7200
#	define CHANNELS 4
#endif

/* PLL inside the synthesizer multiplies input frequency
 * (which is 30MHz in our case) by 8... */
#define PLL_FREQ 8 * 30 * 1000 * 1000



/*
 * Audio frequency synthesizer structures
 */

struct snd_stm_fsynth_channel {
	struct snd_stm_fsynth *fsynth;

	int frequency;  /* Nominal */
	int adjustment; /* Actual (achieved) */
};

struct snd_stm_fsynth {
	const char *bus_id;

	struct resource *mem_region;

	void *base;

	int channels_from, channels_to;
	struct snd_stm_fsynth_channel channels[CHANNELS];

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Toolbox
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

/* Solve the frequency synthesiser equations to provide a specified output
 * frequency.
 *
 * The approach taken to solve the equation is to solve for sdiv assuming
 * maximal values for md and one greater than maximal pe (-16 and 32768
 * respectively) before rounding down. Once sdiv is selected we can
 * solve for md by assuming maximal pe and rounding down. With these
 * values pe can trivially be calculated.
 *
 * The function is implemented entirely with integer calculations making
 * it suitable for use within the Linux kernel.
 *
 * The magic numbers within the function are derived from the Fsynth equation
 * which is as follows:
 *
 * <pre>
 *                                  32768*Fpll
 * #1: Fout = ------------------------------------------------------
 *                            md                        (md + 1)
 *            (sdiv*((pe*(1 + --)) - ((pe - 32768)*(1 + --------))))
 *                            32                           32
 * </pre>
 *
 * Where:
 *
 *  - Fpll and Fout are frequencies in Hz
 *  - sdiv is power of 2 between 1 and 8
 *  - md is an integer between -1 and -16
 *  - pe is an integer between 0 and 32767
 *
 * This simplifies to:
 *
 * <pre>
 *                       1048576*Fpll
 * #2: Fout = ----------------------------------
 *            (sdiv*(1081344 - pe + (32768*md)))
 * </pre>
 *
 * Rearranging:
 *
 * <pre>
 *                 1048576*Fpll
 * #3: predivide = ------------ = (sdiv*(1081344 - pe + (32768*md)))
 *                     Fout
 * </pre>
 *
 * If solve for sdiv and let pe = 32768 and md = -16 we get:
 *
 * <pre>
 *                     predivide            predivide
 * #4: sdiv = --------------------------- = ---------
 *            (1081344 - pe + (32768*md))     524288
 * </pre>
 *
 * Returning to eqn. #3, solving for md and let pe = 32768 we get:
 *
 * <pre>
 *           predivide                    predivide
 *          (--------- - 1081344 + pe)   (--------- - 1048576)
 *             sdiv                         sdiv
 * #5: md = -------------------------- = ---------------------
 *                    32768                      32768

 * </pre>
 *
 * Finally we return to #3 and rearrange for pe:
 *
 * <pre>
 *              predivide
 * #6: pe = -1*(--------- - 1081344 - (32768*md))
 *                sdiv
 * </pre>
 *
 */
static int solve_fsynth_eqn(unsigned int Fpll, unsigned int Fout,
		unsigned int *sdivp, int *mdp, unsigned int *pep)
{
	unsigned long long p, q;
	unsigned int predivide;
	int preshift; /* always +ve but used in subtraction */
	unsigned int sdiv;
	int md;
	unsigned int pe = 1 << 14;

	/* pre-divide the frequencies */
	p = 1048576ull * Fpll;		/* <<20? */
	q = Fout;

	predivide = (unsigned int)div64_64(p, q);

	/* determine an appropriate value for the output divider using eqn. #4
	 * with md = -16 and pe = 32768 (and round down) */
	sdiv = predivide / 524288;
	if (sdiv > 1) {
		/* sdiv = fls(sdiv) - 1; // this doesn't work
		 * for some unknown reason */
		sdiv = most_significant_set_bit(sdiv);
	} else
		sdiv = 1;

	/* pre-shift a common sub-expression of later calculations */
	preshift = predivide >> sdiv;

	/* determine an appropriate value for the coarse selection using eqn. #5
	 * with pe = 32768 (and round down which for signed values means away
	 * from zero) */
	md = ((preshift - 1048576) / 32768) - 1;	/* >>15? */

	/* calculate a value for pe that meets the output target */
	pe = -1 * (preshift - 1081344 - (32768 * md));	/* <<15? */

	/* finally give sdiv its true hardware form */
	sdiv--;

	/* special case for 58593.75Hz and harmonics...
	 * can't quite seem to get the rounding right */
	if (md == -17 && pe == 0) {
		md = -16;
		pe = 32767;
	}

	/* update the outgoing arguments */
	*sdivp = sdiv;
	*mdp = md;
	*pep = pe;

	snd_stm_printt("SDIV == %u, MD == %d, PE == %u\n", sdiv, md, pe);

	/* return 0 if all variables meet their contraints */
	return (sdiv <= 7 && -16 <= md && md <= -1 && pe <= 32767) ? 0 : -1;
}

/*
 *                   1048576*Fpll
 * Fout = ----------------------------------
 *        (sdiv*(1081344 - pe + (32768*md)))
 *
 * Fpll is premultiplied by 8
 * Fout needs dividing by 256 to get real frequency
 *
 * small error compared to double based original
 * i.e. for 44100 (11289600) it reports 11289610 instead of 11289610.36
 */
static int get_fsynth_output(unsigned int Fpll,
		unsigned int sdiv, int md, unsigned int pe)
{
	long long p, q, r, s, t, u;

	p = 1048576ll * Fpll;
	q = 32768 * md;
	r = 1081344 - pe;
	s = r + q;
	t = (1 << (sdiv + 1)) * s;
	u = div64_64(p, t);

	return (int)u;
}

static int snd_stm_fsynth_channel_configure(struct snd_stm_fsynth *fsynth,
		int channel, int frequency, int adjustment)
{
	int result;
	unsigned int sdiv;
	int md;
	unsigned int pe;
	int frequency_adjusted, frequency_achieved, adjustment_achieved;
	int delta;

	snd_stm_printt("snd_stm_fsynth_configure(fsynth=%p, channel=%d, "
			"frequency=%d, adjustment=%d)\n", fsynth, channel,
			frequency, adjustment);

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);
	snd_assert(channel >= fsynth->channels_from, return -EINVAL);
	snd_assert(channel <= fsynth->channels_to, return -EINVAL);

	/*             a
	 * F = f + --------- * f = f + d
	 *          1000000
	 *
	 *         a
	 * d = --------- * f
	 *      1000000
	 *
	 * where:
	 *   f - nominal frequency
	 *   a - adjustment in ppm (parts per milion)
	 *   F - frequency to be set in synthesizer
	 *   d - delta (difference) between f and F
	 */
	if (adjustment < 0) {
		/* div64_64 operates on unsigned values... */
		delta = -1;
		adjustment = -adjustment;
	} else {
		delta = 1;
	}
	/* 500000 ppm is 0.5, which is used to round up values */
	delta *= (int)div64_64((uint64_t)frequency * (uint64_t)adjustment +
			500000, 1000000);
	frequency_adjusted = frequency + delta;

	snd_stm_printt("Setting %s channel %d to frequency %d.\n",
			fsynth->bus_id, channel,
			frequency_adjusted);

	result = solve_fsynth_eqn(PLL_FREQ, frequency_adjusted,
			&sdiv, &md, &pe);
	if (result < 0) {
		snd_stm_printe("Frequency %d can't be generated!\n",
				frequency_adjusted);
		return -EINVAL;
	}

	snd_stm_printt("SDIV == 0x%x, MD == 0x%x, PE == 0x%x\n", sdiv,
			(unsigned int)md & AUDCFG_FSYN_MD__MD__MASK, pe);
	REGFIELD_SET_N(fsynth->base, AUDCFG_FSYN_PROGEN, channel,
			PROG_EN, PE0_MD0_IGNORED);
	REGFIELD_POKE_N(fsynth->base, AUDCFG_FSYN_SDIV, channel,
			SDIV, sdiv);
	REGFIELD_POKE_N(fsynth->base, AUDCFG_FSYN_MD, channel,
			MD, (unsigned int)md & AUDCFG_FSYN_MD__MD__MASK);
	REGFIELD_POKE_N(fsynth->base, AUDCFG_FSYN_PE, channel,
			PE, pe);
	REGFIELD_SET_N(fsynth->base, AUDCFG_FSYN_PROGEN, channel,
			PROG_EN, PE0_MD0_USED);
	REGFIELD_SET_N(fsynth->base, AUDCFG_FSYN_PROGEN, channel,
			PROG_EN, PE0_MD0_IGNORED);

	/*             a                    a
	 * F = f + --------- * f   =>   --------- * f = F - f   ==>
	 *          1000000              1000000
	 *
	 *           a        F - f               F - f
	 * ==>   --------- = -------   ==>   a = ------- * 1000000
	 *        1000000       f                   f
	 *
	 * F = f + d   ==>   d = F - f
	 *
	 *      f + d - f               d
	 * a = ----------- * 1000000 = --- * 1000000
	 *          f                   f
	 * where:
	 *   f - nominal frequency
	 *   a - adjustment in ppm (parts per milion)
	 *   F - frequency actually being generated by fsynch
	 *   d - delta between F and f
	 */
	frequency_achieved = get_fsynth_output(PLL_FREQ, sdiv, md, pe);
	delta = frequency_achieved - frequency;
	if (delta < 0) {
		/* div64_64 operates on unsigned values... */
		delta = -delta;
		adjustment_achieved = -1;
	} else {
		adjustment_achieved = 1;
	}
	/* frequency/2 is added to round up result */
	adjustment_achieved *= (int)div64_64((uint64_t)delta * 1000000 +
			frequency / 2, frequency);

	snd_stm_printt("Nominal frequency is %d, actual frequency is %d, "
			"(%d ppm difference).\n", frequency,
			frequency_achieved, adjustment_achieved);

	/* Save this informations for future generations ;-) */
	fsynth->channels[channel].frequency = frequency;
	fsynth->channels[channel].adjustment = adjustment_achieved;

	return 0;
}



/*
 * ALSA controls
 */

static int snd_stm_fsynth_adjustment_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = -1000000;
	uinfo->value.integer.max = 1000000;

	return 0;
}

static int snd_stm_fsynth_adjustment_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_stm_fsynth_channel *fsynth_channel =
		snd_kcontrol_chip(kcontrol);

	snd_stm_printt("snd_stm_fsynth_adjustment_get(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(fsynth_channel, return -EINVAL);
	snd_assert(fsynth_channel->fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth_channel->fsynth, return -EINVAL);

	ucontrol->value.integer.value[0] = fsynth_channel->adjustment;

	return 0;
}

static int snd_stm_fsynth_adjustment_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int result;
	struct snd_stm_fsynth_channel *fsynth_channel =
		snd_kcontrol_chip(kcontrol);
	struct snd_stm_fsynth *fsynth;
	int channel;
	int old_adjustement;

	snd_stm_printt("snd_stm_fsynth_clock_adjustment_put(kcontrol=0x%p, "
			"ucontrol=0x%p)\n", kcontrol, ucontrol);

	snd_assert(fsynth_channel, return -EINVAL);

	fsynth = fsynth_channel->fsynth;

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);

	channel = fsynth_channel - fsynth_channel->fsynth->channels;
	old_adjustement = fsynth_channel->adjustment;

	result = snd_stm_fsynth_channel_configure(fsynth, channel,
			fsynth_channel->frequency,
			ucontrol->value.integer.value[0]);

	if (result < 0)
		return -EINVAL;

	return old_adjustement != fsynth_channel->adjustment;
}

static struct snd_kcontrol_new __initdata snd_stm_fsynth_adjustment_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_PCM,
	.name = "PCM Playback Oversampling Freq. Adjustment",
	.info = snd_stm_fsynth_adjustment_info,
	.get = snd_stm_fsynth_adjustment_get,
	.put = snd_stm_fsynth_adjustment_put,
};



/*
 * Audio frequency synthesizer public interface implementation
 */

int snd_stm_fsynth_set_frequency(struct device *device, int channel,
		int frequency)
{
	struct snd_stm_fsynth *fsynth = dev_get_drvdata(device);

	snd_stm_printt("snd_stm_fsynth_set_frequency(device=%p, channel=%d, "
			"frequency=%d)\n", device, channel, frequency);

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);
	snd_assert(channel >= fsynth->channels_from, return -EINVAL);
	snd_assert(channel <= fsynth->channels_to, return -EINVAL);

	return snd_stm_fsynth_channel_configure(fsynth, channel, frequency, 0);
}

int __init snd_stm_fsynth_add_adjustement_ctl(struct device *device,
		int channel, struct snd_card *card, int card_device)
{
	int result;
	struct snd_stm_fsynth *fsynth = dev_get_drvdata(device);

	snd_stm_printt("snd_stm_fsynth_add_control(device=%p, channel=%d, "
			"card=%p, card_device=%d)\n", device, channel,
			card, card_device);

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);
	snd_assert(channel >= fsynth->channels_from, return -EINVAL);
	snd_assert(channel <= fsynth->channels_to, return -EINVAL);

	snd_stm_fsynth_adjustment_ctl.device = card_device;
	result = snd_ctl_add(card, snd_ctl_new1(&snd_stm_fsynth_adjustment_ctl,
			&fsynth->channels[channel]));
	/* TODO: index per card */
	snd_stm_fsynth_adjustment_ctl.index++;

	return result;
}



/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r, n) \
		snd_iprintf(buffer, "AUDCFG_FSYN%d_%s (offset 0x%02x) " \
				"= 0x%08x\n", n, __stringify(r), \
				AUDCFG_FSYN_##r(n), \
				REGISTER_PEEK_N(fsynth->base, \
				AUDCFG_FSYN_##r, n))

static void snd_stm_fsynth_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_fsynth *fsynth = entry->private_data;
	int i;

	snd_assert(fsynth, return);
	snd_stm_magic_assert(fsynth, return);

	snd_iprintf(buffer, "AUDCFG_FSYN_CFG (offset 0x00) = 0x%08x\n",
			REGISTER_PEEK(fsynth->base, AUDCFG_FSYN_CFG));

	for (i = 0; i < CHANNELS; i++) {
		DUMP_REGISTER(MD, i);
		DUMP_REGISTER(PE, i);
		DUMP_REGISTER(SDIV, i);
		DUMP_REGISTER(PROGEN, i);
	}
}

static int snd_stm_fsynth_register(struct snd_device *snd_device)
{
	struct snd_stm_fsynth *fsynth = snd_device->device_data;
	unsigned long value = 0;
	int i;

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);

	/* Initialize & reset synthesizer */

	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, RSTP, RESET);
	for (i = fsynth->channels_from; i <= fsynth->channels_to; i++) {
		snd_printd("Enabling synthesizer '%s' channel %d\n",
				fsynth->bus_id, i);
		value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, PCM_CLK_SEL,
				FSYNTH(i));
#ifdef CONFIG_CPU_SUBTYPE_STB7100
		value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, FS_EN, ENABLED(i));
#endif
		value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, NSB, ACTIVE(i));
	}
	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, NPDA, NORMAL);
	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, NDIV, 27_30_MHZ);
	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, BW_SEL, GOOD_REFERENCE);
	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, REF_CLK_IN, 30_MHZ_CLOCK);

	REGISTER_POKE(fsynth->base, AUDCFG_FSYN_CFG, value);
	barrier();

	/* Unreset ;-) it now */

	REGFIELD_SET(fsynth->base, AUDCFG_FSYN_CFG, RSTP, RUNNING);

	/* Additional procfs info */

	snd_stm_info_register(&fsynth->proc_entry, fsynth->bus_id,
			snd_stm_fsynth_dump_registers, fsynth);

	return 0;
}

static int snd_stm_fsynth_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_fsynth *fsynth = snd_device->device_data;
	unsigned long value = 0;
	int i;

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);

	/* Remove procfs entry */

	snd_stm_info_unregister(fsynth->proc_entry);

	/* Disable synthesizer */

	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, RSTP, RUNNING);
	for (i = fsynth->channels_from; i <= fsynth->channels_to; i++) {
#ifdef CONFIG_CPU_SUBTYPE_STB7100
		value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, FS_EN, DISABLED(i));
#endif
		value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, NSB, STANDBY(i));
	}
	value |= REGFIELD_VALUE(AUDCFG_FSYN_CFG, NPDA, POWER_DOWN);

	REGISTER_POKE(fsynth->base, AUDCFG_FSYN_CFG, value);

	return 0;
}

static struct snd_device_ops snd_stm_fsynth_ops = {
	.dev_register = snd_stm_fsynth_register,
	.dev_disconnect = snd_stm_fsynth_disconnect,
};



/*
 * Platform driver routines
 */

static int __init snd_stm_fsynth_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_component *component;
	struct snd_stm_fsynth *fsynth;
	const char *card_id;
	struct snd_card *card;
	int i;

	snd_printd("--- Probing device '%s'...\n", pdev->dev.bus_id);

	component = snd_stm_components_get(pdev->dev.bus_id);
	snd_assert(component, return -EINVAL);

	fsynth = kzalloc(sizeof(*fsynth), GFP_KERNEL);
	if (!fsynth) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(fsynth);
	fsynth->bus_id = pdev->dev.bus_id;
	for (i = 0; i < CHANNELS; i++)
		fsynth->channels[i].fsynth = fsynth;

	result = snd_stm_memory_request(pdev, &fsynth->mem_region,
			&fsynth->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	result = snd_stm_cap_get_string(component, "card_id", &card_id);
	if (result == 0)
		card = snd_stm_cards_get(card_id);
	else
		card = snd_stm_cards_default(&card_id);
	snd_assert(card, return -EINVAL);
	snd_printd("This frequency synthesizer will be a member "
			"of a card '%s'\n", card_id);

	result = snd_stm_cap_get_range(component, "channels",
			&fsynth->channels_from, &fsynth->channels_to);
	snd_assert(result == 0, return -EINVAL);
	snd_assert(fsynth->channels_from < fsynth->channels_to,
			return -EINVAL);
	snd_assert(fsynth->channels_from >= 0, return -EINVAL);
	snd_assert(fsynth->channels_to < CHANNELS, return -EINVAL);

	snd_printd("Used synthesizer channels: %d to %d\n",
			fsynth->channels_from, fsynth->channels_to);

	/* ALSA component */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, fsynth,
			&snd_stm_fsynth_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, fsynth);

	snd_printd("--- Probed successfully!\n");

	return result;

error_device:
	snd_stm_memory_release(fsynth->mem_region, fsynth->base);
error_memory_request:
	snd_stm_magic_clear(fsynth);
	kfree(fsynth);
error_alloc:
	return result;
}

static int snd_stm_fsynth_remove(struct platform_device *pdev)
{
	struct snd_stm_fsynth *fsynth = platform_get_drvdata(pdev);

	snd_assert(fsynth, return -EINVAL);
	snd_stm_magic_assert(fsynth, return -EINVAL);

	snd_stm_memory_release(fsynth->mem_region, fsynth->base);

	snd_stm_magic_clear(fsynth);
	kfree(fsynth);

	return 0;
}

static struct platform_driver snd_stm_fsynth_driver = {
	.driver = {
		.name = "fsynth",
	},
	.probe = snd_stm_fsynth_probe,
	.remove = snd_stm_fsynth_remove,
};



/*
 * Initialization
 */

int __init snd_stm_fsynth_init(void)
{
	return platform_driver_register(&snd_stm_fsynth_driver);
}

void snd_stm_fsynth_cleanup(void)
{
	platform_driver_unregister(&snd_stm_fsynth_driver);
}
