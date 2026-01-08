#include "PianoRollComponent.h"
#include <cmath>
#include <limits>

PianoRollComponent::PianoRollComponent()
{
    addAndMakeVisible(horizontalScrollBar);
    addAndMakeVisible(verticalScrollBar);
    
    horizontalScrollBar.addListener(this);
    verticalScrollBar.addListener(this);
    
    // Set initial scroll range
    verticalScrollBar.setRangeLimits(0, (MAX_MIDI_NOTE - MIN_MIDI_NOTE) * pixelsPerSemitone);
    verticalScrollBar.setCurrentRange(0, 500);
}

PianoRollComponent::~PianoRollComponent()
{
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(COLOR_BACKGROUND));
    
    // Create clipping region for main area
    auto mainArea = getLocalBounds()
        .withTrimmedLeft(pianoKeysWidth)
        .withTrimmedBottom(14)
        .withTrimmedRight(14);
    
    {
        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(mainArea);
        g.setOrigin(pianoKeysWidth - static_cast<int>(scrollX), -static_cast<int>(scrollY));
        
        drawGrid(g);
        drawNotes(g);
        drawPitchCurves(g);
        drawCursor(g);
    }
    
    // Draw piano keys
    drawPianoKeys(g);
}

void PianoRollComponent::resized()
{
    auto bounds = getLocalBounds();
    
    horizontalScrollBar.setBounds(
        pianoKeysWidth, bounds.getHeight() - 14,
        bounds.getWidth() - pianoKeysWidth - 14, 14);
    
    verticalScrollBar.setBounds(
        bounds.getWidth() - 14, 0,
        14, bounds.getHeight() - 14);
    
    updateScrollBars();
}

void PianoRollComponent::drawGrid(juce::Graphics& g)
{
    if (!project) return;
    
    float duration = project->getAudioData().getDuration();
    float width = duration * pixelsPerSecond;
    float height = (MAX_MIDI_NOTE - MIN_MIDI_NOTE) * pixelsPerSemitone;
    
    // Horizontal lines (pitch)
    g.setColour(juce::Colour(COLOR_GRID));
    
    for (int midi = MIN_MIDI_NOTE; midi <= MAX_MIDI_NOTE; ++midi)
    {
        float y = midiToY(static_cast<float>(midi));
        int noteInOctave = midi % 12;
        
        if (noteInOctave == 0)  // C
        {
            g.setColour(juce::Colour(COLOR_GRID_BAR));
            g.drawHorizontalLine(static_cast<int>(y), 0, width);
            g.setColour(juce::Colour(COLOR_GRID));
        }
        else
        {
            g.drawHorizontalLine(static_cast<int>(y), 0, width);
        }
    }
    
    // Vertical lines (time)
    float secondsPerBeat = 60.0f / 120.0f;  // Assuming 120 BPM
    float pixelsPerBeat = secondsPerBeat * pixelsPerSecond;
    
    for (float x = 0; x < width; x += pixelsPerBeat)
    {
        g.setColour(juce::Colour(COLOR_GRID));
        g.drawVerticalLine(static_cast<int>(x), 0, height);
    }
}

