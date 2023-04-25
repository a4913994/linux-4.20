/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

/*
 * Define 'struct task_struct' and provide the main scheduler
 * APIs (schedule(), wakeup variants, etc.)
 */

#include <uapi/linux/sched.h>

#include <asm/current.h>

#include <linux/pid.h>
#include <linux/sem.h>
#include <linux/shm.h>
#include <linux/kcov.h>
#include <linux/mutex.h>
#include <linux/plist.h>
#include <linux/hrtimer.h>
#include <linux/seccomp.h>
#include <linux/nodemask.h>
#include <linux/rcupdate.h>
#include <linux/resource.h>
#include <linux/latencytop.h>
#include <linux/sched/prio.h>
#include <linux/signal_types.h>
#include <linux/psi_types.h>
#include <linux/mm_types_task.h>
#include <linux/task_io_accounting.h>
#include <linux/rseq.h>

/* task_struct member predeclarations (sorted alphabetically): */
struct audit_context;
struct backing_dev_info;
struct bio_list;
struct blk_plug;
struct cfs_rq;
struct fs_struct;
struct futex_pi_state;
struct io_context;
struct mempolicy;
struct nameidata;
struct nsproxy;
struct perf_event_context;
struct pid_namespace;
struct pipe_inode_info;
struct rcu_node;
struct reclaim_state;
struct robust_list_head;
struct sched_attr;
struct sched_param;
struct seq_file;
struct sighand_struct;
struct signal_struct;
struct task_delay_info;
struct task_group;

/*
 * Task state bitmask. NOTE! These bits are also
 * encoded in fs/proc/array.c: get_task_state().
 *
 * We have two separate sets of flags: task->state
 * is about runnability, while task->exit_state are
 * about the task exiting. Confusing, but this way
 * modifying one set can't modify the other one by
 * mistake.
 */

/* Used in tsk->state: */
#define TASK_RUNNING			0x0000
#define TASK_INTERRUPTIBLE		0x0001
#define TASK_UNINTERRUPTIBLE		0x0002
#define __TASK_STOPPED			0x0004
#define __TASK_TRACED			0x0008
/* Used in tsk->exit_state: */
#define EXIT_DEAD			0x0010
#define EXIT_ZOMBIE			0x0020
#define EXIT_TRACE			(EXIT_ZOMBIE | EXIT_DEAD)
/* Used in tsk->state again: */
#define TASK_PARKED			0x0040
#define TASK_DEAD			0x0080
#define TASK_WAKEKILL			0x0100
#define TASK_WAKING			0x0200
#define TASK_NOLOAD			0x0400
#define TASK_NEW			0x0800
#define TASK_STATE_MAX			0x1000

/* Convenience macros for the sake of set_current_state: */
#define TASK_KILLABLE			(TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)
#define TASK_STOPPED			(TASK_WAKEKILL | __TASK_STOPPED)
#define TASK_TRACED			(TASK_WAKEKILL | __TASK_TRACED)

#define TASK_IDLE			(TASK_UNINTERRUPTIBLE | TASK_NOLOAD)

/* Convenience macros for the sake of wake_up(): */
#define TASK_NORMAL			(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)

/* get_task_state(): */
#define TASK_REPORT			(TASK_RUNNING | TASK_INTERRUPTIBLE | \
					 TASK_UNINTERRUPTIBLE | __TASK_STOPPED | \
					 __TASK_TRACED | EXIT_DEAD | EXIT_ZOMBIE | \
					 TASK_PARKED)

#define task_is_traced(task)		((task->state & __TASK_TRACED) != 0)

#define task_is_stopped(task)		((task->state & __TASK_STOPPED) != 0)

#define task_is_stopped_or_traced(task)	((task->state & (__TASK_STOPPED | __TASK_TRACED)) != 0)

#define task_contributes_to_load(task)	((task->state & TASK_UNINTERRUPTIBLE) != 0 && \
					 (task->flags & PF_FROZEN) == 0 && \
					 (task->state & TASK_NOLOAD) == 0)

#ifdef CONFIG_DEBUG_ATOMIC_SLEEP

/*
 * Special states are those that do not use the normal wait-loop pattern. See
 * the comment with set_special_state().
 */
#define is_special_task_state(state)				\
	((state) & (__TASK_STOPPED | __TASK_TRACED | TASK_PARKED | TASK_DEAD))

#define __set_current_state(state_value)			\
	do {							\
		WARN_ON_ONCE(is_special_task_state(state_value));\
		current->task_state_change = _THIS_IP_;		\
		current->state = (state_value);			\
	} while (0)

#define set_current_state(state_value)				\
	do {							\
		WARN_ON_ONCE(is_special_task_state(state_value));\
		current->task_state_change = _THIS_IP_;		\
		smp_store_mb(current->state, (state_value));	\
	} while (0)

#define set_special_state(state_value)					\
	do {								\
		unsigned long flags; /* may shadow */			\
		WARN_ON_ONCE(!is_special_task_state(state_value));	\
		raw_spin_lock_irqsave(&current->pi_lock, flags);	\
		current->task_state_change = _THIS_IP_;			\
		current->state = (state_value);				\
		raw_spin_unlock_irqrestore(&current->pi_lock, flags);	\
	} while (0)
#else
/*
 * set_current_state() includes a barrier so that the write of current->state
 * is correctly serialised wrt the caller's subsequent test of whether to
 * actually sleep:
 *
 *   for (;;) {
 *	set_current_state(TASK_UNINTERRUPTIBLE);
 *	if (!need_sleep)
 *		break;
 *
 *	schedule();
 *   }
 *   __set_current_state(TASK_RUNNING);
 *
 * If the caller does not need such serialisation (because, for instance, the
 * condition test and condition change and wakeup are under the same lock) then
 * use __set_current_state().
 *
 * The above is typically ordered against the wakeup, which does:
 *
 *   need_sleep = false;
 *   wake_up_state(p, TASK_UNINTERRUPTIBLE);
 *
 * where wake_up_state() executes a full memory barrier before accessing the
 * task state.
 *
 * Wakeup will do: if (@state & p->state) p->state = TASK_RUNNING, that is,
 * once it observes the TASK_UNINTERRUPTIBLE store the waking CPU can issue a
 * TASK_RUNNING store which can collide with __set_current_state(TASK_RUNNING).
 *
 * However, with slightly different timing the wakeup TASK_RUNNING store can
 * also collide with the TASK_UNINTERRUPTIBLE store. Loosing that store is not
 * a problem either because that will result in one extra go around the loop
 * and our @cond test will save the day.
 *
 * Also see the comments of try_to_wake_up().
 */
#define __set_current_state(state_value)				\
	current->state = (state_value)

#define set_current_state(state_value)					\
	smp_store_mb(current->state, (state_value))

/*
 * set_special_state() should be used for those states when the blocking task
 * can not use the regular condition based wait-loop. In that case we must
 * serialize against wakeups such that any possible in-flight TASK_RUNNING stores
 * will not collide with our state change.
 */
#define set_special_state(state_value)					\
	do {								\
		unsigned long flags; /* may shadow */			\
		raw_spin_lock_irqsave(&current->pi_lock, flags);	\
		current->state = (state_value);				\
		raw_spin_unlock_irqrestore(&current->pi_lock, flags);	\
	} while (0)

#endif

/* Task command name length: */
#define TASK_COMM_LEN			16

extern void scheduler_tick(void);

#define	MAX_SCHEDULE_TIMEOUT		LONG_MAX

extern long schedule_timeout(long timeout);
extern long schedule_timeout_interruptible(long timeout);
extern long schedule_timeout_killable(long timeout);
extern long schedule_timeout_uninterruptible(long timeout);
extern long schedule_timeout_idle(long timeout);
asmlinkage void schedule(void);
extern void schedule_preempt_disabled(void);

extern int __must_check io_schedule_prepare(void);
extern void io_schedule_finish(int token);
extern long io_schedule_timeout(long timeout);
extern void io_schedule(void);

/**
 * struct prev_cputime - snapshot of system and user cputime
 * struct prev_cputime - 系统和用户 CPU 时间的快照
 * @utime: time spent in user mode 在用户模式下花费的时间
 * @stime: time spent in system mode 在系统模式下花费的时间
 * @lock: protects the above two fields 保护上面两个字段
 *
 * Stores previous user/system time values such that we can guarantee
 * monotonicity. 
 * 存储先前的用户/系统时间值，以便我们可以保证单调性。
 */
struct prev_cputime {
#ifndef CONFIG_VIRT_CPU_ACCOUNTING_NATIVE
	u64				utime;
	u64				stime;
	raw_spinlock_t			lock;
#endif
};

/**
 * struct task_cputime - collected CPU time counts
 * struct task_cputime - 已收集的 CPU 时间计数
 * @utime:		time spent in user mode, in nanoseconds 用户模式下花费的时间，以纳秒为单位
 * @stime:		time spent in kernel mode, in nanoseconds 内核模式下花费的时间，以纳秒为单位
 * @sum_exec_runtime:	total time spent on the CPU, in nanoseconds 在 CPU 上总共花费的时间，以纳秒为单位
 *
 * This structure groups together three kinds of CPU time that are tracked for
 * threads and thread groups.  Most things considering CPU time want to group
 * these counts together and treat all three of them in parallel.
 * 这个结构将线程和线程组跟踪的三种 CPU 时间分组在一起。考虑到 CPU 时间的大多数情况都想将这些计数归为一组，并将它们并行处理
 */
struct task_cputime {
	u64				utime;
	u64				stime;
	unsigned long long		sum_exec_runtime;
};

/* Alternate field names when used on cache expirations: */
#define virt_exp			utime
#define prof_exp			stime
#define sched_exp			sum_exec_runtime

enum vtime_state {
	/* Task is sleeping or running in a CPU with VTIME inactive: */
	VTIME_INACTIVE = 0,
	/* Task runs in userspace in a CPU with VTIME active: */
	VTIME_USER,
	/* Task runs in kernelspace in a CPU with VTIME active: */
	VTIME_SYS,
};

struct vtime {
	seqcount_t		seqcount;
	unsigned long long	starttime;
	enum vtime_state	state;
	u64			utime;
	u64			stime;
	u64			gtime;
};

struct sched_info {
#ifdef CONFIG_SCHED_INFO
	/* Cumulative counters: */

	/* # of times we have run on this CPU: */
	unsigned long			pcount;

	/* Time spent waiting on a runqueue: */
	unsigned long long		run_delay;

	/* Timestamps: */

	/* When did we last run on a CPU? */
	unsigned long long		last_arrival;

	/* When were we last queued to run? */
	unsigned long long		last_queued;

#endif /* CONFIG_SCHED_INFO */
};

