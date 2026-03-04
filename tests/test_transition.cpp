// SPDX-License-Identifier: MIT
// Thrive Video Suite – Transition unit tests

#include <QtTest/QtTest>
#include "../src/core/transition.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestTransition : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstruction()
    {
        Transition t;
        QVERIFY(t.serviceId().isEmpty());
        QVERIFY(t.displayName().isEmpty());
        QVERIFY(t.description().isEmpty());
        QCOMPARE(t.duration().frame(), 0);
    }

    void parameterisedConstruction()
    {
        Transition t(QStringLiteral("luma"),
                     QStringLiteral("Cross Dissolve"),
                     QStringLiteral("Gradually blends two clips"),
                     TimeCode(25, 25.0));

        QCOMPARE(t.serviceId(),   QStringLiteral("luma"));
        QCOMPARE(t.displayName(), QStringLiteral("Cross Dissolve"));
        QCOMPARE(t.description(), QStringLiteral("Gradually blends two clips"));
        QCOMPARE(t.duration().frame(), 25);
    }

    void setDisplayName()
    {
        Transition t;
        t.setDisplayName(QStringLiteral("Wipe"));
        QCOMPARE(t.displayName(), QStringLiteral("Wipe"));
    }

    void setDescription()
    {
        Transition t;
        t.setDescription(QStringLiteral("A linear wipe transition"));
        QCOMPARE(t.description(), QStringLiteral("A linear wipe transition"));
    }

    void setDurationEmitsSignal()
    {
        Transition t(QStringLiteral("luma"),
                     QStringLiteral("Dissolve"),
                     {},
                     TimeCode(10, 25.0));

        int signalCount = 0;
        connect(&t, &Transition::durationChanged, this,
                [&signalCount]() { ++signalCount; });
        t.setDuration(TimeCode(50, 25.0));

        QCOMPARE(signalCount, 1);
        QCOMPARE(t.duration().frame(), 50);
    }

    void setDurationNoDuplicateSignal()
    {
        Transition t(QStringLiteral("luma"), {}, {},
                     TimeCode(30, 25.0));

        int signalCount = 0;
        connect(&t, &Transition::durationChanged, this,
                [&signalCount]() { ++signalCount; });
        t.setDuration(TimeCode(30, 25.0)); // same value

        QCOMPARE(signalCount, 0);
    }

    void accessibleSummary()
    {
        Transition t(QStringLiteral("luma"),
                     QStringLiteral("Cross Dissolve"),
                     QStringLiteral("Blends clips"),
                     TimeCode(25, 25.0));

        const QString summary = t.accessibleSummary();
        QVERIFY(summary.contains(QStringLiteral("Cross Dissolve")));
        QVERIFY(summary.contains(QStringLiteral("Blends clips")));
    }
};

QTEST_MAIN(TestTransition)
#include "test_transition.moc"
