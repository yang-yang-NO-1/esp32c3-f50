# F50 / ESP32-C3 风扇管理

保留 GPIO8 的风扇接线和原有串口协议，增加 WiFiManager 配网和中文网页。
此版本的温度采样与自动阈值仍由 F50 负责，ESP32 不会通过 USB 读取 F50 温度。

## 硬件准备：当前模块的 Wi-Fi 热点问题

**当前使用的模块需要在天线靠近晶振的一端焊接一枚排针，否则收不到配网热点。**
这是当前这块模块的实测装配经验，焊接位置按“天线靠近晶振的一端”识别；
本项目暂未附焊盘照片，也未确认其射频原理，不把此操作推广到其他型号模块。
如果固件已启动但手机扫描不到 `F50-FAN-xxxx`，先检查此处排针是否已焊接。

另外，F50 与 ESP32 必须使用支持数据传输的 USB 连接，并注意接口方向；
F50 出现 `/dev/ttyACM0` 后才可进行串口通信。

## 功能与项目结构

- WiFiManager 配网、保存 Wi-Fi、掉线重连和配网热点。
- 中文管理页：手动开关、恢复自动、查看网络与串口状态、测试串口接收。
- 自动/手动控制仲裁：手动保持 10 分钟，同时缓存 F50 自动意图。
- 两颗外接 LED 分别显示 F50 通信和 Wi-Fi 状态。
- 保留原串口指令，新增 HEARTBEAT 和 AUTO。

| 文件 | 用途 |
| --- | --- |
| `src/main.cpp` | GPIO、串口协议、WiFiManager、HTTP API 与状态灯 |
| `include/control_state.h` | 自动/手动控制状态与自动意图缓存 |
| `include/web_page.h` | `/fan` 详细管理页面，无外部 CDN 依赖 |
| `include/portal_panel.h` | 自动弹出的 WiFiManager 首页中的风扇控制与连接测试 |
| `platformio.ini` | ESP32-C3 编译选项及固定版本的依赖 |
| `tools/f50_heartbeat.sh` | 可选 F50 心跳发送脚本 |
| `tools/f50/f50_fan_control.sh` | F50 v2 温控主程序，集成心跳、重连及周期同步 |
| `tools/f50/ufi_tools_boot.sh` | F50 v2 自启动入口 |
| `tools/f50/restart_fan_control.sh` | 校验 PID 身份后重启温控，保留自动意图 |
| `tools/f50/esp_send.sh` | F50 串口指令测试工具 |
| `tools/test_web.cjs` | 可在电脑运行的网页逻辑测试 |
| `tools/test_portal.cjs` | WiFiManager 首页内嵌控制面板的交互测试 |

## F50 端是否需要更新

**基本温控和网页手动控制不要求更新 F50 脚本；持续监测及断线恢复建议更新。**
原 F50 的 PING、STATUS、FAN_ON、FAN_OFF 在新固件中继续兼容。
目前 F50 配置为 50℃开启、40℃关闭、每 5 秒采样；这些值来自设备原脚本，
不由本项目固件设置。网页的“恢复自动”也不会改变 F50 温度阈值。

| 使用目标 | F50 端操作 |
| --- | --- |
| 沿用原来的自动温控 | 不必改代码；烧录 ESP32 后重启一次温控脚本以同步输出 |
| 网页手动开关 / 恢复自动 | 不必改代码；手动期间自动请求由 ESP32 缓存 |
| 临时双向串口验证 | 不必改代码；网页开始测试后在 F50 执行 `sh /data/local/tmp/esp_send.sh PING` |
| 连接指示灯持续常亮 | 增加 5 秒心跳；可用本项目可选脚本监测通信链路 |
| 判断温控主循环是否存活 | 将心跳放入温控脚本每轮采样，不能仅启动独立心跳进程 |
| 开机时未插 ESP32，稍后再插仍能启动 | 修改自启动脚本：串口缺失时持续重试，不能只检查一次后退出 |
| ESP32 单独重启或 USB 重新枚举后恢复控制 | 温控主循环重新初始化串口，周期性发送当前自动意图，避免旧状态文件阻止同步 |

本项目已提供 F50 v2 温控/自启动脚本：单实例运行、USB 热插拔重试、
每轮心跳、周期性重发自动意图，并限制日志大小。重发 FAN 指令在 ESP32 手动模式下
只更新自动意图，不会覆盖网页手动输出。独立心跳脚本只能解决“有无近期消息”的显示，
不能修复温控或自启动本身。

### F50 v2 安装与运行

将 `tools/f50/` 中脚本按以下目标路径复制，使用 LF 换行：

| 项目文件 | F50 目标路径 |
| --- | --- |
| `f50_fan_control.sh` | `/data/local/tmp/f50_fan_control.sh` |
| `restart_fan_control.sh` | `/data/local/tmp/restart_fan_control.sh` |
| `ufi_tools_boot.sh` | `/data/media/0/ufi_tools_boot.sh` |
| `esp_send.sh` | `/data/local/tmp/esp_send.sh` |

