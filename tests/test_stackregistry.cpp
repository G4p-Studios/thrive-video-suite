// SPDX-License-Identifier: MIT
// Thrive Video Suite - StackRegistry tests

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "../src/core/stackregistry.h"
#include "../src/core/stacktemplate.h"

using namespace Thrive;

class TestStackRegistry : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        m_prevOrg = QCoreApplication::organizationName();
        m_prevApp = QCoreApplication::applicationName();

        QCoreApplication::setOrganizationName(QStringLiteral("ThriveTests"));
        QCoreApplication::setApplicationName(QStringLiteral("StackRegistryTest"));

        clearCustomStacks();
    }

    void cleanup()
    {
        clearCustomStacks();
    }

    void cleanupTestCase()
    {
        clearCustomStacks();
        QCoreApplication::setOrganizationName(m_prevOrg);
        QCoreApplication::setApplicationName(m_prevApp);
    }

    void builtInStackAvailable()
    {
        StackRegistry registry;
        const auto stacks = registry.allStacks();

        QVERIFY(!stacks.isEmpty());

        StackTemplate builtIn;
        QVERIFY(registry.findById(QStringLiteral("builtin.looney_tunes"), &builtIn));
        QCOMPARE(builtIn.name, QStringLiteral("Looney Tunes Intro"));

        StackTemplate pbsBuiltIn;
        QVERIFY(registry.findById(QStringLiteral("builtin.pbs_1971"), &pbsBuiltIn));
        QCOMPARE(pbsBuiltIn.name, QStringLiteral("PBS 1971 Ident"));

        StackTemplate pbs1984BuiltIn;
        QVERIFY(registry.findById(QStringLiteral("builtin.pbs_1984"), &pbs1984BuiltIn));
        QCOMPARE(pbs1984BuiltIn.name, QStringLiteral("PBS 1984 Ident"));
    }

    void saveCustomStackSanitizesId()
    {
        StackRegistry registry;

        StackTemplate t = StackTemplate::builtInLooneyTunes();
        t.id = QStringLiteral(" My Stack!* ");
        t.name = QStringLiteral("My Stack");

        QString error;
        QVERIFY2(registry.saveCustomStack(t, &error), qPrintable(error));

        registry.reload();

        StackTemplate found;
        QVERIFY(registry.findById(QStringLiteral("my_stack__"), &found));
        QCOMPARE(found.name, QStringLiteral("My Stack"));
    }

    void saveBuiltinPrefixedIdIsRewritten()
    {
        StackRegistry registry;

        StackTemplate t = StackTemplate::builtInLooneyTunes();
        t.id = QStringLiteral("builtin.custom_clone");
        t.name = QStringLiteral("Custom Clone");

        QString error;
        QVERIFY2(registry.saveCustomStack(t, &error), qPrintable(error));

        registry.reload();

        StackTemplate found;
        QVERIFY(registry.findById(QStringLiteral("custom.builtin.custom_clone"), &found));
        QCOMPARE(found.name, QStringLiteral("Custom Clone"));
    }

    void importExportAndDeleteRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString importPath = dir.filePath(QStringLiteral("import_source.tstk"));
        const QString exportPath = dir.filePath(QStringLiteral("exported_stack.tstk"));

        StackTemplate src = StackTemplate::builtInLooneyTunes();
        src.id = QStringLiteral(" Imported Stack 01 ");
        src.name = QStringLiteral("Imported Stack");

        QFile importFile(importPath);
        QVERIFY(importFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QJsonDocument srcDoc(src.toJson());
        QVERIFY(importFile.write(srcDoc.toJson(QJsonDocument::Indented)) > 0);
        importFile.close();

        StackRegistry registry;

        QString importedId;
        QString error;
        QVERIFY2(registry.importStackFile(importPath, &importedId, &error), qPrintable(error));
        QVERIFY(!importedId.isEmpty());

        StackTemplate imported;
        QVERIFY(registry.findById(importedId, &imported));
        QCOMPARE(imported.name, QStringLiteral("Imported Stack"));

        QVERIFY2(registry.exportStackFile(importedId, exportPath, &error), qPrintable(error));
        QVERIFY(QFile::exists(exportPath));

        QFile exportedFile(exportPath);
        QVERIFY(exportedFile.open(QIODevice::ReadOnly));
        QJsonParseError parseError;
        const QJsonDocument exportedDoc = QJsonDocument::fromJson(exportedFile.readAll(), &parseError);
        QVERIFY(parseError.error == QJsonParseError::NoError);
        QVERIFY(exportedDoc.isObject());

        const QJsonObject exportedObj = exportedDoc.object();
        QCOMPARE(exportedObj.value(QStringLiteral("id")).toString(), importedId);
        QCOMPARE(exportedObj.value(QStringLiteral("name")).toString(), QStringLiteral("Imported Stack"));

        QVERIFY2(registry.deleteCustomStack(importedId, &error), qPrintable(error));

        registry.reload();
        StackTemplate removed;
        QVERIFY(!registry.findById(importedId, &removed));
    }

    void removeBuiltInRejected()
    {
        StackRegistry registry;
        QString error;

        QVERIFY(!registry.deleteCustomStack(QStringLiteral("builtin.looney_tunes"), &error));
        QVERIFY(!error.isEmpty());
    }

private:
    static QString customStacksDirPath()
    {
        const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return QDir(base).filePath(QStringLiteral("stacks"));
    }

    static void clearCustomStacks()
    {
        QDir dir(customStacksDirPath());
        if (!dir.exists())
            return;

        const QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        for (const auto &entry : entries)
            QFile::remove(dir.filePath(entry));
    }

    QString m_prevOrg;
    QString m_prevApp;
};

QTEST_MAIN(TestStackRegistry)
#include "test_stackregistry.moc"
