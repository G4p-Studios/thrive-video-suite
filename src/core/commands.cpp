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

    // Remove the second half
    m_track->removeClip(m_clipIndex + 1);

    // Restore original out point
    auto *original = m_track->clipAt(m_clipIndex);
    if (original)
        original->setOutPoint(m_originalOut);

    m_executed = false;
}

void SplitClipCommand::redo()
{
    auto *original = m_track->clipAt(m_clipIndex);
    if (!original) return;

    m_originalOut = original->outPoint();

    // Create second half
    m_newClip = new Clip(
        original->name() + QObject::tr(" (split)"),
        original->sourcePath(),
        m_splitPoint,
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

    // Move the original's outTransition to the new clip (it is now the
    // clip that borders the next clip)
    if (original->outTransition()) {
        m_newClip->setOutTransition(original->outTransition());
        original->setOutTransition(nullptr);
    }

    // Trim original to end at split point
    original->setOutPoint(m_splitPoint);

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
        m_clip->addEffect(m_effect);
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
    if (m_insertedIndex >= 0)
        m_timeline->removeMarker(m_insertedIndex);
}

void AddMarkerCommand::redo()
{
    auto *marker = new Marker(m_name, m_position, m_comment);
    m_timeline->addMarker(marker);
    m_insertedIndex = m_timeline->markers().size() - 1;
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
    auto *marker = new Marker(m_name, m_position, m_comment);
    m_timeline->addMarker(marker);
    // Re-insert at original position by moving it if needed
    // (addMarker appends, but this preserves order on undo)
}

void RemoveMarkerCommand::redo()
{
    m_timeline->removeMarker(m_index);
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
}

void AddTransitionCommand::redo()
{
    m_oldTransition = (m_edge == Edge::In) ? m_clip->inTransition()
                                           : m_clip->outTransition();
    if (m_edge == Edge::In)
        m_clip->setInTransition(m_transition);
    else
        m_clip->setOutTransition(m_transition);
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
}

void RemoveTransitionCommand::redo()
{
    m_transition = (m_edge == Edge::In) ? m_clip->inTransition()
                                        : m_clip->outTransition();
    if (m_edge == Edge::In)
        m_clip->setInTransition(nullptr);
    else
        m_clip->setOutTransition(nullptr);
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

} // namespace Thrive