void PianoRollComponent::drawNotes(juce::Graphics& g)
{
    if (!project) return;
    
    for (auto& note : project->getNotes())
    {
        float x = framesToSeconds(note.getStartFrame()) * pixelsPerSecond;
        float w = framesToSeconds(note.getDurationFrames()) * pixelsPerSecond;
        float y = midiToY(note.getAdjustedMidiNote());
        float h = pixelsPerSemitone;
        
        // Note color
        juce::Colour noteColor = note.isSelected() 
            ? juce::Colour(COLOR_NOTE_SELECTED) 
            : getNoteColor(static_cast<int>(note.getAdjustedMidiNote()));
        
        // Draw note rectangle
        g.setColour(noteColor.withAlpha(0.8f));
        g.fillRoundedRectangle(x, y, w, h, 3.0f);
        
        // Border
        g.setColour(noteColor.brighter(0.3f));
        g.drawRoundedRectangle(x, y, w, h, 3.0f, 1.5f);

        // Vibrato overlay (sine curve) - only when enabled
        if (note.isVibratoEnabled() && note.getVibratoDepthSemitones() > 0.0001f && note.getVibratoRateHz() > 0.0001f)
        {
            constexpr float twoPi = 6.2831853071795864769f;
            juce::Path vib;

            const float startSec = framesToSeconds(note.getStartFrame());
            const float endSec = framesToSeconds(note.getEndFrame());
            const float baseMidi = note.getAdjustedMidiNote();
            const float rate = note.getVibratoRateHz();
            const float depth = note.getVibratoDepthSemitones();
            const float phase = note.getVibratoPhaseRadians();

            const float stepPx = 2.0f;
            const int steps = std::max(2, static_cast<int>(w / stepPx));

            for (int s = 0; s <= steps; ++s)
            {
                const float t01 = static_cast<float>(s) / static_cast<float>(steps);
                const float sec = startSec + (endSec - startSec) * t01;
                const float rel = sec - startSec;
                const float vibSemi = depth * std::sin(twoPi * rate * rel + phase);
                const float midi = baseMidi + vibSemi;

                const float vx = sec * pixelsPerSecond;
                const float vy = midiToY(midi) + h * 0.5f;

                if (s == 0)
                    vib.startNewSubPath(vx, vy);
                else
                    vib.lineTo(vx, vy);
            }

            g.setColour(juce::Colour(COLOR_NOTE_SELECTED).withAlpha(0.9f));
            g.strokePath(vib, juce::PathStrokeType(2.0f));
        }
    }
}

void PianoRollComponent::drawPitchCurves(juce::Graphics& g)
{
    if (!project) return;
    
    const auto& audioData = project->getAudioData();
    if (audioData.f0.empty()) return;
    
    // Get global pitch offset
    float globalOffset = project->getGlobalPitchOffset();
    
    // Draw pitch curves per note with their pitch offsets applied
    g.setColour(juce::Colour(COLOR_PITCH_CURVE));
    
    for (const auto& note : project->getNotes())
    {
        juce::Path path;
        bool pathStarted = false;
        
        // Get pitch offset for this note
        float noteOffset = note.getPitchOffset() + globalOffset;
        
        int startFrame = note.getStartFrame();
        int endFrame = std::min(note.getEndFrame(), static_cast<int>(audioData.f0.size()));
        
        for (int i = startFrame; i < endFrame; ++i)
        {
            float f0 = audioData.f0[i];
            
            if (f0 > 0.0f && i < static_cast<int>(audioData.voicedMask.size()) && audioData.voicedMask[i])
            {
                // Apply pitch offset to the displayed F0
                float adjustedF0 = f0 * std::pow(2.0f, noteOffset / 12.0f);
                float midi = freqToMidi(adjustedF0);
                float x = framesToSeconds(i) * pixelsPerSecond;
                float y = midiToY(midi);
                
                if (!pathStarted)
                {
                    path.startNewSubPath(x, y);
                    pathStarted = true;
                }
                else
                {
                    path.lineTo(x, y);
                }
            }
            else if (pathStarted)
            {
                g.strokePath(path, juce::PathStrokeType(2.0f));
                path.clear();
                pathStarted = false;
            }
        }
        
        if (pathStarted)
        {
            g.strokePath(path, juce::PathStrokeType(2.0f));
        }
    }
    
    // Also draw unassigned F0 regions (outside of notes) in a dimmer color
    g.setColour(juce::Colour(COLOR_PITCH_CURVE).withAlpha(0.3f));
    juce::Path unassignedPath;
    bool unassignedStarted = false;
    
    for (size_t i = 0; i < audioData.f0.size(); ++i)
    {
        // Check if this frame is inside any note
        bool inNote = false;
        for (const auto& note : project->getNotes())
        {
            if (i >= static_cast<size_t>(note.getStartFrame()) && 
                i < static_cast<size_t>(note.getEndFrame()))
            {
                inNote = true;
                break;
            }
        }
        
        if (!inNote)
        {
            float f0 = audioData.f0[i];
            if (f0 > 0.0f && i < audioData.voicedMask.size() && audioData.voicedMask[i])
            {
                float midi = freqToMidi(f0);
                float x = framesToSeconds(static_cast<int>(i)) * pixelsPerSecond;
                float y = midiToY(midi);
                
                if (!unassignedStarted)
                {
                    unassignedPath.startNewSubPath(x, y);
                    unassignedStarted = true;
                }
                else
                {
                    unassignedPath.lineTo(x, y);
                }
            }
            else if (unassignedStarted)
            {
                g.strokePath(unassignedPath, juce::PathStrokeType(1.0f));
                unassignedPath.clear();
                unassignedStarted = false;
            }
        }
        else if (unassignedStarted)
        {
            g.strokePath(unassignedPath, juce::PathStrokeType(1.0f));
            unassignedPath.clear();
            unassignedStarted = false;
        }
    }
    
    if (unassignedStarted)
    {
        g.strokePath(unassignedPath, juce::PathStrokeType(1.0f));
    }
}

