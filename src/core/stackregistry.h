// SPDX-License-Identifier: MIT
// Thrive Video Suite - Stack template registry

#pragma once

#include "stacktemplate.h"

#include <QVector>

class QString;

namespace Thrive {

class StackRegistry
{
public:
    StackRegistry();

    void reload();

    QVector<StackTemplate> allStacks() const;
    bool findById(const QString &id, StackTemplate *out) const;

    bool saveCustomStack(const StackTemplate &stack, QString *error = nullptr);
    bool deleteCustomStack(const QString &id, QString *error = nullptr);
    bool importStackFile(const QString &filePath, QString *importedId = nullptr,
                         QString *error = nullptr);
    bool exportStackFile(const QString &id, const QString &filePath,
                         QString *error = nullptr) const;

    static QString stackFileFilter();

private:
    void loadCustomStacks();
    bool writeStackToPath(const StackTemplate &stack, const QString &path,
                          QString *error = nullptr) const;
    QString customStacksDir() const;
    static QString sanitizeId(const QString &value);

    QVector<StackTemplate> m_builtin;
    QVector<StackTemplate> m_custom;
};

} // namespace Thrive
