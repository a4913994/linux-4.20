/*
 *  linux/mm/oom_kill.c
 * 
 *  Copyright (C)  1998,2000  Rik van Riel
 *	Thanks go out to Claus Fischer for some serious inspiration and
 *	for goading me into coding this file...
 *  Copyright (C)  2010  Google, Inc.
 *	Rewritten by David Rientjes
 *
 *  The routines in this file are used to kill a process when
 *  we're seriously out of memory. This gets called from __alloc_pages()
 *  in mm/page_alloc.c when we really run out of memory.
 *
 *  Since we won't call these routines often (on a well-configured
 *  machine) this file will double as a 'coding guide' and a signpost
 *  for newbie kernel hackers. It features several pointers to major
 *  kernel subsystems and hints as to where to find out what things do.
 */

#include <linux/oom.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/coredump.h>
#include <linux/sched/task.h>
#include <linux/swap.h>
#include <linux/timex.h>
#include <linux/jiffies.h>
#include <linux/cpuset.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/security.h>
#include <linux/ptrace.h>
#include <linux/freezer.h>
#include <linux/ftrace.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/mmu_notifier.h>

#include <asm/tlb.h>
#include "internal.h"
#include "slab.h"

#define CREATE_TRACE_POINTS
#include <trace/events/oom.h>

// sysctl_panic_on_oom：当设置为非零值时，内核在遇到OOM（内存耗尽）情况时会触发内核紧急（panic）。
// 默认情况下，该值为0，内核会选择一个进程杀死以释放内存，而不是触发紧急
int sysctl_panic_on_oom;
// sysctl_oom_kill_allocating_task：当设置为非零值时，内核在遇到OOM情况时会杀死当前正在分配内存的任务，
// 而不是选择一个最佳候选进程杀死。默认情况下，该值为0，内核会根据OOM评分选择一个进程杀死。
int sysctl_oom_kill_allocating_task;
// sysctl_oom_dump_tasks：当设置为非零值时，内核在遇到OOM情况时会在内核日志中输出所有任务的内存使用信息。
// 默认情况下，该值为1，内核会输出这些信息。
int sysctl_oom_dump_tasks = 1;

/*
 * Serializes oom killer invocations (out_of_memory()) from all contexts to
 * prevent from over eager oom killing (e.g. when the oom killer is invoked
 * from different domains).
 *
 * oom_killer_disable() relies on this lock to stabilize oom_killer_disabled
 * and mark_oom_victim
 * 
 * 对所有上下文中的OOM killer调用（out_of_memory()）进行串行化，以防止过于热衷的OOM killer执行（例如，当OOM killer从不同的域中调用时）
 * oom_killer_disable()依赖于此锁来稳定oom_killer_disabled和mark_oom_victim。
 */
DEFINE_MUTEX(oom_lock);

#ifdef CONFIG_NUMA
/**
 * has_intersects_mems_allowed() - check task eligiblity for kill
 * @start: task struct of which task to consider
 * @mask: nodemask passed to page allocator for mempolicy ooms
 *
 * Task eligibility is determined by whether or not a candidate task, @tsk,
 * shares the same mempolicy nodes as current if it is bound by such a policy
 * and whether or not it has the same set of allowed cpuset nodes.
 * 
 * has_intersects_mems_allowed() - 检查任务是否有资格被kill
 * @start: 要考虑的任务的任务结构体
 * @mask: 用于内存策略OOM的传递给页面分配器的节点掩码
 * 
 * 任务资格取决于候选任务 @tsk 是否与当前任务共享相同的内存策略节点（如果它被该策略绑定），
 * 以及它是否具有相同的允许cpuset节点集合。
 * 取决于两个条件
 * 1. 是否共享相同的内存策略节点
 * 2. 是否具有相同的允许cpuset节点集合
 */
static bool has_intersects_mems_allowed(struct task_struct *start, const nodemask_t *mask)
{
	struct task_struct *tsk;
	bool ret = false;

	rcu_read_lock();
	for_each_thread(start, tsk) {
		if (mask) {
			/*
			 * If this is a mempolicy constrained oom, tsk's
			 * cpuset is irrelevant.  Only return true if its
			 * mempolicy intersects current, otherwise it may be
			 * needlessly killed.
			 * 如果这是一个mempolicy限制的oom，tsk的cpuset无关紧要。只有在它的mempolicy与当前交叉时才返回true，否则可能会被不必要地杀死。
			 */
			ret = mempolicy_nodemask_intersects(tsk, mask);
		} else {
			/*
			 * This is not a mempolicy constrained oom, so only
			 * check the mems of tsk's cpuset.
			 * 这不是一个mempolicy受限的oom，因此只检查tsk的cpuset的内存。
			 */
			ret = cpuset_mems_allowed_intersects(current, tsk);
		}
		if (ret)
			break;
	}
	rcu_read_unlock();

	return ret;
}
#else
static bool has_intersects_mems_allowed(struct task_struct *tsk,
					const nodemask_t *mask)
{
	return true;
}
#endif /* CONFIG_NUMA */

/*
 * The process p may have detached its own ->mm while exiting or through
 * use_mm(), but one or more of its subthreads may still have a valid
 * pointer.  Return p, or any of its subthreads with a valid ->mm, with
 * task_lock() held.
 * 进程p可能在退出或通过use_mm()分离自己的->mm，但是其一个或多个子线程仍可能具有有效的指针。返回p或具有有效->mm的任何子线程，并保持task_lock()。
 */
struct task_struct *find_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t;

	rcu_read_lock();

	for_each_thread(p, t) {
		task_lock(t);
		if (likely(t->mm))
			goto found;
		task_unlock(t);
	}
	t = NULL;
found:
	rcu_read_unlock();

	return t;
}

/*
 * order == -1 means the oom kill is required by sysrq, otherwise only
 * for display purposes.
 * order == -1表示oom kill是sysrq所需的，否则仅用于显示目的
 */
static inline bool is_sysrq_oom(struct oom_control *oc)
{
	return oc->order == -1;
}

static inline bool is_memcg_oom(struct oom_control *oc)
{
	return oc->memcg != NULL;
}

/* return true if the task is not adequate as candidate victim task. */
// 如果任务不足以成为候选受害任务，则返回true
static bool oom_unkillable_task(struct task_struct *p,
		struct mem_cgroup *memcg, const nodemask_t *nodemask)
{
	if (is_global_init(p))
		return true;
	if (p->flags & PF_KTHREAD)
		return true;

	/* When mem_cgroup_out_of_memory() and p is not member of the group */
	if (memcg && !task_in_mem_cgroup(p, memcg))
		return true;

	/* p may not have freeable memory in nodemask */
	if (!has_intersects_mems_allowed(p, nodemask))
		return true;

	return false;
}

/*
 * Print out unreclaimble slabs info when unreclaimable slabs amount is greater
 * than all user memory (LRU pages)
 * 当不可回收的slab数量大于所有用户内存（LRU页）时，打印出不可回收的slab信息
 */
static bool is_dump_unreclaim_slabs(void)
{
	unsigned long nr_lru;

	nr_lru = global_node_page_state(NR_ACTIVE_ANON) +
		 global_node_page_state(NR_INACTIVE_ANON) +
		 global_node_page_state(NR_ACTIVE_FILE) +
		 global_node_page_state(NR_INACTIVE_FILE) +
		 global_node_page_state(NR_ISOLATED_ANON) +
		 global_node_page_state(NR_ISOLATED_FILE) +
		 global_node_page_state(NR_UNEVICTABLE);

	return (global_node_page_state(NR_SLAB_UNRECLAIMABLE) > nr_lru);
}

