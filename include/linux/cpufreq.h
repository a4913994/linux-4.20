/*
 * linux/include/linux/cpufreq.h
 *
 * Copyright (C) 2001 Russell King
 *           (C) 2002 - 2003 Dominik Brodowski <linux@brodo.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _LINUX_CPUFREQ_H
#define _LINUX_CPUFREQ_H

#include <linux/clk.h>
#include <linux/cpumask.h>
#include <linux/completion.h>
#include <linux/kobject.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>

/*********************************************************************
 *                        CPUFREQ INTERFACE CPU频率接口               *
 *********************************************************************/
/*
 * Frequency values here are CPU kHz 
 * 这里的频率值是 CPU kHz
 *
 * Maximum transition latency is in nanoseconds - if it's unknown,
 * CPUFREQ_ETERNAL shall be used.
 * 最大的过渡延迟以纳秒为单位 - 如果不知道，请使用 CPUFREQ_ETERNAL。
 */

// CPUFREQ_ETERNAL：此常量的值为-1，表示CPU频率永远不会发生变化。这在某些场景下可能会被用到，例如固定频率的CPU或不支持动态频率调整的硬件。
#define CPUFREQ_ETERNAL			(-1)
// CPUFREQ_NAME_LEN：此常量的值为16，用于定义CPU频率调节策略或驱动名称的最大长度。这是一个预定义的限制，以确保内核中的相关数据结构不会变得过大。
#define CPUFREQ_NAME_LEN		16
/* Print length for names. Extra 1 space for accommodating '\n' in prints */
// CPUFREQ_NAME_PLEN：此常量的值为CPUFREQ_NAME_LEN + 1，即17。它为打印CPU频率调节策略或驱动名称预留了额外的一个字符，用于容纳换行符（'\n'）。这使得在打印名称时，换行符可以正确地放置在字符串末尾，以保持输出的整洁和可读性。
#define CPUFREQ_NAME_PLEN		(CPUFREQ_NAME_LEN + 1)

struct cpufreq_governor;

// 定义了CPU频率表的排序状态
enum cpufreq_table_sorting {
	// CPUFREQ_TABLE_UNSORTED：表示频率表未排序。在此状态下，频率表中的频率值可能是无序的，因此在查找特定频率时可能需要进行线性搜索。
	CPUFREQ_TABLE_UNSORTED,
	// CPUFREQ_TABLE_SORTED_ASCENDING：表示频率表按升序排序。在此状态下，频率表中的频率值按从最小到最大的顺序排列。这使得在查找特定频率时可以使用更高效的搜索算法，如二分查找。
	CPUFREQ_TABLE_SORTED_ASCENDING,
	// CPUFREQ_TABLE_SORTED_DESCENDING：表示频率表按降序排序。在此状态下，频率表中的频率值按从最大到最小的顺序排列。这同样允许在查找特定频率时使用更高效的搜索算法。
	CPUFREQ_TABLE_SORTED_DESCENDING
};

// cpufreq_freqs结构体表示在CPU频率变更期间的相关信息。在频率变更期间传递给回调函数，以提供有关即将发生的变更的详细信息
struct cpufreq_freqs {
	// cpu：表示发生频率变更的CPU编号
	unsigned int cpu;	/* cpu nr */
	// old：表示变更前的CPU频率（以千赫兹为单位）
	unsigned int old;
	// new：表示变更后的CPU频率（以千赫兹为单位）
	unsigned int new;
	// flags：表示cpufreq_driver的标志，与驱动程序相关
	u8 flags;		/* flags of cpufreq_driver, see below. */
};

// cpufreq_cpuinfo结构体包含了有关CPU的频率信息
struct cpufreq_cpuinfo {
	// max_freq：表示该CPU支持的最大频率（以千赫兹为单位）
	unsigned int		max_freq;
	// min_freq：表示该CPU支持的最小频率（以千赫兹为单位）
	unsigned int		min_freq;
	/* in 10^(-9) s = nanoseconds */
	// transition_latency：表示从一个频率切换到另一个频率所需的时间（以纳秒为单位）
	unsigned int		transition_latency;
};

// cpufreq_user_policy结构体表示用户定义的CPU频率调整策略
struct cpufreq_user_policy {
	// min：用户定义的最小CPU频率（以千赫兹为单位）
	unsigned int		min;    /* in kHz */
	// max：用户定义的最大CPU频率（以千赫兹为单位）
	unsigned int		max;    /* in kHz */
};

// 表示Linux内核中的一个CPU频率调节策略实例
struct cpufreq_policy {
	/* CPUs sharing clock, require sw coordination */
	// 表示与此策略相关的在线CPU集合。
	cpumask_var_t		cpus;	/* Online CPUs only */
	// 与此策略相关的在线和离线CPU集合
	cpumask_var_t		related_cpus; /* Online + Offline CPUs */
	// 与此策略相关的存在的CPU集合
	cpumask_var_t		real_cpus; /* Related and present */