部署前备份原文件，先对新文件执行 `sh -n`；`/data/local/tmp/` 下脚本权限设为 700。
使用 root 执行 `sh /data/local/tmp/restart_fan_control.sh`。无需重启整个 F50。

- 每 5 秒读取温度、发送心跳，并重发当前 FAN_ON/FAN_OFF 自动意图。
- USB 不存在、串口设置或写入失败时继续重试，单次串口操作最多等待 2 秒。
- 温度读取失败或数值异常时请求风扇开启，不将读取失败当作低温。
- 重启保留已有自动意图，在 40～50℃回差区间不无故切换；首次无状态时沿用旧策略。
- 通过 `flock` 持有单实例锁；Android mksh 使用显式 `9>&9` 传递文件描述符。
- 每分钟记录采样摘要，状态/错误变化立即记录。主日志约 256 KiB 时轮转，保留一份旧日志。
- `serial=sent` 仅表示写入成功；程序不读取串口，不抢占 `esp_send.sh` 的回复，
  因而不把写入成功表述为 ACK 验证成功。
- **不要同时启动独立的 `tools/f50_heartbeat.sh`**，v2 主循环已包含心跳。

检查命令：

```sh
ps -ef | grep '[s]h /data/local/tmp/f50_fan_control.sh'
tail -n 15 /data/local/tmp/f50_fan_control.log
sh /data/local/tmp/esp_send.sh PING
sh /data/local/tmp/esp_send.sh STATUS
```

设备备份路径、回退说明及实测范围见 [F50 部署记录](docs/F50_DEPLOYMENT.md)。
如果串口出现 `ERR UNKNOWN HEARTBEAT`，说明 ESP32 仍是旧固件，需升级后才能识别心跳。

## 编译和烧录

```powershell
pio run
pio run -t upload --upload-port COM实际端口
```

编译产物 `.pio/build/esp32c3/firmware.bin` 是应用固件；首次使用外部烧录工具时，
还需对应 bootloader、分区表和正确地址，推荐直接使用 PlatformIO upload。
烧录会重启 ESP32，上电默认关闭风扇、进入 AUTO。F50 v2 会在下一轮（约 5 秒）
自动重发风扇意图；旧 F50 脚本只在状态变化时发送，烧录后需执行一次下列命令同步：

```sh
sh /data/local/tmp/restart_fan_control.sh
```

使用 v2 时也可以用该命令主动重启温控。`fan_state` 表示 F50 自动意图，
网页手动模式下可以与 ESP32 当前输出不同。

## 配网和管理

1. 有已保存 Wi-Fi 时自动重连；15 秒仍未连上时，开启 `F50-FAN-xxxx` 热点。
2. 热点密码为 `12345678`，连接后打开 `http://192.168.4.1/` 使用 WiFiManager 配网。
3. 热点下打开 `http://192.168.4.1/`：自动弹出的 WiFiManager 首页直接包含风扇开关、
   自动模式、F50 连接状态和串口测试；下方 Configure WiFi 进入原生配网页。
   “详细状态与管理”位于同一地址的 `/fan` 路径。
4. 联网后使用 `http://ESP32的IP/`；支持 mDNS 的客户端也可使用
   `http://f50-fan.local/`。IP 可在路由器接入列表查找。
5. 管理页“开启 WiFiManager 配网”可重新开启热点，不清除原凭据。
   配网、门户首页、详细管理和 API 共享 WiFiManager 的一个 HTTP 服务，统一使用 80 端口。
   热点与联网模式切换后自动重新注册路由，配网完成后管理页面仍可使用。

### BOOT 键清除 Wi-Fi

在固件已正常运行时，连续按住板上的 **BOOT（GPIO9，低电平有效）5 秒**：

- 使用 40ms 消抖，短按不清除；一次长按仅触发一次，松开再按才可重新触发。
- 清除保存的 Wi-Fi SSID 和密码，断开原网络，立即重新开启配网热点。
- 不重启 ESP32，不改变风扇输出或自动/手动模式，也不改变热点密码 `12345678`。
- 按住过程中 GPIO5 状态灯快速闪烁，触发后显示配网热点的闪烁方式。
- 请在正常运行后操作；BOOT 同时是启动模式引脚，按住 BOOT 再复位/上电会进入下载模式，
  此时固件尚未运行，不能清除 Wi-Fi。若进入下载模式，松开 BOOT 后按 RESET 恢复。
- 配网弹窗取决于手机/电脑是否启用门户检测；没有自动弹出时手动访问 `http://192.168.4.1/`。

管理页无需登录，供受信任的本地网络使用；状态修改接口使用启动时随机令牌，
防止其他网页跨站发送开关命令。令牌不是访问密码。

## 手动与自动控制

- 网页“手动开启/关闭”会进入手动模式，持续 10 分钟，可随时点击“恢复 F50 自动”。
- 手动期间缓存 F50 最近的 FAN_ON/FAN_OFF 意图，不覆盖手动输出。
- 恢复自动或手动到期后应用最近的 F50 指令。如果本次启动未收到风扇指令，
  保持当前输出，等待 F50 下发；页面会明确显示此状态。