/**
 * oom_badness - heuristic function to determine which candidate task to kill
 * oom_badness - 启发式函数，用于确定要杀死哪个候选任务
 * 
 * @p: task struct of which task we should calculate // 用于计算的任务结构体
 * @totalpages: total present RAM allowed for page allocation // 用于页面分配的总现有RAM
 * @memcg: task's memory controller, if constrained // 任务的内存控制器，如果受限
 * @nodemask: nodemask passed to page allocator for mempolicy ooms // 传递给页分配器的节点掩码，用于内存策略oom
 *
 * The heuristic for determining which task to kill is made to be as simple and
 * predictable as possible.  The goal is to return the highest value for the
 * task consuming the most memory to avoid subsequent oom failures.
 * 确定要杀死哪个任务的启发式方法尽可能简单和可预测。目标是返回占用最多内存的任务的最高值，以避免后续的OOM故障。
 */
unsigned long oom_badness(struct task_struct *p, struct mem_cgroup *memcg,
			  const nodemask_t *nodemask, unsigned long totalpages)
{
	long points;
	long adj;

	if (oom_unkillable_task(p, memcg, nodemask))
		return 0;

	p = find_lock_task_mm(p);
	if (!p)
		return 0;

	/*
	 * Do not even consider tasks which are explicitly marked oom
	 * unkillable or have been already oom reaped or the are in
	 * the middle of vfork
	 * 不考虑已明确标记为 oom 不可杀或已经被 oom 收割或正在进行 vfork 的任务。
	 */
	adj = (long)p->signal->oom_score_adj;
	if (adj == OOM_SCORE_ADJ_MIN ||
			test_bit(MMF_OOM_SKIP, &p->mm->flags) ||
			in_vfork(p)) {
		task_unlock(p);
		return 0;
	}

	/*
	 * The baseline for the badness score is the proportion of RAM that each
	 * task's rss, pagetable and swap space use.
	 * badness分数的基准是每个任务的Resident Set Size（RSS），页表和交换空间使用的RAM比例
	 * 
	 * Resident Set Size（RSS）是一个进程使用的内存量的指标。它是指在物理内存中驻留的进程代码和数据的大小。这包括所有已映射的文件和共享库，但不包括交换空间中的数据。
	 */
	// 基础坏点数 = 进程占用的物理内存大小 + 使用的交换空间的大小 + 使用的页表占用的内存大小(因为每个页表项通常是一个 struct page 结构体，占用一个页面大小，所以需要将字节数转换为页面数)
	points = get_mm_rss(p->mm) + get_mm_counter(p->mm, MM_SWAPENTS) +
		mm_pgtables_bytes(p->mm) / PAGE_SIZE;
	task_unlock(p);

	/* Normalize to oom_score_adj units */
	// 标准化为 oom_score_adj 单位
	// adj代表根据进程的nice值计算出来的一个权重, 而totalpages则是系统可用的总内存页数
	// 根据进程的nice值，调整这个坏点数
	adj *= totalpages / 1000;
	points += adj;

	/*
	 * Never return 0 for an eligible task regardless of the root bonus and
	 * oom_score_adj (oom_score_adj can't be OOM_SCORE_ADJ_MIN here).
	 * 无论根节点的加分和oom_score_adj如何（oom_score_adj不能是OOM_SCORE_ADJ_MIN），对于符合条件的任务，永远不返回0。
	 */
	return points > 0 ? points : 1;
}

// oom_constraint，表示进程被 OOM Killer 杀掉时所受到的约束条件
enum oom_constraint {
	// CONSTRAINT_NONE 表示没有受到任何约束条件，进程被杀掉可能是由于系统内存不足导致
	CONSTRAINT_NONE,
	// CONSTRAINT_CPUSET 表示进程运行时被限制在一个指定的 CPU 集合中，如果进程被杀掉，可能是由于该 CPU 集合中的所有进程都在消耗系统资源，导致内存不足。
	CONSTRAINT_CPUSET,
	// CONSTRAINT_MEMORY_POLICY 表示进程运行时受到了指定的内存策略限制，如果进程被杀掉，可能是由于该内存策略下的所有进程都在消耗系统资源，导致内存不足。
	CONSTRAINT_MEMORY_POLICY,
	// CONSTRAINT_MEMCG 表示进程运行时被限制在一个指定的内存 cgroup 中，如果进程被杀掉，可能是由于该 cgroup 中的所有进程都在消耗系统资源，导致内存不足。
	CONSTRAINT_MEMCG,
};

/*
 * Determine the type of allocation constraint.
 * 确定分配约束的类型。
 */
static enum oom_constraint constrained_alloc(struct oom_control *oc)
{
	// zone: 一个存储区域的指针，表示当前进程正在尝试分配内存的存储区域。
	struct zone *zone;
	// z: 一个指向zone的zoneref结构体指针。
	struct zoneref *z;
	// high_zoneidx: 一个枚举类型的变量，表示当前进程正在尝试分配的内存所在的区域类型，如DMA区域、普通区域、高端区域等。
	enum zone_type high_zoneidx = gfp_zone(oc->gfp_mask);
	// cpuset_limited: 表示当前进程是否受到了cgroup的cpuset限制，即是否只能在特定的CPU集合中运行。
	bool cpuset_limited = false;
	int nid;

	// 如果当前的内存约束是在memory cgroup中发生的，那么mem_cgroup_get_max()函数会返回该cgroup中可用的最大内存总量，如果没有设置限制，返回1
	if (is_memcg_oom(oc)) {
		oc->totalpages = mem_cgroup_get_max(oc->memcg) ?: 1;
		return CONSTRAINT_MEMCG;
	}

	/* Default to all available memory */
	// 默认情况下，totalpages变量表示系统中所有可用的内存总量
	/*
   +----------------+              +----------------+              +----------------+              +----------------+
   |   Physical     |  Read/Write  |   Swap Space   |  Read/Write  | Virtual Memory |  Read/Write  |  Physical Memory|  Read/Write
   |    Devices     |------------->|                |------------->|                |------------->|                |
   +----------------+              +----------------+              +----------------+              +----------------+
	*/
	oc->totalpages = totalram_pages + total_swap_pages;

	if (!IS_ENABLED(CONFIG_NUMA))
		return CONSTRAINT_NONE;

	if (!oc->zonelist)
		return CONSTRAINT_NONE;
	/*
	 * Reach here only when __GFP_NOFAIL is used. So, we should avoid
	 * to kill current.We have to random task kill in this case.
	 * Hopefully, CONSTRAINT_THISNODE...but no way to handle it, now.
	 * 这里仅在使用__GFP_NOFAIL时才会执行到。因此，我们应该避免杀死当前进程。在这种情况下，我们必须随机杀死一个任务。希望，限制在本节点上...但现在没有办法处理。
	 */
	if (oc->gfp_mask & __GFP_THISNODE)
		return CONSTRAINT_NONE;

