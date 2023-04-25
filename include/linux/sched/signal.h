/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SCHED_SIGNAL_H
#define _LINUX_SCHED_SIGNAL_H

#include <linux/rculist.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/sched/jobctl.h>
#include <linux/sched/task.h>
#include <linux/cred.h>

/*
 * Types defining task->signal and task->sighand and APIs using them:
 */

// 表示了一个进程的信号处理句柄
/*
该结构体通过存储在进程描述符的 sig 成员中，可以使进程通过不同的信号处理方式来响应接收到的信号。想要响应信号，需要设置对应信号的处理函数。
例如，内核中默认的信号处理函数是 do_signal() 函数，如果想要修改默认处理函数，可以通过修改 action 数组中的对应元素来实现。
*/
struct sighand_struct {
	// count：该信号处理句柄的引用计数。
	atomic_t		count;
	// action[_NSIG]：用于存放不同信号编号对应的处理函数等信息的数组。内核中一共定义了 64 种信号，数组的长度就是 _NSIG 常量，通常定义为 64。
	struct k_sigaction	action[_NSIG];
	// siglock：信号处理句柄的自旋锁，用于保护信号处理句柄的修改操作。
	spinlock_t		siglock;
	// signalfd_wqh：用于处理 signalfd 的等待队列头。
	wait_queue_head_t	signalfd_wqh;
};

/*
 * Per-process accounting stats:
 */
struct pacct_struct {
	int			ac_flag;
	long			ac_exitcode;
	unsigned long		ac_mem;
	u64			ac_utime, ac_stime;
	unsigned long		ac_minflt, ac_majflt;
};

struct cpu_itimer {
	u64 expires;
	u64 incr;
};

/*
 * This is the atomic variant of task_cputime, which can be used for
 * storing and updating task_cputime statistics without locking.
 */
struct task_cputime_atomic {
	atomic64_t utime;
	atomic64_t stime;
	atomic64_t sum_exec_runtime;
};

#define INIT_CPUTIME_ATOMIC \
	(struct task_cputime_atomic) {				\
		.utime = ATOMIC64_INIT(0),			\
		.stime = ATOMIC64_INIT(0),			\
		.sum_exec_runtime = ATOMIC64_INIT(0),		\
	}
/**
 * struct thread_group_cputimer - thread group interval timer counts
 * @cputime_atomic:	atomic thread group interval timers.
 * @running:		true when there are timers running and
 *			@cputime_atomic receives updates.
 * @checking_timer:	true when a thread in the group is in the
 *			process of checking for thread group timers.
 *
 * This structure contains the version of task_cputime, above, that is
 * used for thread group CPU timer calculations.
 */
struct thread_group_cputimer {
	struct task_cputime_atomic cputime_atomic;
	bool running;
	bool checking_timer;
};

struct multiprocess_signals {
	sigset_t signal;
	struct hlist_node node;
};

/*
 * NOTE! "signal_struct" does not have its own
 * locking, because a shared signal_struct always
 * implies a shared sighand_struct, so locking
 * sighand_struct is always a proper superset of
 * the locking of signal_struct.
 * 注意！"signal_struct" 没有自己的锁，因为共享的 signal_struct 总是意味着共享的 sighand_struct，
 * 所以锁定 sighand_struct 总是 signal_struct 锁定的适当超集。
 * 
 * signal_struct: 用于表示进程的信号状态
 */
struct signal_struct {
	// sigcnt：表示尚未被处理的信号数目。
	atomic_t		sigcnt;
	// live：表示该进程是否仍然存活。
	atomic_t		live;
	// nr_threads：表示该进程的线程数。
	int			nr_threads;
	// thread_head：task_struct 结构体的链表头，用于遍历该进程的所有线程。
	struct list_head	thread_head;

	// wait_chldexit：管道等待队列头，用于等待子进程退出。
	wait_queue_head_t	wait_chldexit;	/* for wait4() */

	/* current thread group signal load-balancing target: */
	// curr_target：当前目标线程，用于管理针对线程组的信号预先平衡。
	struct task_struct	*curr_target;