/*
 * Integer metrics need fixed point arithmetic, e.g., sched/fair
 * has a few: load, load_avg, util_avg, freq, and capacity.
 *
 * We define a basic fixed point arithmetic range, and then formalize
 * all these metrics based on that basic range.
 */
# define SCHED_FIXEDPOINT_SHIFT		10
# define SCHED_FIXEDPOINT_SCALE		(1L << SCHED_FIXEDPOINT_SHIFT)

struct load_weight {
	unsigned long			weight;
	u32				inv_weight;
};

/**
 * struct util_est - Estimation utilization of FAIR tasks
 * @enqueued: instantaneous estimated utilization of a task/cpu
 * @ewma:     the Exponential Weighted Moving Average (EWMA)
 *            utilization of a task
 *
 * Support data structure to track an Exponential Weighted Moving Average
 * (EWMA) of a FAIR task's utilization. New samples are added to the moving
 * average each time a task completes an activation. Sample's weight is chosen
 * so that the EWMA will be relatively insensitive to transient changes to the
 * task's workload.
 *
 * The enqueued attribute has a slightly different meaning for tasks and cpus:
 * - task:   the task's util_avg at last task dequeue time
 * - cfs_rq: the sum of util_est.enqueued for each RUNNABLE task on that CPU
 * Thus, the util_est.enqueued of a task represents the contribution on the
 * estimated utilization of the CPU where that task is currently enqueued.
 *
 * Only for tasks we track a moving average of the past instantaneous
 * estimated utilization. This allows to absorb sporadic drops in utilization
 * of an otherwise almost periodic task.
 */
struct util_est {
	unsigned int			enqueued;
	unsigned int			ewma;
#define UTIL_EST_WEIGHT_SHIFT		2
} __attribute__((__aligned__(sizeof(u64))));

/*
 * The load_avg/util_avg accumulates an infinite geometric series
 * (see __update_load_avg() in kernel/sched/fair.c).
 *
 * [load_avg definition]
 *
 *   load_avg = runnable% * scale_load_down(load)
 *
 * where runnable% is the time ratio that a sched_entity is runnable.
 * For cfs_rq, it is the aggregated load_avg of all runnable and
 * blocked sched_entities.
 *
 * load_avg may also take frequency scaling into account:
 *
 *   load_avg = runnable% * scale_load_down(load) * freq%
 *
 * where freq% is the CPU frequency normalized to the highest frequency.
 *
 * [util_avg definition]
 *
 *   util_avg = running% * SCHED_CAPACITY_SCALE
 *
 * where running% is the time ratio that a sched_entity is running on
 * a CPU. For cfs_rq, it is the aggregated util_avg of all runnable
 * and blocked sched_entities.
 *
 * util_avg may also factor frequency scaling and CPU capacity scaling:
 *
 *   util_avg = running% * SCHED_CAPACITY_SCALE * freq% * capacity%
 *
 * where freq% is the same as above, and capacity% is the CPU capacity
 * normalized to the greatest capacity (due to uarch differences, etc).
 *
 * N.B., the above ratios (runnable%, running%, freq%, and capacity%)
 * themselves are in the range of [0, 1]. To do fixed point arithmetics,
 * we therefore scale them to as large a range as necessary. This is for
 * example reflected by util_avg's SCHED_CAPACITY_SCALE.
 *
 * [Overflow issue]
 *
 * The 64-bit load_sum can have 4353082796 (=2^64/47742/88761) entities
 * with the highest load (=88761), always runnable on a single cfs_rq,
 * and should not overflow as the number already hits PID_MAX_LIMIT.
 *
 * For all other cases (including 32-bit kernels), struct load_weight's
 * weight will overflow first before we do, because:
 *
 *    Max(load_avg) <= Max(load.weight)
 *
 * Then it is the load_weight's responsibility to consider overflow
 * issues.
 */
struct sched_avg {
	u64				last_update_time;
	u64				load_sum;
	u64				runnable_load_sum;
	u32				util_sum;
	u32				period_contrib;
	unsigned long			load_avg;
	unsigned long			runnable_load_avg;
	unsigned long			util_avg;
	struct util_est			util_est;
} ____cacheline_aligned;

struct sched_statistics {
#ifdef CONFIG_SCHEDSTATS
	u64				wait_start;
	u64				wait_max;
	u64				wait_count;
	u64				wait_sum;
	u64				iowait_count;
	u64				iowait_sum;

	u64				sleep_start;
	u64				sleep_max;
	s64				sum_sleep_runtime;

	u64				block_start;
	u64				block_max;
	u64				exec_max;
	u64				slice_max;

	u64				nr_migrations_cold;
	u64				nr_failed_migrations_affine;
	u64				nr_failed_migrations_running;
	u64				nr_failed_migrations_hot;
	u64				nr_forced_migrations;

	u64				nr_wakeups;
	u64				nr_wakeups_sync;
	u64				nr_wakeups_migrate;
	u64				nr_wakeups_local;
	u64				nr_wakeups_remote;
	u64				nr_wakeups_affine;
	u64				nr_wakeups_affine_attempts;
	u64				nr_wakeups_passive;
	u64				nr_wakeups_idle;
#endif
};

struct sched_entity {
	/* For load-balancing: */
	struct load_weight		load;
	unsigned long			runnable_weight;
	struct rb_node			run_node;
	struct list_head		group_node;
	unsigned int			on_rq;

	u64				exec_start;
	u64				sum_exec_runtime;
	u64				vruntime;
	u64				prev_sum_exec_runtime;

	u64				nr_migrations;

	struct sched_statistics		statistics;

#ifdef CONFIG_FAIR_GROUP_SCHED
	int				depth;
	struct sched_entity		*parent;
	/* rq on which this entity is (to be) queued: */
	struct cfs_rq			*cfs_rq;
	/* rq "owned" by this entity/group: */
	struct cfs_rq			*my_q;
#endif

#ifdef CONFIG_SMP
	/*
	 * Per entity load average tracking.
	 *
	 * Put into separate cache line so it does not
	 * collide with read-mostly values above.
	 */
	struct sched_avg		avg;
#endif
};

struct sched_rt_entity {
	struct list_head		run_list;
	unsigned long			timeout;
	unsigned long			watchdog_stamp;
	unsigned int			time_slice;
	unsigned short			on_rq;
	unsigned short			on_list;

	struct sched_rt_entity		*back;
#ifdef CONFIG_RT_GROUP_SCHED
	struct sched_rt_entity		*parent;
	/* rq on which this entity is (to be) queued: */
	struct rt_rq			*rt_rq;
	/* rq "owned" by this entity/group: */
	struct rt_rq			*my_q;
#endif
} __randomize_layout;

struct sched_dl_entity {
	struct rb_node			rb_node;

	/*
	 * Original scheduling parameters. Copied here from sched_attr
	 * during sched_setattr(), they will remain the same until
	 * the next sched_setattr().
	 */
	u64				dl_runtime;	/* Maximum runtime for each instance	*/
	u64				dl_deadline;	/* Relative deadline of each instance	*/
	u64				dl_period;	/* Separation of two instances (period) */
	u64				dl_bw;		/* dl_runtime / dl_period		*/
	u64				dl_density;	/* dl_runtime / dl_deadline		*/

	/*
	 * Actual scheduling parameters. Initialized with the values above,
	 * they are continously updated during task execution. Note that
	 * the remaining runtime could be < 0 in case we are in overrun.
	 */
	s64				runtime;	/* Remaining runtime for this instance	*/
	u64				deadline;	/* Absolute deadline for this instance	*/
	unsigned int			flags;		/* Specifying the scheduler behaviour	*/

	/*
	 * Some bool flags:
	 *
	 * @dl_throttled tells if we exhausted the runtime. If so, the
	 * task has to wait for a replenishment to be performed at the
	 * next firing of dl_timer.
	 *
	 * @dl_boosted tells if we are boosted due to DI. If so we are
	 * outside bandwidth enforcement mechanism (but only until we
	 * exit the critical section);
	 *
	 * @dl_yielded tells if task gave up the CPU before consuming
	 * all its available runtime during the last job.
	 *
	 * @dl_non_contending tells if the task is inactive while still
	 * contributing to the active utilization. In other words, it
	 * indicates if the inactive timer has been armed and its handler
	 * has not been executed yet. This flag is useful to avoid race
	 * conditions between the inactive timer handler and the wakeup
	 * code.
	 *
	 * @dl_overrun tells if the task asked to be informed about runtime
	 * overruns.
	 */
	unsigned int			dl_throttled      : 1;
	unsigned int			dl_boosted        : 1;
	unsigned int			dl_yielded        : 1;
	unsigned int			dl_non_contending : 1;
	unsigned int			dl_overrun	  : 1;

	/*
	 * Bandwidth enforcement timer. Each -deadline task has its
	 * own bandwidth to be enforced, thus we need one timer per task.
	 */
	struct hrtimer			dl_timer;

	/*
	 * Inactive timer, responsible for decreasing the active utilization
	 * at the "0-lag time". When a -deadline task blocks, it contributes
	 * to GRUB's active utilization until the "0-lag time", hence a
	 * timer is needed to decrease the active utilization at the correct
	 * time.
	 */
	struct hrtimer inactive_timer;
};

union rcu_special {
	struct {
		u8			blocked;
		u8			need_qs;
	} b; /* Bits. */
	u16 s; /* Set of bits. */
};

enum perf_event_task_context {
	perf_invalid_context = -1,
	perf_hw_context = 0,
	perf_sw_context,
	perf_nr_task_contexts,
};

struct wake_q_node {
	struct wake_q_node *next;
};

// task_struct 是Linux内核中用于表示一个任务（进程或线程）的关键数据结构。
// 它包含了一个任务的状态、属性、资源等信息
struct task_struct {
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/*
	 * For reasons of header soup (see current_thread_info()), this
	 * must be the first element of task_struct.
	 */
	struct thread_info		thread_info;
#endif
	/* -1 unrunnable, 0 runnable, >0 stopped: */
	// state：任务的当前状态，如可运行、不可运行或停止
	volatile long			state;

