<div align=center>
	<img src="_v_images/20200910110325796_584.png" width="600"> 
</div>
<br/>
<br/>

<center><font size='6'>DPDK 锁：ticketlock和mcslock</font></center>
<br/>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2021年4月</font></center>
<br/>
<br/>

# 1. spinlock

```c
/**
 * The rte_spinlock_t type.
 */
typedef struct {
	volatile int locked; /**< lock status 0 = unlocked, 1 = locked */
} rte_spinlock_t;
```

其中`volatile`标识易失的，表明每次读取locked变量时需要直接从内存中读取，这虽然减少了缓存震荡带来的问题，但一次内存IO的开销也是严重的。

使用`__atomic_xxx`实现，此处的宏定义的意思为：

* `__ATOMIC_RELAXED`    //编译器和处理器可以对memory access做任何的reorder
* `__ATOMIC_ACQUIRE`    //保证本线程中,所有后续的读操作必须在本条原子操作完成后执行。
* `__ATOMIC_RELEASE`    //保证本线程中,所有之前的写操作完成后才能执行本条原子操作。
* `__ATOMIC_ACQ_REL`    //同时包含 memory_order_acquire 和 memory_order_release
* `__ATOMIC_CONSUME`    //释放消费顺序（release/consume）的规范正在修订中，而且暂时不鼓励使用 
* `__ATOMIC_SEQ_CST`    //最强的同步模式，同时也是默认的模型，具有强烈的happens-before语义

在下面的

```c
static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
{
	int exp = 0;

	while (!__atomic_compare_exchange_n(&sl->locked, &exp, 1, 0,
				__ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		while (__atomic_load_n(&sl->locked, __ATOMIC_RELAXED))
			rte_pause();
		exp = 0;
	}
}
static inline void
rte_spinlock_unlock (rte_spinlock_t *sl)
{
	__atomic_store_n(&sl->locked, 0, __ATOMIC_RELEASE);
}
static inline int
rte_spinlock_trylock (rte_spinlock_t *sl)
{
	int exp = 0;
	return __atomic_compare_exchange_n(&sl->locked, &exp, 1,
				0, /* disallow spurious failure */
				__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}
static inline int rte_spinlock_is_locked (rte_spinlock_t *sl)
{
	return __atomic_load_n(&sl->locked, __ATOMIC_ACQUIRE);
}
```

使用`__sync_xxx`实现
```c
static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
{
	while (__sync_lock_test_and_set(&sl->locked, 1))
		while (sl->locked)
			rte_pause();
}

static inline void
rte_spinlock_unlock(rte_spinlock_t *sl)
{
	__sync_lock_release(&sl->locked);
}

static inline int
rte_spinlock_trylock(rte_spinlock_t *sl)
{
	return __sync_lock_test_and_set(&sl->locked, 1) == 0;
}
```

使用`asm`实现：
```c
static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
{
	int lock_val = 1;
	asm volatile (
			"1:\n"
			"xchg %[locked], %[lv]\n"
			"test %[lv], %[lv]\n"
			"jz 3f\n"
			"2:\n"
			"pause\n"
			"cmpl $0, %[locked]\n"
			"jnz 2b\n"
			"jmp 1b\n"
			"3:\n"
			: [locked] "=m" (sl->locked), [lv] "=q" (lock_val)
			: "[lv]" (lock_val)
			: "memory");
}

static inline void
rte_spinlock_unlock (rte_spinlock_t *sl)
{
	int unlock_val = 0;
	asm volatile (
			"xchg %[locked], %[ulv]\n"
			: [locked] "=m" (sl->locked), [ulv] "=q" (unlock_val)
			: "[ulv]" (unlock_val)
			: "memory");
}

static inline int
rte_spinlock_trylock (rte_spinlock_t *sl)
{
	int lockval = 1;

	asm volatile (
			"xchg %[locked], %[lockval]"
			: [locked] "=m" (sl->locked), [lockval] "=q" (lockval)
			: "[lockval]" (lockval)
			: "memory");

	return lockval == 0;
}
```