	/* shared signal handling: */
	// shared_pending：在进程间共享的待处理信号，用于进程间传递信号。
	struct sigpending	shared_pending;

	/* For collecting multiprocess signals during fork */
	// multiprocess：存储多进程信息的哈希链表。
	struct hlist_head	multiprocess;

	/* thread group exit support */
	// group_exit_code：该进程组是否已经退出。
	int			group_exit_code;
	/* overloaded:
	 * - notify group_exit_task when ->count is equal to notify_count
	 * - everyone except group_exit_task is stopped during signal delivery
	 *   of fatal signals, group_exit_task processes the signal.
	 */
	// notify_count：当该进程组的信号状态计数为通知次数时，通知完毕。
	int			notify_count;
	// group_exit_task：用于处理信号的进程组任务。
	struct task_struct	*group_exit_task;

	/* thread group stop support, overloads group_exit_code too */
	// group_stop_count：用于进程组停止信号功能的计数。
	int			group_stop_count;
	// flags：用于表示 Signal 以及特殊标志的位掩码。
	unsigned int		flags; /* see SIGNAL_* flags below */

	/*
	 * PR_SET_CHILD_SUBREAPER marks a process, like a service
	 * manager, to re-parent orphan (double-forking) child processes
	 * to this process instead of 'init'. The service manager is
	 * able to receive SIGCHLD signals and is able to investigate
	 * the process until it calls wait(). All children of this
	 * process will inherit a flag if they should look for a
	 * child_subreaper process at exit.
	 */
	// is_child_subreaper：标记该进程是否为子控制程序。
	unsigned int		is_child_subreaper:1;
	unsigned int		has_child_subreaper:1;

#ifdef CONFIG_POSIX_TIMERS

	/* POSIX.1b Interval Timers */
	// posix_timer_id：表示定时器链表中计时器的编号。
	int			posix_timer_id;
	// posix_timers：定时器链表，用于存储 POSIX.1b 定时器。
	struct list_head	posix_timers;

	/* ITIMER_REAL timer for the process */
	// real_timer：ITIMER_REAL 定时器。
	struct hrtimer real_timer;
	// it_real_incr：ITIMER_REAL 的定时增量。
	ktime_t it_real_incr;

	/*
	 * ITIMER_PROF and ITIMER_VIRTUAL timers for the process, we use
	 * CPUCLOCK_PROF and CPUCLOCK_VIRT for indexing array as these
	 * values are defined to 0 and 1 respectively
	 */
	// it[2]：ITIMER_PROF 和 ITIMER_VIRTUAL 定时器。
	struct cpu_itimer it[2];

	/*
	 * Thread group totals for process CPU timers.
	 * See thread_group_cputimer(), et al, for details.
	 */
	// cputimer：线程组 CPU 计时器。
	struct thread_group_cputimer cputimer;

	/* Earliest-expiration cache. */
	// cputime_expires：记录时钟周期最靠前过期的时间的值。
	struct task_cputime cputime_expires;

	// cpu_timers[3]：三重强制映射的定时器列表。
	struct list_head cpu_timers[3];

#endif

	/* PID/PID hash table linkage. */
	// pids[PIDTYPE_MAX]：与 PID 相关联的指针数组。
	struct pid *pids[PIDTYPE_MAX];

#ifdef CONFIG_NO_HZ_FULL
	atomic_t tick_dep_mask;
#endif

	struct pid *tty_old_pgrp;

