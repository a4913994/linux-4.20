/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PID_NS_H
#define _LINUX_PID_NS_H

#include <linux/sched.h>
#include <linux/bug.h>
#include <linux/mm.h>
#include <linux/workqueue.h>
#include <linux/threads.h>
#include <linux/nsproxy.h>
#include <linux/kref.h>
#include <linux/ns_common.h>
#include <linux/idr.h>


struct fs_pin;

enum { /* definitions for pid_namespace's hide_pid field */
	HIDEPID_OFF	  = 0,
	HIDEPID_NO_ACCESS = 1,
	HIDEPID_INVISIBLE = 2,
};

// pid_namespace，该结构体用于表示进程 ID 的命名空间
struct pid_namespace {
	// kref：用于为该命名空间维护引用计数和内存管理
	struct kref kref;
	// idr：用于管理该命名空间中的所有进程 ID
	struct idr idr;
	// rcu: 用于实现读-写锁机制
	struct rcu_head rcu;
	// pid_allocated：该命名空间分配的进程 ID 数量
	unsigned int pid_allocated;
	// child_reaper：一个指向负责回收孤儿进程的进程的指针
	struct task_struct *child_reaper;
	// pid_cachep：该命名空间分配进程 ID 使用的 kmem 缓存
	struct kmem_cache *pid_cachep;
	// level：该命名空间的层级，用于表示进程 ID 可能的用途（如进程 ID，进程组 ID 等等）
	unsigned int level;
	// parent：指向该命名空间的父命名空间
	struct pid_namespace *parent;
#ifdef CONFIG_PROC_FS
	// proc_mnt / proc_self / proc_thread_self：用于管理原始进程和线程状态信息的 procfs 文件系统的相关信息（可选）
	struct vfsmount *proc_mnt;
	struct dentry *proc_self;
	struct dentry *proc_thread_self;
#endif
#ifdef CONFIG_BSD_PROCESS_ACCT
	struct fs_pin *bacct;
#endif
	// user_ns：与该命名空间关联的用户命名空间
	struct user_namespace *user_ns;
	// ucounts：与该命名空间关联的计数器列表，保存关于该命名空间使用的各种资源的信息
	struct ucounts *ucounts;
	// proc_work：用于异步操作 procfs 文件系统的工作项
	struct work_struct proc_work;
	// pid_gid：用于管理该命名空间的进程 ID 的组 ID
	kgid_t pid_gid;
	// hide_pid：用于指定是否公开该命名空间中的进程 ID
	int hide_pid;
	// reboot：用于保存进程 ID 命名空间的重启状态信息
	int reboot;	/* group exit code if this pidns was rebooted */
	// ns：该命名空间的通用命名空间结构
	struct ns_common ns;
} __randomize_layout;

extern struct pid_namespace init_pid_ns;

#define PIDNS_ADDING (1U << 31)

#ifdef CONFIG_PID_NS
static inline struct pid_namespace *get_pid_ns(struct pid_namespace *ns)
{
	if (ns != &init_pid_ns)
		kref_get(&ns->kref);
	return ns;
}

extern struct pid_namespace *copy_pid_ns(unsigned long flags,
	struct user_namespace *user_ns, struct pid_namespace *ns);
extern void zap_pid_ns_processes(struct pid_namespace *pid_ns);
extern int reboot_pid_ns(struct pid_namespace *pid_ns, int cmd);
extern void put_pid_ns(struct pid_namespace *ns);

#else /* !CONFIG_PID_NS */
#include <linux/err.h>

static inline struct pid_namespace *get_pid_ns(struct pid_namespace *ns)
{
	return ns;
}

static inline struct pid_namespace *copy_pid_ns(unsigned long flags,
	struct user_namespace *user_ns, struct pid_namespace *ns)
{
	if (flags & CLONE_NEWPID)
		ns = ERR_PTR(-EINVAL);
	return ns;
}

static inline void put_pid_ns(struct pid_namespace *ns)
{
}

static inline void zap_pid_ns_processes(struct pid_namespace *ns)
{
	BUG();
}

static inline int reboot_pid_ns(struct pid_namespace *pid_ns, int cmd)
{
	return 0;
}
#endif /* CONFIG_PID_NS */

extern struct pid_namespace *task_active_pid_ns(struct task_struct *tsk);
void pidhash_init(void);
void pid_idr_init(void);

#endif /* _LINUX_PID_NS_H */
