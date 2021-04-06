<center><font size='6'>Linux内核深入理解中断和异常（3）：异常处理的实现(X86_TRAP_xx) </font></center>
<br/>
<br/>
<center><font size='5'>rtoax</font></center>
<center><font size='5'>2021年3月</font></center>
<br/>

```c
/**
 *  start_kernel()->setup_arch()->idt_setup_early_traps()
 *  start_kernel()->setup_arch()->idt_setup_early_pf()
 *  start_kernel()->trap_init()->idt_setup_traps()
 *  start_kernel()->trap_init()->idt_setup_ist_traps()
 *  start_kernel()->early_irq_init()
 *  start_kernel()->init_IRQ()
 */
asmlinkage __visible void __init __no_sanitize_address start_kernel(void)/* 启动内核 */
{
    /**
     *  start_kernel()->setup_arch()->idt_setup_early_traps()
     *  start_kernel()->setup_arch()->idt_setup_early_pf()
     *  start_kernel()->trap_init()->idt_setup_traps()
     *  start_kernel()->trap_init()->idt_setup_ist_traps()
     */
	trap_init();  /* This function makes initialization of the remaining exceptions handlers */
	/* init some links before init_ISA_irqs() */
	early_irq_init();       /*  */
	init_IRQ();             /*  */
}
```

# 1. Implementation of exception handlers

