#pragma once

#include <QString>
#include <QByteArray>

class Crypto {
public:
    static QString hashPassword(const QString &password, const QByteArray &salt);
    static bool verifyPassword(const QString &password, const QString &storedHash,
                               const QByteArray &salt);
    static QByteArray generateSalt();
    static QByteArray encrypt(const QByteArray &plaintext, const QString &password);
    static QByteArray decrypt(const QByteArray &ciphertext, const QString &password);

private:
    static QByteArray deriveKey(const QString &password, const QByteArray &salt,
                                int iterations = 100000);
};
