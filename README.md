# 洛克王国世界助手

本工具只解析游戏客户端与服务器之间的 TCP 流量，并在本地 Web 页面展示数据，可“旁路部署”；不读取游戏内存、不注入游戏进程，更不会修改游戏数据。

支持宠物筛选、精灵图鉴、孵蛋覆盖表、S3 盒子识别、异色提示等功能，数据与游戏内实时同步。

> [!IMPORTANT]
> 无论使用哪种方案，都应先启动助手并确认流量解析已经运行，再点击游戏中的`进入世界`。如果在进入游戏后才启动，请退出到登录界面后重新进入，或断网重连。

> [!CAUTION]
> 在 Windows 游戏电脑上直接运行并抓包可能存在封号风险，请自行评估。

> [!NOTE]
> 本项目只提供 PVE、宠物筛选、图鉴相关功能，不提供 PVP 或其他影响游戏平衡的功能。

## 交流与友链

遇到问题可加入 QQ 群 939403587；反馈时请说明系统、部署方案、版本、发生时间及已脱敏的相关日志。

- [ROCO Wiki（com.roco.roco_flutter）](https://app.candymo.com/)：ROCO Wiki 是一款面向《洛克王国：世界》的综合资料与策略工具，提供精灵、技能、时装、配队、活动地图、伤害计算和排行榜等功能。

- [洛克捕手（zhuweitung/roco-catcher）](https://github.com/zhuweitung/roco-catcher)：基于本项目 API 开发的 Android 应用，可统计精灵捕捉数量、速率、任务进度及后台通知。

- [洛克图鉴（com.besmile.rocomapp）](https://aismile.dev/zh-hans/roco-tools/app)：洛克图鉴

## 先选一种方案

根据你的环境选择下表中的一种使用方式：

| 你的环境                                     | 使用方案                                                 | 你需要做什么                                              |
|------------------------------------------|------------------------------------------------------|-----------------------------------------------------|
| 软路由、旁路由或 NAS 位于游戏流量路径上                   | [Docker 网卡抓包](#方案-a-docker-网卡抓包推荐)                   | 在该设备上运行 Docker，监听实际经过游戏流量的网卡                        |
| 无法让流量经过软路由/NAS，但可以配置代理                   | [Docker SOCKS5](#方案-b-docker-socks5--proxifierclash) | 运行 Docker SOCKS5，并在游戏电脑用 Proxifier 或 Clash 把游戏连接转入它 |
| Windows 或 macOS 本机直接运行游戏，且可接受本机运行工具带来的风险 | [桌面端直接网卡抓包](#方案-c-windows--macos-桌面端直接网卡抓包)          | 运行桌面程序，选择游戏实际使用的网卡                                  |
| 手机/平板游戏                                  | [让手机流量经过可抓包设备](#手机游戏)                                | 优先使用软路由 Docker；也可用电脑热点或合适的转发方案                      |

### 三种方案的边界

- **Docker 网卡抓包**：Docker 容器直接监听宿主机网卡，只适用于游戏流量确实经过该设备的情况。
- **Docker SOCKS5**：Docker 容器同时提供 SOCKS5 代理并在容器内抓取代理转发的游戏流量；必须在游戏设备上配置 Proxifier 或 Clash 等工具把游戏连接转给该代理。
- **Windows/macOS 桌面端**：只提供直接网卡抓包，**不提供 SOCKS5 服务**。如要使用 SOCKS5，使用 Docker SOCKS5 方案。

仅因 NAS 与游戏设备连接到同一交换机或同一个 Wi-Fi，NAS 通常无法看到其他两台设备之间的单播流量；这种情况不要选择 Docker 网卡抓包，应改用 Docker SOCKS5 或调整网络拓扑。

## 下载与端口

- Docker Compose：[网卡抓包版](docker-compose.yml)｜[SOCKS5 版](docker-compose-socks5.yml)
- Windows / macOS：[下载最新版本](https://github.com/h3110w0r1d-y/rocom-helper/releases/latest)

Docker 镜像为 h3110w0r1d6/roco-helper:latest，支持 amd64 与 arm64。

| 用途 | 默认端口 | 说明 |
| --- | --- | --- |
| 游戏流量 | 8195/TCP | 助手解析的游戏连接 |
| Web 界面 | 4939/TCP | 浏览器访问助手 |
| SOCKS5 | 1080/TCP | 仅 Docker SOCKS5 方案使用 |

## 方案 A：Docker 网卡抓包（推荐）

适合软路由、旁路由、透明网桥，或已确认游戏流量会经过的 NAS。容器使用宿主机网络，并需要 NET_RAW、NET_ADMIN 权限。

### 快速步骤

1. 下载 [docker-compose.yml](docker-compose.yml) 到一个独立目录。
2. 在宿主机确认承载游戏流量的网卡：

   ~~~shell
   ip -br addr
   ip route
   ~~~

3. 编辑 Compose 文件中的 ROCO_IFACE=eth0，改成实际网卡名，例如 br-lan、br0 或 eth0。
4. 启动：

   ~~~shell
   docker compose -f docker-compose.yml up -d
   ~~~

5. 查看日志，确认监听已启动：

   ~~~shell
   docker compose -f docker-compose.yml logs -f
   ~~~

6. 在局域网浏览器打开 http://设备局域网IP:4939，再进入游戏。

### 可选：只抓取一个 IP

当网卡流量很多、或单臂路由出现重复流量时，可在 Compose 中填写：

~~~yaml
ROCO_FILTER_HOST=192.168.1.100
~~~

此值必须是单个 IPv4 或 IPv6 地址；生效后过滤器为 tcp port 8195 and host <IP>。留空即保持默认的端口过滤。

## 方案 B：Docker SOCKS5 + Proxifier/Clash

适合游戏流量不经过 Docker 宿主机网卡的情况。Docker 镜像会启动 microsocks，并在容器内抓取代理转发的游戏流量。

**只启动 SOCKS5 容器还不够**：还必须在游戏所在的 Windows 或 macOS 上使用 Proxifier 或 Clash，让游戏进程访问 8195 的连接经过该 SOCKS5。

### 1. 启动 Docker SOCKS5

1. 下载 [docker-compose-socks5.yml](docker-compose-socks5.yml) 到一个独立目录。
2. 启动并查看日志：

   ~~~shell
   docker compose -f docker-compose-socks5.yml up -d
   docker compose -f docker-compose-socks5.yml logs -f
   ~~~

3. 记下 Docker 宿主机的局域网 IP。游戏电脑需要访问：

   | 项目         | 地址                    |
   |------------|-----------------------|
   | Web 界面     | http://宿主机局域网IP:4939  |
   | SOCKS5 服务器 | 宿主机局域网IP:1080，无用户名和密码 |

宿主机防火墙应允许可信局域网访问 TCP 1080 和 4939。不要将 SOCKS5 的 1080 端口暴露到公网。

### 2. 在游戏电脑配置代理

选择其一：

- [使用 Proxifier](#使用-proxifier)
- [使用 Clash](#使用-clash)

代理规则必须同时限定**游戏进程**和**目标端口 8195**，不要把所有系统流量都转入 SOCKS5。

## 方案 C：Windows / macOS 桌面端直接网卡抓包

桌面端不包含 SOCKS5 服务，只能直接监听本机网卡。游戏流量必须能被这台电脑的网卡看到。不能使用加速器。

### Windows

1. 安装 [Npcap](https://npcap.com/#download)，安装时勾选：

   ~~~text
   Install Npcap in WinPcap API-compatible Mode
   ~~~

2. 启动 roco_helper-vX.X.X.exe。
3. 在流量解析中选择游戏实际使用的 Wi-Fi 或以太网网卡，点击开启解析。
4. 点击打开 Web 界面，在本机访问 http://127.0.0.1:4939。
5. 确认解析运行后再进入游戏。

### macOS

1. 安装 Wireshark 提供的 ChmodBPF.pkg，安装后重启。
2. 启动 roco_helper.app，选择承载游戏流量的 Wi-Fi、以太网或热点网卡。
3. 点击开启解析，确认 Web 界面可在 http://127.0.0.1:4939 打开后再进入游戏。

如果 macOS 提示应用损坏、无法验证开发者或被隔离，确认应用来源可信后可执行：

~~~shell
sudo xattr -dr com.apple.quarantine /Applications/roco_helper.app
~~~

应用不在 /Applications 时请替换为实际路径。

## 使用 Proxifier

以下步骤用于 Docker SOCKS5 方案，Windows 和 macOS 的 Proxifier 操作基本相同。

### 添加 SOCKS5 服务器

在 Profile -> Proxy Servers -> Add 填写：

| 字段             | 值                 |
|----------------|-------------------|
| Address        | Docker 宿主机的局域网 IP |
| Port           | 1080              |
| Protocol       | SOCKS Version 5   |
| Authentication | 不启用               |

先用 Proxifier 的检查功能确认代理可连接。

### 添加游戏规则

在 Profile -> Proxification Rules -> Add 新建规则：

| 字段           | 值                                                              |
|--------------|----------------------------------------------------------------|
| Name         | Roco World 8195                                                |
| Applications | Windows：nrc-win64-shipping.exe；macOS PlayCover：com.tencent.nrc |
| Target hosts | Any                                                            |
| Target ports | 8195                                                           |
| Action       | 上一步添加的 SOCKS5 服务器                                              |

将此规则放在 Default、Direct 等宽泛规则之前。启动游戏后，在 Proxifier 连接列表中确认游戏进程访问目标IP:8195 时命中了该 SOCKS5。

## 使用 Clash

以下步骤同样用于 Docker SOCKS5 方案。不同 Clash 客户端的覆写语法可能不同，核心要求是：

1. 添加一个 socks5 节点，服务器填 Docker 宿主机局域网 IP、端口填 1080。
2. 新规则同时匹配进程名与目标端口 8195：
   - Windows：nrc-win64-shipping.exe
   - macOS PlayCover：com.tencent.nrc
3. 将规则放在 DIRECT、MATCH 等兜底规则之前。
4. 若客户端依赖 TUN 或管理员权限才能按进程名匹配，请按客户端要求启用。

示例规则逻辑：

~~~text
PROCESS-NAME=nrc-win64-shipping.exe AND DST-PORT=8195 → Docker SOCKS5
PROCESS-NAME=com.tencent.nrc AND DST-PORT=8195 → Docker SOCKS5
~~~

启动游戏后，确认客户端的连接或日志显示游戏连接通过该 SOCKS5 节点。

## 手机游戏

手机无法直接运行桌面端。应让游戏流量经过可抓包设备：

1. **优先**：使用软路由或旁路由的 Docker 网卡抓包。
2. **电脑热点**：手机连接电脑热点；在电脑上运行桌面端，并选择热点转发流量对应的网卡。
3. **其他转发工具**：确保手机游戏 TCP 流量实际经过运行助手的设备，再选择对应网卡抓包。

仅设置 HTTP 代理不一定能代理游戏连接；不要安装来源不明的证书。

## 确认是否成功

完成任一方案后，按以下顺序检查：

1. 助手日志或状态显示正在监听或解析运行。
2. Web 页面可打开：
   - 桌面端：http://127.0.0.1:4939
   - Docker：http://Docker宿主机局域网IP:4939
3. SOCKS5 方案：Proxifier 或 Clash 显示游戏进程的 8195 连接命中 SOCKS5。
4. 进入世界后，宠物、地图或其他实时数据开始更新。

页面能打开只表示 Web 服务正常，不代表已抓到游戏流量。

## 常见问题

### Docker 网卡抓包没有数据

- ROCO_IFACE 必须是**宿主机**真正经过游戏流量的接口。
- 同一交换机或 Wi-Fi 不等于流量经过 NAS；确认网关、网桥或旁路由策略。
- 保留 Compose 中的 network_mode: host、NET_RAW 与 NET_ADMIN。

### SOCKS5 已启动但没有数据

- 游戏能联网不代表代理规则已命中；检查 Proxifier/Clash 的连接记录。
- 规则要同时匹配正确的游戏进程和目标端口 8195。
- SOCKS5 服务器地址填 Docker 宿主机的局域网 IP，不能填游戏电脑自己的 127.0.0.1。
- 检查 Docker 端口映射和主机防火墙是否允许 1080。

### Web 页面打不开

- 桌面端在本机访问 http://127.0.0.1:4939，不要访问 0.0.0.0。
- Docker 从其他设备访问时使用宿主机局域网 IP，并确认 TCP 4939 已放行。
- 检查 4939 是否被占用。

### 进入世界后仍然没有数据

- 确认助手先于进入世界启动；否则退出到登录界面后重新进入。
- 检查目标连接仍使用 TCP 8195。
- 查看日志中的网卡、权限、会话信息或代理规则错误。

### 看不到网卡或没有抓包权限

- Windows：确认已安装 Npcap 并勾选 WinPcap API-compatible Mode，必要时重启系统。
- macOS：确认已安装 ChmodBPF.pkg，重新登录或重启。

## 日志、数据与 Docker 常用操作

| 平台      | 默认日志位置                                                    |
|---------|-----------------------------------------------------------|
| Windows | %APPDATA%\roco_helper\roco_helper.log                     |
| macOS   | ~/Library/Application Support/roco_helper/roco_helper.log |
| Docker  | Compose 目录下的 ./data/roco_helper.log                       |

~~~shell
# 查看状态与日志
docker compose ps
docker compose logs --tail=200 roco-helper

# 更新镜像并重建
docker compose pull
docker compose up -d

# 停止容器；不会删除 ./data
docker compose down
~~~

数据库和日志均在数据目录中。日志可能含有网卡和连接地址等信息，反馈问题前请脱敏。

## Linux CLI 与环境变量（高级）

Linux 可执行文件只提供直接网卡抓包，不提供 SOCKS5 服务。

~~~shell
./roco_helper \
  --iface=eth0 \
  --bind=0.0.0.0 \
  --port=4939 \
  --filter-host=192.168.1.100 \
  --data_dir=./data
~~~

| CLI 参数         | 环境变量             | 默认值       | 说明                      |
|----------------|------------------|-----------|-------------------------|
| -i, --iface    | ROCO_IFACE       | 默认路由网卡    | pcap 监听接口               |
| -b, --bind     | ROCO_BIND        | 127.0.0.1 | Web 服务绑定地址              |
| -p, --port     | ROCO_PORT        | 4939      | Web 服务端口                |
| -d, --data_dir | ROCO_DATA_DIR    | Qt 应用数据目录 | 数据库和日志目录                |
| --filter-host  | ROCO_FILTER_HOST | 空         | 单个 IPv4/IPv6 抓包过滤；命令行优先 |

## 功能介绍

桌面端可以选择网卡、启动或停止解析、修改 Web 服务监听地址和端口，并打开 Web 界面。

<img src="./screenshots/img1.png" alt="桌面端控制窗口" width="400">

### 宠物管理

支持按自定义名称、精灵名称、进化链、等级、声音、体重、性别、系别、性格、个体值、血脉、蛋组、咕噜球、天分、技能、特长、佩戴奖牌、盒子和标记等条件筛选。

<img src="./screenshots/img2.png" alt="宠物筛选界面">

### 孵蛋覆盖

<img src="./screenshots/img3.png" alt="孵蛋覆盖界面">

## 赞助

<details>
    <summary>点击展开二维码</summary>
    <img src="https://github.com/h3110w0r1d-y/Phoenix/raw/master/img/wechat.png" width="300" alt="微信赞助码">
    <img src="https://github.com/h3110w0r1d-y/Phoenix/raw/master/img/alipay.jpg" width="300" alt="支付宝赞助码">
</details>
