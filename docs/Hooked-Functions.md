# Hooked Functions

本文档详细说明 OpenSpeedy Linux 拦截的每个函数及其缩放策略。

## 与原版 OpenSpeedy (Windows) 的对应关系

| Windows 原版 | Linux 版 | 说明 |
|---|---|---|
| `Sleep` (user32.dll) | `sleep`, `usleep`, `nanosleep` | 线程睡眠 |
| `SetTimer` (user32.dll) | - (未实现) | Windows 消息定时器，Linux 无对应 |
| `timeGetTime` (winmm.dll) | `clock_gettime(CLOCK_MONOTONIC)` | 获取系统启动后毫秒数 |
| `GetTickCount` (kernel32.dll) | `clock_gettime(CLOCK_MONOTONIC)` | 获取系统启动后毫秒数 |
| `GetTickCount64` (kernel32.dll) | `clock_gettime(CLOCK_MONOTONIC)` | 64位版本 |
| `QueryPerformanceCounter` (kernel32.dll) | `clock_gettime(CLOCK_MONOTONIC)` | 高精度性能计数器 |
| `GetSystemTimeAsFileTime` (kernel32.dll) | `gettimeofday` / `clock_gettime(CLOCK_REALTIME)` | 系统墙钟时间（不缩放） |
| `GetSystemTimePreciseAsFileTime` (kernel32.dll) | `clock_gettime(CLOCK_REALTIME)` | 高精度墙钟（不缩放） |

## 详细缩放策略

### 1. `clock_gettime` — 核心时间函数

```c
int clock_gettime(clockid_t clk_id, struct timespec *tp);
```

**策略**: 仅缩放 monotonic 时钟，不动 realtime。

| clock_id | 行为 |
|---|---|
| `CLOCK_MONOTONIC` | 缩放：`virtual = base + (real - base_real) × multiplier` |
| `CLOCK_MONOTONIC_RAW` | 缩放（同上） |
| `CLOCK_MONOTONIC_COARSE` | 缩放（同上） |
| `CLOCK_REALTIME` | 透传（不缩放） |
| `CLOCK_REALTIME_COARSE` | 透传 |
| `CLOCK_PROCESS_CPUTIME_ID` | 透传 |
| `CLOCK_THREAD_CPUTIME_ID` | 透传 |

**为什么只缩放 MONOTONIC？**
- 游戏引擎用 `CLOCK_MONOTONIC` 计算帧间时间差（delta time）
- `CLOCK_REALTIME` 是墙钟时间，缩放会导致日志时间戳错误、TLS 证书验证失败等问题
- 游戏加速的核心是让游戏"以为"更多时间过去了

**倍率变化时的重定基**: 当用户改变倍率时，`base_virtual` 重置为当前虚拟时间，确保不会出现时间跳跃。

### 2. `sleep` — 秒级睡眠

```c
unsigned int sleep(unsigned int seconds);
```

**策略**: `scaled = seconds / multiplier`，按比例缩短实际睡眠时间。

示例：
- `sleep(2)` @ 2x → 实际睡眠 1 秒
- `sleep(1)` @ 3x → 实际睡眠 ~0.33 秒
- `sleep(10)` @ 0.5x → 实际睡眠 20 秒（减速）

如果被信号中断，剩余时间会按倍率放大后返回给调用者。

### 3. `usleep` — 微秒级睡眠

```c
int usleep(useconds_t usec);
```

**策略**: 同 sleep，`scaled = usec / multiplier`。

### 4. `nanosleep` — 纳秒级高精度睡眠

```c
int nanosleep(const struct timespec *req, struct timespec *rem);
```

**策略**: 将 `req` 中的时间除以 multiplier，传入真实 nanosleep。

### 5. `poll` — I/O 多路复用

```c
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

**策略**: `scaled_timeout = timeout / multiplier`。

游戏常用 poll/select 实现帧率限制（如 `poll(..., 16)` 实现约 60fps）。
2x 加速时 timeout 变为 8ms → 约 120fps。

最小 timeout 为 1ms（避免变成忙等）。

### 6. `select` — I/O 多路复用

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```

**策略**: 缩放 `timeout` 中的秒和微秒字段。

### 7. `gettimeofday` — 透传

**策略**: 不缩放。这是墙钟时间，游戏很少用它做帧计时。

### 8. `time` — 透传

**策略**: 不缩放。`time()` 返回 Unix 时间戳，缩放无意义。

## 未实现但可扩展的函数

以下函数当前未拦截，但未来可能需要：

| 函数 | 用途 | 优先级 |
|---|---|---|
| `clock_nanosleep` | 高精度睡眠 + 时钟选择 | 中 |
| `timerfd_create/settime` | 定时器文件描述符 | 中 |
| `pthread_cond_timedwait` | 条件变量超时等待 | 中 |
| `sem_timedwait` | 信号量超时等待 | 低 |
| `epoll_wait` | epoll 超时等待 | 低 |
| `alarm` / `setitimer` | 传统 Unix 定时器 | 低 |
| `SDL_GetTicks` / `glfwGetTime` | 游戏框架时间函数 | 低（需要单独 hook） |

## 局限性

1. **静态链接的游戏**: 如果游戏静态链接了 libc，LD_PRELOAD 无法拦截（`dlsym(RTLD_NEXT)` 找不到符号）
2. **直接 syscall 的游戏**: 如果游戏绕过 libc 直接调用 `clock_gettime` syscall，无法拦截
3. **自定义计时器**: 部分游戏引擎使用自己的高精度计时器（如读取 TSC 寄存器）
4. **反作弊系统**: EAC、BattlEye 等会检测 LD_PRELOAD