	/*
	 * This is not a __GFP_THISNODE allocation, so a truncated nodemask in
	 * the page allocator means a mempolicy is in effect.  Cpuset policy
	 * is enforced in get_page_from_freelist().
	 * 这不是一个__GFP_THISNODE分配，所以在页分配器中截断节点掩码意味着适用mempolicy。而cpuset策略则在get_page_from_freelist()中执行。
	 * 
	 * 如果进程设置了一个NUMA节点掩码（即oc->nodemask不为空），并且这些节点不是内存节点（即不在node_states[N_MEMORY]中），
	 * 那么oom控制器将总页数设置为交换分区的总页数，加上所有在该NUMA节点上的节点的跨度页面。
	 * 并返回约束条件为内存策略（CONSTRAINT_MEMORY_POLICY）。这里，numa节点的跨度页面是指节点上所有物理页面的总数。
	 */
	if (oc->nodemask &&
	    !nodes_subset(node_states[N_MEMORY], *oc->nodemask)) {
		oc->totalpages = total_swap_pages;
		for_each_node_mask(nid, *oc->nodemask)
			oc->totalpages += node_spanned_pages(nid);
		return CONSTRAINT_MEMORY_POLICY;
	}

	/* 
	Check this allocation failure is caused by cpuset's wall function 
	检查这个分配失败是由cpuset的wall函数引起的
	遍历zone链表，检查cpuset是否限制了gfp_mask标记的内存分配区域。如果当前遍历到的zone被cpuset限制了，则将cpuset_limited标志置为true
	*/
	for_each_zone_zonelist_nodemask(zone, z, oc->zonelist,
			high_zoneidx, oc->nodemask)
		if (!cpuset_zone_allowed(zone, oc->gfp_mask))
			cpuset_limited = true;

	// 如果cpuset限制了可用内存，则将总页面数设置为交换分区的总页数和cpuset限制的可用内存页数之和，
	// 并返回CONSTRAINT_CPUSET表示受到了cpuset限制。该函数中的其他限制类型可能不会起作用。
	if (cpuset_limited) {
		oc->totalpages = total_swap_pages;
		for_each_node_mask(nid, cpuset_current_mems_allowed)
			oc->totalpages += node_spanned_pages(nid);
		return CONSTRAINT_CPUSET;
	}
	return CONSTRAINT_NONE;
}

static int oom_evaluate_task(struct task_struct *task, void *arg)
{
	struct oom_control *oc = arg;
	unsigned long points;
	// 1. 检查当前进程是否是无法被杀死的进程。
	if (oom_unkillable_task(task, NULL, oc->nodemask))
		goto next;

	/*
	 * This task already has access to memory reserves and is being killed.
	 * Don't allow any other task to have access to the reserves unless
	 * the task has MMF_OOM_SKIP because chances that it would release
	 * any memory is quite low.
	 * 此任务已经可以访问内存保留并正在被终止。
	 * 不允许任何其他任务访问储备，除非
	 * 任务有 MMF_OOM_SKIP 因为它可能会释放
	 * 任何内存都非常低。
	 */
	// 检查当前进程是否已经可以访问内存储备并且正在被杀死，如果是并且没有设置 MMF_OOM_SKIP 标志，则终止搜索进程的操作。
	if (!is_sysrq_oom(oc) && tsk_is_oom_victim(task)) {
		if (test_bit(MMF_OOM_SKIP, &task->signal->oom_mm->flags))
			goto next;
		goto abort;
	}

	/*
	 * If task is allocating a lot of memory and has been marked to be
	 * killed first if it triggers an oom, then select it.
	 * 如果任务正在分配大量内存并且已被标记为
	 * 如果它触发了 oom，首先杀死它，然后选择它。
	 */
	// 用来判断一个进程是否具有OOM_ORIGIN标记的，如果有则表示该进程分配了大量内存且被标记为优先级更高的进程，在oom_badness中会被优先考虑
	if (oom_task_origin(task)) {
		points = ULONG_MAX;
		goto select;
	}
	// 根据OOM调度算法的badness值计算当前进程的points。
	points = oom_badness(task, NULL, oc->nodemask, oc->totalpages);
	if (!points || points < oc->chosen_points)
		goto next;

	/* Prefer thread group leaders for display purposes */
	// 出于显示目的优先选择线程组领导者
	if (points == oc->chosen_points && thread_group_leader(oc->chosen))
		goto next;
select:
	// 如果 oc->chosen 不为空，则说明已经记录了 OOM 任务，因此需要放弃旧的 OOM 任务，并释放其内存空间
	if (oc->chosen)
		put_task_struct(oc->chosen);
	// 用 get_task_struct 函数获取新选定的 OOM 任务，并将其保存在 oc->chosen 中
	get_task_struct(task);
	oc->chosen = task;
	// 将 points 的值保存在 oc->chosen_points 中，这将被用于后续选择 OOM 任务
	oc->chosen_points = points;
next:
	return 0;
abort:
	if (oc->chosen)
		put_task_struct(oc->chosen);
	oc->chosen = (void *)-1UL;
	return 1;
}

/*
 * Simple selection loop. We choose the process with the highest number of
 * 'points'. In case scan was aborted, oc->chosen is set to -1.
 * 简单的选择循环。我们选择数量最多的进程
 *“点”。如果扫描中止，oc->chosen 设置为 -1。
 */
static void select_bad_process(struct oom_control *oc)
{
	// 首先判断当前是否在 memcg 中发生 OOM，如果是则通过 mem_cgroup_scan_tasks 函数扫描该 memcg 内的所有进程，
	// 并通过 oom_evaluate_task 函数评估每个进程的 badness 值，最终选出需要杀掉的进程。
	if (is_memcg_oom(oc))
		mem_cgroup_scan_tasks(oc->memcg, oom_evaluate_task, oc);
	else {
		// 否则通过 for_each_process 循环遍历所有进程，对于每个进程，调用 oom_evaluate_task 函数评估其 badness 值，最终选出需要杀掉的进程
		struct task_struct *p;

		rcu_read_lock();
		for_each_process(p)
			if (oom_evaluate_task(p, oc))
				break;
		rcu_read_unlock();
	}
	// 将选中的进程的 badness 值乘以 1000 并除以 totalpages，得到其选中概率，保存在 oc->chosen_points 中。
	oc->chosen_points = oc->chosen_points * 1000 / oc->totalpages;
}

/**
 * dump_tasks - dump current memory state of all system tasks
 * 输出所有符合条件的任务的内存状态信息，包括进程ID、用户ID、虚拟内存大小、驻留内存大小、页表大小、交换空间使用情况、oom_score_adj值和进程名等。
 * @memcg: current's memory controller, if constrained
 * @memcg：当前进程的内存控制器，如果有的话；
 * @nodemask: nodemask passed to page allocator for mempolicy ooms
 * @nodemask：传递给页面分配器用于mempolicy ooms的节点掩码。
 *
 * Dumps the current memory state of all eligible tasks.  Tasks not in the same
 * memcg, not in the same cpuset, or bound to a disjoint set of mempolicy nodes
 * are not shown.
 * State information includes task's pid, uid, tgid, vm size, rss,
 * pgtables_bytes, swapents, oom_score_adj value, and name.
 */
