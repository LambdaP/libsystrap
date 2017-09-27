#ifndef RAW_SYSCALLS_H__
#define RAW_SYSCALLS_H__

/* NOTE: this file futzes with the preprocessing environment
 * presented to libc and kernel-user includes. So here be
 * dragons... more importantly, INCLUDE THIS FILE FIRST,
 * otherwise its futzing may be too late to have the intended
 * effect. */

/* for accessing members of mcontext_t */
#ifdef __FreeBSD__
#define MC_REG(lower, upper) mc_ ## lower
#else
/* So that we get the register name #defines from ucontext.h...
 * ... and struct sigaction
 * ... . */
#define _GNU_SOURCE
#define MC_REG(lower, upper) gregs[REG_ ## upper]
#endif

#include <sys/types.h>

/* At some point this got saner in glibc. Or do I mean in Linux?
 * FIXME: test properly on some older versions. */
#if defined(__linux__)
#define SYS_sigaction SYS_rt_sigaction
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ > 19)
/* We don't have asm/ucontext.h, so have to live with libc versions. */
#include <ucontext.h>
#include <signal.h>
#define __asm_sigaction sigaction
#define __asm_sigset_t sigset_t

#define __need_timespec 1 /* HACK */
#include <time.h>
#define __asm_timespec timespec

#else
/* Before including stuff,
 * rename the kernel's distinct struct types,
 * to avoid conflict with glibc. */
#define AVOID_LIBC_SIGNAL_H_
#define timezone __asm_timezone
#define timeval __asm_timeval
#define itimerval __asm_itimerval
#define itimerspec __asm_itimerspec
#define pid_t __kernel_pid_t
#include <asm/sigcontext.h>
#include <asm/siginfo.h>
#include <asm/ucontext.h>
/* sys/time.h (which later code wants to include)
 * conflicts with linux/time.h, which asm/signal.h includes :-( */
#undef ITIMER_REAL
#undef ITIMER_VIRTUAL
#undef ITIMER_PROF
#include <asm/signal.h>
#endif
#include <asm/types.h>
#include <asm/posix_types.h>
#include <asm-generic/stat.h>
#include <asm/fcntl.h>
#elif defined(__FreeBSD__)
#include <sys/signal.h>
#include <sys/ucontext.h>
#include <sys/stat.h>
#include <fcntl.h>
/* FreeBSD doesn't have separate definitions of these, 
 * so just alias the __asm_* ones to the vanilla ones. */
#define __asm_timezone timezone
#define __asm_timespec timespec
#define __asm_timeval timeval
#define __asm_itimerval itimerval
#define __asm_itimerspec itimerspec
#define __asm_sigset_t sigset_t
#define __kernel_pid_t pid_t
#else
#error "Unrecognised platform."
#endif

#include <stdint.h>

/* Our callee-save registers are
 *	 rbp, rbx, r12, r13, r14, r15
 * but all others need to be in the clobber list.
 *	 rdi, rsi, rax, rcx, rdx, r8, r9, r10, r11
 *	 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15
 *	 condition codes, memory
 */
#define SYSCALL_CLOBBER_LIST \
	"%rdi", "%rsi", "%rax", "%rcx", "%rdx", "%r8", "%r9", "%r10", "%r11", \
	"%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7", "%xmm8", \
	"%xmm9", "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15", \
	"cc" /*, "memory" */
#define FIX_STACK_ALIGNMENT \
	"movq %%rsp, %%rax\n\
	 andq $0xf, %%rax    # now we have either 8 or 0 in rax \n\
	 subq %%rax, %%rsp   # fix the stack pointer \n\
	 movq %%rax, %%r12   # save the amount we fixed it up by in r12 \n\
	 "
#define UNFIX_STACK_ALIGNMENT \
	"addq %%r12, %%rsp\n"

#define stringify(cond) #cond

#ifndef assert
#define assert(cond) \
	do { ((cond) ? ((void) 0) : (__assert_fail("Assertion failed: \"" stringify((cond)) "\"", __FILE__, __LINE__, __func__ ))); }  while (0)
#endif

#define write_string(s) raw_write(2, (s), sizeof (s) - 1)
#define write_chars(s, t)  raw_write(2, s, t - s)
#define write_ulong(a)   raw_write(2, fmt_hex_num((a)), 18)

/* In kernel-speak this is a "struct sigframe" / "struct rt_sigframe" --
 * sadly no user-level header defines it. But it seems to be vaguely standard
 * per-architecture (here Intel iBCS). */
struct ibcs_sigframe
{
	char *pretcode;
	ucontext_t uc;
	siginfo_t info;
};

void restore_rt(void); /* in restorer.s */

void raw_exit(int status) __attribute__((noreturn));
int raw_open(const char *pathname, int flags) __attribute__((noinline));
int raw_fstat(int fd, struct stat *buf) __attribute__((noinline));
int raw_stat(char *filename, struct stat *buf) __attribute__((noinline));
int raw_nanosleep(struct __asm_timespec *req,
		struct __asm_timespec *rem) __attribute__((noinline));
int raw_getpid(void) __attribute__((noinline));
int raw_kill(pid_t pid, int sig) __attribute__((noinline));
int raw_read(int fd, void *buf, size_t count) __attribute__((noinline));
ssize_t raw_write(int fd, const void *buf,
		size_t count) __attribute__((noinline));
int raw_close(int fd) __attribute__((noinline));
int raw_mprotect(const void *addr, size_t len,
		int prot) __attribute__((noinline));
void *raw_mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
int raw_munmap(void *addr, size_t length);
int raw_rt_sigaction(int signum, const struct __asm_sigaction *act,
		     struct __asm_sigaction *oldact) __attribute__((noinline));
void __assert_fail(const char *assertion, const char *file,
                   unsigned int line, const char *function) __attribute__((noreturn));
const char *fmt_hex_num(unsigned long n);

#ifdef __linux__
#undef timezone
#undef timespec
#undef timeval
#undef itimerval
#undef itimerspec
#undef sigset_t
#undef pid_t
#endif
#endif // __RAW_SYSCALLS_H__
