<center><font size='6'>GDB watch的使用</font></center>
<br/>
<center><font size='5'>荣涛</font></center>
<center><font size='5'>2021年3月30日</font></center>
<br/>


> 由于寄存器限制，GDB最多支持4个watchpoint。

# 1. 准备工作

先看一眼gdb watch帮助信息：
```c
Set a watchpoint for an expression.
Usage: watch [-l|-location] EXPRESSION
A watchpoint stops execution of your program whenever the value of
an expression changes.
If -l or -location is given, this evaluates EXPRESSION and watches
the memory to which it refers.
```
为了详细表述GDB watch的使用，首先给出一个简单的demo（为清晰，删除log部分代码，完整代码见文末）：
```c
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

#define NR_CELLS 8
#define NR_VTY  NR_CELLS

struct cell_struct {
	int id;
	char name[60];
};

struct vty_struct {
    int id;
    int task_id;
    int sock_id;
	char name[60];
};

static struct cell_struct *my_cells = NULL;
static struct vty_struct *my_vty = NULL;

void* test_task_fn(void* unused)
{
	struct cell_struct *new_cells = malloc(sizeof(struct cell_struct)*NR_CELLS);
	struct vty_struct *new_vtys = malloc(sizeof(struct vty_struct)*NR_VTY);

	my_cells = new_cells;    //保存内存引用
	my_vty = new_vtys;       //保存内存引用
    pthread_exit(NULL);
}

int main ()
{
	pthread_t thread_id;

	pthread_create(&thread_id, NULL, test_task_fn, NULL);
	pthread_join(thread_id, NULL);

    /* `my_cells` 将越界，写坏 `my_vty` */
    memset(&my_cells[NR_CELLS], 0x0, sizeof(struct cell_struct)*4);
    memset(&my_vty[0], 0xff, sizeof(struct vty_struct));

	return 0;
}
```

编译源程序：
```c
gcc clobber.c  -pthread -g -o clobber.out
```

运行程序：
```c
./clobber.out
```
在程序中，下面位置内存访问越界，在不使用任何调试工具的时候，将写坏结构`my_vty`：
```c
    /* `my_cells` 将越界，写坏 `my_vty` */
    memset(&my_cells[NR_CELLS], 0x0, sizeof(struct cell_struct)*4);
```
但是，这在程序运行过程中是不显示任何错误信息。接下来使用`gdb watch`对这个程序进行调试。

# 2. gdb watch

使用gdb启动需要调试的程序：

```c
$ gdb --quiet clobber.out 
(gdb) 
```

首先查看需要设置断点的位置，当你的服务器中存在源代码时，可以使用list进行源码查看，或者直接使用函数名进行断点设置：

```c
(gdb) break test_task_fn
```
上面的断点`test_task_fn`，因为我们的内存是在这个函数中分配的，在这个函数中设置断点，并单步执行：
```c
(gdb) run
...
Breakpoint 1, test_task_fn (unused=0x0) at clobber.c:28
28		struct cell_struct *new_cells = malloc(sizeof(struct cell_struct)*NR_CELLS);
(gdb) step
29		struct vty_struct *new_vtys = malloc(sizeof(struct vty_struct)*NR_VTY);
(gdb) 
31		my_cells = new_cells;    //保存内存引用
(gdb) 
32		my_vty = new_vtys;       //保存内存引用
(gdb) 
33	    pthread_exit(NULL);
```
查看申请的内存空间：
```c
(gdb) p my_vty[0].task_id
$1 = 0
(gdb) p &my_vty[0].task_id
$2 = (int *) 0x7ffff0000ad4
```
添加对该内存地址`0x7ffff0000ad4`的`watch`:
```c
(gdb) watch *0x7ffff0000ad4
Hardware watchpoint 2: *0x7ffff0000ad4
```
继续运行程序：
```c
(gdb) c
Old value = 65332
New value = 0
0x00007ffff788098a in __memset_sse2 () from /usr/lib64/libc.so.6
Missing separate debuginfos, use: debuginfo-install libgcc-4.8.5-39.el7.x86_64
```
上面，watchpoint检测到内存地址被修改，查看具体栈信息：
```c
(gdb) bt
#0  0x00007ffff788098a in __memset_sse2 () from /usr/lib64/libc.so.6
#1  0x0000000000400b54 in main () at clobber.c:89
```