整体思路比较简单，此处不做解释。




# 2. ticketlock


```c
/**
 * The rte_ticketlock_t type.
 */
typedef union {
	uint32_t tickets;
	struct {
		uint16_t current;
		uint16_t next;
	} s;
} rte_ticketlock_t;
```

结构体中有三个变量：

* tickets：ticket，分为高低十六位；
* current：当前持有锁的cpu；
* 下一个即将持有锁；

ticketlock主要解决的问题是，当各个CPU的性能存在较小的差异，或者在调度过程中不同的调度优先级和不同的时间片，都会影响到原始spinlock获取锁线程的争抢顺序有可能先争抢lock的线程抢不到锁，而后来的线程却先抢到了，ticketlock就解决了这个问题。下面直接看实现（做了一定简化）：

```c
static inline void
rte_ticketlock_lock(rte_ticketlock_t *tl)
{
	uint16_t me = __atomic_fetch_add(&tl->s.next, 1, __ATOMIC_RELAXED);
	rte_wait_until_equal_16(&tl->s.current, me, __ATOMIC_ACQUIRE);
}

static inline void
rte_ticketlock_unlock(rte_ticketlock_t *tl)
{
	uint16_t i = __atomic_load_n(&tl->s.current, __ATOMIC_RELAXED);
	__atomic_store_n(&tl->s.current, i + 1, __ATOMIC_RELEASE);
}

static inline int
rte_ticketlock_trylock(rte_ticketlock_t *tl)
{
	rte_ticketlock_t old, new;
	old.tickets = __atomic_load_n(&tl->tickets, __ATOMIC_RELAXED);
	new.tickets = old.tickets;
	new.s.next++;
	if (old.s.next == old.s.current) {
		if (__atomic_compare_exchange_n(&tl->tickets, &old.tickets,
		    new.tickets, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
			return 1;
	}

	return 0;
}
```

# 3. mcslock

mcslock因发明者而得名，`John M. Mellor-Crummey` 和 `Michael L. Scott`。在官方解释，mcslock主要解决的问题在于：

* 提供可扩展/可伸缩的自旋锁；
* 避免缓存震荡；
* 提供公平锁；


首先看下它的结构：
```c
/**
 * The rte_mcslock_t type.
 */
typedef struct rte_mcslock {
	struct rte_mcslock *next;
	int locked; /* 1 if the queue locked, 0 otherwise */
} rte_mcslock_t;
```
首先还是关注三个方面，lock，unlock，和 trylock

```c
static inline void
rte_mcslock_lock(rte_mcslock_t **msl, rte_mcslock_t *me)
{
	rte_mcslock_t *prev;

	/* Init me node */
	__atomic_store_n(&me->locked, 1, __ATOMIC_RELAXED);
	__atomic_store_n(&me->next, NULL, __ATOMIC_RELAXED);

	prev = __atomic_exchange_n(msl, me, __ATOMIC_ACQ_REL);
	if (likely(prev == NULL)) {
		return;
	}
	__atomic_store_n(&prev->next, me, __ATOMIC_RELAXED);
	__atomic_thread_fence(__ATOMIC_ACQ_REL);
	while (__atomic_load_n(&me->locked, __ATOMIC_ACQUIRE))
		rte_pause();
}

static inline void
rte_mcslock_unlock(rte_mcslock_t **msl, rte_mcslock_t *me)
{
	if (likely(__atomic_load_n(&me->next, __ATOMIC_RELAXED) == NULL)) {
		rte_mcslock_t *save_me = __atomic_load_n(&me, __ATOMIC_RELAXED);

		if (likely(__atomic_compare_exchange_n(msl, &save_me, NULL, 0,
				__ATOMIC_RELEASE, __ATOMIC_RELAXED)))
			return;

		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		while (__atomic_load_n(&me->next, __ATOMIC_RELAXED) == NULL)
			rte_pause();
	}

	__atomic_store_n(&me->next->locked, 0, __ATOMIC_RELEASE);
}

static inline int
rte_mcslock_trylock(rte_mcslock_t **msl, rte_mcslock_t *me)
{
	__atomic_store_n(&me->next, NULL, __ATOMIC_RELAXED);

	rte_mcslock_t *expected = NULL;

	return __atomic_compare_exchange_n(msl, &expected, me, 0,
			__ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}
```

