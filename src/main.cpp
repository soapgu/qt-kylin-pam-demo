#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("Qt 麒麟系统密码解锁示例"));

    auto *layout = new QVBoxLayout(&window);
    auto *status = new QLabel(QStringLiteral("Qt Widgets 已启动，系统密码验证待接入。"));
    layout->addWidget(status);

    window.show();
    return app.exec();
}
