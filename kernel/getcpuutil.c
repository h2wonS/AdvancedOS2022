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

#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

#ifdef arch_idle_time
static u64 get_iowait_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 iowait;

	iowait = kcs->cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}
#else
static u64 get_iowait_time(struct kernel_cpustat *kcs, int cpu)
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

static long long int last_idle_time = 0;
static long long int last_elapsed_time = 0;

asmlinkage long __x64_sys_getcpuutil(void)
{
	int i, j;
	u64 user, nice, system, idle, iowait, irq, softirq, steal;
	long long int total_idle_time, total_elapsed_time;
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
		idle		+= get_idle_time(&kcpustat, i);
		iowait		+= get_iowait_time(&kcpustat, i);
		irq		+= cpustat[CPUTIME_IRQ];
		softirq		+= cpustat[CPUTIME_SOFTIRQ];
		steal		+= cpustat[CPUTIME_STEAL];
		sum		+= kstat_cpu_irqs_sum(i);
		sum		+= arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();
	total_elapsed_time = user + nice + system + idle + iowait + irq + softirq + steal - last_elapsed_time;
	total_idle_time = idle - last_idle_time;

	printk("Last Idle Time=%llu Total Idle Time=%llu\n", last_idle_time, total_idle_time);
	printk("Last Elapsed Time=%llu Total Elapsed Time=%llu\n", last_elapsed_time, total_elapsed_time);

	last_elapsed_time = total_elapsed_time;
	last_idle_time = total_idle_time;

	total_cpu_util = 10000 - (total_idle_time * 10000) / total_elapsed_time;
	printk("Total CPU Utilization=%d\n", total_cpu_util);

	return total_cpu_util;
}

