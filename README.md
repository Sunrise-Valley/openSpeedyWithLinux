# OpenSpeedy Linux

OpenSpeedy 的 Linux 移植版 —— 开源免费的游戏变速工具。

通过 LD_PRELOAD 机制拦截时间函数，按倍率缩放游戏内的时间感知，实现加速/减速效果。

## 安装

```bash
git clone https://github.com/sunriseliu/openSpeedy_linux.git
cd openSpeedy_linux
make
```

## 使用

```bash
# 设置 2 倍速
./build/speedctl set 2.0

# 对目标程序应用变速
LD_PRELOAD=./build/libopenspeedy.so ./your_game

# 或使用 run 子命令（自动设置 LD_PRELOAD）
./build/speedctl run ./your_game

# 查看状态
./build/speedctl status

# 恢复原速
./build/speedctl reset
```

## 许可证

GNU GPL v3
