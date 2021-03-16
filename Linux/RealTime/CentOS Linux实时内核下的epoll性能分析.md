<center><font size='6'>CentOS 7 Linux实时内核下的epoll性能分析</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月4日</font></center>
<br/>

# 1. 问题引入

一些参考链接见文末。
## 1.1. 测试调试环境
非实时环境：
```c
3.10.0-1062.el7.x86_64
CentOS Linux release 7.7.1908 (Core)
```
实时环境：
```c
3.10.0-1127.rt56.1093.el7.x86_64
CentOS Linux release 7.4.1708 (Core)
```

## 1.2. 问题描述
epoll就不用多说了，对于redis、memcached和nginx无一逃过epoll的加持。近期在实时内核中测试epoll的时延性能参数，发现epoll在实时内核中性能低下。

测试场景：

1. 使用epoll+evenfd进行测试；
2. 初始化阶段创建一个epoll fd，三个eventfd，并将三个eventfd加入epoll；
3. 创建四个线程，线程回调函数中满载运行，无睡眠；
4. 线程1监听epoll_wait，并使用eventfd_read进行读；
5. 线程2、3、4使用eventfd_write进行写；


在非实时内核中运行上面的程序发现，CPU利用率可以达到惊人的400%：
```c
   PID  %CPU COMMAND          P
 88866 398.3 Main             0
```
详细查看具体的四个线程，发现均可达到100%。
```c
   PID %CPU COMMAND           P
 88868 99.9 Enqueue1          1
 88870 99.9 Enqueue3          3
 88869 99.7 Enqueue2          2
 88867 99.3 Dequeue           0
 88866  0.0 Main              0
```
在实时内核中：
```c
   PID  %CPU COMMAND          P
 31785 205.9 Main             2
```
```c
   PID %CPU COMMAND           P
 31786 53.8 Dequeue           0
 31787 53.8 Enqueue1          1
 31788 53.8 Enqueue2          2
 31789 53.8 Enqueue3          3
 31785  0.0 Main              2
```
详细查看具体的四个线程，发现均不能达到100%。

## 1.3. 问题追踪

为什么呢？百思不得其解，翻看实时内核相关，翻看红帽的官方手册（见参考链接）发现，电源配置和中断亲和性都需要进行配置，但是我进行了一顿设置无果，但是在查看系统中断过程中发现，在实时内核中运行上述应用会导致`Rescheduling-interrupts`的剧增。

我们采用如下命令实时监控系统中断：
```c
watch -n1 cat /proc/interrupts
```
输出结果为(只保留变化明显的中断)：
```c
            CPU0       CPU1       CPU2       CPU3       
...
 LOC:   12977669    9081580    1821991    1819625   Local timer interrupts
...
 RES:    1791064    2128812        748        726   Rescheduling interrupts
...
```
可见除去本地定时中断LOC，上述中断的变化微乎其微，再在实时内核上尝试，发现：
```c
            CPU0       CPU1       CPU2       CPU3       
...
 RES: 3785763488 2687470633 2751775083 2766930787   Rescheduling interrupts
...
```
`Rescheduling-interrupts`每秒竟然以10万数量级发生变化，Why？对于熟悉进程调度和阅读过内核代码的朋友来说，调度中断是由`schedule()`产生的，举个例子，当一个线程调用了sleep()函数，通过系统调用进入内核态，同时该线程会主动调用schedule()放弃CPU，内核对其他进程进行调度。那么epoll为什么要采用这种策略呢？

> 注意：
> perf-tools：大神Brendan Gregg在GitHub上开源的内核窥探工具。

我们使用funccount工具进行内核函数调用窥探，它的用法如下：
```c
./funccount 
USAGE: funccount [-hT] [-i secs] [-d secs] [-t top] funcstring
                 -d seconds      # total duration of trace
                 -h              # this usage message
                 -i seconds      # interval summary
                 -t top          # show top num entries only
                 -T              # include timestamp (for -i)
  eg,
       funccount 'vfs*'          # trace all funcs that match "vfs*"
       funccount -d 5 'tcp*'     # trace "tcp*" funcs for 5 seconds
       funccount -t 10 'ext3*'   # show top 10 "ext3*" funcs
       funccount -i 1 'ext3*'    # summary every 1 second
       funccount -i 1 -d 5 'ext3*' # 5 x 1 second summaries

See the man page and example file for more info.
```