static void dump_tasks(struct mem_cgroup *memcg, const nodemask_t *nodemask)
{
	struct task_struct *p;
	struct task_struct *task;

	pr_info("Tasks state (memory values in pages):\n");
	pr_info("[  pid  ]   uid  tgid total_vm      rss pgtables_bytes swapents oom_score_adj name\n");
	rcu_read_lock();
	for_each_process(p) {
		if (oom_unkillable_task(p, memcg, nodemask))
			continue;

		task = find_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 * 这是一个kthread或者p的所有线程已经分离了它们的mm。没有必要报告它们；它们无法被oom kill。
			 */
			continue;
		}

		pr_info("[%7d] %5d %5d %8lu %8lu %8ld %8lu         %5hd %s\n",
			task->pid, from_kuid(&init_user_ns, task_uid(task)),
			task->tgid, task->mm->total_vm, get_mm_rss(task->mm),
			mm_pgtables_bytes(task->mm),
			get_mm_counter(task->mm, MM_SWAPENTS),
			task->signal->oom_score_adj, task->comm);
		task_unlock(task);
	}
	rcu_read_unlock();
}

static void dump_header(struct oom_control *oc, struct task_struct *p)
{
	// 1. 打印出当前进程的一些信息和系统的一些配置情况，包括进程名称，gfp_mask、nodemask、order、oom_score_adj等
	pr_warn("%s invoked oom-killer: gfp_mask=%#x(%pGg), nodemask=%*pbl, order=%d, oom_score_adj=%hd\n",
		current->comm, oc->gfp_mask, &oc->gfp_mask,
		nodemask_pr_args(oc->nodemask), oc->order,
			current->signal->oom_score_adj);
	// 如果系统未启用压缩算法（COMPACTION）并且分配的内存超出系统可用内存，则打印出警告信息。
	if (!IS_ENABLED(CONFIG_COMPACTION) && oc->order)
		pr_warn("COMPACTION is disabled!!!\n");

	// 打印出当前进程可以使用的内存节点信息。
	cpuset_print_current_mems_allowed();
	// 打印出当前调用栈信息
	dump_stack();
	// 如果该OOM事件发生在一个内存控制组（memcg）中，则打印出该组的相关信息。
	if (is_memcg_oom(oc))
		mem_cgroup_print_oom_info(oc->memcg, p);
	else {
		// 否则，打印出系统各节点的内存使用情况，如果系统中有不可回收的slab，则打印出相关信息。
		show_mem(SHOW_MEM_FILTER_NODES, oc->nodemask);
		if (is_dump_unreclaim_slabs())
			dump_unreclaimable_slab();
	}
	// 如果sysctl_oom_dump_tasks设置为1，则打印出所有进程的信息。
	if (sysctl_oom_dump_tasks)
		dump_tasks(oc->memcg, oc->nodemask);
}

/*
 * Number of OOM victims in flight
 */
// oom_victims：用于记录当前有多少个任务被 OOM killer 杀掉，是一个原子变量。
static atomic_t oom_victims = ATOMIC_INIT(0);
// oom_victims_wait：一个等待队列，用于在 OOM victims 达到一定数量时阻塞其他任务的分配请求。
static DECLARE_WAIT_QUEUE_HEAD(oom_victims_wait);

// oom_killer_disabled：用于标记 OOM killer 是否被禁用。
static bool oom_killer_disabled __read_mostly;

// K(x)：一个宏，用于将 x KB 转换为页数。
#define K(x) ((x) << (PAGE_SHIFT-10))

/*
 * task->mm can be NULL if the task is the exited group leader.  So to
 * determine whether the task is using a particular mm, we examine all the
 * task's threads: if one of those is using this mm then this task was also
 * using it.
 * 如果一个任务是已经退出的组长（group leader），那么它的task->mm可以为NULL。
 * 因此，为了确定这个任务是否使用了一个特定的mm，我们需要检查所有该任务的线程：如果其中一个线程正在使用该mm，则该任务也正在使用它。
 */
bool process_shares_mm(struct task_struct *p, struct mm_struct *mm)
{
	struct task_struct *t;

	for_each_thread(p, t) {
		struct mm_struct *t_mm = READ_ONCE(t->mm);
		if (t_mm)
			return t_mm == mm;
	}
	return false;
}

#ifdef CONFIG_MMU
/*
 * OOM Reaper kernel thread which tries to reap the memory used by the OOM
 * victim (if that is possible) to help the OOM killer to move on.
 * OOM Reaper内核线程，它试图回收OOM受害者使用的内存（如果可能的话），以帮助OOM杀手继续前进。
 */
// oom_reaper_th：oom reaper 线程，用于等待和收集被 oom killer 杀死的进程，并释放它们占用的资源。
static struct task_struct *oom_reaper_th;
// oom_reaper_wait：oom reaper 的等待队列，当没有待处理的 oom victim 时，oom reaper 线程会进入该等待队列休眠。
static DECLARE_WAIT_QUEUE_HEAD(oom_reaper_wait);
// oom_reaper_list：保存被 oom killer 杀死的进程的链表。当 oom reaper 线程被唤醒时，会处理该链表上的所有进程。
static struct task_struct *oom_reaper_list;
// oom_reaper_lock：用于保护 oom_reaper_list 和 oom_reaper_th 变量的自旋锁。
static DEFINE_SPINLOCK(oom_reaper_lock);

bool __oom_reap_task_mm(struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	bool ret = true;

	/*
	 * Tell all users of get_user/copy_from_user etc... that the content
	 * is no longer stable. No barriers really needed because unmapping
	 * should imply barriers already and the reader would hit a page fault
	 * if it stumbled over a reaped memory.
	 * 告诉所有使用get_user/copy_from_user等函数的用户，该内容不再稳定。实际上不需要障碍，
	 * 因为解除映射应该已经隐含了障碍，如果读者遇到了被回收的内存，它会产生页面错误。
	 */
	// 1. 将进程的内存标记为不稳定状态（set_bit(MMF_UNSTABLE, &mm->flags)）。
	set_bit(MMF_UNSTABLE, &mm->flags);

	// 2. 遍历进程的所有虚拟内存区间（VMA），对于每一个VMA：
	for (vma = mm->mmap ; vma; vma = vma->vm_next) {
		// 如果该VMA是非匿名页（即映射到文件系统中），或是共享页，则跳过。
		if (!can_madv_dontneed_vma(vma))
			continue;

		/*
		 * Only anonymous pages have a good chance to be dropped
		 * without additional steps which we cannot afford as we
		 * are OOM already.
		 *
		 * We do not even care about fs backed pages because all
		 * which are reclaimable have already been reclaimed and
		 * we do not want to block exit_mmap by keeping mm ref
		 * count elevated without a good reason.
		 * 只有匿名页才有很好的机会在不需要额外步骤的情况下被丢弃，
		 * 因为我们已经陷入了OOM，无法承受额外的步骤。
		 * 我们甚至不关心文件系统支持的页，因为所有可回收的页都已经被回收，
		 * 我们不想在没有好的理由的情况下通过保持mm引用计数的升高来阻塞exit_mmap。
		 */
		// 3. 如果该VMA是匿名页，则继续执行以下步骤：
		// 3.1 获取该VMA对应的页表项。
		// 3.2 利用页表项中的信息，调用mmu_notifier_invalidate_range_start_nonblock()函数通知内核无需阻塞地开始进行页表项的更新。
		// 3.3 利用unmap_page_range()函数删除该VMA对应的物理内存映射关系，并在此过程中，将相应的页表项中的标志位置为未映射。
		// 3.4 调用mmu_notifier_invalidate_range_end()函数通知内核更新已经完成。
		// 3.5 最后，调用tlb_finish_mmu()函数刷新页表缓存。
		if (vma_is_anonymous(vma) || !(vma->vm_flags & VM_SHARED)) {
			const unsigned long start = vma->vm_start;
			const unsigned long end = vma->vm_end;
			struct mmu_gather tlb;

			tlb_gather_mmu(&tlb, mm, start, end);
			if (mmu_notifier_invalidate_range_start_nonblock(mm, start, end)) {
				tlb_finish_mmu(&tlb, start, end);
				ret = false;
				continue;
			}
			unmap_page_range(&tlb, vma, start, end, NULL);
			mmu_notifier_invalidate_range_end(mm, start, end);
			tlb_finish_mmu(&tlb, start, end);
		}
	}

	return ret;
}

