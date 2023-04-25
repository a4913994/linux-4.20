/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_SCHED_TYPES_H
#define _UAPI_LINUX_SCHED_TYPES_H

#include <linux/types.h>

struct sched_param {
	int sched_priority;
};

#define SCHED_ATTR_SIZE_VER0	48	/* sizeof first published struct */

/*
 * Extended scheduling parameters data structure.
 * 扩展调度参数数据结构
 *
 * This is needed because the original struct sched_param can not be
 * altered without introducing ABI issues with legacy applications
 * (e.g., in sched_getparam()).
 * 这是必需的，因为原始的 struct sched_param 不能修改，否则会引入与遗留应用程序的ABI问题(例如，sched_getparam()中)
 *
 * However, the possibility of specifying more than just a priority for
 * the tasks may be useful for a wide variety of application fields, e.g.,
 * multimedia, streaming, automation and control, and many others.
 * 然而，对于任务而言，除了指定优先级之外，可能还有指定更多信息的可能性，在多个应用领域(例如，多媒体，流媒体，自动化和控制等)可能很有用
 *
 * This variant (sched_attr) is meant at describing a so-called
 * sporadic time-constrained task. In such model a task is specified by:
 *  - the activation period or minimum instance inter-arrival time;
 *  - the maximum (or average, depending on the actual scheduling
 *    discipline) computation time of all instances, a.k.a. runtime;
 *  - the deadline (relative to the actual activation time) of each
 *    instance.
 * 这种变体 (sched_attr) 用于描述所谓的间歇时间约束任务。在这种模型中，任务由以下内容指定:
	- 激活时间或最小实例到达时间;
	- 所有实例的最大(或平均，具体取决于实际调度策略)计算时间，也称为运行时间;
	- 每个实例相对于实际激活时间的截止时间。
 * Very briefly, a periodic (sporadic) task asks for the execution of
 * some specific computation --which is typically called an instance--
 * (at most) every period. Moreover, each instance typically lasts no more
 * than the runtime and must be completed by time instant t equal to
 * the instance activation time + the deadline.
 * 简而言之，周期性(间歇性)任务要求执行某些特定计算 --通常称为实例--(最多)每个周期。此外，每个实例通常不超过运行时间，并且必须在时间 t 等于实例激活时间加上截止时间时完成。
 *
 * This is reflected by the actual fields of the sched_attr structure:
 * 这反映在 sched_attr 结构的实际字段上:
 *
 *  @size		size of the structure, for fwd/bwd compat.
 *  @ size 结构的大小，用于前向/后向兼容。
 *
 *  @sched_policy	task's scheduling policy 任务的调度策略
 *  @sched_flags	for customizing the scheduler behaviour 用于自定义调度器行为
 *  @sched_nice		task's nice value      (SCHED_NORMAL/BATCH) 任务的良好值 (SCHED_NORMAL/BATCH)
 *  @sched_priority	task's static priority (SCHED_FIFO/RR)  任务的静态优先级 (SCHED_FIFO/RR)
 *  @sched_deadline	representative of the task's deadline 任务截止的代表值
 *  @sched_runtime	representative of the task's runtime 任务的运行时间的代表值
 *  @sched_period	representative of the task's period  任务周期的代表值
 *
 * Given this task model, there are a multiplicity of scheduling algorithms
 * and policies, that can be used to ensure all the tasks will make their
 * timing constraints.
 * 鉴于这个任务模型，有多种调度算法和策略可以用于确保所有任务都能满足其时序限制。
 *
 * As of now, the SCHED_DEADLINE policy (sched_dl scheduling class) is the
 * only user of this new interface. More information about the algorithm
 * available in the scheduling class file or in Documentation/.
 * 目前，SCHED_DEADLINE策略(sched_dl调度类)是唯一使用这个新接口的用户。更多关于在调度类文件或文档中可用的算法的信息。
 */
struct sched_attr {
	__u32 size;

	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;

	/* SCHED_DEADLINE */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
};

#endif /* _UAPI_LINUX_SCHED_TYPES_H */
