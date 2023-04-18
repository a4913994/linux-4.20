/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __INCLUDE_LINUX_OOM_H
#define __INCLUDE_LINUX_OOM_H


#include <linux/sched/signal.h>
#include <linux/types.h>
#include <linux/nodemask.h>
#include <uapi/linux/oom.h>
#include <linux/sched/coredump.h> /* MMF_* */
#include <linux/mm.h> /* VM_FAULT* */

struct zonelist;
struct notifier_block;
struct mem_cgroup;
struct task_struct;

/*
 * Details of the page allocation that triggered the oom killer that are used to
 * determine what should be killed.
 */
// struct oom_control 是一个结构体，用于存储内核在处理内存不足时（即内存压力过大，无法满足内存分配请求时）
// 所需的各种参数和信息。在这种情况下，内核会尝试通过 OOM（Out-of-Memory）机制杀死一些进程，以释放内存空间。
struct oom_control {
	/* Used to determine cpuset */
	// zonelist: 一个指向 struct zonelist 的指针，用于确定进程所在的 cpuset。
	struct zonelist *zonelist;

	/* Used to determine mempolicy */
	// nodemask: 一个指向 nodemask_t 类型的指针，用于确定内存分配策略。
	nodemask_t *nodemask;

	/* Memory cgroup in which oom is invoked, or NULL for global oom */
	// memcg: 一个指向 struct mem_cgroup 的指针，表示触发 OOM 的内存 cgroup，如果为 NULL，则表示全局 OOM。
	struct mem_cgroup *memcg;

	/* Used to determine cpuset and node locality requirement */
	// gfp_mask: gfp_t 类型的变量，用于确定 cpuset 和节点位置要求。
	const gfp_t gfp_mask;

	/*
	 * order == -1 means the oom kill is required by sysrq, otherwise only
	 * for display purposes.
	 */
	// order: 一个整型变量，表示分配的内存大小。当 order 为 -1 时，表示 OOM kill 是由 sysrq 请求的；否则，仅用于显示目的。
	const int order;

	/* Used by oom implementation, do not set */
	// totalpages: 一个无符号长整型变量，用于 OOM 实现，表示系统中总的内存页数。
	unsigned long totalpages;
	// chosen: 一个指向 struct task_struct 的指针，表示被选中用于 OOM kill 的进程。
	struct task_struct *chosen;
	// chosen_points: 一个无符号长整型变量，表示被选中进程的 OOM 分数（用于确定哪个进程更适合被杀死以释放内存）。较高的分数表示该进程更可能被选中用于 OOM kill。
	unsigned long chosen_points;
};

extern struct mutex oom_lock;

static inline void set_current_oom_origin(void)
{
	current->signal->oom_flag_origin = true;
}

static inline void clear_current_oom_origin(void)
{
	current->signal->oom_flag_origin = false;
}

static inline bool oom_task_origin(const struct task_struct *p)
{
	return p->signal->oom_flag_origin;
}

static inline bool tsk_is_oom_victim(struct task_struct * tsk)
{
	return tsk->signal->oom_mm;
}

/*
 * Use this helper if tsk->mm != mm and the victim mm needs a special
 * handling. This is guaranteed to stay true after once set.
 */
static inline bool mm_is_oom_victim(struct mm_struct *mm)
{
	return test_bit(MMF_OOM_VICTIM, &mm->flags);
}

/*
 * Checks whether a page fault on the given mm is still reliable.
 * This is no longer true if the oom reaper started to reap the
 * address space which is reflected by MMF_UNSTABLE flag set in
 * the mm. At that moment any !shared mapping would lose the content
 * and could cause a memory corruption (zero pages instead of the
 * original content).
 *
 * User should call this before establishing a page table entry for
 * a !shared mapping and under the proper page table lock.
 *
 * Return 0 when the PF is safe VM_FAULT_SIGBUS otherwise.
 */
static inline vm_fault_t check_stable_address_space(struct mm_struct *mm)
{
	if (unlikely(test_bit(MMF_UNSTABLE, &mm->flags)))
		return VM_FAULT_SIGBUS;
	return 0;
}

bool __oom_reap_task_mm(struct mm_struct *mm);

extern unsigned long oom_badness(struct task_struct *p,
		struct mem_cgroup *memcg, const nodemask_t *nodemask,
		unsigned long totalpages);

extern bool out_of_memory(struct oom_control *oc);

extern void exit_oom_victim(void);

extern int register_oom_notifier(struct notifier_block *nb);
extern int unregister_oom_notifier(struct notifier_block *nb);

extern bool oom_killer_disable(signed long timeout);
extern void oom_killer_enable(void);

extern struct task_struct *find_lock_task_mm(struct task_struct *p);

/* sysctls */
extern int sysctl_oom_dump_tasks;
extern int sysctl_oom_kill_allocating_task;
extern int sysctl_panic_on_oom;
#endif /* _INCLUDE_LINUX_OOM_H */
