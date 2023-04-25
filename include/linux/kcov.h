/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_KCOV_H
#define _LINUX_KCOV_H

#include <uapi/linux/kcov.h>

struct task_struct;

#ifdef CONFIG_KCOV

// 定义了一个可选的按需代码覆盖率收集机制 (KCOV：Kernel Coverage)，该机制通过枚举类型 kcov_mode 定义收集覆盖率的不同模式:
enum kcov_mode {
	/* Coverage collection is not enabled yet. */
	// KCOV_MODE_DISABLED：表示尚未启用代码覆盖率收集。此时收集为空，覆盖率信息被忽略，不进行任何收集工作。
	KCOV_MODE_DISABLED = 0,
	/* KCOV was initialized, but tracing mode hasn't been chosen yet. */
	// KCOV_MODE_INIT：表示KCOV已经初始化，但还未选择跟踪模式。
	KCOV_MODE_INIT = 1,
	/*
	 * Tracing coverage collection mode.
	 * Covered PCs are collected in a per-task buffer.
	 */
	// KCOV_MODE_TRACE_PC：表示以跟踪模式进行覆盖率收集，涉及到内核中被执行的程序计数器。这时需要分配一个任务相关的缓冲区（task buffer），以存储涉及到的覆盖信息。
	KCOV_MODE_TRACE_PC = 2,
	/* Collecting comparison operands mode. */
	// KCOV_MODE_TRACE_CMP：表示以跟踪模式进行覆盖率收集，涉及到比较操作数。这种模式的覆盖率收集非常适合修改分支条件的程序段。
	KCOV_MODE_TRACE_CMP = 3,
};

#define KCOV_IN_CTXSW	(1 << 30)

void kcov_task_init(struct task_struct *t);
void kcov_task_exit(struct task_struct *t);

#define kcov_prepare_switch(t)			\
do {						\
	(t)->kcov_mode |= KCOV_IN_CTXSW;	\
} while (0)

#define kcov_finish_switch(t)			\
do {						\
	(t)->kcov_mode &= ~KCOV_IN_CTXSW;	\
} while (0)

#else

static inline void kcov_task_init(struct task_struct *t) {}
static inline void kcov_task_exit(struct task_struct *t) {}
static inline void kcov_prepare_switch(struct task_struct *t) {}
static inline void kcov_finish_switch(struct task_struct *t) {}

#endif /* CONFIG_KCOV */
#endif /* _LINUX_KCOV_H */
