/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_NSPROXY_H
#define _LINUX_NSPROXY_H

#include <linux/spinlock.h>
#include <linux/sched.h>

struct mnt_namespace;
struct uts_namespace;
struct ipc_namespace;
struct pid_namespace;
struct cgroup_namespace;
struct fs_struct;

/*
 * A structure to contain pointers to all per-process
 * namespaces - fs (mount), uts, network, sysvipc, etc.
 * 一个结构体，用于保存指向每个进程命名空间（fs（挂载），uts，网络，sysvipc等）的指针。
 *
 * The pid namespace is an exception -- it's accessed using
 * task_active_pid_ns.  The pid namespace here is the
 * namespace that children will use.
 * pid 命名空间是一个例外 - 它是使用 task_active_pid_ns 访问的。这里的 pid 命名空间是子进程将使用的命名空间。
 *
 * 'count' is the number of tasks holding a reference.
 * The count for each namespace, then, will be the number
 * of nsproxies pointing to it, not the number of tasks.
 * 'count' 是持有引用计数的任务数。因此，每个命名空间的计数将是指向它的 nsproxy 数量，而不是任务数。
 *
 * The nsproxy is shared by tasks which share all namespaces.
 * As soon as a single namespace is cloned or unshared, the
 * nsproxy is copied.
 * nsproxy 被那些共享所有命名空间的任务所共享。一旦单个命名空间被克隆或未共享，nsproxy 就会被复制。
 */
// nsproxy 的结构体，它用于在 Linux 内核中管理进程的命名空间。
struct nsproxy {
	// count：引用计数器，用于管理该结构体的生命周期。
	atomic_t count;
	// uts_ns：指向与进程相关联的 UTS 命名空间的指针。UTS 命名空间包含了主机名和域名等信息。
	struct uts_namespace *uts_ns;
	// ipc_ns：指向与进程相关联的 IPC 命名空间的指针。IPC 命名空间包含了 System V IPC 和 POSIX 消息队列等进程间通信的机制。
	struct ipc_namespace *ipc_ns;
	// mnt_ns：指向与进程相关联的挂载命名空间的指针。挂载命名空间决定了进程可访问的文件系统层次结构。
	struct mnt_namespace *mnt_ns;
	// pid_ns_for_children：指向与进程相关联的 PID 命名空间的指针。PID 命名空间为每个进程分配了一个唯一的进程 ID。
	struct pid_namespace *pid_ns_for_children;
	// net_ns：指向与进程相关联的网络命名空间的指针。网络命名空间包含了网络设备和地址等信息，可以使不同进程之间的网络配置独立。
	struct net 	     *net_ns;
	// cgroup_ns：指向与进程相关联的cgroup 命名空间的指针。cgroup命名空间含有控制与进程相关联的 cgroup(hierarchies, controllers and mount points)的信息。
	struct cgroup_namespace *cgroup_ns;
};
extern struct nsproxy init_nsproxy;

/*
 * the namespaces access rules are:
 *
 *  1. only current task is allowed to change tsk->nsproxy pointer or
 *     any pointer on the nsproxy itself.  Current must hold the task_lock
 *     when changing tsk->nsproxy.
 *
 *  2. when accessing (i.e. reading) current task's namespaces - no
 *     precautions should be taken - just dereference the pointers
 *
 *  3. the access to other task namespaces is performed like this
 *     task_lock(task);
 *     nsproxy = task->nsproxy;
 *     if (nsproxy != NULL) {
 *             / *
 *               * work with the namespaces here
 *               * e.g. get the reference on one of them
 *               * /
 *     } / *
 *         * NULL task->nsproxy means that this task is
 *         * almost dead (zombie)
 *         * /
 *     task_unlock(task);
 *
 */

int copy_namespaces(unsigned long flags, struct task_struct *tsk);
void exit_task_namespaces(struct task_struct *tsk);
void switch_task_namespaces(struct task_struct *tsk, struct nsproxy *new);
void free_nsproxy(struct nsproxy *ns);
int unshare_nsproxy_namespaces(unsigned long, struct nsproxy **,
	struct cred *, struct fs_struct *);
int __init nsproxy_cache_init(void);

static inline void put_nsproxy(struct nsproxy *ns)
{
	if (atomic_dec_and_test(&ns->count)) {
		free_nsproxy(ns);
	}
}

static inline void get_nsproxy(struct nsproxy *ns)
{
	atomic_inc(&ns->count);
}

#endif
