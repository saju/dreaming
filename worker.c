/*
 * threads 
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <mach/mach_traps.h>
#include <pthread.h>
#include "dreaming.h"

static pthread_t *threads = NULL;
static int ncpu = 0;

int init_workers() {
  int mib[] = {CTL_HW, HW_NCPU};
  size_t len = sizeof(int);

  sysctl(mib, 2, &ncpu, &len, NULL, 0);

  threads = malloc(sizeof(pthread_t) * ncpu);
  return ncpu;
}

void worker_run(void *(*work)(void *), void *args) {
  int i;
  worker_ctx_t ctx[ncpu];

  for (i = 0; i < ncpu; i++) {
    ctx[i].worker = i;
    ctx[i].args = args;

    pthread_create(&threads[i], NULL, work, &ctx[i]);
  }

  for (i = 0; i < ncpu; i++) {
    pthread_join(threads[i], NULL);
  }
}