void PianoRollComponent::drawCursor(juce::Graphics& g)
{
    float x = timeToX(cursorTime);
    float height = (MAX_MIDI_NOTE - MIN_MIDI_NOTE) * pixelsPerSemitone;
    
    g.setColour(juce::Colours::red);
    g.drawVerticalLine(static_cast<int>(x), 0, height);
}

void PianoRollComponent::drawPianoKeys(juce::Graphics& g)
{
    auto keyArea = getLocalBounds().withWidth(pianoKeysWidth).withTrimmedBottom(14);
    
    g.setColour(juce::Colour(0xFF1A1A24));
    g.fillRect(keyArea);
    
    // Draw each key
    for (int midi = MIN_MIDI_NOTE; midi <= MAX_MIDI_NOTE; ++midi)
    {
        float y = midiToY(static_cast<float>(midi)) - static_cast<float>(scrollY);
        int noteInOctave = midi % 12;
        
        // Check if it's a black key
        bool isBlack = (noteInOctave == 1 || noteInOctave == 3 || 
                        noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        
        if (isBlack)
        {
            g.setColour(juce::Colour(0xFF2D2D37));
        }
        else
        {
            g.setColour(juce::Colour(0xFF3D3D47));
        }
        
        g.fillRect(0.0f, y, static_cast<float>(pianoKeysWidth - 2), pixelsPerSemitone - 1);
        
        // Draw note name for C notes
        if (noteInOctave == 0)
        {
            int octave = midi / 12 - 1;
            g.setColour(juce::Colours::white);
            g.setFont(10.0f);
            g.drawText("C" + juce::String(octave), 2, static_cast<int>(y), 
                      pianoKeysWidth - 4, static_cast<int>(pixelsPerSemitone),
                      juce::Justification::centredLeft);
        }
    }
}

float PianoRollComponent::midiToY(float midiNote) const
{
    return (MAX_MIDI_NOTE - midiNote) * pixelsPerSemitone;
}

float PianoRollComponent::yToMidi(float y) const
{
    return MAX_MIDI_NOTE - y / pixelsPerSemitone;
}

float PianoRollComponent::timeToX(double time) const
{
    return static_cast<float>(time * pixelsPerSecond);
}

double PianoRollComponent::xToTime(float x) const
{
    return x / pixelsPerSecond;
}

void PianoRollComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!project) return;
    
    float adjustedX = e.x - pianoKeysWidth + static_cast<float>(scrollX);
    float adjustedY = e.y + static_cast<float>(scrollY);
    
    if (editMode == EditMode::Draw)
    {
        // Start drawing
        isDrawing = true;
        lastDrawX = 0.0f;
        lastDrawY = 0.0f;
        drawingChanges.clear();
        
        applyPitchDrawing(adjustedX, adjustedY);
        
        if (onPitchEdited)
            onPitchEdited();
        
        repaint();
        return;
    }
    
    // Check if clicking on a note
    Note* note = findNoteAt(adjustedX, adjustedY);
    
    if (note)
    {
        // Select note
        project->deselectAllNotes();
        note->setSelected(true);
        
        if (onNoteSelected)
            onNoteSelected(note);
        
        // Start dragging
        isDragging = true;
        draggedNote = note;
        dragStartY = static_cast<float>(e.y);
        originalPitchOffset = note->getPitchOffset();
        
        repaint();
    }
    else
    {
        // Seek to position
        double time = xToTime(adjustedX);
        cursorTime = std::max(0.0, time);
        
        if (onSeek)
            onSeek(cursorTime);
        
        project->deselectAllNotes();
        repaint();
    }
}

void PianoRollComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (editMode == EditMode::Draw && isDrawing)
    {
        float adjustedX = e.x - pianoKeysWidth + static_cast<float>(scrollX);
        float adjustedY = e.y + static_cast<float>(scrollY);
        
        applyPitchDrawing(adjustedX, adjustedY);
        
        if (onPitchEdited)
            onPitchEdited();
        
        repaint();
        return;
    }
    
    if (isDragging && draggedNote)
    {
        // Calculate pitch offset from drag
        float deltaY = dragStartY - e.y;
        float deltaSemitones = deltaY / pixelsPerSemitone;
        
        float newOffset = originalPitchOffset + deltaSemitones;
        
        // Store old offset for undo if just starting drag
        if (undoManager && std::abs(newOffset - originalPitchOffset) > 0.01f)
        {
            // Note: We'll create the undo action in mouseUp
        }
        
        draggedNote->setPitchOffset(newOffset);
        draggedNote->markDirty();  // Mark as dirty for incremental synthesis
        
        if (onPitchEdited)
            onPitchEdited();
        
        repaint();
    }
}

void PianoRollComponent::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    
    if (editMode == EditMode::Draw && isDrawing)
    {
        isDrawing = false;
        commitPitchDrawing();
        repaint();
        return;
    }
    
    if (isDragging && draggedNote)
    {
        float newOffset = draggedNote->getPitchOffset();
        
        // Create undo action if offset changed
        if (undoManager && std::abs(newOffset - originalPitchOffset) > 0.001f)
        {
            auto action = std::make_unique<PitchOffsetAction>(
                draggedNote, originalPitchOffset, newOffset);
            undoManager->addAction(std::move(action));
        }
        
        // Trigger incremental synthesis when pitch edit is finished
        if (onPitchEditFinished)
            onPitchEditFinished();
    }
    
    isDragging = false;
    draggedNote = nullptr;
}

void PianoRollComponent::mouseMove(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    // Could implement hover effects here
}

void PianoRollComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!project) return;
    
    float adjustedX = e.x - pianoKeysWidth + static_cast<float>(scrollX);
    float adjustedY = e.y + static_cast<float>(scrollY);
    
    // Check if double-clicking on a note
    Note* note = findNoteAt(adjustedX, adjustedY);
    
    if (note)
    {
        // Snap pitch offset to nearest semitone
        float currentOffset = note->getPitchOffset();
        float snappedOffset = std::round(currentOffset);
        
        if (std::abs(snappedOffset - currentOffset) > 0.001f)
        {
            // Create undo action
            if (undoManager && note)
            {
                // Store note index for safer undo (in case notes change)
                auto action = std::make_unique<PitchOffsetAction>(
                    note, currentOffset, snappedOffset);
                undoManager->addAction(std::move(action));
            }
            
            if (note) // Double-check before use
            {
                note->setPitchOffset(snappedOffset);
                note->markDirty();
            }
            
            if (onPitchEdited)
                onPitchEdited();
            
            if (onPitchEditFinished)
                onPitchEditFinished();
            
            repaint();
        }
    }
}

void PianoRollComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom
        float zoomFactor = 1.0f + wheel.deltaY * 0.1f;
        
        if (e.mods.isShiftDown())
        {
            // Vertical zoom
            setPixelsPerSemitone(pixelsPerSemitone * zoomFactor);
        }
        else
        {
            // Horizontal zoom - center on cursor position
            float newPps = pixelsPerSecond * zoomFactor;
            setPixelsPerSecond(newPps, true);
            
            // Notify parent to sync toolbar slider
            if (onZoomChanged)
                onZoomChanged(pixelsPerSecond);
        }
    }
    else
    {
        // Scroll
        if (e.mods.isShiftDown())
        {
            double newScrollX = scrollX - wheel.deltaY * 50.0;
            horizontalScrollBar.setCurrentRangeStart(newScrollX);
        }
        else
        {
            verticalScrollBar.setCurrentRangeStart(scrollY - wheel.deltaY * 50.0);
        }
    }
}

void PianoRollComponent::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart)
{
    if (scrollBar == &horizontalScrollBar)
    {
        scrollX = newRangeStart;
        
        // Notify scroll changed for synchronization
        if (onScrollChanged)
            onScrollChanged(scrollX);
    }
    else if (scrollBar == &verticalScrollBar)
    {
        scrollY = newRangeStart;
    }
    repaint();
}

void PianoRollComponent::setProject(Project* proj)
{
    project = proj;
    updateScrollBars();
    repaint();
}

void PianoRollComponent::setCursorTime(double time)
{
    cursorTime = time;
    repaint();
}

void PianoRollComponent::setPixelsPerSecond(float pps, bool centerOnCursor)
{
    float oldPps = pixelsPerSecond;
    float newPps = juce::jlimit(MIN_PIXELS_PER_SECOND, MAX_PIXELS_PER_SECOND, pps);
    
    if (std::abs(oldPps - newPps) < 0.01f)
        return;  // No significant change
    
    if (centerOnCursor)
    {
        // Calculate cursor position relative to view
        float cursorX = static_cast<float>(cursorTime * oldPps);
        float cursorRelativeX = cursorX - static_cast<float>(scrollX);
        
        // Calculate new scroll position to keep cursor at same relative position
        float newCursorX = static_cast<float>(cursorTime * newPps);
        scrollX = static_cast<double>(newCursorX - cursorRelativeX);
        scrollX = std::max(0.0, scrollX);
    }
    
    pixelsPerSecond = newPps;
    updateScrollBars();
    repaint();
    
    // Don't call onZoomChanged here to avoid infinite recursion
    // The caller is responsible for synchronizing other components
}

void PianoRollComponent::setPixelsPerSemitone(float pps)
{
    pixelsPerSemitone = juce::jlimit(MIN_PIXELS_PER_SEMITONE, MAX_PIXELS_PER_SEMITONE, pps);
    updateScrollBars();
    repaint();
}

void PianoRollComponent::setScrollX(double x)
{
    if (std::abs(scrollX - x) < 0.01)
        return;  // No significant change
    
    scrollX = x;
    horizontalScrollBar.setCurrentRangeStart(x);
    
    // Don't call onScrollChanged here to avoid infinite recursion
    // The caller is responsible for synchronizing other components
    
    repaint();
}

void PianoRollComponent::setEditMode(EditMode mode)
{
    editMode = mode;
    
    // Change cursor based on mode
    if (mode == EditMode::Draw)
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
    
    repaint();
}

Note* PianoRollComponent::findNoteAt(float x, float y)
{
    if (!project) return nullptr;
    
    for (auto& note : project->getNotes())
    {
        float noteX = framesToSeconds(note.getStartFrame()) * pixelsPerSecond;
        float noteW = framesToSeconds(note.getDurationFrames()) * pixelsPerSecond;
        float noteY = midiToY(note.getAdjustedMidiNote());
        float noteH = pixelsPerSemitone;
        
        if (x >= noteX && x < noteX + noteW && y >= noteY && y < noteY + noteH)
        {
            return &note;
        }
    }
    
    return nullptr;
}

