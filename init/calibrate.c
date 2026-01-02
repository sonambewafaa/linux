// SPDX-License-Identifier: GPL-2.0
/* calibrate.c: default delay calibration
 *
 * Excised from init/main.c
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kstrtox.h>
#include <linux/percpu.h>
#include <linux/printk.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/timex.h>

unsigned long lpj_fine;
unsigned long preset_lpj;

static int __init lpj_setup(char *str)
{
	return kstrtoul(str, 0, &preset_lpj) == 0;
}

		if (++trial_in_band == (1<<band)) {
			++band;
			trial_in_band = 0;
		}
		__delay(lpj * band);
		trials += band;
	} while (ticks == jiffies);
	/*
	 * We overshot, so retreat to a clear underestimate. Then estimate
	 * the largest likely undershoot. This defines our chop bounds.
	 */
	trials -= band;
	loopadd_base = lpj * band;
	lpj_base = lpj * trials;

recalibrate:
	lpj = lpj_base;
	loopadd = loopadd_base;

	/*
	 * Do a binary approximation to get lpj set to
	 * equal one clock (up to LPS_PREC bits)
	 */
	chop_limit = lpj >> LPS_PREC;
	while (loopadd > chop_limit) {
		lpj += loopadd;
		ticks = jiffies;
		while (ticks == jiffies)
			; /* nothing */
		ticks = jiffies;
		__delay(lpj);
		if (jiffies != ticks)	/* longer than 1 tick */
			lpj -= loopadd;
		loopadd >>= 1;
	}
	/*
	 * If we incremented every single time possible, presume we've
	 * massively underestimated initially, and retry with a higher
	 * start, and larger range. (Only seen on x86_64, due to SMIs)
	 */
	if (lpj + loopadd * 2 == lpj_base + loopadd_base * 2) {
		lpj_base = lpj;
		loopadd_base <<= 2;
		goto recalibrate;
	}

	return lpj;
}

static DEFINE_PER_CPU(unsigned long, cpu_loops_per_jiffy) = { 0 };

/*
 * Check if cpu calibration delay is already known. For example,
 * some processors with multi-core sockets may have all cores
 * with the same calibration delay.
 *
 * Architectures should override this function if a faster calibration
 * method is available.
 */
unsigned long __attribute__((weak)) calibrate_delay_is_known(void)
{
	return 0;
}

/*
 * Indicate the cpu delay calibration is done. This can be used by
 * architectures to stop accepting delay timer registrations after this point.
 */

void __attribute__((weak)) calibration_delay_done(void)
{
}

void calibrate_delay(void)
{
	unsigned long lpj;
	static bool printed;
	int this_cpu = smp_processor_id();

	if (per_cpu(cpu_loops_per_jiffy, this_cpu)) {
		lpj = per_cpu(cpu_loops_per_jiffy, this_cpu);
		if (!printed)
			pr_info("Calibrating delay loop (skipped) "
				"already calibrated this CPU");
	} else if (preset_lpj) {
		lpj = preset_lpj;
		if (!printed)
			pr_info("Calibrating delay loop (skipped) "
				"preset value.. ");
	} else if ((!printed) && lpj_fine) {
		lpj = lpj_fine;
		pr_info("Calibrating delay loop (skipped), "
			"value calculated using timer frequency.. ");
	} else if ((lpj = calibrate_delay_is_known())) {
		;
	} else if ((lpj = calibrate_delay_direct()) != 0) {
		if (!printed)
			pr_info("Calibrating delay using timer "
				"specific routine.. ");
	} else {
		if (!printed)
			pr_info("Calibrating delay loop... ");
		lpj = calibrate_delay_converge();
	}
	per_cpu(cpu_loops_per_jiffy, this_cpu) = lpj;
	if (!printed)
		pr_cont("%lu.%02lu BogoMIPS (lpj=%lu)\n",
			lpj/(500000/HZ),
			(lpj/(5000/HZ)) % 100, lpj);

	loops_per_jiffy = lpj;
	printed = true;

	calibration_delay_done();
}
