// Changes to accomodate running on top of Tyche
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include "unistd.h"
#include "stdio.h"
#include "tyche.h"
#include "stdlib.h"
#include "string.h"
#include "syscall.h"
#include "tyche_rb.h"


RB_DECLARE_ALL(char);

#define MSG_BUFFER_SIZE 1048

/// The redis enclave shared memory gets typecasted to this.
typedef struct redis_app_t {
  // Sending things to redis.
  rb_char_t to_redis;
  // Receiving messages from redis.
  rb_char_t from_redis;
  // Buffer for the to_redis.
  char to_buffer[MSG_BUFFER_SIZE];
  // Buffer for the from_redis.
  char from_buffer[MSG_BUFFER_SIZE];
} redis_app_t;


#ifdef TYCHE_NO_SYSCALL

// This is all part of the shared state introduced by tychools.
// The untrusted code is responsible for initializing the channels.
static redis_app_t * app = (redis_app_t*) TYCHE_SHARED_ADDR;
static int read_queue_is_init = 1;
#else

static redis_app_t __gbl_app = {0};
static redis_app_t * app = &(__gbl_app);
static int read_queue_is_init = 0;
#endif

enum tyche_test_state {
    TTS_INIT,
    TTS_START,
    TTS_DONE,
} tyche_test_state;

int connection_accepted = 0;
int connection_selected = 0;
enum tyche_test_state state = TTS_INIT;

void tyche_debug(unsigned long long marker) {
#ifndef TYCHE_NO_SYSCALL
    printf("Tyche Debug :)\n");
#else
  __asm__ __volatile__ (
      "movq %0, %%rdi\n\t"
      "movq $10, %%rax\n\t"
      "vmcall\n\t"
      :
      : "rm" (marker)
      : "rax", "rdi", "memory");
#endif
}

pid_t tyche_getpid() {
    return 1;
}

int tyche_isatty(int fd) {
    return 1;
}

char *tyche_getcwd(char *buf, size_t size) {
    char *pwd = "/tmp/tyche-redis";
    strncpy(buf, pwd, size);
    return strdup(pwd);
}

int tyche_gettimeofday(struct timeval *restrict tv, void *restrict tz) {
    tv->tv_sec = 0;
    tv->tv_usec = 0;
    return 0;
}

int tyche_socket() {
    // We only support a single socket for now
    return TYCHE_SOCKET_FD;
}

int tyche_setsockopt(int fd) {
    printf("setsockopt: %d\n", fd);
    // Ignore all options
    return 0;
}

int tyche_bind(int fd) {
    // Just say it succeeded
    return 0;
}

int tyche_listen(int fd) {
    // Just say it succeeded
    return 0;
}

#ifdef TYCHE_DO_INIT
static const char* app_mem_name = "/app_shared";
static const size_t app_size = 0x2000;
static const char* mempool_name = "/mempool_shared";
static const size_t mempool_size = 0x640000;
#endif

void tyche_init_shared_mem(void) {
#ifdef TYCHE_DO_INIT
    // Just need to mount the correct volumes.
    int shm_fd_app = shm_open(app_mem_name, O_RDWR, 0666);
    void *ptr_app = __mmap2((void*)TYCHE_SHARED_ADDR, app_size,
        PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd_app, 0);
    int shm_fd_mem = shm_open(mempool_name, O_RDWR, 0666);
    void* ptr_mem = __mmap2((void*)0x700000, mempool_size,
        PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd_mem, 0);
#endif
}

int tyche_accept(int fd) {
    // Initialize read queue so that the socket has some content

#ifndef TYCHE_NO_SYSCALL
    if (!read_queue_is_init) {
        memset(app->to_buffer, 0, sizeof(char) * MSG_BUFFER_SIZE);
        memset(app->from_buffer, 0, sizeof(char) * MSG_BUFFER_SIZE);
        rb_char_init(&(app->to_redis), MSG_BUFFER_SIZE, app->to_buffer);
        rb_char_init(&(app->from_redis), MSG_BUFFER_SIZE, app->from_buffer);
        read_queue_is_init = 1;

        // Put initial commands
        char *cmds = "PING\r\nSET A 666\r\nCOMMAND\r\nGET A\r\n";
        printf("Commands:\n%s", cmds);
        rb_char_write_n(&(app->to_redis), strlen(cmds), cmds);
    }
#endif

    if (!connection_accepted) {
        printf("Accepting connection\n");
        connection_accepted = 1;
        return TYCHE_CONNECTION_FD;
    } else {
        errno = EAGAIN;
        return -1;
    }
}

int tyche_fcntl(int fd, int flags) {
    if (fd == TYCHE_CONNECTION_FD) {
        if (flags == F_GETFL) {
            printf("  F_GETFL\n");
            return  0x2; // Access rights for the connection
        } else if (flags == F_SETFL) {
            printf("  F_SETFL\n");
            return 0;
        } else if (flags == F_GETFD) {
            printf("  F_GETFD\n");
            return 0;
        } else if (flags == F_SETFD) {
            printf("  F_SETFD\n");
            return 0;
        }
    }
    return 0;
}