void PianoRollComponent::updateScrollBars()
{
    if (project)
    {
        float totalWidth = project->getAudioData().getDuration() * pixelsPerSecond;
        float totalHeight = (MAX_MIDI_NOTE - MIN_MIDI_NOTE) * pixelsPerSemitone;
        
        int visibleWidth = getWidth() - pianoKeysWidth - 14;
        int visibleHeight = getHeight() - 14;
        
        horizontalScrollBar.setRangeLimits(0, totalWidth);
        horizontalScrollBar.setCurrentRange(scrollX, visibleWidth);
        
        verticalScrollBar.setRangeLimits(0, totalHeight);
        verticalScrollBar.setCurrentRange(scrollY, visibleHeight);
    }
}

void PianoRollComponent::applyPitchDrawing(float x, float y)
{
    if (!project) return;
    
    auto& audioData = project->getAudioData();
    if (audioData.f0.empty()) return;
    
    // Convert screen coordinates to time and MIDI
    double time = xToTime(x);
    float midi = yToMidi(y);
    
    // Convert MIDI to frequency
    float freq = midiToFreq(midi);
    
    // Convert time to frame index
    int frameIndex = static_cast<int>(secondsToFrames(static_cast<float>(time)));
    
    if (frameIndex >= 0 && frameIndex < static_cast<int>(audioData.f0.size()))
    {
        // Store the change for undo
        drawingChanges.push_back({frameIndex, freq});
        
        // Apply the change immediately
        audioData.f0[frameIndex] = freq;
        if (frameIndex < static_cast<int>(audioData.voicedMask.size()))
            audioData.voicedMask[frameIndex] = true;
        
        // Interpolate between last draw position and current
        if (lastDrawX > 0 && lastDrawY > 0)
        {
            double lastTime = xToTime(lastDrawX);
            int lastFrame = static_cast<int>(secondsToFrames(static_cast<float>(lastTime)));
            float lastMidi = yToMidi(lastDrawY);
            float lastFreq = midiToFreq(lastMidi);
            
            // Interpolate intermediate frames
            int startFrame = std::min(lastFrame, frameIndex);
            int endFrame = std::max(lastFrame, frameIndex);
            
            for (int f = startFrame + 1; f < endFrame; ++f)
            {
                if (f >= 0 && f < static_cast<int>(audioData.f0.size()))
                {
                    float t = static_cast<float>(f - startFrame) / static_cast<float>(endFrame - startFrame);
                    float interpFreq = lastFreq * (1.0f - t) + freq * t;
                    
                    drawingChanges.push_back({f, interpFreq});
                    audioData.f0[f] = interpFreq;
                    if (f < static_cast<int>(audioData.voicedMask.size()))
                        audioData.voicedMask[f] = true;
                }
            }
        }
        
        lastDrawX = x;
        lastDrawY = y;
    }
}

void PianoRollComponent::commitPitchDrawing()
{
    if (drawingChanges.empty()) return;
    
    // Calculate the dirty frame range from the changes
    int minFrame = std::numeric_limits<int>::max();
    int maxFrame = std::numeric_limits<int>::min();
    for (const auto& change : drawingChanges)
    {
        minFrame = std::min(minFrame, change.first);  // first = frameIndex
        maxFrame = std::max(maxFrame, change.first);
    }
    
    // Set F0 dirty range in project for incremental synthesis
    if (project && minFrame <= maxFrame)
    {
        project->setF0DirtyRange(minFrame, maxFrame);
    }
    
    // Create undo action
    if (undoManager && project)
    {
        auto& audioData = project->getAudioData();
        auto action = std::make_unique<F0EditAction>(&audioData.f0, drawingChanges);
        undoManager->addAction(std::move(action));
    }
    
    drawingChanges.clear();
    lastDrawX = 0.0f;
    lastDrawY = 0.0f;
    
    // Trigger synthesis
    if (onPitchEditFinished)
        onPitchEditFinished();
}

