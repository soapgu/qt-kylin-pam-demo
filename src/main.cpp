#include "authentication.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace {

bool unlock(const QByteArray &user)
{
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("解锁应用"));

    auto *layout = new QVBoxLayout(&dialog);
    auto *prompt = new QLabel(user.isEmpty()
        ? QStringLiteral("请在 Linux 上以普通桌面用户运行应用。")
        : QStringLiteral("请输入当前用户的系统密码。"));
    auto *password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    password->setAccessibleName(QStringLiteral("系统密码"));
    auto *status = new QLabel;
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto *submit = buttons->addButton(QStringLiteral("解锁"), QDialogButtonBox::AcceptRole);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    submit->setDefault(true);

    layout->addWidget(prompt);
    layout->addWidget(password);
    layout->addWidget(status);
    layout->addWidget(buttons);

    password->setEnabled(!user.isEmpty());
    submit->setEnabled(!user.isEmpty());
    password->setFocus();

    QFutureWatcher<AuthResult> watcher;
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(submit, &QPushButton::clicked, &dialog, [&] {
        QByteArray entered = password->text().toUtf8();
        if (entered.isEmpty()) {
            status->setText(QStringLiteral("请输入系统密码。"));
            password->setFocus();
            return;
        }

        password->setText(QString());
        password->setEnabled(false);
        submit->setEnabled(false);
        status->setText(QStringLiteral("正在验证…"));
        watcher.setFuture(QtConcurrent::run([user, entered = std::move(entered)]() mutable {
            return authenticate(user, entered);
        }));
    });

    QObject::connect(&watcher, &QFutureWatcher<AuthResult>::finished, &dialog, [&] {
        switch (watcher.result()) {
        case AuthResult::Success:
            dialog.accept();
            return;
        case AuthResult::PasswordRejected:
            status->setText(QStringLiteral("密码验证失败。3 秒后可重试。"));
            break;
        case AuthResult::AccountRejected:
            status->setText(QStringLiteral("账户不可用或需要修改系统密码。"));
            break;
        case AuthResult::ServiceUnavailable:
            status->setText(QStringLiteral("无法使用认证服务，请检查 PAM 配置。"));
            break;
        }

        QTimer::singleShot(3000, Qt::PreciseTimer, &dialog, [&] {
            if (watcher.result() == AuthResult::PasswordRejected) {
                status->setText(QStringLiteral("密码验证失败，请重试。"));
            }
            password->setEnabled(true);
            submit->setEnabled(true);
            password->setFocus();
        });
    });

    const int result = dialog.exec();
    if (watcher.isRunning()) {
        watcher.waitForFinished();
    }
    return result == QDialog::Accepted;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (!unlock(currentUser())) {
        return 0;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("Qt 麒麟系统密码解锁示例"));

    auto *layout = new QVBoxLayout(&window);
    auto *status = new QLabel(QStringLiteral("已通过系统密码验证。"));
    layout->addWidget(status);

    window.show();
    return app.exec();
}
