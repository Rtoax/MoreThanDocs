<center><font size='6'>__nptl_deallocate_tsd</font></center>
<br/>
<center><font size='2'>rtoax</font></center>
<center><font size='2'>2021年5月25日</font></center>
<br/>

记一次由于`pthread_key_create`导致的`__nptl_deallocate_tsd`。

* 版本：`glibc-2.17`
* [完整示例代码](https://github.com/Rtoax/test/tree/master/glibc/segmentation-fault/__nptl_deallocate_tsd)

# 1. 简介
```c
#include <pthread.h>

int pthread_key_create(pthread_key_t *key, void (*destructor)(void*));
```

其中`destructor`为析构函数，它将在`__nptl_deallocate_tsd`中被调用。

# 2. Coredump：`__nptl_deallocate_tsd`

```c
Thread 2 "a.out" received signal SIGSEGV, Segmentation fault.
[Switching to Thread 0x7ffff77f0700 (LWP 140173)]
__GI___libc_free (mem=0x1) at malloc.c:2941
2941	  if (chunk_is_mmapped(p))                       /* release mmapped memory. */
(gdb) 
(gdb) bt
#0  __GI___libc_free (mem=0x1) at malloc.c:2941
#1  0x00007ffff7bc6c62 in __nptl_deallocate_tsd () at pthread_create.c:155
#2  0x00007ffff7bc6e73 in start_thread (arg=0x7ffff77f0700) at pthread_create.c:314
#3  0x00007ffff78ef88d in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:111
```

为什么会这样？为什么会执行析构函数。一步一步分析。

首先创建key，这里我们挂入malloc对应的free，因为我们将为每个线程的key使用malloc分配内存：

```c
pthread_key_create(&key, free);
```

然后创建线程：

```c
pthread_create(&thid1, NULL, thread1, NULL);
```
在thread1中申请内存并将其设置为该线程的TLS的key值：

```c
int *key_va = malloc(sizeof(int)) ;
*key_va = 2;
pthread_setspecific(key, (void*)key_va);
```

当线程thread1执行结束后，主线程调用pthread_join回收线程，这时候析构函数将被执行：

```c
pthread_join(thid1, NULL);
```

> 这里我在调研过程中，有些文章也讲到，由于key的malloc对应的析构函数被设置为NULL，导致内存泄漏。

当key使用完毕后进行删除：

```c
pthread_key_delete(key);
```

示例代码见[完整示例代码](https://github.com/Rtoax/test/tree/master/glibc/segmentation-fault/__nptl_deallocate_tsd)或者文末章节。

那么段错误如何产生呢？

我们还是使用free填充析构函数：

```c
pthread_key_create(&key, free);
```
但是我将`pthread_setspecific`入参的value填写问静态变量值：

```c
int key_va = 2;
pthread_setspecific(key, (void*)key_va);
```
没错，这时候在运行程序并用gdb调试，就会产生文章开头描述的段错误。


# 3. 不显示调用`pthread_key_create`的段错误

不显示调用`pthread_key_create`的程序也可能出错，比如说文末章节的实例sane.c.

> 不能对开源软件有过高的要求，有bug大家一起解决。

```c
void* scan_thread(void *arg) {
    
    SANE_Status status;

    status = sane_init(NULL, NULL);
    assert(status == SANE_STATUS_GOOD);

    const SANE_Device** device_list = NULL;
    status = sane_get_devices(&device_list, false);
    assert(status == SANE_STATUS_GOOD);
	int i;

    for(i = 0; device_list[i] != NULL; ++i){
        printf("%s\n", device_list[i]->name);
    }

    sane_exit();
}
```

gdb运行程序，并设置断点：

```c
(gdb) b pthread_key_create
Function "pthread_key_create" not defined.
Make breakpoint pending on future shared library load? (y or [n]) y
Breakpoint 1 (pthread_key_create) pending.
```
到达断点：
```c
Thread 2 "test.c.out" hit Breakpoint 1, __GI___pthread_key_create (key=key@entry=0x7ffff75c20c0 <key>, 
    destr=destr@entry=0x7ffff73c01f0 <free_key_mem>) at pthread_key_create.c:26
26	{
```
继续执行，产生段错误：

```c
(gdb) c
Continuing.

Thread 2 "test.c.out" hit Breakpoint 1, __GI___pthread_key_create (key=0x7fffe4eb6dc8, 
    destr=0x7fffe4c77600 <cups_globals_free>) at pthread_key_create.c:26
26	{
(gdb) c
Continuing.

Thread 2 "test.c.out" received signal SIGSEGV, Segmentation fault.
0x00007fffe4c77600 in ?? ()
(gdb) bt
#0  0x00007fffe4c77600 in ?? ()
#1  0x00007ffff7998c62 in __nptl_deallocate_tsd () at pthread_create.c:155
#2  0x00007ffff7998e73 in start_thread (arg=0x7ffff4168700) at pthread_create.c:314
#3  0x00007ffff76c188d in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:111
(gdb) 
```
这种问题如何解决呢？

* 去除`sane_exit();`，但是可能面临内存泄漏的危险；
* TODO：走读sane-backends源码，提交patch；


# 4. 示例代码

## 4.1. main.c

```c
#include <pthread.h>
#include <stdio.h>
#include <malloc.h>

pthread_key_t key;
pthread_t thid1;
pthread_t thid2;

#ifndef MALLOC_KEY
#define MALLOC_KEY  0
#endif

void* thread2(void* arg)
{
    printf("thread:%lu is running\n", pthread_self());
#if MALLOC_KEY    
    int *key_va = malloc(sizeof(int)) ;
    *key_va = 2;
#else
    int key_va = 2;
#endif
    pthread_setspecific(key, (void*)key_va);
    
    printf("thread:%lu return %d\n", pthread_self(), (int)pthread_getspecific(key));
}


void* thread1(void* arg)
{
    printf("thread:%lu is running\n", pthread_self());

#if MALLOC_KEY    
    int *key_va = malloc(sizeof(int)) ;
    *key_va = 1;
#else
    int key_va = 1;
#endif
    
    pthread_setspecific(key, (void*)key_va);

    pthread_create(&thid2, NULL, thread2, NULL);

    printf("thread:%lu return %d\n", pthread_self(), (int)pthread_getspecific(key));
}


int main()
{
    printf("main thread:%lu is running\n", pthread_self());

    //如果 pthread_setspecific 传入的是局部变量，
    //并且 pthread_key_create 传入了析构函数，
    //那么将产生如下段错误
    //#0  __GI___libc_free (mem=0x5) at malloc.c:2941
    //#1  0x00007feb4c550c62 in __nptl_deallocate_tsd () at pthread_create.c:155
    //#2  0x00007feb4c550e73 in start_thread (arg=0x7feb4c17a700) at pthread_create.c:314
    //#3  0x00007feb4c27988d in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:111
    pthread_key_create(&key, free); //这里会段错误 

    pthread_create(&thid1, NULL, thread1, NULL);

    pthread_join(thid1, NULL);
    pthread_join(thid2, NULL);

#if MALLOC_KEY    
    int *key_va = malloc(sizeof(int)) ;
    *key_va = 2;
#else
    int key_va = 2;
#endif
    pthread_setspecific(key, (void*)key_va);
    
    printf("thread:%lu return %d\n", pthread_self(), (int)pthread_getspecific(key));

    pthread_key_delete(key);
        
    printf("main thread exit\n");
    return 0;
}
```

## 4.2. sane.c

此示例参见[https://bugzilla.redhat.com/show_bug.cgi?id=1065695](https://bugzilla.redhat.com/show_bug.cgi?id=1065695)。

```c
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <malloc.h>
#include <sane/sane.h>

#define PTHREAD_STACK_MIN	16384

void* scan_thread(void *arg) {
    
    SANE_Status status;

    status = sane_init(NULL, NULL);
    assert(status == SANE_STATUS_GOOD);

    const SANE_Device** device_list = NULL;
    status = sane_get_devices(&device_list, false);
    assert(status == SANE_STATUS_GOOD);
	int i;

    for(i = 0; device_list[i] != NULL; ++i){
        printf("%s\n", device_list[i]->name);
    }

    sane_exit();
}

int main()
{    
	pthread_t t;
    pthread_attr_t attr;
    void *stackAddr = NULL;
    int paseSize = getpagesize();
    size_t stacksize = paseSize*4;
    
    pthread_attr_init(&attr);
    posix_memalign(&stackAddr, paseSize, stacksize);
    pthread_attr_setstack(&attr, stackAddr, stacksize);
    
	pthread_create(&t, NULL, scan_thread, NULL);
    pthread_join(t, NULL);
    return 0;
}
```