上面的修改，对应源代码中的：
```c
    /* 将 越界，写坏 `my_vty` */
    memset(&my_cells[NR_CELLS], 0x0, sizeof(struct cell_struct)*4);
```
这是一次非法的访问。同样，另外的一次修改为正常访问，

```c
    memset(&my_vty[0], 0xff, sizeof(struct vty_struct));
```
不过也会被gdb watch捕获：

```c
(gdb) c
Continuing.
Hardware watchpoint 2: *0x7ffff0000ad4

Old value = 0
New value = -1
0x00007ffff7880979 in __memset_sse2 () from /usr/lib64/libc.so.6
(gdb) bt
#0  0x00007ffff7880979 in __memset_sse2 () from /usr/lib64/libc.so.6
#1  0x0000000000400b77 in main () at clobber.c:94
(gdb) c
Continuing.
```




# 3. 完成代码
```c
/**
 * 测试 gdb watch 调试
 * 
 * (gdb) watch * [addr]
 * 
 * 作者：荣涛
 * 日期：2021年3月30日
 */
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>

#define NR_CELLS 8
#define NR_VTY  NR_CELLS

struct cell_struct {
	int id;
	char name[60];
};

struct vty_struct {
    int id;
    int task_id;
    int sock_id;
	char name[60];
};

static struct cell_struct *my_cells = NULL;
static struct vty_struct *my_vty = NULL;

static void set_cell(struct cell_struct *cell, int id, char *name) {
	assert(cell && "set_cell: Cell is NULL.");
	cell->id = id;
	snprintf(cell->name, sizeof(cell->name), "%s:%d", name, id);
}
static void set_vty(struct vty_struct *vty, int id, char *name) {
	assert(vty && "set_vty: VTY is NULL.");
	vty->id = id;
    vty->task_id = 0xff33 + id;  //创建进程
    vty->sock_id = 0xee44 + id;  //创建 socket
	snprintf(vty->name, sizeof(vty->name), "%s:%d", name, id);
}
void show_cells() {
	assert(my_cells && "show_cells: Cell is NULL.");
	int i;
	for(i=0; i< 8; i++) { 
		printf("%4d - %s\n", my_cells[i].id, my_cells[i].name);
	}
}
void show_vtys() {
    printf("--------------------------------\n");
	assert(my_vty && "show_vtys: VTY is NULL.");
	int i;
	for(i=0; i< 8; i++) { 
		printf("%4d - %s, %x, %x\n", my_vty[i].id, my_vty[i].name, my_vty[i].task_id, my_vty[i].sock_id);
	}
}

void* test_task_fn(void* unused)
{
	printf("new thread.\n");	

	struct cell_struct *new_cells = malloc(sizeof(struct cell_struct)*NR_CELLS);
	struct vty_struct *new_vtys = malloc(sizeof(struct vty_struct)*NR_VTY);

	int i;
	for(i=0; i<NR_CELLS; i++) {
		set_cell(&new_cells[i], i+1, "cell");
	}
	my_cells = new_cells;
	for(i=0; i<NR_VTY; i++) {
		set_vty(&new_vtys[i], i+1, "vty");
	}
	my_vty = new_vtys;

    pthread_exit(NULL);
}

/* The main program. */
int main ()
{
	pthread_t thread_id;
    
	pthread_create(&thread_id, NULL, test_task_fn, NULL);

	pthread_join(thread_id, NULL);

	printf("task exit.\n");

    show_vtys();

    /* 将 越界，写坏 `my_vty` */
    memset(&my_cells[NR_CELLS], 0x0, sizeof(struct cell_struct)*4);
    
//    show_cells();
    show_vtys();

    memset(&my_vty[0], 0xff, sizeof(struct vty_struct));
    
    show_vtys();

    printf("Exit program.\n");

	return 0;
}
```