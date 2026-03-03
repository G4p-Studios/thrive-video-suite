// SPDX-License-Identifier: MIT
// Thrive Video Suite – Welcome wizard implementation

#include "welcomewizard.h"
#include "../accessibility/announcer.h"

#include <QWizardPage>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QTextDocumentFragment>

namespace Thrive {

// Strip HTML tags and decode entities, returning plain readable text.
static QString stripHtml(const QString &html)
{
    // QTextDocumentFragment handles all HTML → plain text conversion:
    // strips tags, decodes &amp; etc., turns <li> into lines.
    QString plain = QTextDocumentFragment::fromHtml(html).toPlainText();
    // Collapse multiple blank lines into single newlines
    while (plain.contains(QLatin1String("\n\n\n")))
        plain.replace(QLatin1String("\n\n\n"), QLatin1String("\n\n"));
    return plain.trimmed();
}

// Helper: create a wizard page with a title and rich-text body.
static QWizardPage *makePage(const QString &title,
                             const QString &body,
                             QObject * /*parent*/ = nullptr)
{
    auto *page = new QWizardPage;
    page->setTitle(title);

    auto *label = new QLabel(body);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    label->setFocusPolicy(Qt::StrongFocus);

    // Use plain text for the accessible name so screen readers
    // don't read out HTML tags like <p>, <b>, </li>, etc.
    label->setAccessibleName(stripHtml(body));

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(label);
    layout->addStretch();

    return page;
}

WelcomeWizard::WelcomeWizard(Announcer *announcer, QWidget *parent)
    : QWizard(parent)
    , m_announcer(announcer)
{
    setWindowTitle(tr("Welcome to Thrive Video Suite"));
    setAccessibleDescription(
        tr("A step-by-step introduction to the video editor."));
    setWizardStyle(QWizard::ModernStyle);

    addWelcomePage();
    addScreenReaderPage();
    addNavigationPage();
    addPlaybackPage();
    addEditingPage();
    addEffectsPage();
    addFinishPage();

    connect(this, &QWizard::currentIdChanged,
            this, [this]() { announceCurrentPage(); });

    // Announce the first page shortly after the dialog opens,
    // giving the window time to appear and receive focus.
    QTimer::singleShot(500, this, [this]() { announceCurrentPage(); });
}

void WelcomeWizard::addWelcomePage()
{
    addPage(makePage(
        tr("Welcome"),
        tr("<p>Welcome to <b>Thrive Video Suite</b>, a fully accessible "
           "video editor designed for blind and visually impaired users.</p>"
           "<p>This wizard will walk you through the key shortcuts and "
           "concepts you need to get started.</p>"
           "<p>Press <b>Next</b> or <b>Alt+N</b> to continue.</p>")));
}

void WelcomeWizard::addScreenReaderPage()
{
    addPage(makePage(
        tr("Screen Reader Interaction"),
        tr("<p>Video Suite works with your screen reader (NVDA, JAWS, Narrator, "
           "or any SAPI voice).  Every action is announced automatically.</p>"
           "<p>Audio cues — short tones — play when you cross clip "
           "boundaries or reach the end of a track.  You can adjust the "
           "cue volume or disable them in <b>Preferences → General</b>.</p>")));
}

void WelcomeWizard::addNavigationPage()
{
    addPage(makePage(
        tr("Timeline Navigation"),
        tr("<p>The timeline is a grid of <b>tracks</b> (rows) and "
           "<b>clips</b> (columns).</p>"
           "<ul>"
           "<li><b>Up / Down</b> — move between tracks</li>"
           "<li><b>Left / Right</b> — move between clips</li>"
           "<li><b>M / N</b> — jump to next / previous marker</li>"
           "<li><b>Home / End</b> — jump to start / end of timeline</li>"
           "</ul>")));
}

void WelcomeWizard::addPlaybackPage()
{
    addPage(makePage(
        tr("Playback Controls"),
        tr("<p>Transport controls use the standard J-K-L model:</p>"
           "<ul>"
           "<li><b>Space</b> or <b>K</b> — play / pause</li>"
           "<li><b>J</b> — rewind (press again to speed up)</li>"
           "<li><b>L</b> — fast forward (press again to speed up)</li>"
           "<li><b>, (comma)</b> — step one frame back</li>"
           "<li><b>. (period)</b> — step one frame forward</li>"
           "<li><b>Home</b> — jump to start</li>"
           "<li><b>End</b> — jump to end</li>"
           "</ul>")));
}

void WelcomeWizard::addEditingPage()
{
    addPage(makePage(
        tr("Editing Basics"),
        tr("<p>Basic editing shortcuts:</p>"
           "<ul>"
           "<li><b>Ctrl+I</b> — import media files</li>"
           "<li><b>Enter</b> — place selected media on the timeline</li>"
           "<li><b>Delete</b> — remove selected clip</li>"
           "<li><b>Ctrl+X / C / V</b> — cut / copy / paste clips</li>"
           "<li><b>Ctrl+Z / Ctrl+Y</b> — undo / redo</li>"
           "<li><b>Ctrl+S</b> — save project</li>"
           "<li><b>Ctrl+Shift+E</b> — export / render</li>"
           "</ul>")));
}

void WelcomeWizard::addEffectsPage()
{
    addPage(makePage(
        tr("Effects & Transitions"),
        tr("<p>Press <b>Ctrl+E</b> to open the Effects Browser.  "
           "Type to search, then press <b>Enter</b> to apply an effect "
           "to the selected clip.</p>"
           "<p>Each effect has a spoken description so you know exactly "
           "what it does before applying it.</p>"
           "<p>Transitions are added to the boundary between two adjacent "
           "clips on the same track.</p>")));
}

void WelcomeWizard::addFinishPage()
{
    addPage(makePage(
        tr("You're Ready!"),
        tr("<p>You can revisit this wizard any time from "
           "<b>Help → Welcome Wizard</b>.</p>"
           "<p>Open <b>Preferences</b> (<b>Ctrl+,</b>) to customise "
           "keyboard shortcuts, preview resolution, and manage plugins.</p>"
           "<p>Happy editing!</p>")));
}

void WelcomeWizard::announceCurrentPage()
{
    auto *page = currentPage();
    if (!page) return;

    // Build a complete announcement: title + body text
    QString announcement = page->title();

    // Find the QLabel child which holds the body text
    auto *label = page->findChild<QLabel *>();
    if (label) {
        QString body = label->accessibleName();
        if (!body.isEmpty()) {
            announcement += QStringLiteral(". ") + body;
        }
        // Give the label focus so NVDA can also read it via object nav
        label->setFocus();
    }

    m_announcer->announce(announcement, Announcer::Priority::High);
}

} // namespace Thrive
