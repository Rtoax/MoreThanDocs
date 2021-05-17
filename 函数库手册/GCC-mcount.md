<center><font size='6'>GCC(-pg) profile mcount</font></center>
<br/>
<center><font size='2'>荣涛</font></center>
<center><font size='2'>2021年5月12日</font></center>
<br/>

* gcc的profile特性，`gcc` 的 `-pg` 选项将在每个函数的入口处加入对mcount的代码调用;
* 如果ftrace编写了自己的mcount stub函数，则可借此实现trace功能;
* [Linux内核 eBPF基础：ftrace基础](https://rtoax.blog.csdn.net/article/details/116718182)


```c
//$ cat foo.c 
void foo(void) {
	int a = 1;
}
```
编译：
```bash
gcc foo.c -pg -S -o foo-pg.s
gcc foo.c -pg -c -o foo-pg.o
gcc foo.c -S -o foo.s
gcc foo.c -c -o foo.o
```
foo.s文件：
```asm
;$ cat foo.s 
foo:
	pushq	%rbp
	movq	%rsp, %rbp
	movl	$1, -4(%rbp)
	popq	%rbp
	ret
```
foo-pg.s文件：
```asm
;$ cat foo-pg.s 
foo:
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$16, %rsp
	call	mcount
	movl	$1, -4(%rbp)
	leave
	ret
```

查看两个.o文件

```
[rongtao@localhost demo]$ readelf -s foo.o 

Symbol table '.symtab' contains 9 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS foo.c
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    2 
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    3 
     5: 0000000000000000     0 SECTION LOCAL  DEFAULT    5 
     6: 0000000000000000     0 SECTION LOCAL  DEFAULT    6 
     7: 0000000000000000     0 SECTION LOCAL  DEFAULT    4 
     8: 0000000000000000    13 FUNC    GLOBAL DEFAULT    1 foo
[rongtao@localhost demo]$ readelf -s foo-pg.o 

Symbol table '.symtab' contains 10 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     1: 0000000000000000     0 FILE    LOCAL  DEFAULT  ABS foo.c
     2: 0000000000000000     0 SECTION LOCAL  DEFAULT    1 
     3: 0000000000000000     0 SECTION LOCAL  DEFAULT    3 
     4: 0000000000000000     0 SECTION LOCAL  DEFAULT    4 
     5: 0000000000000000     0 SECTION LOCAL  DEFAULT    6 
     6: 0000000000000000     0 SECTION LOCAL  DEFAULT    7 
     7: 0000000000000000     0 SECTION LOCAL  DEFAULT    5 
     8: 0000000000000000    22 FUNC    GLOBAL DEFAULT    1 foo
     9: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  UND mcount
```