	/*
	 * This begins the randomizable portion of task_struct. Only
	 * scheduling-critical items should be added above here.
	 */
	randomized_struct_fields_start
	// stack：任务的内核栈指针。
	void				*stack;
	// usage：原子变量，表示对任务结构的引用计数。
	atomic_t			usage;
	/* Per task flags (PF_*), defined further below: */
	// flags：任务的标志，如PF_EXITING（表示任务正在退出）和PF_FORKNOEXEC（表示子进程在fork时不继承父进程的内存映射）。
	unsigned int			flags;
	// ptrace：表示是否有进程正在跟踪此任务，以及跟踪的类型。
	unsigned int			ptrace;

#ifdef CONFIG_SMP
	// wake_entry：用于跨CPU唤醒任务的链表节点。
	struct llist_node		wake_entry;
	// on_cpu：表示任务是否正在某个CPU上运行。
	int				on_cpu;
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/* Current CPU: */
	// cpu：表示任务当前正在运行的CPU编号（仅在CONFIG_THREAD_INFO_IN_TASK配置选项启用时）。
	unsigned int			cpu;
#endif
	// wakee_flips：表示任务切换的次数。
	unsigned int			wakee_flips;
	// wakee_flip_decay_ts：用于衡量任务的唤醒时间。
	unsigned long			wakee_flip_decay_ts;
	// last_wakee：指向上一个被唤醒的任务的指针。
	struct task_struct		*last_wakee;

	/*
	 * recent_used_cpu is initially set as the last CPU used by a task
	 * that wakes affine another task. Waker/wakee relationships can
	 * push tasks around a CPU where each wakeup moves to the next one.
	 * Tracking a recently used CPU allows a quick search for a recently
	 * used CPU that may be idle.
	 */
	// recent_used_cpu：最近使用过的 CPU，初始值为唤醒另一个任务的任务所使用的最后一个 CPU。
	// 通过跟踪最近使用过的 CPU，可以快速搜索可能空闲的最近使用过的 CPU。
	int				recent_used_cpu;
	// wake_cpu：唤醒任务的 CPU。
	int				wake_cpu;
#endif
	// on_rq：表示任务是否在运行队列上。值为 1 表示任务在运行队列上，值为 0 表示任务不在运行队列上。
	int				on_rq;

	// prio：任务的动态优先级，可能会随着时间和任务行为而变化。
	int				prio;
	// static_prio：任务的静态优先级，表示任务的固定优先级。在任务创建时分配，不会随着任务行为而改变。
	int				static_prio;
	// normal_prio：任务的正常优先级，表示除实时优先级之外的优先级。
	int				normal_prio;
	// rt_priority：任务的实时优先级，表示任务的实时优先级。
	unsigned int			rt_priority;

	// sched_class：指向任务所属调度类的指针，例如实时调度类（rt_sched_class）和完全公平调度类（fair_sched_class）等。
	const struct sched_class	*sched_class;
	// se：调度实体（sched_entity）结构，包含了任务在调度器中的一些基本信息，如运行时间、虚拟运行时间等。
	struct sched_entity		se;
	// rt：实时调度实体（sched_rt_entity）结构，包含了任务在实时调度策略下的信息，如超时时间和实时任务列表等。
	struct sched_rt_entity		rt;
#ifdef CONFIG_CGROUP_SCHED
	// sched_task_group：仅在配置了 CONFIG_CGROUP_SCHED 时可用。表示任务所属的任务组，用于控制组（cgroup）调度。
	struct task_group		*sched_task_group;
#endif
	// dl：截止调度实体（sched_dl_entity）结构，包含了任务在截止调度策略下的信息，如截止时间和运行时间等。
	struct sched_dl_entity		dl;

#ifdef CONFIG_PREEMPT_NOTIFIERS
	/* List of struct preempt_notifier: */
	// preempt_notifiers：仅在配置了 CONFIG_PREEMPT_NOTIFIERS 时可用。包含一个 preempt_notifier 结构的列表，用于在任务抢占发生时通知其他内核组件。
	struct hlist_head		preempt_notifiers;
#endif

#ifdef CONFIG_BLK_DEV_IO_TRACE
	// btrace_seq：仅在配置了 CONFIG_BLK_DEV_IO_TRACE 时可用。表示任务在块设备 I/O 跟踪中的序列号。
	unsigned int			btrace_seq;
#endif
	// policy：任务的调度策略，如 SCHED_FIFO、SCHED_RR 和 SCHED_NORMAL 等。
	unsigned int			policy;
	// nr_cpus_allowed：允许任务运行的 CPU 数量。
	int				nr_cpus_allowed;
	// cpus_allowed：任务允许运行的 CPU 集合，用于表示任务可以在哪些 CPU 上执行。
	cpumask_t			cpus_allowed;

#ifdef CONFIG_PREEMPT_RCU
	// rcu_read_lock_nesting：表示 RCU 读锁的嵌套层数。当为负数时表示没有持有读锁。
	int				rcu_read_lock_nesting;
	// rcu_read_unlock_special：表示 RCU 读锁解锁时需要处理的特殊情况。
	union rcu_special		rcu_read_unlock_special;
	// rcu_node_entry：表示 RCU 节点列表中的一个条目。
	struct list_head		rcu_node_entry;
	// rcu_blocked_node：指向一个 RCU 节点，表示任务在此 RCU 节点上被阻塞。
	struct rcu_node			*rcu_blocked_node;
#endif /* #ifdef CONFIG_PREEMPT_RCU */

#ifdef CONFIG_TASKS_RCU
	// rcu_tasks_nvcsw：表示自从上一次 RCU 任务扫描以来的非自愿上下文切换次数。
	unsigned long			rcu_tasks_nvcsw;
	// rcu_tasks_holdout：表示任务是否被认为是 RCU 任务 holdout，即未响应 RCU 扫描的任务。
	u8				rcu_tasks_holdout;
	// rcu_tasks_idx：表示任务在 RCU 任务数组中的索引。
	u8				rcu_tasks_idx;
	// rcu_tasks_idle_cpu：表示任务在哪个 CPU 上空闲。
	int				rcu_tasks_idle_cpu;
	// rcu_tasks_holdout_list：表示 RCU 任务 holdout 列表。
	struct list_head		rcu_tasks_holdout_list;
#endif /* #ifdef CONFIG_TASKS_RCU */
	// sched_info：调度信息结构，包含了任务在调度器中的一些统计信息。
	struct sched_info		sched_info;
	// tasks：用于链接同一进程中的所有线程，以便将它们组合在一起。
	struct list_head		tasks;
#ifdef CONFIG_SMP
	// pushable_tasks：表示可推送任务列表中的一个条目，此列表用于在多处理器系统中均衡负载。
	struct plist_node		pushable_tasks;
	// pushable_dl_tasks：表示可推送截止任务列表中的一个条目，此列表用于在多处理器系统中均衡截止调度策略下的负载。
	struct rb_node			pushable_dl_tasks;
#endif
	// mm：指向进程的内存描述符，包含虚拟内存区域、内存权限等信息。
	struct mm_struct		*mm;
	// active_mm：指向当前活动的内存描述符。对于运行中的进程，它与 mm 字段相同。
	// 对于内核线程，它可能指向一个用户进程的内存描述符。
	struct mm_struct		*active_mm;

	/* Per-thread vma caching: */
	// vmacache：一个虚拟内存区域（VMA）缓存，用于加速对虚拟内存区域的查找。
	struct vmacache			vmacache;

#ifdef SPLIT_RSS_COUNTING
	// rss_stat：用于记录进程的各种内存使用统计信息，如分页内存、锁定内存等。
	struct task_rss_stat		rss_stat;
#endif
	// exit_state：表示进程的退出状态，可能的值包括：退出、死亡、僵尸等。
	int				exit_state;
	// exit_code：表示进程的退出代码。
	int				exit_code;
	// exit_signal：表示进程的退出信号。
	int				exit_signal;
	/* The signal sent when the parent dies: */
	// pdeath_signal：表示当父进程死亡时发送给子进程的信号。
	int				pdeath_signal;
	/* JOBCTL_*, siglock protected: */
	// jobctl：与作业控制相关的标志，受信号锁保护。
	unsigned long			jobctl;

	/* Used for emulating ABI behavior of previous Linux versions: */
	// personality：表示进程的"性格"，用于模拟不同 Linux 版本的 ABI 行为。
	unsigned int			personality;

	/* Scheduler bits, serialized by scheduler locks: */
	// sched_reset_on_fork：调度器位，表示在执行 fork() 时是否重置调度器状态。
	unsigned			sched_reset_on_fork:1;
	// sched_contributes_to_load：调度器位，表示该任务是否对调度器的负载贡献。
	unsigned			sched_contributes_to_load:1;
	// sched_migrated：调度器位，表示任务是否已经迁移到另一个 CPU。
	unsigned			sched_migrated:1;
	// sched_remote_wakeup：调度器位，表示任务是否由远程 CPU 唤醒。
	unsigned			sched_remote_wakeup:1;
#ifdef CONFIG_PSI
	// sched_psi_wake_requeue：调度器位，表示该任务在 PSI（系统压力指标）监控下是否需要重新排队。
	unsigned			sched_psi_wake_requeue:1;
#endif

	/* Force alignment to the next boundary: */
	unsigned			:0;

	/* Unserialized, strictly 'current' */

	/* Bit to tell LSMs we're in execve(): */
	// in_execve：一个位标志，表示任务是否正在执行 execve() 系统调用，这对 Linux 安全模块（LSM）有意义。
	unsigned			in_execve:1;
	// in_iowait：一个位标志，表示任务是否正在等待 I/O 操作完成。
	unsigned			in_iowait:1;
#ifndef TIF_RESTORE_SIGMASK
	// restore_sigmask：表示任务是否需要在信号处理完成后恢复信号屏蔽字。
	unsigned			restore_sigmask:1;
#endif
#ifdef CONFIG_MEMCG
	// in_user_fault：表示任务是否处于用户故障处理过程中。
	unsigned			in_user_fault:1;
#endif
#ifdef CONFIG_COMPAT_BRK
	// brk_randomized：表示 brk 随机化是否已启用。
	unsigned			brk_randomized:1;
#endif
#ifdef CONFIG_CGROUPS
	/* disallow userland-initiated cgroup migration */
	// no_cgroup_migration：表示禁止用户空间发起的 cgroup 迁移。
	unsigned			no_cgroup_migration:1;
#endif
#ifdef CONFIG_BLK_CGROUP
	/* to be used once the psi infrastructure lands upstream. */
	// use_memdelay：表示一旦 PSI（系统压力指标）基础结构上游被接受，将使用内存延迟。
	unsigned			use_memdelay:1;
#endif

	/*
	 * May usercopy functions fault on kernel addresses?
	 * This is not just a single bit because this can potentially nest.
	 */
	// kernel_uaccess_faults_ok：表示用户空间复制功能是否允许在内核地址上发生故障。这不仅仅是一个位，因为这可能会嵌套。
	unsigned int			kernel_uaccess_faults_ok;
	// atomic_flags：需要原子访问的标志位。
	unsigned long			atomic_flags; /* Flags requiring atomic access. */
	// restart_block：用于在系统调用重新启动时存储有关如何重新启动的信息。
	struct restart_block		restart_block;
	// pid：进程的进程 ID。
	pid_t				pid;
	// tgid：线程组 ID，对于单线程进程，其值与 pid 相同。
	pid_t				tgid;

#ifdef CONFIG_STACKPROTECTOR
	/* Canary value for the -fstack-protector GCC feature: */
	// stack_canary：用于 GCC 的 -fstack-protector 功能的栈保护值。
	unsigned long			stack_canary;
#endif
	/*
	 * Pointers to the (original) parent process, youngest child, younger sibling,
	 * older sibling, respectively.  (p->father can be replaced with
	 * p->real_parent->pid)
	 */