我们对epoll_wait进行追踪统计，在非实时内核中结果如下：
```c
Tracing "*epoll_wait*"... Ctrl-C to end.
^C
FUNC                              COUNT
SyS_epoll_wait                   979621
```
而在实时内核中，epoll_wait的调用次数和`Rescheduling-interrupts`成正比，Why？

接着，使用funcgraph命令对epoll_wait进行监控，在非实时内核中，给出一个epoll_wait的整体调用栈：
```c
SyS_epoll_wait() {
  fget_light();
  ep_poll() {
    _raw_spin_lock_irqsave();
    _raw_spin_unlock_irqrestore();
    ep_scan_ready_list.isra.7() {
      mutex_lock() {
        _cond_resched();
      }
      _raw_spin_lock_irqsave();
      _raw_spin_unlock_irqrestore();
      ep_send_events_proc() {
        eventfd_poll();
        eventfd_poll();
        eventfd_poll();
      }
      _raw_spin_lock_irqsave();
      __pm_relax();
      _raw_spin_unlock_irqrestore();
      mutex_unlock();
    }
  }
  fput();
}
```
而在实时内核中，省略掉大部分代码，可见schedule()被epoll_wait调用：
```c
SyS_epoll_wait() {
  fget_light() {
    __rcu_read_lock();
    __rcu_read_unlock();
  }
  ep_poll() {
    migrate_disable() {
      pin_current_cpu();
    }
    rt_spin_lock() {
      rt_spin_lock_slowlock() {
        _raw_spin_lock_irqsave();
        rt_spin_lock_slowlock_locked() {
          schedule() {
            __schedule() {
              ...
            }
          }
    ...
      rt_spin_lock() {
        rt_spin_lock_slowlock() {
    ...
            schedule() {
              __schedule() {
                rcu_note_context_switch();
                _raw_spin_lock_irq();
    ...
      rt_spin_lock() {
        rt_spin_lock_slowlock() {
          _raw_spin_lock_irqsave();
          rt_spin_lock_slowlock_locked() {
    ...
            schedule() {
              __schedule() {
                rcu_note_context_switch();
                _raw_spin_lock_irq();
                deactivate_task() {
    ...
  fput();
}
```

## 1.4. 实时内核源码路径分析

`kernel-rt-3.10.0-1127.rt56.1093.el7.src`。

### 1.4.1. epoll_wait
在文件`fs/eventpoll.c`中有：
```c

/*
 * Implement the event wait interface for the eventpoll file. It is the kernel
 * part of the user space epoll_wait(2).
 */
SYSCALL_DEFINE4(epoll_wait, int, epfd, struct epoll_event __user *, events,
		int, maxevents, int, timeout)
{
    ...
	/* Time to fish for events ... */
	error = ep_poll(ep, events, maxevents, timeout);
    ...
}
```
### 1.4.2. ep_poll
首先找到自旋锁的位置：
```c
static int ep_poll(struct eventpoll *ep, struct epoll_event __user *events,
		   int maxevents, long timeout)
{
    ...
	spin_lock_irqsave(&ep->lock, flags);
    ...
}
```
在实时内核中在文件`include\linux\spinlock.h`有：
```c
#ifdef CONFIG_PREEMPT_RT_FULL
# include <linux/spinlock_rt.h>
#else /* PREEMPT_RT_FULL */
```

### 1.4.3. spin_lock_irqsave
在`include\linux\spinlock_rt.h`有：
```c
#define spin_lock_irqsave(lock, flags)			 \
	do {						 \
		typecheck(unsigned long, flags);	 \
		flags = 0;				 \
		spin_lock(lock);			 \
	} while (0)
```

