#include "MainWindow.h"
#include "Types.h"
#include <opencv2/core.hpp>

#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>

int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("AIVisionDefect");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("YourOrg");

    // ── 注册自定义类型，使其可通过 Qt 队列信号跨线程传递 ──────────────────
    qRegisterMetaType<FramePtr>("FramePtr");
    qRegisterMetaType<cv::Mat>("cv::Mat");

    QSurfaceFormat fmt;
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QDir modelsDir(QApplication::applicationDirPath() + "/models");
    if (!modelsDir.exists()) modelsDir.mkpath(".");

    MainWindow win;
    win.show();

    return app.exec();
}
