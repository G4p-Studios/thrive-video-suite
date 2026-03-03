// SPDX-License-Identifier: MIT
// Thrive Video Suite – Undo/redo command implementations

#include "commands.h"
#include "timeline.h"
#include "track.h"
#include "clip.h"
#include "effect.h"
#include "marker.h"
#include "transition.h"

namespace Thrive {

// ===========================================================================
// AddClipCommand
// ===========================================================================
AddClipCommand::AddClipCommand(Track *track, Clip *clip, int index,
                               QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
    , m_clip(clip)
    , m_index(index < 0 ? track->clipCount() : index)
{
    //: Undo command text for adding a clip
    setText(QObject::tr("Add clip \"%1\"").arg(clip->name()));
}

void AddClipCommand::undo()
{
    // Take the clip back out – we own it while undone
    m_track->removeClip(m_index);
    m_ownsClip = true;
}

void AddClipCommand::redo()
{
    m_track->insertClip(m_index, m_clip);
    m_ownsClip = false;
}

AddClipCommand::~AddClipCommand()
{
    if (m_ownsClip)
        delete m_clip;
}

// ===========================================================================
// RemoveClipCommand
// ===========================================================================
RemoveClipCommand::RemoveClipCommand(Track *track, int clipIndex,
                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
    , m_index(clipIndex)
{
    m_clip = track->clipAt(clipIndex);
    if (m_clip) {
        //: Undo command text for removing a clip
        setText(QObject::tr("Remove clip \"%1\"").arg(m_clip->name()));
    }
}

void RemoveClipCommand::undo()
{
    if (m_clip) {
        m_track->insertClip(m_index, m_clip);
        m_ownsClip = false;
    }
}

void RemoveClipCommand::redo()
{
    if (m_clip) {
        m_track->removeClip(m_index);
        m_ownsClip = true;
    }
}

RemoveClipCommand::~RemoveClipCommand()
{
    if (m_ownsClip)
        delete m_clip;
}

// ===========================================================================
// MoveClipCommand
// ===========================================================================
MoveClipCommand::MoveClipCommand(Track *track, int fromIndex, int toIndex,
                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
    , m_from(fromIndex)
    , m_to(toIndex)
{
    //: Undo command text for moving a clip
    setText(QObject::tr("Move clip"));
}

void MoveClipCommand::undo()
{
    m_track->moveClip(m_to, m_from);
}

void MoveClipCommand::redo()
{
    m_track->moveClip(m_from, m_to);
}

// ===========================================================================
// TrimClipCommand
// ===========================================================================
TrimClipCommand::TrimClipCommand(Clip *clip, Edge edge,
                                 const TimeCode &newPoint,
                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_edge(edge)
    , m_newPoint(newPoint)
{
    m_oldPoint = (edge == Edge::In) ? clip->inPoint() : clip->outPoint();
    //: Undo command text for trimming a clip
    setText(QObject::tr("Trim clip \"%1\"").arg(clip->name()));
}

void TrimClipCommand::undo()
{
    if (m_edge == Edge::In)
        m_clip->setInPoint(m_oldPoint);
    else
        m_clip->setOutPoint(m_oldPoint);
}

void TrimClipCommand::redo()
{
    if (m_edge == Edge::In)
        m_clip->setInPoint(m_newPoint);
    else
        m_clip->setOutPoint(m_newPoint);
}

// ===========================================================================
// SplitClipCommand
// ===========================================================================
SplitClipCommand::SplitClipCommand(Track *track, int clipIndex,
                                   const TimeCode &splitPoint,
                                   QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
    , m_clipIndex(clipIndex)
    , m_splitPoint(splitPoint)
{
    //: Undo command text for splitting a clip
    setText(QObject::tr("Split clip"));
}

void SplitClipCommand::undo()
{
    if (!m_executed) return;

    // Remove the second half (will be unparented, we keep the pointer)
    m_track->removeClip(m_clipIndex + 1);

    // Restore original out point
    auto *original = m_track->clipAt(m_clipIndex);
    if (original)
        original->setOutPoint(m_originalOut);

    m_executed = false;
}

SplitClipCommand::~SplitClipCommand()
{
    // If undone (m_executed == false) and we created a new clip, we own it
    if (!m_executed)
        delete m_newClip;
}

void SplitClipCommand::redo()
{
    auto *original = m_track->clipAt(m_clipIndex);
    if (!original) return;

    if (!m_newClip) {
        // First redo — capture state and create the split clip
        m_originalOut = original->outPoint();

        // Compute source-relative split point:
        // The split point is a timeline position; convert to source offset
        const int64_t timelineOffset =
            m_splitPoint.frame() - original->timelinePosition().frame();
        const double fps = m_splitPoint.fps();
        TimeCode sourceSplit(original->inPoint().frame() + timelineOffset, fps);

        m_newClip = new Clip(
            original->name() + QObject::tr(" (split)"),
            original->sourcePath(),
            sourceSplit,
            m_originalOut);

        // Deep-copy effects from original to new clip
        for (auto *fx : original->effects()) {
            auto *copy = new Effect(fx->serviceId(), fx->displayName(),
                                    fx->description(), m_newClip);
            for (const auto &p : fx->parameters())
                copy->addParameter(p);
            copy->setEnabled(fx->isEnabled());
            m_newClip->addEffect(copy);
        }

        // Move the original's outTransition to the new clip
        if (original->outTransition()) {
            m_newClip->setOutTransition(original->outTransition());
            original->setOutTransition(nullptr);
        }
    } else {
        // Subsequent redo — just recapture the originalOut
        m_originalOut = original->outPoint();
    }

    // Place the new clip at the split position on the timeline
    m_newClip->setTimelinePosition(m_splitPoint);

    // Compute source-relative split for trimming the original
    const int64_t timelineOffset =
        m_splitPoint.frame() - original->timelinePosition().frame();
    const double fps = m_splitPoint.fps();
    TimeCode sourceSplit(original->inPoint().frame() + timelineOffset, fps);

    // Trim original to end at the source split point
    original->setOutPoint(sourceSplit);

    // Insert new clip after the original
    m_track->insertClip(m_clipIndex + 1, m_newClip);

    m_executed = true;
}

// ===========================================================================
// AddTrackCommand
// ===========================================================================
AddTrackCommand::AddTrackCommand(Timeline *timeline, Track *track, int index,
                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_timeline(timeline)
    , m_track(track)
    , m_index(index < 0 ? timeline->trackCount() : index)
{
    //: Undo command text for adding a track
    setText(QObject::tr("Add track \"%1\"").arg(track->name()));
}

void AddTrackCommand::undo()
{
    m_timeline->removeTrack(m_index);
    m_ownsTrack = true;
}

void AddTrackCommand::redo()
{
    m_timeline->insertTrack(m_index, m_track);
    m_ownsTrack = false;
}

AddTrackCommand::~AddTrackCommand()
{
    if (m_ownsTrack)
        delete m_track;
}

// ===========================================================================
// RemoveTrackCommand
// ===========================================================================
RemoveTrackCommand::RemoveTrackCommand(Timeline *timeline, int trackIndex,
                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_timeline(timeline)
    , m_index(trackIndex)
{
    m_track = timeline->trackAt(trackIndex);
    if (m_track) {
        //: Undo command text for removing a track
        setText(QObject::tr("Remove track \"%1\"").arg(m_track->name()));
    }
}

void RemoveTrackCommand::undo()
{
    if (m_track) {
        m_timeline->insertTrack(m_index, m_track);
        m_ownsTrack = false;
    }
}

void RemoveTrackCommand::redo()
{
    if (m_track) {
        m_timeline->removeTrack(m_index);
        m_ownsTrack = true;
    }
}

RemoveTrackCommand::~RemoveTrackCommand()
{
    if (m_ownsTrack)
        delete m_track;
}

// ===========================================================================
// AddEffectCommand
// ===========================================================================
AddEffectCommand::AddEffectCommand(Clip *clip, Effect *effect,
                                   QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_effect(effect)
{
    //: Undo command text for adding an effect
    setText(QObject::tr("Add effect \"%1\"").arg(effect->displayName()));
}

void AddEffectCommand::undo()
{
    const auto &effects = m_clip->effects();
    const int idx = effects.indexOf(m_effect);
    if (idx >= 0) {
        m_clip->removeEffect(idx);
        m_ownsEffect = true;
    }
}

void AddEffectCommand::redo()
{
    m_clip->addEffect(m_effect);
    m_ownsEffect = false;
}

AddEffectCommand::~AddEffectCommand()
{
    if (m_ownsEffect)
        delete m_effect;
}

// ===========================================================================
// RemoveEffectCommand
// ===========================================================================
RemoveEffectCommand::RemoveEffectCommand(Clip *clip, int effectIndex,
                                         QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_index(effectIndex)
{
    if (effectIndex >= 0 && effectIndex < clip->effects().size()) {
        m_effect = clip->effects().at(effectIndex);
        //: Undo command text for removing an effect
        setText(QObject::tr("Remove effect \"%1\"").arg(m_effect->displayName()));
    }
}

void RemoveEffectCommand::undo()
{
    if (m_effect) {
        m_clip->insertEffect(m_index, m_effect);
        m_ownsEffect = false;
    }
}

void RemoveEffectCommand::redo()
{
    if (m_effect) {
        m_clip->removeEffect(m_index);
        m_ownsEffect = true;
    }
}

RemoveEffectCommand::~RemoveEffectCommand()
{
    if (m_ownsEffect)
        delete m_effect;
}

// ===========================================================================
// AddMarkerCommand
// ===========================================================================
AddMarkerCommand::AddMarkerCommand(Timeline *timeline, const QString &name,
                                   const TimeCode &position,
                                   const QString &comment,
                                   QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_timeline(timeline)
    , m_name(name)
    , m_position(position)
    , m_comment(comment)
{
    //: Undo command text for adding a marker
    setText(QObject::tr("Add marker \"%1\"").arg(name));
}

void AddMarkerCommand::undo()
{
    if (m_insertedIndex >= 0) {
        m_timeline->removeMarker(m_insertedIndex);
        // m_marker is now unparented and owned by this command
        m_ownsMarker = true;
    }
}

void AddMarkerCommand::redo()
{
    if (!m_marker)
        m_marker = new Marker(m_name, m_position, m_comment);
    m_timeline->addMarker(m_marker);
    m_insertedIndex = m_timeline->markers().size() - 1;
    m_ownsMarker = false;
}

AddMarkerCommand::~AddMarkerCommand()
{
    if (m_ownsMarker)
        delete m_marker;
}

// ===========================================================================
// RemoveMarkerCommand
// ===========================================================================
RemoveMarkerCommand::RemoveMarkerCommand(Timeline *timeline, int markerIndex,
                                         QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_timeline(timeline)
    , m_index(markerIndex)
{
    auto *marker = (markerIndex >= 0 && markerIndex < timeline->markers().size())
                       ? timeline->markers().at(markerIndex) : nullptr;
    if (marker) {
        m_name     = marker->name();
        m_position = marker->position();
        m_comment  = marker->comment();
        //: Undo command text for removing a marker
        setText(QObject::tr("Remove marker \"%1\"").arg(m_name));
    }
}

void RemoveMarkerCommand::undo()
{
    if (!m_marker)
        m_marker = new Marker(m_name, m_position, m_comment);
    m_timeline->insertMarker(m_index, m_marker);
    m_ownsMarker = false;
}

void RemoveMarkerCommand::redo()
{
    m_timeline->removeMarker(m_index);
    // m_marker is now unparented and owned by this command
    m_ownsMarker = true;
}

RemoveMarkerCommand::~RemoveMarkerCommand()
{
    if (m_ownsMarker)
        delete m_marker;
}

// ===========================================================================
// AddTransitionCommand
// ===========================================================================
AddTransitionCommand::AddTransitionCommand(Clip *clip, Edge edge,
                                           Transition *transition,
                                           QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_edge(edge)
    , m_transition(transition)
{
    setText(QObject::tr("Add transition \"%1\"")
                .arg(transition ? transition->displayName()
                                : QStringLiteral("?")));
}

void AddTransitionCommand::undo()
{
    if (m_edge == Edge::In)
        m_clip->setInTransition(m_oldTransition);
    else
        m_clip->setOutTransition(m_oldTransition);
    m_ownsTransition = true;
}

void AddTransitionCommand::redo()
{
    m_oldTransition = (m_edge == Edge::In) ? m_clip->inTransition()
                                           : m_clip->outTransition();
    if (m_edge == Edge::In)
        m_clip->setInTransition(m_transition);
    else
        m_clip->setOutTransition(m_transition);
    m_ownsTransition = false;
}

AddTransitionCommand::~AddTransitionCommand()
{
    if (m_ownsTransition)
        delete m_transition;
}

// ===========================================================================
// RemoveTransitionCommand
// ===========================================================================
RemoveTransitionCommand::RemoveTransitionCommand(Clip *clip, Edge edge,
                                                 QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_edge(edge)
{
    auto *t = (edge == Edge::In) ? clip->inTransition() : clip->outTransition();
    setText(QObject::tr("Remove transition \"%1\"")
                .arg(t ? t->displayName() : QStringLiteral("?")));
}

void RemoveTransitionCommand::undo()
{
    if (m_edge == Edge::In)
        m_clip->setInTransition(m_transition);
    else
        m_clip->setOutTransition(m_transition);
    m_ownsTransition = false;
}

void RemoveTransitionCommand::redo()
{
    m_transition = (m_edge == Edge::In) ? m_clip->inTransition()
                                        : m_clip->outTransition();
    if (m_edge == Edge::In)
        m_clip->setInTransition(nullptr);
    else
        m_clip->setOutTransition(nullptr);
    m_ownsTransition = true;
}

RemoveTransitionCommand::~RemoveTransitionCommand()
{
    if (m_ownsTransition)
        delete m_transition;
}

// ===========================================================================
// RenameClipCommand
// ===========================================================================
RenameClipCommand::RenameClipCommand(Clip *clip, const QString &newName,
                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_oldName(clip->name())
    , m_newName(newName)
{
    setText(QObject::tr("Rename clip \"%1\"").arg(newName));
}

void RenameClipCommand::undo()  { m_clip->setName(m_oldName); }
void RenameClipCommand::redo()  { m_clip->setName(m_newName); }

// ===========================================================================
// ChangeClipDescriptionCommand
// ===========================================================================
ChangeClipDescriptionCommand::ChangeClipDescriptionCommand(
        Clip *clip, const QString &newDesc, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_oldDesc(clip->description())
    , m_newDesc(newDesc)
{
    setText(QObject::tr("Edit clip description"));
}

void ChangeClipDescriptionCommand::undo()  { m_clip->setDescription(m_oldDesc); }
void ChangeClipDescriptionCommand::redo()  { m_clip->setDescription(m_newDesc); }

// ===========================================================================
// ChangeEffectParameterCommand
// ===========================================================================
ChangeEffectParameterCommand::ChangeEffectParameterCommand(
        Effect *effect, const QString &paramId,
        const QVariant &newValue, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_effect(effect)
    , m_paramId(paramId)
    , m_oldValue(effect->parameterValue(paramId))
    , m_newValue(newValue)
{
    setText(QObject::tr("Change parameter \"%1\"").arg(paramId));
}

void ChangeEffectParameterCommand::undo()
{
    m_effect->setParameterValue(m_paramId, m_oldValue);
}

void ChangeEffectParameterCommand::redo()
{
    m_effect->setParameterValue(m_paramId, m_newValue);
}

int ChangeEffectParameterCommand::id() const
{
    // Unique command type ID for merge support
    return 1001;
}

bool ChangeEffectParameterCommand::mergeWith(const QUndoCommand *other)
{
    auto *cmd = dynamic_cast<const ChangeEffectParameterCommand *>(other);
    if (!cmd) return false;
    if (cmd->m_effect != m_effect || cmd->m_paramId != m_paramId)
        return false;
    // Keep our original old value, adopt the newer new value
    m_newValue = cmd->m_newValue;
    return true;
}

// ===========================================================================
// SetEffectEnabledCommand
// ===========================================================================
SetEffectEnabledCommand::SetEffectEnabledCommand(
        Effect *effect, bool enabled, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_effect(effect)
    , m_oldEnabled(effect->isEnabled())
    , m_newEnabled(enabled)
{
    setText(enabled ? QObject::tr("Enable effect \"%1\"").arg(effect->displayName())
                    : QObject::tr("Disable effect \"%1\"").arg(effect->displayName()));
}

void SetEffectEnabledCommand::undo()  { m_effect->setEnabled(m_oldEnabled); }
void SetEffectEnabledCommand::redo()  { m_effect->setEnabled(m_newEnabled); }

// ===========================================================================
// MoveTrackCommand
// ===========================================================================
MoveTrackCommand::MoveTrackCommand(Timeline *timeline, int fromIndex,
                                   int toIndex, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_timeline(timeline)
    , m_from(fromIndex)
    , m_to(toIndex)
{
    setText(QObject::tr("Move track"));
}

void MoveTrackCommand::undo()  { m_timeline->moveTrack(m_to, m_from); }
void MoveTrackCommand::redo()  { m_timeline->moveTrack(m_from, m_to); }

// ===========================================================================
// MoveClipBetweenTracksCommand
// ===========================================================================
MoveClipBetweenTracksCommand::MoveClipBetweenTracksCommand(
        Track *srcTrack, int clipIndex,
        Track *dstTrack, int dstIndex, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_srcTrack(srcTrack)
    , m_dstTrack(dstTrack)
    , m_srcIndex(clipIndex)
    , m_dstIndex(dstIndex < 0 ? dstTrack->clipCount() : dstIndex)
{
    m_clip = srcTrack->clipAt(clipIndex);
    if (m_clip)
        setText(QObject::tr("Move clip \"%1\" between tracks").arg(m_clip->name()));
}

void MoveClipBetweenTracksCommand::undo()
{
    if (!m_clip) return;
    m_dstTrack->removeClip(m_dstIndex);
    m_srcTrack->insertClip(m_srcIndex, m_clip);
}

void MoveClipBetweenTracksCommand::redo()
{
    if (!m_clip) return;
    m_srcTrack->removeClip(m_srcIndex);
    m_dstTrack->insertClip(m_dstIndex, m_clip);
}

// ===========================================================================
// RenameTrackCommand
// ===========================================================================
RenameTrackCommand::RenameTrackCommand(Track *track, const QString &newName,
                                       QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
    , m_oldName(track->name())
    , m_newName(newName)
{
    setText(QObject::tr("Rename track \"%1\"").arg(newName));
}

void RenameTrackCommand::undo()  { m_track->setName(m_oldName); }
void RenameTrackCommand::redo()  { m_track->setName(m_newName); }

// ===========================================================================
// MoveEffectCommand
// ===========================================================================
MoveEffectCommand::MoveEffectCommand(Clip *clip, int fromIndex, int toIndex,
                                     QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_from(fromIndex)
    , m_to(toIndex)
{
    setText(QObject::tr("Reorder effect"));
}

void MoveEffectCommand::undo()  { m_clip->moveEffect(m_to, m_from); }
void MoveEffectCommand::redo()  { m_clip->moveEffect(m_from, m_to); }

// ===========================================================================
// NudgeClipPositionCommand
// ===========================================================================
NudgeClipPositionCommand::NudgeClipPositionCommand(
        Clip *clip, const TimeCode &newPosition, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_clip(clip)
    , m_oldPosition(clip->timelinePosition())
    , m_newPosition(newPosition)
{
    setText(QObject::tr("Nudge clip \"%1\"").arg(clip->name()));
}

void NudgeClipPositionCommand::undo()
{
    m_clip->setTimelinePosition(m_oldPosition);
}

void NudgeClipPositionCommand::redo()
{
    m_clip->setTimelinePosition(m_newPosition);
}

int NudgeClipPositionCommand::id() const
{
    return 1003;  // unique ID for merge support
}

bool NudgeClipPositionCommand::mergeWith(const QUndoCommand *other)
{
    auto *cmd = dynamic_cast<const NudgeClipPositionCommand *>(other);
    if (!cmd) return false;
    if (cmd->m_clip != m_clip) return false;
    m_newPosition = cmd->m_newPosition;
    return true;
}

// ===========================================================================
// ChangeTransitionDurationCommand
// ===========================================================================
ChangeTransitionDurationCommand::ChangeTransitionDurationCommand(
        Transition *transition, const TimeCode &newDuration,
        QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_transition(transition)
    , m_oldDuration(transition->duration())
    , m_newDuration(newDuration)
{
    setText(QObject::tr("Change transition duration"));
}

void ChangeTransitionDurationCommand::undo()
{
    m_transition->setDuration(m_oldDuration);
}

void ChangeTransitionDurationCommand::redo()
{
    m_transition->setDuration(m_newDuration);
}

int ChangeTransitionDurationCommand::id() const
{
    return 1002;  // unique ID for merge support
}

bool ChangeTransitionDurationCommand::mergeWith(const QUndoCommand *other)
{
    auto *cmd = dynamic_cast<const ChangeTransitionDurationCommand *>(other);
    if (!cmd) return false;
    if (cmd->m_transition != m_transition) return false;
    m_newDuration = cmd->m_newDuration;
    return true;
}

// =====================================================================
// ToggleMuteTrackCommand
// =====================================================================

ToggleMuteTrackCommand::ToggleMuteTrackCommand(Track *track,
                                                QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
{
    setText(QObject::tr("Toggle mute: %1").arg(track->name()));
}

void ToggleMuteTrackCommand::undo()
{
    m_track->setMuted(!m_track->isMuted());
}

void ToggleMuteTrackCommand::redo()
{
    m_track->setMuted(!m_track->isMuted());
}

// =====================================================================
// ToggleLockTrackCommand
// =====================================================================

ToggleLockTrackCommand::ToggleLockTrackCommand(Track *track,
                                                QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_track(track)
{
    setText(QObject::tr("Toggle lock: %1").arg(track->name()));
}

void ToggleLockTrackCommand::undo()
{
    m_track->setLocked(!m_track->isLocked());
}

void ToggleLockTrackCommand::redo()
{
    m_track->setLocked(!m_track->isLocked());
}

// =====================================================================
// SoloTrackCommand
// =====================================================================

SoloTrackCommand::SoloTrackCommand(Timeline *timeline, int soloIndex,
                                    QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_timeline(timeline)
    , m_soloIndex(soloIndex)
{
    // Capture the previous mute state of every track
    for (int i = 0; i < m_timeline->trackCount(); ++i)
        m_previousMuteStates.append(m_timeline->trackAt(i)->isMuted());

    // Determine whether the track is already soloed
    auto *cur = m_timeline->trackAt(m_soloIndex);
    bool alreadySoloed = cur && !cur->isMuted();
    for (int i = 0; alreadySoloed && i < m_timeline->trackCount(); ++i) {
        if (i != m_soloIndex && !m_timeline->trackAt(i)->isMuted())
            alreadySoloed = false;
    }

    if (alreadySoloed) {
        // Un-solo: unmute everything
        for (int i = 0; i < m_timeline->trackCount(); ++i)
            m_newMuteStates.append(false);
        setText(QObject::tr("Unsolo tracks"));
    } else {
        // Solo: mute everything except soloIndex
        for (int i = 0; i < m_timeline->trackCount(); ++i)
            m_newMuteStates.append(i != m_soloIndex);
        setText(QObject::tr("Solo track \"%1\"").arg(
            cur ? cur->name() : QString()));
    }
}

void SoloTrackCommand::undo()
{
    for (int i = 0; i < m_previousMuteStates.size()
                    && i < m_timeline->trackCount(); ++i)
        m_timeline->trackAt(i)->setMuted(m_previousMuteStates.at(i));
}

void SoloTrackCommand::redo()
{
    for (int i = 0; i < m_newMuteStates.size()
                    && i < m_timeline->trackCount(); ++i)
        m_timeline->trackAt(i)->setMuted(m_newMuteStates.at(i));
}

} // namespace Thrive
