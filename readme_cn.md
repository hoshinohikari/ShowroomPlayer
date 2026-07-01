# ShowroomPlayer

****

| Author | HoshinoKun |
| ------ | ----------- |
| E-mail | hoshinokun@346pro.club |

****

[English](/readme.md)

![ShowroomPlayer](README/screen.png)

## What's this?
一个基于 Qt 6 的 Showroom 直播桌面观看器  
支持多房间监控、HLS 播放，以及实时弹幕与礼物展示

## Features
- **监控列表** — 手动添加 Showroom 用户名，自动刷新直播/离线状态（默认轮询间隔 10 秒）
- **关注同步** — 登录后，关注列表中正在直播的房间会自动加入监控列表
- **视频播放** — 通过 Qt Multimedia（FFmpeg 后端）播放 HLS，支持暂停/继续、停止、追至直播边缘
- **实时弹幕** — 通过房间 WebSocket 接收评论、telop 与系统消息
- **礼物** — 礼物动态、贡献者排行，以及可选的活动模式（×2.5 pt）
- **会话恢复** — 登录状态本地保存，下次启动自动恢复

## How to use
1. 启动 `ShowroomPlayer`。
2. *（可选）* 在侧栏点击 **Login**，使用 Showroom 账号 ID 与密码登录。  
   登录后，关注列表中正在直播的房间会自动出现在监控列表中。
3. 在侧栏输入 Showroom **用户名**，点击 **Add** 添加。
4. 当用户显示 **LIVE** 时，点击对应行即可在中间面板开始播放。
5. 播放期间，右侧面板会显示 **Live chat** 与 **Gifts**。

将鼠标移到视频区域可显示播放控制（暂停、追直播、停止）。

### 会话文件
登录成功后，程序会将会话 cookie（`sr_id`）保存到：

```
%APPDATA%/ShowroomPlayer/session.json        （Windows）
~/Library/Application Support/ShowroomPlayer/  （macOS）
~/.config/ShowroomPlayer/                    （Linux）
```

在侧栏点击 **Logout** 可清除已保存的会话。