/*
 * Reaps the address space of the give task.
 *
 * Returns true on success and false if none or part of the address space
 * has been reclaimed and the caller should retry later.
 * 释放给定任务的地址空间。
 * 如果成功释放所有地址空间，则返回true，否则返回false，表示没有或只有部分地址空间被回收，并且调用者应稍后重试。
 */
static bool oom_reap_task_mm(struct task_struct *tsk, struct mm_struct *mm)
{
	bool ret = true;

	// 获取mm->mmap_sem读取信号量，如果没有获取到读取信号量，则直接返回false。
	if (!down_read_trylock(&mm->mmap_sem)) {
		trace_skip_task_reaping(tsk->pid);
		return false;
	}

	/*
	 * MMF_OOM_SKIP is set by exit_mmap when the OOM reaper can't
	 * work on the mm anymore. The check for MMF_OOM_SKIP must run
	 * under mmap_sem for reading because it serializes against the
	 * down_write();up_write() cycle in exit_mmap().
	 * MMF_OOM_SKIP在OOM reaper无法继续对mm进行操作时由exit_mmap设置。
	 * MMF_OOM_SKIP的检查必须在mmap_sem下进行读取，因为它与exit_mmap()中的down_write(); up_write()周期进行序列化。
	 */
	// 检查标志位MMF_OOM_SKIP是否被设置，如果被设置，则跳转到out_unlock
	if (test_bit(MMF_OOM_SKIP, &mm->flags)) {
		trace_skip_task_reaping(tsk->pid);
		goto out_unlock;
	}
	// 回收任务的地址空间，如果返回失败，则跳转到out_finish。
	trace_start_task_reaping(tsk->pid);

	/* failed to reap part of the address space. Try again later */
	ret = __oom_reap_task_mm(mm);
	if (!ret)
		goto out_finish;

	// 打印当前被回收的进程的信息
	pr_info("oom_reaper: reaped process %d (%s), now anon-rss:%lukB, file-rss:%lukB, shmem-rss:%lukB\n",
			task_pid_nr(tsk), tsk->comm,
			K(get_mm_counter(mm, MM_ANONPAGES)),
			K(get_mm_counter(mm, MM_FILEPAGES)),
			K(get_mm_counter(mm, MM_SHMEMPAGES)));
out_finish:
	trace_finish_task_reaping(tsk->pid);
out_unlock:
	// 释放mm->mmap_sem读取信号量
	up_read(&mm->mmap_sem);

	return ret;
}

#define MAX_OOM_REAP_RETRIES 10
// 回收被杀死的进程的地址空间，将已被回收的内存返回给系统。
static void oom_reap_task(struct task_struct *tsk)
{
	int attempts = 0;
	struct mm_struct *mm = tsk->signal->oom_mm;

	/* Retry the down_read_trylock(mmap_sem) a few times */
	// 尝试获取mm_struct的mmap_sem信号量，由于多个线程可能同时操作同一个mm_struct结构体，因此需要对mmap_sem信号量进行同步。
	// oom_reap_task_mm对地址空间中可以被回收的内存进行回收，如果无法对部分地址空间进行回收，
	// 则需要重新尝试获取mmap_sem信号量。MAX_OOM_REAP_RETRIES常量定义了重新尝试获取mmap_sem信号量的最大次数
	while (attempts++ < MAX_OOM_REAP_RETRIES && !oom_reap_task_mm(tsk, mm))
		schedule_timeout_idle(HZ/10);

	// 如果尝试获取mmap_sem信号量的次数超过了最大次数，或者MMF_OOM_SKIP标志位已经被设置，就跳过循环。此时进程的oom_reaper_list指针被置为NULL。
	if (attempts <= MAX_OOM_REAP_RETRIES ||
	    test_bit(MMF_OOM_SKIP, &mm->flags))
		goto done;

	pr_info("oom_reaper: unable to reap pid:%d (%s)\n",
		task_pid_nr(tsk), tsk->comm);
	debug_show_all_locks();

done:
	// 通过调用put_task_struct函数减少该进程的引用计数，以便其他代码可以正确地回收该进程的资源。
	// set_bit函数将MMF_OOM_SKIP标志位置为1，以隐藏该进程的mm_struct结构体，避免OOM killer对其进行杀死, 因为它已经被收割或有人不能调用 up_write(mmap_sem)。
	tsk->oom_reaper_list = NULL;

	/*
	 * Hide this mm from OOM killer because it has been either reaped or
	 * somebody can't call up_write(mmap_sem).
	 */
	set_bit(MMF_OOM_SKIP, &mm->flags);

	/* Drop a reference taken by wake_oom_reaper */
	put_task_struct(tsk);
}

static int oom_reaper(void *unused)
{
	// 循环等待直到有OOM任务需要被处理
	while (true) {
		struct task_struct *tsk = NULL;

		wait_event_freezable(oom_reaper_wait, oom_reaper_list != NULL);
		spin_lock(&oom_reaper_lock);
		if (oom_reaper_list != NULL) {
			tsk = oom_reaper_list;
			oom_reaper_list = tsk->oom_reaper_list;
		}
		spin_unlock(&oom_reaper_lock);

		if (tsk)
			oom_reap_task(tsk);
	}

	return 0;
}

// wake_oom_reaper是用来唤醒 OOM reaper 线程，并把需要回收内存的进程加入到回收队列中
static void wake_oom_reaper(struct task_struct *tsk)
{
	/* tsk is already queued? */
	// 1. 判断参数 tsk 是否已经加入到回收队列中，如果已经加入，直接返回。
	if (tsk == oom_reaper_list || tsk->oom_reaper_list)
		return;
	// 对进程 tsk 执行引用计数加一，使其引用计数增加 1。
	get_task_struct(tsk);

	spin_lock(&oom_reaper_lock);
	tsk->oom_reaper_list = oom_reaper_list;
	oom_reaper_list = tsk;
	spin_unlock(&oom_reaper_lock);
	trace_wake_reaper(tsk->pid);
	// 唤醒 oom_reaper_wait 等待队列中的进程。
	wake_up(&oom_reaper_wait);
}

static int __init oom_init(void)
{
	oom_reaper_th = kthread_run(oom_reaper, NULL, "oom_reaper");
	return 0;
}
subsys_initcall(oom_init)
#else
static inline void wake_oom_reaper(struct task_struct *tsk)
{
}
#endif /* CONFIG_MMU */

/**
 * mark_oom_victim - mark the given task as OOM victim
 * @tsk: task to mark
 *
 * Has to be called with oom_lock held and never after
 * oom has been disabled already.
 *
 * tsk->mm has to be non NULL and caller has to guarantee it is stable (either
 * under task_lock or operate on the current).
 * MMF_OOM_SKIP在OOM reaper无法继续对mm进行操作时由exit_mmap设置。
 * MMF_OOM_SKIP的检查必须在mmap_sem下进行读取，因为它与exit_mmap()中的down_write(); up_write()周期进行序列化。
 */
