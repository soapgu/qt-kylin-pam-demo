#include "authentication.h"

#ifdef Q_OS_LINUX
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <security/pam_appl.h>
#include <unistd.h>

namespace {

constexpr char serviceName[] = "qt-kylin-pam-demo";
constexpr char servicePath[] = "/etc/pam.d/qt-kylin-pam-demo";

struct Conversation {
    const QByteArray &password;
    bool passwordSent = false;
};

int respond(int count, const pam_message **messages, pam_response **reply, void *data)
{
    auto &conversation = *static_cast<Conversation *>(data);
    if (count <= 0 || count > PAM_MAX_NUM_MSG) {
        return PAM_CONV_ERR;
    }

    int passwordIndex = -1;
    for (int index = 0; index < count; ++index) {
        switch (messages[index]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            if (conversation.passwordSent || passwordIndex >= 0) {
                return PAM_CONV_ERR;
            }
            passwordIndex = index;
            break;
        case PAM_TEXT_INFO:
        case PAM_ERROR_MSG:
            break;
        default:
            return PAM_CONV_ERR;
        }
    }

    auto *responses = static_cast<pam_response *>(std::calloc(count, sizeof(pam_response)));
    if (!responses) {
        return PAM_BUF_ERR;
    }
    if (passwordIndex >= 0) {
        responses[passwordIndex].resp = ::strdup(conversation.password.constData());
        if (!responses[passwordIndex].resp) {
            std::free(responses);
            return PAM_BUF_ERR;
        }
        conversation.passwordSent = true;
    }
    *reply = responses;
    return PAM_SUCCESS;
}

AuthResult runPam(const QByteArray &user, const QByteArray &password)
{
    QFile file(QString::fromLatin1(servicePath));
    if (!QFileInfo(file).isFile() || !file.open(QIODevice::ReadOnly)) {
        return AuthResult::ServiceUnavailable;
    }
    file.close();

    Conversation conversation{password};
    const pam_conv callback{respond, &conversation};
    pam_handle_t *handle = nullptr;
    int result = pam_start(serviceName, user.constData(), &callback, &handle);
    if (result != PAM_SUCCESS) {
        return AuthResult::ServiceUnavailable;
    }

    result = pam_authenticate(handle, PAM_DISALLOW_NULL_AUTHTOK);
    AuthResult outcome = AuthResult::ServiceUnavailable;
    if (result == PAM_SUCCESS && conversation.passwordSent) {
        result = pam_acct_mgmt(handle, PAM_DISALLOW_NULL_AUTHTOK);
        switch (result) {
        case PAM_SUCCESS:
            outcome = AuthResult::Success;
            break;
        case PAM_ACCT_EXPIRED:
        case PAM_NEW_AUTHTOK_REQD:
        case PAM_PERM_DENIED:
            outcome = AuthResult::AccountRejected;
            break;
        default:
            break;
        }
    } else if (result == PAM_AUTH_ERR || result == PAM_MAXTRIES) {
        outcome = AuthResult::PasswordRejected;
    }
    if (pam_end(handle, result) != PAM_SUCCESS) {
        return AuthResult::ServiceUnavailable;
    }
    return outcome;
}

} // namespace
#endif

QByteArray currentUser()
{
#ifdef Q_OS_LINUX
    const uid_t uid = getuid();
    if (uid == 0 || geteuid() != uid) {
        return {};
    }

    QByteArray buffer(1024, '\0');
    passwd entry{};
    passwd *found = nullptr;
    int result;
    do {
        result = getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found);
        if (result == ERANGE) {
            buffer.resize(buffer.size() * 2);
        }
    } while (result == ERANGE);

    if (result == 0 && found) {
        return QByteArray(found->pw_name);
    }
#endif
    return {};
}

AuthResult authenticate(const QByteArray &user, QByteArray &password)
{
    AuthResult result = AuthResult::ServiceUnavailable;
#ifdef Q_OS_LINUX
    // pam_unix 的口令辅助程序最多读取 512 字节。
    if (password.isEmpty() || password.size() > 512 || password.contains('\0')) {
        result = AuthResult::PasswordRejected;
    } else {
        result = runPam(user, password);
    }
#else
    Q_UNUSED(user);
#endif
    password.fill('\0');
    password.clear();
    return result;
}