其中`spin_lock`定义为：
```c
#define spin_lock(lock)				\
	do {					\
		migrate_disable();		\
		rt_spin_lock(lock);		\
	} while (0)
```
找到了`migrate_disable`，继续寻找`rt_spin_lock`：
```c
void __lockfunc rt_spin_lock(spinlock_t *lock)
{
	rt_spin_lock_fastlock(&lock->lock, rt_spin_lock_slowlock);
	spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
}
EXPORT_SYMBOL(rt_spin_lock);
```
`rt_spin_lock_fastlock`如下：
```c
/*
 * preemptible spin_lock functions:
 */
static inline void rt_spin_lock_fastlock(struct rt_mutex *lock,
					 void  (*slowfn)(struct rt_mutex *lock))
{
	might_sleep();

	if (likely(rt_mutex_cmpxchg(lock, NULL, current)))
		rt_mutex_deadlock_account_lock(lock, current);
	else
		slowfn(lock);
}
```
其中的`slowfn`为`rt_spin_lock_slowlock`：
```c
static void noinline __sched rt_spin_lock_slowlock(struct rt_mutex *lock)
{
	struct rt_mutex_waiter waiter;
	unsigned long flags;

	rt_mutex_init_waiter(&waiter, true);

	raw_spin_lock_irqsave(&lock->wait_lock, flags);
	rt_spin_lock_slowlock_locked(lock, &waiter, flags);
	raw_spin_unlock_irqrestore(&lock->wait_lock, flags);
	debug_rt_mutex_free_waiter(&waiter);
}
```
其中`rt_spin_lock_slowlock_locked`如下：
```c
void __sched rt_spin_lock_slowlock_locked(struct rt_mutex *lock,
					  struct rt_mutex_waiter *waiter,
					  unsigned long flags)
{
    ...
	for (;;) {
		/* Try to acquire the lock again. */
		if (__try_to_take_rt_mutex(lock, self, waiter, STEAL_LATERAL))
			break;

		top_waiter = rt_mutex_top_waiter(lock);
		lock_owner = rt_mutex_owner(lock);
        ...
		if (top_waiter != waiter || adaptive_wait(lock, lock_owner))
			schedule_rt_mutex(lock);
        ...
	}
    ...
}
```
期间当条件不满足的情况`for (;;)`会循环调用`schedule_rt_mutex`，
```c
# define schedule_rt_mutex(_lock)			schedule()
```

至此找到了`Rescheduling-interrupts`剧增的根本原因。

## 1.5. 非实时内核的`spin_lock_irqsave`内核路径
下面看一下不使用实时自旋锁的内核路径。非实时情况下，`spin_lock_irqsave`的定义如下：
```c
#define spin_lock_irqsave(lock, flags)				\
do {								\
	raw_spin_lock_irqsave(spinlock_check(lock), flags);	\
} while (0)
```
`raw_spin_lock_irqsave`定义如下：
```c
#define raw_spin_lock_irqsave(lock, flags)			\
	do {						\
		typecheck(unsigned long, flags);	\
		flags = _raw_spin_lock_irqsave(lock);	\
	} while (0)
```
继续`_raw_spin_lock_irqsave`：
```c
unsigned long __lockfunc _raw_spin_lock_irqsave(raw_spinlock_t *lock)
{
	return __raw_spin_lock_irqsave(lock);
}
EXPORT_SYMBOL(_raw_spin_lock_irqsave);
```
`__raw_spin_lock_irqsave`如下：
```c
static inline unsigned long __raw_spin_lock_irqsave(raw_spinlock_t *lock)
{
	...
#ifdef CONFIG_LOCKDEP
	LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
#else
	do_raw_spin_lock_flags(lock, &flags);
#endif
	return flags;
}
```
无论使用`do_raw_spin_lock`还是`do_raw_spin_lock_flags`，最终都会调用`arch_spin_lock`
```c
static inline void
do_raw_spin_lock_flags(raw_spinlock_t *lock, unsigned long *flags) __acquires(lock)
{
	__acquire(lock);
	arch_spin_lock_flags(&lock->raw_lock, *flags);
}
```
```c
void do_raw_spin_lock(raw_spinlock_t *lock)
{
	debug_spin_lock_before(lock);
	arch_spin_lock(&lock->raw_lock);
	debug_spin_lock_after(lock);
}
```
```c
static __always_inline void arch_spin_lock_flags(arch_spinlock_t *lock,
						  unsigned long flags)
{
	arch_spin_lock(lock);
}
```
`arch_spin_lock`如下：
```c
static __always_inline void arch_spin_lock(arch_spinlock_t *lock)
{
	register struct __raw_tickets inc = { .tail = TICKET_LOCK_INC };

	inc = xadd(&lock->tickets, inc);
	if (likely(inc.head == inc.tail))
		goto out;

	inc.tail &= ~TICKET_SLOWPATH_FLAG;
	for (;;) {
		unsigned count = SPIN_THRESHOLD;

		do {
			if (ACCESS_ONCE(lock->tickets.head) == inc.tail)
				goto out;
			cpu_relax();
		} while (--count);
		__ticket_lock_spinning(lock, inc.tail);
	}
out:	barrier();	/* make sure nothing creeps before the lock is taken */
}
```
可见非实时内核的`spin_lock_irqsave`是单纯的自旋，期间不会放弃CPU进行`schedule()`。

