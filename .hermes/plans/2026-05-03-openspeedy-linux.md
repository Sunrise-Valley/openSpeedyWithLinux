# OpenSpeedy Linux 实现计划

> **For Hermes:** 逐任务实现，每完成一个 task 提交一次。

**目标:** 在 Linux 上实现 OpenSpeedy 的游戏变速功能 —— 通过 LD_PRELOAD 拦截时间函数，共享内存传递倍率，CLI 工具控制。

**架构:** 纯 C 实现。核心是一个被 LD_PRELOAD 注入的共享库 `libopenspeedy.so`，它拦截 `clock_gettime`、`gettimeofday`、`sleep` 族等时间相关函数，按共享内存中的倍率缩放时间。控制端 `speedctl` 通过写入共享内存来动态调整倍率。

**技术栈:** C11, GNU Make, POSIX shared memory (shm_open/mmap), dlsym(RTLD_NEXT)

**与原版 OpenSpeedy 的对应关系:**

| 原版 (Windows) | Linux 版 |
|---|---|
| speedpatch/ (MinHook DLL) | libopenspeedy.so (LD_PRELOAD) |
| bridge/ (IPC) | POSIX shared memory (/dev/shm/openspeedy) |
| Qt GUI | speedctl CLI (先做 CLI，GUI 后续) |
| Sleep/SetTimer/timeGetTime/GetTickCount/QueryPerformanceCounter/GetSystemTime... | clock_gettime/gettimeofday/time/sleep/nanosleep/poll/select... |

---

## 目录结构

```
openSpeedy_linux/
├── Makefile
├── README.md
├── .gitignore
├── .hermes/
│   └── plans/
│       └── 2026-05-03-openspeedy-linux.md
├── include/
│   └── openspeedy.h          # 公共头文件：共享内存协议、常量
├── src/
│   ├── libopenspeedy.c       # LD_PRELOAD 拦截库
│   └── speedctl.c            # CLI 控制工具
├── tests/
│   ├── test_sleep.c          # 测试 sleep 加速
│   └── test_time.c           # 测试 gettimeofday/clock_gettime 加速
└── docs/
    └── Hooked-Functions.md   # 拦截函数清单及原理
```

---

### Task 1: 初始化项目结构和 Git 仓库

**目标:** 创建项目骨架、Makefile 框架、.gitignore

**文件:**
- 创建: `Makefile`
- 创建: `.gitignore`
- 创建: `include/openspeedy.h` (骨架)
- 创建: `README.md` (骨架)

**步骤:**

**Step 1: 创建 .gitignore**

```makefile
*.o
*.so
*.a
build/
*.swp
*.swo
*~
```

**Step 2: 创建 Makefile 框架**

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c11
LDFLAGS = -lrt -ldl -lpthread

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests

.PHONY: all clean test

all: $(BUILD_DIR)/libopenspeedy.so $(BUILD_DIR)/speedctl

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# libopenspeedy.so — LD_PRELOAD library
$(BUILD_DIR)/libopenspeedy.so: $(SRC_DIR)/libopenspeedy.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $< -I$(INC_DIR) $(LDFLAGS)

# speedctl — CLI controller
$(BUILD_DIR)/speedctl: $(SRC_DIR)/speedctl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -I$(INC_DIR) -lrt

test: all
	@echo "Running tests..."
	@# bash tests/run_tests.sh

clean:
	rm -rf $(BUILD_DIR)
```

**Step 3: 创建 include/openspeedy.h 骨架**

```c
#ifndef OPENSPEEDY_H
#define OPENSPEEDY_H

/* Shared memory protocol between speedctl and libopenspeedy.so */

#define SHM_NAME "/openspeedy_shm"

/* Speed state stored in shared memory */
struct speed_state {
    double multiplier;   /* 1.0 = normal, 2.0 = 2x speed, 0.5 = half speed */
    volatile int enabled; /* 1 = speed mod active, 0 = bypass */
};

#endif /* OPENSPEEDY_H */
```

**Step 4: 初始化 git 仓库**

```bash
cd /home/clawbot/workspace/openSpeedy_linux
git init
git add -A
git commit -m "chore: init project structure"
```

---

### Task 2: 实现公共头文件和共享内存协议

**目标:** 完善 `include/openspeedy.h`，定义完整的共享内存协议和拦截函数签名

**文件:**
- 修改: `include/openspeedy.h`

**步骤:**

**Step 1: 编写完整 openspeedy.h**

```c
#ifndef OPENSPEEDY_H
#define OPENSPEEDY_H

#include <stdint.h>
#include <time.h>

/* ── Shared Memory Protocol ─────────────────────────────────── */

