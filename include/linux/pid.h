/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PID_H
#define _LINUX_PID_H

#include <linux/rculist.h>

// 用来表示所涉及到的进程实体的类型。因为在 Linux 中，不仅进程有 ID，进程组、会话也都有对应的 ID，它们各自之间是有关联的。而区分它们的 ID 类型，可以方便内核代码的编写和调用。
enum pid_type
{
	// PIDTYPE_PID: 表示进程 ID
	PIDTYPE_PID,
	// PIDTYPE_TGID: 表示进程组 ID
	PIDTYPE_TGID,
	// PIDTYPE_PGID: 表示进程组的领导进程 ID
	PIDTYPE_PGID,
	// PIDTYPE_SID: 表示会话 ID
	PIDTYPE_SID,
	// PIDTYPE_MAX: 表示枚举常量的最大值
	PIDTYPE_MAX,
};

/*
 * What is struct pid?
 * 什么是 struct pid？
 *
 * A struct pid is the kernel's internal notion of a process identifier.
 * It refers to individual tasks, process groups, and sessions.  While
 * there are processes attached to it the struct pid lives in a hash
 * table, so it and then the processes that it refers to can be found
 * quickly from the numeric pid value.  The attached processes may be
 * quickly accessed by following pointers from struct pid.
 * struct pid 是内核对进程标识符的内部概念。它涵盖了个别任务、进程组和会话等。当有进程与之关联时，struct pid 存在于哈希表中，因此可以快速从数字 pid 值中查找到它和相应的进程。
 * 从 struct pid 开始，可以通过指针快速访问所关联的进程。
 *
 * Storing pid_t values in the kernel and referring to them later has a
 * problem.  The process originally with that pid may have exited and the
 * pid allocator wrapped, and another process could have come along
 * and been assigned that pid.
 * 在内核中存储 pid_t 值并随后引用它们存在一个问题。原本具有该 pid 的进程可能已经退出，而 pid 分配器已经包含了该 pid，可能已经分配给另一个进程。
 *
 * Referring to user space processes by holding a reference to struct
 * task_struct has a problem.  When the user space process exits
 * the now useless task_struct is still kept.  A task_struct plus a
 * stack consumes around 10K of low kernel memory.  More precisely
 * this is THREAD_SIZE + sizeof(struct task_struct).  By comparison
 * a struct pid is about 64 bytes.
 * 通过持有对 struct task_struct 的引用来引用用户进程也存在问题。当用户进程退出时，无用的 task_struct 仍被保留。
 * task_struct 加上一个堆栈会消耗大约 10k 的低端内核内存。更准确地说，这是 THREAD_SIZE + sizeof(struct task_struct)。相比之下，struct pid 大约只有 64 字节。
 *
 * Holding a reference to struct pid solves both of these problems.
 * It is small so holding a reference does not consume a lot of
 * resources, and since a new struct pid is allocated when the numeric pid
 * value is reused (when pids wrap around) we don't mistakenly refer to new
 * processes.
 * 持有对 struct pid 的引用可以解决这两个问题。它很小，因此持有引用不会消耗大量资源，
 * 而且当数字 pid 值被重用（当 pid 回环）时，会分配新的 struct pid，因此不会错误地引用新的进程。
 */


/*
 * struct upid is used to get the id of the struct pid, as it is
 * seen in particular namespace. Later the struct pid is found with
 * find_pid_ns() using the int nr and struct pid_namespace *ns.
 * struct upid 用于获取在特定命名空间中看到的 struct pid 的 ID。稍后，
 * 使用 int nr 和 struct pid_namespace *ns 查找 struct pid，通过 find_pid_ns() 实现。
 */
// upid 结构体是用来表示一个进程 ID 和它所在的 PID 命名空间的关联关系的
struct upid {
	int nr; // nr：表示进程 ID
	struct pid_namespace *ns; // ns：表示进程所在的 PID 命名空间的指针
};

// pid，它用于表示一个进程 ID 在内核中的数据结构
struct pid
{
	// count：是一个原子计数器，用于记录该进程 ID 所在的 pid 结构体的引用计数，防止出现并发使用或无效释放等问题。
	atomic_t count;
	// level：记录该进程 ID 的级别，表示该 ID 是进程 ID 还是进程组 ID，还是其他类型的 ID。
	unsigned int level;
	/* lists of tasks that use this pid */
	// tasks：是一个哈希列表数组，用于记录所有使用该进程 ID 的进程、线程或者内核任务的指针。
	// 不同类型的任务会放在不同的哈希列表中，例如所有进程 ID 为 1 的进程会放在 tasks[PIDTYPE_PID] 对应的哈希列表中。
	struct hlist_head tasks[PIDTYPE_MAX];
	// rcu：这是一个 RCU（Read-Copy Update）机制的内核结构体，用于实现内核代码中的读-写锁的功能，保证并发访问的正确性和性能。
	struct rcu_head rcu;
	// numbers：这是一个数组，在内存中只占用一个成员，用于存储该进程 ID 在所有 PID 命名空间中的记录，其中的 nr 和 ns 分别表示该进程在某个特定的 PID 命名空间中的实际 ID 和所在的命名空间。
	struct upid numbers[1];
};

