// SPDX-License-Identifier: MIT
// Thrive Video Suite – Undo/redo commands

#pragma once

#include <QUndoCommand>
#include <QString>
#include <QVariant>
#include "timecode.h"

namespace Thrive {

class Timeline;
class Track;
class Clip;
class Effect;
class Marker;
class Transition;

// ---------------------------------------------------------------------------
// AddClipCommand
// ---------------------------------------------------------------------------
class AddClipCommand : public QUndoCommand
{
public:
    AddClipCommand(Track *track, Clip *clip, int index = -1,
                   QUndoCommand *parent = nullptr);
    ~AddClipCommand() override;
    void undo() override;
    void redo() override;

private:
    Track *m_track;
    Clip  *m_clip;
    int    m_index;
    bool   m_ownsClip = false;
};

// ---------------------------------------------------------------------------
// RemoveClipCommand
// ---------------------------------------------------------------------------
class RemoveClipCommand : public QUndoCommand
{
public:
    RemoveClipCommand(Track *track, int clipIndex,
                      QUndoCommand *parent = nullptr);
    ~RemoveClipCommand() override;
    void undo() override;
    void redo() override;

private:
    Track *m_track;
    Clip  *m_clip = nullptr;
    int    m_index;
    bool   m_ownsClip = false;
};

// ---------------------------------------------------------------------------
// MoveClipCommand
// ---------------------------------------------------------------------------
class MoveClipCommand : public QUndoCommand
{
public:
    MoveClipCommand(Track *track, int fromIndex, int toIndex,
                    QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Track *m_track;
    int    m_from;
    int    m_to;
};

// ---------------------------------------------------------------------------
// TrimClipCommand
// ---------------------------------------------------------------------------
class TrimClipCommand : public QUndoCommand
{
public:
    enum class Edge { In, Out };

    TrimClipCommand(Clip *clip, Edge edge, const TimeCode &newPoint,
                    QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Clip    *m_clip;
    Edge     m_edge;
    TimeCode m_oldPoint;
    TimeCode m_newPoint;
};

// ---------------------------------------------------------------------------
// SplitClipCommand – splits a clip at a given position
// ---------------------------------------------------------------------------
class SplitClipCommand : public QUndoCommand
{
public:
    SplitClipCommand(Track *track, int clipIndex, const TimeCode &splitPoint,
                     QUndoCommand *parent = nullptr);
    ~SplitClipCommand() override;
    void undo() override;
    void redo() override;

private:
    Track   *m_track;
    int      m_clipIndex;
    TimeCode m_splitPoint;
    Clip    *m_newClip = nullptr;
    TimeCode m_originalOut;
    bool     m_executed = false;
};

// ---------------------------------------------------------------------------
// AddTrackCommand
// ---------------------------------------------------------------------------
class AddTrackCommand : public QUndoCommand
{
public:
    AddTrackCommand(Timeline *timeline, Track *track, int index = -1,
                    QUndoCommand *parent = nullptr);
    ~AddTrackCommand() override;
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    Track    *m_track;
    int       m_index;
    bool      m_ownsTrack = false;
};

// ---------------------------------------------------------------------------
// RemoveTrackCommand
// ---------------------------------------------------------------------------
class RemoveTrackCommand : public QUndoCommand
{
public:
    RemoveTrackCommand(Timeline *timeline, int trackIndex,
                       QUndoCommand *parent = nullptr);
    ~RemoveTrackCommand() override;
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    Track    *m_track = nullptr;
    int       m_index;
    bool      m_ownsTrack = false;
};

// ---------------------------------------------------------------------------
// AddEffectCommand
// ---------------------------------------------------------------------------
class AddEffectCommand : public QUndoCommand
{
public:
    AddEffectCommand(Clip *clip, Effect *effect,
                     QUndoCommand *parent = nullptr);
    ~AddEffectCommand() override;
    void undo() override;
    void redo() override;

private:
    Clip   *m_clip;
    Effect *m_effect;
    bool    m_ownsEffect = false;
};

// ---------------------------------------------------------------------------
// RemoveEffectCommand
// ---------------------------------------------------------------------------
class RemoveEffectCommand : public QUndoCommand
{
public:
    RemoveEffectCommand(Clip *clip, int effectIndex,
                        QUndoCommand *parent = nullptr);
    ~RemoveEffectCommand() override;
    void undo() override;
    void redo() override;

private:
    Clip   *m_clip;
    Effect *m_effect = nullptr;
    int     m_index;
    bool    m_ownsEffect = false;
};

// ---------------------------------------------------------------------------
// AddMarkerCommand
// ---------------------------------------------------------------------------
class AddMarkerCommand : public QUndoCommand
{
public:
    AddMarkerCommand(Timeline *timeline, const QString &name,
                     const TimeCode &position, const QString &comment = {},
                     QUndoCommand *parent = nullptr);
    ~AddMarkerCommand() override;
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    QString   m_name;
    TimeCode  m_position;
    QString   m_comment;
    Marker   *m_marker = nullptr;
    int       m_insertedIndex = -1;
    bool      m_ownsMarker = false;
};

// ---------------------------------------------------------------------------
// RemoveMarkerCommand
// ---------------------------------------------------------------------------
class RemoveMarkerCommand : public QUndoCommand
{
public:
    RemoveMarkerCommand(Timeline *timeline, int markerIndex,
                        QUndoCommand *parent = nullptr);
    ~RemoveMarkerCommand() override;
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    int       m_index;
    QString   m_name;
    TimeCode  m_position;
    QString   m_comment;
    Marker   *m_marker = nullptr;
    bool      m_ownsMarker = false;
};

// ---------------------------------------------------------------------------
// AddTransitionCommand – sets a transition on a clip's in or out edge
// ---------------------------------------------------------------------------
class AddTransitionCommand : public QUndoCommand
{
public:
    enum class Edge { In, Out };