static void mark_oom_victim(struct task_struct *tsk)
{
	struct mm_struct *mm = tsk->mm;

	// 首先会检查 oom_killer_disabled 标志，如果已经被设置则直接返回；
	WARN_ON(oom_killer_disabled);
	/* OOM killer might race with memcg OOM */
	//  test_and_set_tsk_thread_flag 函数将 TIF_MEMDIE 标志设置为当前任务，表示该任务被标记为 OOM victim。该函数会将原先的标志返回，如果原先标志已经被设置，则直接返回，否则继续执行；
	if (test_and_set_tsk_thread_flag(tsk, TIF_MEMDIE))
		return;

	/* oom_mm is bound to the signal struct life time. */
	// 检查该任务的 oom_mm 是否已经与信号结构体绑定，如果没有，则将其绑定并将 MMF_OOM_VICTIM 标志设置为 mm->flags，表示该任务的 mm 结构体被标记为 OOM victim；
	if (!cmpxchg(&tsk->signal->oom_mm, NULL, mm)) {
		mmgrab(tsk->signal->oom_mm);
		set_bit(MMF_OOM_VICTIM, &mm->flags);
	}

	/*
	 * Make sure that the task is woken up from uninterruptible sleep
	 * if it is frozen because OOM killer wouldn't be able to free
	 * any memory and livelock. freezing_slow_path will tell the freezer
	 * that TIF_MEMDIE tasks should be ignored.
	 * 确保从不可中断的睡眠状态中唤醒任务，如果任务因为OOM杀手无法释放内存而被冻结，可能会导致死锁。freezing_slow_path将告诉冷冻器忽略TIF_MEMDIE任务。
	 */
	__thaw_task(tsk);
	// 在原子变量 oom_victims 上增加计数器；
	atomic_inc(&oom_victims);
	// 调用 __thaw_task(tsk) 函数，如果任务处于 uninterruptible sleep 状态则唤醒它。
	trace_mark_victim(tsk->pid);
}

/**
 * exit_oom_victim - note the exit of an OOM victim
 */
void exit_oom_victim(void)
{
	clear_thread_flag(TIF_MEMDIE);

	if (!atomic_dec_return(&oom_victims))
		wake_up_all(&oom_victims_wait);
}

/**
 * oom_killer_enable - enable OOM killer
 */
void oom_killer_enable(void)
{
	oom_killer_disabled = false;
	pr_info("OOM killer enabled.\n");
}

/**
 * oom_killer_disable - disable OOM killer
 * @timeout: maximum timeout to wait for oom victims in jiffies
 *
 * Forces all page allocations to fail rather than trigger OOM killer.
 * Will block and wait until all OOM victims are killed or the given
 * timeout expires.
 *
 * The function cannot be called when there are runnable user tasks because
 * the userspace would see unexpected allocation failures as a result. Any
 * new usage of this function should be consulted with MM people.
 *
 * Returns true if successful and false if the OOM killer cannot be
 * disabled.
 * oom_killer_disable - 禁用OOM killer
 * @timeout：在jiffies中等待oom受害者的最长超时时间
 * 强制所有页面分配失败，而不是触发OOM killer。将阻塞并等待，直到所有OOM受害者被杀死或给定的超时时间到期。
 * 当存在可运行的用户任务时，无法调用该函数，因为用户空间将会出现意外的分配失败。任何对该函数的新使用都应与MM人员协商。
 * 如果成功，则返回true；如果无法禁用OOM killer，则返回false。
 */
bool oom_killer_disable(signed long timeout)
{
	signed long ret;

	/*
	 * Make sure to not race with an ongoing OOM killer. Check that the
	 * current is not killed (possibly due to sharing the victim's memory).
	 * 确保不与正在进行的OOM killer竞争。检查当前进程是否已被杀死（可能由于共享受害者的内存）。
	 */
	if (mutex_lock_killable(&oom_lock))
		return false;
	oom_killer_disabled = true;
	mutex_unlock(&oom_lock);

	ret = wait_event_interruptible_timeout(oom_victims_wait,
			!atomic_read(&oom_victims), timeout);
	if (ret <= 0) {
		oom_killer_enable();
		return false;
	}
	pr_info("OOM killer disabled.\n");

	return true;
}

// __task_will_free_mem，用于判断一个进程是否会释放内存
static inline bool __task_will_free_mem(struct task_struct *task)
{
	// 获取进程的信号结构体sig
	struct signal_struct *sig = task->signal;

	/*
	 * A coredumping process may sleep for an extended period in exit_mm(),
	 * so the oom killer cannot assume that the process will promptly exit
	 * and release memory.
	 * 核心转储进程在exit_mm()中可能会睡眠一段时间，因此OOM killer不能假定该进程会立即退出并释放内存。
	 */
	// 如果该进程是一个核心转储进程（SIGNAL_GROUP_COREDUMP标志位被设置），则认为它不会及时退出并释放内存，返回false
	if (sig->flags & SIGNAL_GROUP_COREDUMP)
		return false;

	// 如果该进程的信号组已经退出（SIGNAL_GROUP_EXIT标志位被设置），则认为它会释放内存，返回true
	if (sig->flags & SIGNAL_GROUP_EXIT)
		return true;

	// 如果该进程是一个线程组的最后一个线程（通过thread_group_empty函数判断），并且PF_EXITING标志位被设置，则认为它会释放内存，返回true
	if (thread_group_empty(task) && (task->flags & PF_EXITING))
		return true;

	return false;
}

/*
 * Checks whether the given task is dying or exiting and likely to
 * release its address space. This means that all threads and processes
 * sharing the same mm have to be killed or exiting.
 * Caller has to make sure that task->mm is stable (hold task_lock or
 * it operates on the current).
 * 检查给定的任务是否正在死亡或退出，并可能释放其地址空间。这意味着所有共享相同mm的线程和进程都必须被杀死或退出。
 * 调用方必须确保task->mm是稳定的（持有task_lock或者它在操作当前进程）。
 */
static bool task_will_free_mem(struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	struct task_struct *p;
	bool ret = true;

	/*
	 * Skip tasks without mm because it might have passed its exit_mm and
	 * exit_oom_victim. oom_reaper could have rescued that but do not rely
	 * on that for now. We can consider find_lock_task_mm in future.
	 * 跳过没有mm的任务，因为它可能已经通过了exit_mm和exit_oom_victim。oom_reaper可以解救它，
	 * 但现在不要依赖它。我们可以在未来考虑使用find_lock_task_mm。
	 */
	if (!mm)
		return false;

	if (!__task_will_free_mem(task))
		return false;

	/*
	 * This task has already been drained by the oom reaper so there are
	 * only small chances it will free some more
	 * 这个任务已经被oom reaper耗尽了，所以只有很小的机会它会释放更多的内存。
	 */
	if (test_bit(MMF_OOM_SKIP, &mm->flags))
		return false;

	if (atomic_read(&mm->mm_users) <= 1)
		return true;

	/*
	 * Make sure that all tasks which share the mm with the given tasks
	 * are dying as well to make sure that a) nobody pins its mm and
	 * b) the task is also reapable by the oom reaper.
	 * 确保所有共享与给定任务相同的mm的任务也正在死亡，以确保
	 * a）没有人固定它的mm，
	 * b）oom reaper也可以收割该任务
	 */
	rcu_read_lock();
	for_each_process(p) {
		if (!process_shares_mm(p, mm))
			continue;
		if (same_thread_group(task, p))
			continue;
		ret = __task_will_free_mem(p);
		if (!ret)
			break;
	}
	rcu_read_unlock();

	return ret;
}

