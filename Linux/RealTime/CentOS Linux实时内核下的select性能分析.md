<center><font size='6'>CentOS 7 Linux实时内核下的select性能分析</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月5日</font></center>
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

具体请参见《CentOS Linux实时内核下的epoll性能分析》，本文**将epoll替换为 select系统调用**，发现在实时内核中的测试性能和非实时内核中的测试性能基本一致，并且`重新调度中断`不会暴增。

# 2. 实时内核源码分析

## 2.1. 查看select()的内核路径
首先使用perf-tools工具查看程序中select系统调用的内核路径，发现无论如何都没有像epoll那样的schedule()调度函数的运行，这是为什么呢？

```c
SyS_select() {
  core_sys_select() {
    ...
    do_select() {
      ...
      eventfd_poll() {
        __pollwait() {
          add_wait_queue() {
            ...
            rt_spin_lock();
            rt_spin_unlock() {
              rt_spin_lock_slowunlock() {
                _raw_spin_lock_irqsave();
                mark_wakeup_next_waiter() {
                  _raw_spin_lock();
                  rt_mutex_dequeue_pi();
                  wake_q_add();
                }
                _raw_spin_unlock_irqrestore();
                wake_up_q() {
                  __wake_up_q();
                }
                wake_up_q_sleeper() {
                  __wake_up_q() {
                    try_to_wake_up() {
                      _raw_spin_lock_irqsave();
                      _raw_spin_unlock_irqrestore();
                    }
                  }
                }
                rt_mutex_adjust_prio() {
                  _raw_spin_lock_irqsave();
                  __rt_mutex_adjust_prio();
                  _raw_spin_unlock_irqrestore();
                }
              }
            }
            migrate_enable() {
              unpin_current_cpu();
            }
          }
        }
      }
      ...
      eventfd_poll();
      ...
      poll_freewait() {
        remove_wait_queue() {
          ...
          rt_spin_lock();
          rt_spin_unlock();
          ...
        fput();
      }
    }
    ...
}
```

为此我们只比较epoll和select的内核路径中rt_spin_lock部分，epoll的rt_spin_lock会触发schedule()，如下：
```c
    rt_spin_lock() {
      rt_spin_lock_slowlock() {
        _raw_spin_lock_irqsave();
        rt_spin_lock_slowlock_locked() {
          __try_to_take_rt_mutex.isra.12();
          _raw_spin_lock();
          task_blocks_on_rt_mutex() {
            _raw_spin_lock();
            __rt_mutex_adjust_prio();
            rt_mutex_enqueue();
          }
          __try_to_take_rt_mutex.isra.12();
          _raw_spin_unlock_irqrestore();
          schedule() {
            __schedule() {
              rcu_note_context_switch();
              _raw_spin_lock_irq();
              deactivate_task() {
                update_rq_clock.part.81() {
                  kvm_steal_clock();
                }
                dequeue_task_fair() {
                  dequeue_entity() {
                    update_curr() {
                      update_min_vruntime();
                      cpuacct_charge() {
                        __rcu_read_lock();
                        __rcu_read_unlock();
                      }
                    }
                    update_cfs_rq_blocked_load();
                    clear_buddies();
                    account_entity_dequeue();
                    update_min_vruntime();
                    update_cfs_shares();
                  }
                  hrtick_update();
                }
              }
              pick_next_task_fair();
              pick_next_task_idle() {
                put_prev_task_fair() {
                  put_prev_entity() {
                    check_cfs_rq_runtime();
                  }
                }
              }
              finish_task_switch() {
                _raw_spin_unlock_irq();
              }
            }
          } /* schedule */
          _raw_spin_lock_irqsave();
          _raw_spin_lock();
          __try_to_take_rt_mutex.isra.12() {
            _raw_spin_lock();
            rt_mutex_dequeue();
            rt_mutex_enqueue_pi();
          }
          _raw_spin_lock();
        }
        _raw_spin_unlock_irqrestore();
      }
    }
```
而select内核路径中的rt_spin_lock大多数为可以获取锁成功，如下：
```c
            ...
              pin_current_cpu();
            }
            rt_spin_lock();
            rt_spin_unlock();
            migrate_enable() {
              unpin_current_cpu();            
            ...
```
很少情况会进入rt_spin_lock执行
```c
          rt_spin_lock() {
            rt_spin_lock_slowlock() {
              _raw_spin_lock_irqsave();
              rt_spin_lock_slowlock_locked() {
                __try_to_take_rt_mutex.isra.12();
              }
              _raw_spin_unlock_irqrestore();
            }
          }
```
这是什么原因导致的呢？首先回归到epoll系统调用，看看rt_spinlock是如何分配的。

