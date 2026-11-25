#pragma once

#include <QByteArray>

enum class AuthResult {
    Success,
    PasswordRejected,
    AccountRejected,
    ServiceUnavailable,
};

QByteArray currentUser();
AuthResult authenticate(const QByteArray &user, QByteArray &password);