	// 该策略是否在受影响的CPU之间共享。
	unsigned int		shared_type; /* ACPI: ANY or ALL affected CPUs
						should set cpufreq */
	// 表示管理此策略的在线CPU。
	unsigned int		cpu;    /* cpu managing this policy, must be online */

	// 指向CPU时钟的指针
	struct clk		*clk;
	// 包含有关CPU的频率信息
	struct cpufreq_cpuinfo	cpuinfo;/* see above */
	// 表示允许的最小CPU频率（以千赫兹为单位）。
	unsigned int		min;    /* in kHz */
	// 表示允许的最大CPU频率（以千赫兹为单位）。
	unsigned int		max;    /* in kHz */
	// 表示当前CPU频率（以千赫兹为单位），如果使用cpufreq调度器，此值是必需的
	unsigned int		cur;    /* in kHz, only needed if cpufreq
					 * governors are used */
	// 在转换之前等于policy->cur的值。
	unsigned int		restore_freq; /* = policy->cur before transition */
	// 在挂起期间设置的频率。
	unsigned int		suspend_freq; /* freq to set during suspend */
	// 表示此策略的类型。
	unsigned int		policy; /* see above */
	// 在卸载之前使用的策略。
	unsigned int		last_policy; /* policy before unplug */
	// 指向当前使用的调度器的指针。
	struct cpufreq_governor	*governor; /* see below */
	// 一个指向调度器私有数据的指针
	void			*governor_data;
	// 表示最后使用的调度器名称。
	char			last_governor[CPUFREQ_NAME_LEN]; /* last governor used */
	// 如果需要调用update_policy()但当前处于IRQ上下文，将使用此工作结构
	struct work_struct	update; /* if update_policy() needs to be
					 * called, but you're in IRQ context */
	// 表示用户策略的结构体。
	struct cpufreq_user_policy user_policy;
	// 指向频率表的指针。
	struct cpufreq_frequency_table	*freq_table;
	// 表示频率表的排序状态。
	enum cpufreq_table_sorting freq_table_sorted;

	// 用于将此策略添加到策略列表中的列表头。
	struct list_head        policy_list;
	// 用于sysfs接口的内核对象。
	struct kobject		kobj;
	// 表示内核对象注销完成的完成结构。
	struct completion	kobj_unregister;

	/*
	 * The rules for this semaphore:
	 * - Any routine that wants to read from the policy structure will
	 *   do a down_read on this semaphore.
	 * - Any routine that will write to the policy structure and/or may take away
	 *   the policy altogether (eg. CPU hotplug), will hold this lock in write
	 *   mode before doing so.
	 */
	// 用于保护策略结构的读写信号量。
	struct rw_semaphore	rwsem;

	/*
	 * Fast switch flags:
	 * - fast_switch_possible should be set by the driver if it can
	 *   guarantee that frequency can be changed on any CPU sharing the
	 *   policy and that the change will affect all of the policy CPUs then.
	 * - fast_switch_enabled is to be set by governors that support fast
	 *   frequency switching with the help of cpufreq_enable_fast_switch().
	 */
	// 如果驱动程序可以快速切换频率，应将其设置为true。
	bool			fast_switch_possible;
	// 如果支持快速频率切换的调度器启用了快速切换，将其设置为true
	bool			fast_switch_enabled;

	/*
	 * Preferred average time interval between consecutive invocations of
	 * the driver to set the frequency for this policy.  To be set by the
	 * scaling driver (0, which is the default, means no preference).
	 */
	// 表示驱动程序在连续设置频率之间的首选平均时间间隔。由调度驱动程序设置（默认值为0，表示无特定偏好）。
	unsigned int		transition_delay_us;

	/*
	 * Remote DVFS flag (Not added to the driver structure as we don't want
	 * to access another structure from scheduler hotpath).
	 *
	 * Should be set if CPUs can do DVFS on behalf of other CPUs from
	 * different cpufreq policies.
	 */
	// 如果CPU可以代表不同cpufreq策略的其他CPU进行DVFS（动态电压频率调整），则设置此标志。
	bool			dvfs_possible_from_any_cpu;

	 /* Cached frequency lookup from cpufreq_driver_resolve_freq. */
	 // 从cpufreq_driver_resolve_freq缓存的频率查找。
	unsigned int cached_target_freq;
	// 缓存的已解析索引。
	int cached_resolved_idx;

	/* Synchronization for frequency transitions */
	// 跟踪转换状态。
	bool			transition_ongoing; /* Tracks transition status */
	// 用于保护频率转换的自旋锁。
	spinlock_t		transition_lock;
	// 等待队列头，用于等待频率转换完成。
	wait_queue_head_t	transition_wait;
	// 执行转换的任务指针。
	struct task_struct	*transition_task; /* Task which is doing the transition */

	/* cpufreq-stats */
	// 指向cpufreq统计信息的指针。
	struct cpufreq_stats	*stats;

	/* For cpufreq driver's internal use */
	// 指向驱动程序私有数据的指针，供驱动程序内部使用。
	void			*driver_data;
};