## 2.2. epoll中的rt_spinlock自旋锁

首先看下`eventpoll`结构：
```c
struct eventpoll {
	/* Protect the access to this structure */
	spinlock_t lock;
	...
};
```
`spinlock_t`在实时内核中为：
```c
/*
 * PREEMPT_RT: spinlocks - an RT mutex plus lock-break field:
 */
typedef struct spinlock {
	struct rt_mutex		lock;
	unsigned int		break_lock;
} spinlock_t;
```


`epoll_create`
```c
SYSCALL_DEFINE1(epoll_create1, int, flags)
{
	...
	error = ep_alloc(&ep);
	...
}
```
`ep_alloc`中分配自旋锁：
```c

static int ep_alloc(struct eventpoll **pep)
{
	...
	spin_lock_init(&ep->lock);
	...
}
```
可以确定，用户态调用epoll_create后，在进程的未使用的fd中分配一个fd，这个fd即是一个file，这个epollfd是这个file的私有数据：
```c
SYSCALL_DEFINE1(epoll_create1, int, flags)
{
	struct eventpoll *ep = NULL;
	struct file *file;
    ...
	error = ep_alloc(&ep);
	...
	file = anon_inode_getfile("[eventpoll]", &eventpoll_fops, ep,
				 O_RDWR | (flags & O_CLOEXEC));
    ...
}
struct file *anon_inode_getfile(const char *name,
				const struct file_operations *fops,
				void *priv, int flags)
{
    ...
	file->private_data = priv;
    ...
}
```
根据eventpoll结构中对自旋锁的注释，该结构用于保护eventpoll结构，根据实际情况中该自旋锁会因为获取不到锁而进入锁内部进行调度，也就是说这里的eventpoll结构中的自旋锁lock被多个进程竞争。

继而，我们看epoll_wait系统调用(只保留关键部分)：
```c
SYSCALL_DEFINE4(epoll_wait, int, epfd, struct epoll_event __user *, events,
		int, maxevents, int, timeout)
{
	struct fd f;
	struct eventpoll *ep;
	f = fdget(epfd);
	ep = f.file->private_data;
	error = ep_poll(ep, events, maxevents, timeout);
    ...
}
```


## 2.3. select()系统调用
```c
SYSCALL_DEFINE5(select, int, n, fd_set __user *, inp, fd_set __user *, outp,
		fd_set __user *, exp, struct timeval __user *, tvp)
{
    ...
	ret = core_sys_select(n, inp, outp, exp, to);
    ...
}
```
接着，`core_sys_select`如下：
```c
int core_sys_select(int n, fd_set __user *inp, fd_set __user *outp,
			   fd_set __user *exp, struct timespec *end_time)
{    
    ...
	ret = do_select(n, &fds, end_time);
    ...
}
```
然后，`do_select`如下：



# 3. 参考链接与文章

* [实时补丁二进制RPM](http://mirror.centos.org/centos/7/rt/x86_64/Packages/)
* [实时补丁源代码RPM](http://mirror.centos.org/centos/7/rt/Source/SPackages/)
* [3.10.0-1127.rt56.1093.el7.x86_64](https://linuxsoft.cern.ch/cern/centos/7/rt/x86_64/Packages/kernel-rt-doc-3.10.0-1127.rt56.1093.el7.noarch.rpm)
* [Product Documentation for Red Hat Enterprise Linux for Real Time 7](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux_for_real_time/7/)
* 《CentOS Linux实时内核下的epoll性能分析》










