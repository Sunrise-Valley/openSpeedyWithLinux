#!/bin/bash
# OpenSpeedy Linux — install script
set -e

INSTALL_DIR="${INSTALL_DIR:-/usr/local}"
BIN_DIR="$INSTALL_DIR/bin"
LIB_DIR="$INSTALL_DIR/lib"
DESKTOP_DIR="/usr/local/share/applications"

echo "OpenSpeedy Linux Installer"
echo "========================="

if [ "$(id -u)" -ne 0 ]; then
    echo "需要 root 权限安装到 $INSTALL_DIR"
    echo "用法: sudo ./install.sh"
    echo "或:   INSTALL_DIR=$HOME/.local ./install.sh  (用户安装)"
    exit 1
fi

# Find script directory
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$(dirname "$SCRIPT_DIR")"

echo "安装到: $INSTALL_DIR"
echo ""

# Create directories
mkdir -p "$BIN_DIR" "$LIB_DIR" "$DESKTOP_DIR"

# Install binaries
if [ -f "$SRC_DIR/build/speedctl" ]; then
    cp "$SRC_DIR/build/speedctl" "$BIN_DIR/speedctl"
    chmod 755 "$BIN_DIR/speedctl"
    echo "✓ speedctl → $BIN_DIR/speedctl"
else
    echo "✗ speedctl not found (run 'make' first)"
fi

if [ -f "$SRC_DIR/build/libopenspeedy.so" ]; then
    cp "$SRC_DIR/build/libopenspeedy.so" "$LIB_DIR/libopenspeedy.so"
    chmod 644 "$LIB_DIR/libopenspeedy.so"
    echo "✓ libopenspeedy.so → $LIB_DIR/libopenspeedy.so"
else
    echo "✗ libopenspeedy.so not found (run 'make' first)"
fi

# Install GUI
if [ -f "$SRC_DIR/gui/openspeedy-gui" ]; then
    cp "$SRC_DIR/gui/openspeedy-gui" "$BIN_DIR/openspeedy-gui"
    chmod 755 "$BIN_DIR/openspeedy-gui"
    echo "✓ openspeedy-gui → $BIN_DIR/openspeedy-gui"
fi

# Install desktop entry
if [ -f "$SRC_DIR/gui/openspeedy.desktop" ]; then
    cp "$SRC_DIR/gui/openspeedy.desktop" "$DESKTOP_DIR/openspeedy.desktop"
    echo "✓ 桌面快捷方式 → $DESKTOP_DIR/openspeedy.desktop"
fi

echo ""
echo "安装完成！"
echo ""
echo "使用方式："
echo "  openspeedy-gui              # 图形界面"
echo "  speedctl set 2.0            # 命令行设置速度"
echo "  speedctl run ./your_game    # 启动游戏"