extern struct pid init_struct_pid;

static inline struct pid *get_pid(struct pid *pid)
{
	if (pid)
		atomic_inc(&pid->count);
	return pid;
}

extern void put_pid(struct pid *pid);
extern struct task_struct *pid_task(struct pid *pid, enum pid_type);
extern struct task_struct *get_pid_task(struct pid *pid, enum pid_type);

extern struct pid *get_task_pid(struct task_struct *task, enum pid_type type);

/*
 * these helpers must be called with the tasklist_lock write-held.
 */
extern void attach_pid(struct task_struct *task, enum pid_type);
extern void detach_pid(struct task_struct *task, enum pid_type);
extern void change_pid(struct task_struct *task, enum pid_type,
			struct pid *pid);
extern void transfer_pid(struct task_struct *old, struct task_struct *new,
			 enum pid_type);

struct pid_namespace;
extern struct pid_namespace init_pid_ns;

/*
 * look up a PID in the hash table. Must be called with the tasklist_lock
 * or rcu_read_lock() held.
 *
 * find_pid_ns() finds the pid in the namespace specified
 * find_vpid() finds the pid by its virtual id, i.e. in the current namespace
 *
 * see also find_task_by_vpid() set in include/linux/sched.h
 */
extern struct pid *find_pid_ns(int nr, struct pid_namespace *ns);
extern struct pid *find_vpid(int nr);

/*
 * Lookup a PID in the hash table, and return with it's count elevated.
 */
extern struct pid *find_get_pid(int nr);
extern struct pid *find_ge_pid(int nr, struct pid_namespace *);
int next_pidmap(struct pid_namespace *pid_ns, unsigned int last);

extern struct pid *alloc_pid(struct pid_namespace *ns);
extern void free_pid(struct pid *pid);
extern void disable_pid_allocation(struct pid_namespace *ns);

/*
 * ns_of_pid() returns the pid namespace in which the specified pid was
 * allocated.
 *
 * NOTE:
 * 	ns_of_pid() is expected to be called for a process (task) that has
 * 	an attached 'struct pid' (see attach_pid(), detach_pid()) i.e @pid
 * 	is expected to be non-NULL. If @pid is NULL, caller should handle
 * 	the resulting NULL pid-ns.
 */
static inline struct pid_namespace *ns_of_pid(struct pid *pid)
{
	struct pid_namespace *ns = NULL;
	if (pid)
		ns = pid->numbers[pid->level].ns;
	return ns;
}

/*
 * is_child_reaper returns true if the pid is the init process
 * of the current namespace. As this one could be checked before
 * pid_ns->child_reaper is assigned in copy_process, we check
 * with the pid number.
 */
static inline bool is_child_reaper(struct pid *pid)
{
	return pid->numbers[pid->level].nr == 1;
}

/*
 * the helpers to get the pid's id seen from different namespaces
 *
 * pid_nr()    : global id, i.e. the id seen from the init namespace;
 * pid_vnr()   : virtual id, i.e. the id seen from the pid namespace of
 *               current.
 * pid_nr_ns() : id seen from the ns specified.
 *
 * see also task_xid_nr() etc in include/linux/sched.h
 */

static inline pid_t pid_nr(struct pid *pid)
{
	pid_t nr = 0;
	if (pid)
		nr = pid->numbers[0].nr;
	return nr;
}

pid_t pid_nr_ns(struct pid *pid, struct pid_namespace *ns);
pid_t pid_vnr(struct pid *pid);

#define do_each_pid_task(pid, type, task)				\
	do {								\
		if ((pid) != NULL)					\
			hlist_for_each_entry_rcu((task),		\
				&(pid)->tasks[type], pid_links[type]) {

			/*
			 * Both old and new leaders may be attached to
			 * the same pid in the middle of de_thread().
			 */
#define while_each_pid_task(pid, type, task)				\
				if (type == PIDTYPE_PID)		\
					break;				\
			}						\
	} while (0)

#define do_each_pid_thread(pid, type, task)				\
	do_each_pid_task(pid, type, task) {				\
		struct task_struct *tg___ = task;			\
		for_each_thread(tg___, task) {

#define while_each_pid_thread(pid, type, task)				\
		}							\
		task = tg___;						\
	} while_each_pid_task(pid, type, task)
#endif /* _LINUX_PID_H */