	/* boolean value for session group leader */
	// leader：指示是否为会话组领导者。
	int leader;
	// tty：进程的 tty 设备指针。
	struct tty_struct *tty; /* NULL if no tty */

#ifdef CONFIG_SCHED_AUTOGROUP
	struct autogroup *autogroup;
#endif
	/*
	 * Cumulative resource counters for dead threads in the group,
	 * and for reaped dead child processes forked by this group.
	 * Live threads maintain their own counters and add to these
	 * in __exit_signal, except for the group leader.
	 */
	// stats_lock：用于同步统计数据的序列化锁。
	seqlock_t stats_lock;
	// utime、stime、cutime、cstime：记录耗费在用户空间/内核空间、所有子进程的用户空间/内核空间时间。
	u64 utime, stime, cutime, cstime;
	// gtime 和 cgtime：有关 CPU 时间的数据。
	u64 gtime;
	u64 cgtime;
	// prev_cputime：保留用于统计上一个 CPU 时间量的结构体。
	struct prev_cputime prev_cputime;
	// nvcsw、nivcsw、cnvcsw、cnivcsw、inblock、oublock、cinblock、coublock、maxrss、cmaxrss：以不同方式度量极限和累计资源使用。配合 task_IO_accounting 结构体使用。
	unsigned long nvcsw, nivcsw, cnvcsw, cnivcsw;
	unsigned long min_flt, maj_flt, cmin_flt, cmaj_flt;
	unsigned long inblock, oublock, cinblock, coublock;
	unsigned long maxrss, cmaxrss;
	// ioac：记录任务进行的所有 I/O 请求的结构体。
	struct task_io_accounting ioac;

	/*
	 * Cumulative ns of schedule CPU time fo dead threads in the
	 * group, not including a zombie group leader, (This only differs
	 * from jiffies_to_ns(utime + stime) if sched_clock uses something
	 * other than jiffies.)
	 */
	// sum_sched_runtime：记录所属线程组过去已使用的 CPU 时间，对内核动态时间调度非常重要。
	unsigned long long sum_sched_runtime;

	/*
	 * We don't bother to synchronize most readers of this at all,
	 * because there is no reader checking a limit that actually needs
	 * to get both rlim_cur and rlim_max atomically, and either one
	 * alone is a single word that can safely be read normally.
	 * getrlimit/setrlimit use task_lock(current->group_leader) to
	 * protect this instead of the siglock, because they really
	 * have no need to disable irqs.
	 */
	// rlim[RLIM_NLIMITS]：关于进程资源限制的数组。
	struct rlimit rlim[RLIM_NLIMITS];

#ifdef CONFIG_BSD_PROCESS_ACCT
	// pacct：关于进程级别的账户信息的结构体。
	struct pacct_struct pacct;	/* per-process accounting information */
#endif
#ifdef CONFIG_TASKSTATS
	// stats：任务状态结构体指针。
	struct taskstats *stats;
#endif
#ifdef CONFIG_AUDIT
	// audit_tty：审计终端。
	unsigned audit_tty;
	// tty_audit_buf：审计缓冲区。
	struct tty_audit_buf *tty_audit_buf;
#endif

	/*
	 * Thread is the potential origin of an oom condition; kill first on
	 * oom
	 */
	// oom_flag_origin：标记是否为 OOM 警告起源的线程组。
	bool oom_flag_origin;
	// oom_score_adj：OOM 警告的得分调整。
	short oom_score_adj;		/* OOM kill score adjustment */
	// oom_score_adj_min：OOM 警告得分调整的最小值。
	short oom_score_adj_min;	/* OOM kill score adjustment min value.
					 * Only settable by CAP_SYS_RESOURCE. */
	// oom_mm：记录为什么原因、在什么情况下杀死当前进程的内存映像。
	struct mm_struct *oom_mm;	/* recorded mm when the thread group got
					 * killed by the oom killer */

	struct mutex cred_guard_mutex;	/* guard against foreign influences on
					 * credential calculations
					 * (notably. ptrace) */
} __randomize_layout;

/*
 * Bits in flags field of signal_struct.
 */
// SIGNAL_STOP_STOPPED：表示作业控制停止在进行中。
#define SIGNAL_STOP_STOPPED	0x00000001 /* job control stop in effect */
// SIGNAL_STOP_CONTINUED：表示收到 SIGCONT 信号以恢复作业控制。
#define SIGNAL_STOP_CONTINUED	0x00000002 /* SIGCONT since WCONTINUED reap */
// SIGNAL_GROUP_EXIT：表示进程组退出正在进行中。
#define SIGNAL_GROUP_EXIT	0x00000004 /* group exit in progress */
// SIGNAL_GROUP_COREDUMP：表示进程组的内存映像正在进行 coredump。
#define SIGNAL_GROUP_COREDUMP	0x00000008 /* coredump in progress */
/*
 * Pending notifications to parent.
 这段代码定义了另外一些标志宏用于定义 signal_struct 结构体 flags 字段的属性
 */