/* Only for ACPI */
// 这些宏定义表示ACPI（高级配置与电源接口）下的CPU频率共享类型
// 它们描述了如何在共享相同硬件时钟的多个CPU之间协调频率设置
// CPUFREQ_SHARED_TYPE_NONE (0)：表示没有共享类型，也就是说每个CPU独立地设置频率，不需要协调。
#define CPUFREQ_SHARED_TYPE_NONE (0) /* None */
// CPUFREQ_SHARED_TYPE_HW (1)：表示硬件负责必要的协调。在这种情况下，硬件会自动确保在共享时钟的所有CPU之间同步频率设置。
#define CPUFREQ_SHARED_TYPE_HW	 (1) /* HW does needed coordination */
// CPUFREQ_SHARED_TYPE_ALL (2)：表示所有依赖的CPU都应设置频率。在这种情况下，当一个CPU尝试更改频率时，所有共享相同硬件时钟的其他CPU也必须相应地更改频率。
#define CPUFREQ_SHARED_TYPE_ALL	 (2) /* All dependent CPUs should set freq */
// CPUFREQ_SHARED_TYPE_ANY (3)：表示频率可以从任何依赖的CPU设置。在这种情况下，任何一个共享相同硬件时钟的CPU都可以设置频率，同时其他CPU会自动适应这个变化。
#define CPUFREQ_SHARED_TYPE_ANY	 (3) /* Freq can be set from any dependent CPU*/

#ifdef CONFIG_CPU_FREQ
struct cpufreq_policy *cpufreq_cpu_get_raw(unsigned int cpu);
struct cpufreq_policy *cpufreq_cpu_get(unsigned int cpu);
void cpufreq_cpu_put(struct cpufreq_policy *policy);
#else
static inline struct cpufreq_policy *cpufreq_cpu_get_raw(unsigned int cpu)
{
	return NULL;
}
static inline struct cpufreq_policy *cpufreq_cpu_get(unsigned int cpu)
{
	return NULL;
}
static inline void cpufreq_cpu_put(struct cpufreq_policy *policy) { }
#endif

static inline bool policy_is_shared(struct cpufreq_policy *policy)
{
	return cpumask_weight(policy->cpus) > 1;
}

/* /sys/devices/system/cpu/cpufreq: entry point for global variables */
extern struct kobject *cpufreq_global_kobject;

#ifdef CONFIG_CPU_FREQ
unsigned int cpufreq_get(unsigned int cpu);
unsigned int cpufreq_quick_get(unsigned int cpu);
unsigned int cpufreq_quick_get_max(unsigned int cpu);
void disable_cpufreq(void);

u64 get_cpu_idle_time(unsigned int cpu, u64 *wall, int io_busy);
int cpufreq_get_policy(struct cpufreq_policy *policy, unsigned int cpu);
void cpufreq_update_policy(unsigned int cpu);
bool have_governor_per_policy(void);
struct kobject *get_governor_parent_kobj(struct cpufreq_policy *policy);
void cpufreq_enable_fast_switch(struct cpufreq_policy *policy);
void cpufreq_disable_fast_switch(struct cpufreq_policy *policy);
#else
static inline unsigned int cpufreq_get(unsigned int cpu)
{
	return 0;
}
static inline unsigned int cpufreq_quick_get(unsigned int cpu)
{
	return 0;
}
static inline unsigned int cpufreq_quick_get_max(unsigned int cpu)
{
	return 0;
}
static inline void disable_cpufreq(void) { }
#endif

#ifdef CONFIG_CPU_FREQ_STAT
void cpufreq_stats_create_table(struct cpufreq_policy *policy);
void cpufreq_stats_free_table(struct cpufreq_policy *policy);
void cpufreq_stats_record_transition(struct cpufreq_policy *policy,
				     unsigned int new_freq);
#else
static inline void cpufreq_stats_create_table(struct cpufreq_policy *policy) { }
static inline void cpufreq_stats_free_table(struct cpufreq_policy *policy) { }
static inline void cpufreq_stats_record_transition(struct cpufreq_policy *policy,
						   unsigned int new_freq) { }
#endif /* CONFIG_CPU_FREQ_STAT */

/*********************************************************************
 *                      CPUFREQ DRIVER INTERFACE                     *
 *********************************************************************/

#define CPUFREQ_RELATION_L 0  /* lowest frequency at or above target */
#define CPUFREQ_RELATION_H 1  /* highest frequency below or at target */
#define CPUFREQ_RELATION_C 2  /* closest frequency to target */

struct freq_attr {
	struct attribute attr;
	ssize_t (*show)(struct cpufreq_policy *, char *);
	ssize_t (*store)(struct cpufreq_policy *, const char *, size_t count);
};