	/* Real parent process: */
	// real_parent：指向任务的真实父进程的指针。
	struct task_struct __rcu	*real_parent;

	/* Recipient of SIGCHLD, wait4() reports: */
	// parent：指向任务的父进程的指针，用于接收 SIGCHLD 信号和报告 wait4()。
	struct task_struct __rcu	*parent;

	/*
	 * Children/sibling form the list of natural children:
	 */
	// children：一个链表，包含任务的所有子任务。
	struct list_head		children;
	// sibling：一个链表，包含任务的所有兄弟任务。
	struct list_head		sibling;
	// group_leader：指向任务的线程组（进程）领导者的指针。
	struct task_struct		*group_leader;

	/*
	 * 'ptraced' is the list of tasks this task is using ptrace() on.
	 *
	 * This includes both natural children and PTRACE_ATTACH targets.
	 * 'ptrace_entry' is this task's link on the p->parent->ptraced list.
	 */
	// ptraced：这个任务正在使用 ptrace() 追踪的任务列表。这既包括自然子任务，也包括 PTRACE_ATTACH 目标
	struct list_head		ptraced;
	// ptrace_entry 是这个任务在 p->parent->ptraced 列表上的链接。
	struct list_head		ptrace_entry;

	/* PID/PID hash table linkage. */
	// thread_pid：指向任务的 PID 结构的指针。
	struct pid			*thread_pid;
	// pid_links：一个散列列表节点数组，用于 PID/PID 类型哈希表链接。
	struct hlist_node		pid_links[PIDTYPE_MAX];
	// thread_group：一个链表，包含同一线程组中的所有任务（即，所有属于同一进程的线程）。
	struct list_head		thread_group;
	// thread_node：一个链表，包含同一线程组中的所有任务（即，所有属于同一进程的线程）。
	struct list_head		thread_node;
	// vfork_done：一个指向 completion 结构的指针，用于在 vfork 系统调用完成时唤醒父进程。
	struct completion		*vfork_done;

	/* CLONE_CHILD_SETTID: */
	// set_child_tid：一个指向用户空间整数的指针，用于在创建新线程时设置子线程的 TID。这个字段与 CLONE_CHILD_SETTID 标志一起使用。
	int __user			*set_child_tid;

	/* CLONE_CHILD_CLEARTID: */
	// clear_child_tid：一个指向用户空间整数的指针，用于在子线程退出时清除 TID。这个字段与 CLONE_CHILD_CLEARTID 标志一起使用。
	int __user			*clear_child_tid;
	// utime：任务在用户态运行的累计时间（以时钟滴答计数）。
	u64				utime;
	// stime：任务在内核态运行的累计时间（以时钟滴答计数）。
	u64				stime;
#ifdef CONFIG_ARCH_HAS_SCALED_CPUTIME
	// utimescaled 和 stimescaled：任务在用户态和内核态运行的缩放累计时间。
	u64				utimescaled;
	u64				stimescaled;
#endif
	// gtime：任务的子进程在用户态和内核态运行的累计时间。
	u64				gtime;
	// prev_cputime：任务在上一次统计周期内的用户态和内核态运行时间。
	struct prev_cputime		prev_cputime;
#ifdef CONFIG_VIRT_CPU_ACCOUNTING_GEN
	// vtime：虚拟运行时间信息，用于跟踪任务在虚拟环境中的 CPU 时间。
	struct vtime			vtime;
#endif

#ifdef CONFIG_NO_HZ_FULL
	// tick_dep_mask：一个原子变量，用于跟踪任务的时钟滴答依赖项。
	atomic_t			tick_dep_mask;
#endif
	/* Context switch counts: */
	// nvcsw：自愿上下文切换（voluntary context switch）的计数，表示任务主动让出 CPU 的次数。
	unsigned long			nvcsw;
	// nivcsw：非自愿上下文切换（involuntary context switch）的计数，表示任务被迫让出 CPU 的次数。
	unsigned long			nivcsw;
	
	/* Monotonic time in nsecs: */
	// start_time：任务的开始时间，以纳秒为单位的单调时间。
	u64				start_time;

	/* Boot based time in nsecs: */
	// real_start_time：任务的实际开始时间，以纳秒为单位的基于引导的时间。
	u64				real_start_time;

	/* MM fault and swap info: this can arguably be seen as either mm-specific or thread-specific: */
	// min_flt：任务发生的次要缺页错误次数。次要缺页错误是指可以通过从内存中分配一个新的物理页面来处理的缺页错误。
	unsigned long			min_flt;
	// maj_flt：任务发生的主要缺页错误次数。主要缺页错误是指需要从磁盘中换入数据以解决的缺页错误。
	unsigned long			maj_flt;

#ifdef CONFIG_POSIX_TIMERS
	// cputime_expires：（仅在 CONFIG_POSIX_TIMERS 启用时可用）任务的 CPU 时间到期信息，用于跟踪任务的 POSIX 定时器。
	struct task_cputime		cputime_expires;
	// cpu_timers：（仅在 CONFIG_POSIX_TIMERS 启用时可用）任务的 CPU 定时器列表。这是一个大小为 3 的列表数组，分别表示实时、虚拟和分摊 CPU 时间。
	struct list_head		cpu_timers[3];
#endif

	/* Process credentials: */

	/* Tracer's credentials at attach: */
	// ptracer_cred：在附加（attach）时跟踪器（即调试器）的凭证。此凭证用于确定调试器是否有权限跟踪和控制任务。
	const struct cred __rcu		*ptracer_cred;

	/* Objective and real subjective task credentials (COW): */
	// real_cred：任务的实际（目标）和真实（主观）凭证（COW，即写时复制）。这些凭证包括用户 ID、组 ID、辅助组列表等，用于确定任务的权限。
	const struct cred __rcu		*real_cred;

	/* Effective (overridable) subjective task credentials (COW): */
	// cred：任务的有效（可覆盖）主观凭证（COW，即写时复制）。这些凭证可以被安全模块（如 SELinux 或 AppArmor）临时覆盖，以实现特权控制。
	const struct cred __rcu		*cred;

	/*
	 * executable name, excluding path.
	 *
	 * - normally initialized setup_new_exec()
	 * - access it with [gs]et_task_comm()
	 * - lock it with task_lock()
	 */
	// comm：任务的名称（通常与可执行文件的名称相同），长度为 TASK_COMM_LEN。
	char				comm[TASK_COMM_LEN];

	// nameidata：文件路径查找时使用的名称解析数据结构，用于在内核中处理路径名解析。
	struct nameidata		*nameidata;

#ifdef CONFIG_SYSVIPC
	// sysvsem 和 sysvshm：（仅在 CONFIG_SYSVIPC 启用时可用）分别用于跟踪任务的 System V 信号量和共享内存信息。
	struct sysv_sem			sysvsem;
	struct sysv_shm			sysvshm;
#endif
#ifdef CONFIG_DETECT_HUNG_TASK
	// last_switch_count 和 last_switch_time：（仅在 CONFIG_DETECT_HUNG_TASK 启用时可用）分别记录任务最后一次上下文切换的次数和时间。
	// 这些信息用于检测停顿的任务。
	//todo 在这里做些任务
	unsigned long			last_switch_count;
	unsigned long			last_switch_time;
#endif
	/* Filesystem information: */
	// fs：任务的文件系统信息，包括根目录、当前工作目录等。
	struct fs_struct		*fs;

	/* Open file information: */
	// files：任务的打开文件信息，包括所有已打开文件的描述符等。
	struct files_struct		*files;

	/* Namespaces: */
	// nsproxy：任务的命名空间代理，用于管理任务的多个命名空间，如进程、网络、挂载等。
	struct nsproxy			*nsproxy;

	/* Signal handlers: */
	// signal：任务的信号状态信息，包括已发送和已接收信号、信号处理函数等。
	struct signal_struct		*signal;
	// sighand：任务的信号处理程序结构，包含指向信号处理函数的指针。
	struct sighand_struct		*sighand;
	// blocked：任务阻塞的信号集合。
	sigset_t			blocked;
	// real_blocked：任务实际阻塞的信号集合。
	sigset_t			real_blocked;
	/* Restored if set_restore_sigmask() was used: */
	// saved_sigmask：如果使用了 set_restore_sigmask()，在信号处理完毕后将恢复此信号掩码。
	sigset_t			saved_sigmask;
	// pending：任务的挂起信号。
	struct sigpending		pending;
	// sas_ss_sp、sas_ss_size 和 sas_ss_flags：分别表示任务的信号备选栈（signal alternate stack）的栈指针、大小和标志。信号备选栈用于在处理特定信号时，为信号处理程序提供一个独立的栈空间。
	unsigned long			sas_ss_sp;
	size_t				sas_ss_size;
	unsigned int			sas_ss_flags;
	// task_works：任务中待处理的回调链表头，用于安排任务上下文中的工作。
	struct callback_head		*task_works;
	// audit_context：任务的审计上下文，用于在内核中处理审计事件。
	struct audit_context		*audit_context;
#ifdef CONFIG_AUDITSYSCALL
	// loginuid 和 sessionid：分别表示任务的登录用户 ID 和会话 ID，用于审计跟踪。
	kuid_t				loginuid;
	unsigned int			sessionid;
#endif
	// seccomp：任务的安全计算模式，用于限制系统调用的范围。
	struct seccomp			seccomp;

	/* Thread group tracking: */
	// parent_exec_id 和 self_exec_id：分别表示父任务和当前任务的执行 ID，用于跟踪线程组。
	u32				parent_exec_id;
	u32				self_exec_id;

	/* Protection against (de-)allocation: mm, files, fs, tty, keyrings, mems_allowed, mempolicy: */
	// alloc_lock：用于保护任务中涉及到分配和释放的数据结构（如 mm、files、fs、tty、keyrings、mems_allowed 和 mempolicy）。
	spinlock_t			alloc_lock;

	/* Protection of the PI data structures: */
	// pi_lock：用于保护优先级继承（Priority Inheritance）相关的数据结构的自旋锁。
	raw_spinlock_t			pi_lock;