// SIGNAL_CLD_STOPPED：表示作业控制停止位于子进程中。
#define SIGNAL_CLD_STOPPED	0x00000010
// SIGNAL_CLD_CONTINUED：表示子进程正在处理来自父进程的 SIGCONT 信号。
#define SIGNAL_CLD_CONTINUED	0x00000020
// SIGNAL_CLD_MASK：与子进程相关的信号标志掩码。
#define SIGNAL_CLD_MASK		(SIGNAL_CLD_STOPPED|SIGNAL_CLD_CONTINUED)

// SIGNAL_UNKILLABLE：表示进程在接收到致命信号时不会被杀死，只会进行错误处理或产生内核转储。
#define SIGNAL_UNKILLABLE	0x00000040 /* for init: ignore fatal signals */

// SIGNAL_STOP_MASK：用于掩盖与停止有关的信号标志。
#define SIGNAL_STOP_MASK (SIGNAL_CLD_MASK | SIGNAL_STOP_STOPPED | \
			  SIGNAL_STOP_CONTINUED)

static inline void signal_set_stop_flags(struct signal_struct *sig,
					 unsigned int flags)
{
	WARN_ON(sig->flags & (SIGNAL_GROUP_EXIT|SIGNAL_GROUP_COREDUMP));
	sig->flags = (sig->flags & ~SIGNAL_STOP_MASK) | flags;
}

/* If true, all threads except ->group_exit_task have pending SIGKILL */
static inline int signal_group_exit(const struct signal_struct *sig)
{
	return	(sig->flags & SIGNAL_GROUP_EXIT) ||
		(sig->group_exit_task != NULL);
}

extern void flush_signals(struct task_struct *);
extern void ignore_signals(struct task_struct *);
extern void flush_signal_handlers(struct task_struct *, int force_default);
extern int dequeue_signal(struct task_struct *tsk, sigset_t *mask, kernel_siginfo_t *info);

static inline int kernel_dequeue_signal(void)
{
	struct task_struct *tsk = current;
	kernel_siginfo_t __info;
	int ret;

	spin_lock_irq(&tsk->sighand->siglock);
	ret = dequeue_signal(tsk, &tsk->blocked, &__info);
	spin_unlock_irq(&tsk->sighand->siglock);

	return ret;
}

static inline void kernel_signal_stop(void)
{
	spin_lock_irq(&current->sighand->siglock);
	if (current->jobctl & JOBCTL_STOP_DEQUEUED)
		set_special_state(TASK_STOPPED);
	spin_unlock_irq(&current->sighand->siglock);

	schedule();
}
#ifdef __ARCH_SI_TRAPNO
# define ___ARCH_SI_TRAPNO(_a1) , _a1
#else
# define ___ARCH_SI_TRAPNO(_a1)
#endif
#ifdef __ia64__
# define ___ARCH_SI_IA64(_a1, _a2, _a3) , _a1, _a2, _a3
#else
# define ___ARCH_SI_IA64(_a1, _a2, _a3)
#endif

int force_sig_fault(int sig, int code, void __user *addr
	___ARCH_SI_TRAPNO(int trapno)
	___ARCH_SI_IA64(int imm, unsigned int flags, unsigned long isr)
	, struct task_struct *t);
int send_sig_fault(int sig, int code, void __user *addr
	___ARCH_SI_TRAPNO(int trapno)
	___ARCH_SI_IA64(int imm, unsigned int flags, unsigned long isr)
	, struct task_struct *t);

int force_sig_mceerr(int code, void __user *, short, struct task_struct *);
int send_sig_mceerr(int code, void __user *, short, struct task_struct *);