static void __oom_kill_process(struct task_struct *victim)
{
	struct task_struct *p;
	struct mm_struct *mm;
	bool can_oom_reap = true;

	p = find_lock_task_mm(victim);
	if (!p) {
		put_task_struct(victim);
		return;
	} else if (victim != p) {
		get_task_struct(p);
		put_task_struct(victim);
		victim = p;
	}

	/* Get a reference to safely compare mm after task_unlock(victim) */
	//  获取一个引用，以便在task_unlock(victim)之后安全比较mm
	mm = victim->mm;
	mmgrab(mm);

	/* Raise event before sending signal: task reaper must see this */
	// 发送信号之前触发事件：任务回收器必须看到这个事件。
	count_vm_event(OOM_KILL);
	memcg_memory_event_mm(mm, MEMCG_OOM_KILL);

	/*
	 * We should send SIGKILL before granting access to memory reserves
	 * in order to prevent the OOM victim from depleting the memory
	 * reserves from the user space under its control.
	 */
	// 在授予对内存保留的访问权限之前，我们应该发送SIGKILL，以防止OOM受害者耗尽其控制下的用户空间的内存保留。
	do_send_sig_info(SIGKILL, SEND_SIG_PRIV, victim, PIDTYPE_TGID);
	mark_oom_victim(victim);
	pr_err("Killed process %d (%s) total-vm:%lukB, anon-rss:%lukB, file-rss:%lukB, shmem-rss:%lukB\n",
		task_pid_nr(victim), victim->comm, K(victim->mm->total_vm),
		K(get_mm_counter(victim->mm, MM_ANONPAGES)),
		K(get_mm_counter(victim->mm, MM_FILEPAGES)),
		K(get_mm_counter(victim->mm, MM_SHMEMPAGES)));
	task_unlock(victim);

	/*
	 * Kill all user processes sharing victim->mm in other thread groups, if
	 * any.  They don't get access to memory reserves, though, to avoid
	 * depletion of all memory.  This prevents mm->mmap_sem livelock when an
	 * oom killed thread cannot exit because it requires the semaphore and
	 * its contended by another thread trying to allocate memory itself.
	 * That thread will now get access to memory reserves since it has a
	 * pending fatal signal.
	 * 杀死所有共享victim->mm的其他线程组中的用户进程（如果有）。虽然它们不能访问内存保留，但是为了避免耗尽所有内存，
	 * 不能给它们访问权限。这可以防止当一个oom killed线程无法退出时，mm->mmap_sem livelock发生，因为它需要该信号量，
	 * 并且被另一个试图分配内存的线程争用。由于该线程有一个待处理的致命信号，因此现在将获得对内存保留的访问权限。
	 */
	rcu_read_lock();
	for_each_process(p) {
		if (!process_shares_mm(p, mm))
			continue;
		if (same_thread_group(p, victim))
			continue;
		if (is_global_init(p)) {
			can_oom_reap = false;
			set_bit(MMF_OOM_SKIP, &mm->flags);
			pr_info("oom killer %d (%s) has mm pinned by %d (%s)\n",
					task_pid_nr(victim), victim->comm,
					task_pid_nr(p), p->comm);
			continue;
		}
		/*
		 * No use_mm() user needs to read from the userspace so we are
		 * ok to reap it.
		 */
		if (unlikely(p->flags & PF_KTHREAD))
			continue;
		do_send_sig_info(SIGKILL, SEND_SIG_PRIV, p, PIDTYPE_TGID);
	}
	rcu_read_unlock();

	if (can_oom_reap)
		wake_oom_reaper(victim);

	mmdrop(mm);
	put_task_struct(victim);
}
#undef K

/*
 * Kill provided task unless it's secured by setting
 * oom_score_adj to OOM_SCORE_ADJ_MIN.
 * 杀死提供的任务，除非它通过设置oom_score_adj为OOM_SCORE_ADJ_MIN来受到保护。
 */
static int oom_kill_memcg_member(struct task_struct *task, void *unused)
{
	if (task->signal->oom_score_adj != OOM_SCORE_ADJ_MIN) {
		get_task_struct(task);
		__oom_kill_process(task);
	}
	return 0;
}

static void oom_kill_process(struct oom_control *oc, const char *message)
{
	struct task_struct *p = oc->chosen;
	unsigned int points = oc->chosen_points;
	struct task_struct *victim = p;
	struct task_struct *child;
	struct task_struct *t;
	struct mem_cgroup *oom_group;
	unsigned int victim_points = 0;
	// 定义了一个名为 oom_rs 的限速器，使用了 DEFINE_RATELIMIT_STATE 宏。该宏可以在内核代码中方便地定义限速器，可以限制一个事件在一定时间内发生的次数，以避免过多占用系统资源。
	static DEFINE_RATELIMIT_STATE(oom_rs, DEFAULT_RATELIMIT_INTERVAL,
					      DEFAULT_RATELIMIT_BURST);

	/*
	 * If the task is already exiting, don't alarm the sysadmin or kill
	 * its children or threads, just give it access to memory reserves
	 * so it can die quickly
	 * 如果任务已经退出，不要警告系统管理员或杀死其子进程或线程，只需授予其访问内存保留的权限，以便它可以快速死亡
	 */
	task_lock(p);
	if (task_will_free_mem(p)) {
		mark_oom_victim(p);
		wake_oom_reaper(p);
		task_unlock(p);
		put_task_struct(p);
		return;
	}
	task_unlock(p);

	if (__ratelimit(&oom_rs))
		dump_header(oc, p);

	pr_err("%s: Kill process %d (%s) score %u or sacrifice child\n",
		message, task_pid_nr(p), p->comm, points);

	/*
	 * If any of p's children has a different mm and is eligible for kill,
	 * the one with the highest oom_badness() score is sacrificed for its
	 * parent.  This attempts to lose the minimal amount of work done while
	 * still freeing memory.
	 * 如果p的任何一个子进程具有不同的mm并且有资格被杀死，具有最高oom_badness()得分的进程将被牺牲为其父进程。这试图在仍然释放内存的同时最小化完成的工作量。
	 */
	read_lock(&tasklist_lock);
	for_each_thread(p, t) {
		list_for_each_entry(child, &t->children, sibling) {
			unsigned int child_points;

			if (process_shares_mm(child, p->mm))
				continue;
			/*
			 * oom_badness() returns 0 if the thread is unkillable
			 */
			child_points = oom_badness(child,
				oc->memcg, oc->nodemask, oc->totalpages);
			if (child_points > victim_points) {
				put_task_struct(victim);
				victim = child;
				victim_points = child_points;
				get_task_struct(victim);
			}
		}
	}
	read_unlock(&tasklist_lock);

	/*
	 * Do we need to kill the entire memory cgroup?
	 * Or even one of the ancestor memory cgroups?
	 * Check this out before killing the victim task.
	 * 我们需要杀死整个内存cgroup吗？
	 * 或者需要杀死一个祖先内存cgroup吗？
	 * 在杀死受害者任务之前，请先进行检查。
	 */
	oom_group = mem_cgroup_get_oom_group(victim, oc->memcg);

	__oom_kill_process(victim);

	/*
	 * If necessary, kill all tasks in the selected memory cgroup.
	 如果必要，杀死所选内存cgroup中的所有任务。
	 */
	if (oom_group) {
		mem_cgroup_print_oom_group(oom_group);
		mem_cgroup_scan_tasks(oom_group, oom_kill_memcg_member, NULL);
		mem_cgroup_put(oom_group);
	}
}

