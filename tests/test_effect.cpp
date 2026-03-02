// SPDX-License-Identifier: MIT
// Thrive Video Suite – Effect unit tests

#include <QTest>
#include <QSignalSpy>
#include "../src/core/effect.h"

using namespace Thrive;

class TestEffect : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_effect = new Effect(QStringLiteral("brightness"),
                              QStringLiteral("Brightness"),
                              QStringLiteral("Adjusts image brightness"),
                              this);
    }

    void cleanup()
    {
        delete m_effect;
        m_effect = nullptr;
    }

    // ── identity ────────────────────────────────────────────────────

    void constructorSetsFields()
    {
        QCOMPARE(m_effect->serviceId(), QStringLiteral("brightness"));
        QCOMPARE(m_effect->displayName(), QStringLiteral("Brightness"));
        QCOMPARE(m_effect->description(), QStringLiteral("Adjusts image brightness"));
        QVERIFY(m_effect->isEnabled());
    }

    void defaultConstructor()
    {
        Effect e;
        QVERIFY(e.serviceId().isEmpty());
        QVERIFY(e.displayName().isEmpty());
        QVERIFY(e.isEnabled());
    }

    void setDisplayName()
    {
        m_effect->setDisplayName(QStringLiteral("Brightness v2"));
        QCOMPARE(m_effect->displayName(), QStringLiteral("Brightness v2"));
    }

    void setCategory()
    {
        m_effect->setCategory(QStringLiteral("Color"));
        QCOMPARE(m_effect->category(), QStringLiteral("Color"));
    }

    // ── enabled state ───────────────────────────────────────────────

    void toggleEnabled()
    {
        QSignalSpy spy(m_effect, &Effect::enabledChanged);
        m_effect->setEnabled(false);
        QVERIFY(!m_effect->isEnabled());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), false);

        m_effect->setEnabled(false); // same
        QCOMPARE(spy.count(), 1);    // no dup
    }

    // ── parameters ──────────────────────────────────────────────────

    void addAndGetParameter()
    {
        EffectParameter param;
        param.id = QStringLiteral("level");
        param.displayName = QStringLiteral("Level");
        param.type = QStringLiteral("float");
        param.defaultValue = 1.0;
        param.minimum = 0.0;
        param.maximum = 2.0;
        param.currentValue = 1.0;

        m_effect->addParameter(param);
        QCOMPARE(m_effect->parameters().size(), 1);
        QCOMPARE(m_effect->parameterValue(QStringLiteral("level")).toDouble(), 1.0);
    }

    void setParameterValue()
    {
        EffectParameter param;
        param.id = QStringLiteral("amount");
        param.currentValue = 0.5;
        m_effect->addParameter(param);

        QSignalSpy spy(m_effect, &Effect::parameterChanged);
        m_effect->setParameterValue(QStringLiteral("amount"), 0.75);

        QCOMPARE(m_effect->parameterValue(QStringLiteral("amount")).toDouble(), 0.75);
        QCOMPARE(spy.count(), 1);
    }

    void parameterValueNotFound()
    {
        QVERIFY(!m_effect->parameterValue(QStringLiteral("nonexistent")).isValid());
    }

    // ── accessible summary ──────────────────────────────────────────

    void accessibleSummary()
    {
        const QString s = m_effect->accessibleSummary();
        QVERIFY(s.contains(QStringLiteral("Brightness")));
        QVERIFY(s.contains(QStringLiteral("Enabled")) || s.contains(QStringLiteral("enabled")));
    }

    void accessibleSummaryDisabled()
    {
        m_effect->setEnabled(false);
        const QString s = m_effect->accessibleSummary();
        QVERIFY(s.contains(QStringLiteral("Disabled")) || s.contains(QStringLiteral("disabled")));
    }

private:
    Effect *m_effect = nullptr;
};

QTEST_MAIN(TestEffect)
#include "test_effect.moc"