int force_sig_bnderr(void __user *addr, void __user *lower, void __user *upper);
int force_sig_pkuerr(void __user *addr, u32 pkey);

int force_sig_ptrace_errno_trap(int errno, void __user *addr);

extern int send_sig_info(int, struct kernel_siginfo *, struct task_struct *);
extern void force_sigsegv(int sig, struct task_struct *p);
extern int force_sig_info(int, struct kernel_siginfo *, struct task_struct *);
extern int __kill_pgrp_info(int sig, struct kernel_siginfo *info, struct pid *pgrp);
extern int kill_pid_info(int sig, struct kernel_siginfo *info, struct pid *pid);
extern int kill_pid_info_as_cred(int, struct kernel_siginfo *, struct pid *,
				const struct cred *);
extern int kill_pgrp(struct pid *pid, int sig, int priv);
extern int kill_pid(struct pid *pid, int sig, int priv);
extern __must_check bool do_notify_parent(struct task_struct *, int);
extern void __wake_up_parent(struct task_struct *p, struct task_struct *parent);
extern void force_sig(int, struct task_struct *);
extern int send_sig(int, struct task_struct *, int);
extern int zap_other_threads(struct task_struct *p);
extern struct sigqueue *sigqueue_alloc(void);
extern void sigqueue_free(struct sigqueue *);
extern int send_sigqueue(struct sigqueue *, struct pid *, enum pid_type);
extern int do_sigaction(int, struct k_sigaction *, struct k_sigaction *);

static inline int restart_syscall(void)
{
	set_tsk_thread_flag(current, TIF_SIGPENDING);
	return -ERESTARTNOINTR;
}

static inline int signal_pending(struct task_struct *p)
{
	return unlikely(test_tsk_thread_flag(p,TIF_SIGPENDING));
}

static inline int __fatal_signal_pending(struct task_struct *p)
{
	return unlikely(sigismember(&p->pending.signal, SIGKILL));
}

static inline int fatal_signal_pending(struct task_struct *p)
{
	return signal_pending(p) && __fatal_signal_pending(p);
}

static inline int signal_pending_state(long state, struct task_struct *p)
{
	if (!(state & (TASK_INTERRUPTIBLE | TASK_WAKEKILL)))
		return 0;
	if (!signal_pending(p))
		return 0;

	return (state & TASK_INTERRUPTIBLE) || __fatal_signal_pending(p);
}

/*
 * Reevaluate whether the task has signals pending delivery.
 * Wake the task if so.
 * This is required every time the blocked sigset_t changes.
 * callers must hold sighand->siglock.
 */
extern void recalc_sigpending_and_wake(struct task_struct *t);
extern void recalc_sigpending(void);
extern void calculate_sigpending(void);

extern void signal_wake_up_state(struct task_struct *t, unsigned int state);

static inline void signal_wake_up(struct task_struct *t, bool resume)
{
	signal_wake_up_state(t, resume ? TASK_WAKEKILL : 0);
}
static inline void ptrace_signal_wake_up(struct task_struct *t, bool resume)
{
	signal_wake_up_state(t, resume ? __TASK_TRACED : 0);
}

void task_join_group_stop(struct task_struct *task);

#ifdef TIF_RESTORE_SIGMASK
/*
 * Legacy restore_sigmask accessors.  These are inefficient on
 * SMP architectures because they require atomic operations.
 */

/**
 * set_restore_sigmask() - make sure saved_sigmask processing gets done
 *
 * This sets TIF_RESTORE_SIGMASK and ensures that the arch signal code
 * will run before returning to user mode, to process the flag.  For
 * all callers, TIF_SIGPENDING is already set or it's no harm to set
 * it.  TIF_RESTORE_SIGMASK need not be in the set of bits that the
 * arch code will notice on return to user mode, in case those bits
 * are scarce.  We set TIF_SIGPENDING here to ensure that the arch
 * signal code always gets run when TIF_RESTORE_SIGMASK is set.
 */
