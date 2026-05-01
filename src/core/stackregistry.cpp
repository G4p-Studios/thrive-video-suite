// SPDX-License-Identifier: MIT
// Thrive Video Suite - Stack template registry implementation

#include "stackregistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

namespace Thrive {

StackRegistry::StackRegistry()
    : m_builtin{ StackTemplate::builtInLooneyTunes() }
{
    reload();
}

void StackRegistry::reload()
{
    m_custom.clear();
    loadCustomStacks();
}

QVector<StackTemplate> StackRegistry::allStacks() const
{
    QVector<StackTemplate> all = m_builtin;
    all += m_custom;
    return all;
}

bool StackRegistry::findById(const QString &id, StackTemplate *out) const
{
    const auto all = allStacks();
    for (const auto &stack : all) {
        if (stack.id == id) {
            if (out)
                *out = stack;
            return true;
        }
    }
    return false;
}

bool StackRegistry::saveCustomStack(const StackTemplate &stack, QString *error)
{
    StackTemplate toSave = stack;
    toSave.id = sanitizeId(toSave.id.isEmpty() ? toSave.name : toSave.id);
    if (toSave.id.startsWith(QStringLiteral("builtin.")))
        toSave.id.prepend(QStringLiteral("custom."));
    if (!toSave.isValid()) {
        if (error)
            *error = QStringLiteral("Stack template is invalid.");
        return false;
    }

    const QString dirPath = customStacksDir();
    QDir dir;
    if (!dir.mkpath(dirPath)) {
        if (error)
            *error = QStringLiteral("Unable to create custom stacks folder.");
        return false;
    }

    const QString filePath = QDir(dirPath).filePath(toSave.id + QStringLiteral(".tstk"));
    if (!writeStackToPath(toSave, filePath, error))
        return false;

    reload();
    return true;
}

bool StackRegistry::deleteCustomStack(const QString &id, QString *error)
{
    if (id.startsWith(QStringLiteral("builtin."))) {
        if (error)
            *error = QStringLiteral("Built-in stacks cannot be removed.");
        return false;
    }

    const QString filePath = QDir(customStacksDir()).filePath(id + QStringLiteral(".tstk"));
    if (!QFile::exists(filePath)) {
        if (error)
            *error = QStringLiteral("Stack file does not exist.");
        return false;
    }

    if (!QFile::remove(filePath)) {
        if (error)
            *error = QStringLiteral("Failed to remove stack file.");
        return false;
    }

    reload();
    return true;
}

bool StackRegistry::importStackFile(const QString &filePath,
                                    QString *importedId,
                                    QString *error)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open stack file for reading.");
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid stack file JSON.");
        return false;
    }

    StackTemplate stack;
    if (!StackTemplate::fromJson(doc.object(), &stack, error))
        return false;

    stack.id = sanitizeId(stack.id.isEmpty() ? stack.name : stack.id);
    if (!saveCustomStack(stack, error))
        return false;

    if (importedId)
        *importedId = stack.id;
    return true;
}

bool StackRegistry::exportStackFile(const QString &id,
                                    const QString &filePath,
                                    QString *error) const
{
    StackTemplate stack;
    if (!findById(id, &stack)) {
        if (error)
            *error = QStringLiteral("Stack not found.");
        return false;
    }
    return writeStackToPath(stack, filePath, error);
}

QString StackRegistry::stackFileFilter()
{
    return QStringLiteral("Thrive Stack (*.tstk);;All Files (*)");
}

void StackRegistry::loadCustomStacks()
{
    QDir dir(customStacksDir());
    if (!dir.exists())
        return;

    const QStringList files = dir.entryList({ QStringLiteral("*.tstk") }, QDir::Files);
    for (const QString &fileName : files) {
        QFile f(dir.filePath(fileName));
        if (!f.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        StackTemplate stack;
        if (!StackTemplate::fromJson(doc.object(), &stack))
            continue;

        stack.id = sanitizeId(stack.id.isEmpty() ? QFileInfo(fileName).baseName() : stack.id);
        if (!stack.id.startsWith(QStringLiteral("builtin.")))
            m_custom.append(stack);
    }
}

bool StackRegistry::writeStackToPath(const StackTemplate &stack,
                                     const QString &path,
                                     QString *error) const
{
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Cannot open stack file for writing.");
        return false;
    }

    const QJsonDocument doc(stack.toJson());
    if (out.write(doc.toJson(QJsonDocument::Indented)) < 0) {
        if (error)
            *error = QStringLiteral("Failed to write stack file.");
        return false;
    }

    return true;
}

QString StackRegistry::customStacksDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base).filePath(QStringLiteral("stacks"));
}

QString StackRegistry::sanitizeId(const QString &value)
{
    QString id = value.trimmed().toLower();
    if (id.isEmpty())
        id = QStringLiteral("custom_stack");

    for (int i = 0; i < id.size(); ++i) {
        const QChar c = id.at(i);
        const bool allowed = c.isLetterOrNumber()
            || c == QLatin1Char('_')
            || c == QLatin1Char('.')
            || c == QLatin1Char('-');
        if (!allowed)
            id[i] = QLatin1Char('_');
    }

    return id;
}

} // namespace Thrive
