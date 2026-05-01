// SPDX-License-Identifier: MIT
// Thrive Video Suite - StackTemplate tests

#include <QTest>

#include "../src/core/stacktemplate.h"

using namespace Thrive;

class TestStackTemplate : public QObject
{
    Q_OBJECT

private slots:
    void builtInTemplateIsValid()
    {
        const StackTemplate t = StackTemplate::builtInLooneyTunes();
        QVERIFY(t.isValid());
        QCOMPARE(t.id, QStringLiteral("builtin.looney_tunes"));
        QCOMPARE(t.name, QStringLiteral("Looney Tunes Intro"));

        const StackTemplate p = StackTemplate::builtInPbs1971();
        QVERIFY(p.isValid());
        QCOMPARE(p.id, QStringLiteral("builtin.pbs_1971"));
        QCOMPARE(p.name, QStringLiteral("PBS 1971 Ident"));
    }

    void jsonRoundTrip()
    {
        StackTemplate src;
        src.id = QStringLiteral("custom.my_stack");
        src.name = QStringLiteral("My Stack");
        src.description = QStringLiteral("Custom test stack");
        src.captionDefaultText = QStringLiteral("CAPTION");
        src.secondaryPhaseName = QStringLiteral("Second phase");
        src.secondaryDefaultText = QStringLiteral("SECOND");
        src.includeSecondaryByDefault = false;
        src.totalSeconds = 8.0;
        src.overlayStartSeconds = 1.0;
        src.captionStartSeconds = 1.5;
        src.secondaryStartSeconds = 5.0;
        src.fadeSeconds = 0.7;

        StackTemplate dst;
        QString error;
        QVERIFY(StackTemplate::fromJson(src.toJson(), &dst, &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QCOMPARE(dst.id, src.id);
        QCOMPARE(dst.name, src.name);
        QCOMPARE(dst.description, src.description);
        QCOMPARE(dst.captionDefaultText, src.captionDefaultText);
        QCOMPARE(dst.secondaryPhaseName, src.secondaryPhaseName);
        QCOMPARE(dst.secondaryDefaultText, src.secondaryDefaultText);
        QCOMPARE(dst.includeSecondaryByDefault, src.includeSecondaryByDefault);
        QCOMPARE(dst.totalSeconds, src.totalSeconds);
        QCOMPARE(dst.overlayStartSeconds, src.overlayStartSeconds);
        QCOMPARE(dst.captionStartSeconds, src.captionStartSeconds);
        QCOMPARE(dst.secondaryStartSeconds, src.secondaryStartSeconds);
        QCOMPARE(dst.fadeSeconds, src.fadeSeconds);
    }

    void invalidTemplateRejected()
    {
        StackTemplate out;
        QString error;

        QJsonObject bad;
        bad.insert(QStringLiteral("id"), QStringLiteral("custom.bad"));
        bad.insert(QStringLiteral("name"), QStringLiteral("Bad"));
        bad.insert(QStringLiteral("totalSeconds"), 0.0);

        QVERIFY(!StackTemplate::fromJson(bad, &out, &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestStackTemplate)
#include "test_stacktemplate.moc"