	// wake_q：任务唤醒队列节点，用于唤醒任务。
	struct wake_q_node		wake_q;

#ifdef CONFIG_RT_MUTEXES
	/* PI waiters blocked on a rt_mutex held by this task: */
	// pi_waiters：等待当前任务持有的实时互斥锁的优先级继承（Priority Inheritance）等待者。
	struct rb_root_cached		pi_waiters;
	/* Updated under owner's pi_lock and rq lock */
	// pi_top_task：在优先级继承场景下，具有最高优先级的任务。在任务的 pi_lock 和运行队列锁下更新。
	struct task_struct		*pi_top_task;
	/* Deadlock detection and priority inheritance handling: */
	// pi_blocked_on：表示任务在哪个实时互斥锁上阻塞，用于死锁检测和优先级继承处理。
	struct rt_mutex_waiter		*pi_blocked_on;
#endif

#ifdef CONFIG_DEBUG_MUTEXES
	/* Mutex deadlock detection: */
	// blocked_on：用于互斥锁死锁检测的任务阻塞信息。
	struct mutex_waiter		*blocked_on;
#endif

#ifdef CONFIG_TRACE_IRQFLAGS
    // 这些字段用于跟踪任务中硬中断和软中断的使能、禁用事件以及相关上下文。它们记录了事件发生的次数、位置以及中断是否已启用等信息。
	unsigned int			irq_events;
	unsigned long			hardirq_enable_ip;
	unsigned long			hardirq_disable_ip;
	unsigned int			hardirq_enable_event;
	unsigned int			hardirq_disable_event;
	int				hardirqs_enabled;
	int				hardirq_context;
	unsigned long			softirq_disable_ip;
	unsigned long			softirq_enable_ip;
	unsigned int			softirq_disable_event;
	unsigned int			softirq_enable_event;
	int				softirqs_enabled;
	int				softirq_context;
#endif

#ifdef CONFIG_LOCKDEP
# define MAX_LOCK_DEPTH			48UL
	// curr_chain_key: 当前锁链的 key 值，在使用 lockdep 模块分析内核时用于跟踪锁的持有情况。
	u64				curr_chain_key;
	// lockdep_depth: 当前任务递归的深度，用于 lockdep 模块进行死锁检测。
	int				lockdep_depth;
	// lockdep_recursion: 在持有一个锁期间，该任务递归持有的次数，用于 lockdep 模块进行死锁检测。
	unsigned int			lockdep_recursion;
	// held_locks: 当前任务持有的所有锁的列表，用于 lockdep 模块进行死锁检测。
	struct held_lock		held_locks[MAX_LOCK_DEPTH];
#endif

#ifdef CONFIG_UBSAN
	// in_ubsan: 标识是否正在进行 undefined behavior sanitizer 检测。
	unsigned int			in_ubsan;
#endif

	/* Journalling filesystem info: */
	// journal_info: 用于支持日志文件系统的相关信息。
	void				*journal_info;

	/* Stacked block device info: */
	// bio_list: 存储块设备的 bio 列表。
	struct bio_list			*bio_list;

#ifdef CONFIG_BLOCK
	/* Stack plugging: */
	// plug: 存储正在运行的 blk_plug 结构体的指针，用于延迟块层操作以提高性能。
	struct blk_plug			*plug;
#endif

	/* VM state: */
	// reclaim_state: 内存回收状态。
	struct reclaim_state		*reclaim_state;

	// backing_dev_info: 存储块设备的后备设备信息。
	struct backing_dev_info		*backing_dev_info;
	// io_context: 存储 I/O 上下文信息，用于进行 I/O 调度和帐户管理。
	struct io_context		*io_context;

	/* Ptrace state: */
	// ptrace_message: 存储最近的 ptrace 消息，用于追踪调试状态。
	unsigned long			ptrace_message;
	// last_siginfo: 存储最近的信号信息，用于追踪信号处理状态。
	kernel_siginfo_t		*last_siginfo;

	// ioac: 存储任务的 I/O 帐户信息，包括读写和取消读写的次数，以及消耗的时间等。
	struct task_io_accounting	ioac;
#ifdef CONFIG_PSI
	/* Pressure stall state */
	// psi_flags（Pressure Stall Information标志）用于跟踪与内存、CPU、IO相关的各种压力状况。
	unsigned int			psi_flags;
#endif
#ifdef CONFIG_TASK_XACCT
	/* Accumulated RSS usage: */
	// acct_rss_mem1和acct_vm_mem1用于跟踪进程的累积RSS和虚拟内存使用情况。这些值在进程执行fork()操作时被复制到子进程中。
	u64				acct_rss_mem1;
	/* Accumulated virtual memory usage: */
	u64				acct_vm_mem1;
	/* stime + utime since last update: */
	// acct_timexpd记录了最后一次更新以来的进程系统和用户空间的CPU时间。
	u64				acct_timexpd;
#endif
#ifdef CONFIG_CPUSETS
	/* Protected by ->alloc_lock: */
	// mems_allowed是一个节点掩码，表示进程允许使用的内存节点。它是由cgroups中的cpuset子系统使用的。
	nodemask_t			mems_allowed;
	/* Seqence number to catch updates: */
	// mems_allowed_seq是一个序列计数器，用于捕获mems_allowed字段的更改。
	seqcount_t			mems_allowed_seq;
	// cpuset_mem_spread_rotor和cpuset_slab_spread_rotor是两个调度器使用的转子值。
	int				cpuset_mem_spread_rotor;
	int				cpuset_slab_spread_rotor;
#endif
#ifdef CONFIG_CGROUPS
	/* Control Group info protected by css_set_lock: */
	// cgroups：控制组信息。受 css_set_lock 保护的 css_set __rcu 结构体指针。
	struct css_set __rcu		*cgroups;
	/* cg_list protected by css_set_lock and tsk->alloc_lock: */
	// cg_list 由 css_set_lock 和 tsk->alloc_lock 保护。
	struct list_head		cg_list;
#endif
#ifdef CONFIG_INTEL_RDT
	// closid 和 rmid：Intel RDT 中的相关字段。
	u32				closid;
	u32				rmid;
#endif
#ifdef CONFIG_FUTEX
	// robust_list 和 compat_robust_list：进程的鲁棒性 futex 链表，用于存储进程在退出时需要唤醒的 futex 等待者。
	struct robust_list_head __user	*robust_list;
#ifdef CONFIG_COMPAT
	struct compat_robust_list_head __user *compat_robust_list;
#endif
	// pi_state_list 和 pi_state_cache：PI futex 信息。pi_state_list 用于追踪使用 PI futex 的进程，pi_state_cache 用于缓存 PI futex 状态。
	struct list_head		pi_state_list;
	struct futex_pi_state		*pi_state_cache;
#endif
#ifdef CONFIG_PERF_EVENTS
	// perf_event_ctxp、perf_event_mutex 和 perf_event_list：
	// 性能事件信息。perf_event_ctxp 数组用于存储性能事件上下文，perf_event_mutex 用于保护 perf_event_list 链表，其中存储了任务正在运行的性能事件。
	struct perf_event_context	*perf_event_ctxp[perf_nr_task_contexts];
	struct mutex			perf_event_mutex;
	struct list_head		perf_event_list;
#endif
#ifdef CONFIG_DEBUG_PREEMPT
	// preempt_disable_ip：当预占抢占被禁用时，保存禁用预占抢占的指令指针地址。
	unsigned long			preempt_disable_ip;
#endif
#ifdef CONFIG_NUMA
	/* Protected by alloc_lock: */
	// mempolicy、il_prev 和 pref_node_fork：NUMA 信息。
	// mempolicy 存储了进程的内存策略，
	struct mempolicy		*mempolicy;
	// il_prev 是一个短整型，表示上一个使用的内存节点的编号。
	short				il_prev;
	// pref_node_fork 表示进程 fork 时的首选 NUMA 节点。
	short				pref_node_fork;
#endif
#ifdef CONFIG_NUMA_BALANCING
	// numa_scan_seq: 用于标记NUMA扫描序列的编号。
	int				numa_scan_seq;
	// numa_scan_period 和 numa_scan_period_max: 用于控制NUMA扫描的周期，分别是最小周期和最大周期。
	unsigned int			numa_scan_period;
	unsigned int			numa_scan_period_max;
	// numa_preferred_nid: 用于标记任务所偏好的NUMA节点ID。
	int				numa_preferred_nid;
	// numa_migrate_retry: 用于记录NUMA迁移的重试次数。
	unsigned long			numa_migrate_retry;
	/* Migration stamp: */
	// node_stamp: 用于记录最近一次NUMA迁移的时间戳。
	u64				node_stamp;
	// last_task_numa_placement: 用于记录最近一次任务的NUMA节点位置。
	u64				last_task_numa_placement;
	// last_sum_exec_runtime: 用于记录最近一次周期内任务的总运行时间。
	u64				last_sum_exec_runtime;
	// numa_work: 用于管理NUMA迁移相关的回调函数。
	struct callback_head		numa_work;

	// numa_group: 用于记录任务所在的NUMA组。
	struct numa_group		*numa_group;

	/*
	 * numa_faults is an array split into four regions:
	 * faults_memory, faults_cpu, faults_memory_buffer, faults_cpu_buffer
	 * in this precise order.
	 *
	 * faults_memory: Exponential decaying average of faults on a per-node
	 * basis. Scheduling placement decisions are made based on these
	 * counts. The values remain static for the duration of a PTE scan.
	 * faults_cpu: Track the nodes the process was running on when a NUMA
	 * hinting fault was incurred.
	 * faults_memory_buffer and faults_cpu_buffer: Record faults per node
	 * during the current scan window. When the scan completes, the counts
	 * in faults_memory and faults_cpu decay and these values are copied.
	 */
	// numa_faults：一个指向unsigned long类型的指针，记录了进程在NUMA架构下发生的内存错误数。此变量仅在启用NUMA平衡时才有效。
	unsigned long			*numa_faults;
	// total_numa_faults：一个unsigned long类型的计数器，表示该进程在NUMA架构下的内存错误总数。此变量仅在启用NUMA平衡时才有效。
	unsigned long			total_numa_faults;

	/*
	 * numa_faults_locality tracks if faults recorded during the last
	 * scan window were remote/local or failed to migrate. The task scan
	 * period is adapted based on the locality of the faults with different
	 * weights depending on whether they were shared or private faults
	 */
	// numa_faults_locality：一个长度为3的unsigned long类型数组，用于跟踪最近扫描窗口中发生的NUMA内存错误的本地性，共享性和失败迁移。此变量仅在启用NUMA平衡时才有效。
	unsigned long			numa_faults_locality[3];