- 模式不写入闪存，重启回到 AUTO、风扇关闭。
- 断开 USB 或 Wi-Fi 不会主动改变风扇输出，不根据“没有消息”猜测温度。
- 页面“已开启”仅代表 GPIO 输出，无转速传感器反馈。

## F50 连接状态与测试

分开显示 USB 主机帧信号，以及 30 秒内是否收到有效串口消息。
主机可能是 F50，也可能是电脑；仅靠 USB 无法识别主机身份。
原 F50 温控程序只在切换状态时发送命令，超过 30 秒无消息显示“待通信”，
不代表断线或温控程序停止。

点击“测试串口接收”后，15 秒内在 F50 执行：

```sh
sh /data/local/tmp/esp_send.sh PING
```

F50 返回 `OK PONG`，网页显示测试期间收到新消息，这两项合起来可验证双向通信。
网页测试本身只检测接收，不会主动向 F50 发探测要求，因为旧 F50 程序没有应答协议。
`/data/local/tmp` 默认不在系统 PATH 中，请按上例使用完整路径，
直接输入 `esp_send.sh PING` 可能报 `inaccessible or not found`。

可选：将 `tools/f50_heartbeat.sh` 复制到 F50 的 `/data/local/tmp/f50_heartbeat.sh`，
启动一个实例：

```sh
nohup sh /data/local/tmp/f50_heartbeat.sh >/data/local/tmp/f50_heartbeat.out 2>&1 </dev/null &
```

它每 5 秒发 HEARTBEAT，并在串口重新出现后重试，不会读取串口抢走 PONG。
执行 `esp_send.sh` 时可能看到夹杂的 `OK HEARTBEAT`，属正常现象。
**心跳只能证明心跳脚本到 ESP32 的链路，不能证明温控程序本身存活。**
需要监测温控主循环时，使用 `tools/f50/` 中的 v2 程序，其每轮采样已集成心跳。
如果仍使用旧 F50 自启动脚本，它只在 90 秒后检查一次 USB；v2 不再有此限制。

## 状态灯接线与规划

风扇 GPIO8 保持原接线。SuperMini 板载灯也在 GPIO8，随风扇电平变化，
不能独立闪烁指示网络或连接（否则会脉冲驱动风扇）。使用两颗外接普通 LED：

`GPIO → 330Ω～1kΩ 电阻 → LED 正极；LED 负极 → GND`。

| 引脚 | 灯效 | 含义 |
| --- | --- | --- |
| GPIO4（建议绿灯） | 常亮 | USB 存在且 30 秒内收到有效消息 |
| GPIO4 | 每 2 秒双短闪 | USB 存在，等待近期消息 |
| GPIO4 | 每 2 秒单短闪 | 没有 USB 主机信号 |
| GPIO5（建议蓝灯） | 常亮 | Wi-Fi 已连接 |
| GPIO5 | 每 400ms 闪烁 | 配网热点开启 |
| GPIO5 | 每 2 秒短闪 | Wi-Fi 重连中 |
| GPIO5 | 每 100ms 闪烁 | BOOT 长按计时中（优先显示） |

引脚及风扇极性在 `src/main.cpp` 顶部调整。状态灯固定高电平点亮；不接 LED 时无需接线，
也可将对应 LED 引脚设置为 `-1` 禁用。不要接到 USB GPIO18/19。

## 串口协议（115200，换行结束）

| 命令 | 回复 |
| --- | --- |
| PING | OK PONG |
| STATUS | OK STATUS FAN=ON 或 OFF |
| FAN_ON / FAN_OFF | AUTO 下保持 OK FAN ON / OFF；手动下 ERR MANUAL_OVERRIDE 并缓存请求 |
| HEARTBEAT | OK HEARTBEAT |
| AUTO | OK AUTO，恢复 F50 控制 |

原 F50 脚本没有检查 ACK，手动模式下其状态文件可能表示自动意图而非实际输出，
所以固件单独缓存自动意图，在恢复自动时应用。网页按钮不会往串口输出额外日志。
超过 95 字节的行会整体丢弃并回复 `ERR LINE_TOO_LONG`。

验证步骤：配网、网页开关、手动期间发送反向 FAN 指令并恢复自动、PING 和
网页接收测试、拔插 USB、联网中断与重连、BOOT 短按/长按清除并重新配网、
配网前后 80 端口门户与 API 可用、最后恢复 AUTO 并重启 F50 温控脚本。

## 本地验证

```powershell
pio run
node tools/test_web.cjs
node tools/test_portal.cjs
```

网页测试覆盖 USB 连接与近期通信的区别、手动模式提示、SSID 作为纯文本显示、
等待接收状态、控制请求令牌、门户首页开关/自动/测试操作和 HTTP 错误处理。编译及电脑端测试不能代替
真实开发板的配网、USB 通信、输出极性和 LED 接线验证。

