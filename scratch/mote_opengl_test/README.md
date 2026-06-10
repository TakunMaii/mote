# Mote OpenGL Cube Demo

这个样例使用：

- `vendor/glfw`
- `vendor/opengl`
- `lib/c`

效果：

- 一个旋转立方体
- 一个地面平面
- 一个方向光
- 一个投到平面上的假阴影

建议从 `mote` 仓库根目录编译，方便直接复用 `vendor/` 和 `lib/`：

```powershell
.\mote_test scratch\mote_opengl_test\main.mote -I . -I lib -lglfw -framework OpenGL -o scratch\mote_opengl_test\cube_demo
```

在 macOS 上如果你的 GLFW 是 Homebrew 安装，可能还需要：

```powershell
.\mote_test scratch\mote_opengl_test\main.mote -I . -I lib -L /opt/homebrew/lib -lglfw -framework OpenGL -o scratch\mote_opengl_test\cube_demo
```