	// numa_pages_migrated：一个unsigned long类型的计数器，表示该进程在NUMA架构下迁移的页面总数。此变量仅在启用NUMA平衡时才有效。
	unsigned long			numa_pages_migrated;
#endif /* CONFIG_NUMA_BALANCING */

#ifdef CONFIG_RSEQ
	// rseq：一个指向用户空间的rseq结构的指针，表示进程的RSEQ（restartable sequences）事件。此变量仅在启用RSEQ时才有效。
	struct rseq __user *rseq;
	// rseq_len：一个u32类型的计数器，表示RSEQ事件的长度（以字节为单位）。此变量仅在启用RSEQ时才有效。
	u32 rseq_len;
	// rseq_sig：一个u32类型的标志，表示当RSEQ事件发生时应该发送的信号。此变量仅在启用RSEQ时才有效。
	u32 rseq_sig;
	/*
	 * RmW on rseq_event_mask must be performed atomically
	 * with respect to preemption.
	 */
	// rseq_event_mask：一个unsigned long类型的标志，表示启用RSEQ事件掩码。此变量仅在启用RSEQ时才有效。
	unsigned long rseq_event_mask;
#endif
	// tlb_ubc：TLB刷新相关的数据结构，用于处理TLB失效和页表解除映射时的批处理。
	struct tlbflush_unmap_batch	tlb_ubc;

	// rcu：用于任务RCU机制的数据结构，用于在内存释放之前等待所有RCU引用的完成。
	struct rcu_head			rcu;

	/* Cache last used pipe for splice(): */
	// splice_pipe：缓存上一次使用的管道inode信息，用于splice()系统调用。
	struct pipe_inode_info		*splice_pipe;

	// task_frag：用于分配内核页框的数据结构，用于帮助内核分配小的内存块而不是整页。
	struct page_frag		task_frag;

#ifdef CONFIG_TASK_DELAY_ACCT
	// delays：与任务延迟账户相关的数据结构，用于记录任务的等待时间和延迟统计信息。
	struct task_delay_info		*delays;
#endif

#ifdef CONFIG_FAULT_INJECTION
	// make_it_fail 和 fail_nth：故障注入相关的字段，用于在特定的代码路径上模拟错误。
	int				make_it_fail;
	unsigned int			fail_nth;
#endif
	/*
	 * When (nr_dirtied >= nr_dirtied_pause), it's time to call
	 * balance_dirty_pages() for a dirty throttling pause:
	 */
	// nr_dirtied 和 nr_dirtied_pause 用于记录脏页的数量，其中 nr_dirtied_pause 记录了最近一次脏页写入操作暂停时的脏页数量。
	int				nr_dirtied;
	int				nr_dirtied_pause;
	/* Start of a write-and-pause period: */
	// dirty_paused_when 记录了最近一次脏页写入操作暂停的时间戳。
	unsigned long			dirty_paused_when;

#ifdef CONFIG_LATENCYTOP
	// latency_record_count 和 latency_record 用于记录调用函数的延迟时间，它们用于支持 latencytop 工具。
	int				latency_record_count;
	struct latency_record		latency_record[LT_SAVECOUNT];
#endif
	/*
	 * Time slack values; these are used to round up poll() and
	 * select() etc timeout values. These are in nanoseconds.
	 */
	// timer_slack_ns 和 default_timer_slack_ns 记录了计时器的时间松弛值，用于调整 poll()、select() 和其他超时操作的时间戳。
	u64				timer_slack_ns;
	u64				default_timer_slack_ns;

#ifdef CONFIG_KASAN
	// kasan_depth 表示当前的栈深度。
	unsigned int			kasan_depth;
#endif

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	/* Index of current stored address in ret_stack: */
	// curr_ret_stack 表示当前已经存储的返回地址的下标
	int				curr_ret_stack;
	// curr_ret_depth 表示当前已经存储的返回地址的数量
	int				curr_ret_depth;

	/* Stack of return addresses for return function tracing: */
	// ret_stack 是一个栈，用于存储返回地址
	struct ftrace_ret_stack		*ret_stack;

	/* Timestamp for last schedule: */
	// ftrace_timestamp 存储了最后一次调度的时间戳。
	unsigned long long		ftrace_timestamp;

	/*
	 * Number of functions that haven't been traced
	 * because of depth overrun:
	 */
	// trace_overrun 表示因为函数调用深度超出而未被追踪的函数数量
	atomic_t			trace_overrun;

	/* Pause tracing: */
	// tracing_graph_pause 表示是否暂停函数追踪
	atomic_t			tracing_graph_pause;
#endif

#ifdef CONFIG_TRACING
	/* State flags for use by tracers: */
	//  trace 字段可以用于跟踪状态标志
	unsigned long			trace;

	/* Bitmask and counter of trace recursion: */
	// trace_recursion 是一个位掩码和计数器，用于追踪跟踪的递归深度
	unsigned long			trace_recursion;
#endif /* CONFIG_TRACING */

#ifdef CONFIG_KCOV
	/* Coverage collection mode enabled for this task (0 if disabled): */
	//  kcov_mode 字段表示启用了哪种覆盖收集模式
	unsigned int			kcov_mode;

	/* Size of the kcov_area: */
	// kcov_size 字段表示覆盖缓冲区的大小
	unsigned int			kcov_size;

	/* Buffer for coverage collection: 
	// kcov_area 字段表示覆盖缓冲区的指针
	void				*kcov_area;

	/* KCOV descriptor wired with this task or NULL: */
	// kcov 字段是指向当前进程的 kcov 描述符的指针，该描述符与内核中的 kcov 子系统相关联。
	struct kcov			*kcov;
#endif

#ifdef CONFIG_MEMCG
	struct mem_cgroup		*memcg_in_oom;
	gfp_t				memcg_oom_gfp_mask;
	int				memcg_oom_order;

	/* Number of pages to reclaim on returning to userland: */
	unsigned int			memcg_nr_pages_over_high;

	/* Used by memcontrol for targeted memcg charge: */
	struct mem_cgroup		*active_memcg;
#endif

#ifdef CONFIG_BLK_CGROUP
	struct request_queue		*throttle_queue;
#endif

#ifdef CONFIG_UPROBES
	struct uprobe_task		*utask;
#endif
#if defined(CONFIG_BCACHE) || defined(CONFIG_BCACHE_MODULE)
	unsigned int			sequential_io;
	unsigned int			sequential_io_avg;
#endif
#ifdef CONFIG_DEBUG_ATOMIC_SLEEP
	unsigned long			task_state_change;
#endif
	int				pagefault_disabled;
#ifdef CONFIG_MMU
	// oom_reaper_list：用于OOM killer的重启列表。
	struct task_struct		*oom_reaper_list;
#endif
#ifdef CONFIG_VMAP_STACK
	// stack_vm_area：如果启用了VMAP stack，则此字段指向与任务关联的VM区域。
	struct vm_struct		*stack_vm_area;
#endif
#ifdef CONFIG_THREAD_INFO_IN_TASK
	/* A live task holds one reference: */
	// stack_refcount：如果线程信息存储在任务结构中，则此字段用于跟踪对任务堆栈的引用计数。
	atomic_t			stack_refcount;
#endif
#ifdef CONFIG_LIVEPATCH
	// patch_state：如果启用了Livepatch，则此字段用于跟踪任务的内核补丁状态。
	int patch_state;
#endif
#ifdef CONFIG_SECURITY
	/* Used by LSM modules for access restriction: */
	// security：用于LSM模块以限制访问的指针。
	void				*security;
#endif

#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	unsigned long			lowest_stack;
	unsigned long			prev_lowest_stack;
#endif

	/*
	 * New fields for task_struct should be added above here, so that
	 * they are included in the randomized portion of task_struct.
	 */
	randomized_struct_fields_end

	/* CPU-specific state of this task: */
	struct thread_struct		thread;

	/*
	 * WARNING: on x86, 'thread_struct' contains a variable-sized
	 * structure.  It *MUST* be at the end of 'task_struct'.
	 *
	 * Do not put anything below here!
	 */
};

static inline struct pid *task_pid(struct task_struct *task)
{
	return task->thread_pid;
}

/*
 * the helpers to get the task's different pids as they are seen
 * from various namespaces
 *
 * task_xid_nr()     : global id, i.e. the id seen from the init namespace;
 * task_xid_vnr()    : virtual id, i.e. the id seen from the pid namespace of
 *                     current.
 * task_xid_nr_ns()  : id seen from the ns specified;
 *
 * see also pid_nr() etc in include/linux/pid.h
 */
pid_t __task_pid_nr_ns(struct task_struct *task, enum pid_type type, struct pid_namespace *ns);

static inline pid_t task_pid_nr(struct task_struct *tsk)
{
	return tsk->pid;
}

static inline pid_t task_pid_nr_ns(struct task_struct *tsk, struct pid_namespace *ns)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_PID, ns);
}

static inline pid_t task_pid_vnr(struct task_struct *tsk)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_PID, NULL);
}


static inline pid_t task_tgid_nr(struct task_struct *tsk)
{
	return tsk->tgid;
}

/**
 * pid_alive - check that a task structure is not stale
 * @p: Task structure to be checked.
 *
 * Test if a process is not yet dead (at most zombie state)
 * If pid_alive fails, then pointers within the task structure
 * can be stale and must not be dereferenced.
 *
 * Return: 1 if the process is alive. 0 otherwise.
 */
static inline int pid_alive(const struct task_struct *p)
{
	return p->thread_pid != NULL;
}

static inline pid_t task_pgrp_nr_ns(struct task_struct *tsk, struct pid_namespace *ns)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_PGID, ns);
}

static inline pid_t task_pgrp_vnr(struct task_struct *tsk)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_PGID, NULL);
}


static inline pid_t task_session_nr_ns(struct task_struct *tsk, struct pid_namespace *ns)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_SID, ns);
}

static inline pid_t task_session_vnr(struct task_struct *tsk)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_SID, NULL);
}

static inline pid_t task_tgid_nr_ns(struct task_struct *tsk, struct pid_namespace *ns)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_TGID, ns);
}

static inline pid_t task_tgid_vnr(struct task_struct *tsk)
{
	return __task_pid_nr_ns(tsk, PIDTYPE_TGID, NULL);
}

static inline pid_t task_ppid_nr_ns(const struct task_struct *tsk, struct pid_namespace *ns)
{
	pid_t pid = 0;

	rcu_read_lock();
	if (pid_alive(tsk))
		pid = task_tgid_nr_ns(rcu_dereference(tsk->real_parent), ns);
	rcu_read_unlock();

	return pid;
}

static inline pid_t task_ppid_nr(const struct task_struct *tsk)
{
	return task_ppid_nr_ns(tsk, &init_pid_ns);
}