首先看下完整的用法：

```c
#include <stdio.h>
#include <pthread.h>

#include "rte_mcslock.h"

rte_mcslock_t *pmcslock;

void *task1(void*arg){
    rte_mcslock_t ml;

    rte_mcslock_lock(&pmcslock, &ml);
    printf("MCS lock taken on thread %ld\n", pthread_self());
    sleep(2);
    rte_mcslock_unlock(&pmcslock, &ml);
}

void *task2(void*arg){
    rte_mcslock_t ml;

    sleep(1);

    rte_mcslock_lock(&pmcslock, &ml);
    printf("MCS lock taken on thread %ld\n", pthread_self());
    rte_mcslock_unlock(&pmcslock, &ml);
}

int main()
{
    pthread_t t1, t2;

    pthread_create(&t1, NULL, task1, NULL);
    pthread_create(&t2, NULL, task2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
}
```

输出为：

```c
$ ./a.out 
MCS lock taken on thread 140330136704768
MCS lock taken on thread 140330128312064
```

整体思路就是：为了方式缓存cache震荡，为每个线程提供自己的变量锁，这样，就克服了使用像原始的spinlock那样使用volatile关键字不使用cache造成的不能发挥缓存的优势的问题，同时，解决了缓存震荡问题。


# 4. rwlock

读写自旋锁的数据结构：

```c
typedef struct {
	volatile int32_t cnt; /**< -1 when W lock held, > 0 when R locks held. */
} rte_rwlock_t;
```

从数据结构来看，和常规的自旋锁基本一致，除了将lock改成了计数器，这个cnt是用来计算读者个数的。

```c
static inline void
rte_rwlock_read_lock(rte_rwlock_t *rwl)
{
	int32_t x;
	int success = 0;

	while (success == 0) {
		x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
		/* write lock is held */
		if (x < 0) {
			rte_pause();
			continue;
		}
		success = __atomic_compare_exchange_n(&rwl->cnt, &x, x + 1, 1,
					__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
	}
}

static inline int
rte_rwlock_read_trylock(rte_rwlock_t *rwl)
{
	int32_t x;
	int success = 0;

	while (success == 0) {
		x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
		/* write lock is held */
		if (x < 0)
			return -EBUSY;
		success = __atomic_compare_exchange_n(&rwl->cnt, &x, x + 1, 1,
					__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
	}

	return 0;
}

static inline void
rte_rwlock_read_unlock(rte_rwlock_t *rwl)
{
	__atomic_fetch_sub(&rwl->cnt, 1, __ATOMIC_RELEASE);
}

static inline int
rte_rwlock_write_trylock(rte_rwlock_t *rwl)
{
	int32_t x;

	x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
	if (x != 0 || __atomic_compare_exchange_n(&rwl->cnt, &x, -1, 1,
			      __ATOMIC_ACQUIRE, __ATOMIC_RELAXED) == 0)
		return -EBUSY;

	return 0;
}

static inline void
rte_rwlock_write_lock(rte_rwlock_t *rwl)
{
	int32_t x;
	int success = 0;

	while (success == 0) {
		x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
		/* a lock is held */
		if (x != 0) {
			rte_pause();
			continue;
		}
		success = __atomic_compare_exchange_n(&rwl->cnt, &x, -1, 1,
					__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
	}
}

static inline void
rte_rwlock_write_unlock(rte_rwlock_t *rwl)
{
	__atomic_store_n(&rwl->cnt, 0, __ATOMIC_RELEASE);
}
```



