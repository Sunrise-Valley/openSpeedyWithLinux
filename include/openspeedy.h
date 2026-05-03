#ifndef OPENSPEEDY_H
#define OPENSPEEDY_H

#include <stdint.h>
#include <time.h>

/* ── Shared Memory Protocol ─────────────────────────────────── */

#define SHM_NAME "/openspeedy_shm"
#define SHM_SIZE 4096

struct speed_state {
    double   multiplier;      /* Speed multiplier: 1.0 = normal */
    int      enabled;         /* 1 = active, 0 = bypass */
    uint64_t cmd_seq;         /* Incremented on every command, triggers reload */
    uint32_t version;         /* Protocol version */
    pid_t    controller_pid;  /* PID of the controlling process */
};

/* ── Function pointer types for intercepted functions ────────── */

typedef int (*clock_gettime_t)(clockid_t clk_id, struct timespec *tp);
typedef int (*gettimeofday_t)(struct timeval *tv, struct timezone *tz);
typedef time_t (*time_t_fn)(time_t *t);
typedef unsigned int (*sleep_t)(unsigned int seconds);
typedef int (*usleep_t)(useconds_t usec);
typedef int (*nanosleep_t)(const struct timespec *req, struct timespec *rem);
typedef int (*poll_t)(struct pollfd *fds, nfds_t nfds, int timeout);
typedef int (*select_t)(int nfds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds, struct timeval *timeout);
typedef int (*clock_getres_t)(clockid_t clk_id, struct timespec *res);

/* ── Internal helpers (defined in libopenspeedy.c) ───────────── */

int openspeedy_get_state(struct speed_state *state);
void openspeedy_get_original_funcs(void);
void openspeedy_scale_timespec(const struct timespec *in,
                               struct timespec *out, double mult);

#endif /* OPENSPEEDY_H */
