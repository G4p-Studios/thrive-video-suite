// SPDX-License-Identifier: MIT
// Thrive Video Suite – Undo/redo commands

#pragma once

#include <QUndoCommand>
#include <QString>
#include "timecode.h"

namespace Thrive {

class Timeline;
class Track;
class Clip;
class Effect;

// ---------------------------------------------------------------------------
// AddClipCommand
// ---------------------------------------------------------------------------
class AddClipCommand : public QUndoCommand
{
public:
    AddClipCommand(Track *track, Clip *clip, int index = -1,
                   QUndoCommand *parent = nullptr);
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
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    QString   m_name;
    TimeCode  m_position;
    QString   m_comment;
    int       m_insertedIndex = -1;
};

// ---------------------------------------------------------------------------
// RemoveMarkerCommand
// ---------------------------------------------------------------------------
class RemoveMarkerCommand : public QUndoCommand
{
public:
    RemoveMarkerCommand(Timeline *timeline, int markerIndex,
                        QUndoCommand *parent = nullptr);
    void undo() override;
    void redo() override;

private:
    Timeline *m_timeline;
    int       m_index;
    QString   m_name;
    TimeCode  m_position;
    QString   m_comment;
};

} // namespace Thrive
