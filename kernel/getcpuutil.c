#include <linux/syscalls.h>
#include <linux/kernel.h>

#include <linux/linkage.h>
#include <linux/kernel_stat.h>


#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/stat.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/time_namespace.h>
#include <linux/irqnr.h>
#include <linux/sched/cputime.h>
#include <linux/tick.h>

#include <asm/fpu/api.h> // for floating point operation

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

#ifdef arch_idle_time

u64 calc_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle;

	idle = kcs->cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static u64 calc_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait;

	iowait = kcs->cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

u64 calc_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcs->cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static u64 calc_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait, iowait_usecs = -1ULL;

	if (cpu_online(cpu))
		iowait_usecs = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcs->cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_usecs * NSEC_PER_USEC;

	return iowait;
}

#endif

static int calc_cpu_util(void)
{
	int i, j;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	u64 total_idle_time, total_elapsed_time;
	int quotient, remainder;
        int total_cpu_util; 
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
	struct timespec64 boottime;

	user = nice = system = idle = iowait =
		irq = softirq = steal = 0;
	getboottime64(&boottime);
	/* shift boot timestamp according to the timens offset */
	timens_sub_boottime(&boottime);

	for_each_possible_cpu(i) {
		struct kernel_cpustat kcpustat;
		u64 *cpustat = kcpustat.cpustat;

		kcpustat_cpu_fetch(&kcpustat, i);

		user		+= cpustat[CPUTIME_USER];
		nice		+= cpustat[CPUTIME_NICE];
		system		+= cpustat[CPUTIME_SYSTEM];
		idle		+= calc_idle_time(&kcpustat, i);
		iowait		+= calc_iowait_time(&kcpustat, i);
		irq		+= cpustat[CPUTIME_IRQ];
		softirq		+= cpustat[CPUTIME_SOFTIRQ];
		steal		+= cpustat[CPUTIME_STEAL];

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();
	total_elapsed_time = user + nice + system + idle + iowait + irq + softirq + steal;
	total_idle_time = idle;

	if (unlikely(!irq_fpu_usable())) {
		total_cpu_util = -1; 
		printk("Unusable IRQ FPU\n");
		return total_cpu_util; // cannot use floating point execution
	}
	else {
		kernel_fpu_begin();
		printk("Total Idle Time=%llu\n", total_idle_time);
		printk("Total Elapsed Time=%llu\n", total_elapsed_time);
		quotient = total_idle_time / total_elapsed_time;
		remainder = (total_idle_time * 10000) / total_elapsed_time - quotient * 10000;
		total_cpu_util = 10000 - (quotient + remainder) ;
		printk("Total CPU Utilization=%d Q=%d, R=%d\n", total_cpu_util, quotient, remainder);
		kernel_fpu_end();
	return total_cpu_util;
	}
}


SYSCALL_DEFINE0(getcpuutil){
	int cpu_util = calc_cpu_util();
	printk("Hello This is GetCPUUtil, Your Util %d\n", cpu_util);
	return cpu_util;
}
