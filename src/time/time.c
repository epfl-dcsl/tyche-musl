#include <time.h>
#include "syscall.h"

#include "../internal/tyche.h"

static unsigned long long tyche_monotonic = 0;

time_t time(time_t *t)
{
	struct timespec ts;
  ts.tv_sec = tyche_monotonic++;
  ts.tv_nsec = 0;
	if (t) *t = ts.tv_sec;
	return ts.tv_sec;
}