#define SHM_NAME "/openspeedy_shm"
#define SHM_SIZE 4096

struct speed_state {
    double   multiplier;   /* Speed multiplier: 1.0 = normal */
    int      enabled;      /* 1 = active, 0 = bypass */
    uint64_t cmd_seq;      /* Incremented on every command, triggers reload */
    uint32_t version;      /* Protocol version */
    pid_t    controller_pid; /* PID of the controlling process */
};

/* ── Function pointer types for intercepted functions ────────── */

typedef int (*clock_gettime_t)(clockid_t clk_id, struct timespec *tp);
typedef int (*gettimeofday_t)(struct timeval *tv, struct timezone *tz);
typedef time_t (*time_t)(time_t *t);
typedef unsigned int (*sleep_t)(unsigned int seconds);
typedef int (*usleep_t)(useconds_t usec);
typedef int (*nanosleep_t)(const struct timespec *req, struct timespec *rem);
typedef int (*poll_t)(struct pollfd *fds, nfds_t nfds, int timeout);
typedef int (*select_t)(int nfds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds, struct timeval *timeout);
typedef int (*clock_getres_t)(clockid_t clk_id, struct timespec *res);
/* CLOCK_REALTIME variants for new glibc */
typedef int (*clock_gettime64_t)(clockid_t clk_id, struct timespec *tp);

/* ── Internal helpers (declared, defined in libopenspeedy.c) ── */

int openspeedy_get_state(struct speed_state *state);
void openspeedy_get_original_funcs(void);
void openspeedy_scale_timespec(const struct timespec *in,
                               struct timespec *out, double mult);

#endif /* OPENSPEEDY_H */
```

**Step 2: 提交**

```bash
git add include/openspeedy.h
git commit -m "feat: shared memory protocol and function types"
```

---

### Task 3: 实现 libopenspeedy.so 核心 —— dlsym 加载和状态读取

**目标:** 实现库初始化逻辑，包括获取原始函数指针、从共享内存读取状态

**文件:**
- 创建: `src/libopenspeedy.c`

**步骤:**

**Step 1: 编写 libopenspeedy.c 框架（约 100 行）**

```c
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
static time_t           real_time           = NULL;
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
    real_time          = (time_t)dlsym(RTLD_NEXT, "time");
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

/* ── Scale a timespec by multiplier ──────────────────────────── */
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
```

**Step 2: 编译验证**

```bash
cd /home/clawbot/workspace/openSpeedy_linux
make
```

**Step 3: 提交**

```bash
git add src/libopenspeedy.c
git commit -m "feat: libopenspeedy init, shm reader, timespec scaling"
```

---

### Task 4: 实现时间查询函数的拦截 (clock_gettime, gettimeofday, time)

**目标:** 拦截时间查询函数，按倍率缩放返回值

**文件:**
- 修改: `src/libopenspeedy.c` (追加到文件末尾)

**步骤:**

在 `libopenspeedy.c` 末尾添加以下函数。每个函数逻辑：
1. 调用原始函数获取真实时间
2. 检查是否启用变速
3. 如果启用，缩放时间值（用 openspeedy_scale_timespec）
4. 返回

关键点：对于 `clock_gettime(CLOCK_MONOTONIC, ...)`，需要记录基准偏移量，以便后续调用返回一致的时间（单调递增）。

```c
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
        /* Reset baseline when speed returns to normal */
        if (cached_multiplier == 1.0) init = 0;
        return ret;
    }

    /* Only scale monotonic clocks (not CLOCK_REALTIME for correct wall time) */
    if (clk_id == CLOCK_MONOTONIC || clk_id == CLOCK_MONOTONIC_RAW ||
        clk_id == CLOCK_MONOTONIC_COARSE) {

        if (!init) {
            base_real = *tp;
            base_virtual = *tp;
            last_mult = cached_multiplier;
            init = 1;
        }

        /* If multiplier changed, rebase */
        if (cached_multiplier != last_mult) {
            base_virtual = *tp;
            base_real = *tp;
            last_mult = cached_multiplier;
        }

        /* virtual_time = base_virtual + (real_time - base_real) * multiplier */
        struct timespec diff;
        diff.tv_sec  = tp->tv_sec  - base_real.tv_sec;
        diff.tv_nsec = tp->tv_nsec - base_real.tv_nsec;
        if (diff.tv_nsec < 0) {
            diff.tv_sec--;
            diff.tv_nsec += 1000000000L;
        }

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
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    if (!real_gettimeofday) openspeedy_init();
    if (!real_gettimeofday) return -1;

    int ret = real_gettimeofday(tv, tz);
    if (ret != 0) return ret;

    refresh_speed();
    /* gettimeofday is typically used as wall-clock. We don't scale it
     * unless user explicitly wants to. Scaling CLOCK_MONOTONIC is
     * usually sufficient for games. */

    return ret;
}

