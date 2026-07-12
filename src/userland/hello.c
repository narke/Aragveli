#define SYS_EXIT  0
#define SYS_WRITE 1

static inline int syscall1(int n, unsigned arg)
{
    int ret;
    asm volatile("int $0x80"
                 : "=a"(ret)
                 : "a"(n), "b"(arg)
                 : "memory");
    return ret;
}
void main(void)
{
    syscall1(SYS_WRITE, (unsigned)"Hello, world!\n");
    syscall1(SYS_EXIT, 0);
}