# 2. `rt_spin_lock_slowlock_locked`分析

在非实时内核情况下的自旋锁的结构如下：
```c
typedef struct qspinlock {
	atomic_t	val;
} arch_spinlock_t;

typedef struct raw_spinlock {
	arch_spinlock_t raw_lock;
} raw_spinlock_t;
/*
 * The non RT version maps spinlocks to raw_spinlocks
 */
typedef struct spinlock {
	struct raw_spinlock rlock;
} spinlock_t;
```

在实时内核下，自旋锁定义为：
```c
/**
 * The rt_mutex structure
 *
 * @wait_lock:	spinlock to protect the structure
 * @waiters:	rbtree root to enqueue waiters in priority order
 * @waiters_leftmost: top waiter
 * @owner:	the mutex owner
 */
struct rt_mutex {
	raw_spinlock_t		wait_lock;
#ifdef __GENKSYMS__
	struct plist_head	wait_list;
#else
	struct rb_root		waiters;
	struct rb_node		*waiters_leftmost;
#endif
	struct task_struct	*owner;
	int			save_state;
};
/*
 * PREEMPT_RT: spinlocks - an RT mutex plus lock-break field:
 */
typedef struct spinlock {
	struct rt_mutex		lock;
	unsigned int		break_lock;
} spinlock_t;
```
造成`rt_spin_lock_slowlock_locked`代码如下：
```c
void __sched rt_spin_lock_slowlock_locked(struct rt_mutex *lock,
					  struct rt_mutex_waiter *waiter,
					  unsigned long flags)
{
	struct task_struct *lock_owner, *self = current;
	struct rt_mutex_waiter *top_waiter;
	int ret;

	if (__try_to_take_rt_mutex(lock, self, NULL, STEAL_LATERAL))
		return;

	BUG_ON(rt_mutex_owner(lock) == self);

	/*
	 * We save whatever state the task is in and we'll restore it
	 * after acquiring the lock taking real wakeups into account
	 * as well. We are serialized via pi_lock against wakeups. See
	 * try_to_wake_up().
	 */
	raw_spin_lock(&self->pi_lock);
	self->saved_state = self->state;
	__set_current_state(TASK_UNINTERRUPTIBLE);
	raw_spin_unlock(&self->pi_lock);

	ret = task_blocks_on_rt_mutex(lock, waiter, self, 0);
	BUG_ON(ret);

	for (;;) {
		/* Try to acquire the lock again. */
		if (__try_to_take_rt_mutex(lock, self, waiter, STEAL_LATERAL))
			break;

		top_waiter = rt_mutex_top_waiter(lock);
		lock_owner = rt_mutex_owner(lock);

		raw_spin_unlock_irqrestore(&lock->wait_lock, flags);

		debug_rt_mutex_print_deadlock(waiter);

		if (top_waiter != waiter || adaptive_wait(lock, lock_owner))
			schedule_rt_mutex(lock);

		raw_spin_lock_irqsave(&lock->wait_lock, flags);

		raw_spin_lock(&self->pi_lock);
		__set_current_state(TASK_UNINTERRUPTIBLE);
		raw_spin_unlock(&self->pi_lock);
	}

	/*
	 * Restore the task state to current->saved_state. We set it
	 * to the original state above and the try_to_wake_up() code
	 * has possibly updated it when a real (non-rtmutex) wakeup
	 * happened while we were blocked. Clear saved_state so
	 * try_to_wakeup() does not get confused.
	 */
	raw_spin_lock(&self->pi_lock);
	__set_current_state(self->saved_state);
	self->saved_state = TASK_RUNNING;
	raw_spin_unlock(&self->pi_lock);

	/*
	 * try_to_take_rt_mutex() sets the waiter bit
	 * unconditionally. We might have to fix that up:
	 */
	fixup_rt_mutex_waiters(lock);

	BUG_ON(rt_mutex_has_waiters(lock) && waiter == rt_mutex_top_waiter(lock));
	BUG_ON(!RB_EMPTY_NODE(&waiter->tree_entry));
}
```
而在ep_poll函数中：
```c
static int ep_poll(struct eventpoll *ep, struct epoll_event __user *events,
		   int maxevents, long timeout)
{
    ...
	spin_lock_irqsave(&ep->lock, flags);

	if (!ep_events_available(ep)) {
		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (ep_events_available(ep) || timed_out)
				break;
			if (signal_pending(current)) {
				res = -EINTR;
				break;
			}

			spin_unlock_irqrestore(&ep->lock, flags);
			if (!schedule_hrtimeout_range(to, slack, HRTIMER_MODE_ABS))
				timed_out = 1;

			spin_lock_irqsave(&ep->lock, flags);
		}
	}
	...
}
```
当有event发生`ep_events_available`或被信号打断`signal_pending`，`for (;;)`循环才会终止，而在`for (;;)`期间，实时内核中自旋锁的获取和释放都需要重新调度，而非实时内核中不会这样。有人疑问`schedule_hrtimeout_range`在非实时内核中不也会别调用吗，看下这个函数中的实现：
```c
if (expires && !expires->tv64) {
	__set_current_state(TASK_RUNNING);
	return 0;
}
if (!expires) {
	schedule();
	return -EINTR;
}
```
用户态的调用如下：
```c
nfds = epoll_wait(ectx->epollfd, ectx->events, MAX_EVENTS, -1);
```
也就是说函数ep_poll的timeout值为-1，也就导致`schedule_hrtimeout_range`的to为NULL，下面的代码将被调用
```c
if (!expires) {
	schedule();
	return -EINTR;
}
```
直接进行调度`schedule()`，待下次该进程被调度后继续轮询epollfd。

# 3. 结论

目前可以初步确认，当采用实时内核时，epoll_wait系统调用将自旋锁替换为spinlock_rt.h中定义的自旋锁，其内部进行进程调度触发重调度中断，而非实时内核中仅在ep_poll中会重新调度（通过对内核进行追踪，该调度函数被调用的概率极低）。

# 4. 参考链接

* [实时补丁二进制RPM](http://mirror.centos.org/centos/7/rt/x86_64/Packages/)
* [实时补丁源代码RPM](http://mirror.centos.org/centos/7/rt/Source/SPackages/)
* [3.10.0-1127.rt56.1093.el7.x86_64](https://linuxsoft.cern.ch/cern/centos/7/rt/x86_64/Packages/kernel-rt-doc-3.10.0-1127.rt56.1093.el7.noarch.rpm)
* [Product Documentation for Red Hat Enterprise Linux for Real Time 7](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/7/)