/* ── time ────────────────────────────────────────────────────── */
time_t time(time_t *t)
{
    if (!real_time) openspeedy_init();
    if (!real_time) return -1;

    time_t result = real_time(NULL);

    refresh_speed();
    /* Wall-clock time: don't scale. Games rarely use time() for timing. */

    if (t) *t = result;
    return result;
}
```

**验证:**

```bash
make
# 手动验证：LD_PRELOAD 后 clock_gettime 是否正常返回
```

**提交:**

```bash
git add src/libopenspeedy.c
git commit -m "feat: intercept clock_gettime with monotonic time scaling"
```

---

### Task 5: 实现 sleep 族函数拦截 (sleep, usleep, nanosleep)

**目标:** 拦截 sleep 函数，按倍率缩短睡眠时间（2x 速度 → 睡眠时间减半）

**文件:**
- 修改: `src/libopenspeedy.c`

**步骤:**

在文件末尾添加：

```c
/* ── sleep ───────────────────────────────────────────────────── */
unsigned int sleep(unsigned int seconds)
{
    if (!real_sleep) openspeedy_init();
    if (!real_sleep) return seconds;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0) {
        return real_sleep(seconds);
    }

    /* Scale down sleep time */
    double scaled = (double)seconds / cached_multiplier;
    unsigned int s = (unsigned int)scaled;
    double rem_ns = (scaled - (double)s) * 1e9;

    unsigned int remaining = real_sleep(s);
    if (remaining > 0) {
        /* We were interrupted — scale remaining back up */
        return (unsigned int)((double)remaining * cached_multiplier);
    }

    /* Handle fractional remainder with nanosleep */
    if (rem_ns > 0 && real_nanosleep) {
        struct timespec ts = {
            .tv_sec = 0,
            .tv_nsec = (long)rem_ns
        };
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

    if (!cached_enabled || cached_multiplier == 1.0) {
        return real_usleep(usec);
    }

    /* Scale: shorter sleep for faster speed */
    double scaled = (double)usec / cached_multiplier;
    return real_usleep((useconds_t)scaled);
}

/* ── nanosleep ───────────────────────────────────────────────── */
int nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!real_nanosleep) openspeedy_init();
    if (!real_nanosleep) return -1;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0) {
        return real_nanosleep(req, rem);
    }

    /* Scale the sleep duration */
    struct timespec scaled;
    openspeedy_scale_timespec(req, &scaled, 1.0 / cached_multiplier);
    /* Actually: shorter sleep = divide duration by multiplier */
    /* Wait, openspeedy_scale_timespec divides by mult. We want:
     *   scaled = req / multiplier  (to sleep shorter for speedup)
     * openspeedy_scale_timespec(in, out, mult) does: out = in / mult
     * So we pass cached_multiplier directly. Wait no, it divides.
     * Let's just recompute:
     */
    double total_ns = (double)req->tv_sec * 1e9 + (double)req->tv_nsec;
    total_ns /= cached_multiplier;
    scaled.tv_sec  = (time_t)(total_ns / 1e9);
    scaled.tv_nsec = (long)(total_ns - (double)scaled.tv_sec * 1e9);

    int ret = real_nanosleep(&scaled, rem);

    /* If interrupted, scale remaining back up */
    if (ret == -1 && rem && rem->tv_sec == 0 && rem->tv_nsec == 0) {
        /* Actually real_nanosleep handles rem itself */
    }

    return ret;
}
```

**验证编译:**

```bash
make
```

**提交:**

```bash
git add src/libopenspeedy.c
git commit -m "feat: intercept sleep/usleep/nanosleep with speed scaling"
```

---

### Task 6: 实现 poll/select 拦截

**目标:** 拦截 poll/select 的超时参数，按倍率缩放

**文件:**
- 修改: `src/libopenspeedy.c`

**步骤:**

```c
/* ── poll ────────────────────────────────────────────────────── */
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    if (!real_poll) openspeedy_init();
    if (!real_poll) return -1;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0 || timeout <= 0) {
        return real_poll(fds, nfds, timeout);
    }

    /* Scale timeout: shorter wait for faster speed */
    int scaled_timeout = (int)((double)timeout / cached_multiplier);
    if (scaled_timeout < 1 && timeout > 0)
        scaled_timeout = 1;

    return real_poll(fds, nfds, scaled_timeout);
}