#define cpufreq_freq_attr_ro(_name)		\
static struct freq_attr _name =			\
__ATTR(_name, 0444, show_##_name, NULL)

#define cpufreq_freq_attr_ro_perm(_name, _perm)	\
static struct freq_attr _name =			\
__ATTR(_name, _perm, show_##_name, NULL)

#define cpufreq_freq_attr_rw(_name)		\
static struct freq_attr _name =			\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define cpufreq_freq_attr_wo(_name)		\
static struct freq_attr _name =			\
__ATTR(_name, 0200, NULL, store_##_name)

struct global_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj,
			struct attribute *attr, char *buf);
	ssize_t (*store)(struct kobject *a, struct attribute *b,
			 const char *c, size_t count);
};

#define define_one_global_ro(_name)		\
static struct global_attr _name =		\
__ATTR(_name, 0444, show_##_name, NULL)

#define define_one_global_rw(_name)		\
static struct global_attr _name =		\
__ATTR(_name, 0644, show_##_name, store_##_name)


struct cpufreq_driver {
	char		name[CPUFREQ_NAME_LEN];
	u8		flags;
	void		*driver_data;

	/* needed by all drivers */
	int		(*init)(struct cpufreq_policy *policy);
	int		(*verify)(struct cpufreq_policy *policy);

	/* define one out of two */
	int		(*setpolicy)(struct cpufreq_policy *policy);

	/*
	 * On failure, should always restore frequency to policy->restore_freq
	 * (i.e. old freq).
	 */
	int		(*target)(struct cpufreq_policy *policy,
				  unsigned int target_freq,
				  unsigned int relation);	/* Deprecated */
	int		(*target_index)(struct cpufreq_policy *policy,
					unsigned int index);
	unsigned int	(*fast_switch)(struct cpufreq_policy *policy,
				       unsigned int target_freq);

	/*
	 * Caches and returns the lowest driver-supported frequency greater than
	 * or equal to the target frequency, subject to any driver limitations.
	 * Does not set the frequency. Only to be implemented for drivers with
	 * target().
	 */
	unsigned int	(*resolve_freq)(struct cpufreq_policy *policy,
					unsigned int target_freq);

	/*
	 * Only for drivers with target_index() and CPUFREQ_ASYNC_NOTIFICATION
	 * unset.
	 *
	 * get_intermediate should return a stable intermediate frequency
	 * platform wants to switch to and target_intermediate() should set CPU
	 * to to that frequency, before jumping to the frequency corresponding
	 * to 'index'. Core will take care of sending notifications and driver
	 * doesn't have to handle them in target_intermediate() or
	 * target_index().
	 *
	 * Drivers can return '0' from get_intermediate() in case they don't
	 * wish to switch to intermediate frequency for some target frequency.
	 * In that case core will directly call ->target_index().
	 */
	unsigned int	(*get_intermediate)(struct cpufreq_policy *policy,
					    unsigned int index);
	int		(*target_intermediate)(struct cpufreq_policy *policy,
					       unsigned int index);

	/* should be defined, if possible */
	unsigned int	(*get)(unsigned int cpu);

	/* optional */
	int		(*bios_limit)(int cpu, unsigned int *limit);

	int		(*exit)(struct cpufreq_policy *policy);
	void		(*stop_cpu)(struct cpufreq_policy *policy);
	int		(*suspend)(struct cpufreq_policy *policy);
	int		(*resume)(struct cpufreq_policy *policy);

	/* Will be called after the driver is fully initialized */
	void		(*ready)(struct cpufreq_policy *policy);

	struct freq_attr **attr;

	/* platform specific boost support code */
	bool		boost_enabled;
	int		(*set_boost)(int state);
};

/* flags */
#define CPUFREQ_STICKY		(1 << 0)	/* driver isn't removed even if
						   all ->init() calls failed */
#define CPUFREQ_CONST_LOOPS	(1 << 1)	/* loops_per_jiffy or other
						   kernel "constants" aren't
						   affected by frequency
						   transitions */
#define CPUFREQ_PM_NO_WARN	(1 << 2)	/* don't warn on suspend/resume
						   speed mismatches */

/*
 * This should be set by platforms having multiple clock-domains, i.e.
 * supporting multiple policies. With this sysfs directories of governor would
 * be created in cpu/cpu<num>/cpufreq/ directory and so they can use the same
 * governor with different tunables for different clusters.
 */
#define CPUFREQ_HAVE_GOVERNOR_PER_POLICY (1 << 3)

/*
 * Driver will do POSTCHANGE notifications from outside of their ->target()
 * routine and so must set cpufreq_driver->flags with this flag, so that core
 * can handle them specially.
 */
#define CPUFREQ_ASYNC_NOTIFICATION  (1 << 4)

/*
 * Set by drivers which want cpufreq core to check if CPU is running at a
 * frequency present in freq-table exposed by the driver. For these drivers if
 * CPU is found running at an out of table freq, we will try to set it to a freq
 * from the table. And if that fails, we will stop further boot process by
 * issuing a BUG_ON().
 */
#define CPUFREQ_NEED_INITIAL_FREQ_CHECK	(1 << 5)

/*
 * Set by drivers to disallow use of governors with "dynamic_switching" flag
 * set.
 */
#define CPUFREQ_NO_AUTO_DYNAMIC_SWITCHING (1 << 6)

int cpufreq_register_driver(struct cpufreq_driver *driver_data);
int cpufreq_unregister_driver(struct cpufreq_driver *driver_data);

const char *cpufreq_get_current_driver(void);
void *cpufreq_get_driver_data(void);

static inline void cpufreq_verify_within_limits(struct cpufreq_policy *policy,
		unsigned int min, unsigned int max)
{
	if (policy->min < min)
		policy->min = min;
	if (policy->max < min)
		policy->max = min;
	if (policy->min > max)
		policy->min = max;
	if (policy->max > max)
		policy->max = max;
	if (policy->min > policy->max)
		policy->min = policy->max;
	return;
}

static inline void
cpufreq_verify_within_cpu_limits(struct cpufreq_policy *policy)
{
	cpufreq_verify_within_limits(policy, policy->cpuinfo.min_freq,
			policy->cpuinfo.max_freq);
}

#ifdef CONFIG_CPU_FREQ
void cpufreq_suspend(void);
void cpufreq_resume(void);
int cpufreq_generic_suspend(struct cpufreq_policy *policy);
#else
static inline void cpufreq_suspend(void) {}
static inline void cpufreq_resume(void) {}
#endif

/*********************************************************************
 *                     CPUFREQ NOTIFIER INTERFACE                    *
 *********************************************************************/

#define CPUFREQ_TRANSITION_NOTIFIER	(0)
#define CPUFREQ_POLICY_NOTIFIER		(1)

/* Transition notifiers */
#define CPUFREQ_PRECHANGE		(0)
#define CPUFREQ_POSTCHANGE		(1)

/* Policy Notifiers  */
#define CPUFREQ_ADJUST			(0)
#define CPUFREQ_NOTIFY			(1)

#ifdef CONFIG_CPU_FREQ
int cpufreq_register_notifier(struct notifier_block *nb, unsigned int list);
int cpufreq_unregister_notifier(struct notifier_block *nb, unsigned int list);

void cpufreq_freq_transition_begin(struct cpufreq_policy *policy,
		struct cpufreq_freqs *freqs);
void cpufreq_freq_transition_end(struct cpufreq_policy *policy,
		struct cpufreq_freqs *freqs, int transition_failed);

#else /* CONFIG_CPU_FREQ */
static inline int cpufreq_register_notifier(struct notifier_block *nb,
						unsigned int list)
{
	return 0;
}
static inline int cpufreq_unregister_notifier(struct notifier_block *nb,
						unsigned int list)
{
	return 0;
}
#endif /* !CONFIG_CPU_FREQ */

/**
 * cpufreq_scale - "old * mult / div" calculation for large values (32-bit-arch
 * safe)
 * @old:   old value
 * @div:   divisor
 * @mult:  multiplier
 *
 *
 * new = old * mult / div
 */
static inline unsigned long cpufreq_scale(unsigned long old, u_int div,
		u_int mult)
{
#if BITS_PER_LONG == 32
	u64 result = ((u64) old) * ((u64) mult);
	do_div(result, div);
	return (unsigned long) result;

#elif BITS_PER_LONG == 64
	unsigned long result = old * ((u64) mult);
	result /= div;
	return result;
#endif
}

/*********************************************************************
 *                          CPUFREQ GOVERNORS                        *
 *********************************************************************/

/*
 * If (cpufreq_driver->target) exists, the ->governor decides what frequency
 * within the limits is used. If (cpufreq_driver->setpolicy> exists, these
 * two generic policies are available:
 */
#define CPUFREQ_POLICY_POWERSAVE	(1)
#define CPUFREQ_POLICY_PERFORMANCE	(2)

/*
 * The polling frequency depends on the capability of the processor. Default
 * polling frequency is 1000 times the transition latency of the processor. The
 * ondemand governor will work on any processor with transition latency <= 10ms,
 * using appropriate sampling rate.
 */
#define LATENCY_MULTIPLIER		(1000)

// cpufreq_governor表示Linux内核中的一个CPU频率调节策略（也称为“调度器”), 用于实现不同的CPU频率调节策略
/**
 * 常见的CPU频率调节策略包括：
 * 性能（performance）：始终保持最高的CPU频率，以获得最佳性能。
 * 电源节能（powersave）：尽可能使用较低的CPU频率，以节省电源。
 * 用户空间（userspace）：允许用户空间应用程序直接设置CPU频率。
 * 按需（ondemand）：根据CPU负载动态调整CPU频率，以在性能和功耗之间找到平衡。
 * 保守（conservative）：类似于按需策略，但更倾向于使用较低的频率，以节省电源。
 */
struct cpufreq_governor {
	// name[CPUFREQ_NAME_LEN]：这是一个字符数组，用于存储调度器的名称。其长度由之前定义的常量CPUFREQ_NAME_LEN决定。
	char	name[CPUFREQ_NAME_LEN];
	// 指向初始化函数的指针。当启用此调度器时，此函数将被调用。它接受一个cpufreq_policy结构体指针作为参数。
	int	(*init)(struct cpufreq_policy *policy);
	// 指向退出函数的指针。当禁用此调度器时，此函数将被调用。它也接受一个cpufreq_policy结构体指针作为参数。
	void	(*exit)(struct cpufreq_policy *policy);
	// 指向启动函数的指针。当CPU频率调节策略开始运行时，此函数将被调用。它也接受一个cpufreq_policy结构体指针作为参数。
	int	(*start)(struct cpufreq_policy *policy);
	// 指向停止函数的指针。当CPU频率调节策略停止运行时，此函数将被调用。它也接受一个cpufreq_policy结构体指针作为参数。
	void	(*stop)(struct cpufreq_policy *policy);
	// 指向限制函数的指针。当调整CPU频率的上限或下限时，此函数将被调用。它也接受一个cpufreq_policy结构体指针作为参数。
	void	(*limits)(struct cpufreq_policy *policy);
	// 指向展示设定速度函数的指针。当需要显示当前设定的CPU频率时，此函数将被调用。它接受一个cpufreq_policy结构体指针和一个字符缓冲区指针作为参数。
	ssize_t	(*show_setspeed)	(struct cpufreq_policy *policy,
					 char *buf);
	// 指向存储设定速度函数的指针。当需要设定新的CPU频率时，此函数将被调用。它接受一个cpufreq_policy结构体指针和一个无符号整数作为参数。
	int	(*store_setspeed)	(struct cpufreq_policy *policy,
					 unsigned int freq);
	/* For governors which change frequency dynamically by themselves */
	// 表示是否为动态切换类型的调度器。动态切换类型的调度器会自行调整CPU频率。
	bool			dynamic_switching;
	// 用于将这个调度器添加到内核中的调度器列表中。
	struct list_head	governor_list;
	// 表示这个调度器所属的内核模块。当调度器作为一个内核模块被加载时，这个指针将被设置为对应的模块。这有助于处理模块的引用计数，以确保在调度器仍在使用时，模块不会被意外卸载
	struct module		*owner;
};

/* Pass a target to the cpufreq driver */
unsigned int cpufreq_driver_fast_switch(struct cpufreq_policy *policy,
					unsigned int target_freq);
int cpufreq_driver_target(struct cpufreq_policy *policy,
				 unsigned int target_freq,
				 unsigned int relation);
int __cpufreq_driver_target(struct cpufreq_policy *policy,
				   unsigned int target_freq,
				   unsigned int relation);
unsigned int cpufreq_driver_resolve_freq(struct cpufreq_policy *policy,
					 unsigned int target_freq);
unsigned int cpufreq_policy_transition_delay_us(struct cpufreq_policy *policy);
int cpufreq_register_governor(struct cpufreq_governor *governor);
void cpufreq_unregister_governor(struct cpufreq_governor *governor);

struct cpufreq_governor *cpufreq_default_governor(void);
struct cpufreq_governor *cpufreq_fallback_governor(void);

static inline void cpufreq_policy_apply_limits(struct cpufreq_policy *policy)
{
	if (policy->max < policy->cur)
		__cpufreq_driver_target(policy, policy->max, CPUFREQ_RELATION_H);
	else if (policy->min > policy->cur)
		__cpufreq_driver_target(policy, policy->min, CPUFREQ_RELATION_L);
}

/* Governor attribute set */
struct gov_attr_set {
	struct kobject kobj;
	struct list_head policy_list;
	struct mutex update_lock;
	int usage_count;
};

/* sysfs ops for cpufreq governors */
extern const struct sysfs_ops governor_sysfs_ops;

void gov_attr_set_init(struct gov_attr_set *attr_set, struct list_head *list_node);
void gov_attr_set_get(struct gov_attr_set *attr_set, struct list_head *list_node);
unsigned int gov_attr_set_put(struct gov_attr_set *attr_set, struct list_head *list_node);

/* Governor sysfs attribute */
struct governor_attr {
	struct attribute attr;
	ssize_t (*show)(struct gov_attr_set *attr_set, char *buf);
	ssize_t (*store)(struct gov_attr_set *attr_set, const char *buf,
			 size_t count);
};

static inline bool cpufreq_this_cpu_can_update(struct cpufreq_policy *policy)
{
	/*
	 * Allow remote callbacks if:
	 * - dvfs_possible_from_any_cpu flag is set
	 * - the local and remote CPUs share cpufreq policy
	 */
	return policy->dvfs_possible_from_any_cpu ||
		cpumask_test_cpu(smp_processor_id(), policy->cpus);
}

/*********************************************************************
 *                     FREQUENCY TABLE HELPERS                       *
 *********************************************************************/

/* Special Values of .frequency field */
#define CPUFREQ_ENTRY_INVALID	~0u
#define CPUFREQ_TABLE_END	~1u
/* Special Values of .flags field */
#define CPUFREQ_BOOST_FREQ	(1 << 0)

struct cpufreq_frequency_table {
	unsigned int	flags;
	unsigned int	driver_data; /* driver specific data, not used by core */
	unsigned int	frequency; /* kHz - doesn't need to be in ascending
				    * order */
};

#if defined(CONFIG_CPU_FREQ) && defined(CONFIG_PM_OPP)
int dev_pm_opp_init_cpufreq_table(struct device *dev,
				  struct cpufreq_frequency_table **table);
void dev_pm_opp_free_cpufreq_table(struct device *dev,
				   struct cpufreq_frequency_table **table);
#else
static inline int dev_pm_opp_init_cpufreq_table(struct device *dev,
						struct cpufreq_frequency_table
						**table)
{
	return -EINVAL;
}

static inline void dev_pm_opp_free_cpufreq_table(struct device *dev,
						 struct cpufreq_frequency_table
						 **table)
{
}
#endif

/*
 * cpufreq_for_each_entry -	iterate over a cpufreq_frequency_table
 * @pos:	the cpufreq_frequency_table * to use as a loop cursor.
 * @table:	the cpufreq_frequency_table * to iterate over.
 */

#define cpufreq_for_each_entry(pos, table)	\
	for (pos = table; pos->frequency != CPUFREQ_TABLE_END; pos++)

/*
 * cpufreq_for_each_entry_idx -	iterate over a cpufreq_frequency_table
 *	with index
 * @pos:	the cpufreq_frequency_table * to use as a loop cursor.
 * @table:	the cpufreq_frequency_table * to iterate over.
 * @idx:	the table entry currently being processed
 */

#define cpufreq_for_each_entry_idx(pos, table, idx)	\
	for (pos = table, idx = 0; pos->frequency != CPUFREQ_TABLE_END; \
		pos++, idx++)

/*
 * cpufreq_for_each_valid_entry -     iterate over a cpufreq_frequency_table
 *	excluding CPUFREQ_ENTRY_INVALID frequencies.
 * @pos:        the cpufreq_frequency_table * to use as a loop cursor.
 * @table:      the cpufreq_frequency_table * to iterate over.
 */

#define cpufreq_for_each_valid_entry(pos, table)			\
	for (pos = table; pos->frequency != CPUFREQ_TABLE_END; pos++)	\
		if (pos->frequency == CPUFREQ_ENTRY_INVALID)		\
			continue;					\
		else

/*
 * cpufreq_for_each_valid_entry_idx -     iterate with index over a cpufreq
 *	frequency_table excluding CPUFREQ_ENTRY_INVALID frequencies.
 * @pos:	the cpufreq_frequency_table * to use as a loop cursor.
 * @table:	the cpufreq_frequency_table * to iterate over.
 * @idx:	the table entry currently being processed
 */

#define cpufreq_for_each_valid_entry_idx(pos, table, idx)		\
	cpufreq_for_each_entry_idx(pos, table, idx)			\
		if (pos->frequency == CPUFREQ_ENTRY_INVALID)		\
			continue;					\
		else


int cpufreq_frequency_table_cpuinfo(struct cpufreq_policy *policy,
				    struct cpufreq_frequency_table *table);

int cpufreq_frequency_table_verify(struct cpufreq_policy *policy,
				   struct cpufreq_frequency_table *table);
int cpufreq_generic_frequency_table_verify(struct cpufreq_policy *policy);

int cpufreq_table_index_unsorted(struct cpufreq_policy *policy,
				 unsigned int target_freq,
				 unsigned int relation);
int cpufreq_frequency_table_get_index(struct cpufreq_policy *policy,
		unsigned int freq);

ssize_t cpufreq_show_cpus(const struct cpumask *mask, char *buf);

#ifdef CONFIG_CPU_FREQ
int cpufreq_boost_trigger_state(int state);
int cpufreq_boost_enabled(void);
int cpufreq_enable_boost_support(void);
bool policy_has_boost_freq(struct cpufreq_policy *policy);

/* Find lowest freq at or above target in a table in ascending order */
static inline int cpufreq_table_find_index_al(struct cpufreq_policy *policy,
					      unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = policy->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq >= target_freq)
			return idx;

		best = idx;
	}

	return best;
}

/* Find lowest freq at or above target in a table in descending order */
static inline int cpufreq_table_find_index_dl(struct cpufreq_policy *policy,
					      unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = policy->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq == target_freq)
			return idx;

		if (freq > target_freq) {
			best = idx;
			continue;
		}

		/* No freq found above target_freq */
		if (best == -1)
			return idx;

		return best;
	}

	return best;
}

