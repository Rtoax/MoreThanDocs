<center><font size='6'>Linux内核深入理解中断和异常（4）：不可屏蔽中断NMI、浮点异常和SIMD </font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>


本文介绍一下几种trap：

```c
//* External hardware asserts (外部设备断言)the non-maskable interrupt [pin] on the CPU.
//* The processor receives a message on the system bus or the APIC serial bus with a delivery mode `NMI`.
#define X86_TRAP_NMI	 2	/*  2, 不可屏蔽中断 *//* Non-maskable Interrupt 不可屏蔽中断， 严重问题 */
                            /**
                             *  hardware interrupt
                             *      exc_nmi  : arch\x86\kernel\nmi.c
                             *      ()
                             */
                             
#define X86_TRAP_BR		 5	/*  5, 超出范围 *//* Bound Range Exceeded */
                            /**
                             *      exc_bounds  : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_MF		16	/* 16, x87 浮点异常 *//* x87 Floating-Point Exception */
                            /**
                             *      exc_coprocessor_error  : arch\x86\kernel\traps.c
                             *      ()
                             */                             
#define X86_TRAP_XF		19	/* 19, SIMD （单指令多数据结构浮点）异常 *//* SIMD Floating-Point Exception */
                            /**
                             *      exc_simd_coprocessor_error  : arch\x86\kernel\traps.c
                             *      ()
                             */
                            /* `SSE` or `SSE2` or `SSE3` SIMD floating-point exception */
                            //There are six classes of numeric exception conditions that 
                            //can occur while executing an SIMD floating-point instruction:
                            //
                            //* Invalid operation
                            //* Divide-by-zero
                            //* Denormal operand
                            //* Numeric overflow
                            //* Numeric underflow
                            //* Inexact result (Precision)                             
```

首先看下`X86_TRAP_NMI`出现的位置：
```c
arch/x86/mm/extable.c:194:	if (trapnr == X86_TRAP_NMI)
arch/x86/kernel/idt.c:77:	INTG(X86_TRAP_NMI,		asm_exc_nmi),   //arch/x86/entry/entry_64.S
arch/x86/kernel/idt.c:238:	ISTG(X86_TRAP_NMI,	asm_exc_nmi,			IST_INDEX_NMI), ////arch/x8
6/entry/entry_64.Sarch/x86/platform/uv/uv_nmi.c:905:		ret = kgdb_nmicallin(cpu, X86_TRAP_NMI, regs, reason,
arch/x86/include/asm/trapnr.h:52:#define X86_TRAP_NMI	 2	/*  2, 不可屏蔽中断 *//* Non-maskable Interrupt 不
可屏蔽中断， 严重问题 */arch/x86/include/asm/idtentry.h:594:DECLARE_IDTENTRY_NMI(X86_TRAP_NMI,	exc_nmi);
arch/x86/include/asm/idtentry.h:596:DECLARE_IDTENTRY_RAW(X86_TRAP_NMI,	xenpv_exc_nmi);
```

# 1. Non-maskable interrupt handler

