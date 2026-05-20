# RKDetector

基于 RK3588 NPU 的无人机目标检测项目，支持 YOLOv8 和 YOLOv26 两个模型。

## 项目结构

```
RKDetector/
├── yolov8/                 # 检测 Demo（v8 + v26 共享代码）
│   ├── main.cpp            # 入口：读取图片、推理、绘制结果
│   ├── inference.cpp/.h    # 共享推理流程（letterbox、RKNN 推理）
│   ├── postprocess_common.cpp/.h  # 共享后处理工具（NMS、量化、DFL 等）
│   ├── postprocess_v8.cpp/.h      # YOLOv8 专用：DFL 解码 + 逐类 NMS
│   ├── postprocess_26.cpp/.h      # YOLOv26 专用：直接 4 值解码，无 NMS
│   ├── CMakeLists.txt      # 构建两个可执行文件
│   └── model/              # 模型和标签文件
│       ├── v8.rknn         # YOLOv8 模型
│       ├── 26.rknn         # YOLOv26 模型
│       ├── drone.txt       # 标签文件（drone）
│       └── drone.png       # 测试图片
├── 3rdparty/               # 第三方依赖
│   ├── rknpu2/             # RKNN 运行时库
│   ├── opencv2/            # OpenCV 4.11.0（图片读写/绘制）
│   ├── librga/             # RGA 硬件加速（letterbox 缩放）
│   ├── jpeg_turbo/         # JPEG 编解码
│   └── stb_image/          # 图片工具
├── utils/                  # 公共工具库
│   ├── image_utils.c/.h    # 图片处理（letterbox、格式转换）
│   ├── file_utils.c/.h     # 文件读写
│   └── RkType.h            # 数据结构定义
└── build-linux.sh          # 交叉编译脚本
```

## 构建

需要 aarch64-linux-gnu 交叉编译工具链：

```bash
# 构建（同时生成 rknn_yolov8_demo 和 rknn_yolo26_demo）
bash build-linux.sh -d yolov8

# 构建产物在 install/linux/rknn_yolov8_demo/
```

## 运行

部署到 RK3588 设备后：

```bash
# YOLOv8 推理
./rknn_yolov8_demo ./model/v8.rknn ./model/drone.png

# YOLOv26 推理
./rknn_yolo26_demo ./model/26.rknn ./model/drone.png
```

输出 `out.png` 为带检测框的结果图片。

## YOLOv8 与 YOLOv26 的区别

| 特性 | YOLOv8 | YOLOv26 |
|------|--------|---------|
| Box 解码 | DFL（分布焦点损失） | 直接 4 值回归 |
| NMS | 逐类 NMS | 无（依赖模型自身抑制） |
| 推理流程 | 相同 | 相同 |
| 共享代码 | `inference.cpp`、`postprocess_common.cpp`、`main.cpp` | 同左 |

## 模型说明

- 单类别无人机检测模型
- 输入：640x640 RGB 图片
- 输出：检测框坐标 + 置信度 + 类别
- 量化格式：INT8（affine asymmetric）
- 标签文件：`model/drone.txt`