/* Works only on sorted freq-tables */
static inline int cpufreq_table_find_index_l(struct cpufreq_policy *policy,
					     unsigned int target_freq)
{
	target_freq = clamp_val(target_freq, policy->min, policy->max);

	if (policy->freq_table_sorted == CPUFREQ_TABLE_SORTED_ASCENDING)
		return cpufreq_table_find_index_al(policy, target_freq);
	else
		return cpufreq_table_find_index_dl(policy, target_freq);
}

/* Find highest freq at or below target in a table in ascending order */
static inline int cpufreq_table_find_index_ah(struct cpufreq_policy *policy,
					      unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = policy->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq == target_freq)
			return idx;

		if (freq < target_freq) {
			best = idx;
			continue;
		}

		/* No freq found below target_freq */
		if (best == -1)
			return idx;

		return best;
	}

	return best;
}

/* Find highest freq at or below target in a table in descending order */
static inline int cpufreq_table_find_index_dh(struct cpufreq_policy *policy,
					      unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = policy->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq <= target_freq)
			return idx;

		best = idx;
	}

	return best;
}

/* Works only on sorted freq-tables */
static inline int cpufreq_table_find_index_h(struct cpufreq_policy *policy,
					     unsigned int target_freq)
{
	target_freq = clamp_val(target_freq, policy->min, policy->max);

	if (policy->freq_table_sorted == CPUFREQ_TABLE_SORTED_ASCENDING)
		return cpufreq_table_find_index_ah(policy, target_freq);
	else
		return cpufreq_table_find_index_dh(policy, target_freq);
}