It is sixth part of the [Interrupts and Interrupt Handling in the Linux kernel](https://0xax.gitbook.io/linux-insides/summary/interrupts) chapter and in the previous [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-5) we saw implementation of some exception handlers for the [General Protection Fault](https://en.wikipedia.org/wiki/General_protection_fault) exception, divide exception, invalid [opcode](https://en.wikipedia.org/wiki/Opcode) exceptions and etc. As I wrote in the previous part we will see implementations of the rest exceptions in this part. We will see implementation of the following handlers:

* [Non-Maskable](https://en.wikipedia.org/wiki/Non-maskable_interrupt) interrupt;
* [BOUND](http://pdos.csail.mit.edu/6.828/2005/readings/i386/BOUND.htm) Range Exceeded Exception;
* [Coprocessor](https://en.wikipedia.org/wiki/Coprocessor) exception;
* [SIMD](https://en.wikipedia.org/wiki/SIMD) coprocessor exception.

in this part. So, let's start.

# 2. Non-Maskable interrupt handling

A [Non-Maskable](https://en.wikipedia.org/wiki/Non-maskable_interrupt) interrupt is a hardware interrupt that cannot be ignored by standard masking techniques. In a general way, a non-maskable interrupt can be generated in either of two ways:

* **External hardware asserts (外部设备断言)**the non-maskable interrupt [pin](https://en.wikipedia.org/wiki/CPU_socket) on the CPU.
* The processor receives a message on the **system bus** or the **APIC serial bus** with a delivery mode `NMI`.

When the processor receives a `NMI` from one of these sources, the processor handles it immediately by calling the `NMI` handler pointed to by interrupt vector which has number `2` (see table in the first [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-1)). 

```c
#define X86_TRAP_NMI	 2
```

We already filled the [Interrupt Descriptor Table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table) with the [vector number](https://en.wikipedia.org/wiki/Interrupt_vector_table), address of the `nmi` interrupt handler and `NMI_STACK` [Interrupt Stack Table entry](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/x86/kernel-stacks):

```C
set_intr_gate_ist(X86_TRAP_NMI, &nmi, NMI_STACK);
```
5.10.13中：
```c
static const __initconst struct idt_data def_idts[] = {/* 默认的 中断描述符表 */
	...
	INTG(X86_TRAP_NMI,		asm_exc_nmi),   //arch/x86/entry/entry_64.S
	...
};
```

in the `trap_init` function which defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c) source code file. In the previous [parts](https://0xax.gitbook.io/linux-insides/summary/interrupts) we saw that entry points of the all interrupt handlers are defined with the:

```assembly
.macro idtentry sym do_sym has_error_code:req paranoid=0 shift_ist=-1
ENTRY(\sym)
...
...
...
END(\sym)
.endm
```

macro from the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly source code file. But the handler of the `Non-Maskable` interrupts is not defined with this macro. It has own entry point:

```assembly
ENTRY(nmi)
...
...
...
END(nmi)
```

in the same [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly file. Lets dive into it and will try to understand how `Non-Maskable` interrupt handler works. The `nmi` handlers starts from the call of the:

```assembly
PARAVIRT_ADJUST_EXCEPTION_FRAME
```

macro but we will not dive into details about it in this part, because this macro related to the [Paravirtualization](https://en.wikipedia.org/wiki/Paravirtualization) stuff which we will see in another chapter. After this save the content of the `rdx` register on the stack:

```assembly
pushq	%rdx
```

And allocated check that `cs` was not the kernel segment when an non-maskable interrupt occurs:

```assembly
cmpl	$__KERNEL_CS, 16(%rsp)
jne	first_nmi
```

The `__KERNEL_CS` macro defined in the [arch/x86/include/asm/segment.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/asm/segment.h) and represented second descriptor in the [Global Descriptor Table](https://en.wikipedia.org/wiki/Global_Descriptor_Table):

```C
#define GDT_ENTRY_KERNEL_CS	2
#define __KERNEL_CS	(GDT_ENTRY_KERNEL_CS*8)
```

more about `GDT` you can read in the second [part](https://0xax.gitbook.io/linux-insides/summary/booting/linux-bootstrap-2) of the Linux kernel booting process chapter. If `cs` is not kernel segment, it means that it is not nested `NMI` and we jump on the `first_nmi` label. Let's consider this case. First of all we put address of the current stack pointer to the `rdx` and pushes `1` to the stack in the `first_nmi` label:

```assembly
first_nmi:
	movq	(%rsp), %rdx
	pushq	$1
```

Why do we push `1` on the stack? 

5.10.13中改成了0
```asm
first_nmi:
	/* Restore rdx. */
	movq	(%rsp), %rdx

	/* Make room for "NMI executing". */
	pushq	$0
```
总的来说是为了解决在处理NMI期间又来了一个NMI。

As the comment says: `We allow breakpoints in NMIs`. On the [x86_64](https://en.wikipedia.org/wiki/X86-64), like other architectures, the CPU will not execute another `NMI` until the first `NMI` is completed. A `NMI` interrupt finished with the [iret](http://faydoc.tripod.com/cpu/iret.htm) instruction like other interrupts and exceptions do it. If the `NMI` handler triggers either a [page fault](https://en.wikipedia.org/wiki/Page_fault) or [breakpoint](https://en.wikipedia.org/wiki/Breakpoint) or another exception which are use `iret` instruction too. If this happens while in `NMI` context, the CPU will leave `NMI` context and a new `NMI` may come in. 

The `iret` used to return from those exceptions will re-enable `NMIs` and we will get nested non-maskable interrupts. The problem the `NMI` handler will not return to the state that it was, when the exception triggered, but instead it will return to a state that will allow new `NMIs` to preempt the running `NMI` handler. 

**If another `NMI` comes in before the first NMI handler is complete**, the new NMI will write all over the preempted `NMIs` stack. **We can have nested `NMIs` where the next `NMI` is using the top of the stack of the previous `NMI`. It means that we cannot execute it because a nested non-maskable interrupt will corrupt stack of a previous non-maskable interrupt. **

> 当NMIs嵌套，下一个NMI使用上一个NMI的栈顶，那么我们就不能执行它，因为嵌套的NMI将摧毁上一个NMI的栈。

**That's why we have allocated space on the stack for temporary variable**. We will check this variable that it was set when a previous `NMI` is executing and clear if it is not nested `NMI`. We push `1` here to the previously allocated space on the stack to denote that a `non-maskable` interrupt executed currently. Remember that when and `NMI` or another exception occurs we have the following [stack frame](https://en.wikipedia.org/wiki/Call_stack):

```
+------------------------+
|         SS             |
|         RSP            |
|        RFLAGS          |
|         CS             |
|         RIP            |
+------------------------+
```

and also an error code if an exception has it. So, after all of these manipulations our stack frame will look like this:

```
+------------------------+
|         SS             |
|         RSP            |
|        RFLAGS          |
|         CS             |
|         RIP            |
|         RDX            |
|          1             |
+------------------------+
```

In the next step we allocate yet another `40` bytes on the stack:

```assembly
subq	$(5*8), %rsp
```

and pushes the copy of the original stack frame after the allocated space:

```C
.rept 5
pushq	11*8(%rsp)
.endr
```

with the [.rept](http://tigcc.ticalc.org/doc/gnuasm.html#SEC116) assembly directive. We need in the copy of the original stack frame. Generally we need in two copies of the interrupt stack. 

* First is `copied` interrupts stack: `saved` stack frame and `copied` stack frame. Now we pushes original stack frame to the `saved` stack frame which locates after the just allocated `40` bytes (`copied` stack frame). This stack frame is used to fixup the `copied` stack frame that a nested NMI may change. 
* The second - `copied` stack frame modified by any nested `NMIs` to let the first `NMI` know that we triggered a second `NMI` and we should repeat the first `NMI` handler. Ok, we have made first copy of the original stack frame, now time to make second copy:

```assembly
addq	$(10*8), %rsp

.rept 5
pushq	-6*8(%rsp)
.endr
subq	$(5*8), %rsp
```

After all of these manipulations our stack frame will be like this:

```
+-------------------------+
| original SS             |
| original Return RSP     |
| original RFLAGS         |
| original CS             |
| original RIP            |
+-------------------------+
| temp storage for rdx    |
+-------------------------+
| NMI executing variable  |
+-------------------------+
| copied SS               |
| copied Return RSP       |
| copied RFLAGS           |
| copied CS               |
| copied RIP              |
+-------------------------+
| Saved SS                |
| Saved Return RSP        |
| Saved RFLAGS            |
| Saved CS                |
| Saved RIP               |
+-------------------------+
```

After this we push dummy error code on the stack as we did it already in the previous exception handlers and allocate space for the general purpose registers on the stack:

```assembly
pushq	$-1
ALLOC_PT_GPREGS_ON_STACK
```

We already saw implementation of the `ALLOC_PT_GREGS_ON_STACK` macro in the third part of the interrupts [chapter](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-3). This macro defined in the [arch/x86/entry/calling.h](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/calling.h) and yet another allocates `120` bytes on stack for the general purpose registers, from the `rdi` to the `r15`:

```assembly
.macro ALLOC_PT_GPREGS_ON_STACK addskip=0
addq	$-(15*8+\addskip), %rsp
.endm
```

After space allocation for the general registers we can see call of the `paranoid_entry`:

```assembly
call	paranoid_entry
```

We can remember from the previous parts this label. It pushes general purpose registers on the stack, reads `MSR_GS_BASE` [Model Specific register](https://en.wikipedia.org/wiki/Model-specific_register) and checks its value. If the value of the `MSR_GS_BASE` is negative, we came from the **kernel mode** and just return from the `paranoid_entry`, in other way it means that we came from the **usermode** and need to execute `swapgs` instruction which will change user `gs` with the kernel `gs`:

```assembly
ENTRY(paranoid_entry)
	cld
	SAVE_C_REGS 8
	SAVE_EXTRA_REGS 8
	movl	$1, %ebx
	movl	$MSR_GS_BASE, %ecx
	rdmsr
	testl	%edx, %edx
	js	1f
	SWAPGS
	xorl	%ebx, %ebx
1:	ret
END(paranoid_entry)
```

Note that after the `swapgs` instruction we zeroed the `ebx` register. 

Next time we will check content of this register and if we executed `swapgs` than `ebx` must contain `0` and `1` in other way. In the next step we store value of the `cr2` [control register](https://en.wikipedia.org/wiki/Control_register) to the `r12` register, because the `NMI` handler can cause `page fault` and corrupt the value of this control register:

```C
movq	%cr2, %r12
```

Now time to call actual `NMI` handler. We push the address of the `pt_regs` to the `rdi`, error code to the `rsi` and call the `do_nmi` handler:

```assembly
movq	%rsp, %rdi
movq	$-1, %rsi
call	do_nmi
```
在5.10.13中：
```asm
call	exc_nmi
```

We will back to the `do_nmi` little later in this part, but now let's look what occurs after the `do_nmi` will finish its execution.

After the `do_nmi` handler will be finished we check the `cr2` register, because we can got page fault during `do_nmi` performed and if we got it we restore original `cr2`, in other way we jump on the label `1`. After this we test content of the `ebx` register (remember it must contain `0` if we have used `swapgs` instruction and `1` if we didn't use it) and execute `SWAPGS_UNSAFE_STACK` if it contains `1` or jump to the `nmi_restore` label. 

The `SWAPGS_UNSAFE_STACK` macro just expands to the `swapgs` instruction. 

In the `nmi_restore` label we restore general purpose registers, clear allocated space on the stack for this registers, clear our temporary variable and exit from the interrupt handler with the `INTERRUPT_RETURN` macro:

```assembly
	movq	%cr2, %rcx
	cmpq	%rcx, %r12
	je	1f
	movq	%r12, %cr2
1:
	testl	%ebx, %ebx
	jnz	nmi_restore
nmi_swapgs:
	SWAPGS_UNSAFE_STACK
nmi_restore:
	RESTORE_EXTRA_REGS
	RESTORE_C_REGS
	/* Pop the extra iret frame at once */
	REMOVE_PT_GPREGS_FROM_STACK 6*8
	/* Clear the NMI executing stack variable */
	movq	$0, 5*8(%rsp)
	INTERRUPT_RETURN
```
5.10.13中是这样的：
```asm
nmi_restore:    //EBX contains `0`
	POP_REGS

	/*
	 * Skip orig_ax and the "outermost" frame to point RSP at the "iret"
	 * at the "iret" frame.
	 */
	addq	$6*8, %rsp

	/*
	 * Clear "NMI executing".  Set DF first so that we can easily
	 * distinguish the remaining code between here and IRET from
	 * the SYSCALL entry and exit paths.
	 *
	 * We arguably should just inspect RIP instead, but I (Andy) wrote
	 * this code when I had the misapprehension that Xen PV supported
	 * NMIs, and Xen PV would break that approach.
	 */
	std
	movq	$0, 5*8(%rsp)		/* clear "NMI executing" */

	/*
	 * iretq reads the "iret" frame and exits the NMI stack in a
	 * single instruction.  We are returning to kernel mode, so this
	 * cannot result in a fault.  Similarly, we don't need to worry
	 * about espfix64 on the way back to kernel mode.
	 */
	iretq
```
where `INTERRUPT_RETURN` is defined in the [arch/x86/include/asm/irqflags.h](https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/irqflags.h) and just expands to the `iret` instruction. That's all.

**当一个NMI未终止，另一个NMI发生会发生什么呢？**

Now let's consider case when another `NMI` interrupt occurred when previous `NMI` interrupt didn't finish its execution. You can remember from the beginning of this part that we've made a check that we came from userspace and jump on the `first_nmi` in this case:

```assembly
cmpl	$__KERNEL_CS, 16(%rsp)
jne	first_nmi
```

Note that in this case it is first `NMI` every time, because if the first `NMI` catched page fault, breakpoint or another exception it will be executed in the kernel mode. If we didn't come from userspace, first of all we test our temporary variable:

```assembly
cmpl	$1, -8(%rsp)
je	nested_nmi
```
```asm
	/* This is a nested NMI. */

nested_nmi: //嵌套的 NMI 开始
	/*
	 * Modify the "iret" frame to point to repeat_nmi, forcing another
	 * iteration of NMI handling.
	 */
	subq	$8, %rsp
	leaq	-10*8(%rsp), %rdx
	pushq	$__KERNEL_DS
	pushq	%rdx
	pushfq
	pushq	$__KERNEL_CS
	pushq	$repeat_nmi

	/* Put stack back */
	addq	$(6*8), %rsp

nested_nmi_out: //嵌套的 NMI 结束
```
and if it is set to `1` we jump to the `nested_nmi` label. If it is not `1`, we test the `IST` stack. In the case of nested `NMIs` we check that we are above the `repeat_nmi`. In this case we ignore it, in other way we check that we above than `end_repeat_nmi` and jump on the `nested_nmi_out` label.

Now let's look on the `do_nmi` exception handler. This function defined in the [arch/x86/kernel/nmi.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/nmi.c) source code file and takes two parameters:

* address of the `pt_regs`;
* error code.

as all exception handlers. 

> 在5.10.13中显然已经不是这样，有些中断处理函数有errorcode，有些则没有。

The `do_nmi` starts from the call of the `nmi_nesting_preprocess` function and ends with the call of the `nmi_nesting_postprocess`. The `nmi_nesting_preprocess` function checks that we likely do not work with the debug stack and if we on the debug stack set the `update_debug_stack` [per-cpu](https://0xax.gitbook.io/linux-insides/summary/concepts/linux-cpu-1) variable to `1` and call the `debug_stack_set_zero` function from the [arch/x86/kernel/cpu/common.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/cpu/common.c). This function increases the `debug_stack_use_ctr` per-cpu variable and loads new `Interrupt Descriptor Table`:
 
```C
static inline void nmi_nesting_preprocess(struct pt_regs *regs)
{
    if (unlikely(is_debug_stack(regs->sp))) {
        debug_stack_set_zero();
        this_cpu_write(update_debug_stack, 1);
    }
}
```

The `nmi_nesting_postprocess` function checks the `update_debug_stack` per-cpu variable which we set in the `nmi_nesting_preprocess` and resets debug stack or in another words it loads origin `Interrupt Descriptor Table`. After the call of the `nmi_nesting_preprocess` function, we can see the call of the `nmi_enter` in the `do_nmi`. The `nmi_enter` increases `lockdep_recursion` field of the interrupted process, update preempt counter and informs the [RCU](https://en.wikipedia.org/wiki/Read-copy-update) subsystem about `NMI`. There is also `nmi_exit` function that does the same stuff as `nmi_enter`, but vice-versa. After the `nmi_enter` we increase `__nmi_count` in the `irq_stat` structure and call the `default_do_nmi` function. First of all in the `default_do_nmi` we check the address of the previous nmi and update address of the last nmi to the actual:

```C
if (regs->ip == __this_cpu_read(last_nmi_rip))
    b2b = true;
else
    __this_cpu_write(swallow_nmi, false);

__this_cpu_write(last_nmi_rip, regs->ip);
```

After this first of all we need to handle CPU-specific `NMIs`:

```C
handled = nmi_handle(NMI_LOCAL, regs, b2b);
__this_cpu_add(nmi_stats.normal, handled);
```

And then non-specific `NMIs` depends on its reason:

```C
reason = x86_platform.get_nmi_reason();
if (reason & NMI_REASON_MASK) {
	if (reason & NMI_REASON_SERR)
		pci_serr_error(reason, regs);
	else if (reason & NMI_REASON_IOCHK)
		io_check_error(reason, regs);

	__this_cpu_add(nmi_stats.external, 1);
	return;
}
```

5.10.13中不叫do_nmi，而是如下的函数：
```c
void exc_nmi(struct pt_regs *regs){/* 我加的 */}
DEFINE_IDTENTRY_RAW(exc_nmi)
{
	bool irq_state;

	/*
	 * Re-enable NMIs right here when running as an SEV-ES guest. This might
	 * cause nested NMIs, but those can be handled safely.
	 */
	sev_es_nmi_complete();

	if (IS_ENABLED(CONFIG_SMP) && arch_cpu_is_offline(smp_processor_id()))
		return;

	if (this_cpu_read(nmi_state) != NMI_NOT_RUNNING) {
		this_cpu_write(nmi_state, NMI_LATCHED);
		return;
	}
	this_cpu_write(nmi_state, NMI_EXECUTING);
	this_cpu_write(nmi_cr2, read_cr2());
nmi_restart:

	/*
	 * Needs to happen before DR7 is accessed, because the hypervisor can
	 * intercept DR7 reads/writes, turning those into #VC exceptions.
	 */
	sev_es_ist_enter(regs);

	this_cpu_write(nmi_dr7, local_db_save());

	irq_state = idtentry_enter_nmi(regs);

	inc_irq_stat(__nmi_count);

	if (!ignore_nmis)
		default_do_nmi(regs);

	idtentry_exit_nmi(regs, irq_state);

	local_db_restore(this_cpu_read(nmi_dr7));

	sev_es_ist_exit();

	if (unlikely(this_cpu_read(nmi_cr2) != read_cr2()))
		write_cr2(this_cpu_read(nmi_cr2));
	if (this_cpu_dec_return(nmi_state))
		goto nmi_restart;

	if (user_mode(regs))
		mds_user_clear_cpu_buffers();
}
```
这里将其极端简化为：
```c
void exc_nmi(struct pt_regs *regs){
	if (!ignore_nmis)
		default_do_nmi(regs);
}
```
而`default_do_nmi`极端简化为：
```c
static noinstr void default_do_nmi(struct pt_regs *regs)
{
    nmi_handle(NMI_LOCAL, regs);
}
```
引入数据结构：
```c
struct nmiaction {
	struct list_head	list;
	nmi_handler_t		handler;
	u64			max_duration;
	unsigned long		flags;
	const char		*name;
};
```
结构`nmiaction`被如下结构连成双向链表：
```c
struct nmi_desc {   /* NMI：不可屏蔽中断处理函数 链表头 */
	raw_spinlock_t lock;
	struct list_head head; /* struct nmiaction->list */
};
```
而在nmi_handle中所作的就是遍历链表，执行回调函数：
```c
static int nmi_handle(unsigned int type, struct pt_regs *regs)
{
	struct nmi_desc *desc = nmi_to_desc(type);
	struct nmiaction *a;
	int handled=0;

	rcu_read_lock();

	/*
	 * NMIs are edge-triggered, which means if you have enough
	 * of them concurrently, you can lose some because only one
	 * can be latched at any given time.  Walk the whole list
	 * to handle those situations.
	 */
	list_for_each_entry_rcu(a, &desc->head, list) {
		int thishandled;
		u64 delta;

		delta = sched_clock();
		thishandled = a->handler(type, regs);
		handled += thishandled;
		delta = sched_clock() - delta;
		trace_nmi_handler(a->handler, (int)delta, thishandled);

		nmi_check_duration(a, delta);
	}

	rcu_read_unlock();

	/* return total number of NMI events handled */
	return handled;
}
NOKPROBE_SYMBOL(nmi_handle);
```

That's all.

# 3. Range Exceeded Exception

> Exceeded: 超过(数量); 超越(法律、命令等)的限制

The next exception is the `BOUND` range exceeded exception. The `BOUND` instruction determines if the first operand (array index) is within the bounds of an array specified the second operand (bounds operand). If the index is not within bounds, a `BOUND` range exceeded exception or `#BR` is occurred. The handler of the `#BR` exception is the `do_bounds` function that defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c). 

5.10.13中：
```c
void exc_bounds(struct pt_regs *regs){/* 我加的 */}
DEFINE_IDTENTRY(exc_bounds)
{
	if (notify_die(DIE_TRAP, "bounds", regs, 0,
			X86_TRAP_BR, SIGSEGV) == NOTIFY_STOP)
		return;
	cond_local_irq_enable(regs);

	if (!user_mode(regs))
		die("bounds", regs, 0);

	do_trap(X86_TRAP_BR, SIGSEGV, "bounds", regs, 0, 0, NULL);

	cond_local_irq_disable(regs);
}
```

The `do_bounds` handler starts with the call of the `exception_enter` function and ends with the call of the `exception_exit`:

```C
prev_state = exception_enter();

if (notify_die(DIE_TRAP, "bounds", regs, error_code,
	           X86_TRAP_BR, SIGSEGV) == NOTIFY_STOP)
    goto exit;
...
...
...
exception_exit(prev_state);
return;
```

After we have got the state of the previous context, we add the exception to the `notify_die` chain and if it will return `NOTIFY_STOP` we return from the exception. More about notify chains and the `context tracking` functions you can read in the [previous part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-5). In the next step we enable interrupts if they were disabled with the `contidional_sti` function that checks `IF` flag and call the `local_irq_enable` depends on its value:

```C
conditional_sti(regs);

if (!user_mode(regs))
	die("bounds", regs, error_code);
```

and check that if we didn't came from user mode we send `SIGSEGV` signal with the `die` function. After this we check is [MPX](https://en.wikipedia.org/wiki/Intel_MPX) enabled or not, and if this feature is disabled we jump on the `exit_trap` label:

```C
if (!cpu_feature_enabled(X86_FEATURE_MPX)) {
	goto exit_trap;
}

where we execute `do_trap` function (more about it you can find in the previous part):

```C
exit_trap:
	do_trap(X86_TRAP_BR, SIGSEGV, "bounds", regs, error_code, NULL);
	exception_exit(prev_state);
```

If `MPX` feature is enabled we check the `BNDSTATUS` with the `get_xsave_field_ptr` function and if it is zero, it means that the `MPX` was not responsible for this exception:

```C
bndcsr = get_xsave_field_ptr(XSTATE_BNDCSR);
if (!bndcsr)
		goto exit_trap;
```

After all of this, there is still only one way when `MPX` is responsible for this exception. We will not dive into the details about Intel Memory Protection Extensions in this part, but will see it in another chapter.

而在5.10.13中，会调用do_trap，do_trap继续晚上上述的一系列操作。


# 4. Coprocessor exception and SIMD exception


The next two exceptions are [x87 FPU](https://en.wikipedia.org/wiki/X87) Floating-Point Error exception or `#MF` and [SIMD](https://en.wikipedia.org/wiki/SIMD) Floating-Point Exception or `#XF`. The first exception occurs when the `x87 FPU` has detected floating point error. For example divide by zero, numeric overflow and etc. The second exception occurs when the processor has detected [SSE/SSE2/SSE3](https://en.wikipedia.org/wiki/SSE3) `SIMD` floating-point exception. It can be the same as for the `x87 FPU`. The handlers for these exceptions are `do_coprocessor_error` and `do_simd_coprocessor_error` are defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c) and very similar on each other. They both make a call of the `math_error` function from the same source code file but pass different vector number. The `do_coprocessor_error` passes `X86_TRAP_MF` vector number to the `math_error`:

```C
dotraplinkage void do_coprocessor_error(struct pt_regs *regs, long error_code)
{
	enum ctx_state prev_state;

	prev_state = exception_enter();
	math_error(regs, error_code, X86_TRAP_MF);
	exception_exit(prev_state);
}
```

and `do_simd_coprocessor_error` passes `X86_TRAP_XF` to the `math_error` function:

```C
dotraplinkage void
do_simd_coprocessor_error(struct pt_regs *regs, long error_code)
{
	enum ctx_state prev_state;

	prev_state = exception_enter();
	math_error(regs, error_code, X86_TRAP_XF);
	exception_exit(prev_state);
}
```
在5.10.13中：
```c
void exc_coprocessor_error(struct pt_regs *regs){/* 我加的 */}
DEFINE_IDTENTRY(exc_coprocessor_error)
{
	math_error(regs, X86_TRAP_MF);
}
void exc_simd_coprocessor_error(struct pt_regs *regs){/* 我加的 */}
DEFINE_IDTENTRY(exc_simd_coprocessor_error)
{
	if (IS_ENABLED(CONFIG_X86_INVD_BUG)) {
		/* AMD 486 bug: INVD in CPL 0 raises #XF instead of #GP */
		if (!static_cpu_has(X86_FEATURE_XMM)) {
			__exc_general_protection(regs, 0);
			return;
		}
	}
	math_error(regs, X86_TRAP_XF);
}
```
First of all the `math_error` function defines current interrupted task, address of its fpu, string which describes an exception, add it to the `notify_die` chain and return from the exception handler if it will return `NOTIFY_STOP`:

```C
	struct task_struct *task = current;
	struct fpu *fpu = &task->thread.fpu;
	siginfo_t info;
	char *str = (trapnr == X86_TRAP_MF) ? "fpu exception" :
						"simd exception";

	if (notify_die(DIE_TRAP, str, regs, error_code, trapnr, SIGFPE) == NOTIFY_STOP)
		return;
```

After this we check that we are from the kernel mode and if yes we will try to fix an exception with the `fixup_exception` function. If we cannot we fill the task with the exception's error code and vector number and die:

```C
if (!user_mode(regs)) {
	if (!fixup_exception(regs)) {
		task->thread.error_code = error_code;
		task->thread.trap_nr = trapnr;
		die(str, regs, error_code);
	}
	return;
}
```

If we came from the user mode, we save the `fpu` state, fill the task structure with the vector number of an exception and `siginfo_t` with the number of signal, `errno`, the address where exception occurred and signal code:

```C
fpu__save(fpu);

task->thread.trap_nr	= trapnr;
task->thread.error_code = error_code;
info.si_signo		= SIGFPE;
info.si_errno		= 0;
info.si_addr		= (void __user *)uprobe_get_trap_addr(regs);
info.si_code = fpu__exception_code(fpu, trapnr);
```

After this we check the signal code and if it is non-zero we return:

```C
if (!info.si_code)
	return;
```
		
Or send the `SIGFPE` signal in the end:

```C
force_sig_info(SIGFPE, &info, task);
```

That's all.

# 5. Conclusion

It is the end of the sixth part of the [Interrupts and Interrupt Handling](https://0xax.gitbook.io/linux-insides/summary/interrupts) chapter and we saw implementation of some exception handlers in this part, like `non-maskable` interrupt, [SIMD](https://en.wikipedia.org/wiki/SIMD) and [x87 FPU](https://en.wikipedia.org/wiki/X87) floating point exception. Finally we have finsihed with the `trap_init` function in this part and will go ahead in the next part. The next our point is the external interrupts and the `early_irq_init` function from the [init/main.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/init/main.c).

If you have any questions or suggestions write me a comment or ping me at [twitter](https://twitter.com/0xAX).

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

# 6. Links

* [General Protection Fault](https://en.wikipedia.org/wiki/General_protection_fault)
* [opcode](https://en.wikipedia.org/wiki/Opcode)
* [Non-Maskable](https://en.wikipedia.org/wiki/Non-maskable_interrupt) 
* [BOUND instruction](http://pdos.csail.mit.edu/6.828/2005/readings/i386/BOUND.htm)
* [CPU socket](https://en.wikipedia.org/wiki/CPU_socket)
* [Interrupt Descriptor Table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table)
* [Interrupt Stack Table](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/Documentation/x86/kernel-stacks)
* [Paravirtualization](https://en.wikipedia.org/wiki/Paravirtualization)
* [.rept](http://tigcc.ticalc.org/doc/gnuasm.html#SEC116)
* [SIMD](https://en.wikipedia.org/wiki/SIMD)
* [Coprocessor](https://en.wikipedia.org/wiki/Coprocessor)
* [x86_64](https://en.wikipedia.org/wiki/X86-64)
* [iret](http://faydoc.tripod.com/cpu/iret.htm)
* [page fault](https://en.wikipedia.org/wiki/Page_fault)
* [breakpoint](https://en.wikipedia.org/wiki/Breakpoint)
* [Global Descriptor Table](https://en.wikipedia.org/wiki/Global_Descriptor_Table)
* [stack frame](https://en.wikipedia.org/wiki/Call_stack)
* [Model Specific regiser](https://en.wikipedia.org/wiki/Model-specific_register)
* [percpu](https://0xax.gitbook.io/linux-insides/summary/concepts/linux-cpu-1)
* [RCU](https://en.wikipedia.org/wiki/Read-copy-update) 
* [MPX](https://en.wikipedia.org/wiki/Intel_MPX)
* [x87 FPU](https://en.wikipedia.org/wiki/X87)
* [Previous part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-5)