    AddTransitionCommand(Clip *clip, Edge edge, Transition *transition,
                         QUndoCommand *parent = nullptr);
    ~AddTransitionCommand() override;
    void undo() override;
    void redo() override;

private:
    Clip       *m_clip;
    Edge        m_edge;
    Transition *m_transition;
    Transition *m_oldTransition = nullptr;
    bool        m_ownsTransition = false;
};

// ---------------------------------------------------------------------------
// RemoveTransitionCommand – removes a transition from a clip edge
// ---------------------------------------------------------------------------
class RemoveTransitionCommand : public QUndoCommand
{
public:
    using Edge = AddTransitionCommand::Edge;

    RemoveTransitionCommand(Clip *clip, Edge edge,
                            QUndoCommand *parent = nullptr);
    ~RemoveTransitionCommand() override;
    void undo() override;
    void redo() override;

private:
    Clip       *m_clip;
    Edge        m_edge;
    Transition *m_transition = nullptr;
    bool        m_ownsTransition = false;
};

// ---------------------------------------------------------------------------
// RenameClipCommand – undoable clip name change
// ---------------------------------------------------------------------------
class RenameClipCommand : public QUndoCommand
{
public:
    RenameClipCommand(Clip *clip, const QString &newName,
                      QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Clip    *m_clip;
    QString  m_oldName;
    QString  m_newName;
};

// ---------------------------------------------------------------------------
// ChangeClipDescriptionCommand
// ---------------------------------------------------------------------------
class ChangeClipDescriptionCommand : public QUndoCommand
{
public:
    ChangeClipDescriptionCommand(Clip *clip, const QString &newDesc,
                                 QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Clip    *m_clip;
    QString  m_oldDesc;
    QString  m_newDesc;
};

// ---------------------------------------------------------------------------
// ChangeEffectParameterCommand – undoable effect parameter edit
// ---------------------------------------------------------------------------
class ChangeEffectParameterCommand : public QUndoCommand
{
public:
    ChangeEffectParameterCommand(Effect *effect, const QString &paramId,
                                 const QVariant &newValue,
                                 QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int  id()   const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    Effect  *m_effect;
    QString  m_paramId;
    QVariant m_oldValue;
    QVariant m_newValue;
};

// ---------------------------------------------------------------------------
// SetEffectEnabledCommand – undoable effect toggle
// ---------------------------------------------------------------------------
class SetEffectEnabledCommand : public QUndoCommand
{
public:
    SetEffectEnabledCommand(Effect *effect, bool enabled,
                            QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Effect *m_effect;
    bool    m_oldEnabled;
    bool    m_newEnabled;
};

// ---------------------------------------------------------------------------
// MoveTrackCommand – reorder tracks within the timeline
// ---------------------------------------------------------------------------
class MoveTrackCommand : public QUndoCommand
{
public:
    MoveTrackCommand(Timeline *timeline, int fromIndex, int toIndex,
                     QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    int       m_from;
    int       m_to;
};

// ---------------------------------------------------------------------------
// MoveClipBetweenTracksCommand – move a clip from one track to another
// ---------------------------------------------------------------------------
class MoveClipBetweenTracksCommand : public QUndoCommand
{
public:
    MoveClipBetweenTracksCommand(Track *srcTrack, int clipIndex,
                                 Track *dstTrack, int dstIndex = -1,
                                 QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Track *m_srcTrack;
    Track *m_dstTrack;
    Clip  *m_clip = nullptr;
    int    m_srcIndex;
    int    m_dstIndex;
};

// ---------------------------------------------------------------------------
// RenameTrackCommand – undoable track name change
// ---------------------------------------------------------------------------
class RenameTrackCommand : public QUndoCommand
{
public:
    RenameTrackCommand(Track *track, const QString &newName,
                       QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Track   *m_track;
    QString  m_oldName;
    QString  m_newName;
};

// ---------------------------------------------------------------------------
// MoveEffectCommand – reorder effects on a clip
// ---------------------------------------------------------------------------
class MoveEffectCommand : public QUndoCommand
{
public:
    MoveEffectCommand(Clip *clip, int fromIndex, int toIndex,
                      QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Clip *m_clip;
    int   m_from;
    int   m_to;
};

// ---------------------------------------------------------------------------
// NudgeClipPositionCommand – undoable small position adjustment
// ---------------------------------------------------------------------------
class NudgeClipPositionCommand : public QUndoCommand
{
public:
    NudgeClipPositionCommand(Clip *clip, const TimeCode &newPosition,
                             QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int  id()   const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    Clip    *m_clip;
    TimeCode m_oldPosition;
    TimeCode m_newPosition;
};

// ---------------------------------------------------------------------------
// ChangeTransitionDurationCommand – undoable transition duration edit
// ---------------------------------------------------------------------------
class ChangeTransitionDurationCommand : public QUndoCommand
{
public:
    ChangeTransitionDurationCommand(Transition *transition,
                                    const TimeCode &newDuration,
                                    QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;
    int  id()   const override;
    bool mergeWith(const QUndoCommand *other) override;

private:
    Transition *m_transition;
    TimeCode    m_oldDuration;
    TimeCode    m_newDuration;
};

// ---------------------------------------------------------------------------
// ToggleMuteTrackCommand – undoable track mute toggle
// ---------------------------------------------------------------------------
class ToggleMuteTrackCommand : public QUndoCommand
{
public:
    explicit ToggleMuteTrackCommand(Track *track,
                                    QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Track *m_track;
};

// ---------------------------------------------------------------------------
// ToggleLockTrackCommand – undoable track lock toggle
// ---------------------------------------------------------------------------
class ToggleLockTrackCommand : public QUndoCommand
{
public:
    explicit ToggleLockTrackCommand(Track *track,
                                    QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Track *m_track;
};

} // namespace Thrive
