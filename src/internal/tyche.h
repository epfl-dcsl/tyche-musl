// Interface for Tyche specific functions

#ifndef	_TYCHE_H
#define	_TYCHE_H	1

#include "sys/select.h"

#define TYCHE_SOCKET_FD 14
#define TYCHE_CONNECTION_FD 15

void tyche_debug();
int tyche_socket();
int tyche_setsockopt(int fd);
int tyche_bind(int fd);
int tyche_listen(int fd);
int tyche_accept(int fd);
int tyche_fcntl(int fd, int flags);
int tyche_select(int n, fd_set *restrict rfds, fd_set *restrict wfds);
ssize_t tyche_read(int fd, void *buff, size_t count);
ssize_t tyche_write(int fd, const void *buf, size_t count);

#endif
