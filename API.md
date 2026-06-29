# roco_helper Plugin HTTP API

本项目作为 HTTP/SSE 客户端运行，不再抓包、不再启动本地 HTTP 服务，也不做 SQL 或业务数据持久化。当前只接入地图角色移动，其他功能等待后续 HTTP API。

默认服务端：`http://127.0.0.1:4939`

主界面可配置：

- 服务端 IP/主机名
- 服务端 HTTP 端口
- `uid` 过滤值，`0` 表示所有用户

## 地图移动快照

启动或重连时，客户端先读取最新地图位置：

```text
GET /api/memory/map.player_position_changed?uid=<uid>
```

响应：

```json
{
  "kind": "map.player_position_changed",
  "uid": "123456789",
  "count": 42,
  "mode": "latest",
  "latest": {
    "game_x": 0,
    "game_y": 0,
    "game_z": 0,
    "rotation": 0.0,
    "ctrl_rotation": 0.0,
    "visible": true
  }
}
```

客户端只消费 `latest` 对象，并写入内存中的地图状态。

## 地图移动 SSE

客户端订阅地图事件流：

```text
GET /api/events/map?uid=<uid>
Accept: text/event-stream
```

连接成功后服务端先返回：

```text
event: ready
data: {"ok":true}

```

角色移动事件：

```text
event: map.player_position_changed
data: {"game_x":0,"game_y":0,"game_z":0,"rotation":0.0,"ctrl_rotation":0.0,"visible":true}

```

字段说明：

| 字段 | 类型 | 说明 |
|------|------|------|
| `game_x` | number | 游戏坐标 X |
| `game_y` | number | 游戏坐标 Y |
| `game_z` | number | 游戏坐标 Z/层级 |
| `rotation` | number | 玩家朝向，单位为度 |
| `ctrl_rotation` | number | 控制朝向，单位为度 |
| `visible` | boolean | 玩家是否显示 |

## 当前客户端数据流

```text
HTTP 快照 -> HttpApiClient -> EventDispatcher -> DataCenter(内存) -> MapWindow
SSE 增量 -> HttpApiClient -> EventDispatcher -> DataCenter(内存) -> MapWindow
```

所有地图移动数据只保存在当前进程内存中。关闭程序后不会写入 SQL 或业务数据文件。
