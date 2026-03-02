// SPDX-License-Identifier: MIT
// Thrive Video Suite – Welcome wizard (first-run onboarding)

#pragma once

#include <QWizard>

QT_FORWARD_DECLARE_CLASS(QLabel)

namespace Thrive {

class Announcer;

/// A QWizard shown on first launch that introduces keyboard
/// shortcuts, screen reader interaction, and basic editing workflow.
/// Every page is fully accessible: the description text is announced
/// via the Announcer when the page is entered.
class WelcomeWizard : public QWizard
{
    Q_OBJECT

public:
    explicit WelcomeWizard(Announcer *announcer,
                           QWidget *parent = nullptr);

private:
    void addWelcomePage();
    void addScreenReaderPage();
    void addNavigationPage();
    void addPlaybackPage();
    void addEditingPage();
    void addEffectsPage();
    void addFinishPage();

    /// Announce the page description when the user moves to it.
    void announceCurrentPage();

    Announcer *m_announcer = nullptr;
};

} // namespace Thrive