/* Obsolete, do not use: */
static inline pid_t task_pgrp_nr(struct task_struct *tsk)
{
	return task_pgrp_nr_ns(tsk, &init_pid_ns);
}

#define TASK_REPORT_IDLE	(TASK_REPORT + 1)
#define TASK_REPORT_MAX		(TASK_REPORT_IDLE << 1)

static inline unsigned int task_state_index(struct task_struct *tsk)
{
	unsigned int tsk_state = READ_ONCE(tsk->state);
	unsigned int state = (tsk_state | tsk->exit_state) & TASK_REPORT;

	BUILD_BUG_ON_NOT_POWER_OF_2(TASK_REPORT_MAX);

	if (tsk_state == TASK_IDLE)
		state = TASK_REPORT_IDLE;

	return fls(state);
}

static inline char task_index_to_char(unsigned int state)
{
	static const char state_char[] = "RSDTtXZPI";

	BUILD_BUG_ON(1 + ilog2(TASK_REPORT_MAX) != sizeof(state_char) - 1);

	return state_char[state];
}

static inline char task_state_to_char(struct task_struct *tsk)
{
	return task_index_to_char(task_state_index(tsk));
}

/**
 * is_global_init - check if a task structure is init. Since init
 * is free to have sub-threads we need to check tgid.
 * @tsk: Task structure to be checked.
 *
 * Check if a task structure is the first user space task the kernel created.
 *
 * Return: 1 if the task structure is init. 0 otherwise.
 */
static inline int is_global_init(struct task_struct *tsk)
{
	return task_tgid_nr(tsk) == 1;
}

extern struct pid *cad_pid;

/*
 * Per process flags
 */
#define PF_IDLE			0x00000002	/* I am an IDLE thread */
#define PF_EXITING		0x00000004	/* Getting shut down */
#define PF_EXITPIDONE		0x00000008	/* PI exit done on shut down */
#define PF_VCPU			0x00000010	/* I'm a virtual CPU */
#define PF_WQ_WORKER		0x00000020	/* I'm a workqueue worker */
#define PF_FORKNOEXEC		0x00000040	/* Forked but didn't exec */
#define PF_MCE_PROCESS		0x00000080      /* Process policy on mce errors */
#define PF_SUPERPRIV		0x00000100	/* Used super-user privileges */
#define PF_DUMPCORE		0x00000200	/* Dumped core */
#define PF_SIGNALED		0x00000400	/* Killed by a signal */
#define PF_MEMALLOC		0x00000800	/* Allocating memory */
#define PF_NPROC_EXCEEDED	0x00001000	/* set_user() noticed that RLIMIT_NPROC was exceeded */
#define PF_USED_MATH		0x00002000	/* If unset the fpu must be initialized before use */
#define PF_USED_ASYNC		0x00004000	/* Used async_schedule*(), used by module init */
#define PF_NOFREEZE		0x00008000	/* This thread should not be frozen */
#define PF_FROZEN		0x00010000	/* Frozen for system suspend */
#define PF_KSWAPD		0x00020000	/* I am kswapd */
#define PF_MEMALLOC_NOFS	0x00040000	/* All allocation requests will inherit GFP_NOFS */
#define PF_MEMALLOC_NOIO	0x00080000	/* All allocation requests will inherit GFP_NOIO */
#define PF_LESS_THROTTLE	0x00100000	/* Throttle me less: I clean memory */
#define PF_KTHREAD		0x00200000	/* I am a kernel thread */
#define PF_RANDOMIZE		0x00400000	/* Randomize virtual address space */
#define PF_SWAPWRITE		0x00800000	/* Allowed to write to swap */
#define PF_MEMSTALL		0x01000000	/* Stalled due to lack of memory */
#define PF_NO_SETAFFINITY	0x04000000	/* Userland is not allowed to meddle with cpus_allowed */
#define PF_MCE_EARLY		0x08000000      /* Early kill for mce process policy */
#define PF_MUTEX_TESTER		0x20000000	/* Thread belongs to the rt mutex tester */
#define PF_FREEZER_SKIP		0x40000000	/* Freezer should not count it as freezable */
#define PF_SUSPEND_TASK		0x80000000      /* This thread called freeze_processes() and should not be frozen */

/*
 * Only the _current_ task can read/write to tsk->flags, but other
 * tasks can access tsk->flags in readonly mode for example
 * with tsk_used_math (like during threaded core dumping).
 * There is however an exception to this rule during ptrace
 * or during fork: the ptracer task is allowed to write to the
 * child->flags of its traced child (same goes for fork, the parent
 * can write to the child->flags), because we're guaranteed the
 * child is not running and in turn not changing child->flags
 * at the same time the parent does it.
 */
#define clear_stopped_child_used_math(child)	do { (child)->flags &= ~PF_USED_MATH; } while (0)
#define set_stopped_child_used_math(child)	do { (child)->flags |= PF_USED_MATH; } while (0)
#define clear_used_math()			clear_stopped_child_used_math(current)
#define set_used_math()				set_stopped_child_used_math(current)

#define conditional_stopped_child_used_math(condition, child) \
	do { (child)->flags &= ~PF_USED_MATH, (child)->flags |= (condition) ? PF_USED_MATH : 0; } while (0)

#define conditional_used_math(condition)	conditional_stopped_child_used_math(condition, current)

#define copy_to_stopped_child_used_math(child) \
	do { (child)->flags &= ~PF_USED_MATH, (child)->flags |= current->flags & PF_USED_MATH; } while (0)

/* NOTE: this will return 0 or PF_USED_MATH, it will never return 1 */
#define tsk_used_math(p)			((p)->flags & PF_USED_MATH)
#define used_math()				tsk_used_math(current)

static inline bool is_percpu_thread(void)
{
#ifdef CONFIG_SMP
	return (current->flags & PF_NO_SETAFFINITY) &&
		(current->nr_cpus_allowed  == 1);
#else
	return true;
#endif
}

/* Per-process atomic flags. */
#define PFA_NO_NEW_PRIVS		0	/* May not gain new privileges. */
#define PFA_SPREAD_PAGE			1	/* Spread page cache over cpuset */
#define PFA_SPREAD_SLAB			2	/* Spread some slab caches over cpuset */
#define PFA_SPEC_SSB_DISABLE		3	/* Speculative Store Bypass disabled */
#define PFA_SPEC_SSB_FORCE_DISABLE	4	/* Speculative Store Bypass force disabled*/
#define PFA_SPEC_IB_DISABLE		5	/* Indirect branch speculation restricted */
#define PFA_SPEC_IB_FORCE_DISABLE	6	/* Indirect branch speculation permanently restricted */