/* Find closest freq to target in a table in ascending order */
static inline int cpufreq_table_find_index_ac(struct cpufreq_policy *policy,
					      unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = policy->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq == target_freq)
			return idx;

		if (freq < target_freq) {
			best = idx;
			continue;
		}

		/* No freq found below target_freq */
		if (best == -1)
			return idx;

		/* Choose the closest freq */
		if (target_freq - table[best].frequency > freq - target_freq)
			return idx;

		return best;
	}

	return best;
}

/* Find closest freq to target in a table in descending order */
static inline int cpufreq_table_find_index_dc(struct cpufreq_policy *policy,
					      unsigned int target_freq)
{
	struct cpufreq_frequency_table *table = policy->freq_table;
	struct cpufreq_frequency_table *pos;
	unsigned int freq;
	int idx, best = -1;

	cpufreq_for_each_valid_entry_idx(pos, table, idx) {
		freq = pos->frequency;

		if (freq == target_freq)
			return idx;

		if (freq > target_freq) {
			best = idx;
			continue;
		}

		/* No freq found above target_freq */
		if (best == -1)
			return idx;

		/* Choose the closest freq */
		if (table[best].frequency - target_freq > target_freq - freq)
			return idx;

		return best;
	}

	return best;
}

/* Works only on sorted freq-tables */
static inline int cpufreq_table_find_index_c(struct cpufreq_policy *policy,
					     unsigned int target_freq)
{
	target_freq = clamp_val(target_freq, policy->min, policy->max);

	if (policy->freq_table_sorted == CPUFREQ_TABLE_SORTED_ASCENDING)
		return cpufreq_table_find_index_ac(policy, target_freq);
	else
		return cpufreq_table_find_index_dc(policy, target_freq);
}