/* ── select ──────────────────────────────────────────────────── */
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout)
{
    if (!real_select) openspeedy_init();
    if (!real_select) return -1;

    refresh_speed();

    if (!cached_enabled || cached_multiplier == 1.0 || !timeout) {
        return real_select(nfds, readfds, writefds, exceptfds, timeout);
    }

    /* Scale timeout */
    struct timeval scaled = *timeout;
    double total_us = (double)scaled.tv_sec * 1e6 + (double)scaled.tv_usec;
    total_us /= cached_multiplier;
    scaled.tv_sec  = (time_t)(total_us / 1e6);
    scaled.tv_usec = (suseconds_t)(total_us - (double)scaled.tv_sec * 1e6);

    return real_select(nfds, readfds, writefds, exceptfds, &scaled);
}
```

**验证编译:**

```bash
make
```

**提交:**

```bash
git add src/libopenspeedy.c
git commit -m "feat: intercept poll/select timeout scaling"
```

---

### Task 7: 实现 speedctl CLI 控制工具

**目标:** 实现控制端 CLI，用于创建共享内存、设置/查询速度倍率

**文件:**
- 创建: `src/speedctl.c`

**步骤:**

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "openspeedy.h"

static struct speed_state *shm = NULL;
static int shm_fd = -1;

/* Create or open shared memory */
static int shm_init(void)
{
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        return -1;
    }

    shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    /* Init if first time */
    if (shm->version == 0) {
        shm->version = 1;
        shm->multiplier = 1.0;
        shm->enabled = 0;
        shm->cmd_seq = 0;
        shm->controller_pid = getpid();
    }

    return 0;
}

static void shm_cleanup(void)
{
    if (shm && shm != MAP_FAILED)
        munmap(shm, SHM_SIZE);
    if (shm_fd != -1)
        close(shm_fd);
    /* Don't unlink — target processes may still be reading */
}

static void send_cmd(double multiplier, int enabled)
{
    shm->multiplier = multiplier;
    shm->enabled = enabled;
    shm->cmd_seq++;
    shm->controller_pid = getpid();
}

static void cmd_status(void)
{
    if (shm_init() != 0) {
        printf("No active speed modifier\n");
        return;
    }
    printf("Multiplier: %.2fx\n", shm->multiplier);
    printf("Status:     %s\n", shm->enabled ? "ACTIVE" : "inactive");
    printf("CmdSeq:     %lu\n", (unsigned long)shm->cmd_seq);
    shm_cleanup();
}

static void cmd_set(const char *arg)
{
    double mult = atof(arg);
    if (mult <= 0.0) {
        fprintf(stderr, "Invalid multiplier: %s (must be > 0)\n", arg);
        exit(1);
    }
    if (shm_init() != 0) exit(1);
    send_cmd(mult, (mult != 1.0) ? 1 : 0);
    printf("Speed set to %.2fx\n", mult);
    shm_cleanup();
}

static void cmd_reset(void)
{
    if (shm_init() != 0) exit(1);
    send_cmd(1.0, 0);
    printf("Speed reset to 1.0x\n");
    shm_cleanup();
}

static void cmd_off(void)
{
    if (shm_init() != 0) exit(1);
    send_cmd(shm->multiplier, 0);
    printf("Speed modifier disabled (multiplier kept at %.2fx)\n",
           shm->multiplier);
    shm_cleanup();
}

/* Usage: speedctl [command] [args] */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr,
            "Usage: speedctl <command> [args]\n"
            "Commands:\n"
            "  status         Show current speed multiplier\n"
            "  set <N>        Set speed multiplier (e.g. 2.0 = 2x)\n"
            "  reset          Reset to 1.0x\n"
            "  off            Disable speed modifier\n"
            "  run <cmd...>   Run command with LD_PRELOAD set\n"
        );
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        cmd_status();
    } else if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: speedctl set <multiplier>\n"); return 1; }
        cmd_set(argv[2]);
    } else if (strcmp(argv[1], "reset") == 0) {
        cmd_reset();
    } else if (strcmp(argv[1], "off") == 0) {
        cmd_off();
    } else if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: speedctl run <program> [args...]\n"); return 1; }
        /* Set LD_PRELOAD and exec */
        setenv("LD_PRELOAD", "build/libopenspeedy.so", 1);
        execvp(argv[2], &argv[2]);
        perror("execvp");
        return 1;
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
```

**验证编译:**

```bash
make
./build/speedctl status
```

**提交:**

```bash
git add src/speedctl.c
git commit -m "feat: speedctl CLI — set/status/reset/off/run"
```

---

