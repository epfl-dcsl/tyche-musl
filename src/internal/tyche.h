// Interface for Tyche specific functions

#ifndef _TYCHE_H
#define _TYCHE_H 1

#include "sys/select.h"
#include "unistd.h"

// Comment/uncomment to enable or disable syscalls
#define TYCHE_NO_SYSCALL
// Uncomment to run redis-musl outside of tyche using fork-exec
//#define TYCHE_DO_INIT
//#define TYCHE_RAW_MEM_SYSCALL

#define TYCHE_SOCKET_FD 14
#define TYCHE_CONNECTION_FD 15
#define TYCHE_SHARED_ADDR 0x300000

void tyche_debug(unsigned long long marker);
int tyche_socket();
int tyche_setsockopt(int fd);
int tyche_bind(int fd);
int tyche_listen(int fd);
int tyche_accept(int fd);
int tyche_fcntl(int fd, int flags);
int tyche_select(int n, fd_set* restrict rfds, fd_set* restrict wfds);
size_t tyche_read(int fd, void* buff, size_t count);
size_t tyche_write(int fd, const void* buf, size_t count);
int tyche_gettimeofday(struct timeval* restrict tv, void* restrict tz);
pid_t tyche_getpid(void);
char* tyche_getcwd(char* buf, size_t size);
int tyche_isatty(int fd);
void* tyche_mmap(void* start, size_t len, int prot, int flags, int fd, off_t off);
int tyche_munmap(void* start, size_t len);
size_t tyche_brk(void* end);
ssize_t tyche_writev(int fd, const struct iovec* iov, int count);
void tyche_suicide(unsigned int v);

void tyche_init_shared_mem(void);

#endif