/*
 * Determines whether the kernel must panic because of the panic_on_oom sysctl.
 确定内核是否必须因为panic_on_oom sysctl而导致内核崩溃。
 */
static void check_panic_on_oom(struct oom_control *oc,
			       enum oom_constraint constraint)
{
	// 当系统的 panic_on_oom 配置开启时，检查是否需要在出现内存不足时引发 kernel panic。函
	if (likely(!sysctl_panic_on_oom))
		return;
	if (sysctl_panic_on_oom != 2) {
		/*
		 * panic_on_oom == 1 only affects CONSTRAINT_NONE, the kernel
		 * does not panic for cpuset, mempolicy, or memcg allocation
		 * failures.
		 * panic_on_oom == 1 仅影响 CONSTRAINT_NONE，内核不会因为cpuset、mempolicy或memcg分配失败而导致内核崩溃。
		 */
		if (constraint != CONSTRAINT_NONE)
			return;
	}
	/* Do not panic for oom kills triggered by sysrq */
	// 不要因为由sysrq触发的oom kills而导致内核崩
	if (is_sysrq_oom(oc))
		return;
	dump_header(oc, NULL);
	panic("Out of memory: %s panic_on_oom is enabled\n",
		sysctl_panic_on_oom == 2 ? "compulsory" : "system-wide");
}

static BLOCKING_NOTIFIER_HEAD(oom_notify_list);

int register_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_oom_notifier);

int unregister_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_oom_notifier);

/**
 * out_of_memory - kill the "best" process when we run out of memory
 * @oc: pointer to struct oom_control
 *
 * If we run out of memory, we have the choice between either
 * killing a random task (bad), letting the system crash (worse)
 * OR try to be smart about which process to kill. Note that we
 * don't have to be perfect here, we just have to be good.
 * out_of_memory - 在内存耗尽时杀死“最佳”进程
 * @oc: 指向struct oom_cA
 * 如果我们的内存耗尽了，我们可以选择杀死一个随机的任务（不好），让系统崩溃（更糟），或者尝试智能选择要杀死的进程。
 * 请注意，我们不必在这里做得完美，我们只需要做得好就可以了。ontrol的指针
 */
bool out_of_memory(struct oom_control *oc)
{
	unsigned long freed = 0;
	enum oom_constraint constraint = CONSTRAINT_NONE;

	if (oom_killer_disabled)
		return false;

	if (!is_memcg_oom(oc)) {
		blocking_notifier_call_chain(&oom_notify_list, 0, &freed);
		if (freed > 0)
			/* Got some memory back in the last second. */
			return true;
	}

	/*
	 * If current has a pending SIGKILL or is exiting, then automatically
	 * select it.  The goal is to allow it to allocate so that it may
	 * quickly exit and free its memory.
	 * 如果当前进程有一个待处理的SIGKILL或正在退出，则自动选择它。目的是允许它分配内存，以便它可以快速退出并释放其内存。
	 */
	if (task_will_free_mem(current)) {
		mark_oom_victim(current);
		wake_oom_reaper(current);
		return true;
	}

	/*
	 * The OOM killer does not compensate for IO-less reclaim.
	 * pagefault_out_of_memory lost its gfp context so we have to
	 * make sure exclude 0 mask - all other users should have at least
	 * ___GFP_DIRECT_RECLAIM to get here.
	 * OOM killer不会对无IO回收进行补偿。pagefault_out_of_memory失去了gfp上下文，
	 * 因此我们必须确保排除0掩码 - 所有其他用户应至少具有___GFP_DIRECT_RECLAIM才能到达此处。
	 */
	if (oc->gfp_mask && !(oc->gfp_mask & __GFP_FS))
		return true;

	/*
	 * Check if there were limitations on the allocation (only relevant for
	 * NUMA and memcg) that may require different handling.
	 * 检查是否有对分配的限制（仅适用于NUMA和memcg），这可能需要不同的处理。
	 */
	constraint = constrained_alloc(oc);
	if (constraint != CONSTRAINT_MEMORY_POLICY)
		oc->nodemask = NULL;
	check_panic_on_oom(oc, constraint);

	// 如果不是内存控制组触发的 OOM，并且启用了 sysctl_oom_kill_allocating_task，
	// 当前进程具有内存映射并且不是不可杀进程，并且当前进程的 oom_score_adj 不是 OOM_SCORE_ADJ_MIN，
	// 则将当前进程设置为选择的进程并杀死该进程，发送 "Out of memory (oom_kill_allocating_task)" 的信号。然后返回 true。
	if (!is_memcg_oom(oc) && sysctl_oom_kill_allocating_task &&
	    current->mm && !oom_unkillable_task(current, NULL, oc->nodemask) &&
	    current->signal->oom_score_adj != OOM_SCORE_ADJ_MIN) {
		get_task_struct(current);
		oc->chosen = current;
		oom_kill_process(oc, "Out of memory (oom_kill_allocating_task)");
		return true;
	}

	select_bad_process(oc);
	/* Found nothing?!?! */
	if (!oc->chosen) {
		dump_header(oc, NULL);
		pr_warn("Out of memory and no killable processes...\n");
		/*
		 * If we got here due to an actual allocation at the
		 * system level, we cannot survive this and will enter
		 * an endless loop in the allocator. Bail out now.
		 */
		if (!is_sysrq_oom(oc) && !is_memcg_oom(oc))
			panic("System is deadlocked on memory\n");
	}
	if (oc->chosen && oc->chosen != (void *)-1UL)
		oom_kill_process(oc, !is_memcg_oom(oc) ? "Out of memory" :
				 "Memory cgroup out of memory");
	return !!oc->chosen;
}

/*
 * The pagefault handler calls here because it is out of memory, so kill a
 * memory-hogging task. If oom_lock is held by somebody else, a parallel oom
 * killing is already in progress so do nothing.
 * pagefault处理程序因内存不足而调用此处，因此需要杀死一个占用内存大的任务。
 * 如果oom_lock已被其他人持有，则已经有一个并行的oom killing正在进行，因此不需要做任何事情。
 */
// pagefault_out_of_memory用于处理当系统内存不足时的情况
void pagefault_out_of_memory(void)
{
	struct oom_control oc = {
		.zonelist = NULL,
		.nodemask = NULL,
		.memcg = NULL,
		.gfp_mask = 0,
		.order = 0,
	};

	// mem_cgroup_oom_synchronize函数来同步内存控制组中的内存使用情况。如果返回值为true，则说明已经在其他地方处理过内存不足的情况，该函数直接返回
	if (mem_cgroup_oom_synchronize(true))
		return;

	// 尝试获取oom_lock互斥锁
	if (!mutex_trylock(&oom_lock))
		return;
	// 调用out_of_memory函数来处理内存不足的情况
	out_of_memory(&oc);
	mutex_unlock(&oom_lock);
}
