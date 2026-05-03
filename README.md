# OpenSpeedy Linux

OpenSpeedy 的 Linux 移植版 —— 开源免费的游戏变速工具。

通过 LD_PRELOAD 机制拦截时间相关函数，按倍率缩放游戏内的时间感知，实现加速/减速效果。

与原版 [OpenSpeedy](https://github.com/game1024/OpenSpeedy) (Windows) 功能对等：
- Windows 版通过 MinHook 注入 DLL Hook 8 个 Win32 时间函数
- Linux 版通过 LD_PRELOAD 拦截 POSIX 时间函数

## 原理

```
┌─────────────┐     ┌──────────────────┐     ┌──────────────┐
│  speedctl   │────▶│  共享内存 (SHM)   │◀────│  目标进程     │
│  (控制器)    │     │  multiplier/enable│     │  LD_PRELOAD  │
└─────────────┘     └──────────────────┘     │ libopenspeedy│
                                             │ 拦截时间函数  │
                                             └──────────────┘
```

- **共享内存**: `/dev/shm/openspeedy` (POSIX shm_open)
- **拦截方式**: `dlsym(RTLD_NEXT)` 获取原始函数指针
- **时间缩放**: 仅缩放 `CLOCK_MONOTONIC`，不动 `CLOCK_REALTIME`

## 安装

```bash
git clone git@github.com:Sunrise-Valley/openSpeedyWithLinux.git
cd openSpeedyWithLinux
make
```

依赖: `gcc`, `make`, glibc (需要 `librt`, `libdl`, `libpthread`)

## 使用

```bash
# 构建
make

# 查看状态
./build/speedctl status

# 设置 2 倍速
./build/speedctl set 2.0

# 对目标程序应用变速
LD_PRELOAD=./build/libopenspeedy.so ./your_game

# 或使用 run 子命令（自动设置 LD_PRELOAD）
./build/speedctl run ./your_game

# 禁用变速
./build/speedctl off

# 恢复原速
./build/speedctl reset

# 运行测试
make test
```

## 拦截函数

| 函数 | 策略 |
|---|---|
| `clock_gettime(CLOCK_MONOTONIC*)` | 按倍率缩放返回值 |
| `clock_gettime(CLOCK_REALTIME)` | 透传（不缩放墙钟） |
| `gettimeofday` | 透传 |
| `time` | 透传 |
| `sleep` | 按倍率缩短睡眠时间 |
| `usleep` | 按倍率缩短睡眠时间 |
| `nanosleep` | 按倍率缩短睡眠时间 |
| `poll` | 按倍率缩短 timeout |
| `select` | 按倍率缩短 timeout |

详见 [docs/Hooked-Functions.md](docs/Hooked-Functions.md)

## 测试

```bash
# 无变速：sleep(2) = 2s
./build/test_sleep

# 2倍速：sleep(2) 实际睡 1s，clock_gettime 报告 2s
./build/speedctl set 2.0
LD_PRELOAD=./build/libopenspeedy.so ./build/test_sleep

# 验证 MONOTONIC vs REALTIME 行为
LD_PRELOAD=./build/libopenspeedy.so ./build/test_time
./build/speedctl reset
```

## 注意事项

- 仅用于学习和研究目的
- 部分网游有反作弊系统，使用可能导致封号
- 过度加速可能导致游戏物理引擎异常或崩溃
- 不建议在竞技类网游中使用
- 仅支持 x86_64 Linux，需要 glibc

## 许可证

GNU GPL v3 — 与原版 OpenSpeedy 保持一致