### Task 8: 编写测试程序 test_sleep.c

**目标:** 编写一个简单的测试程序，验证 sleep 变速效果

**文件:**
- 创建: `tests/test_sleep.c`

**步骤:**

```c
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main()
{
    struct timespec start, end;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Should take ~2 seconds, but with 2x speed = ~1 second */
    printf("Sleeping for 2 seconds...\n");
    sleep(2);

    clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed = (end.tv_sec - start.tv_sec) +
              (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Elapsed (real): %.3f seconds\n", elapsed);
    printf("Expected with 2x speed: ~1.0s\n");
    printf("Expected without speed:  ~2.0s\n");

    return 0;
}
```

**更新 Makefile 添加 test_sleep 编译目标:**

```makefile
$(BUILD_DIR)/test_sleep: $(TEST_DIR)/test_sleep.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -I$(INC_DIR)
```

**验证:**

```bash
make
# 普通运行：
./build/test_sleep
# 2倍速运行：
./build/speedctl set 2.0
LD_PRELOAD=./build/libopenspeedy.so ./build/test_sleep
```

**提交:**

```bash
git add tests/test_sleep.c Makefile
git commit -m "test: sleep speed test program"
```

---

### Task 9: 编写测试程序 test_time.c (验证 clock_gettime 缩放)

**目标:** 编写测试程序，验证 clock_gettime(CLOCK_MONOTONIC) 的时间缩放

**文件:**
- 创建: `tests/test_time.c`

**步骤:**

```c
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main()
{
    struct timespec t1, t2, t3, t4;
    double elapsed_real, elapsed_mono;

    /* Measure 1 second of real sleep */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    clock_gettime(CLOCK_REALTIME, &t3);
    sleep(1);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    clock_gettime(CLOCK_REALTIME, &t4);

    elapsed_mono = (t2.tv_sec - t1.tv_sec) +
                   (t2.tv_nsec - t1.tv_nsec) / 1e9;
    elapsed_real = (t4.tv_sec - t3.tv_sec) +
                   (t4.tv_nsec - t3.tv_nsec) / 1e9;

    printf("CLOCK_MONOTONIC elapsed: %.3f seconds\n", elapsed_mono);
    printf("CLOCK_REALTIME elapsed:  %.3f seconds\n", elapsed_real);
    printf("With 2x speed, MONOTONIC should be ~2.0s, REALTIME ~1.0s\n");

    return 0;
}
```

**更新 Makefile 添加 test_time:**

```makefile
$(BUILD_DIR)/test_time: $(TEST_DIR)/test_time.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -I$(INC_DIR)
```

**验证编译:**

```bash
make
```

**提交:**

```bash
git add tests/test_time.c Makefile
git commit -m "test: clock_gettime speed verification"
```

---

### Task 10: 端到端集成测试 & README

**目标:** 完整测试流程，编写 README

**文件:**
- 修改: `README.md`
- 创建: `docs/Hooked-Functions.md`

**测试流程:**

```bash
# Terminal 1: 启动 speedctl，设置 3x 速度
./build/speedctl set 3.0

# Terminal 2: 运行测试程序（带 LD_PRELOAD）
LD_PRELOAD=./build/libopenspeedy.so ./build/test_sleep
# 预期: sleep(2) 实际只睡 ~0.67 秒

# Terminal 3: 验证 clock_gettime 缩放
./build/speedctl set 2.0
LD_PRELOAD=./build/libopenspeedy.so ./build/test_time
# 预期: CLOCK_MONOTONIC 显示 ~2.0s，CLOCK_REALTIME 显示 ~1.0s

# 禁用变速
./build/speedctl off

# 测试 run 子命令
./build/speedctl set 4.0
./build/speedctl run ./build/test_sleep
# 等同于: LD_PRELOAD=... ./build/test_sleep
```

**README.md 内容:** 项目介绍、安装方法（`make`）、使用说明、技术原理（LD_PRELOAD + shm）、拦截函数列表、注意事项

**docs/Hooked-Functions.md:** 详细列出拦截的函数、每个函数的缩放策略、与 Windows 原版的对应关系

**提交:**

```bash
git add README.md docs/Hooked-Functions.md
git commit -m "docs: README and hooked functions documentation"
```

---

## 后续扩展 (不在本计划范围内)

- Qt GUI 控制面板
- 进程列表显示（类似原版的进程选择）
- 热键支持（快速切换速度）
- 配置文件持久化
- systemd 用户服务（开机自启共享内存）
- 更多函数拦截：`timerfd_*`, `pthread_cond_timedwait`, `sem_timedwait`, `epoll_wait`, `sigevent` 定时器
