#define _GNU_SOURCE
#include <dlfcn.h>
#include <math.h>
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

/* ── clock_gettime ───────────────────────────────────────────── */
int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    static struct timespec base_real = {0, 0};
    static struct timespec base_virtual = {0, 0};
    static double last_mult = 1.0;
    static int init = 0;

    if (!real_clock_gettime) openspeedy_init();
    if (!real_clock_gettime) return -1;

    int ret = real_clock_gettime(clk_id, tp);
    if (ret != 0) return ret;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0) {
        if (cached_multiplier == 1.0) init = 0;
        return ret;
    }

    /* Only scale monotonic clocks (not CLOCK_REALTIME) */
    if (clk_id == CLOCK_MONOTONIC || clk_id == CLOCK_MONOTONIC_RAW ||
        clk_id == CLOCK_MONOTONIC_COARSE) {

        if (!init) {
            base_real = *tp;
            base_virtual = *tp;
            last_mult = cached_multiplier;
            init = 1;
        }

        /* If multiplier changed, rebase to current real time */
        if (cached_multiplier != last_mult) {
            base_virtual = *tp;
            last_mult = cached_multiplier;
        }

        /* diff = real - base_real */
        struct timespec diff;
        diff.tv_sec  = tp->tv_sec  - base_real.tv_sec;
        diff.tv_nsec = tp->tv_nsec - base_real.tv_nsec;
        if (diff.tv_nsec < 0) {
            diff.tv_sec--;
            diff.tv_nsec += 1000000000L;
        }

        /* virtual = base_virtual + diff * multiplier */
        double total_ns = (double)diff.tv_sec * 1e9 + (double)diff.tv_nsec;
        total_ns *= cached_multiplier;

        tp->tv_sec  = base_virtual.tv_sec  + (time_t)(total_ns / 1e9);
        tp->tv_nsec = base_virtual.tv_nsec + (long)fmod(total_ns, 1e9);

        if (tp->tv_nsec >= 1000000000L) {
            tp->tv_sec++;
            tp->tv_nsec -= 1000000000L;
        }
    }

    return ret;
}

/* ── gettimeofday ────────────────────────────────────────────── */
int gettimeofday(struct timeval *tv, void *tz)
{
    if (!real_gettimeofday) openspeedy_init();
    if (!real_gettimeofday) return -1;

    int ret = real_gettimeofday(tv, tz);
    if (ret != 0) return ret;

    refresh_speed();
    /* gettimeofday is typically wall-clock time.
     * Most games use clock_gettime(CLOCK_MONOTONIC) instead.
     * We keep this as a pass-through for now. */

    return ret;
}

/* ── time ────────────────────────────────────────────────────── */
time_t time(time_t *t)
{
    if (!real_time) openspeedy_init();
    if (!real_time) return -1;

    time_t result = real_time(NULL);

    refresh_speed();
    /* Wall-clock: pass through. Games rarely use time() for timing. */

    if (t) *t = result;
    return result;
}

/* ── sleep ───────────────────────────────────────────────────── */
unsigned int sleep(unsigned int seconds)
{
    if (!real_sleep) openspeedy_init();
    if (!real_sleep) return seconds;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0)
        return real_sleep(seconds);

    double scaled = (double)seconds / cached_multiplier;
    unsigned int s = (unsigned int)scaled;
    double rem_ns = (scaled - (double)s) * 1e9;

    unsigned int remaining = real_sleep(s);
    if (remaining > 0)
        return (unsigned int)((double)remaining * cached_multiplier);

    if (rem_ns > 0 && real_nanosleep) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)rem_ns };
        real_nanosleep(&ts, NULL);
    }

    return 0;
}

/* ── usleep ──────────────────────────────────────────────────── */
int usleep(useconds_t usec)
{
    if (!real_usleep) openspeedy_init();
    if (!real_usleep) return -1;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0)
        return real_usleep(usec);

    double scaled = (double)usec / cached_multiplier;
    return real_usleep((useconds_t)scaled);
}

/* ── nanosleep ───────────────────────────────────────────────── */
int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!real_nanosleep) openspeedy_init();
    if (!real_nanosleep) return -1;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0)
        return real_nanosleep(req, rem);

    /* Scale sleep duration: shorter sleep for faster speed */
    double total_ns = (double)req->tv_sec * 1e9 + (double)req->tv_nsec;
    total_ns /= cached_multiplier;

    struct timespec scaled;
    scaled.tv_sec  = (time_t)(total_ns / 1e9);
    scaled.tv_nsec = (long)(total_ns - (double)scaled.tv_sec * 1e9);

    return real_nanosleep(&scaled, rem);
}
