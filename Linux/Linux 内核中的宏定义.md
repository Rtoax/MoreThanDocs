<center><font size='6'>Linux 内核中的宏定义</font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>日期</font></center>
<br/>

* 内核版本：linux-5.10.13
* 注释版代码：[https://github.com/Rtoax/linux-5.10.13](https://github.com/Rtoax/linux-5.10.13)
* `__attribute__`宏
* `__builtin_`宏
* 常规宏

# 1. __ASSEMBLY__
关于`__attribute__`的宏定义在`include\linux\compiler_types.h`文件中，首先是宏`__ASSEMBLY__`，这是在编译阶段使用`-D`加上去的，也就是`-D__ASSEMBLY__`，`AFLAGS`这个变量也定义了这个变量：如下：

```c
$ grep AFLAGS `find -name Makefile` | grep __ASSEMBLY__
./arch/powerpc/boot/Makefile:BOOTAFLAGS	:= -D__ASSEMBLY__ $(BOOTCFLAGS) -nostdinc
./arch/powerpc/platforms/cell/spufs/Makefile:SPU_AFLAGS	:= -c -D__ASSEMBLY__ -I$(srctree)/include -D__KERNEL__
./arch/mips/boot/compressed/Makefile:KBUILD_AFLAGS := $(KBUILD_AFLAGS) -D__ASSEMBLY__ \
./arch/arm64/kernel/vdso32/Makefile:VDSO_AFLAGS += -D__ASSEMBLY__
./arch/x86/boot/Makefile:KBUILD_AFLAGS	:= $(KBUILD_CFLAGS) -D__ASSEMBLY__
./arch/x86/boot/compressed/Makefile:KBUILD_AFLAGS  := $(KBUILD_CFLAGS) -D__ASSEMBLY__
./arch/x86/realmode/rm/Makefile:KBUILD_AFLAGS	:= $(KBUILD_CFLAGS) -D__ASSEMBLY__
./arch/s390/Makefile:KBUILD_AFLAGS_DECOMPRESSOR := $(CLANG_FLAGS) -m64 -D__ASSEMBLY__
./Makefile:KBUILD_AFLAGS   := -D__ASSEMBLY__ -fno-PIE
```
用在这里，是因为汇编代码里，不会用到类似于__user这样的属性（关于__user这样的属性是怎么回子事，本文后面会提到），因为这样的属性是在定义函数的时候加的，这样避免不必要的在编译汇编代码时候的引用。

# 2. __CHECKER__

当编译内核代码的时候，使用make C=1或C=2的时候，会调用一个叫Sparse的工具，这个工具对内核代码进行检查，怎么检查呢，就是靠对那些声明过Sparse这个工具所能识别的特性的内核函数或是变量进行检查。在调用Sparse这个工具的同时，在Sparse代码里，会加上`#define __CHECKER__ 1`的字样。换句话说，就是，如果使用Sparse对代码进行检查，那么内核代码就会定义`__CHECKER__`宏，否则就不定义。

所以这里就能看出来，类似于`__attribute__((noderef, address_space(1)))`这样的属性就是Sparse这个工具所能识别的了。

接下来就正式进入`__attribute__`属性，

# 3. __attribute__

## 3.1. __user 

```c
#ifndef __ASSEMBLY__

#ifdef __CHECKER__
# define __user		__attribute__((noderef, address_space(__user)))
#else 
# ifdef STRUCTLEAK_PLUGIN
#  define __user	__attribute__((user))
# else
#  define __user
# endif
#endif /* __CHECKER__ */
```
这里实际上是`# define __user  __attribute__((noderef, address_space(1)))`，`__user`这个特性，即`__attribute__((noderef, address_space(1)))`，是用来修饰一个变量的，这个变量必须是非解除参考（no dereference）的，即**这个变量地址必须是有效的**，而且变量所在的地址空间必须是1，即用户程序空间的。


这里把程序空间分成了3个部分：

* 0表示normal space，即普通地址空间，对内核代码来说，当然就是内核空间地址了；
* 1表示用户地址空间，这个不用多讲；
* 还有一个2，表示是设备地址映射空间，例如硬件设备的寄存器在内核里所映射的地址空间。

所以在内核函数里，有一个copy_to_user的函数，函数的参数定义就使用了这种方式。当然，这种特性检查，只有当机器上安装了Sparse这个工具，而且进行了编译的时候调用，才能起作用的。


## 3.2. __kernel

下面的介绍中都会省去`__ASSEMBLY__`和`__CHECKER__`的分支。

```c
# define __kernel	__attribute__((address_space(0)))
```
根据定义，就是默认的地址空间，即0，我想定义成`__attribute__((noderef, address_space(0)))`也是没有问题的。

## 3.3. __iomem
```c
# define __iomem	__attribute__((noderef, address_space(__iomem)))
```

这个定义与`__user`, `__kernel`是一样的，只不过这里的变量地址是需要在设备地址映射空间的。

## 3.4. __safe 

```c
# define __safe		__attribute__((safe))
```
这个定义在sparse里也有，内核代码是在2.6.6-rc1版本变到2.6.6-rc2的时候被Linus加入的，经过我的艰苦的查找，终于查找到原因了，知道了为什么Linus要加入这个定义，原因是这样的：
有人发现在代码编译的时候，编译器对变量的检查有些苛刻，导致代码在编译的时候老是出问题（我这里没有去检查是编译不通过还是有警告信息，因为现在的编译器已经不是当年的编译器了，代码也不是当年的代码）。比如说这样一个例子，
```c
int test( struct a * a, struct b * b, struct c * c ) {
    return a->a + b->b + c->c;
}
```
这个编译的时候会有问题，因为没有检查参数是否为空，就直接进行调用。但是呢，在内核里，有好多函数，当它们被调用的时候，这些个参数必定不为空，所以根本用不着去对这些个参数进行非空的检查，所以呢，就增加了一个`__safe`的属性，如果这样声明变量，
```c
int test( struct a * __safe a, struct b * __safe b, struct c * __safe c ) {
    return a->a + b->b + c->c;
}
```
编译就没有问题了。

不过我在现在的代码里没有发现有使用`__safe`这个定义的地方，不知道是不是编译器现在已经支持这种特殊的情况了，所以就不用再加这样的代码了。

## 3.5. __force

```c
# define __force	__attribute__((force))
```
表示所定义的变量类型是可以做强制类型转换的，在进行Sparse分析的时候，是不用报告警信息的。

## 3.6. __nocast
```c
# define __nocast	__attribute__((nocast))
```
这里表示这个变量的参数类型与实际参数类型一定得对得上才行，要不就在Sparse的时候生产告警信息。

## 3.7. __acquires,__releases
```c
# define __acquires(x)	__attribute__((context(x,0,1)))
# define __releases(x)	__attribute__((context(x,1,0)))
```
这是一对相互关联的函数定义，第一句表示参数x在执行之前，引用计数必须为0，执行后，引用计数必须为1，第二句则正好相反，这个定义是用在修饰函数定义的变量的。

举个例子：

```c
/*
 * task_rq_lock - lock p->pi_lock and lock the rq @p resides on.
 */
struct rq *task_rq_lock(struct task_struct *p, struct rq_flags *rf)
	__acquires(p->pi_lock)
	__acquires(rq->lock)
{
	struct rq *rq;

	for (;;) {
		raw_spin_lock_irqsave(&p->pi_lock, rf->flags);
		rq = task_rq(p);
		raw_spin_lock(&rq->lock);
```

对于另一个宏定义：
```c
/**
 * schedule_tail - first thing a freshly forked thread must call.
 * @prev: the thread we just switched away from.
 */
asmlinkage __visible void schedule_tail(struct task_struct *prev)
	__releases(rq->lock)
{
	struct rq *rq;

```

## 3.8. __acquire,__release
```c
# define __acquire(x)	__context__(x,1)
# define __release(x)	__context__(x,-1)
```
这是一对相互关联的函数定义，第一句表示要增加变量x的计数，增加量为1，第二句则正好相反，这个是用来函数执行的过程中。

举个例子：
```c
static inline void sdata_lock(struct ieee80211_sub_if_data *sdata)
	__acquires(&sdata->wdev.mtx)
{
	mutex_lock(&sdata->wdev.mtx);
	__acquire(&sdata->wdev.mtx);
}

static inline void sdata_unlock(struct ieee80211_sub_if_data *sdata)
	__releases(&sdata->wdev.mtx)
{
	mutex_unlock(&sdata->wdev.mtx);
	__release(&sdata->wdev.mtx);
}
```

以上四句如果在代码中出现了不平衡的状况，那么在Sparse的检测中就会报警。当然，Sparse的检测只是一个手段，而且是静态检查代码的手段，所以它的帮助有限，有可能把正确的认为是错误的而发出告警。要是对以上四句的意思还是不太了解的话，请在源代码里搜一下相关符号的用法就能知道了。这第一组与第二组，在本质上，是没什么区别的，只是使用的位置上，有所区别罢了。

## 3.9. __cond_lock

```c
# define __cond_lock(x,c)	((c) ? ({ __acquire(x); 1; }) : 0)
```

这句话的意思就是条件锁。当c这个值不为0时，则让计数值加1，并返回值为1。

> 文末链接的文章中，作者有这样的疑问：
> 不过这里我有一个疑问，就是在这里，有一个`__cond_lock`定义，但没有定义相应的__cond_unlock，那么在变量的释放上，就没办法做到一致。而且我查了一下关于spin_trylock()这个函数的定义，它就用了__cond_lock，而且里面又用了_spin_trylock函数，在_spin_trylock函数里，再经过几次调用，就会使用到__acquire函数，这样的话，相当于一个操作，就进行了两次计算，会导致Sparse的检测出现告警信息，经过我写代码进行实验，验证了我的判断，确实是会出现告警信息，如果我写两遍unlock指令，就没有告警信息了，但这是与程序的运行是不一致的。

关于这个问题，内核设计者肯定是考虑到了，可以看下自旋锁的实现中有这样的定义：

```c
#define ___LOCK(lock) \
  do { __acquire(lock); (void)(lock); } while (0)
#define ___UNLOCK(lock) \
  do { __release(lock); (void)(lock); } while (0)
```

## 3.10. __chk_user_ptr,__chk_io_ptr
```c
static inline void __chk_user_ptr(const volatile void __user *ptr) { }
static inline void __chk_io_ptr(const volatile void __iomem *ptr) { }
```

在进行Sparse的时候，让Sparse给代码做必要的参数类型检查，在实际的编译过程中，并不需要这两个函数的实现。


## 3.11. notrace

```c
#define notrace			__attribute__((__no_instrument_function__))
```

这一句，是定义了一个属性，这个属性可以用来修饰一个函数，指定这个函数不被跟踪。那么这个属性到底是怎么回子事呢？原来，在gcc编译器里面，实现了一个非常强大的功能，如果在编译的时候把一个相应的选择项打开，那么就可以在执行完程序的时候，用一些工具来显示整个函数被调用的过程，这样就不需要让程序员手动在所有的函数里一点点添加能显示函数被调用过程的语句，这样耗时耗力，还容易出错。那么对应在应用程序方面，可以使用Graphviz这个工具来进行显示，至于使用说明与软件实现的原理可以自己在网上查一查，很容易查到。对应于内核，因为内核一直是在运行阶段，所以就不能使用这套东西了，内核是在自己的内部实现了一个ftrace的机制，编译内核的时候，如果打开这个选项，那么通过挂载一个debugfs的文件系统来进行相应内容的显示，具体的操作步骤，可以参看内核源码所带的文档。那上面说了这么多，与notrace这个属性有什么关系呢？因为在进行函数调用流程的显示过程中，是使用了两个特殊的函数的，当函数被调用与函数被执行完返回之前，都会分别调用这两个特别的函数。如果不把这两个函数的函数指定为不被跟踪的属性，那么整个跟踪的过程就会陷入一个无限循环当中。

## 3.12. __randomize_layout

```c
# define __randomize_layout __designated_init
```

## 3.13. __designated_init
```c
# define __designated_init              __attribute__((__designated_init__))
```

## 3.14. __has_attribute

```c
# define __has_attribute(x) __GCC4_has_attribute_##x
```

具体关于属性的内容很多，不再一一讲解，详情请看源代码文件：

* `include\linux\compiler_attributes.h`
* `include\linux\compiler_types.h`


# 4. __builtin_

## 4.1. likely,unlikely
```c
#define likely(x)	(__builtin_expect(!!(x), 1))
#define unlikely(x)	(__builtin_expect(!!(x), 0))
```
这两句是一对对应关系。__builtin_expect(expr, c)这个函数是新版gcc支持的，它是用来作代码优化的，用来告诉编译器，expr的期，非常有可能是c，这样在gcc生成对应的汇编代码的时候，会把相应的可能执行的代码都放在一起，这样能少执行代码的跳转。为什么这样能提高CPU的执行效率呢？因为CPU在执行的时候，都是有预先取指令的机制的，把将要执行的指令取出一部分出来准备执行。CPU不知道程序的逻辑，所以都是从可程序程序里挨着取的，如果这个时候，能不做跳转，则CPU预先取出的指令都可以接着使用，反之，则预先取出来的指令都是没有用的。还有个问题是需要注意的，在__builtin_expect的定义中，以前的版本是没有!!这个符号的，这个符号的作用其实就是负负得正，为什么要这样做呢？就是为了保证非零的x的值，后来都为1，如果为零的0值，后来都为0，仅此而已。

## 4.2. __same_type

```c
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))
```

# 5. asm


## 5.1. barrier
```c
# define barrier() __asm__ __volatile__("": : :"memory")
```

这里表示如果没有定义barrier函数，则定义barrier()函数为__memory_barrier()。但在内核代码里，是会包含compiler-gcc.h这个文件的，所以在这个文件里，定义barrier()为__asm__ __volatile__("": : :"memory")。barrier翻译成中文就是屏障的意思，在这里，为什么要一个屏障呢？这是因为CPU在执行的过程中，为了优化指令，可能会对部分指令以它自己认为最优的方式进行执行，这个执行的顺序并不一定是按照程序在源码内写的顺序。编译器也有可能在生成二进制指令的时候，也进行一些优化。这样就有可能在多CPU，多线程或是互斥锁的执行中遇到问题。那么这个内存屏障可以看作是一条线，内存屏障用在这里，就是为了保证屏障以上的操作，不会影响到屏障以下的操作。然后再看看这个屏障怎么实现的。__asm__表示后面的东西都是汇编指令，当然，这是一种在C语言中嵌入汇编的方法，语法有其特殊性，我在这里只讲跟这条指令有关的。__volatile__表示不对此处的汇编指令做优化，这样就会保证这里代码的正确性。""表示这里是个空指令，那么既然是空指令，则所对应的指令所需要的输入与输出都没有。在gcc中规定，如果以这种方式嵌入汇编，如果输出没有，则需要两个冒号来代替输出操作数的位置，所以需要加两个::，这时的指令就为"" : :。然后再加上为分隔输入而加入的冒号，再加上空的输入，即为"" : : :。后面的memory是gcc中的一个特殊的语法，加上它，gcc编译器则会产生一个动作，这个动作使gcc不保留在寄存器内内存的值，并且对相应的内存不会做存储与加载的优化处理，这个动作不产生额外的代码，这个行为是由gcc编译器来保证完成的。如果对这部分有更大的兴趣，可以考察gcc的帮助文档与内核中一篇名为memory-barriers.txt的文章。

## 5.2. asm_volatile_goto
```c
#define asm_volatile_goto(x...) asm goto(x)
```
这在Jump_label中会用到。


## 5.3. RELOC_HIDE
```c
/*
 * This macro obfuscates arithmetic on a variable address so that gcc
 * shouldn't recognize the original var, and make assumptions about it.
 *
 * This is needed because the C standard makes it undefined to do
 * pointer arithmetic on "objects" outside their boundaries and the
 * gcc optimizers assume this is the case. In particular they
 * assume such arithmetic does not wrap.
 *
 * A miscompilation has been observed because of this on PPC.
 * To work around it we hide the relationship of the pointer and the object
 * using this macro.
 *
 * Versions of the ppc64 compiler before 4.1 had a bug where use of
 * RELOC_HIDE could trash r30. The bug can be worked around by changing
 * the inline assembly constraint from =g to =r, in this particular
 * case either is valid.
 */
#define RELOC_HIDE(ptr, off)						\
({									\
	unsigned long __ptr;						\
	__asm__ ("" : "=r"(__ptr) : "0"(ptr));				\
	(typeof(ptr)) (__ptr + (off));					\
})
```


# 6. 参考

1. [https://blog.csdn.net/zb872676223/article/details/39929305](https://blog.csdn.net/zb872676223/article/details/39929305)