#define TASK_PFA_TEST(name, func)					\
	static inline bool task_##func(struct task_struct *p)		\
	{ return test_bit(PFA_##name, &p->atomic_flags); }

#define TASK_PFA_SET(name, func)					\
	static inline void task_set_##func(struct task_struct *p)	\
	{ set_bit(PFA_##name, &p->atomic_flags); }

#define TASK_PFA_CLEAR(name, func)					\
	static inline void task_clear_##func(struct task_struct *p)	\
	{ clear_bit(PFA_##name, &p->atomic_flags); }

TASK_PFA_TEST(NO_NEW_PRIVS, no_new_privs)
TASK_PFA_SET(NO_NEW_PRIVS, no_new_privs)

TASK_PFA_TEST(SPREAD_PAGE, spread_page)
TASK_PFA_SET(SPREAD_PAGE, spread_page)
TASK_PFA_CLEAR(SPREAD_PAGE, spread_page)

TASK_PFA_TEST(SPREAD_SLAB, spread_slab)
TASK_PFA_SET(SPREAD_SLAB, spread_slab)
TASK_PFA_CLEAR(SPREAD_SLAB, spread_slab)

TASK_PFA_TEST(SPEC_SSB_DISABLE, spec_ssb_disable)
TASK_PFA_SET(SPEC_SSB_DISABLE, spec_ssb_disable)
TASK_PFA_CLEAR(SPEC_SSB_DISABLE, spec_ssb_disable)

TASK_PFA_TEST(SPEC_SSB_FORCE_DISABLE, spec_ssb_force_disable)
TASK_PFA_SET(SPEC_SSB_FORCE_DISABLE, spec_ssb_force_disable)

TASK_PFA_TEST(SPEC_IB_DISABLE, spec_ib_disable)
TASK_PFA_SET(SPEC_IB_DISABLE, spec_ib_disable)
TASK_PFA_CLEAR(SPEC_IB_DISABLE, spec_ib_disable)

TASK_PFA_TEST(SPEC_IB_FORCE_DISABLE, spec_ib_force_disable)
TASK_PFA_SET(SPEC_IB_FORCE_DISABLE, spec_ib_force_disable)

static inline void
current_restore_flags(unsigned long orig_flags, unsigned long flags)
{
	current->flags &= ~flags;
	current->flags |= orig_flags & flags;
}

extern int cpuset_cpumask_can_shrink(const struct cpumask *cur, const struct cpumask *trial);
extern int task_can_attach(struct task_struct *p, const struct cpumask *cs_cpus_allowed);
#ifdef CONFIG_SMP
extern void do_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask);
extern int set_cpus_allowed_ptr(struct task_struct *p, const struct cpumask *new_mask);
#else
static inline void do_set_cpus_allowed(struct task_struct *p, const struct cpumask *new_mask)
{
}
static inline int set_cpus_allowed_ptr(struct task_struct *p, const struct cpumask *new_mask)
{
	if (!cpumask_test_cpu(0, new_mask))
		return -EINVAL;
	return 0;
}
#endif

#ifndef cpu_relax_yield
#define cpu_relax_yield() cpu_relax()
#endif

extern int yield_to(struct task_struct *p, bool preempt);
extern void set_user_nice(struct task_struct *p, long nice);
extern int task_prio(const struct task_struct *p);

/**
 * task_nice - return the nice value of a given task.
 * @p: the task in question.
 *
 * Return: The nice value [ -20 ... 0 ... 19 ].
 */
static inline int task_nice(const struct task_struct *p)
{
	return PRIO_TO_NICE((p)->static_prio);
}

extern int can_nice(const struct task_struct *p, const int nice);
extern int task_curr(const struct task_struct *p);
extern int idle_cpu(int cpu);
extern int available_idle_cpu(int cpu);
extern int sched_setscheduler(struct task_struct *, int, const struct sched_param *);
extern int sched_setscheduler_nocheck(struct task_struct *, int, const struct sched_param *);
extern int sched_setattr(struct task_struct *, const struct sched_attr *);
extern int sched_setattr_nocheck(struct task_struct *, const struct sched_attr *);
extern struct task_struct *idle_task(int cpu);

/**
 * is_idle_task - is the specified task an idle task?
 * @p: the task in question.
 *
 * Return: 1 if @p is an idle task. 0 otherwise.
 */
static inline bool is_idle_task(const struct task_struct *p)
{
	return !!(p->flags & PF_IDLE);
}

extern struct task_struct *curr_task(int cpu);
extern void ia64_set_curr_task(int cpu, struct task_struct *p);

void yield(void);

union thread_union {
#ifndef CONFIG_ARCH_TASK_STRUCT_ON_STACK
	struct task_struct task;
#endif
#ifndef CONFIG_THREAD_INFO_IN_TASK
	struct thread_info thread_info;
#endif
	unsigned long stack[THREAD_SIZE/sizeof(long)];
};

#ifndef CONFIG_THREAD_INFO_IN_TASK
extern struct thread_info init_thread_info;
#endif

extern unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];

#ifdef CONFIG_THREAD_INFO_IN_TASK
static inline struct thread_info *task_thread_info(struct task_struct *task)
{
	return &task->thread_info;
}
#elif !defined(__HAVE_THREAD_FUNCTIONS)
# define task_thread_info(task)	((struct thread_info *)(task)->stack)
#endif

/*
 * find a task by one of its numerical ids
 *
 * find_task_by_pid_ns():
 *      finds a task by its pid in the specified namespace
 * find_task_by_vpid():
 *      finds a task by its virtual pid
 *
 * see also find_vpid() etc in include/linux/pid.h
 */

extern struct task_struct *find_task_by_vpid(pid_t nr);
extern struct task_struct *find_task_by_pid_ns(pid_t nr, struct pid_namespace *ns);

/*
 * find a task by its virtual pid and get the task struct
 */
extern struct task_struct *find_get_task_by_vpid(pid_t nr);

extern int wake_up_state(struct task_struct *tsk, unsigned int state);
extern int wake_up_process(struct task_struct *tsk);
extern void wake_up_new_task(struct task_struct *tsk);

#ifdef CONFIG_SMP
extern void kick_process(struct task_struct *tsk);
#else
static inline void kick_process(struct task_struct *tsk) { }
#endif

extern void __set_task_comm(struct task_struct *tsk, const char *from, bool exec);

static inline void set_task_comm(struct task_struct *tsk, const char *from)
{
	__set_task_comm(tsk, from, false);
}

extern char *__get_task_comm(char *to, size_t len, struct task_struct *tsk);
#define get_task_comm(buf, tsk) ({			\
	BUILD_BUG_ON(sizeof(buf) != TASK_COMM_LEN);	\
	__get_task_comm(buf, sizeof(buf), tsk);		\
})

#ifdef CONFIG_SMP
void scheduler_ipi(void);
extern unsigned long wait_task_inactive(struct task_struct *, long match_state);
#else
static inline void scheduler_ipi(void) { }
static inline unsigned long wait_task_inactive(struct task_struct *p, long match_state)
{
	return 1;
}
#endif

/*
 * Set thread flags in other task's structures.
 * See asm/thread_info.h for TIF_xxxx flags available:
 */
static inline void set_tsk_thread_flag(struct task_struct *tsk, int flag)
{
	set_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline void clear_tsk_thread_flag(struct task_struct *tsk, int flag)
{
	clear_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline void update_tsk_thread_flag(struct task_struct *tsk, int flag,
					  bool value)
{
	update_ti_thread_flag(task_thread_info(tsk), flag, value);
}

static inline int test_and_set_tsk_thread_flag(struct task_struct *tsk, int flag)
{
	return test_and_set_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline int test_and_clear_tsk_thread_flag(struct task_struct *tsk, int flag)
{
	return test_and_clear_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline int test_tsk_thread_flag(struct task_struct *tsk, int flag)
{
	return test_ti_thread_flag(task_thread_info(tsk), flag);
}

static inline void set_tsk_need_resched(struct task_struct *tsk)
{
	set_tsk_thread_flag(tsk,TIF_NEED_RESCHED);
}

static inline void clear_tsk_need_resched(struct task_struct *tsk)
{
	clear_tsk_thread_flag(tsk,TIF_NEED_RESCHED);
}

static inline int test_tsk_need_resched(struct task_struct *tsk)
{
	return unlikely(test_tsk_thread_flag(tsk,TIF_NEED_RESCHED));
}

/*
 * cond_resched() and cond_resched_lock(): latency reduction via
 * explicit rescheduling in places that are safe. The return
 * value indicates whether a reschedule was done in fact.
 * cond_resched_lock() will drop the spinlock before scheduling,
 */
#ifndef CONFIG_PREEMPT
extern int _cond_resched(void);
#else
static inline int _cond_resched(void) { return 0; }
#endif

#define cond_resched() ({			\
	___might_sleep(__FILE__, __LINE__, 0);	\
	_cond_resched();			\
})

extern int __cond_resched_lock(spinlock_t *lock);

#define cond_resched_lock(lock) ({				\
	___might_sleep(__FILE__, __LINE__, PREEMPT_LOCK_OFFSET);\
	__cond_resched_lock(lock);				\
})

static inline void cond_resched_rcu(void)
{
#if defined(CONFIG_DEBUG_ATOMIC_SLEEP) || !defined(CONFIG_PREEMPT_RCU)
	rcu_read_unlock();
	cond_resched();
	rcu_read_lock();
#endif
}

/*
 * Does a critical section need to be broken due to another
 * task waiting?: (technically does not depend on CONFIG_PREEMPT,
 * but a general need for low latency)
 */
static inline int spin_needbreak(spinlock_t *lock)
{
#ifdef CONFIG_PREEMPT
	return spin_is_contended(lock);
#else
	return 0;
#endif
}

static __always_inline bool need_resched(void)
{
	return unlikely(tif_need_resched());
}

/*
 * Wrappers for p->thread_info->cpu access. No-op on UP.
 */
#ifdef CONFIG_SMP

static inline unsigned int task_cpu(const struct task_struct *p)
{
#ifdef CONFIG_THREAD_INFO_IN_TASK
	return p->cpu;
#else
	return task_thread_info(p)->cpu;
#endif
}

extern void set_task_cpu(struct task_struct *p, unsigned int cpu);

#else

static inline unsigned int task_cpu(const struct task_struct *p)
{
	return 0;
}

static inline void set_task_cpu(struct task_struct *p, unsigned int cpu)
{
}

#endif /* CONFIG_SMP */

/*
 * In order to reduce various lock holder preemption latencies provide an
 * interface to see if a vCPU is currently running or not.
 *
 * This allows us to terminate optimistic spin loops and block, analogous to
 * the native optimistic spin heuristic of testing if the lock owner task is
 * running or not.
 */
#ifndef vcpu_is_preempted
# define vcpu_is_preempted(cpu)	false
#endif

extern long sched_setaffinity(pid_t pid, const struct cpumask *new_mask);
extern long sched_getaffinity(pid_t pid, struct cpumask *mask);

#ifndef TASK_SIZE_OF
#define TASK_SIZE_OF(tsk)	TASK_SIZE
#endif

#ifdef CONFIG_RSEQ

/*
 * Map the event mask on the user-space ABI enum rseq_cs_flags
 * for direct mask checks.
 */
enum rseq_event_mask_bits {
	RSEQ_EVENT_PREEMPT_BIT	= RSEQ_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT,
	RSEQ_EVENT_SIGNAL_BIT	= RSEQ_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT,
	RSEQ_EVENT_MIGRATE_BIT	= RSEQ_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT,
};

enum rseq_event_mask {
	RSEQ_EVENT_PREEMPT	= (1U << RSEQ_EVENT_PREEMPT_BIT),
	RSEQ_EVENT_SIGNAL	= (1U << RSEQ_EVENT_SIGNAL_BIT),
	RSEQ_EVENT_MIGRATE	= (1U << RSEQ_EVENT_MIGRATE_BIT),
};

static inline void rseq_set_notify_resume(struct task_struct *t)
{
	if (t->rseq)
		set_tsk_thread_flag(t, TIF_NOTIFY_RESUME);
}

void __rseq_handle_notify_resume(struct ksignal *sig, struct pt_regs *regs);

static inline void rseq_handle_notify_resume(struct ksignal *ksig,
					     struct pt_regs *regs)
{
	if (current->rseq)
		__rseq_handle_notify_resume(ksig, regs);
}

static inline void rseq_signal_deliver(struct ksignal *ksig,
				       struct pt_regs *regs)
{
	preempt_disable();
	__set_bit(RSEQ_EVENT_SIGNAL_BIT, &current->rseq_event_mask);
	preempt_enable();
	rseq_handle_notify_resume(ksig, regs);
}

/* rseq_preempt() requires preemption to be disabled. */
static inline void rseq_preempt(struct task_struct *t)
{
	__set_bit(RSEQ_EVENT_PREEMPT_BIT, &t->rseq_event_mask);
	rseq_set_notify_resume(t);
}

/* rseq_migrate() requires preemption to be disabled. */
static inline void rseq_migrate(struct task_struct *t)
{
	__set_bit(RSEQ_EVENT_MIGRATE_BIT, &t->rseq_event_mask);
	rseq_set_notify_resume(t);
}

/*
 * If parent process has a registered restartable sequences area, the
 * child inherits. Only applies when forking a process, not a thread.
 */
static inline void rseq_fork(struct task_struct *t, unsigned long clone_flags)
{
	if (clone_flags & CLONE_THREAD) {
		t->rseq = NULL;
		t->rseq_len = 0;
		t->rseq_sig = 0;
		t->rseq_event_mask = 0;
	} else {
		t->rseq = current->rseq;
		t->rseq_len = current->rseq_len;
		t->rseq_sig = current->rseq_sig;
		t->rseq_event_mask = current->rseq_event_mask;
	}
}

static inline void rseq_execve(struct task_struct *t)
{
	t->rseq = NULL;
	t->rseq_len = 0;
	t->rseq_sig = 0;
	t->rseq_event_mask = 0;
}

#else

static inline void rseq_set_notify_resume(struct task_struct *t)
{
}
static inline void rseq_handle_notify_resume(struct ksignal *ksig,
					     struct pt_regs *regs)
{
}
static inline void rseq_signal_deliver(struct ksignal *ksig,
				       struct pt_regs *regs)
{
}
static inline void rseq_preempt(struct task_struct *t)
{
}
static inline void rseq_migrate(struct task_struct *t)
{
}
static inline void rseq_fork(struct task_struct *t, unsigned long clone_flags)
{
}
static inline void rseq_execve(struct task_struct *t)
{
}

#endif

#ifdef CONFIG_DEBUG_RSEQ

void rseq_syscall(struct pt_regs *regs);

#else

static inline void rseq_syscall(struct pt_regs *regs)
{
}

#endif

#endif
