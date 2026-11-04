# Render 部署

本仓库提供 Render 免费试运行配置，使用现有 Dockerfile 编译原版 C++ 裁判，并运行 Node.js 网页服务。没有第三方 npm 依赖，也不需要额外数据库。首次部署仍需检查 Linux 构建、页面、SSE 推送和完整联机对局。

## 推荐：从 Blueprint 创建

在 Render 控制台选择 New → Blueprint，连接 GitHub 私有仓库 `MistForest-Web`，选择分支 `codex/render-deploy-20261104`，使用仓库根目录的 `render.yaml`。

配置为新加坡区域、Free 套餐、单实例、最多 4 个房间，健康检查地址 `/api/health`。提交前确认仍是 Free，且没有添加付费资源。若连接私有仓库需要 GitHub 授权，由仓库所有者在页面完成。

## 也可手动创建 Web Service

选择 New → Web Service，连接同一仓库和分支，按以下内容设置：

| 项目 | 设置 |
| --- | --- |
| Name | `mistforest-web`，名称占用时由平台生成唯一网址 |
| Language | Docker |
| Region | Singapore |
| Instance Type | Free |
| Root Directory | 留空 |
| Dockerfile Path | `./Dockerfile` |
| Docker Build Context Directory | `.` |
| Health Check Path | `/api/health` |
| HOST | `0.0.0.0` |
| PORT | `10000` |
| MAX_ROOMS | `4` |

不设置额外 Build Command 或 Docker Command，使用 Dockerfile 中的构建与启动指令。

## 部署后验证

1. 等待服务状态为 Live，访问平台生成的 `https://...onrender.com` 网址。
2. 访问相同网址下的 `/api/health`，应返回 `ok: true` 和版本信息。
3. 两个不同浏览器标签页分别建房、加入，完成部署、行动、胜负和重开。
4. 用手机流量访问相同网址，检查跨网络联机、触摸操作和返回前台后的同步。电脑窄屏模拟不能代替手机真机验证。

## 试运行边界

- Free 服务闲置 15 分钟会休眠，重新打开约需一分钟启动；当前版本的房间只存在内存，服务休眠、重启和重新部署会清空房间。
- 当前架构必须保持单实例，同一房间由同一个服务进程维护。直接横向扩容会造成房间无法互通。
- 免费配置的 4 房间上限是保守的初始限制，并非容量测试结论；国内网络访问速度仍需实测。
- 后续升级付费实例可以减少休眠影响，不能代替对局持久恢复功能。

参考：[Render Web Services](https://render.com/docs/web-services)、[Docker 部署](https://render.com/docs/docker)、[Blueprint 配置](https://render.com/docs/blueprint-spec)、[免费版限制](https://render.com/docs/free)。