int tyche_select(int n, fd_set *restrict rfds, fd_set *restrict wfds) {
    //printf("Tyche select\n");

    //printf("Read set:\n");
    for (int i = 0; i < 32; i++) {
        if (FD_ISSET(i, rfds)) {
            //printf("  %dTyche connection is set!\n", i);

        }
    }
    //printf("Write set:\n");
    for (int i = 0; i < 32; i++) {
        if (FD_ISSET(i, wfds)) {
            //printf("  %d\n", i);
        }
    }

    // Clear all bits
    FD_ZERO(rfds);
    FD_ZERO(wfds);

    if (!connection_selected) {
        // Set the bit for the Tyche socket
        FD_SET(TYCHE_SOCKET_FD, rfds);
        connection_selected = 1;
        //printf("Connection ready to accept\n");
        return 1;
    } else {
        while (rb_char_is_empty(&(app->to_redis))) {}
        // We got some messages on the channel!
        FD_SET(TYCHE_CONNECTION_FD, rfds);
        //printf("Channel ready to be read\n");
        return 1;
        /* switch (state) { */
        /*     case TTS_INIT: */
        /*         FD_SET(TYCHE_CONNECTION_FD, rfds); */
        /*         state = TTS_START; */
        /*         break; */
        /*     default: */
        /*         printf("Done testing, blocking on select\n"); */
        /*         while (1) { */
        /*             exit(0); */
        /*         } */
        /*         break; */
        /* } */
    }
}

size_t tyche_read(int fd, void *buff, size_t count) {
#ifndef TYCHE_NO_SYSCALL
  printf("Tyche read: %d, count: %d\n", fd, count);
#endif
    int ret = rb_char_read_alias_n(&(app->to_redis), app->to_buffer, (int) count, (char *)buff);
    if (ret == FAILURE) {
      tyche_suicide(101);
      errno = EAGAIN;
      return 0;
    }
    return ret;
}

size_t tyche_write(int fd, const void *buf, size_t count) {
#ifndef TYCHE_NO_SYSCALL
    printf("Tyche write:\n  %.*s", count, buf);
#else
    int written = 0;
    char *source = (char *) buf;
    while (written < count) {
      int res = rb_char_write_alias_n(&(app->from_redis), app->from_buffer, count - written, &source[written]);
      if (res == FAILURE) {
        //TODO: figure something out.
        tyche_suicide(100);
        return 0;
      }
      written += res;
    }
#endif
    return count;
}

ssize_t tyche_writev(int fd, const struct iovec *iov, int count) {
    ssize_t n = 0;
#ifndef TYCHE_NO_SYSCALL
    printf("Tyche writev: count %d\n ", count);
#endif
    for (int i = 0; i < count; i++) {
        n += tyche_write(fd, iov[i].iov_base, iov[i].iov_len);
    }
    return n;
}
void tyche_suicide(unsigned int v) {
  int* suicide = (int *) 0xdeadbabe;
  tyche_debug(v);
  *suicide = v;
}

// ——————————————————————————— Memory Management ———————————————————————————— //

#define PAGE_SIZE (0x1000)
#define NB_PAGES  (800 * 2)

#ifdef TYCHE_NO_SYSCALL
static char *mempool = (char*) 0x700000;
#else
static char mempool[NB_PAGES * PAGE_SIZE] __attribute__((aligned(0x1000))) = {0};
#endif
//TODO implement the bitmap.
//static uint64_t bitmap [NB_PAGES/64 + 1] = {0};
//For now let's just use a pointer.
static int mempool_next_free = 0;
static int mempool_is_init = 0;

static size_t align_page_up(size_t val) {
    return (val + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
}

static void *alloc_segment(size_t len) {
    void* res = NULL;
    int nb_pages = 0;
    // Align size to next page size multiple
    len = align_page_up(len);
    nb_pages = (len >> 12);
    if ((mempool_next_free + nb_pages) >= NB_PAGES) {
      // Running out of memory.
      tyche_suicide(0xb00b);
    }
    res = (void*) &mempool[(mempool_next_free) * PAGE_SIZE];
    mempool_next_free += nb_pages;
    return res;
}

void *tyche_mmap(void *start, size_t len, int prot, int flags, int fd, off_t off) {
    // We just ignore PROT_NONE as it is used only for guard pages
    if (prot == PROT_NONE) {
        return start;
    }

    // Print a warning if we are mapping a file, this is not supported!
    if (fd != -1) {
      tyche_suicide(0xB00B1);
      return NULL;
    }
    void* res = alloc_segment(len);
    return res;
}

int tyche_munmap(void *start, size_t len) { 
    //TODO implement.
    // TODO: insert a new node here.
    return 0;
}

#define BRK_NB_PAGES 20
static char brk_pool[BRK_NB_PAGES * PAGE_SIZE];
static char *brk_cursor;
static int brk_is_init = 0;

size_t tyche_brk(void *end) {
    // Initialize if needed
    if (!brk_is_init) {
        brk_cursor = brk_pool;
        brk_is_init = 1;
    }

    if (end == NULL) {
        return (size_t)brk_cursor;
    }

    if ((size_t)end > (size_t)brk_pool + BRK_NB_PAGES * PAGE_SIZE || (size_t)end < (size_t)brk_pool) {
        printf("Invalid brk!!!\n");
        return -ENOMEM;
    } else {
        brk_cursor = end;
        return (size_t)end;
    }
}
