<center><font size='5'>Linux内核中的同步原语</font></center>
<center><font size='6'>自旋锁，信号量，互斥锁，读写信号量，顺序锁</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>

* 在[英文原文](https://0xax.gitbook.io/linux-insides)基础上，针对[中文译文](https://xinqiu.gitbooks.io/linux-insides-cn)增加5.10.13内核源码相关内容。


# 1. Linux 内核中的同步原语介绍

这一部分为 [linux-insides](https://xinqiu.gitbooks.io/linux-insides-cn/content/) 这本书开启了新的章节。定时器和时间管理相关的概念在上一个[章节](https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/index.html)已经描述过了。现在是时候继续了。就像你可能从这一部分的标题所了解的那样，本章节将会描述 Linux 内核中的[同步](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)原语。

像往常一样，在考虑一些同步相关的事情之前，我们会尝试去概括地了解什么是`同步原语`。事实上，同步原语是一种软件机制，提供了两个或者多个[并行](https://en.wikipedia.org/wiki/Parallel_computing)进程或者线程在不同时刻执行一段相同的代码段的能力。例如下面的代码片段：

```C
mutex_lock(&clocksource_mutex);
...
...
...
clocksource_enqueue(cs);
clocksource_enqueue_watchdog(cs);
clocksource_select();
...
...
...
mutex_unlock(&clocksource_mutex);
```
出自 [kernel/time/clocksource.c](https://github.com/torvalds/linux/master/kernel/time/clocksource.c) 源文件。这段代码来自于 `__clocksource_register_scale` 函数，此函数添加给定的 [clocksource](https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/linux-timers-2.html) 到时钟源列表中。这个函数在注册时钟源列表中生成两个不同的操作。例如 `clocksource_enqueue` 函数就是添加给定时钟源到注册时钟源列表——`clocksource_list` 中。注意这几行代码被两个函数所包围：`mutex_lock` 和 `mutex_unlock`，这两个函数都带有一个参数——在本例中为 `clocksource_mutex`。

这些函数展示了基于[互斥锁 (mutex)](https://en.wikipedia.org/wiki/Mutual_exclusion) 同步原语的加锁和解锁。当 `mutex_lock` 被执行，允许我们阻止两个或两个以上线程执行这段代码，而 `mute_unlock` 还没有被互斥锁的处理拥有者锁执行。换句话说，就是阻止在 `clocksource_list`上的并行操作。为什么在这里需要使用`互斥锁`？ 如果两个并行处理尝试去注册一个时钟源会怎样。正如我们已经知道的那样，其中具有最大的等级（其具有最高的频率在系统中注册的时钟源）的列表中选择一个时钟源后，`clocksource_enqueue` 函数立即将一个给定的时钟源到 `clocksource_list` 列表：

```C
static void clocksource_enqueue(struct clocksource *cs)
{
	struct list_head *entry = &clocksource_list;
	struct clocksource *tmp;

	list_for_each_entry(tmp, &clocksource_list, list)
		if (tmp->rating >= cs->rating)
			entry = &tmp->list;
	list_add(&cs->list, entry);
}
```

如果两个并行处理尝试同时去执行这个函数，那么这两个处理可能会找到相同的 `入口 (entry)` 可能发生[竞态条件 (race condition)](https://en.wikipedia.org/wiki/Race_condition) 或者换句话说，第二个执行 `list_add` 的处理程序，将会重写第一个线程写入的时钟源。

除了这个简答的例子，同步原语在 Linux 内核无处不在。如果再翻阅之前的[章节] (https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/index.html) 或者其他章节或者如果大概看看 Linux 内核源码，就会发现许多地方都使用同步原语。我们不考虑 `mutex` 在 Linux 内核是如何实现的。事实上，Linux 内核提供了一系列不同的同步原语：

* `mutex`;
* `semaphores`;
* `seqlocks`;
* `atomic operations`;
* 等等。

现在从`自旋锁 (spinlock)` 这个章节开始。

# 2. Linux 内核中的自旋锁。

自旋锁简单来说是一种低级的同步机制，表示了一个变量可能的两个状态：

* `acquired`;
* `released`.

每一个想要获取`自旋锁`的处理，必须为这个变量写入一个表示`自旋锁获取 (spinlock acquire)`状态的值，并且为这个变量写入`锁释放 (spinlock released)`状态。如果一个处理程序尝试执行受`自旋锁`保护的代码，那么代码将会被锁住，直到占有锁的处理程序释放掉。在本例中，所有相关的操作必须是
[原子的 (atomic)](https://en.wikipedia.org/wiki/Linearizability)，来阻止[竞态条件](https://en.wikipedia.org/wiki/Race_condition)状态。`自旋锁`在 Linux 内核中使用 `spinlock_t` 类型来表示。如果我们查看 Linux 内核代码，我们会看到，这个类型被[广泛地 (widely)](http://lxr.free-electrons.com/ident?i=spinlock_t) 使用。`spinlock_t` 的定义如下：

```C
typedef struct spinlock {
    union {
      struct raw_spinlock rlock;

#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define LOCK_PADSIZE (offsetof(struct raw_spinlock, dep_map))
        struct {
            u8 __padding[LOCK_PADSIZE];
            struct lockdep_map dep_map;
        };
#endif
    };
} spinlock_t;
```

这段代码在 [include/linux/spinlock_types.h](https://github.com/torvalds/linux/master/include/linux/spinlock_types.h) 头文件中定义。可以看出，它的实现依赖于 `CONFIG_DEBUG_LOCK_ALLOC` 内核配置选项这个状态。现在我们跳过这一块，因为所有的调试相关的事情都将会在这一部分的最后。所以，如果 `CONFIG_DEBUG_LOCK_ALLOC` 内核配置选项不可用，那么 `spinlock_t` 则包含[联合体 (union)](https://en.wikipedia.org/wiki/Union_type#C.2FC.2B.2B)，这个联合体有一个字段——`raw_spinlock`：

```C
typedef struct spinlock {
    union {
          struct raw_spinlock rlock;
    };
} spinlock_t;
```
`raw_spinlock` 结构的定义在[相同的](https://github.com/torvalds/linux/master/include/linux/spinlock_types.h)头文件中并且表达了`普通 (normal)` 自旋锁的实现。让我们看看 `raw_spinlock`结构是如何定义的：

```C
typedef struct raw_spinlock {
        arch_spinlock_t raw_lock;
#ifdef CONFIG_GENERIC_LOCKBREAK
        unsigned int break_lock;
#endif
} raw_spinlock_t;
```

这里的 `arch_spinlock_t` 表示了体系结构指定的`自旋锁`实现并且 `break_lock` 字段持有值—— 为`1`，当一个处理器开始等待而锁被另一个处理器持有时，使用的[对称多处理器 (SMP)](https://en.wikipedia.org/wiki/Symmetric_multiprocessing) 系统的例子中。这样就可以防止长时间加锁。考虑本书的 [x86_64](https://en.wikipedia.org/wiki/X86-64) 架构，因此 `arch_spinlock_t` 被定义在 [arch/x86/include/asm/spinlock_types.h](https://github.com/torvalds/linux/master/arch/x86/include/asm/spinlock_types.h) 头文件中，并且看上去是这样：

```C
#ifdef CONFIG_QUEUED_SPINLOCKS
#include <asm-generic/qspinlock_types.h>
#else
typedef struct arch_spinlock {
    union {
        __ticketpair_t head_tail;
        struct __raw_tickets {
                __ticket_t head, tail;
        } tickets;
    };
} arch_spinlock_t;
```

正如我们所看到的，`arch_spinlock` 结构的定义依赖于 `CONFIG_QUEUED_SPINLOCKS` 内核配置选项的值。这个 Linux内核配置选项支持使用队列的 `自旋锁`。这个`自旋锁`的特殊类型替代了 `acquired` 和 `released` [原子](https://en.wikipedia.org/wiki/Linearizability)值，在`队列`上使用`原子`操作。如果 `CONFIG_QUEUED_SPINLOCKS` 内核配置选项启动，那么 `arch_spinlock_t` 将会被表示成如下的结构：

```C
typedef struct qspinlock {
	atomic_t	val;
} arch_spinlock_t;
```

来自于 [include/asm-generic/qspinlock_types.h](https://github.com/torvalds/linux/master/include/asm-generic/qspinlock_types.h) 头文件。

目前我们不会在这个结构上停止探索，在考虑 `arch_spinlock` 和 `qspinlock` 之前，先看看自旋锁上的操作。 Linux内核在`自旋锁`上提供了一下主要的操作：

* `spin_lock_init` ——给定的`自旋锁`进行初始化；
* `spin_lock` ——获取给定的`自旋锁`；
* `spin_lock_bh` ——禁止软件[中断](https://en.wikipedia.org/wiki/Interrupt)并且获取给定的`自旋锁`。
* `spin_lock_irqsave` 和 `spin_lock_irq`——禁止本地处理器上的中断，并且保存／不保存之前的中断状态的`标识 (flag)`；
* `spin_unlock` ——释放给定的`自旋锁`;
* `spin_unlock_bh` ——释放给定的`自旋锁`并且启动软件中断；
* `spin_is_locked` - 返回给定的`自旋锁`的状态；
* 等等。

来看看 `spin_lock_init` 宏的实现。就如我已经写过的一样，这个宏和其他宏定义都在 [include/linux/spinlock.h](https://github.com/torvalds/linux/master/include/linux/spinlock.h) 头文件里，并且 `spin_lock_init` 宏如下所示：

```C
#define spin_lock_init(_lock)		\
do {							                \
	spinlock_check(_lock);				        \
	raw_spin_lock_init(&(_lock)->rlock);		\
} while (0)
```

你在5.10.13中可能遇到这样的：

```c
#ifdef CONFIG_DEBUG_SPINLOCK

# define spin_lock_init(lock)					\
do {								\
	static struct lock_class_key __key;			\
								\
	__raw_spin_lock_init(spinlock_check(lock),		\
			     #lock, &__key, LD_WAIT_CONFIG);	\
} while (0)

#else

# define spin_lock_init(_lock)			\
do {						\
	spinlock_check(_lock);			\
	*(_lock) = __SPIN_LOCK_UNLOCKED(_lock);	\
} while (0)

#endif
```

正如所看到的，`spin_lock_init` 宏有一个`自旋锁`，执行两步操作：检查我们看到的给定的`自旋锁`和执行 `raw_spin_lock_init`。`spinlock_check`的实现相当简单，实现的函数仅仅返回已知的`自旋锁`的 `raw_spinlock_t`，来确保我们精确获得`正常 (normal)` 原生自旋锁：

```C
static __always_inline raw_spinlock_t *spinlock_check(spinlock_t *lock)
{
	return &lock->rlock;
}
```

`raw_spin_lock_init` 宏:

```C
# define raw_spin_lock_init(lock)		\
do {                                                  \
    *(lock) = __RAW_SPIN_LOCK_UNLOCKED(lock);         \
} while (0)                                           \
```

用 `__RAW_SPIN_LOCK_UNLOCKED` 的值和给定的`自旋锁`赋值给给定的 `raw_spinlock_t`。就像我们能从 `__RAW_SPIN_LOCK_UNLOCKED` 宏的名字中了解的那样，这个宏为给定的`自旋锁`执行初始化操作，并且将锁设置为`释放 (released)` 状态。宏的定义在 [include/linux/spinlock_types.h](https://github.com/torvalds/linux/master/include/linux/spinlock_types.h) 头文件中，并且扩展了一下的宏：

```C
#define __RAW_SPIN_LOCK_UNLOCKED(lockname)      \
         (raw_spinlock_t) __RAW_SPIN_LOCK_INITIALIZER(lockname)

#define __RAW_SPIN_LOCK_INITIALIZER(lockname)   \
         {                                                      \
             .raw_lock = __ARCH_SPIN_LOCK_UNLOCKED,             \
             SPIN_DEBUG_INIT(lockname)                          \
             SPIN_DEP_MAP_INIT(lockname)                        \
         }
```

正如之前所写的一样，我们不考虑同步原语调试相关的东西。在本例中也不考虑 `SPIN_DEBUG_INIT` 和 `SPIN_DEP_MAP_INIT` 宏。于是 `__RAW_SPINLOCK_UNLOCKED` 宏被扩展成：

```C
*(&(_lock)->rlock) = __ARCH_SPIN_LOCK_UNLOCKED;
```

而 `__ARCH_SPIN_LOCK_UNLOCKED` 宏是:

```C
#define __ARCH_SPIN_LOCK_UNLOCKED       { { 0 } }
```

还有:

```C
#define __ARCH_SPIN_LOCK_UNLOCKED       { ATOMIC_INIT(0) }
```

这是对于 [x86_64] 架构，如果 `CONFIG_QUEUED_SPINLOCKS` 内核配置选项启用的情况。那么，在 `spin_lock_init` 宏的扩展之后，给定的`自旋锁`将会初始化并且状态变为——`解锁 (unlocked)`。

从这一时刻起我们了解了如何去初始化一个`自旋锁`，现在考虑 Linux 内核为`自旋锁`的操作提供的 [API](https://en.wikipedia.org/wiki/Application_programming_interface)。首先是：

```C
static __always_inline void spin_lock(spinlock_t *lock)
{
	raw_spin_lock(&lock->rlock);
}
```

此函数允许我们`获取`一个自旋锁。`raw_spin_lock` 宏定义在同一个头文件中，并且扩展了 `_raw_spin_lock` 函数的调用：

```C
#define raw_spin_lock(lock)	_raw_spin_lock(lock)
```

就像在 [include/linux/spinlock.h](https://github.com/torvalds/linux/blob/master/include/linux/spinlock.h) 头文件所了解的那样，`_raw_spin_lock` 宏的定义依赖于 `CONFIG_SMP` 内核配置参数：

```C
#if defined(CONFIG_SMP) || defined(CONFIG_DEBUG_SPINLOCK)
# include <linux/spinlock_api_smp.h>
#else
# include <linux/spinlock_api_up.h>
#endif
```

因此，如果在 Linux内核中 [SMP](https://en.wikipedia.org/wiki/Symmetric_multiprocessing) 启用了，那么 `_raw_spin_lock` 宏就在 [arch/x86/include/asm/spinlock.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/spinlock.h) 头文件中定义，并且看起来像这样：

```C
#define _raw_spin_lock(lock) __raw_spin_lock(lock)
```

`__raw_spin_lock` 函数的定义:

```C
static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
    preempt_disable();
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
    LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
}
```
就像你们可能了解的那样， 首先我们禁用了[抢占](https://en.wikipedia.org/wiki/Preemption_%28computing%29)，通过 [include/linux/preempt.h](https://github.com/torvalds/linux/blob/master/include/linux/preempt.h) (在 Linux 内核初始化进程章节的第九[部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/Initialization/linux-initialization-9.html)会了解到更多关于抢占)中的 `preempt_disable` 调用实现禁用。当我们将要解开给定的`自旋锁`，抢占将会再次启用：

```C
static inline void __raw_spin_unlock(raw_spinlock_t *lock)
{
    ...
    ...
    ...
    preempt_enable();
}
```
当程序正在自旋锁时，这个已经获取锁的程序必须阻止其他程序方法的抢占。`spin_acquire` 宏通过其他宏宏展调用实现：

```C
#define spin_acquire(l, s, t, i)                lock_acquire_exclusive(l, s, t, NULL, i)
#define lock_acquire_exclusive(l, s, t, n, i)           lock_acquire(l, s, t, 0, 1, n, i)
```

`lock_acquire` 函数:

```C
void lock_acquire(struct lockdep_map *lock, unsigned int subclass,
                  int trylock, int read, int check,
                  struct lockdep_map *nest_lock, unsigned long ip)
{
     unsigned long flags;

     if (unlikely(current->lockdep_recursion))
            return;

     raw_local_irq_save(flags);
     check_flags(flags);

     current->lockdep_recursion = 1;
     trace_lock_acquire(lock, subclass, trylock, read, check, nest_lock, ip);
     __lock_acquire(lock, subclass, trylock, read, check,
                    irqs_disabled_flags(flags), nest_lock, ip, 0, 0);
     current->lockdep_recursion = 0;
     raw_local_irq_restore(flags);
}
```

就像之前所写的，我们不考虑这些调试或跟踪相关的东西。`lock_acquire` 函数的主要是通过 `raw_local_irq_save` 宏调用禁用硬件中断，因为给定的自旋锁可能被启用的硬件中断所获取。以这样的方式获取的话程序将不会被抢占。注意 `lock_acquire` 函数的最后将使用 `raw_local_irq_restore` 宏的帮助再次启动硬件中断。正如你们可能猜到的那样，主要工作将在 `__lock_acquire` 函数中定义，这个函数在 [kernel/locking/lockdep.c](https://github.com/torvalds/linux/blob/master/kernel/locking/lockdep.c) 源代码文件中。

`__lock_acquire` 函数看起来很大。我们将试图去理解这个函数要做什么，但不是在这一部分。事实上这个函数于 Linux内核[锁验证器 (lock validator)](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) 密切相关，而这也不是此部分的主题。如果我们要返回 `__raw_spin_lock` 函数的定义，我们将会发现最终这个定义包含了以下的定义：

```C
LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
```

`LOCK_CONTENDED` 宏的定义在  [include/linux/lockdep.h](https://github.com/torvalds/linux/blob/master/include/linux/lockdep.h) 头文件中，而且只是使用给定`自旋锁`调用已知函数:

```C
#define LOCK_CONTENDED(_lock, try, lock) \
         lock(_lock)
```

在本例中，`lock` 就是 [include/linux/spinlock.h](https://github.com/torvalds/linux/blob/master/include/linux/spinlock.h) 头文件中的 `do_raw_spin_lock`，而`_lock` 就是给定的 `raw_spinlock_t`：

```C
static inline void do_raw_spin_lock(raw_spinlock_t *lock) __acquires(lock)
{
        __acquire(lock);
         arch_spin_lock(&lock->raw_lock);
}
```
这里的 `__acquire` 只是[稀疏(sparse)]相关宏，并且当前我们也对这些不感兴趣。`arch_spin_lock` 函数定义的位置依赖于两件事：第一是系统架构，第二是我们是否使用了`队列自旋锁(queued spinlocks)`。本例中我们仅以 `x86_64` 架构为例介绍，因此 `arch_spin_lock` 的定义的宏表示源自 [include/asm-generic/qspinlock.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/qspinlocks.h) 头文件中：

```C
#define arch_spin_lock(l)               queued_spin_lock(l)
```

如果使用 `队列自旋锁`，或者其他例子中，`arch_spin_lock` 函数定在 [arch/x86/include/asm/spinlock.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/spinlock.h) 头文件中，如何处理？现在我们只考虑`普通的自旋锁`，`队列自旋锁`相关的信息将在以后了解。来再看看 `arch_spinlock` 结构的定义，理解以下 `arch_spin_lock` 函数的实现：

```C
typedef struct arch_spinlock {
     union {
        __ticketpair_t head_tail;
        struct __raw_tickets {
                __ticket_t head, tail;
        } tickets;
    };
} arch_spinlock_t;
```
这个`自旋锁`的变体被称为——`标签自旋锁 (ticket spinlock)`。 就像我们锁了解的，标签自旋锁包括两个部分。当锁被获取，如果有程序想要获取`自旋锁`它就会将`尾部(tail)`值加1。如果`尾部`不等于`头部`， 那么程序就会被锁住，直到这些变量的值不再相等。来看看`arch_spin_lock`函数上的实现：

```C
static __always_inline void arch_spin_lock(arch_spinlock_t *lock)
{
        register struct __raw_tickets inc = { .tail = TICKET_LOCK_INC };

        inc = xadd(&lock->tickets, inc);

        if (likely(inc.head == inc.tail))
                goto out;

        for (;;) {
             unsigned count = SPIN_THRESHOLD;

             do {
               inc.head = READ_ONCE(lock->tickets.head);
               if (__tickets_equal(inc.head, inc.tail))
                        goto clear_slowpath;
                cpu_relax();
             } while (--count);
             __ticket_lock_spinning(lock, inc.tail);
         }
clear_slowpath:
        __ticket_check_and_clear_slowpath(lock, inc.head);
out:
        barrier();
}
```

`arch_spin_lock` 函数在一开始能够使用`尾部`—— `1` 对 `__raw_tickets` 结构初始化：

```C
#define __TICKET_LOCK_INC       1
```

在`inc` 和 `lock->tickets` 的下一行执行 [xadd](http://x86.renejeschke.de/html/file_module_x86_id_327.html) 操作。这个操作之后 `inc`将存储给定`标签 (tickets)` 的值，然后 `tickets.tail` 将增加 `inc` 或 `1`。`尾部`值增加 `1` 意味着一个程序开始尝试持有锁。下一步做检查，检查`头部`和`尾部`是否有相同的值。如果值相等，这意味着没有程序持有锁并且我们去到了 `out` 标签。在 `arch_spin_lock` 函数的最后，我们可能了解了 `barrier` 宏表示 `屏障指令 (barrier instruction)`，该指令保证了编译器将不更改进入内存操作的顺序(更多关于内存屏障的知识可以阅读内核[文档 (documentation)](https://www.kernel.org/doc/Documentation/memory-barriers.txt))。

如果前一个程序持有锁而第二个程序开始执行 `arch_spin_lock` 函数，那么 `头部`将不会`等于``尾部`，因为`尾部`比`头部`大`1`。这样，程序将循环发生。在每次循坏迭代的时候`头部`和`尾部`的值进行比较。如果值不相等，`cpu_relax` ，也就是 [NOP](https://en.wikipedia.org/wiki/NOP) 指令将会被调用：

```C
#define cpu_relax()     asm volatile("rep; nop")
```

然后将开始循环的下一次迭代。如果值相等，这意味着持有锁的程序，释放这个锁并且下一个程序获取这个锁。

`spin_unlock` 操作遍布所有有 `spin_lock` 的宏或函数中，当然，使用的是 `unlock` 前缀。最后，`arch_spin_unlock` 函数将会被调用。如果看看 `arch_spin_lock` 函数的实现，我们将了解到这个函数增加了 `lock tickets` 列表的`头部`：

```C
__add(&lock->tickets.head, TICKET_LOCK_INC, UNLOCK_LOCK_PREFIX);
```
在 `spin_lock` 和 `spin_unlock` 的组合使用中，我们得到一个队列，其`头部`包含了一个索引号，映射了当前执行的持有锁的程序，而`尾部`包含了一个索引号，映射了最后尝试持有锁的程序：

```
     +-------+       +-------+
     |       |       |       |
head |   7   | - - - |   7   | tail
     |       |       |       |
     +-------+       +-------+
                         |
                     +-------+
                     |       |
                     |   8   |
                     |       |
                     +-------+
                         |
                     +-------+
                     |       |
                     |   9   |
                     |       |
                     +-------+
```

目前这就是全部。这一部分不涵盖所有的`自旋锁` API，但我认为这个概念背后的主要思想现在一定清楚了。

## 2.1. 结论

涵盖 Linux 内核中的同步原语的第一部分到此结束。在这一部分，我们遇见了第一个 Linux 内核提供的同步原语`自旋锁`。下一部分将会继续深入这个有趣的主题，而且将会了解到其他`同步`相关的知识。

如果您有疑问或者建议，请在twitter [0xAX](https://twitter.com/0xAX) 上联系我，通过 [email](anotherworldofworld@gmail.com) 联系我，或者创建一个 [issue](https://github.com/MintCN/linux-insides-zh/issues/new)。

**友情提示：英语不是我的母语，对于译文给您带来了的不便我感到非常抱歉。如果您发现任何错误请给我发送PR到 [linux-insides](https://github.com/0xAX/linux-insides)。**

## 2.2. 链接

* [Concurrent computing](https://en.wikipedia.org/wiki/Concurrent_computing)
* [Synchronization](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)
* [Clocksource framework](https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/linux-timers-2.html)
* [Mutex](https://en.wikipedia.org/wiki/Mutual_exclusion)
* [Race condition](https://en.wikipedia.org/wiki/Race_condition)
* [Atomic operations](https://en.wikipedia.org/wiki/Linearizability)
* [SMP](https://en.wikipedia.org/wiki/Symmetric_multiprocessing)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [Interrupts](https://en.wikipedia.org/wiki/Interrupt)
* [Preemption](https://en.wikipedia.org/wiki/Preemption_%28computing%29)
* [Linux kernel lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt)
* [Sparse](https://en.wikipedia.org/wiki/Sparse)
* [xadd instruction](http://x86.renejeschke.de/html/file_module_x86_id_327.html)
* [NOP](https://en.wikipedia.org/wiki/NOP)
* [Memory barriers](https://www.kernel.org/doc/Documentation/memory-barriers.txt)
* [Previous chapter](https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/index.html)



# 3. 队列自旋锁

这是本[章节](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/index.html)的第二部分，这部分描述 Linux 内核的和我们在本章的第一[部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-1.html)所见到的－－[自旋锁](https://en.wikipedia.org/wiki/Spinlock)的同步原语。在这个部分我们将继续学习自旋锁的同步原语。 如果阅读了上一部分的相关内容，你可能记得除了正常自旋锁，Linux 内核还提供`自旋锁`的一种特殊类型 - `队列自旋锁`。 在这个部分我们将尝试理解此概念锁代表的含义。

我们在上一[部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-1.html)已知`自旋锁`的 [API](https://en.wikipedia.org/wiki/Application_programming_interface):

* `spin_lock_init` - 为给定`自旋锁`进行初始化；
* `spin_lock` - 获取给定`自旋锁`；
* `spin_lock_bh` - 禁止软件[中断](https://en.wikipedia.org/wiki/Interrupt)并且获取给定`自旋锁`；
* `spin_lock_irqsave` 和 `spin_lock_irq` - 禁止本地处理器中断并且保存/不保存之前`标识位`的中断状态；
* `spin_unlock` - 释放给定的`自旋锁`；
* `spin_unlock_bh` - 释放给定的`自旋锁`并且启用软件中断；
* `spin_is_locked` - 返回给定`自旋锁`的状态；
* 等等。

而且我们知道所有这些宏都在 [include/linux/spinlock.h](https://github.com/torvalds/linux/blob/master/include/linux/spinlock.h) 头文件中所定义，都被扩展成针对 [x86_64](https://en.wikipedia.org/wiki/X86-64) 架构，来自于 [arch/x86/include/asm/spinlock.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/spinlock.h) 文件的 `arch_spin_.*` 前缀的函数调用。如果我们关注这个头文件，我们会发现这些函数(`arch_spin_is_locked`， `arch_spin_lock`， `arch_spin_unlock` 等等)只在 `CONFIG_QUEUED_SPINLOCKS` 内核配置选项禁用的时才定义：

```c
#ifdef CONFIG_QUEUED_SPINLOCKS
#include <asm/qspinlock.h>
#else
static __always_inline void arch_spin_lock(arch_spinlock_t *lock)
{
    ...
    ...
    ...
}
...
...
...
#endif
```
这意味着 [arch/x86/include/asm/qspinlock.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/qspinlock.h) 这个头文件提供提供这些函数自己的实现。实际上这些函数是宏定义并且在分布在其他头文件中。这个头文件是 [include/asm-generic/qspinlock.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/qspinlock.h#L126)。如果我们查看这个头文件，我们会发现这些宏的定义：

```c
#define arch_spin_is_locked(l)          queued_spin_is_locked(l)
#define arch_spin_is_contended(l)       queued_spin_is_contended(l)
#define arch_spin_value_unlocked(l)     queued_spin_value_unlocked(l)
#define arch_spin_lock(l)               queued_spin_lock(l)
#define arch_spin_trylock(l)            queued_spin_trylock(l)
#define arch_spin_unlock(l)             queued_spin_unlock(l)
#define arch_spin_lock_flags(l, f)      queued_spin_lock(l)
#define arch_spin_unlock_wait(l)        queued_spin_unlock_wait(l)
```

在我们考虑怎么排列自旋锁和实现他们的 [API](https://en.wikipedia.org/wiki/Application_programming_interface)，我们首先看看理论部分。

## 3.1. 介绍队列自旋锁

队列自旋锁是 Linux 内核的[锁机制](https://en.wikipedia.org/wiki/Lock_%28computer_science%29)，是标准`自旋锁`的代替物。至少对 [x86_64](https://en.wikipedia.org/wiki/X86-64) 架构是真的。如果我们查看了以下内核配置文件 - [kernel/Kconfig.locks](https://github.com/torvalds/linux/blob/master/kernel/Kconfig.locks)，我们将会发现以下配置入口：

```
config ARCH_USE_QUEUED_SPINLOCKS
	bool

config QUEUED_SPINLOCKS
	def_bool y if ARCH_USE_QUEUED_SPINLOCKS
	depends on SMP
```
这意味着如果 `ARCH_USE_QUEUED_SPINLOCKS` 启用，那么 `CONFIG_QUEUED_SPINLOCKS` 内核配置选项将默认启用。 我们能够看到 `ARCH_USE_QUEUED_SPINLOCKS` 在 `x86_64` 特定内核配置文件 - [arch/x86/Kconfig](https://github.com/torvalds/linux/blob/master/arch/x86/Kconfig) **默认开启**：

```
config X86
    ...
    ...
    ...
    select ARCH_USE_QUEUED_SPINLOCKS
    ...
    ...
    ...
```

在开始考虑什么是队列自旋锁概念之前，让我们看看其他`自旋锁`的类型。一开始我们考虑`正常`自旋锁是如何实现的。通常，`正常`自旋锁的实现是基于 [test and set](https://en.wikipedia.org/wiki/Test-and-set) 指令。这个指令的工作原则真的很简单。该指令写入一个值到内存地址然后返回该地址原来的旧值。这些操作都是在原子的上下文中完成的。也就是说，这个指令是不可中断的。因此如果第一个线程开始执行这个指令，第二个线程将会等待，直到第一个线程完成。基本锁可以在这个机制之上建立。可能看起来如下所示：

```C
int lock(lock)
{
    while (test_and_set(lock) == 1)
        ;
    return 0;
}

int unlock(lock)
{
    lock=0;

    return lock;
}
```

* `排队自旋锁(ticket spinlock)`解决了不公平问题而且能够保证想要获取锁的线程的顺序，单会导致导致缓存失效问题；
* `队列自旋锁(queue spinlock)`解决不公平问题和缓存失效问题；

第一个线程将执行 `test_and_set` 指令设置 `lock` 为 `1`。当第二个线程调用 `lock` 函数，它将在 `while` 循环中自旋，直到第一个线程调用 `unlock` 函数而且 `lock` 等于 `0`。这个实现对于执行不是很好，因为该实现至少有两个问题。第一个问题是该实现可能是非公平的而且一个处理器的线程可能有很长的等待时间，即使有其他线程也在等待释放锁，它还是调用了 `lock`。第二个问题是所有想要获取锁的线程，必须在共享内存的变量上执行很多类似`test_and_set` 这样的`原子`操作。这导致缓存失效，因为处理器缓存会存储 `lock=1`，但是在线程释放锁之后，内存中 `lock`可能只是`1`。

在上一[部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-1.html) 我们了解了自旋锁的第二种实现 -
`排队自旋锁(ticket spinlock)`。这一方法解决了第一个问题而且能够保证想要获取锁的线程的顺序，但是仍然存在第二个问题。

这一部分的主旨是 `队列自旋锁`。这个方法能够帮助解决上述的两个问题。`队列自旋锁`允许每个处理器对自旋过程使用他自己的内存地址。通过学习名为 [MCS](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf) 锁的这种基于队列自旋锁的实现，能够最好理解基于队列自旋锁的基本原则。在了解`队列自旋锁`的实现之前，我们先尝试理解什么是 `MCS` 锁。

**`MCS`锁**的基本理念就在上一段已经写到了，一个线程在本地变量上自旋然后每个系统的处理器自己拥有这些变量的拷贝。换句话说这个概念建立在 Linux 内核中的 [per-cpu](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html) 变量概念之上。

当第一个线程想要获取锁，线程在`队列`中注册了自身，或者换句话说，因为线程现在是闲置的，线程要加入特殊`队列`并且获取锁。当第二个线程想要在第一个线程释放锁之前获取相同锁，这个线程就会**把他自身的锁变量的拷贝加入到这个特殊`队列`中**。这个例子中第一个线程会包含一个 `next` 字段指向第二个线程。从这一时刻，第二个线程会等待直到第一个线程释放它的锁并且关于这个事件通知给 `next` 线程。第一个线程从`队列`中删除而第二个线程持有该锁。

```c
typedef struct qspinlock {
	union {
		atomic_t val;

		/*
		 * By using the whole 2nd least significant byte for the
		 * pending bit, we can allow better optimization of the lock
		 * acquisition for the pending bit holder.
		 */
#ifdef __LITTLE_ENDIAN
		struct {
			u8	locked;
			u8	pending;
		};
		struct {
			u16	locked_pending;
			u16	tail;
		};
#else
		struct {
			u16	tail;
			u16	locked_pending;
		};
		struct {
			u8	reserved[2];
			u8	pending;
			u8	locked;
		};
#endif
	};
} arch_spinlock_t;
```

我们可以这样代表示意一下：

空队列：

```
+---------+
|         |
|  Queue  |
|         |
+---------+
```

第一个线程尝试获取锁：

```
+---------+     +----------------------------+
|         |     |                            |
|  Queue  |---->| First thread acquired lock |
|         |     |                            |
+---------+     +----------------------------+
```

第二个队列尝试获取锁:

```
+---------+     +----------------------------------------+     +-------------------------+
|         |     |                                        |     |                         |
|  Queue  |---->|  Second thread waits for first thread  |<----| First thread holds lock |
|         |     |                                        |     |                         |
+---------+     +----------------------------------------+     +-------------------------+
```

或者伪代码描述为：

```C
void lock(...)
{
    lock.next = NULL;
    ancestor = put_lock_to_queue_and_return_ancestor(queue, lock);

    // if we have ancestor, the lock already acquired and we
    // need to wait until it will be released
    if (ancestor)
    {
        lock.locked = 1;
        ancestor.next = lock;

        while (lock.is_locked == true)
            ;
    }

    // in other way we are owner of the lock and may exit
}

void unlock(...)
{
    // do we need to notify somebody or we are alonw in the
    // queue?
    if (lock.next != NULL) {
        // the while loop from the lock() function will be
        // finished
        lock.next.is_locked = false;
        // delete ourself from the queue and exit
        ...
        ...
        ...
        return;
    }

    // So, we have no next threads in the queue to notify about
    // lock releasing event. Let's just put `0` to the lock, will
    // delete ourself from the queue and exit.
}
```

想法很简单，但是`队列自旋锁`的实现一定是比伪代码复杂。就如同我上面写到的，`队列自旋锁`机制计划在 Linux 内核中成为`排队自旋锁`的替代品。但你们可能还记得，常用`自旋锁`适用于`32位(32-bit)`的 [字(word)](https://en.wikipedia.org/wiki/Word_%28computer_architecture%29)。而基于`MCS`的锁不能使用这个大小，你们可能知道 `spinlock_t` 类型在 Linux 内核中的使用是[宽字符(widely)](http://lxr.free-electrons.com/ident?i=spinlock_t)的。这种情况下可能不得不重写 Linux 内核中重要的组成部分，但这是不可接受的。除了这一点，一些包含自旋锁用于保护的内核结构不能增长大小。但无论怎样，基于这一概念的 Linux 内核中的`队列自旋锁`实现有一些修改，可以适应`32`位的字。

这就是所有有关`队列自旋锁`的理论，现在让我们考虑以下在 Linux 内核中这个机制是如何实现的。`队列自旋锁`的实现看起来比`排队自旋锁`的实现更加复杂和混乱，但是细致的研究会引导成功。

## 3.2. 队列自旋锁的API

现在我们从原理角度了解了一些`队列自旋锁`，是时候了解 Linux 内核中这一机制的实现了。就像我们之前了解的那样 [include/asm-generic/qspinlock.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/qspinlock.h#L126) 头文件提供一套宏，代表 API 中的自旋锁的获取、释放等等。

```C
#define arch_spin_is_locked(l)          queued_spin_is_locked(l)
#define arch_spin_is_contended(l)       queued_spin_is_contended(l)
#define arch_spin_value_unlocked(l)     queued_spin_value_unlocked(l)
#define arch_spin_lock(l)               queued_spin_lock(l)
#define arch_spin_trylock(l)            queued_spin_trylock(l)
#define arch_spin_unlock(l)             queued_spin_unlock(l)
#define arch_spin_lock_flags(l, f)      queued_spin_lock(l)
#define arch_spin_unlock_wait(l)        queued_spin_unlock_wait(l)
```

所有这些宏扩展了同一头文件下的函数的调用。此外，我们发现 [include/asm-generic/qspinlock_types.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/qspinlock_types.h) 头文件的 `qspinlock` 结构代表了 Linux 内核队列自旋锁。

```C
typedef struct qspinlock {
	atomic_t	val;
} arch_spinlock_t;
```

如我们所了解的，`qspinlock` 结构只包含了一个字段 - `val`。这个字段代表给定`自旋锁`的状态。`4` 个字节字段包括如下 4 个部分：

* `0-7` - 上锁字节(locked byte);
* `8` - 未决位(pending bit);
* `16-17` - 这两位代表了 `MCS` 锁的 `per_cpu` 数组(马上就会了解)；
* `18-31` - 包括表明队列尾部的处理器数。

`9-15` 字节没有被使用。

就像我们已经知道的，系统中每个处理器有自己的锁拷贝。这个锁由以下结构所表示：

```C
struct mcs_spinlock {
       struct mcs_spinlock *next;
       int locked;
       int count;
};
```

来自 [kernel/locking/mcs_spinlock.h](https://github.com/torvalds/linux/blob/master/kernel/locking/mcs_spinlock.h) 头文件。第一个字段代表了指向`队列`中下一个线程的指针。第二个字段代表了`队列`中当前线程的状态，其中 `1` 是 `锁`已经获取而 `0` 相反。然后最后一个 `mcs_spinlock` 字段 结构代表嵌套锁 (nested locks)，了解什么是嵌套锁，就像想象一下当线程已经获取锁的情况，而被硬件[中断](https://en.wikipedia.org/wiki/Interrupt) 所中断，然后[中断处理程序](https://en.wikipedia.org/wiki/Interrupt_handler)又尝试获取锁。这个例子里，每个处理器不只是 `mcs_spinlock` 结构的拷贝，也是这些结构的数组：

```C
static DEFINE_PER_CPU_ALIGNED(struct mcs_spinlock, mcs_nodes[4]);
```

此数组允许以下情况的四个事件的锁获取的四个尝试(原文：This array allows to make four attempts of a lock acquisition for the four events in following contexts:
)：

* 普通任务上下文；
* 硬件中断上下文；
* 软件中断上下文；
* 屏蔽中断上下文。

现在让我们返回 `qspinlock` 结构和`队列自旋锁`的 `API` 中来。在我们考虑`队列自旋锁`的 `API` 之前，请注意 `qspinlock` 结构的 `val` 字段有类型 - `atomic_t`，此类型代表原子变量或者变量的一次操作(原文：one operation at a time variable)。一次，所有这个字段的操作都是[原子的](https://en.wikipedia.org/wiki/Linearizability)。比如说让我们看看 `val` API 的值：

```C
static __always_inline int queued_spin_is_locked(struct qspinlock *lock)
{
	return atomic_read(&lock->val);
}
```

Ok，现在我们知道 Linux 内核的代表队列自旋锁数据结构，那么是时候看看`队列自旋锁`[API](https://en.wikipedia.org/wiki/Application_programming_interface)中`主要（main）`函数的实现。

```C
#define arch_spin_lock(l)               queued_spin_lock(l)
```

没错，这个函数是 - `queued_spin_lock`。正如我们可能从函数名中所了解的一样，函数允许通过线程获取锁。这个函数在 [include/asm-generic/qspinlock_types.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/qspinlock_types.h) 头文件中定义，它的实现看起来是这样：

```C
static __always_inline void queued_spin_lock(struct qspinlock *lock)
{
        u32 val;

        val = atomic_cmpxchg_acquire(&lock->val, 0, _Q_LOCKED_VAL);
        if (likely(val == 0))
                 return;
        queued_spin_lock_slowpath(lock, val);
}
```

看起来很简单，除了 `queued_spin_lock_slowpath` 函数，我们可能发现它只有一个参数。在我们的例子中这个参数代表 `队列自旋锁` 被上锁。让我们考虑`队列`锁为空，现在第一个线程想要获取锁的情况。正如我们可能了解的 `queued_spin_lock` 函数从调用 `atomic_cmpxchg_acquire` 宏开始。就像你们可能从宏的名字猜到的那样，它执行原子的 [CMPXCHG](http://x86.renejeschke.de/html/file_module_x86_id_41.html) 指令，使用第一个参数（当前给定自旋锁的状态）比较第二个参数（在我们的例子为零）的值，如果他们相等，那么第二个参数在存储位置保存 `_Q_LOCKED_VAL` 的值，该存储位置通过 `&lock->val` 指向并且返回这个存储位置的初始值。

`atomic_cmpxchg_acquire` 宏定义在 [include/linux/atomic.h](https://github.com/torvalds/linux/blob/master/include/linux/atomic.h) 头文件中并且扩展了 `atomic_cmpxchg` 函数的调用：

```C
#define  atomic_cmpxchg_acquire         atomic_cmpxchg
```

这实现是架构所指定的。我们考虑 [x86_64](https://en.wikipedia.org/wiki/X86-64) 架构，因此在我们的例子中这个头文件在 [arch/x86/include/asm/atomic.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/atomic.h) 并且`atomic_cmpxchg` 函数的实现只是返回 `cmpxchg` 宏的结果：

```C
static __always_inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
        return cmpxchg(&v->counter, old, new);
}
```

这个宏在[arch/x86/include/asm/cmpxchg.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/cmpxchg.h)头文件中定义，看上去是这样：

```C
#define cmpxchg(ptr, old, new) \
    __cmpxchg(ptr, old, new, sizeof(*(ptr)))

#define __cmpxchg(ptr, old, new, size) \
    __raw_cmpxchg((ptr), (old), (new), (size), LOCK_PREFIX)
```

就像我们可能了解的那样，`cmpxchg` 宏使用几乎相同的参数集合扩展了 `__cpmxchg` 宏。新添加的参数是原子值的大小。`__cpmxchg` 宏添加了 `LOCK_PREFIX`，还扩展了 `__raw_cmpxchg` 宏中 `LOCK_PREFIX`的 [LOCK](http://x86.renejeschke.de/html/file_module_x86_id_159.html)指令。毕竟 `__raw_cmpxchg` 对我们来说做了所有的的工作：

```C
#define __raw_cmpxchg(ptr, old, new, size, lock) \
({
    ...
    ...
    ...
    volatile u32 *__ptr = (volatile u32 *)(ptr);            \
    asm volatile(lock "cmpxchgl %2,%1"                      \
                 : "=a" (__ret), "+m" (*__ptr)              \
                 : "r" (__new), "" (__old)                  \
                 : "memory");                               \
    ...
    ...
    ...
})
```

在 `atomic_cmpxchg_acquire` 宏被执行后，该宏返回内存地址之前的值。现在只有一个线程尝试获取锁，因此 `val` 将会置为零然后我们从 `queued_spin_lock` 函数返回：

```C
val = atomic_cmpxchg_acquire(&lock->val, 0, _Q_LOCKED_VAL);
if (likely(val == 0))
    return;
```

此时此刻，我们的第一个线程持有锁。注意这个行为与在 `MCS` 算法的描述有所区别。线程获取锁，但是我们不添加此线程入`队列`。就像我之前已经写到的，`队列自旋锁` 概念的实现在 Linux 内核中基于 `MCS` 算法，但是于此同时它对优化目的有一些差异。

所以第一个线程已经获取了锁然后现在让我们考虑第二个线程尝试获取相同的锁的情况。第二个线程将从同样的 `queued_spin_lock` 函数开始，但是 `lock->val` 会包含 `1` 或者 `_Q_LOCKED_VAL`，因为第一个线程已经持有了锁。因此，在本例中 `queued_spin_lock_slowpath` 函数将会被调用。`queued_spin_lock_slowpath`函数定义在 [kernel/locking/qspinlock.c](https://github.com/torvalds/linux/blob/master/kernel/locking/qspinlock.c) 源码文件中并且从以下的检查开始：

```C
/**
 * queued_spin_lock_slowpath - acquire the queued spinlock
 * @lock: Pointer to queued spinlock structure
 * @val: Current value of the queued spinlock 32-bit word
 *
 * (queue tail, pending bit, lock value)
 *
 *              fast     :    slow                                  :    unlock
 *                       :                                          :
 * uncontended  (0,0,0) -:--> (0,0,1) ------------------------------:--> (*,*,0)
 *                       :       | ^--------.------.             /  :
 *                       :       v           \      \            |  :
 * pending               :    (0,1,1) +--> (0,1,0)   \           |  :
 *                       :       | ^--'              |           |  :
 *                       :       v                   |           |  :
 * uncontended           :    (n,x,y) +--> (n,0,0) --'           |  :
 *   queue               :       | ^--'                          |  :
 *                       :       v                               |  :
 * contended             :    (*,x,y) +--> (*,0,0) ---> (*,0,1) -'  :
 *   queue               :         ^--'                             :
 */
void queued_spin_lock_slowpath(struct qspinlock *lock, u32 val)
{
	if (pv_enabled())
	    goto queue;

    if (virt_spin_lock(lock))
		return;

    ...
    ...
    ...
}
```

这些检查操作检查了 `pvqspinlock` 的状态。`pvqspinlock` 是在[准虚拟化/半虚拟化（paravirtualized）](https://en.wikipedia.org/wiki/Paravirtualization)环境中的`队列自旋锁`。就像这一章节只相关 Linux 内核同步原语一样，我们跳过这些和其他不直接相关本章节主题的部分。这些检查之后我们比较使用 `_Q_PENDING_VAL` 宏的值所代表的锁，然后什么都不做直到该比较为真（原文：After these checks we compare our value which represents lock with the value of the `_Q_PENDING_VAL` macro and do nothing while this is true）：

```C
if (val == _Q_PENDING_VAL) {
	while ((val = atomic_read(&lock->val)) == _Q_PENDING_VAL)
		cpu_relax();
}
```
这里 `cpu_relax` 只是 [NOP](https://en.wikipedia.org/wiki/NOP) 指令。综上，我们了解了锁饱含着 - `pending` 位。这个位代表了想要获取锁的线程，但是这个锁已经被其他线程获取了，并且与此同时`队列`为空。在本例中，`pending` 位将被设置并且`队列`不会被创建(touched)。这是优化所完成的，因为不需要考虑在引发缓存无效的自身 `mcs_spinlock` 数组的创建产生的非必需隐患（原文：This is done for optimization, because there are no need in unnecessary latency which will be caused by the cache invalidation in a touching of own `mcs_spinlock` array.）。

下一步我们进入下面的循环：

```C
for (;;) {
	if (val & ~_Q_LOCKED_MASK)
		goto queue;

	new = _Q_LOCKED_VAL;
	if (val == new)
		new |= _Q_PENDING_VAL;

	old = atomic_cmpxchg_acquire(&lock->val, val, new);
	if (old == val)
		break;

	val = old;
}
```

这里第一个 `if` 子句检查锁 (`val`) 的状态是上锁还是待定的(pending)。这意味着第一个线程已经获取了锁，第二个线程也试图获取锁，但现在第二个线程是待定状态。本例中我们需要开始建立队列。我们将稍后考虑这个情况。在我们的例子中，第一个线程持有锁而第二个线程也尝试获取锁。这个检查之后我们在上锁状态并且使用之前锁状态比较后创建新锁。就像你记得的那样，`val` 包含了 `&lock->val` 状态，在第二个线程调用 `atomic_cmpxchg_acquire` 宏后状态将会等于 `1`。由于 `new` 和 `val` 的值相等，所以我们在第二个线程的锁上设置待定位。在此之后，我们需要再次检查 `&lock->val` 的值，因为第一个线程可能在这个时候释放锁。如果第一个线程还又没释放锁，`旧`的值将等于 `val` （因为 `atomic_cmpxchg_acquire` 将会返回存储地址指向 `lock->val` 的值并且当前为 `1`）然后我们将退出循环。因为我们退出了循环，我们会等待第一个线程直到它释放锁，清除待定位，获取锁并且返回：

```C
smp_cond_acquire(!(atomic_read(&lock->val) & _Q_LOCKED_MASK));
clear_pending_set_locked(lock);
return;
```

注意我们还没创建`队列`。这里我们不需要，因为对于两个线程来说，队列只是导致对内存访问的非必需潜在因素。在其他的例子中，第一个线程可能在这个时候释放其锁。在本例中 `lock->val` 将包含 `_Q_LOCKED_VAL | _Q_PENDING_VAL` 并且我们会开始建立`队列`。通过获得处理器执行线程的本地 `mcs_nodes` 数组的拷贝我们开始建立`队列`：

```C
node = this_cpu_ptr(&mcs_nodes[0]);
idx = node->count++;
tail = encode_tail(smp_processor_id(), idx);
```

除此之外我们计算 表示`队列`尾部和代表 `mcs_nodes` 数组实体的`索引`的`tail` 。在此之后我们设置 `node` 指向正确的 `mcs_nodes` 数组，设置 `locked` 为零应为这个线程还没有获取锁，还有 `next` 为 `NULL` 因为我们不知道任何有关其他`队列`实体的信息：

```C
node += idx;
node->locked = 0;
node->next = NULL;
```

我们已经创建了对于执行当前线程想获取锁的处理器的队列的`每个 cpu（per-cpu）` 的拷贝，这意味着锁的拥有者可能在这个时刻释放了锁。因此我们可能通过 `queued_spin_trylock` 函数的调用尝试去再次获取锁。

```C
if (queued_spin_trylock(lock))
		goto release;
```

`queued_spin_trylock` 函数在 [include/asm-generic/qspinlock.h](https://github.com/torvalds/linux/blob/master/include/asm-generic/qspinlock.h) 头文件中被定义而且就像 `queued_spin_lock` 函数一样：

```C
static __always_inline int queued_spin_trylock(struct qspinlock *lock)
{
	if (!atomic_read(&lock->val) &&
	   (atomic_cmpxchg_acquire(&lock->val, 0, _Q_LOCKED_VAL) == 0))
		return 1;
	return 0;
}
```
如果锁成功被获取那么我们跳过`释放`标签而释放`队列`中的一个节点：

```C
release:
	this_cpu_dec(mcs_nodes[0].count);
```

现在我们不再需要它了，因为锁已经获得了。如果 `queued_spin_trylock` 不成功，我们更新队列的尾部：

```C
old = xchg_tail(lock, tail);
```

然后检索原先的尾部。下一步是检查`队列`是否为空。这个例子中我们需要用新的实体链接之前的实体：

```C
if (old & _Q_TAIL_MASK) {
	prev = decode_tail(old);
	WRITE_ONCE(prev->next, node);

    arch_mcs_spin_lock_contended(&node->locked);
}
```

队列实体链接之后，我们开始等待直到队列的头部到来。由于我们等待头部，我们需要对可能在这个等待实践加入的新的节点做一些检查：

```C
next = READ_ONCE(node->next);
if (next)
	prefetchw(next);
```

如果新节点被添加，我们从通过使用 [PREFETCHW](http://www.felixcloutier.com/x86/PREFETCHW.html) 指令指出下一个队列实体的内存中预先去除缓存线（cache line）。以优化为目的我们现在预先载入这个指针。我们只是改变了队列的头而这意味着有将要到来的 `MCS` 进行解锁操作并且下一个实体会被创建。

是的，从这个时刻我们在`队列`的头部。但是在我们有能力获取锁之前，我们需要至少等待两个事件：当前锁的拥有者释放锁和第二个线程处于`待定`位也获取锁：

```C
smp_cond_acquire(!((val = atomic_read(&lock->val)) & _Q_LOCKED_PENDING_MASK));
```

两个线程都释放锁后，`队列`的头部会持有锁。最后我们只是需要更新`队列`尾部然后移除从队列中移除头部。

以上。

## 3.3. 总结

这是 Linux 内核[同步原语](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)章节第二部分的结尾。在上一个[部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-1.html)我们已经见到了第一个同步原语`自旋锁`通过 Linux 内核 实现的`排队自旋锁（ticket spinlock）`。在这个部分我们了解了另一个`自旋锁`机制的实现 - `队列自旋锁`。下一个部分我们继续深入 Linux 内核同步原语。

如果您有疑问或者建议，请在twitter [0xAX](https://twitter.com/0xAX) 上联系我，通过 [email](anotherworldofworld@gmail.com) 联系我，或者创建一个 [issue](https://github.com/0xAX/linux-insides/issues/new).


## 3.4. 链接

* [spinlock](https://en.wikipedia.org/wiki/Spinlock)
* [interrupt](https://en.wikipedia.org/wiki/Interrupt)
* [interrupt handler](https://en.wikipedia.org/wiki/Interrupt_handler)
* [API](https://en.wikipedia.org/wiki/Application_programming_interface)
* [Test and Set](https://en.wikipedia.org/wiki/Test-and-set)
* [MCS](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf)
* [per-cpu variables](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html)
* [atomic instruction](https://en.wikipedia.org/wiki/Linearizability)
* [CMPXCHG instruction](http://x86.renejeschke.de/html/file_module_x86_id_41.html)
* [LOCK instruction](http://x86.renejeschke.de/html/file_module_x86_id_159.html)
* [NOP instruction](https://en.wikipedia.org/wiki/NOP)
* [PREFETCHW instruction](http://www.felixcloutier.com/x86/PREFETCHW.html)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [Previous part](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-1.html)


# 4. 信号量

这是本章的第三部分 [chapter](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/index.html)，本章描述了内核中的同步原语,在之前的部分我们见到了特殊的 [自旋锁](https://en.wikipedia.org/wiki/Spinlock) - `排队自旋锁`。 在更前的 [部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-2.html) 是和 `自旋锁` 相关的描述。我们将描述更多同步原语。

在 `自旋锁` 之后的下一个我们将要讲到的 [内核同步原语](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)是 [信号量](https://en.wikipedia.org/wiki/Semaphore_%28programming%29)。我们会从理论角度开始学习什么是 `信号量`， 然后我们会像前几章一样讲到Linux内核是如何实现信号量的。

好吧，现在我们开始。

## 4.1. 介绍Linux内核中的信号量

那么究竟什么是 `信号量` ？就像你可以猜到那样 - `信号量` 是另外一种支持线程或者进程的同步机制。Linux内核已经提供了一种同步机制 - `自旋锁`， 为什么我们还需要另外一种呢？为了回答这个问题，我们需要理解这两种机制。我们已经熟悉了 `自旋锁` ,因此我们从 `信号量` 机制开始。

**`自旋锁` 的设计理念是它仅会被持有非常短的时间**。 但持有自旋锁的时候我们**不可以进入睡眠模式**因为其他的进程在等待我们。为了防止 [死锁](https://en.wikipedia.org/wiki/Deadlock) [上下文交换](https://en.wikipedia.org/wiki/Context_switch) 也是不允许的。

当需要长时间持有一个锁的时候 [信号量](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) 就是一个很好的解决方案。从另一个方面看，这个机制对于需要短期持有锁的应用并不是最优。为了理解这个问题，我们需要知道什么是 `信号量`。

就像一般的同步原语，`信号量` 是基于变量的。这个变量可以变大或者减少，并且这个变量的状态代表了获取锁的能力。注意这个变量的值并不限于 `0` 和 `1`。有两种类型的 `信号量`：

* `二值信号量`;
* `普通信号量`.

第一种 `信号量` 的值可以为 `1` 或者 `0`。第二种 `信号量` 的值可以为任何非负数。如果 `信号量` 的值大于 `1` 那么它被叫做 `计数信号量`，并且它允许多于 `1` 个进程获取它。这种机制允许我们记录现有的资源，而 `自旋锁` 只允许我们为一个任务上锁。除了所有这些之外，另外一个重要的点是 `信号量` 允许进入睡眠状态。 另外当某进程在等待一个被其他进程获取的锁时， [调度器](https://en.wikipedia.org/wiki/Scheduling_%28computing%29) 也许会切换别的进程。

## 4.2. 信号量 API

因此，我们从理论方面了解一些 `信号量`的知识，我们来看看它在Linux内核中是如何实现的。所有 `信号量` 相关的 [API](https://en.wikipedia.org/wiki/Application_programming_interface) 都在名为 [include/linux/semaphore.h](https://github.com/torvalds/linux/blob/master/include/linux/semaphore.h) 的头文件中

我们看到 `信号量` 机制是有以下的结构体表示的：

```C
struct semaphore {
	raw_spinlock_t		lock;
	unsigned int		count;
	struct list_head	wait_list;
};
```

在内核中， `信号量` 结构体由三部分组成：

* `lock` - 保护 `信号量` 的 `自旋锁`;
* `count` - 现有资源的数量;
* `wait_list` - 等待获取此锁的进程序列.

在我们考虑Linux内核的的 `信号量` [API](https://en.wikipedia.org/wiki/Application_programming_interface) 之前，我们需要知道如何初始化一个 `信号量`。事实上， Linux内核提供了两个 `信号量` 的初始函数。这些函数允许初始化一个 `信号量` 为：

* `静态`;
* `动态`.

我们来看看第一个种初始化静态 `信号量`。我们可以使用 `DEFINE_SEMAPHORE` 宏将 `信号量` 静态初始化。

```C
#define DEFINE_SEMAPHORE(name)  \
         struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)
```

就像我们看到这样，`DEFINE_SEMAPHORE` 宏只提供了初始化 `二值` 信号量。 `DEFINE_SEMAPHORE` 宏展开到 `信号量` 结构体的定义。结构体通过 `__SEMAPHORE_INITIALIZER` 宏初始化。我们来看看这个宏的实现
```C
#define __SEMAPHORE_INITIALIZER(name, n)              \
{                                                                       \
    .lock           = __RAW_SPIN_LOCK_UNLOCKED((name).lock),        \
    .count          = n,                                            \
    .wait_list      = LIST_HEAD_INIT((name).wait_list),             \
}
```

`__SEMAPHORE_INITIALIZER` 宏传入了 `信号量` 结构体的名字并且初始化这个结构体的各个域。首先我们使用 `__RAW_SPIN_LOCK_UNLOCKED` 宏对给予的 `信号量` 初始化一个 `自旋锁`。就像你从 [之前](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-1.html) 的部分看到那样，`__RAW_SPIN_LOCK_UNLOCKED` 宏是在 [include/linux/spinlock_types.h](https://github.com/torvalds/linux/blob/master/include/linux/spinlock_types.h) 头文件中定义，它展开到 `__ARCH_SPIN_LOCK_UNLOCKED` 宏，而 `__ARCH_SPIN_LOCK_UNLOCKED` 宏又展开到零或者无锁状态

```C
#define __ARCH_SPIN_LOCK_UNLOCKED       { { 0 } }
```

 `信号量` 的最后两个域 `count` 和 `wait_list` 是通过现有资源的数量和空 [链表](https://xinqiu.gitbooks.io/linux-insides-cn/content/DataStructures/linux-datastructures-1.html)来初始化。
第二种初始化 `信号量` 的方式是将 `信号量` 和现有资源数目传送给 `sema_init` 函数。 这个函数是在 [include/linux/semaphore.h](https://github.com/torvalds/linux/blob/master/include/linux/semaphore.h) 头文件中定义的。

```C
static inline void sema_init(struct semaphore *sem, int val)
{
   static struct lock_class_key __key;
   *sem = (struct semaphore) __SEMAPHORE_INITIALIZER(*sem, val);
   lockdep_init_map(&sem->lock.dep_map, "semaphore->lock", &__key, 0);
}
```

我们来看看这个函数是如何实现的。它看起来很简单。函数使用我们刚看到的 `__SEMAPHORE_INITIALIZER` 宏对传入的 `信号量` 进行初始化。就像我们在之前 [部分](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/index.html) 写的那样，我们将会跳过Linux内核关于 [锁验证](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) 的部分。
从现在开始我们知道如何初始化一个 `信号量`，我们看看如何上锁和解锁。Linux内核提供了如下操作 `信号量` 的 [API](https://en.wikipedia.org/wiki/Application_programming_interface) 

```
void down(struct semaphore *sem);
void up(struct semaphore *sem);
int  down_interruptible(struct semaphore *sem);
int  down_killable(struct semaphore *sem);
int  down_trylock(struct semaphore *sem);
int  down_timeout(struct semaphore *sem, long jiffies);
```

前两个函数： `down` 和 `up` 是用来获取或释放 `信号量`。 `down_interruptible`函数试图去获取一个 `信号量`。如果被成功获取，`信号量` 的计数就会被减少并且锁也会被获取。同时当前任务也会被调度到受阻状态，也就是说 `TASK_INTERRUPTIBLE` 标志将会被至位。`TASK_INTERRUPTIBLE` 表示这个进程也许可以通过 [信号](https://en.wikipedia.org/wiki/Unix_signal) 退回到销毁状态。

`down_killable` 函数和 `down_interruptible` 函数提供类似的功能，但是它还将当前进程的 `TASK_KILLABLE` 标志置位。这表示等待的进程可以被杀死信号中断。

`down_trylock` 函数和 `spin_trylock` 函数相似。这个函数试图去获取一个锁并且退出如果这个操作是失败的。在这个例子中，想获取锁的进程不会等待。最后的 `down_timeout`函数试图去获取一个锁。当前进程将会被中断进入到等待状态当超过传入的可等待时间。除此之外你也许注意到，这个等待的时间是以 [jiffies](https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/linux-timers-1.html)计数。

我们刚刚看了 `信号量` [API](https://en.wikipedia.org/wiki/Application_programming_interface)的定义。我们从 `down` 函数开始看。这个函数是在 [kernel/locking/semaphore.c](https://github.com/torvalds/linux/blob/master/kernel/locking/semaphore.c) 源代码定义的。我们来看看函数实现：

```C
void down(struct semaphore *sem)
{
        unsigned long flags;

        raw_spin_lock_irqsave(&sem->lock, flags);
        if (likely(sem->count > 0))
                sem->count--;
        else
                __down(sem);
        raw_spin_unlock_irqrestore(&sem->lock, flags);
}
EXPORT_SYMBOL(down);
```

我们先看在 `down` 函数起始处定义的 `flags` 变量。这个变量将会传入到 `raw_spin_lock_irqsave` 和 `raw_spin_lock_irqrestore` 宏定义。这些宏是在 [include/linux/spinlock.h](https://github.com/torvalds/linux/blob/master/include/linux/spinlock.h)头文件定义的。这些宏用来保护当前 `信号量` 的计数器。事实上这两个宏的作用和 `spin_lock` 和 `spin_unlock` 宏相似。只不过这组宏会存储/重置当前中断标志并且禁止 [中断](https://en.wikipedia.org/wiki/Interrupt)。

就像你猜到那样， `down` 函数的主要就是通过 `raw_spin_lock_irqsave` 和 `raw_spin_unlock_irqrestore` 宏来实现的。我们通过将 `信号量` 的计数器和零对比，如果计数器大于零，我们可以减少这个计数器。这表示我们已经获取了这个锁。否则如果计数器是零，这表示所以的现有资源都已经被占用，我们需要等待以获取这个锁。就像我们看到那样， `__down` 函数将会被调用。
 `__down` 函数是在 [相同](https://github.com/torvalds/linux/blob/master/kernel/locking/semaphore.c))的源代码定义的，它的实现看起来如下：
```C
static noinline void __sched __down(struct semaphore *sem)
{
        __down_common(sem, TASK_UNINTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}
```

 `__down` 函数仅仅调用了 `__down_common` 函数，并且传入了三个参数

* `semaphore`;
* `flag` - 对当前任务;
* `timeout` - 最长等待 `信号量` 的时间.

在我们看  `__down_common` 函数之前，注意 `down_trylock`, `down_timeout` 和 `down_killable` 的实现也都是基于 `__down_common` 函数。

```C
static noinline int __sched __down_interruptible(struct semaphore *sem)
{
        return __down_common(sem, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}
```

`__down_killable` 函数：

```C
static noinline int __sched __down_killable(struct semaphore *sem)
{
        return __down_common(sem, TASK_KILLABLE, MAX_SCHEDULE_TIMEOUT);
}
```

`__down_timeout` 函数:

```C
static noinline int __sched __down_timeout(struct semaphore *sem, long timeout)
{
        return __down_common(sem, TASK_UNINTERRUPTIBLE, timeout);
}
```

现在我们来看看 `__down_common` 函数的实现。这个函数是在 [kernel/locking/semaphore.c](https://github.com/torvalds/linux/blob/master/kernel/locking/semaphore.c)源文件中定义的。这个函数的定义从以下两个本地变量开始。

```C
struct task_struct *task = current;
struct semaphore_waiter waiter;
```

第一个变量表示当前想获取本地处理器锁的任务。 `current` 宏是在 [arch/x86/include/asm/current.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/current.h) 头文件中定义的。

```C
#define current get_current()
```

`get_current` 函数返回 `current_task` [per-cpu](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html) 变量的值。


```C
DECLARE_PER_CPU(struct task_struct *, current_task);

static __always_inline struct task_struct *get_current(void)
{
        return this_cpu_read_stable(current_task);
}
```

第二个变量是 `waiter` 表示了一个 `semaphore.wait_list` 列表的入口：

```C
struct semaphore_waiter {
    struct list_head list;
    struct task_struct *task;
    bool up;
};
```

下一步我们将当前进程加入到 `wait_list` 并且在定义如下变量后填充 `waiter` 域

```C
list_add_tail(&waiter.list, &sem->wait_list);
waiter.task = current;
waiter.up = false;
```

下一步我们进入到如下的无限循环：

```C
for (;;) {
    if (signal_pending_state(state, task))
        goto interrupted;

    if (unlikely(timeout <= 0))
        goto timed_out;

    __set_task_state(task, state);

    raw_spin_unlock_irq(&sem->lock);
    timeout = schedule_timeout(timeout);
    raw_spin_lock_irq(&sem->lock);

    if (waiter.up)
        return 0;
}
```

在之前的代码中我们将 `waiter.up` 设置为 `false`。所以当 `up` 没有设置为 `true` 任务将会在这个无限循环中循环。这个循环从检查当前的任务是否处于 `pending` 状态开始，也就是说此任务的标志包含 `TASK_INTERRUPTIBLE` 或者 `TASK_WAKEKILL` 标志。我之前写到当一个任务在等待获取一个信号的时候任务也许可以被 [信号](https://en.wikipedia.org/wiki/Unix_signal) 中断。`signal_pending_state` 函数是在 [include/linux/sched.h](https://github.com/torvalds/linux/blob/master/include/linux/sched.h)原文件中定义的，它看起来如下：

```C
static inline int signal_pending_state(long state, struct task_struct *p)
{
     if (!(state & (TASK_INTERRUPTIBLE | TASK_WAKEKILL)))
             return 0;
     if (!signal_pending(p))
             return 0;

     return (state & TASK_INTERRUPTIBLE) || __fatal_signal_pending(p);
}
```
我们先会检测 `state` [位掩码](https://en.wikipedia.org/wiki/Mask_%28computing%29) 包含 `TASK_INTERRUPTIBLE` 或者 `TASK_WAKEKILL` 位，如果不包含这两个位，函数退出。下一步我们检测当前任务是否有一个挂起信号，如果没有挂起信号函数退出。最后我们就检测 `state` 位掩码的 `TASK_INTERRUPTIBLE` 位。如果，我们任务包含一个挂起信号，我们将会跳转到 `interrupted` 标签：

```C
interrupted:
    list_del(&waiter.list);
    return -EINTR;
```

在这个标签中，我们会删除等待锁的列表，然后返回 `-EINTR` [错误码](https://en.wikipedia.org/wiki/Errno.h)。 如果一个任务没有挂起信号，我们检测超时是否小于等于零。

```C
if (unlikely(timeout <= 0))
    goto timed_out;
```

我们跳转到 `timed_out` 标签：

```C
timed_out:
    list_del(&waiter.list);
    return -ETIME;
```

在这个标签里，我们继续做和 `interrupted` 一样的事情。我们将任务从锁等待者中删除，但是返回  `-ETIME` 错误码。如果一个任务没有挂起信号而且给予的超时也没有过期，当前的任务将会被设置为传入的 `state`：

```C
__set_task_state(task, state);
```

然后调用 `schedule_timeout` 函数：

```C
raw_spin_unlock_irq(&sem->lock);
timeout = schedule_timeout(timeout);
raw_spin_lock_irq(&sem->lock);
```

这个函数是在 [kernel/time/timer.c](https://github.com/torvalds/linux/blob/master/kernel/time/timer.c) 代码中定义的。`schedule_timeout` 函数将当前的任务置为休眠到设置的超时为止。 

这就是所有关于 `__down_common` 函数。如果一个函数想要获取一个已经被其它任务获取的锁，它将会转入到无限循环。并且它不能被信号中断，当前设置的超时不会过期或者当前持有锁的任务不释放它。现在我们来看看 `up` 函数的实现。

`up` 函数和 `down` 函数定义在[同一个](https://github.com/torvalds/linux/blob/master/kernel/locking/semaphore.c) 原文件。这个函数的主要功能是释放锁，这个函数看起来：

```C
void up(struct semaphore *sem)
{
    unsigned long flags;

    raw_spin_lock_irqsave(&sem->lock, flags);
    if (likely(list_empty(&sem->wait_list)))
            sem->count++;
    else
            __up(sem);
    raw_spin_unlock_irqrestore(&sem->lock, flags);
}
EXPORT_SYMBOL(up);
```

它看起来和 `down` 函数相似。这里有两个不同点。首先我们增加 `semaphore` 的计数。如果等待列表是空的，我们调用在当前原文件中定义的 `__up` 函数。如果等待列表不是空的，我们需要允许列表中的第一个任务去获取一个锁：

```C
static noinline void __sched __up(struct semaphore *sem)
{
        struct semaphore_waiter *waiter = list_first_entry(&sem->wait_list,
                                                struct semaphore_waiter, list);
        list_del(&waiter->list);
        waiter->up = true;
        wake_up_process(waiter->task);
}
```

在此我们获取待序列中的第一个任务，将它从列表中删除，将它的 `waiter-up` 设置为真。从此刻起 `__down_common` 函数中的无限循环将会被停止。 `wake_up_process` 函数将会在 `__up` 函数的结尾调用。我们从 `__down_common` 函数调用的 `schedule_timeout` 函数调用了  `schedule_timeout` 函数。`schedule_timeout` 函数将当前任务置于睡眠状态直到超时等待。现在我们进程也许会睡眠，我们需要唤醒。这就是为什么我们需要从 [kernel/sched/core.c](https://github.com/torvalds/linux/blob/master/kernel/sched/core.c) 源代码中调用 `wake_up_process` 函数

这就是所有的信息了。

## 4.3. 小结

这就是Linux内核中关于 [同步原语](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) 的第三部分的终结。在之前的两个部分，我们已经见到了第一个Linux内核的同步原语 `自旋锁`，它是使用 `ticket spinlock` 实现并且用于很短时间的锁。在这个部分我们见到了另外一种同步原语 － [信号量](https://en.wikipedia.org/wiki/Semaphore_%28programming%29)，信号量用于长时间的锁，因为它会导致 [上下文切换](https://en.wikipedia.org/wiki/Context_switch)。 在下一部分，我们将会继续深入Linux内核的同步原语并且讨论另一个同步原语 － [互斥量](https://en.wikipedia.org/wiki/Mutual_exclusion)。

如果你有问题或者建议，请在twitter [0xAX](https://twitter.com/0xAX)上联系我，通过 [email](anotherworldofworld@gmail.com)联系我，或者创建一个 [issue](https://github.com/MintCN/linux-insides-zh/issues/new)




## 4.4. 链接

* [spinlocks](https://en.wikipedia.org/wiki/Spinlock)
* [synchronization primitive](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)
* [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29)
* [context switch](https://en.wikipedia.org/wiki/Context_switch)
* [preemption](https://en.wikipedia.org/wiki/Preemption_%28computing%29)
* [deadlocks](https://en.wikipedia.org/wiki/Deadlock)
* [scheduler](https://en.wikipedia.org/wiki/Scheduling_%28computing%29)
* [Doubly linked list in the Linux kernel](https://xinqiu.gitbooks.io/linux-insides-cn/content/DataStructures/linux-datastructures-1.html)
* [jiffies](https://xinqiu.gitbooks.io/linux-insides-cn/content/Timers/linux-timers-1.html)
* [interrupts](https://en.wikipedia.org/wiki/Interrupt)
* [per-cpu](https://xinqiu.gitbooks.io/linux-insides-cn/content/Concepts/linux-cpu-1.html)
* [bitmask](https://en.wikipedia.org/wiki/Mask_%28computing%29)
* [SIGKILL](https://en.wikipedia.org/wiki/Unix_signal#SIGKILL)
* [errno](https://en.wikipedia.org/wiki/Errno.h)
* [API](https://en.wikipedia.org/wiki/Application_programming_interface)
* [mutex](https://en.wikipedia.org/wiki/Mutual_exclusion)
* [Previous part](https://xinqiu.gitbooks.io/linux-insides-cn/content/SyncPrim/linux-sync-2.html)



# 5. 互斥锁

This is the fourth part of the [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim) which describes synchronization primitives in the Linux kernel and in the previous parts we finished to consider different types [spinlocks](https://en.wikipedia.org/wiki/Spinlock) and [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) synchronization primitives. We will continue to learn [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) in this part and consider yet another one which is called - [mutex](https://en.wikipedia.org/wiki/Mutual_exclusion) which is stands for `MUTual EXclusion`.

As in all previous parts of this [book](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md), we will try to consider this synchronization primitive from the theoretical side and only than we will consider [API](https://en.wikipedia.org/wiki/Application_programming_interface) provided by the Linux kernel to manipulate with `mutexes`.

So, let's start.

## 5.1. 互斥锁的概念

We already familiar with the [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) synchronization primitive from the previous [part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-3). It represented by the:

```C
struct semaphore {
	raw_spinlock_t		lock;
	unsigned int		count;
	struct list_head	wait_list;
};
```

structure which holds information about state of a [lock](https://en.wikipedia.org/wiki/Lock_%28computer_science%29) and list of a lock waiters. Depends on the value of the `count` field, a `semaphore` can provide access to a resource of more than one wishing of this resource. The [mutex](https://en.wikipedia.org/wiki/Mutual_exclusion) concept is very similar to a [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) concept. But it has some differences. The main difference between `semaphore` and `mutex` synchronization primitive is that `mutex` has more strict semantic. Unlike a `semaphore`, only one [process](https://en.wikipedia.org/wiki/Process_%28computing%29) may hold `mutex` at one time and only the `owner` of a `mutex` may release or unlock it. Additional difference in implementation of `lock` [API](https://en.wikipedia.org/wiki/Application_programming_interface). The `semaphore` synchronization primitive forces rescheduling of processes which are in waiters list. The implementation of `mutex` lock `API` allows to avoid this situation and as a result expensive [context switches](https://en.wikipedia.org/wiki/Context_switch).

The `mutex` synchronization primitive represented by the following:

```C
struct mutex {
    atomic_t                count;
    spinlock_t              wait_lock;
    struct list_head        wait_list;
#if defined(CONFIG_DEBUG_MUTEXES) || defined(CONFIG_MUTEX_SPIN_ON_OWNER)
    struct task_struct      *owner;
#endif
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
    struct optimistic_spin_queue osq;
#endif
#ifdef CONFIG_DEBUG_MUTEXES
    void                    *magic;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map      dep_map;
#endif
};
```

structure in the Linux kernel. This structure is defined in the [include/linux/mutex.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/mutex.h) header file and contains similar to the `semaphore` structure set of fields. The first field of the `mutex` structure is - `count`. Value of this field represents state of a `mutex`. In a case when the value of the `count` field is `1`, a `mutex` is in `unlocked` state. When the value of the `count` field is `zero`, a `mutex` is in the `locked` state. Additionally value of the `count` field may be `negative`. In this case a `mutex` is in the `locked` state and has possible waiters.

The next two fields of the `mutex` structure - `wait_lock` and `wait_list` are [spinlock](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/mutex.h) for the protection of a `wait queue` and list of waiters which represents this `wait queue` for a certain lock. As you may notice, the similarity of the `mutex` and `semaphore` structures ends. Remaining fields of the `mutex` structure, as we may see depends on different configuration options of the Linux kernel.

The first field - `owner` represents [process](https://en.wikipedia.org/wiki/Process_%28computing%29) which acquired a lock. As we may see, existence of this field in the `mutex` structure depends on the `CONFIG_DEBUG_MUTEXES` or `CONFIG_MUTEX_SPIN_ON_OWNER` kernel configuration options. Main point of this field and the next `osq` fields is support of `optimistic spinning` which we will see later. The last two fields - `magic` and `dep_map` are used only in [debugging](https://en.wikipedia.org/wiki/Debugging) mode. The `magic` field is to storing a `mutex` related information for debugging and the second field - `lockdep_map` is for [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) of the Linux kernel.

Now, after we have considered the `mutex` structure, we may consider how this synchronization primitive works in the Linux kernel. As you may guess, a process which wants to acquire a lock, must to decrease value of the `mutex->count` if possible. And if a process wants to release a lock, it must to increase the same value. That's true. But as you may also guess, it is not so simple in the Linux kernel.

Actually, when a process try to acquire a `mutex`, there three possible paths:

* `fastpath`：没人占用这个锁;
* `midpath`：已经被加锁，使用MCS锁;
* `slowpath`：已经被加锁.

```c
/* optimistic spinning，乐观自旋，到底有多乐观呢？
 *  当发现锁被持有时，optimistic spinning相信持有者很快就能把锁释放，
 *  因此它选择自旋等待，而不是睡眠等待，这样也就能减少进程切换带来的开销了。
 *//*
MCS锁
-------------------------------------------------------------------------------- 上文中提到过Mutex在实现过程中，采用了optimistic spinning自旋等待机制， 这个机制的核心就是基于MCS锁机制来实现的；
MCS锁机制是由John Mellor Crummey和Michael Scott在论文中《algorithms for scalable synchronization on shared-memory multiprocessors》提出的，并以 他俩的名字来命名；
MCS锁机制要解决的问题是：在多CPU系统中，自旋锁都在同一个变量上进行自旋， 在获取锁时会将包含锁的cache line移动到本地CPU，这种cache-line bouncing会 很大程度影响性能；
MCS锁机制的核心思想：每个CPU都分配一个自旋锁结构体，自旋锁的申请者（per- CPU）在local-CPU变量上自旋，这些结构体组建成一个链表，申请者自旋等待前驱 节点释放该锁；
osq(optimistci spinning queue)是基于MCS算法的一个具体实现，并经过了迭代优化；
*/
```

which may be taken, depending on the current state of the `mutex`. The first path or `fastpath` is the fastest as you may understand from its name. Everything is easy in this case. Nobody acquired a `mutex`, so the value of the `count` field of the `mutex` structure may be directly decremented. In a case of unlocking of a `mutex`, the algorithm is the same. A process just increments the value of the `count` field of the `mutex` structure. Of course, all of these operations must be [atomic](https://en.wikipedia.org/wiki/Linearizability).

Yes, this looks pretty easy. But what happens if a process wants to acquire a `mutex` which is already acquired by other process? In this case, the control will be transferred to the second path - `midpath`. The `midpath` or `optimistic spinning` tries to [spin](https://en.wikipedia.org/wiki/Spinlock) with already familiar for us [MCS lock](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf) while the lock owner is running. This path will be executed only if there are no other processes ready to run that have higher priority. **This path is called `optimistic` because the waiting task will not be sleep and rescheduled. **This allows to avoid expensive [context switch](https://en.wikipedia.org/wiki/Context_switch).

> 之所以叫做乐观锁，是因为它相信很快就能获取锁，自旋等待，并且不会被调度出去。

In the last case, when the `fastpath` and `midpath` may not be executed, the last path - `slowpath` will be executed. This path acts like a [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) lock. If the lock is unable to be acquired by a process, this process will be added to `wait queue` which is represented by the following:

```C
struct mutex_waiter {
    struct list_head        list;
    struct task_struct      *task;
#ifdef CONFIG_DEBUG_MUTEXES
    void                    *magic;
#endif
};
```
5.10.13中多了一个字段：
```c
/*
 * This is the control structure for tasks blocked on mutex,
 * which resides on the blocked task's kernel stack:
 */
struct mutex_waiter {
	struct list_head	list;
	struct task_struct	*task;
	struct ww_acquire_ctx	*ww_ctx;
#ifdef CONFIG_DEBUG_MUTEXES
	void			*magic;
#endif
};
```

structure from the [include/linux/mutex.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/mutex.h) header file and will be sleep. Before we will consider [API](https://en.wikipedia.org/wiki/Application_programming_interface) which is provided by the Linux kernel for manipulation with `mutexes`, let's consider the `mutex_waiter` structure. If you have read the [previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-3) of this chapter, you may notice that the `mutex_waiter` structure is similar to the `semaphore_waiter` structure from the [kernel/locking/semaphore.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/semaphore.c) source code file:

```C
struct semaphore_waiter {
    struct list_head list;
    struct task_struct *task;
    bool up;
};
```

It also contains `list` and `task` fields which are represent entry of the mutex wait queue. The one difference here that the `mutex_waiter` does not contains `up` field, but contains the `magic` field which depends on the `CONFIG_DEBUG_MUTEXES` kernel configuration option and used to store a `mutex` related information for debugging purpose.

Now we know what is it `mutex` and how it is represented the Linux kernel. In this case, we may go ahead and start to look at the [API](https://en.wikipedia.org/wiki/Application_programming_interface) which the Linux kernel provides for manipulation of `mutexes`.

## 5.2. 互斥锁 API

Ok, in the previous paragraph we knew what is it `mutex` synchronization primitive and saw the `mutex` structure which represents `mutex` in the Linux kernel. Now it's time to consider [API](https://en.wikipedia.org/wiki/Application_programming_interface) for manipulation of mutexes. Description of the `mutex` API is located in the [include/linux/mutex.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/mutex.h) header file. As always, before we will consider how to acquire and release a `mutex`, we need to know how to initialize it.

There are two approaches to initialize a `mutex`. The first is to do it statically. For this purpose the Linux kernel provides following:

```C
#define DEFINE_MUTEX(mutexname) \
        struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)
```

macro. Let's consider implementation of this macro. As we may see, the `DEFINE_MUTEX` macro takes name for the `mutex` and expands to the definition of the new `mutex` structure. Additionally new `mutex` structure get initialized with the `__MUTEX_INITIALIZER` macro. Let's look at the implementation of the `__MUTEX_INITIALIZER`:

```C
#define __MUTEX_INITIALIZER(lockname)         \
{                                                             \
       .count = ATOMIC_INIT(1),                               \
       .wait_lock = __SPIN_LOCK_UNLOCKED(lockname.wait_lock), \
       .wait_list = LIST_HEAD_INIT(lockname.wait_list)        \
}
```

This macro is defined in the [same](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/mutex.h) header file and as we may understand it initializes fields of the `mutex` structure the initial values. The `count` field get initialized with the `1` which represents `unlocked` state of a mutex. The `wait_lock` [spinlock](https://en.wikipedia.org/wiki/Spinlock) get initialized to the unlocked state and the last field `wait_list` to empty [doubly linked list](https://0xax.gitbook.io/linux-insides/summary/datastructures/linux-datastructures-1).

The second approach allows us to initialize a `mutex` dynamically. To do this we need to call the `__mutex_init` function from the [kernel/locking/mutex.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/mutex.c) source code file. Actually, the `__mutex_init` function rarely called directly. Instead of the `__mutex_init`, the:

```C
# define mutex_init(mutex)                \
do {                                                    \
    static struct lock_class_key __key;             \
                                                    \
    __mutex_init((mutex), #mutex, &__key);          \
} while (0)
```

macro is used. We may see that the `mutex_init` macro just defines the `lock_class_key` and call the `__mutex_init` function. Let's look at the implementation of this function:

```C
void
__mutex_init(struct mutex *lock, const char *name, struct lock_class_key *key)
{
        atomic_set(&lock->count, 1);
        spin_lock_init(&lock->wait_lock);
        INIT_LIST_HEAD(&lock->wait_list);
        mutex_clear_owner(lock);
#ifdef CONFIG_MUTEX_SPIN_ON_OWNER
        osq_lock_init(&lock->osq);
#endif
        debug_mutex_init(lock, name, key);
}
```

As we may see the `__mutex_init` function takes three arguments:

* `lock` - a mutex itself;
* `name` - name of mutex for debugging purpose;
* `key`  - key for [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt).

At the beginning of the `__mutex_init` function, we may see initialization of the `mutex` state. We set it to `unlocked` state with the `atomic_set` function which atomically set the give variable to the given value. After this we may see initialization of the `spinlock` to the unlocked state which will protect `wait queue` of the `mutex` and initialization of the `wait queue` of the `mutex`. After this we clear owner of the `lock` and initialize optimistic queue by the call of the `osq_lock_init` function from the [include/linux/osq_lock.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/osq_lock.h) header file. This function just sets the tail of the optimistic queue to the unlocked state:

```C
static inline bool osq_is_locked(struct optimistic_spin_queue *lock)
{
   return atomic_read(&lock->tail) != OSQ_UNLOCKED_VAL;
}
```

In the end of the `__mutex_init` function we may see the call of the `debug_mutex_init` function, but as I already wrote in previous parts of this [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim), we will not consider debugging related stuff in this chapter.

After the `mutex` structure is initialized, we may go ahead and will look at the `lock` and `unlock` API of `mutex` synchronization primitive. Implementation of `mutex_lock` and `mutex_unlock` functions located in the [kernel/locking/mutex.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/mutex.c) source code file. First of all let's start from the implementation of the `mutex_lock`. It looks:

```C
void __sched mutex_lock(struct mutex *lock)
{
    might_sleep();
    __mutex_fastpath_lock(&lock->count, __mutex_lock_slowpath);
    mutex_set_owner(lock);
}
```

We may see the call of the `might_sleep` macro from the [include/linux/kernel.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/kernel.h) header file at the beginning of the `mutex_lock` function. Implementation of this macro depends on the `CONFIG_DEBUG_ATOMIC_SLEEP` kernel configuration option and if this option is enabled, this macro just prints a stack trace if it was executed in [atomic](https://en.wikipedia.org/wiki/Linearizability) context. This macro is helper for debugging purposes. In other way this macro does nothing.

After the `might_sleep` macro, we may see the call of the `__mutex_fastpath_lock` function. This function is architecture-specific and as we consider [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture in this book, the implementation of the `__mutex_fastpath_lock` is located in the [arch/x86/include/asm/mutex_64.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/mutex_64.h) header file. As we may understand from the name of the `__mutex_fastpath_lock` function, this function will try to acquire lock in a fast path or in other words this function will try to decrement the value of the `count` of the given mutex.

Implementation of the `__mutex_fastpath_lock` function consists from two parts. The first part is [inline assembly](https://0xax.gitbook.io/linux-insides/summary/theory/linux-theory-3) statement. Let's look at it:

```C
asm_volatile_goto(LOCK_PREFIX "   decl %0\n"
                              "   jns %l[exit]\n"
                              : : "m" (v->counter)
                              : "memory", "cc"
                              : exit);
```

First of all, let's pay attention to the `asm_volatile_goto`. This macro is defined in the [include/linux/compiler-gcc.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/compiler-gcc.h) header file and just expands to the two inline assembly statements:

```C
#define asm_volatile_goto(x...) do { asm goto(x); asm (""); } while (0)
```

可参见：[Linux Jump Label/static-key机制详解](https://rtoax.blog.csdn.net/article/details/115279591)。

The first assembly statement contains `goto` specificator and the second empty inline assembly statement is [barrier](https://en.wikipedia.org/wiki/Memory_barrier). Now let's return to the our inline assembly statement. As we may see it starts from the definition of the `LOCK_PREFIX` macro which just expands to the [lock](http://x86.renejeschke.de/html/file_module_x86_id_159.html) instruction:

```C
#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "
```

As we already know from the previous parts, this instruction allows to execute prefixed instruction [atomically](https://en.wikipedia.org/wiki/Linearizability). So, at the first step in the our assembly statement we try decrement value of the given `mutex->counter`. At the next step the [jns](http://unixwiz.net/techtips/x86-jumps.html) instruction will execute jump at the `exit` label if the value of the decremented `mutex->counter` is not negative. The `exit` label is the second part of the `__mutex_fastpath_lock` function and it just points to the exit from this function:

```C
exit:
        return;
```

For this moment he implementation of the `__mutex_fastpath_lock` function looks pretty easy. But the value of the `mutex->counter` may be negative after increment. In this case the: 

```C
fail_fn(v);
```

will be called after our inline assembly statement. The `fail_fn` is the second parameter of the `__mutex_fastpath_lock` function and represents pointer to function which represents `midpath/slowpath` paths to acquire the given lock. In our case the `fail_fn` is the `__mutex_lock_slowpath` function. Before we will look at the implementation of the `__mutex_lock_slowpath` function, let's finish with the implementation of the `mutex_lock` function. In the simplest way, the lock will be acquired successfully by a process and the `__mutex_fastpath_lock` will be finished. In this case, we just call the

```C
mutex_set_owner(lock);
```

in the end of the `mutex_lock`. The `mutex_set_owner` function is defined in the [kernel/locking/mutex.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/mutex.h) header file and just sets owner of a lock to the current process:

```C
static inline void mutex_set_owner(struct mutex *lock)
{
        lock->owner = current;
}
```

In other way, let's consider situation when a process which wants to acquire a lock is unable to do it, because another process already acquired the same lock. We already know that the `__mutex_lock_slowpath` function will be called in this case. Let's consider implementation of this function. This function is defined in the [kernel/locking/mutex.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/mutex.c) source code file and starts from the obtaining of the proper mutex by the mutex state given from the `__mutex_fastpath_lock` with the `container_of` macro:

```C
__visible void __sched
__mutex_lock_slowpath(atomic_t *lock_count)
{
        struct mutex *lock = container_of(lock_count, struct mutex, count);

        __mutex_lock_common(lock, TASK_UNINTERRUPTIBLE, 0,
                            NULL, _RET_IP_, NULL, 0);
}
```

and call the `__mutex_lock_common` function with the obtained `mutex`. The `__mutex_lock_common` function starts from [preemption](https://en.wikipedia.org/wiki/Preemption_%28computing%29) disabling until rescheduling:

```C
preempt_disable();
```

After this comes the stage of optimistic spinning. As we already know this stage depends on the `CONFIG_MUTEX_SPIN_ON_OWNER` kernel configuration option. If this option is disabled, we skip this stage and move at the last path - `slowpath` of a `mutex` acquisition:

```C
if (mutex_optimistic_spin(lock, ww_ctx, use_ww_ctx)) {
        preempt_enable();
        return 0;
}
```

First of all the `mutex_optimistic_spin` function check that we don't need to reschedule or in other words there are no other tasks ready to run that have higher priority. If this check was successful we need to update `MCS` lock wait queue with the current spin. In this way only one spinner can complete for the mutex at one time:

```C
osq_lock(&lock->osq)
```

At the next step we start to spin in the next loop:

```C
while (true) {
    owner = READ_ONCE(lock->owner);

    if (owner && !mutex_spin_on_owner(lock, owner))
        break;

    if (mutex_try_to_acquire(lock)) {
        lock_acquired(&lock->dep_map, ip);

        mutex_set_owner(lock);
        osq_unlock(&lock->osq);
        return true;
    }
}
```

and try to acquire a lock. First of all we try to take current owner and if the owner exists (it may not exists in a case when a process already released a mutex) and we wait for it in the `mutex_spin_on_owner` function before the owner will release a lock. If new task with higher priority have appeared during wait of the lock owner, we break the loop and go to sleep. In other case, the process already may release a lock, so we try to acquire a lock with the `mutex_try_to_acquired`. If this operation finished successfully, we set new owner for the given mutex, removes ourself from the `MCS` wait queue and exit from the `mutex_optimistic_spin` function. At this state a lock will be acquired by a process and we enable [preemption](https://en.wikipedia.org/wiki/Preemption_%28computing%29) and exit from the `__mutex_lock_common` function:

```C
if (mutex_optimistic_spin(lock, ww_ctx, use_ww_ctx)) {
    preempt_enable();
    return 0;
}

```

That's all for this case.

In other case all may not be so successful. For example new task may occur during we spinning in the loop from the `mutex_optimistic_spin` or even we may not get to this loop from the `mutex_optimistic_spin` in a case when there were task(s) with higher priority before this loop. Or finally the `CONFIG_MUTEX_SPIN_ON_OWNER` kernel configuration option disabled. In this case the `mutex_optimistic_spin` will do nothing:

```C
#ifndef CONFIG_MUTEX_SPIN_ON_OWNER
static bool mutex_optimistic_spin(struct mutex *lock,
                                  struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx)
{
    return false;
}
#endif
```

In all of these cases, the `__mutex_lock_common` function will acct like a `semaphore`. We try to acquire a lock again because the owner of a lock might already release a lock before this time:

```C
if (!mutex_is_locked(lock) &&
   (atomic_xchg_acquire(&lock->count, 0) == 1))
      goto skip_wait;
```

In a failure case the process which wants to acquire a lock will be added to the waiters list

```C
list_add_tail(&waiter.list, &lock->wait_list);
waiter.task = task;
```

In a successful case we update the owner of a lock, enable preemption and exit from the `__mutex_lock_common` function:

```C
skip_wait:
        mutex_set_owner(lock);
        preempt_enable();
        return 0; 
```

In this case a lock will be acquired. If can't acquire a lock for now, we enter into the following loop:

```C
for (;;) {

    if (atomic_read(&lock->count) >= 0 && (atomic_xchg_acquire(&lock->count, -1) == 1))
        break;

    if (unlikely(signal_pending_state(state, task))) {
        ret = -EINTR;
        goto err;
    } 

    __set_task_state(task, state);

     schedule_preempt_disabled();
}
```

where try to acquire a lock again and exit if this operation was successful. Yes, we try to acquire a lock again right after unsuccessful try  before the loop. We need to do it to make sure that we get a wakeup once a lock will be unlocked. Besides this, it allows us to acquire a lock after sleep.  In other case we check the current process for pending [signals](https://en.wikipedia.org/wiki/Unix_signal) and exit if the process was interrupted by a `signal` during wait for a lock acquisition. In the end of loop we didn't acquire a lock, so we set the task state for `TASK_UNINTERRUPTIBLE` and go to sleep with call of the `schedule_preempt_disabled` function.

That's all. We have considered all three possible paths through which a process may pass when it will wan to acquire a lock. Now let's consider how `mutex_unlock` is implemented. When the `mutex_unlock` will be called by a process which wants to release a lock, the `__mutex_fastpath_unlock` will be called from the  [arch/x86/include/asm/mutex_64.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/mutex_64.h)  header file:

```C
void __sched mutex_unlock(struct mutex *lock)
{
    __mutex_fastpath_unlock(&lock->count, __mutex_unlock_slowpath);
}
```

Implementation of the `__mutex_fastpath_unlock` function is very similar to the implementation of the `__mutex_fastpath_lock` function:

```C
static inline void __mutex_fastpath_unlock(atomic_t *v,
                                           void (*fail_fn)(atomic_t *))
{
       asm_volatile_goto(LOCK_PREFIX "   incl %0\n"
                         "   jg %l[exit]\n"
                         : : "m" (v->counter)
                         : "memory", "cc"
                         : exit);
       fail_fn(v);
exit:
       return;
}
```

Actually, there is only one difference. We increment value if the `mutex->count`. So it will represent `unlocked` state after this operation. As `mutex` released, but we have something in the `wait queue` we need to update it. In this case the `fail_fn` function will be called which is `__mutex_unlock_slowpath`. The `__mutex_unlock_slowpath` function just gets the correct `mutex` instance by the given `mutex->count` and calls the `__mutex_unlock_common_slowpath` function:

```C
__mutex_unlock_slowpath(atomic_t *lock_count)
{
      struct mutex *lock = container_of(lock_count, struct mutex, count);

      __mutex_unlock_common_slowpath(lock, 1);
}
```

In the `__mutex_unlock_common_slowpath` function we will get the first entry from the wait queue if the wait queue is not empty and wakeup related process:

```C
if (!list_empty(&lock->wait_list)) {
    struct mutex_waiter *waiter =
           list_entry(lock->wait_list.next, struct mutex_waiter, list); 
                wake_up_process(waiter->task);
}
```

After this, a mutex will be released by previous process and will be acquired by another process from a wait queue.

That's all. We have considered main `API` for manipulation with `mutexes`: `mutex_lock` and `mutex_unlock`. Besides this the Linux kernel provides following API:

* `mutex_lock_interruptible`;
* `mutex_lock_killable`;
* `mutex_trylock`.

and corresponding versions of `unlock` prefixed functions. This part will not describe this `API`, because it is similar to corresponding `API` of `semaphores`. More about it you may read in the [previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-3).

That's all.

## 5.3. 结论
This is the end of the fourth part of the [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) chapter in the Linux kernel. In this part we met with new synchronization primitive which is called - `mutex`. From the theoretical side, this synchronization primitive very similar on a [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29). Actually, `mutex` represents binary semaphore. But its implementation differs from the implementation of `semaphore` in the Linux kernel. In the next part we will continue to dive into synchronization primitives in the Linux kernel.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](mailto:anotherworldofworld@gmail.com) or just create [issue](https://github.com/0xAX/linux-insides/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

## 5.4. 链接

* [Mutex](https://en.wikipedia.org/wiki/Mutual_exclusion)
* [Spinlock](https://en.wikipedia.org/wiki/Spinlock)
* [Semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29)
* [Synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)
* [API](https://en.wikipedia.org/wiki/Application_programming_interface) 
* [Locking mechanism](https://en.wikipedia.org/wiki/Lock_%28computer_science%29)
* [Context switches](https://en.wikipedia.org/wiki/Context_switch)
* [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt)
* [Atomic](https://en.wikipedia.org/wiki/Linearizability)
* [MCS lock](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf)
* [Doubly linked list](https://0xax.gitbook.io/linux-insides/summary/datastructures/linux-datastructures-1)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [Inline assembly](https://0xax.gitbook.io/linux-insides/summary/theory/linux-theory-3)
* [Memory barrier](https://en.wikipedia.org/wiki/Memory_barrier)
* [Lock instruction](http://x86.renejeschke.de/html/file_module_x86_id_159.html)
* [JNS instruction](http://unixwiz.net/techtips/x86-jumps.html)
* [preemption](https://en.wikipedia.org/wiki/Preemption_%28computing%29)
* [Unix signals](https://en.wikipedia.org/wiki/Unix_signal)
* [Previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-3)


# 6. 读写信号量

This is the fifth part of the [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim) which describes synchronization primitives in the Linux kernel and in the previous parts we finished to consider different types [spinlocks](https://en.wikipedia.org/wiki/Spinlock), [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) and [mutex](https://en.wikipedia.org/wiki/Mutual_exclusion) synchronization primitives. We will continue to learn [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) in this part and start to consider special type of synchronization primitives - [readers–writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock).

The first synchronization primitive of this type will be already familiar for us - [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29). As in all previous parts of this [book](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md), before we will consider implementation of the `reader/writer semaphores` in the Linux kernel, we will start from the theoretical side and will try to understand what is the difference between `reader/writer semaphores` and `normal semaphores`.

So, let's start.

## 6.1. 介绍

Actually there are two types of operations may be performed on the data. We may read data and make changes in data. Two fundamental operations - `read` and `write`. Usually (but not always), `read` operation is performed more often than `write` operation. In this case, it would be logical to we may lock data in such way, that some processes may read locked data in one time, on condition that no one will not change the data. The [readers/writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock) allows us to get this lock.

When a process which wants to write something into data, all other `writer` and `reader` processes will be blocked until the process which acquired a lock, will not release it. When a process reads data, other processes which want to read the same data too, will not be locked and will be able to do this. As you may guess, implementation of the `reader/writer semaphore` is based on the implementation of the `normal semaphore`. We already familiar with the [semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29) synchronization primitive from the third [part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-4) of this chapter. From the theoretical side everything looks pretty simple. Let's look how `reader/writer semaphore` is represented in the Linux kernel.

The `semaphore` is represented by the:

```C
struct semaphore {
	raw_spinlock_t		lock;
	unsigned int		count;
	struct list_head	wait_list;
};
```

structure. If you will look in the [include/linux/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/rwsem.h) header file, you will find definition of the `rw_semaphore` structure which represents `reader/writer semaphore` in the Linux kernel. Let's look at the definition of this structure:

```C
#ifdef CONFIG_RWSEM_GENERIC_SPINLOCK
#include <linux/rwsem-spinlock.h>
#else
struct rw_semaphore {
        long count;
        struct list_head wait_list;
        raw_spinlock_t wait_lock;
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
        struct optimistic_spin_queue osq;
        struct task_struct *owner;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
        struct lockdep_map      dep_map;
#endif
};
```

Before we will consider fields of the `rw_semaphore` structure, we may notice, that declaration of the `rw_semaphore` structure depends on the `CONFIG_RWSEM_GENERIC_SPINLOCK` kernel configuration option. This option is disabled for the [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture by default. We can be sure in this by looking at the corresponding kernel configuration file. In our case, this configuration file is - [arch/x86/um/Kconfig](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/um/Kconfig):

```
config RWSEM_XCHGADD_ALGORITHM
	def_bool 64BIT

config RWSEM_GENERIC_SPINLOCK
	def_bool !RWSEM_XCHGADD_ALGORITHM
```

So, as this [book](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md) describes only [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture related stuff, we will skip the case when the `CONFIG_RWSEM_GENERIC_SPINLOCK` kernel configuration is enabled and consider definition of the `rw_semaphore` structure only from the [include/linux/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/rwsem.h) header file.

If we will take a look at the definition of the `rw_semaphore` structure, we will notice that first three fields are the same that in the `semaphore` structure. It contains `count` field which represents amount of available resources, the `wait_list` field which represents [doubly linked list](https://0xax.gitbook.io/linux-insides/summary/datastructures/linux-datastructures-1) of processes which are waiting to acquire a lock and `wait_lock` [spinlock](https://en.wikipedia.org/wiki/Spinlock) for protection of this list. Notice that `rw_semaphore.count` field is `long` type unlike the same field in the `semaphore` structure.

The `count` field of a `rw_semaphore` structure may have following values:

* `0x0000000000000000` - `reader/writer semaphore` is in unlocked state and no one is waiting for a lock;
* `0x000000000000000X` - `X` readers are active or attempting to acquire a lock and no writer waiting;
* `0xffffffff0000000X` - may represent different cases. The first is - `X` readers are active or attempting to acquire a lock with waiters for the lock. The second is - one writer attempting a lock, no waiters for the lock. And the last - one writer is active and no waiters for the lock;
* `0xffffffff00000001` - may represented two different cases. The first is - one reader is active or attempting to acquire a lock and exist waiters for the lock. The second case is one writer is active or attempting to acquire a lock and no waiters for the lock;
* `0xffffffff00000000` - represents situation when there are readers or writers are queued, but no one is active or is in the process of acquire of a lock;
* `0xfffffffe00000001` - a writer is active or attempting to acquire a lock and waiters are in queue.

So, besides the `count` field, all of these fields are similar to fields of the `semaphore` structure. Last three fields depend on the two configuration options of the Linux kernel: the `CONFIG_RWSEM_SPIN_ON_OWNER` and `CONFIG_DEBUG_LOCK_ALLOC`. The first two fields may be familiar us by declaration of the [mutex](https://en.wikipedia.org/wiki/Mutual_exclusion) structure from the [previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-4). The first `osq` field represents [MCS lock](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf) spinner for `optimistic spinning` and the second represents process which is current owner of a lock.

The last field of the `rw_semaphore` structure is - `dep_map` - debugging related, and as I already wrote in previous parts, we will skip debugging related stuff in this chapter.

That's all. Now we know a little about what is it `reader/writer lock` in general and `reader/writer semaphore` in particular. Additionally we saw how a `reader/writer semaphore` is represented in the Linux kernel. In this case, we may go ahead and start to look at the [API](https://en.wikipedia.org/wiki/Application_programming_interface) which the Linux kernel provides for manipulation of `reader/writer semaphores`.

## 6.2. 读写信号量 API

So, we know a little about `reader/writer semaphores` from theoretical side, let's look on its implementation in the Linux kernel. All `reader/writer semaphores` related [API](https://en.wikipedia.org/wiki/Application_programming_interface) is located in the [include/linux/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/rwsem.h) header file.

As always Before we will consider an [API](https://en.wikipedia.org/wiki/Application_programming_interface) of the `reader/writer semaphore` mechanism in the Linux kernel, we need to know how to initialize the `rw_semaphore` structure. As we already saw in previous parts of this [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim), all [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) may be initialized in two ways:

* `statically`;
* `dynamically`.

And `reader/writer semaphore` is not an exception. First of all, let's take a look at the first approach. We may initialize `rw_semaphore` structure with the help of the `DECLARE_RWSEM` macro in compile time. This macro is defined in the [include/linux/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/rwsem.h) header file and looks:

```C
#define DECLARE_RWSEM(name) \
        struct rw_semaphore name = __RWSEM_INITIALIZER(name)
```

As we may see, the `DECLARE_RWSEM` macro just expands to the definition of the `rw_semaphore` structure with the given name. Additionally new `rw_semaphore` structure is initialized with the value of the `__RWSEM_INITIALIZER` macro:

```C
#define __RWSEM_INITIALIZER(name)              \
{                                                              \
        .count = RWSEM_UNLOCKED_VALUE,                         \
        .wait_list = LIST_HEAD_INIT((name).wait_list),         \
        .wait_lock = __RAW_SPIN_LOCK_UNLOCKED(name.wait_lock)  \
         __RWSEM_OPT_INIT(name)                                \
         __RWSEM_DEP_MAP_INIT(name)
} 
```

and expands to the initialization of fields of `rw_semaphore` structure. First of all we initialize `count` field of the `rw_semaphore` structure to the `unlocked` state with `RWSEM_UNLOCKED_VALUE` macro from the [arch/x86/include/asm/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/rwsem.h) architecture specific header file:

```C
#define RWSEM_UNLOCKED_VALUE            0x00000000L
```

After this we initialize list of a lock waiters with the empty linked list and [spinlock](https://en.wikipedia.org/wiki/Spinlock) for protection of this list with the `unlocked` state too. The `__RWSEM_OPT_INIT` macro depends on the state of the `CONFIG_RWSEM_SPIN_ON_OWNER` kernel configuration option and if this option is enabled it expands to the initialization of the `osq` and `owner` fields of the `rw_semaphore` structure. As we already saw above, the `CONFIG_RWSEM_SPIN_ON_OWNER` kernel configuration option is enabled by default for [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture, so let's take a look at the definition of the `__RWSEM_OPT_INIT` macro:

```C
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
    #define __RWSEM_OPT_INIT(lockname) , .osq = OSQ_LOCK_UNLOCKED, .owner = NULL
#else
    #define __RWSEM_OPT_INIT(lockname)
#endif
```

As we may see, the `__RWSEM_OPT_INIT` macro initializes the [MCS lock](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf) lock with `unlocked` state and initial `owner` of a lock with `NULL`. From this moment, a `rw_semaphore` structure will be initialized in a compile time and may be used for data protection.

The second way to initialize a `rw_semaphore` structure is `dynamically` or use the `init_rwsem` macro from the [include/linux/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/rwsem.h) header file. This macro declares an instance of the `lock_class_key` which is related to the [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) of the Linux kernel and to the call of the `__init_rwsem` function with the given `reader/writer semaphore`:

```C
#define init_rwsem(sem)                         \
do {                                                            \
        static struct lock_class_key __key;                     \
                                                                \
        __init_rwsem((sem), #sem, &__key);                      \
} while (0)
```

If you will start definition of the `__init_rwsem` function, you will notice that there are couple of source code files which contain it. As you may guess, sometimes we need to initialize additional fields of the `rw_semaphore` structure, like the `osq` and `owner`. But sometimes not. All of this depends on some kernel configuration options. If we will look at the [kernel/locking/Makefile](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/Makefile) makefile, we will see following lines:

```Makefile
obj-$(CONFIG_RWSEM_GENERIC_SPINLOCK) += rwsem-spinlock.o
obj-$(CONFIG_RWSEM_XCHGADD_ALGORITHM) += rwsem-xadd.o
```

As we already know, the Linux kernel for `x86_64` architecture has enabled `CONFIG_RWSEM_XCHGADD_ALGORITHM` kernel configuration option by default:

```
config RWSEM_XCHGADD_ALGORITHM
	def_bool 64BIT
```

in the [arch/x86/um/Kconfig](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/um/Kconfig) kernel configuration file. In this case, implementation of the `__init_rwsem` function will be located in the [kernel/locking/rwsem.c](https://github.com/torvalds/linux/blob/master/kernel/locking/rwsem.c) source code file for us. Let's take a look at this function:

```C
void __init_rwsem(struct rw_semaphore *sem, const char *name,
                    struct lock_class_key *key)
{
#ifdef CONFIG_DEBUG_LOCK_ALLOC
        debug_check_no_locks_freed((void *)sem, sizeof(*sem));
        lockdep_init_map(&sem->dep_map, name, key, 0);
#endif
        sem->count = RWSEM_UNLOCKED_VALUE;
        raw_spin_lock_init(&sem->wait_lock);
        INIT_LIST_HEAD(&sem->wait_list);
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
        sem->owner = NULL;
        osq_lock_init(&sem->osq);
#endif
}
```

We may see here almost the same as in `__RWSEM_INITIALIZER` macro with difference that all of this will be executed in [runtime](https://en.wikipedia.org/wiki/Run_time_%28program_lifecycle_phase%29).

So, from now we are able to initialize a `reader/writer semaphore` let's look at the `lock` and `unlock` API. The Linux kernel provides following primary [API](https://en.wikipedia.org/wiki/Application_programming_interface) to manipulate `reader/writer semaphores`:

* `void down_read(struct rw_semaphore *sem)` - lock for reading;
* `int down_read_trylock(struct rw_semaphore *sem)` - try lock for reading;
* `void down_write(struct rw_semaphore *sem)` - lock for writing;
* `int down_write_trylock(struct rw_semaphore *sem)` - try lock for writing;
* `void up_read(struct rw_semaphore *sem)` - release a read lock;
* `void up_write(struct rw_semaphore *sem)` - release a write lock;

Let's start as always from the locking. First of all let's consider implementation of the `down_write` function which executes a try of acquiring of a lock for `write`. This function is [kernel/locking/rwsem.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/rwsem.c) source code file and starts from the call of the macro from the [include/linux/kernel.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/kernel.h) header file:

```C
void __sched down_write(struct rw_semaphore *sem)
{
        might_sleep();
        rwsem_acquire(&sem->dep_map, 0, 0, _RET_IP_);

        LOCK_CONTENDED(sem, __down_write_trylock, __down_write);
        rwsem_set_owner(sem);
}
```

We already met the `might_sleep` macro in the [previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-4). In short words, Implementation of the `might_sleep` macro depends on the `CONFIG_DEBUG_ATOMIC_SLEEP` kernel configuration option and if this option is enabled, this macro just prints a stack trace if it was executed in [atomic](https://en.wikipedia.org/wiki/Linearizability) context. As this macro is mostly for debugging purpose we will skip it and will go ahead. Additionally we will skip the next macro from the `down_read` function - `rwsem_acquire` which is related to the [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) of the Linux kernel, because this is topic of other part.

The only two things that remained in the `down_write` function is the call of the `LOCK_CONTENDED` macro which is defined in the [include/linux/lockdep.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/lockdep.h) header file and setting of owner of a lock with the `rwsem_set_owner` function which sets owner to currently running process:

```C
static inline void rwsem_set_owner(struct rw_semaphore *sem)
{
        sem->owner = current;
}
```

As you already may guess, the `LOCK_CONTENDED` macro does all job for us. Let's look at the implementation of the `LOCK_CONTENDED` macro:

```C
#define LOCK_CONTENDED(_lock, try, lock) \
        lock(_lock)
```

As we may see it just calls the `lock` function which is third parameter of the `LOCK_CONTENDED` macro with the given `rw_semaphore`. In our case the third parameter of the `LOCK_CONTENDED` macro is the `__down_write` function which is architecture specific function and located in the [arch/x86/include/asm/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/rwsem.h) header file. Let's look at the implementation of the `__down_write` function:

```C
static inline void __down_write(struct rw_semaphore *sem)
{
        __down_write_nested(sem, 0);
}
```

which just executes a call of the `__down_write_nested` function from the same source code file. Let's take a look at the implementation of the `__down_write_nested` function:

```C
static inline void __down_write_nested(struct rw_semaphore *sem, int subclass)
{
        long tmp;

        asm volatile("# beginning down_write\n\t"
                     LOCK_PREFIX "  xadd      %1,(%2)\n\t"
                     "  test " __ASM_SEL(%w1,%k1) "," __ASM_SEL(%w1,%k1) "\n\t"
                     "  jz        1f\n"
                     "  call call_rwsem_down_write_failed\n"
                     "1:\n"
                     "# ending down_write"
                     : "+m" (sem->count), "=d" (tmp)
                     : "a" (sem), "1" (RWSEM_ACTIVE_WRITE_BIAS)
                     : "memory", "cc");
}
```

As for other synchronization primitives which we saw in this chapter, usually `lock/unlock` functions consists only from an [inline assembly](https://0xax.gitbook.io/linux-insides/summary/theory/linux-theory-3) statement. As we may see, in our case the same for `__down_write_nested` function. Let's try to understand what does this function do. The first line of our assembly statement is just a comment, let's skip it. The second like contains `LOCK_PREFIX` which will be expanded to the [LOCK](http://x86.renejeschke.de/html/file_module_x86_id_159.html) instruction as we already know. The next [xadd](http://x86.renejeschke.de/html/file_module_x86_id_327.html) instruction executes `add` and `exchange` operations. In other words, `xadd` instruction adds value of the `RWSEM_ACTIVE_WRITE_BIAS`:

```C
#define RWSEM_ACTIVE_WRITE_BIAS         (RWSEM_WAITING_BIAS + RWSEM_ACTIVE_BIAS)

#define RWSEM_WAITING_BIAS              (-RWSEM_ACTIVE_MASK-1)
#define RWSEM_ACTIVE_BIAS               0x00000001L
```

or `0xffffffff00000001` to the `count` of the given `reader/writer semaphore` and returns previous value of it. After this we check the active mask in the `rw_semaphore->count`. If it was zero before, this means that there were no-one writer before, so we acquired a lock. In other way we call the `call_rwsem_down_write_failed` function from the [arch/x86/lib/rwsem.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/lib/rwsem.S) assembly file. The `call_rwsem_down_write_failed` function just calls the `rwsem_down_write_failed` function from the [kernel/locking/rwsem-xadd.c](https://github.com/torvalds/linux/blob/master/kernel/locking/rwsem.c) source code file anticipatorily save general purpose registers:

```assembly
ENTRY(call_rwsem_down_write_failed)
	FRAME_BEGIN
	save_common_regs
	movq %rax,%rdi
	call rwsem_down_write_failed
	restore_common_regs
	FRAME_END
	ret
    ENDPROC(call_rwsem_down_write_failed)
```

The `rwsem_down_write_failed` function starts from the [atomic](https://en.wikipedia.org/wiki/Linearizability) update of the `count` value:

```C
 __visible
struct rw_semaphore __sched *rwsem_down_write_failed(struct rw_semaphore *sem)
{
    count = rwsem_atomic_update(-RWSEM_ACTIVE_WRITE_BIAS, sem);
    ...
    ...
    ...
}
```

with the `-RWSEM_ACTIVE_WRITE_BIAS` value. The `rwsem_atomic_update` function is defined in the [arch/x86/include/asm/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/rwsem.h) header file and implement exchange and add logic:

```C
static inline long rwsem_atomic_update(long delta, struct rw_semaphore *sem)
{
        return delta + xadd(&sem->count, delta);
}
```

This function atomically adds the given delta to the `count` and returns old value of the count. After this it just returns sum of the given `delta` and old value of the `count` field. In our case we undo write bias from the `count` as we didn't acquire a lock. After this step we try to do `optimistic spinning` by the call of the `rwsem_optimistic_spin` function:

```C
if (rwsem_optimistic_spin(sem))
      return sem;
```

We will skip implementation of the `rwsem_optimistic_spin` function, as it is similar on the `mutex_optimistic_spin` function which we saw in the [previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-4). In short words we check existence other tasks ready to run that have higher priority in the `rwsem_optimistic_spin` function. If there are such tasks, the process will be added to the [MCS](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf) `waitqueue` and start to spin in the loop until a lock will be able to be acquired. If `optimistic spinning` is disabled, a process will be added to the and marked as waiting for write:

```C
waiter.task = current;
waiter.type = RWSEM_WAITING_FOR_WRITE;

if (list_empty(&sem->wait_list))
    waiting = false;

list_add_tail(&waiter.list, &sem->wait_list);
```

waiters list and start to wait until it will successfully acquire the lock. After we have added a process to the waiters list which was empty before this moment, we update the value of the `rw_semaphore->count` with the `RWSEM_WAITING_BIAS`:

```C
count = rwsem_atomic_update(RWSEM_WAITING_BIAS, sem);
```

with this we mark `rw_semaphore->counter` that it is already locked and exists/waits one `writer` which wants to acquire the lock. In other way we try to wake `reader` processes from the `wait queue` that were queued before this `writer` process and there are no active readers. In the end of the `rwsem_down_write_failed` a `writer` process will go to sleep which didn't acquire a lock in the following loop:

```C
while (true) {
    if (rwsem_try_write_lock(count, sem))
        break;
    raw_spin_unlock_irq(&sem->wait_lock);
    do {
        schedule();
        set_current_state(TASK_UNINTERRUPTIBLE);
    } while ((count = sem->count) & RWSEM_ACTIVE_MASK);
    raw_spin_lock_irq(&sem->wait_lock);
}
```

I will skip explanation of this loop as we already met similar functional in the [previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-4).

That's all. From this moment, our `writer` process will acquire or not acquire a lock depends on the value of the `rw_semaphore->count` field. Now if we will look at the implementation of the `down_read` function which executes a try of acquiring of a lock. We will see similar actions which we saw in the `down_write` function. This function calls different debugging and lock validator related functions/macros:

```C
void __sched down_read(struct rw_semaphore *sem)
{
        might_sleep();
        rwsem_acquire_read(&sem->dep_map, 0, 0, _RET_IP_);

        LOCK_CONTENDED(sem, __down_read_trylock, __down_read);
}
```

and does all job in the `__down_read` function. The `__down_read` consists of inline assembly statement:

```C
static inline void __down_read(struct rw_semaphore *sem)
{
         asm volatile("# beginning down_read\n\t"
                     LOCK_PREFIX _ASM_INC "(%1)\n\t"
                     "  jns        1f\n"
                     "  call call_rwsem_down_read_failed\n"
                     "1:\n\t"
                     "# ending down_read\n\t"
                     : "+m" (sem->count)
                     : "a" (sem)
                     : "memory", "cc");
}
```

which increments value of the given `rw_semaphore->count` and call the `call_rwsem_down_read_failed` if this value is negative. In other way we jump at the label `1:` and exit. After this `read` lock will be successfully acquired. Notice that we check a sign of the `count` value as it may be negative, because as you may remember most significant [word](https://en.wikipedia.org/wiki/Word_%28computer_architecture%29) of the `rw_semaphore->count` contains negated number of active writers.

Let's consider case when a process wants to acquire a lock for `read` operation, but it is already locked. In this case the `call_rwsem_down_read_failed` function from the [arch/x86/lib/rwsem.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/lib/rwsem.S)  assembly file will be called. If you will look at the implementation of this function, you will notice that it does the same that `call_rwsem_down_read_failed` function does. Except it calls the `rwsem_down_read_failed` function instead of `rwsem_dow_write_failed`. Now let's consider implementation of the `rwsem_down_read_failed` function. It starts from the adding a process to the `wait queue` and updating of value of the `rw_semaphore->counter`:

```C
long adjustment = -RWSEM_ACTIVE_READ_BIAS;

waiter.task = tsk;
waiter.type = RWSEM_WAITING_FOR_READ;

if (list_empty(&sem->wait_list))
    adjustment += RWSEM_WAITING_BIAS;
list_add_tail(&waiter.list, &sem->wait_list);

count = rwsem_atomic_update(adjustment, sem);
```

Notice that if the `wait queue` was empty before we clear the `rw_semaphore->counter` and undo `read` bias in other way. At the next step we check that there are no active locks and we are first in the `wait queue` we need to join currently active `reader` processes. In other way we go to sleep until a lock will not be able to acquired.

That's all. Now we know how `reader` and `writer` processes will behave in different cases during a lock acquisition. Now let's take a short look at `unlock` operations. The `up_read` and `up_write` functions allows us to unlock a `reader` or `writer` lock. First of all let's take a look at the implementation of the `up_write` function which is defined in the [kernel/locking/rwsem.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/locking/rwsem.c) source code file:

```C
void up_write(struct rw_semaphore *sem)
{
        rwsem_release(&sem->dep_map, 1, _RET_IP_);

        rwsem_clear_owner(sem);
        __up_write(sem);
}
```

First of all it calls the `rwsem_release` macro which is related to the lock validator of the Linux kernel, so we will skip it now. And at the next line the `rwsem_clear_owner` function which as you may understand from the name of this function, just clears the `owner` field of the given `rw_semaphore`:

```C
static inline void rwsem_clear_owner(struct rw_semaphore *sem)
{
	sem->owner = NULL;
}
```

The `__up_write` function does all job of unlocking of the lock. The `_up_write` is architecture-specific function, so for our case it will be located in the [arch/x86/include/asm/rwsem.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/rwsem.h) source code file. If we will take a look at the implementation of this function, we will see that it does almost the same that `__down_write` function, but conversely. Instead of adding of the `RWSEM_ACTIVE_WRITE_BIAS` to the `count`, we subtract the same value and check the `sign` of the previous value.

If the previous value of the `rw_semaphore->count` is not negative, a writer process released a lock and now it may be acquired by someone else. In other case, the `rw_semaphore->count` will contain negative values. This means that there is at least one `writer` in a wait queue. In this case the `call_rwsem_wake` function will be called. This function acts like similar functions which we already saw above. It store general purpose registers at the stack for preserving and call the `rwsem_wake` function.

First of all the `rwsem_wake` function checks if a spinner is present. In this case it will just acquire a lock which is just released by lock owner. In other case there must be someone in the `wait queue` and we need to wake or writer process if it exists at the top of the `wait queue` or all `reader` processes. The `up_read` function which release a `reader` lock acts in similar way like `up_write`, but with a little difference. Instead of subtracting of `RWSEM_ACTIVE_WRITE_BIAS` from the `rw_semaphore->count`, it subtracts `1` from it, because less significant word of the `count` contains number active locks. After this it checks `sign` of the `count` and calls the `rwsem_wake` like `__up_write` if the `count` is negative or in other way lock will be successfully released.

That's all. We have considered API for manipulation with `reader/writer semaphore`: `up_read/up_write` and `down_read/down_write`. We saw that the Linux kernel provides additional API, besides this functions, like the ``, `` and etc. But I will not consider implementation of these function in this part because it must be similar on that we have seen in this part of except few subtleties.

## 6.3. 结论
This is the end of the fifth part of the [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) chapter in the Linux kernel. In this part we met with special type of `semaphore` - `readers/writer` semaphore which provides access to data for multiply process to read or for one process to writer. In the next part we will continue to dive into synchronization primitives in the Linux kernel.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](mailto:anotherworldofworld@gmail.com) or just create [issue](https://github.com/0xAX/linux-insides/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

## 6.4. 链接

* [Synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29)
* [Readers/Writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock)
* [Spinlocks](https://en.wikipedia.org/wiki/Spinlock)
* [Semaphore](https://en.wikipedia.org/wiki/Semaphore_%28programming%29)
* [Mutex](https://en.wikipedia.org/wiki/Mutual_exclusion)
* [x86_64 architecture](https://en.wikipedia.org/wiki/X86-64)
* [Doubly linked list](https://0xax.gitbook.io/linux-insides/summary/datastructures/linux-datastructures-1)
* [MCS lock](http://www.cs.rochester.edu/~scott/papers/1991_TOCS_synch.pdf)
* [API](https://en.wikipedia.org/wiki/Application_programming_interface)
* [Linux kernel lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt)
* [Atomic operations](https://en.wikipedia.org/wiki/Linearizability)
* [Inline assembly](https://0xax.gitbook.io/linux-insides/summary/theory/linux-theory-3)
* [XADD instruction](http://x86.renejeschke.de/html/file_module_x86_id_327.html)
* [LOCK instruction](http://x86.renejeschke.de/html/file_module_x86_id_159.html)
* [Previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-4)


# 7. 顺序锁

> 顺序锁(seqlock)是对读写锁的一种优化, 提高了读锁和写锁的独立性。写锁不会被读锁阻塞，读锁也不会被写锁阻塞。写锁会被写锁阻塞。
> 若使用顺序锁,读执行单元绝对不会被写执行单元所阻塞,也就是说, 临界区可以在写临界区对被顺序锁保护的共享资源进行写操作的同时仍然可以继续读, 而不必等待写执行单元完成之后再去读,同样, 写执行单元也不必等待所有的读执行单元读完之后才去进行写操作。
> 但是写执行单元与写执行单元之间仍然是互斥的,即:如果有写执行单元正在进行写操作, 那么其它的写执行单元必须自旋在那里,直到写执行单元释放顺序锁为止。
> 如果读执行单元在读操作期间,写执行单元已经发生了写操作,那么, 读执行单元必须重新去读数据,以便确保读到的数据是完整的; 这种锁在读写操作同时进行的概率比较小,性能是非常好的,而且它允许读写操作同时进行, 因而更大地提高了并发性。
> 顺序锁有一个限制:它必须要求被保护的共享资源中不能含有指针; 因为写执行单元可能会使指针失效,当读执行单元如果正要访问该指针时,系统就会崩溃。

This is the sixth part of the chapter which describes [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_\(computer_science\)) in the Linux kernel and in the previous parts we finished to consider different [readers-writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock) synchronization primitives. We will continue to learn synchronization primitives in this part and start to consider a similar synchronization primitive which can be used to avoid the `writer starvation` problem. The name of this synchronization primitive is - `seqlock` or `sequential locks`.

We know from the previous [part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-5) that [readers-writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock) is a special lock mechanism which allows concurrent access for read-only operations, but an exclusive lock is needed for writing or modifying data. As we may guess, it may lead to a problem which is called `writer starvation`. In other words, a writer process can't acquire a lock as long as at least one reader process which acquired a lock holds it. So, in the situation when contention is high, it will lead to situation when a writer process which wants to acquire a lock will wait for it for a long time.

The `seqlock` synchronization primitive can help solve this problem.

As in all previous parts of this [book](https://github.com/0xAX/linux-insides/blob/master/SUMMARY.md), we will try to consider this synchronization primitive from the theoretical side and only than we will consider [API](https://en.wikipedia.org/wiki/Application_programming_interface) provided by the Linux kernel to manipulate with `seqlocks`.

So, let's start.

## 7.1. 顺序锁介绍

So, what is a `seqlock` synchronization primitive and how does it work? Let's try to answer on these questions in this paragraph. Actually `sequential locks` were introduced in the Linux kernel 2.6.x. Main point of this synchronization primitive is to **provide fast and lock-free access to shared resources**. Since the heart of `sequential lock` synchronization primitive is [spinlock](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-1) synchronization primitive, `sequential locks` work in situations where the protected resources are small and simple. Additionally write access must be rare and also should be fast. 

Work of this synchronization primitive is based on the sequence of events counter. Actually a `sequential lock` allows free access to a resource for readers, but each reader must check existence of conflicts with a writer. This synchronization primitive introduces a special counter. The main algorithm of work of `sequential locks` is simple: Each writer which acquired a sequential lock increments this counter and additionally acquires a [spinlock](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-1). When this writer finishes, it will release the acquired spinlock to give access to other writers and increment the counter of a sequential lock again.

Read only access works on the following principle, it gets the value of a `sequential lock` counter before it will enter into [critical section](https://en.wikipedia.org/wiki/Critical_section) and compares it with the value of the same `sequential lock` counter at the exit of critical section. If their values are equal, this means that there weren't writers for this period. If their values are not equal, this means that a writer has incremented the counter during the [critical section](https://en.wikipedia.org/wiki/Critical_section). This conflict means that reading of protected data must be repeated.

That's all. As we may see principle of work of `sequential locks` is simple.

```C
unsigned int seq_counter_value;

do {
    seq_counter_value = get_seq_counter_val(&the_lock);
    //
    // do as we want here
    //
} while (__retry__);
```

Actually the Linux kernel does not provide `get_seq_counter_val()` function. Here it is just a stub. Like a `__retry__` too. As I already wrote above, we will see actual the [API](https://en.wikipedia.org/wiki/Application_programming_interface) for this in the next paragraph of this part.

Ok, now we know what a `seqlock` synchronization primitive is and how it is represented in the Linux kernel. In this case, we may go ahead and start to look at the [API](https://en.wikipedia.org/wiki/Application_programming_interface) which the Linux kernel provides for manipulation of synchronization primitives of this type.

## 7.2. 顺序锁 API

So, now we know a little about `sequential lock` synchronization primitive from theoretical side, let's look at its implementation in the Linux kernel. All `sequential locks` [API](https://en.wikipedia.org/wiki/Application_programming_interface) are located in the [include/linux/seqlock.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/seqlock.h) header file.

First of all we may see that the a `sequential lock` mechanism is represented by the following type:

```C
typedef struct {
	struct seqcount seqcount;
	spinlock_t lock;
} seqlock_t;
```

As we may see the `seqlock_t` provides two fields. These fields represent a sequential lock counter, description of which we saw above and also a [spinlock](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-1) which will protect data from other writers. Note that the `seqcount` counter represented as `seqcount` type. The `seqcount` is structure:

```C
typedef struct seqcount {
	unsigned sequence;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map dep_map;
#endif
} seqcount_t;
```

which holds counter of a sequential lock and [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) related field.

As always in previous parts of this [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim), before we will consider an [API](https://en.wikipedia.org/wiki/Application_programming_interface) of `sequential lock` mechanism in the Linux kernel, we need to know how to initialize an instance of `seqlock_t`.

We saw in the previous parts that often the Linux kernel provides two approaches to execute initialization of the given synchronization primitive. The same situation with the `seqlock_t` structure. These approaches allows to initialize a `seqlock_t` in two following:

* `statically`;
* `dynamically`.

ways. Let's look at the first approach. We are able to initialize a `seqlock_t` statically with the `DEFINE_SEQLOCK` macro:

```C
#define DEFINE_SEQLOCK(x) \
		seqlock_t x = __SEQLOCK_UNLOCKED(x)
```

which is defined in the [include/linux/seqlock.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/seqlock.h) header file. As we may see, the `DEFINE_SEQLOCK` macro takes one argument and expands to the definition and initialization of the `seqlock_t` structure. Initialization occurs with the help of the `__SEQLOCK_UNLOCKED` macro which is defined in the same source code file. Let's look at the implementation of this macro:

```C
#define __SEQLOCK_UNLOCKED(lockname)			\
	{						\
		.seqcount = SEQCNT_ZERO(lockname),	\
		.lock =	__SPIN_LOCK_UNLOCKED(lockname)	\
	}
```

As we may see the, `__SEQLOCK_UNLOCKED` macro executes initialization of fields of the given `seqlock_t` structure. The first field is `seqcount` initialized with the `SEQCNT_ZERO` macro which expands to the:

```C
#define SEQCNT_ZERO(lockname) { .sequence = 0, SEQCOUNT_DEP_MAP_INIT(lockname)}
```

So we just initialize counter of the given sequential lock to zero and additionally we can see [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) related initialization which depends on the state of the `CONFIG_DEBUG_LOCK_ALLOC` kernel configuration option:

```C
#ifdef CONFIG_DEBUG_LOCK_ALLOC
# define SEQCOUNT_DEP_MAP_INIT(lockname) \
    .dep_map = { .name = #lockname } \
    ...
    ...
    ...
#else
# define SEQCOUNT_DEP_MAP_INIT(lockname)
    ...
    ...
    ...
#endif
```

As I already wrote in previous parts of this [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim) we will not consider [debugging](https://en.wikipedia.org/wiki/Debugging) and [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt) related stuff in this part. So for now we just skip the `SEQCOUNT_DEP_MAP_INIT` macro. The second field of the given `seqlock_t` is `lock` initialized with the `__SPIN_LOCK_UNLOCKED` macro which is defined in the [include/linux/spinlock_types.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/spinlock_types.h) header file. We will not consider implementation of this macro here as it just initialize [rawspinlock](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-1) with architecture-specific methods (More abot spinlocks you may read in first parts of this [chapter](https://0xax.gitbook.io/linux-insides/summary/syncprim)).

We have considered the first way to initialize a sequential lock. Let's consider second way to do the same, but do it dynamically. We can initialize a sequential lock with the `seqlock_init` macro which is defined in the same  [include/linux/seqlock.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/linux/seqlock.h) header file.

Let's look at the implementation of this macro:

```C
#define seqlock_init(x)					\
	do {						\
		seqcount_init(&(x)->seqcount);		\
		spin_lock_init(&(x)->lock);		\
	} while (0)
```

As we may see, the `seqlock_init` expands into two macros. The first macro `seqcount_init` takes counter of the given sequential lock and expands to the call of the `__seqcount_init` function:

```C
# define seqcount_init(s)				\
	do {						\
		static struct lock_class_key __key;	\
		__seqcount_init((s), #s, &__key);	\
	} while (0)
```

from the same header file. This function

```C
static inline void __seqcount_init(seqcount_t *s, const char *name,
					  struct lock_class_key *key)
{
    lockdep_init_map(&s->dep_map, name, key, 0);
    s->sequence = 0;
}
```

just initializes counter of the given `seqcount_t` with zero. The second call from the `seqlock_init` macro is the call of the `spin_lock_init` macro which we saw in the [first part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-1) of this chapter.

So, now we know how to initialize a `sequential lock`, now let's look at how to use it. The Linux kernel provides following [API](https://en.wikipedia.org/wiki/Application_programming_interface) to manipulate `sequential locks`:

```C
static inline unsigned read_seqbegin(const seqlock_t *sl);
static inline unsigned read_seqretry(const seqlock_t *sl, unsigned start);
static inline void write_seqlock(seqlock_t *sl);
static inline void write_sequnlock(seqlock_t *sl);
static inline void write_seqlock_irq(seqlock_t *sl);
static inline void write_sequnlock_irq(seqlock_t *sl);
static inline void read_seqlock_excl(seqlock_t *sl)
static inline void read_sequnlock_excl(seqlock_t *sl)
```

and others. Before we move on to considering the implementation of this [API](https://en.wikipedia.org/wiki/Application_programming_interface), we must know that actually there are two types of readers. The first type of reader never blocks a writer process. In this case writer will not wait for readers. The second type of reader which can lock. In this case, the locking reader will block the writer as it will wait while reader will not release its lock.

First of all let's consider the first type of readers. The `read_seqbegin` function begins a seq-read [critical section](https://en.wikipedia.org/wiki/Critical_section).

As we may see this function just returns value of the `read_seqcount_begin` function:

```C
static inline unsigned read_seqbegin(const seqlock_t *sl)
{
	return read_seqcount_begin(&sl->seqcount);
}
```

In its turn the `read_seqcount_begin` function calls the `raw_read_seqcount_begin` function:

```C
static inline unsigned read_seqcount_begin(const seqcount_t *s)
{
	return raw_read_seqcount_begin(s);
}
```

which just returns value of the `sequential lock` counter:

```C
static inline unsigned raw_read_seqcount(const seqcount_t *s)
{
	unsigned ret = READ_ONCE(s->sequence);
	smp_rmb();
	return ret;
}
```

After we have the initial value of the given `sequential lock` counter and did some stuff, we know from the previous paragraph of this function, that we need to compare it with the current value of the counter the same `sequential lock` before we will exit from the critical section. We can achieve this by the call of the `read_seqretry` function. This function takes a `sequential lock`, start value of the counter and through a chain of functions:

```C
static inline unsigned read_seqretry(const seqlock_t *sl, unsigned start)
{
	return read_seqcount_retry(&sl->seqcount, start);
}

static inline int read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	smp_rmb();
	return __read_seqcount_retry(s, start);
}
```

it calls the `__read_seqcount_retry` function:

```C
static inline int __read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	return unlikely(s->sequence != start);
}
```

which just compares value of the counter of the given `sequential lock` with the initial value of this counter. If the initial value of the counter which is obtained from `read_seqbegin()` function is odd, this means that a writer was in the middle of updating the data when our reader began to act. In this case the value of the data can be in inconsistent state, so we need to try to read it again.

This is a common pattern in the Linux kernel. For example, you may remember the `jiffies` concept from the [first part](https://0xax.gitbook.io/linux-insides/summary/timers/linux-timers-1) of the [timers and time management in the Linux kernel](https://0xax.gitbook.io/linux-insides/summary/timers/) chapter. The sequential lock is used to obtain value of `jiffies` at [x86_64](https://en.wikipedia.org/wiki/X86-64) architecture:

```C
u64 get_jiffies_64(void)
{
	unsigned long seq;
	u64 ret;

	do {
		seq = read_seqbegin(&jiffies_lock);
		ret = jiffies_64;
	} while (read_seqretry(&jiffies_lock, seq));
	return ret;
}
```

Here we just read the value of the counter of the `jiffies_lock` sequential lock and then we write value of the `jiffies_64` system variable to the `ret`. As here we may see `do/while` loop, the body of the loop will be executed at least one time. So, as the body of loop was executed, we read and compare the current value of the counter of the `jiffies_lock` with the initial value. If these values are not equal, execution of the loop will be repeated, else `get_jiffies_64` will return its value in `ret`.

We just saw the first type of readers which do not block writer and other readers. Let's consider second type. It does not update value of a `sequential lock` counter, but just locks `spinlock`:

```C
static inline void read_seqlock_excl(seqlock_t *sl)
{
	spin_lock(&sl->lock);
}
```

So, no one reader or writer can't access protected data. When a reader finishes, the lock must be unlocked with the:

```C
static inline void read_sequnlock_excl(seqlock_t *sl)
{
	spin_unlock(&sl->lock);
}
```

function.

Now we know how `sequential lock` work for readers. Let's consider how does writer act when it wants to acquire a `sequential lock` to modify data. To acquire a `sequential lock`, writer should use `write_seqlock` function. If we look at the implementation of this function:

```C
static inline void write_seqlock(seqlock_t *sl)
{
	spin_lock(&sl->lock);
	write_seqcount_begin(&sl->seqcount);
}
```

We will see that it acquires `spinlock` to prevent access from other writers and calls the `write_seqcount_begin` function. This function just increments value of the `sequential lock` counter:

```C
static inline void raw_write_seqcount_begin(seqcount_t *s)
{
	s->sequence++;
	smp_wmb();
}
```

When a writer process will finish to modify data, the `write_sequnlock` function must be called to release a lock and give access to other writers or readers. Let's consider at the implementation of the `write_sequnlock` function. It looks pretty simple:

```C
static inline void write_sequnlock(seqlock_t *sl)
{
	write_seqcount_end(&sl->seqcount);
	spin_unlock(&sl->lock);
}
```

First of all it just calls `write_seqcount_end` function to increase value of the counter of the `sequential` lock again:

```C
static inline void raw_write_seqcount_end(seqcount_t *s)
{
	smp_wmb();
	s->sequence++;
}
```

and in the end we just call the `spin_unlock` macro to give access for other readers or writers.

That's all about `sequential lock` mechanism in the Linux kernel. Of course we did not consider full [API](https://en.wikipedia.org/wiki/Application_programming_interface) of this mechanism in this part. But all other functions are based on these which we described here. For example, Linux kernel also provides some safe macros/functions to use `sequential lock` mechanism in [interrupt handlers](https://en.wikipedia.org/wiki/Interrupt_handler) of [softirq](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-9): `write_seqclock_irq` and `write_sequnlock_irq`:

```C
static inline void write_seqlock_irq(seqlock_t *sl)
{
	spin_lock_irq(&sl->lock);
	write_seqcount_begin(&sl->seqcount);
}

static inline void write_sequnlock_irq(seqlock_t *sl)
{
	write_seqcount_end(&sl->seqcount);
	spin_unlock_irq(&sl->lock);
}
```

As we may see, these functions differ only in the initialization of spinlock. They call `spin_lock_irq` and `spin_unlock_irq` instead of `spin_lock` and `spin_unlock`.

Or for example `write_seqlock_irqsave` and `write_sequnlock_irqrestore` functions which are the same but used `spin_lock_irqsave` and `spin_unlock_irqsave` macro to use in [IRQ](https://en.wikipedia.org/wiki/Interrupt_request_\(PC_architecture\)) handlers.

That's all.

## 7.3. 结论

This is the end of the sixth part of the [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_%28computer_science%29) chapter in the Linux kernel. In this part we met with new synchronization primitive which is called - `sequential lock`. From the theoretical side, this synchronization primitive very similar on a [readers-writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock) synchronization primitive, but allows to avoid `writer-starving` issue.

If you have questions or suggestions, feel free to ping me in twitter [0xAX](https://twitter.com/0xAX), drop me [email](mailto:anotherworldofworld@gmail.com) or just create [issue](https://github.com/0xAX/linux-insides/issues/new).

**Please note that English is not my first language and I am really sorry for any inconvenience. If you found any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

## 7.4. 链接

* [synchronization primitives](https://en.wikipedia.org/wiki/Synchronization_\(computer_science\))
* [readers-writer lock](https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock) 
* [spinlock](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-1)
* [critical section](https://en.wikipedia.org/wiki/Critical_section)
* [lock validator](https://www.kernel.org/doc/Documentation/locking/lockdep-design.txt)
* [debugging](https://en.wikipedia.org/wiki/Debugging)
* [API](https://en.wikipedia.org/wiki/Application_programming_interface)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [Timers and time management in the Linux kernel](https://0xax.gitbook.io/linux-insides/summary/timers/)
* [interrupt handlers](https://en.wikipedia.org/wiki/Interrupt_handler)
* [softirq](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-9)
* [IRQ](https://en.wikipedia.org/wiki/Interrupt_request_\(PC_architecture\))
* [Previous part](https://0xax.gitbook.io/linux-insides/summary/syncprim/linux-sync-5)
