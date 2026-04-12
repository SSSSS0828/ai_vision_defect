#include "MainWindow.h"
#include "Types.h"
#include <opencv2/core.hpp>

#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>

int main(int argc, char* argv[]) {
    // ── 启用 Qt 高分辨率缩放 ───────────────────────────────────────────────
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // ── 创建 Qt 应用程序 ───────────────────────────────────────────────────
    QApplication app(argc, argv);
    app.setApplicationName("AIVisionDefect");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Neu");

    // ── 注册自定义类型，使其可通过 Qt 队列信号跨线程传递 ──────────────────
    qRegisterMetaType<FramePtr>("FramePtr");
    qRegisterMetaType<cv::Mat>("cv::Mat");

    // ── 设置 OpenGL 抗锯齿配置
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    // ── 创建模型目录
    QDir modelsDir(QApplication::applicationDirPath() + "/models");
    if (!modelsDir.exists()) modelsDir.mkpath(".");

    MainWindow win;
    win.show();

    return app.exec();
}
