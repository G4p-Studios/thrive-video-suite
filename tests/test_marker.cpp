// SPDX-License-Identifier: MIT
// Thrive Video Suite – Marker unit tests

#include <QTest>
#include <QSignalSpy>
#include "../src/core/marker.h"
#include "../src/core/timecode.h"

using namespace Thrive;

class TestMarker : public QObject
{
    Q_OBJECT

private slots:

    // ── constructors ────────────────────────────────────────────────

    void defaultConstructor()
    {
        Marker m;
        QVERIFY(m.name().isEmpty());
        QCOMPARE(m.position().frame(), 0);
        QVERIFY(m.comment().isEmpty());
    }

    void namedConstructor()
    {
        Marker m(QStringLiteral("Chapter 1"),
                 TimeCode(100, 25.0),
                 QStringLiteral("First chapter"));
        QCOMPARE(m.name(), QStringLiteral("Chapter 1"));
        QCOMPARE(m.position().frame(), 100);
        QCOMPARE(m.comment(), QStringLiteral("First chapter"));
    }

    // ── setters emit changed ────────────────────────────────────────

    void setNameEmitsChanged()
    {
        Marker m;
        QSignalSpy spy(&m, &Marker::changed);
        m.setName(QStringLiteral("Intro"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m.name(), QStringLiteral("Intro"));
    }

    void setPositionEmitsChanged()
    {
        Marker m;
        QSignalSpy spy(&m, &Marker::changed);
        m.setPosition(TimeCode(250, 25.0));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m.position().frame(), 250);
    }

    void setCommentEmitsChanged()
    {
        Marker m;
        QSignalSpy spy(&m, &Marker::changed);
        m.setComment(QStringLiteral("Scene change"));
        QCOMPARE(spy.count(), 1);
    }

    // ── accessible summary ──────────────────────────────────────────

    void accessibleSummary()
    {
        Marker m(QStringLiteral("End Credits"),
                 TimeCode(7500, 25.0));
        const QString s = m.accessibleSummary();
        QVERIFY(s.contains(QStringLiteral("End Credits")));
        QVERIFY(s.contains(QStringLiteral("Marker")));
    }
};

QTEST_MAIN(TestMarker)
#include "test_marker.moc"