static inline void set_restore_sigmask(void)
{
	set_thread_flag(TIF_RESTORE_SIGMASK);
	WARN_ON(!test_thread_flag(TIF_SIGPENDING));
}
static inline void clear_restore_sigmask(void)
{
	clear_thread_flag(TIF_RESTORE_SIGMASK);
}
static inline bool test_restore_sigmask(void)
{
	return test_thread_flag(TIF_RESTORE_SIGMASK);
}
static inline bool test_and_clear_restore_sigmask(void)
{
	return test_and_clear_thread_flag(TIF_RESTORE_SIGMASK);
}

#else	/* TIF_RESTORE_SIGMASK */

/* Higher-quality implementation, used if TIF_RESTORE_SIGMASK doesn't exist. */
static inline void set_restore_sigmask(void)
{
	current->restore_sigmask = true;
	WARN_ON(!test_thread_flag(TIF_SIGPENDING));
}
static inline void clear_restore_sigmask(void)
{
	current->restore_sigmask = false;
}
static inline bool test_restore_sigmask(void)
{
	return current->restore_sigmask;
}
static inline bool test_and_clear_restore_sigmask(void)
{
	if (!current->restore_sigmask)
		return false;
	current->restore_sigmask = false;
	return true;
}
#endif

static inline void restore_saved_sigmask(void)
{
	if (test_and_clear_restore_sigmask())
		__set_current_blocked(&current->saved_sigmask);
}

static inline sigset_t *sigmask_to_save(void)
{
	sigset_t *res = &current->blocked;
	if (unlikely(test_restore_sigmask()))
		res = &current->saved_sigmask;
	return res;
}

static inline int kill_cad_pid(int sig, int priv)
{
	return kill_pid(cad_pid, sig, priv);
}

/* These can be the second arg to send_sig_info/send_group_sig_info.  */
#define SEND_SIG_NOINFO ((struct kernel_siginfo *) 0)
#define SEND_SIG_PRIV	((struct kernel_siginfo *) 1)

/*
 * True if we are on the alternate signal stack.
 */
static inline int on_sig_stack(unsigned long sp)
{
	/*
	 * If the signal stack is SS_AUTODISARM then, by construction, we
	 * can't be on the signal stack unless user code deliberately set
	 * SS_AUTODISARM when we were already on it.
	 *
	 * This improves reliability: if user state gets corrupted such that
	 * the stack pointer points very close to the end of the signal stack,
	 * then this check will enable the signal to be handled anyway.
	 */
	if (current->sas_ss_flags & SS_AUTODISARM)
		return 0;

#ifdef CONFIG_STACK_GROWSUP
	return sp >= current->sas_ss_sp &&
		sp - current->sas_ss_sp < current->sas_ss_size;
#else
	return sp > current->sas_ss_sp &&
		sp - current->sas_ss_sp <= current->sas_ss_size;
#endif
}

static inline int sas_ss_flags(unsigned long sp)
{
	if (!current->sas_ss_size)
		return SS_DISABLE;

	return on_sig_stack(sp) ? SS_ONSTACK : 0;
}

static inline void sas_ss_reset(struct task_struct *p)
{
	p->sas_ss_sp = 0;
	p->sas_ss_size = 0;
	p->sas_ss_flags = SS_DISABLE;
}

static inline unsigned long sigsp(unsigned long sp, struct ksignal *ksig)
{
	if (unlikely((ksig->ka.sa.sa_flags & SA_ONSTACK)) && ! sas_ss_flags(sp))
#ifdef CONFIG_STACK_GROWSUP
		return current->sas_ss_sp;
#else
		return current->sas_ss_sp + current->sas_ss_size;
#endif
	return sp;
}

extern void __cleanup_sighand(struct sighand_struct *);
extern void flush_itimer_signals(void);

#define tasklist_empty() \
	list_empty(&init_task.tasks)

#define next_task(p) \
	list_entry_rcu((p)->tasks.next, struct task_struct, tasks)

#define for_each_process(p) \
	for (p = &init_task ; (p = next_task(p)) != &init_task ; )

extern bool current_is_single_threaded(void);