static inline int cpufreq_frequency_table_target(struct cpufreq_policy *policy,
						 unsigned int target_freq,
						 unsigned int relation)
{
	if (unlikely(policy->freq_table_sorted == CPUFREQ_TABLE_UNSORTED))
		return cpufreq_table_index_unsorted(policy, target_freq,
						    relation);

	switch (relation) {
	case CPUFREQ_RELATION_L:
		return cpufreq_table_find_index_l(policy, target_freq);
	case CPUFREQ_RELATION_H:
		return cpufreq_table_find_index_h(policy, target_freq);
	case CPUFREQ_RELATION_C:
		return cpufreq_table_find_index_c(policy, target_freq);
	default:
		pr_err("%s: Invalid relation: %d\n", __func__, relation);
		return -EINVAL;
	}
}

static inline int cpufreq_table_count_valid_entries(const struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pos;
	int count = 0;

	if (unlikely(!policy->freq_table))
		return 0;

	cpufreq_for_each_valid_entry(pos, policy->freq_table)
		count++;

	return count;
}
#else
static inline int cpufreq_boost_trigger_state(int state)
{
	return 0;
}
static inline int cpufreq_boost_enabled(void)
{
	return 0;
}

static inline int cpufreq_enable_boost_support(void)
{
	return -EINVAL;
}

static inline bool policy_has_boost_freq(struct cpufreq_policy *policy)
{
	return false;
}
#endif

extern void arch_freq_prepare_all(void);
extern unsigned int arch_freq_get_on_cpu(int cpu);

extern void arch_set_freq_scale(struct cpumask *cpus, unsigned long cur_freq,
				unsigned long max_freq);

/* the following are really really optional */
extern struct freq_attr cpufreq_freq_attr_scaling_available_freqs;
extern struct freq_attr cpufreq_freq_attr_scaling_boost_freqs;
extern struct freq_attr *cpufreq_generic_attr[];
int cpufreq_table_validate_and_sort(struct cpufreq_policy *policy);

unsigned int cpufreq_generic_get(unsigned int cpu);
int cpufreq_generic_init(struct cpufreq_policy *policy,
		struct cpufreq_frequency_table *table,
		unsigned int transition_latency);
#endif /* _LINUX_CPUFREQ_H */