This is the fifth part about an interrupts and exceptions handling in the Linux kernel and in the previous [part](https://rtoax.blog.csdn.net/article/details/115174192) we stopped on the setting of interrupt gates to the [Interrupt descriptor Table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table). We did it in the `trap_init` function from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c) source code file. We saw only setting of these interrupt gates in the previous part and in the current part we will see implementation of the exception handlers for these gates. 

The preparation before an exception handler will be executed is in the [arch/x86/entry/entry_64.S](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S) assembly file and occurs in the [idtentry](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/entry/entry_64.S#L820) macro that defines exceptions entry points:

```assembly
idtentry divide_error			        do_divide_error			       has_error_code=0
idtentry overflow			            do_overflow			           has_error_code=0
idtentry invalid_op			            do_invalid_op			       has_error_code=0
idtentry bounds				            do_bounds			           has_error_code=0
idtentry device_not_available		    do_device_not_available		   has_error_code=0
idtentry coprocessor_segment_overrun	do_coprocessor_segment_overrun has_error_code=0
idtentry invalid_TSS			        do_invalid_TSS			       has_error_code=1
idtentry segment_not_present		    do_segment_not_present		   has_error_code=1
idtentry spurious_interrupt_bug		    do_spurious_interrupt_bug	   has_error_code=0
idtentry coprocessor_error		        do_coprocessor_error		   has_error_code=0
idtentry alignment_check		        do_alignment_check		       has_error_code=1
idtentry simd_coprocessor_error		    do_simd_coprocessor_error	   has_error_code=0
```
在5.10.13中已经变成了这样：
```c
/**
 *  arch/x86/entry/entry_64.S
 *
 *  .macro idtentry_irq vector cfunc            for Interrupt entry/exit.
 *  .macro idtentry_sysvec vector cfunc         for System vectors
 *  .macro idtentry_mce_db vector asmsym cfunc  for #MC and #DB
 *  .macro idtentry_vc vector asmsym cfunc      for #VC
 *  .macro idtentry_df vector asmsym cfunc      for Double fault
 *  
 */
```
结合两者看：

```c
/**
 * idtentry - Macro to generate entry stubs for simple IDT entries
 * @vector:		Vector number
 * @asmsym:		ASM symbol for the entry point
 * @cfunc:		C function to be called
 * @has_error_code:	Hardware pushed error code on stack
 *
 * The macro emits code to set up the kernel context for straight forward
 * and simple IDT entries. No IST stack, no paranoid entry checks.
 */
.macro idtentry vector asmsym cfunc has_error_code:req
SYM_CODE_START(\asmsym)
	UNWIND_HINT_IRET_REGS offset=\has_error_code*8
	ASM_CLAC

	.if \has_error_code == 0
		pushq	$-1			/* ORIG_RAX: no syscall to restart */
	.endif

	.if \vector == X86_TRAP_BP
		/*
		 * If coming from kernel space, create a 6-word gap to allow the
		 * int3 handler to emulate a call instruction.
		 */
		testb	$3, CS-ORIG_RAX(%rsp)
		jnz	.Lfrom_usermode_no_gap_\@
		.rept	6
		pushq	5*8(%rsp)
		.endr
		UNWIND_HINT_IRET_REGS offset=8
.Lfrom_usermode_no_gap_\@:
	.endif

	idtentry_body \cfunc \has_error_code

_ASM_NOKPROBE(\asmsym)
SYM_CODE_END(\asmsym)
.endm

/*
 * Interrupt entry/exit.
 *
 + The interrupt stubs push (vector) onto the stack, which is the error_code
 * position of idtentry exceptions, and jump to one of the two idtentry points
 * (common/spurious).
 *
 * common_interrupt is a hotpath, align it to a cache line
 */
.macro idtentry_irq vector cfunc
	.p2align CONFIG_X86_L1_CACHE_SHIFT
	idtentry \vector asm_\cfunc \cfunc has_error_code=1
.endm
```

The `idtentry` macro does following preparation before an actual exception handler (`do_divide_error` for the `divide_error`, `do_overflow` for the `overflow` and etc.) will get control. In another words the `idtentry` macro allocates place for the registers ([pt_regs](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/include/uapi/asm/ptrace.h#L43) structure) on the stack, pushes dummy error code for the stack consistency if an interrupt/exception has no error code, checks the segment selector in the `cs` segment register and switches depends on the previous state(userspace or kernelspace). After all of these preparations it makes a call of an actual interrupt/exception handler:

```assembly
.macro idtentry sym do_sym has_error_code:req paranoid=0 shift_ist=-1
ENTRY(\sym)
	...
	...
	...
	call	\do_sym
	...
	...
	...
END(\sym)
.endm
```

After an exception handler will finish its work, the `idtentry` macro restores stack and general purpose registers of an interrupted task and executes [iret](http://x86.renejeschke.de/html/file_module_x86_id_145.html) instruction:

```assembly
ENTRY(paranoid_exit)
	...
	...
	...
	RESTORE_EXTRA_REGS
	RESTORE_C_REGS
	REMOVE_PT_GPREGS_FROM_STACK 8
	INTERRUPT_RETURN
END(paranoid_exit)
```

where `INTERRUPT_RETURN` is:

```assembly
#define INTERRUPT_RETURN	jmp native_iret
...
ENTRY(native_iret)
.global native_irq_return_iret
native_irq_return_iret:
iretq
```

More about the `idtentry` macro you can read in the third part of the [初步中断处理-中断加载](https://rtoax.blog.csdn.net/article/details/115174192) chapter. Ok, now we saw the preparation before an exception handler will be executed and now time to look on the handlers. 

First of all let's look on the following handlers:

* divide_error
* overflow
* invalid_op
* coprocessor_segment_overrun
* invalid_TSS
* segment_not_present
* stack_segment
* alignment_check

All these handlers defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c) source code file with the `DO_ERROR` macro:

```C
DO_ERROR(X86_TRAP_DE,     SIGFPE,  "divide error",                divide_error)
DO_ERROR(X86_TRAP_OF,     SIGSEGV, "overflow",                    overflow)
DO_ERROR(X86_TRAP_UD,     SIGILL,  "invalid opcode",              invalid_op)
DO_ERROR(X86_TRAP_OLD_MF, SIGFPE,  "coprocessor segment overrun", coprocessor_segment_overrun)
DO_ERROR(X86_TRAP_TS,     SIGSEGV, "invalid TSS",                 invalid_TSS)
DO_ERROR(X86_TRAP_NP,     SIGBUS,  "segment not present",         segment_not_present)
DO_ERROR(X86_TRAP_SS,     SIGBUS,  "stack segment",               stack_segment)
DO_ERROR(X86_TRAP_AC,     SIGBUS,  "alignment check",             alignment_check)
``` 

As we can see the `DO_ERROR` macro takes 4 parameters:

* Vector number of an interrupt;
* Signal number which will be sent to the interrupted process;
* String which describes an exception;
* Exception handler entry point.

This macro defined in the same source code file and expands to the function with the `do_handler` name:

```C
#define DO_ERROR(trapnr, signr, str, name)                              \
dotraplinkage void do_##name(struct pt_regs *regs, long error_code)     \
{                                                                       \
        do_error_trap(regs, error_code, str, trapnr, signr);            \
}
```

Note on the `##` tokens. This is special feature - [GCC macro Concatenation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html#Concatenation) which concatenates two given strings. For example, first `DO_ERROR` in our example will expands to the:

```C
dotraplinkage void do_divide_error(struct pt_regs *regs, long error_code)     \
{
	...
}
```

We can see that all functions which are generated by the `DO_ERROR` macro just make a call of the `do_error_trap` function from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). Let's look on implementation of the `do_error_trap` function.


# 2. Trap handlers

The `do_error_trap` function starts and ends from the two following functions:

```C
enum ctx_state prev_state = exception_enter();
...
...
...
exception_exit(prev_state);
```

5.10.13中是这样的：
```c
static void do_error_trap(struct pt_regs *regs, long error_code, char *str,
	unsigned long trapnr, int signr, int sicode, void __user *addr)
{
	RCU_LOCKDEP_WARN(!rcu_is_watching(), "entry code didn't wake RCU");

	if (notify_die(DIE_TRAP, str, regs, error_code, trapnr, signr) !=
			NOTIFY_STOP) {
		cond_local_irq_enable(regs);
		do_trap(trapnr, signr, str, regs, error_code, sicode, addr);
		cond_local_irq_disable(regs);
	}
}
```

from the [include/linux/context_tracking.h](https://github.com/torvalds/linux/tree/master/include/linux/context_tracking.h). The context tracking in the Linux kernel subsystem which provide kernel boundaries probes to keep track of the transitions between level contexts with two basic initial contexts: `user` or `kernel`. The `exception_enter` function checks that context tracking is enabled. After this if it is enabled, the `exception_enter` reads previous context and compares it with the `CONTEXT_KERNEL`. If the previous context is `user`, we call `context_tracking_exit` function from the [kernel/context_tracking.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/context_tracking.c) which inform the context tracking subsystem that a processor is exiting user mode and entering the kernel mode:

```C
if (!context_tracking_is_enabled())
	return 0;

prev_ctx = this_cpu_read(context_tracking.state);
if (prev_ctx != CONTEXT_KERNEL)
	context_tracking_exit(prev_ctx);

return prev_ctx;
```

If previous context is non `user`, we just return it. The `pre_ctx` has `enum ctx_state` type which defined in the [include/linux/context_tracking_state.h](https://github.com/torvalds/linux/tree/master/include/linux/context_tracking_state.h) and looks as:

```C
enum ctx_state {
	CONTEXT_KERNEL = 0,
	CONTEXT_USER,
	CONTEXT_GUEST,
} state;
```

The second function is `exception_exit` defined in the same [include/linux/context_tracking.h](https://github.com/torvalds/linux/tree/master/include/linux/context_tracking.h) file and checks that context tracking is enabled and call the `contert_tracking_enter` function if the previous context was `user`:

```C
static inline void exception_exit(enum ctx_state prev_ctx)
{
	if (context_tracking_is_enabled()) {
		if (prev_ctx != CONTEXT_KERNEL)
			context_tracking_enter(prev_ctx);
	}
}
```

The `context_tracking_enter` function informs the context tracking subsystem that a processor is going to enter to the user mode from the kernel mode. We can see the following code between the `exception_enter` and `exception_exit`:

```C
if (notify_die(DIE_TRAP, str, regs, error_code, trapnr, signr) !=
		NOTIFY_STOP) {
	conditional_sti(regs);
	do_trap(trapnr, signr, str, regs, error_code,
		fill_trap_info(regs, signr, trapnr, &info));
}
```

First of all it calls the `notify_die` function which defined in the [kernel/notifier.c](https://github.com/torvalds/linux/tree/master/kernel/notifier.c). 

```c
//通知链
int notrace notify_die(enum die_val val, const char *str,
	       struct pt_regs *regs, long err, int trap, int sig)
{
	struct die_args args = {
		.regs	= regs,
		.str	= str,
		.err	= err,
		.trapnr	= trap,
		.signr	= sig,

	};
	RCU_LOCKDEP_WARN(!rcu_is_watching(),
			   "notify_die called but RCU thinks we're quiescent");
	return atomic_notifier_call_chain(&die_chain, val, &args);
}
NOKPROBE_SYMBOL(notify_die);
```

To get notified for [kernel panic](https://en.wikipedia.org/wiki/Kernel_panic), [kernel oops](https://en.wikipedia.org/wiki/Linux_kernel_oops), [Non-Maskable Interrupt](https://en.wikipedia.org/wiki/Non-maskable_interrupt) or other events the caller needs to insert itself in the `notify_die` chain and the `notify_die` function does it. 

枚举如下：
```c
/* Grossly misnamed. */
enum die_val {  /*  */
	DIE_OOPS = 1,   /* Oops */
	DIE_INT3,
	DIE_DEBUG,
	DIE_PANIC,
	DIE_NMI,    /* 不可屏蔽中断 */
	DIE_DIE,
	DIE_KERNELDEBUG,
	DIE_TRAP,   /* 陷阱 */
	DIE_GPF,
	DIE_CALL,
	DIE_PAGE_FAULT,
	DIE_NMIUNKNOWN,
};
```

> 通知链：The Linux kernel has special mechanism that allows kernel to ask when something happens and this mechanism called `notifiers` or `notifier chains`. This mechanism used for example for the `USB` hotplug events (look on the [drivers/usb/core/notify.c](https://github.com/torvalds/linux/tree/master/drivers/usb/core/notify.c)), for the memory [hotplug](https://en.wikipedia.org/wiki/Hot_swapping) (look on the [include/linux/memory.h](https://github.com/torvalds/linux/tree/master/include/linux/memory.h), the `hotplug_memory_notifier` macro and etc...), system reboots and etc. A notifier chain is thus a simple, singly-linked list. When a Linux kernel subsystem wants to be notified of specific events, it fills out a special `notifier_block` structure and passes it to the `notifier_chain_register` function. An event can be sent with the call of the `notifier_call_chain` function. 
 
First of all the `notify_die` function fills `die_args` structure with the trap number, trap string, registers and other values:

```C
struct die_args args = {
       .regs   = regs,
       .str    = str,
       .err    = err,
       .trapnr = trap,
       .signr  = sig,
}
```

and returns the result of the `atomic_notifier_call_chain` function with the `die_chain`:

```C
static ATOMIC_NOTIFIER_HEAD(die_chain);
return atomic_notifier_call_chain(&die_chain, val, &args);
```

which just expands to the `atomic_notifier_head` structure that contains lock and `notifier_block`:

```C
struct atomic_notifier_head {
        spinlock_t lock;
        struct notifier_block __rcu *head;
};
```

The `atomic_notifier_call_chain` function calls each function in a notifier chain in turn and returns the value of the last notifier function called. If the `notify_die` in the `do_error_trap` does not return `NOTIFY_STOP` we execute `conditional_sti` function from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/traps.c) that checks the value of the [interrupt flag](https://en.wikipedia.org/wiki/Interrupt_flag) and enables interrupt depends on it:

```C
static inline void conditional_sti(struct pt_regs *regs)
{
        if (regs->flags & X86_EFLAGS_IF)
                local_irq_enable();
}
```

more about `local_irq_enable` macro you can read in the second [part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-2) of this chapter. 

The next and last call in the `do_error_trap` is the `do_trap` function. 

```c
static void
do_trap(int trapnr, int signr, char *str, struct pt_regs *regs,
	long error_code, int sicode, void __user *addr)
{
	struct task_struct *tsk = current;

	if (!do_trap_no_signal(tsk, trapnr, str, regs, error_code))
		return;

	show_signal(tsk, signr, "trap ", str, regs, error_code);

	if (!sicode)
		force_sig(signr);
	else
		force_sig_fault(signr, sicode, addr);
}
NOKPROBE_SYMBOL(do_trap);
```

First of all the `do_trap` function defined the `tsk` variable which has `task_struct` type and represents the current interrupted process. After the definition of the `tsk`, we can see the call of the `do_trap_no_signal` function:

```C
struct task_struct *tsk = current;

if (!do_trap_no_signal(tsk, trapnr, str, regs, error_code))
	return;
```

The `do_trap_no_signal` function makes two checks:

* Did we come from the [Virtual 8086](https://en.wikipedia.org/wiki/Virtual_8086_mode) mode;
* Did we come from the kernelspace.

```C
if (v8086_mode(regs)) {
	...
}

if (!user_mode(regs)) {
	...
}

return -1;
```

在长模式下，没有virtual 8086模式：
```c
static inline int v8086_mode(struct pt_regs *regs)
{
#ifdef CONFIG_X86_32
	return (regs->flags & X86_VM_MASK);
#else
	return 0;	/* No V86 mode support in long mode */
#endif
}
```

We will not consider first case because the [long mode](https://en.wikipedia.org/wiki/Long_mode) does not support the [Virtual 8086](https://en.wikipedia.org/wiki/Virtual_8086_mode) mode. In the second case we invoke `fixup_exception` function which will try to recover a fault and `die` if we can't:

```C
if (!fixup_exception(regs)) {
	tsk->thread.error_code = error_code;
	tsk->thread.trap_nr = trapnr;
	die(str, regs, error_code);
}
```

The `die` function defined in the [arch/x86/kernel/dumpstack.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/arch/x86/kernel/dumpstack.c) source code file, 

```c
/*
 * This is gone through when something in the kernel has done something bad
 * and is about to be terminated:
 */
void die(const char *str, struct pt_regs *regs, long err)
{
	unsigned long flags = oops_begin();
	int sig = SIGSEGV;

	if (__die(str, regs, err))
		sig = 0;
	oops_end(flags, regs, sig);
}
```

prints useful information about stack, registers, kernel modules and caused kernel [oops](https://en.wikipedia.org/wiki/Linux_kernel_oops). If we came from the userspace the `do_trap_no_signal` function will return `-1` and the execution of the `do_trap` function will continue. If we passed through the `do_trap_no_signal` function and did not exit from the `do_trap` after this, it means that previous context was - `user`.  

Most exceptions caused by the processor are interpreted by Linux as error conditions, for example division by zero, invalid opcode and etc. When an exception occurs the Linux kernel sends a [signal](https://en.wikipedia.org/wiki/Unix_signal) to the interrupted process that caused the exception to notify it of an incorrect condition. So, in the `do_trap` function we need to send a signal with the given number (`SIGFPE` for the divide error, `SIGILL` for a illegal instruction and etc...). First of all we save error code and vector number in the current interrupts process with the filling `thread.error_code` and `thread_trap_nr`:

```C
tsk->thread.error_code = error_code;
tsk->thread.trap_nr = trapnr;
```

After this we make a check do we need to print information about unhandled signals for the interrupted process. We check that `show_unhandled_signals` variable is set, that `unhandled_signal` function from the [kernel/signal.c](https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/kernel/signal.c) will return unhandled signal(s) and [printk](https://en.wikipedia.org/wiki/Printk) rate limit:

```C
#ifdef CONFIG_X86_64
	if (show_unhandled_signals && unhandled_signal(tsk, signr) &&
	    printk_ratelimit()) {
		pr_info("%s[%d] trap %s ip:%lx sp:%lx error:%lx",
			tsk->comm, tsk->pid, str,
			regs->ip, regs->sp, error_code);
		print_vma_addr(" in ", regs->ip);
		pr_cont("\n");
	}
#endif
```

在5.10.13中封装成：
```c
static void show_signal(struct task_struct *tsk, int signr,
			const char *type, const char *desc,
			struct pt_regs *regs, long error_code)
{
	if (show_unhandled_signals && unhandled_signal(tsk, signr) &&
	    printk_ratelimit()) {
		pr_info("%s[%d] %s%s ip:%lx sp:%lx error:%lx",
			tsk->comm, task_pid_nr(tsk), type, desc,
			regs->ip, regs->sp, error_code);
		print_vma_addr(KERN_CONT " in ", regs->ip);
		pr_cont("\n");
	}
}
```

And send a given signal to interrupted process:

```C
force_sig_info(signr, info ?: SEND_SIG_PRIV, tsk);
```

This is the end of the `do_trap`. We just saw generic implementation for eight different exceptions which are defined with the `DO_ERROR` macro. Now let's look on another exception handlers.

# 3. Double fault

```c
#define X86_TRAP_DF		 8	/*  8, 当前处于异常处理,异常再次发生 *//* Double Fault */
                            /**
                             *      exc_double_fault : arch\x86\kernel\traps.c
                             *      ()
                             */
```

The next exception is `#DF` or `Double fault`. This exception occurs when the processor detected a second exception while calling an exception handler for a prior exception.
在默认处理函数中对于double fault为：
```c
static const __initconst struct idt_data def_idts[] = {/* 默认的 中断描述符表 */
    ...
#ifdef CONFIG_X86_32
//	TSKG(X86_TRAP_DF,		GDT_ENTRY_DOUBLEFAULT_TSS),
#else
	INTG(X86_TRAP_DF,		asm_exc_double_fault),
#endif
    ...
};
```

We set the trap gate for this exception in the previous part:

```C
set_intr_gate_ist(X86_TRAP_DF, &double_fault, DOUBLEFAULT_STACK);
```

Note that this exception runs on the `DOUBLEFAULT_STACK` [Interrupt Stack Table](https://www.kernel.org/doc/Documentation/x86/x86_64/kernel-stacks) which has index - `1`:

```C
#define DOUBLEFAULT_STACK 1
```

The `double_fault` is handler for this exception and defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). The `double_fault` handler starts from the definition of two variables: string that describes exception and interrupted process, as other exception handlers:

```C
static const char str[] = "double fault";
struct task_struct *tsk = current;
```

The handler of the double fault exception split on two parts. 

* The first part is the check which checks that a fault is a `non-IST` fault on the `espfix64` stack. Actually the `iret` instruction restores only the bottom `16` bits when returning to a `16` bit segment. The `espfix` feature solves this problem. So if the `non-IST` fault on the espfix64 stack we modify the stack to make it look like `General Protection Fault`:

```C
struct pt_regs *normal_regs = task_pt_regs(current);

memmove(&normal_regs->ip, (void *)regs->sp, 5*8);
ormal_regs->orig_ax = 0;
regs->ip = (unsigned long)general_protection;
regs->sp = (unsigned long)&normal_regs->orig_ax;
return;
```
在5.10.13中为：
```c
#ifdef CONFIG_X86_ESPFIX64
	extern unsigned char native_irq_return_iret[];

	/*
	 * If IRET takes a non-IST fault on the espfix64 stack, then we
	 * end up promoting it to a doublefault.  In that case, take
	 * advantage of the fact that we're not using the normal (TSS.sp0)
	 * stack right now.  We can write a fake #GP(0) frame at TSS.sp0
	 * and then modify our own IRET frame so that, when we return,
	 * we land directly at the #GP(0) vector with the stack already
	 * set up according to its expectations.
	 *
	 * The net result is that our #GP handler will think that we
	 * entered from usermode with the bad user context.
	 *
	 * No need for nmi_enter() here because we don't use RCU.
	 */
	if (((long)regs->sp >> P4D_SHIFT) == ESPFIX_PGD_ENTRY &&
		regs->cs == __KERNEL_CS &&
		regs->ip == (unsigned long)native_irq_return_iret)
	{
		struct pt_regs *gpregs = (struct pt_regs *)this_cpu_read(cpu_tss_rw.x86_tss.sp0) - 1;
		unsigned long *p = (unsigned long *)regs->sp;

		/*
		 * regs->sp points to the failing IRET frame on the
		 * ESPFIX64 stack.  Copy it to the entry stack.  This fills
		 * in gpregs->ss through gpregs->ip.
		 *
		 */
		gpregs->ip	= p[0];
		gpregs->cs	= p[1];
		gpregs->flags	= p[2];
		gpregs->sp	= p[3];
		gpregs->ss	= p[4];
		gpregs->orig_ax = 0;  /* Missing (lost) #GP error code */

		/*
		 * Adjust our frame so that we return straight to the #GP
		 * vector with the expected RSP value.  This is safe because
		 * we won't enable interupts or schedule before we invoke
		 * general_protection, so nothing will clobber the stack
		 * frame we just set up.
		 *
		 * We will enter general_protection with kernel GSBASE,
		 * which is what the stub expects, given that the faulting
		 * RIP will be the IRET instruction.
		 */
		regs->ip = (unsigned long)asm_exc_general_protection;
		regs->sp = (unsigned long)&gpregs->orig_ax;

		return;
	}
#endif
```

* In the second case we do almost the same that we did in the previous exception handlers. The first is the call of the `ist_enter` function that discards previous context, `user` in our case:

```C
ist_enter(regs);
```

And after this we fill the interrupted process with the vector number of the `Double fault` exception and error code as we did it in the previous handlers:

```C
tsk->thread.error_code = error_code;
tsk->thread.trap_nr = X86_TRAP_DF;
```

Next we print useful information about the double fault ([PID](https://en.wikipedia.org/wiki/Process_identifier) number, registers content):

```C
#ifdef CONFIG_DOUBLEFAULT
	df_debug(regs, error_code);
#endif
```

And die:

```
	for (;;)
		die(str, regs, error_code);
```

That's all.

# 4. Device not available exception handler

```c
#define X86_TRAP_NM		 7	/*  7, 设备不可用 *//* Device Not Available */
                            /**
                             *      exc_device_not_available  : arch\x86\kernel\traps.c
                             *      ()
                             */
```

The next exception is the `#NM` or `Device not available`. The `Device not available` exception can occur depending on these things:

* The processor executed an [x87 FPU](https://en.wikipedia.org/wiki/X87) floating-point instruction while the EM flag in [control register](https://en.wikipedia.org/wiki/Control_register) `cr0` was set;
* The processor executed a `wait` or `fwait` instruction while the `MP` and `TS` flags of register `cr0` were set;
* The processor executed an [x87 FPU](https://en.wikipedia.org/wiki/X87), [MMX](https://en.wikipedia.org/wiki/MMX_%28instruction_set%29) or [SSE](https://en.wikipedia.org/wiki/Streaming_SIMD_Extensions) instruction while the `TS` flag in control register `cr0` was set and the `EM` flag is clear.

先给出5.10.13中的处理函数：
```c
DEFINE_IDTENTRY(exc_device_not_available)
{
	unsigned long cr0 = read_cr0();

#ifdef CONFIG_MATH_EMULATION
	if (!boot_cpu_has(X86_FEATURE_FPU) && (cr0 & X86_CR0_EM)) {
		struct math_emu_info info = { };

		cond_local_irq_enable(regs);

		info.regs = regs;
		math_emulate(&info);

		cond_local_irq_disable(regs);
		return;
	}
#endif

	/* This should not happen. */
	if (WARN(cr0 & X86_CR0_TS, "CR0.TS was set")) {
		/* Try to fix it up and carry on. */
		write_cr0(cr0 & ~X86_CR0_TS);
	} else {
		/*
		 * Something terrible happened, and we're better off trying
		 * to kill the task than getting stuck in a never-ending
		 * loop of #NM faults.
		 */
		die("unexpected #NM exception", regs, 0);
	}
}
```

The handler of the `Device not available` exception is the `do_device_not_available` function and it defined in the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c) source code file too. It starts and ends from the getting of the previous context, as other traps which we saw in the beginning of this part:

```C
enum ctx_state prev_state;
prev_state = exception_enter();
...
...
...
exception_exit(prev_state);
```

In the next step we check that `FPU` is not eager:

```C
BUG_ON(use_eager_fpu());
```

When we switch into a task or interrupt we may avoid loading the `FPU` state. If a task will use it, we catch `Device not Available exception` exception. If we loading the `FPU` state during task switching, the `FPU` is eager. In the next step we check `cr0` control register on the `EM` flag which can show us is `x87` floating point unit present (flag clear) or not (flag set):

```C
#ifdef CONFIG_MATH_EMULATION
	if (read_cr0() & X86_CR0_EM) {
		struct math_emu_info info = { };

		conditional_sti(regs);

		info.regs = regs;
		math_emulate(&info);
		exception_exit(prev_state);
		return;
	}
#endif
```

If the `x87` floating point unit not presented, we enable interrupts with the `conditional_sti`, fill the `math_emu_info` (defined in the [arch/x86/include/asm/math_emu.h](https://github.com/torvalds/linux/tree/master/arch/x86/include/asm/math_emu.h)) structure with the registers of an interrupt task and call `math_emulate` function from the [arch/x86/math-emu/fpu_entry.c](https://github.com/torvalds/linux/tree/master/arch/x86/math-emu/fpu_entry.c). As you can understand from function's name, it emulates `X87 FPU` unit (more about the `x87` we will know in the special chapter). In other way, if `X86_CR0_EM` flag is clear which means that `x87 FPU` unit is presented, we call the `fpu__restore` function from the [arch/x86/kernel/fpu/core.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/fpu/core.c) which copies the `FPU` registers from the `fpustate` to the live hardware registers. After this `FPU` instructions can be used:

```C
fpu__restore(&current->thread.fpu);
```

# 5. General protection fault exception handler

```c
#define X86_TRAP_GP		13	/* 13, 一般保护故障 *//* General Protection Fault */
                            /**
                             *      exc_general_protection  : arch\x86\kernel\traps.c
                             *      ()
                             */
```

The next exception is the `#GP` or `General protection fault`. This exception occurs when the processor detected one of a class of protection violations called `general-protection violations`. It can be:

> violations: 违反，违法，违章，越轨，触犯法令; 侵犯；破坏; 违例，犯规;


* Exceeding the segment limit when accessing the `cs`, `ds`, `es`, `fs` or `gs` segments;
* Loading the `ss`, `ds`, `es`, `fs` or `gs` register with a segment selector for a system segment.;
* Violating any of the privilege rules;
* and other...

The exception handler for this exception is the `do_general_protection` from the [arch/x86/kernel/traps.c](https://github.com/torvalds/linux/tree/master/arch/x86/kernel/traps.c). The `do_general_protection` function starts and ends as other exception handlers from the getting of the previous context:

```C
prev_state = exception_enter();
...
exception_exit(prev_state);
```

After this we enable interrupts if they were disabled and check that we came from the [Virtual 8086](https://en.wikipedia.org/wiki/Virtual_8086_mode) mode:

```C
conditional_sti(regs);

if (v8086_mode(regs)) {
	local_irq_enable();
	handle_vm86_fault((struct kernel_vm86_regs *) regs, error_code);
	goto exit;
}
```

As long mode does not support this mode, we will not consider exception handling for this case. In the next step check that previous mode was kernel mode and try to fix the trap. If we can't fix the current general protection fault exception we fill the interrupted process with the vector number and error code of the exception and add it to the `notify_die` chain:

```C
if (!user_mode(regs)) {
	if (fixup_exception(regs))
		goto exit;

	tsk->thread.error_code = error_code;
	tsk->thread.trap_nr = X86_TRAP_GP;
	if (notify_die(DIE_GPF, "general protection fault", regs, error_code,
		       X86_TRAP_GP, SIGSEGV) != NOTIFY_STOP)
		die("general protection fault", regs, error_code);
	goto exit;
}
```

If we can fix exception we go to the `exit` label which exits from exception state:

```C
exit:
	exception_exit(prev_state);
```

If we came from user mode we send `SIGSEGV` signal to the interrupted process from user mode as we did it in the `do_trap` function:

```C
if (show_unhandled_signals && unhandled_signal(tsk, SIGSEGV) &&
		printk_ratelimit()) {
	pr_info("%s[%d] general protection ip:%lx sp:%lx error:%lx",
		tsk->comm, task_pid_nr(tsk),
		regs->ip, regs->sp, error_code);
	print_vma_addr(" in ", regs->ip);
	pr_cont("\n");
}

force_sig_info(SIGSEGV, SEND_SIG_PRIV, tsk);
```

如果既不是用户态，也不可 fixup这个异常，继续往下看，如果不可抢占、kprobe在运行、kprobe->fault_handler执行成功：
```c
	/*
	 * To be potentially processing a kprobe fault and to trust the result
	 * from kprobe_running(), we have to be non-preemptible.
	 */
	if (!preemptible() &&
	    kprobe_running() &&
	    kprobe_fault_handler(regs, X86_TRAP_GP))
		goto exit;
```
如果上面情况不满足：
```c
    /**
     *  如果上面的抢救措施都不行，只能 通知   段错误
     */
	ret = notify_die(DIE_GPF, desc, regs, error_code, X86_TRAP_GP, SIGSEGV);
	if (ret == NOTIFY_STOP)
		goto exit;

	if (error_code)
		snprintf(desc, sizeof(desc), "segment-related " GPFSTR);
	else
		hint = get_kernel_gp_address(regs, &gp_addr);

	if (hint != GP_NO_HINT)
		snprintf(desc, sizeof(desc), GPFSTR ", %s 0x%lx",
			 (hint == GP_NON_CANONICAL) ? "probably for non-canonical address"
						    : "maybe for address",
			 gp_addr);

	/*
	 * KASAN is interested only in the non-canonical case, clear it
	 * otherwise.
	 */
	if (hint != GP_NON_CANONICAL)
		gp_addr = 0;

	die_addr(desc, regs, error_code, gp_addr);
```

That's all.

# 6. X86_TRAP_XX注释
```c
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_TRAPNR_H
#define _ASM_X86_TRAPNR_H

/* Interrupts/Exceptions */

/**
 *  start_kernel()->setup_arch()->idt_setup_early_traps()
 *  start_kernel()->setup_arch()->idt_setup_early_pf()
 *  start_kernel()->trap_init()->idt_setup_traps()
 *  start_kernel()->trap_init()->idt_setup_ist_traps()
 *  start_kernel()->early_irq_init()
 *  start_kernel()->init_IRQ()
 */

/**
 *  arch\x86\include\asm\idtentry.h
 */

/**
 *  arch/x86/entry/entry_64.S
 *
 *  .macro idtentry_irq vector cfunc            for Interrupt entry/exit.
 *  .macro idtentry_sysvec vector cfunc         for System vectors
 *  .macro idtentry_mce_db vector asmsym cfunc  for #MC and #DB
 *  .macro idtentry_vc vector asmsym cfunc      for #VC
 *  .macro idtentry_df vector asmsym cfunc      for Double fault
 *  
 */


#define X86_TRAP_DE		 0	/*  0, 除零错误 *//* Divide-by-zero */    /* 被 0 除 */
                            /**
                             *  exc_divide_error    : arch\x86\kernel\traps.c
                             *  do_error_trap(...)  : arch\x86\kernel\traps.c
                             */
#define X86_TRAP_DB		 1	/*  1, 调试 *//* Debug 没有 Error Code */
                            //+-----------------------------------------------------+
                            //|Vector|Mnemonic|Description         |Type |Error Code|
                            //+-----------------------------------------------------+
                            //|1     | #DB    |Reserved            |F/T  |NO        |
                            //+-----------------------------------------------------+
                            /**
                             *  X86_TRAP_DB
                             *      exc_debug           : arch\x86\kernel\traps.c
                             *      exc_debug_kernel()  : arch\x86\kernel\traps.c
                             *      exc_debug_user()    : arch\x86\kernel\traps.c
                             */



#define X86_TRAP_NMI	 2	/*  2, 不可屏蔽中断 *//* Non-maskable Interrupt 不可屏蔽中断， 严重问题 */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_BP		 3	/*  3, 断点 *//* Breakpoint */    /* 断点 -> int3 */
                            /**
                             *  X86_TRAP_BP
                             *      do_int3() : arch\x86\kernel\traps.c
                             *      exc_int3  : arch\x86\kernel\traps.c
                             */
                            //#include <stdio.h>
                            //
                            //int main() {
                            //    int i;
                            //    while (i < 6){
                            //	    printf("i equal to: %d\n", i);
                            //	    __asm__("int3");
                            //		++i;
                            //    }
                            //}

#define X86_TRAP_OF		 4	/*  4, 溢出 *//* Overflow */
                            /**
                             *  exc_overflow        : arch\x86\kernel\traps.c
                             *  do_error_trap(...)  : arch\x86\kernel\traps.c
                             */
#define X86_TRAP_BR		 5	/*  5, 超出范围 *//* Bound Range Exceeded */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_UD		 6	/*  6, 操作码无效 *//* Invalid Opcode */
                            /**
                             *  X86_TRAP_UD
                             *      exc_invalid_op  : arch\x86\kernel\traps.c
                             *      handle_invalid_op()
                             */

//* an [x87 FPU]floating-point instruction while the EM flag in [control register]`cr0` was set;
//* a `wait` or `fwait` instruction while the `MP` and `TS` flags of register `cr0` were set;
//* an [x87 FPU], [MMX] or [SSE] instruction while the `TS` flag in control register `cr0` was set and the `EM` flag is clear.
#define X86_TRAP_NM		 7	/*  7, 设备不可用 *//* Device Not Available */
                            /**
                             *      exc_device_not_available  : arch\x86\kernel\traps.c
                             *      ()
                             */

#define X86_TRAP_DF		 8	/*  8, 当前处于异常处理,异常再次发生 *//* Double Fault */
                            /**
                             *      exc_double_fault : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_OLD_MF	9	/*  9, 协处理器段溢出 *//* Coprocessor Segment Overrun */
                            /**
                             *      exc_coproc_segment_overrun  : arch\x86\kernel\traps.c
                             *      do_error_trap()
                             */
#define X86_TRAP_TS		10	/* 10, 无效的 TSS *//* Invalid TSS */
                            /**
                             *      exc_invalid_tss  : arch\x86\kernel\traps.c
                             *      do_error_trap()
                             */
#define X86_TRAP_NP		11	/* 11, 段不存在 *//* Segment Not Present */
                            /**
                             *      exc_segment_not_present  : arch\x86\kernel\traps.c
                             *      do_error_trap()
                             */
#define X86_TRAP_SS		12	/* 12, 堆栈段故障 *//* Stack Segment Fault */
                            /**
                             *      exc_stack_segment  : arch\x86\kernel\traps.c
                             *      do_error_trap()
                             */
                             
//* Exceeding the segment limit when accessing the `cs`, `ds`, `es`, `fs` or `gs` segments;
//* Loading the `ss`, `ds`, `es`, `fs` or `gs` register with a segment selector for a system segment.;
//* Violating any of the privilege rules;
//* and other...
#define X86_TRAP_GP		13	/* 13, 一般保护故障 *//* General Protection Fault */
                            /**
                             *      exc_general_protection  : arch\x86\kernel\traps.c
                             *      ()
                             */


#define X86_TRAP_PF		14	/* 14, 页错误 *//* Page Fault */    /* 页 fault */
                            /**
                             *  X86_TRAP_PF
                             *      exc_page_fault  : fault.c arch\x86\mm 
                             *      handle_page_fault()
                             */
#define X86_TRAP_SPURIOUS	15	/* 15, 伪中断 *//* Spurious Interrupt */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_MF		16	/* 16, x87 浮点异常 *//* x87 Floating-Point Exception */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_AC		17	/* 17, 对齐检查 *//* Alignment Check */
                            /**
                             *      exc_alignment_check  : arch\x86\kernel\traps.c
                             *      do_trap()
                             */
#define X86_TRAP_MC		18	/* 18, 机器检测 *//* Machine Check */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_XF		19	/* 19, SIMD （单指令多数据结构浮点）异常 *//* SIMD Floating-Point Exception */
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

#define X86_TRAP_VE		20	/* 20, 虚拟化异常 *//* Virtualization Exception */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_CP		21	/* 21, 控制保护异常 *//* Control Protection Exception */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_VC		29	/* 29, VMM 异常 *//* VMM Communication Exception 硬件虚拟化之vmm接管异常中断 */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */
#define X86_TRAP_IRET	32	/* 32, IRET （中断返回）异常 *//* IRET Exception */
                            /**
                             *        : arch\x86\kernel\traps.c
                             *      ()
                             */



#endif
```

# 7. Conclusion

It is the end of the fifth part of the [Interrupts and Interrupt Handling](https://0xax.gitbook.io/linux-insides/summary/interrupts) chapter and we saw implementation of some interrupt handlers in this part. In the next part we will continue to dive into interrupt and exception handlers and will see handler for the [Non-Maskable Interrupts](https://en.wikipedia.org/wiki/Non-maskable_interrupt), handling of the math [coprocessor](https://en.wikipedia.org/wiki/Coprocessor) and [SIMD](https://en.wikipedia.org/wiki/SIMD) coprocessor exceptions and many many more.

If you have any questions or suggestions write me a comment or ping me at [twitter](https://twitter.com/0xAX).

**Please note that English is not my first language, And I am really sorry for any inconvenience. If you find any mistakes please send me PR to [linux-insides](https://github.com/0xAX/linux-insides).**

# 8. Links

* [Interrupt descriptor Table](https://en.wikipedia.org/wiki/Interrupt_descriptor_table)
* [iret instruction](http://x86.renejeschke.de/html/file_module_x86_id_145.html)
* [GCC macro Concatenation](https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html#Concatenation)
* [kernel panic](https://en.wikipedia.org/wiki/Kernel_panic)
* [kernel oops](https://en.wikipedia.org/wiki/Linux_kernel_oops)
* [Non-Maskable Interrupt](https://en.wikipedia.org/wiki/Non-maskable_interrupt)
* [hotplug](https://en.wikipedia.org/wiki/Hot_swapping)
* [interrupt flag](https://en.wikipedia.org/wiki/Interrupt_flag)
* [long mode](https://en.wikipedia.org/wiki/Long_mode)
* [signal](https://en.wikipedia.org/wiki/Unix_signal)
* [printk](https://en.wikipedia.org/wiki/Printk)
* [coprocessor](https://en.wikipedia.org/wiki/Coprocessor)
* [SIMD](https://en.wikipedia.org/wiki/SIMD)
* [Interrupt Stack Table](https://www.kernel.org/doc/Documentation/x86/x86_64/kernel-stacks)
* [PID](https://en.wikipedia.org/wiki/Process_identifier)
* [x87 FPU](https://en.wikipedia.org/wiki/X87)
* [control register](https://en.wikipedia.org/wiki/Control_register)
* [MMX](https://en.wikipedia.org/wiki/MMX_%28instruction_set%29)
* [Previous part](https://0xax.gitbook.io/linux-insides/summary/interrupts/linux-interrupts-4)
