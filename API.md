# roco_helper Plugin HTTP API

本项目作为 HTTP/SSE 客户端运行，不再抓包、不再启动本地 HTTP 服务，也不做 SQL 或业务数据持久化。当前接入服务端版本校验、地图角色移动、地图标注快照和地图事件 SSE。

默认服务端：`http://127.0.0.1:4939`

主界面可配置：

- 服务端 IP/主机名
- 服务端 HTTP 端口
- `uid` 过滤值，`0` 表示所有用户

## 服务端版本

连接服务端时，客户端先读取服务端版本：

```text
GET /api/version
```

响应：

```json
{
  "version": "2.0",
  "display": "v2.0"
}
```

插件会比较 `version` 和本地 `appVersionString()`，一致才继续连接地图快照和 SSE；版本不一致时停止连接。

## 地图移动快照

版本校验通过后，客户端读取最新地图位置：

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

## 地图标注快照

版本校验通过后，客户端读取全局地图标注：

```text
GET /api/map-markers
```

响应为标注数组。标注不区分 UID：

```json
[
  {
    "id": "uuid-or-custom-id",
    "marker_type": "fruit",
    "label": "",
    "visible": true,
    "temporary": false,
    "game_x": 0,
    "game_y": 0,
    "game_z": 0,
    "extra": {}
  }
]
```

客户端收到全量快照后会替换本地持久标注；临时标注仍只保存在插件进程内。

## 地图标注手动操作

持久标注的新增、拖动、删除都提交到服务端，由服务端写库并通过 SSE 回放到客户端。

```text
POST /api/map-markers
PUT /api/map-markers/<id>
DELETE /api/map-markers/<id>
```

插件的小地图“临时标记”不调用服务端 API。

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

地图标注事件：

```text
event: map.marker_added
data: {"id":"...","marker_type":"fruit","label":"","visible":true,"temporary":false,"game_x":0,"game_y":0,"game_z":0,"extra":{}}

event: map.marker_updated
data: {"id":"...","marker_type":"fruit","label":"","visible":true,"temporary":false,"game_x":1,"game_y":2,"game_z":0,"extra":{}}

event: map.marker_deleted
data: {"id":"..."}

event: map.marker_visibility_changed
data: {"id":"actor-or-marker-id","visible":false}

```

`map.marker_visibility_changed` 用于采集后的前端临时隐藏，不会写入服务端 `map_markers`。插件会按标注 `id` 查找；找不到时，会继续按标注 `extra.actor_id` 匹配。

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
HTTP 版本校验 -> HttpApiClient(版本一致才继续连接)
HTTP 角色快照 -> HttpApiClient -> EventDispatcher -> DataCenter(内存) -> MapWindow
HTTP 标注快照 -> HttpApiClient -> DataCenter(替换持久标注) -> MapWindow
SSE 增量 -> HttpApiClient -> EventDispatcher -> DataCenter(内存) -> MapWindow
手动标点 -> MapWindow -> HttpApiClient -> roco-helper HTTP API -> SSE 回灌
```

角色移动和临时标记只保存在当前进程内存中；持久地图标注由 roco-helper 服务端保存。