/*
 * Careful: do_each_thread/while_each_thread is a double loop so
 *          'break' will not work as expected - use goto instead.
 */
#define do_each_thread(g, t) \
	for (g = t = &init_task ; (g = t = next_task(g)) != &init_task ; ) do

#define while_each_thread(g, t) \
	while ((t = next_thread(t)) != g)

#define __for_each_thread(signal, t)	\
	list_for_each_entry_rcu(t, &(signal)->thread_head, thread_node)

#define for_each_thread(p, t)		\
	__for_each_thread((p)->signal, t)

/* Careful: this is a double loop, 'break' won't work as expected. */
#define for_each_process_thread(p, t)	\
	for_each_process(p) for_each_thread(p, t)

typedef int (*proc_visitor)(struct task_struct *p, void *data);
void walk_process_tree(struct task_struct *top, proc_visitor, void *);

static inline
struct pid *task_pid_type(struct task_struct *task, enum pid_type type)
{
	struct pid *pid;
	if (type == PIDTYPE_PID)
		pid = task_pid(task);
	else
		pid = task->signal->pids[type];
	return pid;
}

static inline struct pid *task_tgid(struct task_struct *task)
{
	return task->signal->pids[PIDTYPE_TGID];
}

/*
 * Without tasklist or RCU lock it is not safe to dereference
 * the result of task_pgrp/task_session even if task == current,
 * we can race with another thread doing sys_setsid/sys_setpgid.
 */
static inline struct pid *task_pgrp(struct task_struct *task)
{
	return task->signal->pids[PIDTYPE_PGID];
}

static inline struct pid *task_session(struct task_struct *task)
{
	return task->signal->pids[PIDTYPE_SID];
}

static inline int get_nr_threads(struct task_struct *tsk)
{
	return tsk->signal->nr_threads;
}

static inline bool thread_group_leader(struct task_struct *p)
{
	return p->exit_signal >= 0;
}

/* Do to the insanities of de_thread it is possible for a process
 * to have the pid of the thread group leader without actually being
 * the thread group leader.  For iteration through the pids in proc
 * all we care about is that we have a task with the appropriate
 * pid, we don't actually care if we have the right task.
 */
static inline bool has_group_leader_pid(struct task_struct *p)
{
	return task_pid(p) == task_tgid(p);
}

static inline
bool same_thread_group(struct task_struct *p1, struct task_struct *p2)
{
	return p1->signal == p2->signal;
}

static inline struct task_struct *next_thread(const struct task_struct *p)
{
	return list_entry_rcu(p->thread_group.next,
			      struct task_struct, thread_group);
}

static inline int thread_group_empty(struct task_struct *p)
{
	return list_empty(&p->thread_group);
}

#define delay_group_leader(p) \
		(thread_group_leader(p) && !thread_group_empty(p))

extern struct sighand_struct *__lock_task_sighand(struct task_struct *tsk,
							unsigned long *flags);

static inline struct sighand_struct *lock_task_sighand(struct task_struct *tsk,
						       unsigned long *flags)
{
	struct sighand_struct *ret;

	ret = __lock_task_sighand(tsk, flags);
	(void)__cond_lock(&tsk->sighand->siglock, ret);
	return ret;
}

static inline void unlock_task_sighand(struct task_struct *tsk,
						unsigned long *flags)
{
	spin_unlock_irqrestore(&tsk->sighand->siglock, *flags);
}

static inline unsigned long task_rlimit(const struct task_struct *tsk,
		unsigned int limit)
{
	return READ_ONCE(tsk->signal->rlim[limit].rlim_cur);
}

static inline unsigned long task_rlimit_max(const struct task_struct *tsk,
		unsigned int limit)
{
	return READ_ONCE(tsk->signal->rlim[limit].rlim_max);
}

static inline unsigned long rlimit(unsigned int limit)
{
	return task_rlimit(current, limit);
}

static inline unsigned long rlimit_max(unsigned int limit)
{
	return task_rlimit_max(current, limit);
}

#endif /* _LINUX_SCHED_SIGNAL_H */
