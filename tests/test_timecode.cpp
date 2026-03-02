// SPDX-License-Identifier: MIT
// Thrive Video Suite – TimeCode unit tests

#include <QTest>
#include "../src/core/timecode.h"

using Thrive::TimeCode;

class TestTimeCode : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructor()
    {
        TimeCode tc;
        QCOMPARE(tc.frame(), 0);
        QCOMPARE(tc.fps(), 25.0);
    }

    void frameConstructor()
    {
        TimeCode tc(90, 30.0);          // 3 seconds at 30 fps
        QCOMPARE(tc.frame(), 90);
        QCOMPARE(tc.fps(), 30.0);
    }

    void toString_data()
    {
        QTest::addColumn<int>("frames");
        QTest::addColumn<double>("fps");
        QTest::addColumn<QString>("expected");

        QTest::newRow("zero")       << 0   << 30.0 << QStringLiteral("00:00:00:00");
        QTest::newRow("1 frame")    << 1   << 30.0 << QStringLiteral("00:00:00:01");
        QTest::newRow("1 second")   << 30  << 30.0 << QStringLiteral("00:00:01:00");
        QTest::newRow("1 minute")   << 1800 << 30.0 << QStringLiteral("00:01:00:00");
        QTest::newRow("1 hour")     << 108000 << 30.0 << QStringLiteral("01:00:00:00");
        QTest::newRow("mixed")      << 3661 * 30 + 15 << 30.0
                                    << QStringLiteral("01:01:01:15");
    }

    void toString()
    {
        QFETCH(int, frames);
        QFETCH(double, fps);
        QFETCH(QString, expected);

        TimeCode tc(frames, fps);
        QCOMPARE(tc.toString(), expected);
    }

    void toSpokenString()
    {
        TimeCode tc(3661 * 30, 30.0);   // 1h 1m 1s
        const QString spoken = tc.toSpokenString();
        QVERIFY(spoken.contains(QStringLiteral("1 hour")));
        QVERIFY(spoken.contains(QStringLiteral("1 minute")));
        QVERIFY(spoken.contains(QStringLiteral("1 second")));
    }

    void arithmetic()
    {
        TimeCode a(10, 30.0);
        TimeCode b(5,  30.0);
        QCOMPARE((a + b).frame(), 15);
        QCOMPARE((a - b).frame(), 5);
    }

    void comparison()
    {
        TimeCode a(10, 30.0);
        TimeCode b(20, 30.0);
        QVERIFY(a < b);
        QVERIFY(b > a);
        QVERIFY(a == TimeCode(10, 30.0));
        QVERIFY(a != b);
    }
};

QTEST_MAIN(TestTimeCode)
#include "test_timecode.moc"
