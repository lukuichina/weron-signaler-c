# weron-signaler-c

纯 C WebSocket relay 服务器，用于 weron VPN 信令。

## 概述

基于 libwebsockets 4.1.6 的零依赖静态二进制 relay。
单二进制、无外部运行时依赖，适合 OpenWrt / Linux / Windows 自托管部署。

## 功能

- WebSocket 信令中继：客户端 A ↔ 客户端 B，服务器只做广播
- **分片重组**：SDP Offer/Answer 经 AES-GCM 加密后通常超过 4KB，
  被 gorilla/websocket 拆分后 relay 侧自动重组再广播
- BINARY 帧原样转发，不修改 payload 字节
- 256 并发客户端上限
- 优雅关闭（SIGINT / SIGTERM）
- 命令行参数：`-i` 监听地址、`-p` 端口、`-h` 帮助

## 构建

### 单目标编译

```bash
make TARGET=linux              # Linux x86_64 musl static
make TARGET=win                # Windows x64 (MinGW-w64)
make TARGET=mipsel             # OpenWrt MIPSel glibc
make TARGET=mipsel-musl-sf     # OpenWrt MIPSel musl soft-float
make TARGET=mipsisa64r6        # OpenWrt MIPS64 r6
make TARGET=armhf              # OpenWrt ARMv7 hard-float
```

### 全部 14 个目标

```bash
./build-all.sh
```

## 支持的平台

| 平台 | 目标名 | 二进制文件 | 大小 |
|---|---|---|---|
| Linux x86_64 musl static | `linux` | `build-linux/weron-signaler-c` | 350K |
| Linux x86_64 musl static-pie | `linux_x86_64_musl_static` | `build-linux-x86_64-musl-static/weron-signaler-c` | 339K |
| Linux i686 musl static | `linux_i686_musl_static` | `build-linux-i686-musl-static/weron-signaler-c` | 376K |
| Linux aarch64 musl static | `linux_aarch64_musl_static` | `build-linux-aarch64-musl-static/weron-signaler-c` | 430K |
| Linux armhf glibc static | `linux_armhf` | `build-armhf/weron-signaler-c-armhf` | 857K |
| Linux armv7 musleabihf static | `linux_armv7_musleabihf_static` | `build-linux-armv7-musleabihf-static/weron-signaler-c` | 360K |
| Linux mipsel glibc static | `linux_mipsel` | `build-mipsel/weron-signaler-c-mipsel` | 1.3M |
| Linux mipsel musl SF | `linux_mipsel_musl_sf` | `build-mipsel-musl-sf/weron-signaler-c-mipsel-sf` | 519K |
| Linux mips_be musl static | `linux_mips_be_musl_static` | `build-linux-mips-be-musl-static/weron-signaler-c` | 502K |
| Linux mipsisa64r6 glibc static | `linux_mipsisa64r6` | `build-mipsisa64r6/weron-signaler-c-mipsisa64r6` | 1.9M |
| Linux mipsisa64r6el glibc static | `linux_mipsisa64r6el` | `build-mipsisa64r6el/weron-signaler-c-mipsisa64r6el` | 1.9M |
| Windows x64 glibc (MinGW) | `win_x64` | `build-win/weron-signaler-c.exe` | 531K |
| Windows x64 musl | `win_x64_musl` | `build-win-x64-musl/weron-signaler-c.exe` | 551K |
| Windows i686 musl | `win_i686_musl` | `build-win-i686-musl/weron-signaler-c.exe` | 535K |

所有 Linux/OpenWrt 目标均为**完全静态链接**，零动态库依赖。
Windows 目标因 MinGW-w64 工具链限制，依赖 `KERNEL32.dll`、`msvcrt.dll`、`WS2_32.dll`（均为 Windows 系统自带）。

## 运行

```bash
# 默认监听 127.0.0.1:8788
./build-linux/weron-signaler-c

# 自定义端口和地址
./build-linux/weron-signaler-c -i 0.0.0.0 -p 8788
```

## 七个实现的对比

| | C（libwebsockets） | Cloudflare Workers | Python（aiohttp） | Python（stdlib） | Python（websockets） | Node.js（自托管） | Rust（axum） |
|---|---|---|---|---|---|---|---|
| 项目目录 | `weron-signaler-c/` | `weron-signaler-cf/` | `weron-signaler-python/relay.py` | `weron-signaler-python/relay_std.py` | `weron-signaler-python/relay_ws.py` | `weron-signaler-nodejs/` | `weron-signaler-rust/` |
| 运行时 | 零依赖（静态二进制） | Cloudflare 平台 | Python + aiohttp | Python >= 3.10 stdlib | Python + websockets | Node.js >= 16 | Rust std + tokio |
| 部署 | 直接运行二进制 | `wrangler deploy` | `pip install aiohttp` | 直接运行脚本 | `pip install websockets` | `npm install && npm start` 或 `node app_std.js` | `cargo build --release` |
| 内存 | ~2MB | — | ~15MB | ~5MB | ~8MB | ~30MB / ~15MB(stdlib) | ~5MB |
| 并发 | libwebsockets 多线程 | Durable Objects | asyncio 单线程 | asyncio 单线程 | asyncio 单线程 | 单线程事件循环 | tokio 异步多线程 |
| TLS | 编译时可选 OpenSSL | 原生 HTTPS | 需反向代理 | 需反向代理 | 需反向代理 | 需反向代理 | 需反向代理 |
| 分片重组 | ✅ 原生支持 | 不支持 | 不支持 | 不支持 | 不支持 | ✅ 支持(app_std.js) | 不支持 |
| 第三方依赖 | 零 | wrangler CLI | `aiohttp` | 零 | `websockets>=14` | `ws` 或 零 | tokio + axum + tungstenite |

## 注意事项

Go 客户端**必须**使用 `BinaryMessage` 接收数据。
C 版和 Node.js `app_std.js` 均支持 WebSocket 分片重组——SDP 消息超过 4KB 时
gorilla/websocket 会自动拆分，relay 侧重组后广播，接收方拿到完整密文。