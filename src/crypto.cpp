#include "crypto.hpp"

#include <QRandomGenerator>
#include <QDataStream>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <cstring>

static const int SALT_LEN = 16;
static const int KEY_LEN = 32;  // 256 bits
static const int IV_LEN = 12;   // GCM IV
static const int TAG_LEN = 16;  // GCM tag
static const int ITERATIONS = 100000;

QByteArray Crypto::generateSalt() {
    QByteArray salt(SALT_LEN, 0);
    RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), SALT_LEN);
    return salt;
}

QByteArray Crypto::deriveKey(const QString &password, const QByteArray &salt, int iterations) {
    QByteArray key(KEY_LEN, 0);
    PKCS5_PBKDF2_HMAC(
        password.toUtf8().constData(), password.toUtf8().size(),
        reinterpret_cast<const unsigned char*>(salt.constData()), salt.size(),
        iterations, EVP_sha256(),
        KEY_LEN, reinterpret_cast<unsigned char*>(key.data())
    );
    return key;
}

QString Crypto::hashPassword(const QString &password, const QByteArray &salt) {
    QByteArray key = deriveKey(password, salt);
    // Store as base64(salt):base64(key)
    return salt.toBase64() + ":" + key.toBase64();
}

bool Crypto::verifyPassword(const QString &password, const QString &storedHash,
                             const QByteArray &salt) {
    QByteArray expectedKey = deriveKey(password, salt);
    QByteArray storedKey = QByteArray::fromBase64(storedHash.toUtf8());
    // Constant-time comparison
    if (expectedKey.size() != storedKey.size()) return false;
    volatile unsigned char diff = 0;
    for (int i = 0; i < expectedKey.size(); ++i)
        diff |= static_cast<unsigned char>(expectedKey[i] ^ storedKey[i]);
    return diff == 0;
}

QByteArray Crypto::encrypt(const QByteArray &plaintext, const QString &password) {
    QByteArray salt = generateSalt();
    QByteArray key = deriveKey(password, salt);

    QByteArray iv(IV_LEN, 0);
    RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), IV_LEN);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH, 0);
    int outLen = 0, totalLen = 0;

    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));

    EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()),
                      &outLen,
                      reinterpret_cast<const unsigned char*>(plaintext.constData()),
                      plaintext.size());
    totalLen = outLen;

    EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + totalLen,
                        &outLen);
    totalLen += outLen;
    ciphertext.resize(totalLen);

    QByteArray tag(TAG_LEN, 0);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_LEN, tag.data());
    EVP_CIPHER_CTX_free(ctx);

    // Format: salt(16) + iv(12) + tag(16) + ciphertext
    QByteArray result;
    result.append(salt);
    result.append(iv);
    result.append(tag);
    result.append(ciphertext);
    return result;
}

QByteArray Crypto::decrypt(const QByteArray &ciphertext, const QString &password) {
    if (ciphertext.size() < SALT_LEN + IV_LEN + TAG_LEN) return {};

    QByteArray salt = ciphertext.left(SALT_LEN);
    QByteArray iv = ciphertext.mid(SALT_LEN, IV_LEN);
    QByteArray tag = ciphertext.mid(SALT_LEN + IV_LEN, TAG_LEN);
    QByteArray data = ciphertext.mid(SALT_LEN + IV_LEN + TAG_LEN);

    QByteArray key = deriveKey(password, salt);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray plaintext(data.size() + EVP_MAX_BLOCK_LENGTH, 0);
    int outLen = 0, totalLen = 0;

    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_LEN, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                       reinterpret_cast<const unsigned char*>(key.constData()),
                       reinterpret_cast<const unsigned char*>(iv.constData()));

    EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()),
                      &outLen,
                      reinterpret_cast<const unsigned char*>(data.constData()),
                      data.size());
    totalLen = outLen;

    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_LEN,
                        const_cast<char*>(tag.constData()));

    int ret = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + totalLen,
                                  &outLen);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0) return {};  // Authentication failed
    totalLen += outLen;
    plaintext.resize(totalLen);
    return plaintext;
}
