// SPDX-License-Identifier: GPL-2.0-only
/*
 * clock.c - generic clocksource implementation
 *
 * This file contains the clocksource implementation from the Linux
 * kernel originally by John Stultz
 *
 * Copyright (C) 2004, 2005 IBM, John Stultz (johnstul@us.ibm.com)
 * Copyright (c) 2007 Sascha Hauer <s.hauer@pengutronix.de>, Pengutronix
 */

#include <common.h>
#include <init.h>
#include <linux/math64.h>
#include <clock.h>
#include <sched.h>

static uint64_t time_ns;

static uint64_t dummy_read(void)
{
	static uint64_t dummy_counter;

	dummy_counter += CONFIG_CLOCKSOURCE_DUMMY_RATE;

	return dummy_counter;
}

static struct clocksource dummy_cs = {
	.shift = 0,
	.mult = 1,
	.read = dummy_read,
	.mask = CLOCKSOURCE_MASK(64),
	.priority = -1,
};

static struct clocksource *current_clock = IN_PROPER ? &dummy_cs : NULL;

static int dummy_csrc_warn(void)
{
	if (current_clock == &dummy_cs) {
		pr_warn("Warning: Using dummy clocksource\n");
	}

	return 0;
}
late_initcall(dummy_csrc_warn);

/**
 * get_time_ns - get current timestamp in nanoseconds
 */
uint64_t get_time_ns(void)
{
	struct clocksource *cs = current_clock;
	uint64_t cycle_now, cycle_delta;
	uint64_t ns_offset;

	if (IN_PBL && !cs)
		panic("No PBL clocksource has been initialized\n");

	/* read clocksource: */
	cycle_now = cs->read() & cs->mask;

	/* calculate the delta since the last call: */
	cycle_delta = (cycle_now - cs->cycle_last) & cs->mask;

	/* convert to nanoseconds: */
	ns_offset = cyc2ns(cs, cycle_delta);

	cs->cycle_last = cycle_now;

	time_ns += ns_offset;
	return time_ns;
}
EXPORT_SYMBOL(get_time_ns);

/**
 * clocks_calc_mult_shift - calculate mult/shift factors for scaled math of clocks
 * @mult:       pointer to mult variable
 * @shift:      pointer to shift variable
 * @from:       frequency to convert from
 * @to:         frequency to convert to
 * @maxsec:     guaranteed runtime conversion range in seconds
 *
 * The function evaluates the shift/mult pair for the scaled math
 * operations of clocksources and clockevents.
 *
 * @to and @from are frequency values in HZ. For clock sources @to is
 * NSEC_PER_SEC == 1GHz and @from is the counter frequency. For clock
 * event @to is the counter frequency and @from is NSEC_PER_SEC.
 *
 * The @maxsec conversion range argument controls the time frame in
 * seconds which must be covered by the runtime conversion with the
 * calculated mult and shift factors. This guarantees that no 64bit
 * overflow happens when the input value of the conversion is
 * multiplied with the calculated mult factor. Larger ranges may
 * reduce the conversion accuracy by choosing smaller mult and shift
 * factors.
 */

void clocks_calc_mult_shift(uint32_t *mult, uint32_t *shift, uint32_t from, uint32_t to, uint32_t maxsec)
{
	uint64_t tmp;
	uint32_t sft, sftacc = 32;

	/*
	 * Calculate the shift factor which is limiting the conversion
	 * range:
	 */
	tmp = ((uint64_t)maxsec * from) >> 32;
	while (tmp) {
		tmp >>= 1;
		sftacc--;
	}

	/*
	 * Find the conversion shift/mult pair which has the best
	 * accuracy and fits the maxsec conversion range:
	 */
	for (sft = 32; sft > 0; sft--) {
		tmp = (uint64_t) to << sft;
		tmp += from / 2;
		do_div(tmp, from);
		if ((tmp >> sftacc) == 0)
			break;
	}
	*mult = tmp;
	*shift = sft;
}


/**
 * clocksource_hz2mult - calculates mult from hz and shift
 * @hz:                 Clocksource frequency in Hz
 * @shift_constant:     Clocksource shift factor
 *
 * Helper functions that converts a hz counter
 * frequency to a timsource multiplier, given the
 * clocksource shift value
 */
uint32_t clocksource_hz2mult(uint32_t hz, uint32_t shift_constant)
{
	/*  hz = cyc/(Billion ns)
	 *  mult/2^shift  = ns/cyc
	 *  mult = ns/cyc * 2^shift
	 *  mult = 1Billion/hz * 2^shift
	 *  mult = 1000000000 * 2^shift / hz
	 *  mult = (1000000000<<shift) / hz
	 */
	uint64_t tmp = ((uint64_t)1000000000) << shift_constant;

	tmp += hz/2; /* round for do_div */
	do_div(tmp, hz);

	return (uint32_t)tmp;
}

int is_timeout_non_interruptible(uint64_t start_ns, uint64_t time_offset_ns)
{
	if ((int64_t)(start_ns + time_offset_ns - get_time_ns()) < 0)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_timeout_non_interruptible);

int is_timeout(uint64_t start_ns, uint64_t time_offset_ns)
{
	int ret = is_timeout_non_interruptible(start_ns, time_offset_ns);

	if (time_offset_ns >= 100 * USECOND)
		resched();

	return ret;
}
EXPORT_SYMBOL(is_timeout);

void ndelay(unsigned long nsecs)
{
	uint64_t start = get_time_ns();

	while(!is_timeout_non_interruptible(start, nsecs));
}
EXPORT_SYMBOL(ndelay);

void udelay(unsigned long usecs)
{
	uint64_t start = get_time_ns();

	while(!is_timeout(start, usecs * USECOND));
}
EXPORT_SYMBOL(udelay);

void mdelay(unsigned long msecs)
{
	udelay(msecs * USECOND);
}
EXPORT_SYMBOL(mdelay);

void mdelay_non_interruptible(unsigned long msecs)
{
	uint64_t start = get_time_ns();

	while (!is_timeout_non_interruptible(start, msecs * MSECOND))
		;
}
EXPORT_SYMBOL(mdelay_non_interruptible);

int init_clock(struct clocksource *cs)
{
	if (current_clock && cs->priority <= current_clock->priority)
		return 0;

	if (cs->init) {
		int ret;

		ret = cs->init(cs);
		if (ret)
			return ret;
	}

	/*
	 * If clocksource is freerunning it might have been running for a while
	 * before barebox started, we only care about the time spent in barebox
	 * thus we must discard the clocksource cycles up to this exact moment:
	 */
	cs->cycle_last = cs->read() & cs->mask;
	current_clock = cs;

	return 0;
}
