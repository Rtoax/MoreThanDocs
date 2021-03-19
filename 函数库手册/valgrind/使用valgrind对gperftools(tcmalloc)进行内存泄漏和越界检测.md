<center><font size='6'>使用valgrind对gperftools(tcmalloc)进行内存泄漏和越界检测</font></center>
<br/>
<center><font size='5'>RToax</font></center>
<center><font size='5'>2020年12月</font></center>
<br/>

# 1. 问题引入

在[《内存分配器ptmalloc,jemalloc,tcmalloc调研与对比》](https://rtoax.blog.csdn.net/article/details/110689404)中已经对几种内存分配器进行了性能比较，其中tcmalloc(gperftools)性能较为突出。在编译应用程序时添加`-ltcmalloc`选项，就可以把源glibc的malloc等内存分配函数重定向到gperftools中。但是，若产生了内存泄漏和内存越界问题，使用gperftools原生的pprof并不能有效的对内存泄漏进行精确的统计（经过我统计，在泄漏次数上有误差），并且对内存越界访问问题pprof是不支持的。

# 2. 解决方案

libc原生的valgrind工具对libc的malloc函数有很好的支持，但是，一个程序在编译过程制定了动态库libtcmalloc.so，如何将它的malloc接口重定向到glibc中呢？采用`LD_PRELOAD`能很好的解决。
那我们也就得出了使用LD_PRELOAD+dlfcn的方式解决，并通过valgrind进行内存越界和泄漏检测。

# 3. 先给出结论

* 使用valgrind 完全支持tcmalloc的内存泄漏和越界检测。

# 4. valgrind内存检测

首先看下手册：
```
NAME
       valgrind - a suite of tools for debugging and profiling programs

SYNOPSIS
       valgrind [valgrind-options] [your-program] [your-program-options]

DESCRIPTION
       Valgrind is a flexible program for debugging and profiling Linux executables. It consists of a core,...
       
TOOL SELECTION OPTIONS
       The single most important option.

       --tool=<toolname> [default: memcheck]
           Run the Valgrind tool called toolname, e.g. memcheck, cachegrind, callgrind, helgrind, drd, massif,
           lackey, none, exp-sgcheck, exp-bbv, exp-dhat, etc.
```
这里显然使用`--tool=memcheck`功能，先不说别的，直接给出一个脚本：
```bash
#!/bin/bash

# --leak-check=full 完全检查内存泄漏
# --show-reachable=yes 显示内存泄漏地点
# --trace-children=yes 跟入子进程
# --quiet 只打印错误信息

if [ $# -lt 1 ]; then
	echo "usage: $0 [program + arguments]"
	exit
fi

valgrind --leak-check=full \
		 --show-reachable=yes \
		 --trace-children=yes \
		 --quiet \
		 $*
```

## 4.1. valgrind检测内存泄漏

### 4.1.1. 测试例
```c
#include <malloc.h>
#include <stdio.h>
#include <pthread.h>

#ifndef NOLEAK
#define LEAK 1
#endif

void* test_task_fn(void* unused)
{
	printf("test_task_fn.\n");
    
    char *str = malloc(64);

    int i;
#ifdef LEAK    
    for (i=0; i<2; i++) {
        str = malloc(64);
    }
#endif //LEAK    
    free(str);

    pthread_exit(NULL);
	return NULL;
}

/* The main program. */
int main ()
{
	pthread_t thread_id;
    
	pthread_create(&thread_id, NULL, test_task_fn, NULL);

	pthread_join(thread_id, NULL);

    printf("Exit program.\n");

	return 0;
}
```
上述程序在宏定义`LEAK`存在内存泄漏，使用上面的脚本对该程序编译的可执行文件进行内存泄漏检测，可得到：

```
$ ./valgrind--leak-check.sh ./leak.out 
test_task_fn.
Exit program.
==91903== 64 bytes in 1 blocks are definitely lost in loss record 1 of 2
==91903==    at 0x4C29F93: malloc (vg_replace_malloc.c:307)
==91903==    by 0x4006FC: test_task_fn (in /work/workspace/test/valgrind/study/leak.out)
==91903==    by 0x4E3EE64: start_thread (in /usr/lib64/libpthread-2.17.so)
==91903==    by 0x515188C: clone (in /usr/lib64/libc-2.17.so)
==91903== 
==91903== 64 bytes in 1 blocks are definitely lost in loss record 2 of 2
==91903==    at 0x4C29F93: malloc (vg_replace_malloc.c:307)
==91903==    by 0x400713: test_task_fn (in /work/workspace/test/valgrind/study/leak.out)
==91903==    by 0x4E3EE64: start_thread (in /usr/lib64/libpthread-2.17.so)
==91903==    by 0x515188C: clone (in /usr/lib64/libc-2.17.so)
==91903== 
```
可以清楚的检测出内存泄漏的位置。

## 4.2. valgrind检测内存越界

### 4.2.1. 测试例
```c
#include <malloc.h>
#include <stdio.h>
#include <pthread.h>

#ifndef NOCLOBBER
#define CLOBBER 1
#endif

void* test_task_fn(void* unused)
{
	printf("test_task_fn.\n");

    char *str_pad1 = malloc(64);
    char *str = malloc(64);
    char *str_pad2 = malloc(6400);
    
    str_pad1[63] = 'Z';
    str_pad2[0] = 'Z';
    
#ifdef CLOBBER
    str[-65] = 'A'; //检测不到
    str[-64] = 'A'; //能检测到
//    str[-1] = 'A';  //能检测到
//    str[64] = 'A';  //能检测到
    str[127] = 'A'; //能检测到
    str[128] = 'A'; //检测不到
#else
    str[0] = 'A';
    str[63] = 'A';
#endif

    printf("%c\n", str_pad1[63]);
    printf("%c\n", str_pad2[0]);
    
    free(str);
    free(str_pad1);
    free(str_pad2);
    
    pthread_exit(NULL);
}

/* The main program. */
int main ()
{
	pthread_t thread_id;
    
	pthread_create(&thread_id, NULL, test_task_fn, NULL);

	pthread_join(thread_id, NULL);

    printf("Exit program.\n");

	return 0;
}
```
在宏定义`CLOBBER`处发生了几次内存越界现象，但是遗憾的是，valgrind通过**红区**（在内核中叫做**金丝雀**，当然在其他情况下也可能叫做**金丝雀**，也可能是**魔数**）进行越界检测。下面的示例图中可以清楚看出当使用valgrind运行程序时候，在每次分配的内存前后均添加了红区（标记为#####），使用valgrind之前的内存布局：
```
      |       str_pad1    |   str   |     str_pad2       |
      +-------------------+---------+--------------------+
```
使用valgrind之后的内存布局：
```
|     |       str_pad1    |     |   str   |     |    str_pad2      |     |
+#####+-------------------+#####+---------+#####+------------------+#####+
```
可见如果越界将把红区中的魔数篡改，这是不允许的。
下面给出内存越界的检测结果，根据代码中的注释，有些内存越界是检测不到的，是因为str越界位置不在红区内，而在str_pad中（这在str_pad的打印中得以验证）。
```
$ ./valgrind--leak-check.sh ./clobber.out 
test_task_fn.
==92428== Thread 2:
==92428== Invalid write of size 1
==92428==    at 0x400792: test_task_fn (in /work/workspace/test/valgrind/study/clobber.out)
==92428==    by 0x4E3EE64: start_thread (in /usr/lib64/libpthread-2.17.so)
==92428==    by 0x515188C: clone (in /usr/lib64/libc-2.17.so)
==92428==  Address 0x5c222f0 is 0 bytes after a block of size 64 alloc'd
==92428==    at 0x4C29F93: malloc (vg_replace_malloc.c:307)
==92428==    by 0x40074C: test_task_fn (in /work/workspace/test/valgrind/study/clobber.out)
==92428==    by 0x4E3EE64: start_thread (in /usr/lib64/libpthread-2.17.so)
==92428==    by 0x515188C: clone (in /usr/lib64/libc-2.17.so)
==92428== 
==92428== Invalid write of size 1
==92428==    at 0x40079D: test_task_fn (in /work/workspace/test/valgrind/study/clobber.out)
==92428==    by 0x4E3EE64: start_thread (in /usr/lib64/libpthread-2.17.so)
==92428==    by 0x515188C: clone (in /usr/lib64/libc-2.17.so)
==92428==  Address 0x5c223af is 1 bytes before a block of size 6,400 alloc'd
==92428==    at 0x4C29F93: malloc (vg_replace_malloc.c:307)
==92428==    by 0x400768: test_task_fn (in /work/workspace/test/valgrind/study/clobber.out)
==92428==    by 0x4E3EE64: start_thread (in /usr/lib64/libpthread-2.17.so)
==92428==    by 0x515188C: clone (in /usr/lib64/libc-2.17.so)
==92428== 
A
A
Exit program.
```

## 4.3. tcmalloc的追踪

依然采用以上leak.c测试例进行泄漏检测，同时采用tcmalloc，编译：
```c
$ gcc leak.c -pthread -ltcmalloc
```
运行：
```
$ ./valgrind--leak-check.sh ./a.out 
==94268== Mismatched free() / delete / delete []
==94268==    at 0x4C2B08D: free (vg_replace_malloc.c:538)
==94268==    by 0x4E4D93F: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268==  Address 0x60351a0 is 0 bytes inside a block of size 4 alloc'd
==94268==    at 0x4C2AC58: operator new[](unsigned long) (vg_replace_malloc.c:431)
==94268==    by 0x4E4D937: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268== 
==94268== Mismatched free() / delete / delete []
==94268==    at 0x4C2B08D: free (vg_replace_malloc.c:538)
==94268==    by 0x58D471A: std::string::reserve(unsigned long) (in /usr/lib64/libstdc++.so.6.0.19)
==94268==    by 0x58D493E: std::string::append(char const*, unsigned long) (in /usr/lib64/libstdc++.so.6.0.19)
==94268==    by 0x4E62160: MallocExtension::Initialize() (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x4E4D944: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268==  Address 0x6035820 is 0 bytes inside a block of size 47 alloc'd
==94268==    at 0x4C2AC58: operator new[](unsigned long) (vg_replace_malloc.c:431)
==94268==    by 0x58D3A18: std::string::_Rep::_S_create(unsigned long, unsigned long, std::allocator<char> const&) 
(in /usr/lib64/libstdc++.so.6.0.19)==94268==    by 0x58D52A0: char* std::string::_S_construct<char const*>(char const*, char const*, std::allocator<ch
ar> const&, std::forward_iterator_tag) (in /usr/lib64/libstdc++.so.6.0.19)==94268==    by 0x58D56D7: std::basic_string<char, std::char_traits<char>, std::allocator<char> >::basic_string(cha
r const*, std::allocator<char> const&) (in /usr/lib64/libstdc++.so.6.0.19)==94268==    by 0x4E6214C: MallocExtension::Initialize() (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x4E4D944: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268== 
==94268== Mismatched free() / delete / delete []
==94268==    at 0x4C2B08D: free (vg_replace_malloc.c:538)
==94268==    by 0x4E6219B: MallocExtension::Initialize() (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x4E4D944: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268==  Address 0x6035890 is 0 bytes inside a block of size 69 alloc'd
==94268==    at 0x4C2AC58: operator new[](unsigned long) (vg_replace_malloc.c:431)
==94268==    by 0x58D3A18: std::string::_Rep::_S_create(unsigned long, unsigned long, std::allocator<char> const&) 
(in /usr/lib64/libstdc++.so.6.0.19)==94268==    by 0x58D462A: std::string::_Rep::_M_clone(std::allocator<char> const&, unsigned long) (in /usr/lib64/l
ibstdc++.so.6.0.19)==94268==    by 0x58D46D3: std::string::reserve(unsigned long) (in /usr/lib64/libstdc++.so.6.0.19)
==94268==    by 0x58D493E: std::string::append(char const*, unsigned long) (in /usr/lib64/libstdc++.so.6.0.19)
==94268==    by 0x4E62160: MallocExtension::Initialize() (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x4E4D944: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268== 
test_task_fn.
Exit program.
```
可见存在大量的new/delete和tcmalloc的`__attribute__((constructor(101)))`相关的内容，如何解决呢？

> `__attribute__((constructor(101)))`在下一章节中解释。

使用如下选项忽略相关的检测结果即可：

```
$ man valgrind
    ...
       --suppressions=<filename> [default: $PREFIX/lib/valgrind/default.supp]
           Specifies an extra file from which to read descriptions of errors to suppress. You may use up to 100
           extra suppression files.

       --gen-suppressions=<yes|no|all> [default: no]
           When set to yes, Valgrind will pause after every error shown and print the line:

                   ---- Print suppression ? --- [Return/N/n/Y/y/C/c] ----
    ...
```

首先生成过滤配置文件：

```bash
$ ./valgrind--gen-suppressions.sh ./a.out 
==94720== Mismatched free() / delete / delete []
==94720==    at 0x4C2B08D: free (vg_replace_malloc.c:538)
==94720==    by 0x4E4D93F: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94720==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94720==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94720==  Address 0x60351a0 is 0 bytes inside a block of size 4 alloc'd
==94720==    at 0x4C2AC58: operator new[](unsigned long) (vg_replace_malloc.c:431)
==94720==    by 0x4E4D937: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94720==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94720==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94720== 
==94720== 
==94720== ---- Print suppression ? --- [Return/N/n/Y/y/C/c] ---- 
```
输入`y`。
```
==94720== ---- Print suppression ? --- [Return/N/n/Y/y/C/c] ---- y
{
   <insert_a_suppression_name_here>
   Memcheck:Free
   fun:free
   obj:/usr/lib64/libtcmalloc.so.4.4.5
   fun:_dl_init
   obj:/usr/lib64/ld-2.17.so
}
```
将以上内存保存至文件中，继续重复上述步骤。

然后再次使用valgrind的suppressions功能运行应用程序，进行内存泄漏检测。
```bash
$ ./valgrind--suppressions.sh ./a.out tcmalloc-suppressions.txt 
test_task_fn.
Exit program.
==94872== 64 bytes in 1 blocks are definitely lost in loss record 4 of 5
==94872==    at 0x4C29F93: malloc (vg_replace_malloc.c:307)
==94872==    by 0x4007CC: test_task_fn (in /work/workspace/test/valgrind/study/a.out)
==94872==    by 0x5233E64: start_thread (in /usr/lib64/libpthread-2.17.so)
==94872==    by 0x554688C: clone (in /usr/lib64/libc-2.17.so)
==94872== 
==94872== 64 bytes in 1 blocks are definitely lost in loss record 5 of 5
==94872==    at 0x4C29F93: malloc (vg_replace_malloc.c:307)
==94872==    by 0x4007E3: test_task_fn (in /work/workspace/test/valgrind/study/a.out)
==94872==    by 0x5233E64: start_thread (in /usr/lib64/libpthread-2.17.so)
==94872==    by 0x554688C: clone (in /usr/lib64/libc-2.17.so)
```
可见大量的冗余信息已经消失。

两个脚本内容如下：

1. valgrind--gen-suppressions.sh 

```bash
#!/bin/bash

# --leak-check=full 完全检查内存泄漏
# --show-reachable=yes 显示内存泄漏地点
# --trace-children=yes 跟入子进程
# --quiet 只打印错误信息
# --gen-suppressions=yes 生成抑制文件
function gen-suppressions() {
	if [ $# -lt 1 ]; then
		echo "usage: $0 [program]"
		exit
	fi

	valgrind --leak-check=full \
			 --show-reachable=yes \
			 --trace-children=yes \
			 --gen-suppressions=yes \
			 --quiet \
			 $*
}

# --tool=memcheck
# --quiet 只打印错误信息
function gen-suppressions2() {
	if [ $# -lt 1 ]; then
		echo "usage: $0 [program]"
		exit
	fi

#  --default-suppressions=no
#  --suppressions=/path/to/file.supp
	valgrind --tool=memcheck \
			 --gen-suppressions=yes \
			 --quiet \
			$*

# 然后调用
#callgrind_annotate --auto=yes \
#				callgrind.out.[pid]
}
gen-suppressions $*

```

2. valgrind--suppressions.sh 

```bash
#!/bin/bash

# --leak-check=full 完全检查内存泄漏
# --show-reachable=yes 显示内存泄漏地点
# --trace-children=yes 跟入子进程
# --quiet 只打印错误信息
# --gen-suppressions=yes 生成抑制文件
# --suppressions=file.txt 指定抑制文件
function suppressions() {
	if [ $# -lt 2 ]; then
		echo "usage: $0 [program] [suppressions file]"
		if [ ! -f $2 ]; then
			echo "File <$2> not exist"
		fi
		exit
	fi

	valgrind --leak-check=full \
			 --show-reachable=yes \
			 --trace-children=yes \
			 --suppressions=$2 \
			 --quiet \
			 $1
}

suppressions $*
```


# 5. libc的内存分配钩子

## 5.1. 钩子实现
采用dlsym系列接口可以从动态库中动态加载需要执行的函数，其声明如下：

```c
#include <dlfcn.h>

void *dlopen(const char *filename, int flag);
char *dlerror(void);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);
```
如果我们要从系统中获取一个函数钩子，但又不知道函数具体在那个库文件中，可以采用如下的handle
```c
/* If the first argument of `dlsym' or `dlvsym' is set to RTLD_NEXT
   the run-time address of the symbol called NAME in the next shared
   object is returned.  The "next" relation is defined by the order
   the shared objects were loaded.  */
# define RTLD_NEXT	((void *) -1l)

/* If the first argument to `dlsym' or `dlvsym' is set to RTLD_DEFAULT
   the run-time address of the symbol called NAME in the global scope
   is returned.  */
# define RTLD_DEFAULT	((void *) 0)
```

首先定义钩子函数数据类型：
```c
typedef void *(*malloc_fn_t)(size_t size);
typedef void  (*free_fn_t)(void *ptr);
typedef void *(*calloc_fn_t)(size_t nmemb, size_t size);
typedef void *(*realloc_fn_t)(void *ptr, size_t size);

typedef char *(*strdup_fn_t)(const char *s);
typedef char *(*strndup_fn_t)(const char *s, size_t n);
```
然后声明从系统库中获取函数钩子：
```c
static malloc_fn_t      g_sys_malloc_func = NULL;
static free_fn_t        g_sys_free_func = NULL;
static calloc_fn_t      g_sys_calloc_func = NULL;
static realloc_fn_t     g_sys_realloc_func = NULL;
static strdup_fn_t      g_sys_strdup_func = NULL;
static strndup_fn_t     g_sys_strndup_func = NULL;
```
对于上述的函数指针，操作是一样的，所以定义宏：
```c
#define HOOK_SYS_FUNC(name) \
    if( !g_sys_##name##_func ) { \
        g_sys_##name##_func = (name##_fn_t)dlsym(RTLD_NEXT,#name);\
        if(!g_sys_##name##_func) { \
            log_debug("Warning: failed to find "#name" in sys_RTLD_NEXT.\n"); \
        }\
    }
```
接下来从系统库中获取函数钩子指针：
```c
static void __real_sys_malloc(void)
{
    HOOK_SYS_FUNC(malloc);
    HOOK_SYS_FUNC(free);
    HOOK_SYS_FUNC(calloc);
    HOOK_SYS_FUNC(realloc);
    HOOK_SYS_FUNC(strdup);
    HOOK_SYS_FUNC(strndup);
}
```
后续我们就可以使用`g_sys_malloc_func`代替`malloc`进行内存分配。

> 如果我不想显示的在main函数中调用`__real_sys_malloc`怎么办呢？不用怕，gcc提供了attribute预处理原语：

```c
__attribute__((constructor(101)))
```
其中101为优先级（越小越先执行）。用改attribute定义的函数将在main函数之前执行，也就是说可以这么做：
```c

void __attribute__((constructor(101))) __dlsym_sys_func_init()
{
    log_debug("Memory allocator redirect start.\n");

    __real_sys_malloc();
    
    log_debug("Memory allocator redirect done.\n");
}
```

### 5.1.1. mymalloc.c
完整代码：
```c
#include <stdio.h>

#define __USE_GNU
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef NODEBUG
#define log_debug(str) do {} while(0)
#else
#define log_debug(str) do {\
        write(2, str, sizeof(str)); \
    } while(0)
#endif

#ifndef LIBC_SO
#define LIBC_SO "libc.so.6"//"/usr/lib64/libc.so.6"
#endif

typedef void *(*malloc_fn_t)(size_t size);
typedef void  (*free_fn_t)(void *ptr);
typedef void *(*calloc_fn_t)(size_t nmemb, size_t size);
typedef void *(*realloc_fn_t)(void *ptr, size_t size);

typedef char *(*strdup_fn_t)(const char *s);
typedef char *(*strndup_fn_t)(const char *s, size_t n);

static malloc_fn_t      g_sys_malloc_func = NULL;
static free_fn_t        g_sys_free_func = NULL;
static calloc_fn_t      g_sys_calloc_func = NULL;
static realloc_fn_t     g_sys_realloc_func = NULL;
static strdup_fn_t      g_sys_strdup_func = NULL;
static strndup_fn_t     g_sys_strndup_func = NULL;

static malloc_fn_t      g_libc_real_malloc_func = NULL;
static free_fn_t        g_libc_real_free_func = NULL;
static calloc_fn_t      g_libc_real_calloc_func = NULL;
static realloc_fn_t     g_libc_real_realloc_func = NULL;
static strdup_fn_t      g_libc_real_strdup_func = NULL;
static strndup_fn_t     g_libc_real_strndup_func = NULL;

#define HOOK_SYS_FUNC(name) \
    if( !g_sys_##name##_func ) { \
        g_sys_##name##_func = (name##_fn_t)dlsym(RTLD_NEXT,#name);\
        if(!g_sys_##name##_func) { \
            log_debug("Warning: failed to find "#name" in sys_RTLD_NEXT.\n"); \
        }\
    }

#define HOOK_LIBC_FUNC(name, dl) \
    if( !g_libc_real_##name##_func ) { \
        g_libc_real_##name##_func = (name##_fn_t)dlsym(dl,#name);\
        if(!g_libc_real_##name##_func) { \
            log_debug("Warning: failed to find "#name" in "LIBC_SO".\n"); \
        }\
    }
    
static void __real_sys_malloc(void)
{
    HOOK_SYS_FUNC(malloc);
    HOOK_SYS_FUNC(free);
    HOOK_SYS_FUNC(calloc);
    HOOK_SYS_FUNC(realloc);
    HOOK_SYS_FUNC(strdup);
    HOOK_SYS_FUNC(strndup);
}

static void __real_libc_malloc(void)
{
    log_debug("load libc real malloc start.\n");

    void *libc = dlopen(LIBC_SO, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (!libc) {
        log_debug("[WARN] failed to find "LIBC_SO"\n");
        return;
    }
    HOOK_LIBC_FUNC(malloc, libc);
    HOOK_LIBC_FUNC(free, libc);
    HOOK_LIBC_FUNC(calloc, libc);
    HOOK_LIBC_FUNC(realloc, libc);
    HOOK_LIBC_FUNC(strdup, libc);
    HOOK_LIBC_FUNC(strndup, libc);
    
    log_debug("load "LIBC_SO" real malloc done.\n");
}

void __attribute__((constructor(101))) __dlsym_sys_func_init()
{
    log_debug("Memory allocator redirect start.\n");

    __real_sys_malloc();
    __real_libc_malloc();
    
    log_debug("Memory allocator redirect done.\n");
}

void *malloc(size_t size)
{
    if(g_sys_malloc_func) {
        log_debug("g_sys_malloc_func called.\n");
        return g_sys_malloc_func(size);
    }
    if(g_libc_real_malloc_func) {
        log_debug("g_libc_real_malloc_func called.\n");
        return g_libc_real_malloc_func(size);
    }
    log_debug("[ERROR] malloc null.\n");
    return NULL;
}

void free(void *ptr)
{
    if(g_sys_free_func) {
        log_debug("g_sys_free_func called.\n");
        g_sys_free_func(ptr);
        return ;
    }
    if(g_libc_real_free_func) {
        log_debug("g_libc_real_free_func called.\n");
        g_libc_real_free_func(ptr);
        return ;
    }
    log_debug("[ERROR] free null.\n");
    return ;
}

void *calloc(size_t nmemb, size_t size)
{
    if(g_sys_calloc_func) {
        log_debug("g_sys_calloc_func called.\n");
        return g_sys_calloc_func(nmemb, size);
    }
    if(g_libc_real_calloc_func) {
        log_debug("g_libc_real_calloc_func called.\n");
        return g_libc_real_calloc_func(nmemb, size);
    }
    log_debug("[ERROR] calloc null.\n");
    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    if(g_sys_realloc_func) {
        log_debug("g_sys_realloc_func called.\n");
        return g_sys_realloc_func(ptr, size);
    }
    if(g_libc_real_realloc_func) {
        log_debug("g_libc_real_realloc_func called.\n");
        return g_libc_real_realloc_func(ptr, size);
    }
    log_debug("[ERROR] realloc null.\n");
    return NULL;
}

char *strdup(const char *s) 
{
    if(g_sys_strdup_func) {
        log_debug("g_sys_strdup_func called.\n");
        return g_sys_strdup_func(s);
    }
    if(g_libc_real_strdup_func) {
        log_debug("g_libc_real_strdup_func called.\n");
        return g_libc_real_strdup_func(s);
    }
    log_debug("[ERROR] strdup null.\n");
    return NULL;
}

char *strndup(const char *s, size_t n)
{
    if(g_sys_strndup_func) {
        log_debug("g_sys_strndup_func called.\n");
        return g_sys_strndup_func(s, n);
    }
    if(g_libc_real_strndup_func) {
        log_debug("g_libc_real_strndup_func called.\n");
        return g_libc_real_strndup_func(s, n);
    }
    log_debug("[ERROR] strndup null.\n");
    return NULL;
}

#ifndef NOTEST
#define TEST
#endif

#ifdef TEST
int main() 
{
    printf(">>>>>>>>>>>>>>>>>>>>>> main start <<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
    
    char *str1 = malloc(64);
    str1 = realloc(str1, 128);

    sprintf(str1, "Hello World.");
    printf("str1 = %s\n", str1);

    free(str1);
}
#endif
```
编译成动态库：
```bash
gcc mymalloc.c -ldl -I. -fPIC -shared -o libmymalloc.so
```

## 5.2. LD_PRELOAD

使用`LD_PRELOAD`执行程序，或者使用export指定环境变量`LD_PRELOAD`，即可将可重定位代码定位到`LD_PRELOAD`所指定的函数中。


## 5.3. 使用tcmalloc的静态库

### 5.3.1. vos.h
```c
#ifndef __VOS_H
#define __VOS_H 1

#include <sys/types.h>

#define vos_malloc(size) __vos_malloc(size, __func__, __LINE__)
#define vos_free(ptr) __vos_free(ptr, __func__, __LINE__)
#define vos_calloc(nmemb, size) __vos_calloc(nmemb, size, __func__, __LINE__)
#define vos_realloc(ptr, size) __vos_realloc(ptr, size, __func__, __LINE__)
#define vos_strdup(s) __vos_strdup(s, __func__, __LINE__)
#define vos_strndup(s, n) __vos_strndup(s, n, __func__, __LINE__)

void *__vos_malloc(size_t size, const char* _func, const int _line);
void __vos_free(void *ptr, const char* _func, const int _line);
void *__vos_calloc(size_t nmemb, size_t size, const char* _func, const int _line);
void *__vos_realloc(void *ptr, size_t size, const char* _func, const int _line);
char *__vos_strdup(const char *s, const char* _func, const int _line);
char *__vos_strndup(const char *s, size_t n, const char* _func, const int _line);

#endif /*<__VOS_H>*/
```

### 5.3.2. vos.c
```c
#include <vos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef VOS_DEBUG
#define vos_debug(f, l, fmt...) do {\
        fprintf(stderr, "[%s:%d call %s]\n", f, l, __func__); \
        fprintf(stderr, fmt);   \
    }while(0)
#else
#define vos_debug(f, l, fmt...) do {}while(0)
#endif

void *__vos_malloc(size_t size, const char* _func, const int _line)
{
    vos_debug(_func, _line, "");
    return malloc(size);
}

void __vos_free(void *ptr, const char* _func, const int _line)
{
    vos_debug(_func, _line, "");
    free(ptr);
}

void *__vos_calloc(size_t nmemb, size_t size, const char* _func, const int _line)
{
    vos_debug(_func, _line, "");
    return calloc(nmemb, size);
}

void *__vos_realloc(void *ptr, size_t size, const char* _func, const int _line)
{
    vos_debug(_func, _line, "");
    return realloc(ptr, size);
}

char *__vos_strdup(const char *s, const char* _func, const int _line)
{
    vos_debug(_func, _line, "");
    return strdup(s);
}

char *__vos_strndup(const char *s, size_t n, const char* _func, const int _line)
{
    vos_debug(_func, _line, "");
    return strndup(s, n);
}
```

### 5.3.3. vos-test.c
```c
#include <vos.h>
#include <stdio.h>
#include <malloc.h>
#include <pthread.h>

void demo_test1() {
    char *str1 = vos_malloc(64);
    
    str1 = vos_realloc(str1, 128);

    sprintf(str1, "Hello World.");
    printf("str1 = %s\n", str1);
    char *str2 = vos_strdup(str1);
    printf("str2 = %s\n", str2);
    vos_free(str2);
    
    vos_free(str1);
    str1 = vos_malloc(128);

    vos_free(str1);
}

void* test_task_fn(void* unused)
{
	printf("test_task_fn.\n");
    
    int i, j=0;
    while(1) {
        printf("while loop -> %d.\n", ++j);
        demo_test1();
        sleep(1);
    }

    pthread_exit(NULL);
	return NULL;
}

/* The main program. */
int main(int argc, char *argv[]) 
{
    int i;
    for(i=0;i<argc;i++){
        printf("argv[%d] = %s\n", i, argv[i]);
    }

	pthread_t thread_id;
    
	pthread_create(&thread_id, NULL, test_task_fn, NULL);

	pthread_join(thread_id, NULL);

    printf("Exit program.\n");

	return 0;
}
```

### 5.3.4. Makefile
```makefile
MM=${mm}

all:vos mymalloc test

vos:
	@echo MM=${MM}
	gcc -c vos.c -I. ${MM} 
	ar -crv libvos.a vos.o

mymalloc:
	gcc mymalloc.c -ldl -I. -fPIC -shared -o libmymalloc.so

test:
	@echo MM=${MM}
	gcc vos-test.c ${MM} libvos.a -I. -pthread

clean:
	rm -f *.o *.a *.so *.out
```

# 6. 正式开始

## 6.1. 不使用tcmalloc

首先不使用tcmalloc，直接使用glibc提供的内存分配器。

编译静态库：
```bash
$ make
MM=
gcc -c vos.c -I.  
ar -crv libvos.a vos.o
r - vos.o
gcc mymalloc.c -ldl -I. -fPIC -shared -o libmymalloc.so
MM=
gcc vos-test.c  libvos.a -I. -pthread
```
执行应用程序：
```bash
$ ./a.out a b c d
argv[0] = ./a.out
argv[1] = a
argv[2] = b
argv[3] = c
argv[4] = d
test_task_fn.
while loop -> 1.
str1 = Hello World.
str2 = Hello World.
while loop -> 2.
str1 = Hello World.
str2 = Hello World.
while loop -> 3.
str1 = Hello World.
str2 = Hello World.
while loop -> 4.
str1 = Hello World.
str2 = Hello World.
```
开启libmymalloc.so的调试开关(大量的打印信息)，重新编译，并使用LD_PRELOAD进行重定位，再次运行：
```c
$ LD_PRELOAD=./libmymalloc.so ./a.out 1 b c d
Memory allocator redirect start.
[ERROR] calloc null.
[ERROR] calloc null.
[ERROR] calloc null.
g_sys_calloc_func called.
load libc real malloc start.
g_sys_malloc_func called.
load libc.so.6 real malloc done.
Memory allocator redirect done.
argv[0] = ./a.out
g_sys_free_func called.
g_sys_free_func called.
argv[1] = 1
g_sys_free_func called.
g_sys_free_func called.
argv[2] = b
g_sys_free_func called.
g_sys_free_func called.
argv[3] = c
g_sys_free_func called.
g_sys_free_func called.
argv[4] = d
g_sys_free_func called.
g_sys_free_func called.
g_sys_calloc_func called.
test_task_fn.
while loop -> 1.
g_sys_free_func called.
g_sys_free_func called.
g_sys_malloc_func called.
g_sys_realloc_func called.
str1 = Hello World.
g_sys_free_func called.
g_sys_free_func called.
g_sys_strdup_func called.
g_sys_malloc_func called.
str2 = Hello World.
g_sys_free_func called.
...
```
在没有设置LD_PRELOAD的终端下，a.out的依赖如下：
```c
$ ldd a.out 
	linux-vdso.so.1 =>  (0x00007fff1adce000)
	libpthread.so.0 => /usr/lib64/libpthread.so.0 (0x00007f3516587000)
	libc.so.6 => /usr/lib64/libc.so.6 (0x00007f35161b9000)
	/lib64/ld-linux-x86-64.so.2 (0x00007f35167a3000)
```
设置LD_PRELOAD的终端下的输出则为：
```c
$ ldd a.out 
	linux-vdso.so.1 =>  (0x00007ffdc63e4000)
	./libmymalloc.so (0x00007efebeed7000)
	libpthread.so.0 => /usr/lib64/libpthread.so.0 (0x00007efebecbb000)
	libc.so.6 => /usr/lib64/libc.so.6 (0x00007efebe8ed000)
	libdl.so.2 => /usr/lib64/libdl.so.2 (0x00007efebe6e9000)
	/lib64/ld-linux-x86-64.so.2 (0x00007efebf0da000)
```

> 注意，这是完全相同的文件a.out，这里如果不知道原理，可以必应一下`可重定向库`相关知识。

## 6.2. 使用tcmalloc

再次编译：
```c
$ make mm=-ltcmalloc
MM=-ltcmalloc
gcc -c vos.c -I. -ltcmalloc 
ar -crv libvos.a vos.o
r - vos.o
gcc mymalloc.c -ldl -I. -fPIC -shared -o libmymalloc.so
MM=-ltcmalloc
gcc vos-test.c -ltcmalloc libvos.a -I. -pthread
```
这次是已经链接了tcmalloc动态库的，再次确认一下：
```c
$ ldd a.out 
	linux-vdso.so.1 =>  (0x00007ffe04c66000)
	libtcmalloc.so.4 => /usr/lib64/libtcmalloc.so.4 (0x00007fd434b45000)
	libpthread.so.0 => /usr/lib64/libpthread.so.0 (0x00007fd434929000)
	libc.so.6 => /usr/lib64/libc.so.6 (0x00007fd43455b000)
	libstdc++.so.6 => /usr/lib64/libstdc++.so.6 (0x00007fd434254000)
	libm.so.6 => /usr/lib64/libm.so.6 (0x00007fd433f52000)
	/lib64/ld-linux-x86-64.so.2 (0x00007fd434f3a000)
	libgcc_s.so.1 => /usr/lib64/libgcc_s.so.1 (0x00007fd433d3c000)
```
还是不放弃，再次确认一下：
```
$ gdb a.out 
(gdb) b main
(gdb) r
(gdb) n
(gdb) b malloc
test_task_fn.
Breakpoint 2, 0x00007ffff7a1be80 in tc_malloc () from /usr/lib64/libtcmalloc.so.4
(gdb) step
0x00007ffff7a1bd30 in tcmalloc::allocate_full_malloc_oom(unsigned long) () from /usr/lib64/libtcmalloc.so.4
(gdb) n

0x0000000000400b1b in __vos_malloc ()
(gdb) step
0x000000000040093a in demo_test1 ()
(gdb) n
str1 = Hello World.

Breakpoint 2, 0x00007ffff7a1be80 in tc_malloc () from /usr/lib64/libtcmalloc.so.4
```
可以看到，实际使用的内存分配器为tc_malloc。

> 注意，tcmalloc内存分配器不能使用valgrind进行内存监控和追踪，那么，我们前面讲的libmymalloc.so就要派上用场了。
```
$ gcc leak.c -pthread -ltcmalloc
$ ./valgrind--leak-check.sh ./a.out 
==94268== Mismatched free() / delete / delete []
==94268==    at 0x4C2B08D: free (vg_replace_malloc.c:538)
==94268==    by 0x4E4D93F: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
==94268==  Address 0x60351a0 is 0 bytes inside a block of size 4 alloc'd
==94268==    at 0x4C2AC58: operator new[](unsigned long) (vg_replace_malloc.c:431)
==94268==    by 0x4E4D937: ??? (in /usr/lib64/libtcmalloc.so.4.4.5)
==94268==    by 0x400F972: _dl_init (in /usr/lib64/ld-2.17.so)
==94268==    by 0x4001159: ??? (in /usr/lib64/ld-2.17.so)
```

接下来LD_PRELOAD要上场了。

```c
$ export LD_PRELOAD=./libmymalloc.so
$ ./valgrind.sh ./a.out 
==95017== Failed to connect to logging server '10.170.6.66:2049'.
==95017== Logging messages will be sent to stderr instead.
argv[0] = ./a.out
test_task_fn.
while loop -> 1.
str1 = Hello World.
str2 = Hello World.
while loop -> 2.
str1 = Hello World.
str2 = Hello World.
while loop -> 3.
str1 = Hello World.
str2 = Hello World.
while loop -> 4.
str1 = Hello World.
str2 = Hello World.
```
valgrind.sh脚本如下：
```bash
#!/bin/bash
# 荣涛 2021年3月18日

LOG_FILE="valgrind.log"
LOG_SOCK="10.170.6.66:2049"
SUPPRESSION_FILE="./tcmalloc-suppression.txt"

# --leak-check=full 完全检查内存泄漏
# --show-reachable=yes 显示内存泄漏地点
# --trace-children=yes 跟入子进程
# --quiet 只打印错误信息
# --gen-suppressions=yes 生成抑制文件
function gen-suppressions() {
	if [ $# -lt 1 ]; then
		echo "usage: $0 [program]"
		exit
	fi

	if [ ! -f $SUPPRESSION_FILE ]; then
		echo "File <$SUPPRESSION_FILE> not exist"
		exit
	fi
	valgrind --leak-check=full \
			 --show-reachable=yes \
			 --trace-children=yes \
			 --gen-suppressions=all \
			 --suppressions=$SUPPRESSION_FILE \
			 --quiet \
			 $*
}

# --leak-check=full 完全检查内存泄漏
# --show-reachable=yes 显示内存泄漏地点
# --trace-children=yes 跟入子进程
# --quiet 只打印错误信息
# --gen-suppressions=yes 生成抑制文件
# --suppressions=file.txt 指定抑制文件			 
# --time-stamp=yes 
# --quiet \

function suppressions() {
	if [ $# -lt 1 ]; then
		echo "usage: $0 [program]"
		exit
	fi
	if [ ! -f $SUPPRESSION_FILE ]; then
		echo "File <$SUPPRESSION_FILE> not exist"
		exit
	fi

	valgrind --leak-check=full \
			 --log-file=$LOG_FILE \
			 --log-socket=$LOG_SOCK \
			 --progress-interval=1 \
			 --child-silent-after-fork=yes \
			 --show-leak-kinds=all \
			 --show-reachable=yes \
			 --trace-children=no \
			 --allow-mismatched-debuginfo=no \
			 --suppressions=$SUPPRESSION_FILE \
			 --quiet \
			 $*
}

#gen-suppressions $*
suppressions $*
```
其中`tcmalloc-suppression.txt`的生成见上面章节。

全文完。

<br/>
<div align=right>RToax
</div>