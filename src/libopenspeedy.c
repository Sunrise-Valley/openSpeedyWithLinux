#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "openspeedy.h"

/* ── Original function pointers ──────────────────────────────── */
static clock_gettime_t  real_clock_gettime  = NULL;
static gettimeofday_t   real_gettimeofday   = NULL;
static time_t_fn        real_time           = NULL;
static sleep_t          real_sleep          = NULL;
static usleep_t         real_usleep         = NULL;
static nanosleep_t      real_nanosleep      = NULL;
static poll_t           real_poll           = NULL;
static select_t         real_select         = NULL;
static clock_getres_t   real_clock_getres   = NULL;

/* ── Cached speed state ─────────────────────────────────────── */
static double   cached_multiplier = 1.0;
static int      cached_enabled    = 0;
static uint64_t cached_seq        = 0;

/* ── Initialize: load original function pointers (called once) ─ */
__attribute__((constructor))
static void openspeedy_init(void)
{
    real_clock_gettime = (clock_gettime_t)dlsym(RTLD_NEXT, "clock_gettime");
    real_gettimeofday  = (gettimeofday_t)dlsym(RTLD_NEXT, "gettimeofday");
    real_time          = (time_t_fn)dlsym(RTLD_NEXT, "time");
    real_sleep         = (sleep_t)dlsym(RTLD_NEXT, "sleep");
    real_usleep        = (usleep_t)dlsym(RTLD_NEXT, "usleep");
    real_nanosleep     = (nanosleep_t)dlsym(RTLD_NEXT, "nanosleep");
    real_poll          = (poll_t)dlsym(RTLD_NEXT, "poll");
    real_select        = (select_t)dlsym(RTLD_NEXT, "select");
    real_clock_getres  = (clock_getres_t)dlsym(RTLD_NEXT, "clock_getres");
}

/* ── Read shared memory, update cache if cmd_seq changed ─────── */
int openspeedy_get_state(struct speed_state *state)
{
    int fd;
    struct speed_state *shm;

    fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (fd == -1)
        return -1;  /* Not yet created by speedctl — normal speed */

    shm = mmap(NULL, sizeof(struct speed_state), PROT_READ,
               MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
        close(fd);
        return -1;
    }

    memcpy(state, shm, sizeof(struct speed_state));
    munmap(shm, sizeof(struct speed_state));
    close(fd);
    return 0;
}

/* ── Check shared memory and update cached multiplier ────────── */
static void refresh_speed(void)
{
    struct speed_state state;

    if (openspeedy_get_state(&state) != 0) {
        cached_enabled = 0;
        cached_multiplier = 1.0;
        return;
    }

    if (state.cmd_seq != cached_seq) {
        cached_seq = state.cmd_seq;
        cached_enabled = state.enabled;
        cached_multiplier = state.multiplier;
        if (cached_multiplier <= 0.0)
            cached_multiplier = 1.0;
    }
}

/* ── Scale a timespec by multiplier (divide by mult) ──────────── */
void openspeedy_scale_timespec(const struct timespec *in,
                               struct timespec *out, double mult)
{
    double total_ns = (double)in->tv_sec * 1e9 + (double)in->tv_nsec;
    total_ns /= mult;
    out->tv_sec  = (time_t)(total_ns / 1e9);
    out->tv_nsec = (long)(total_ns - (double)out->tv_sec * 1e9);
    if (out->tv_nsec < 0) {
        out->tv_sec--;
        out->tv_nsec += 1000000000L;
    }
}
