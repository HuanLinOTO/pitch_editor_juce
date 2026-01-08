#include "Project.h"
#include "../Utils/Constants.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float twoPi = 6.2831853071795864769f;
}

Project::Project()
{
}

bool Project::saveToFile(const juce::File& file) const
{
    juce::XmlElement root("PitchEditorProject");
    root.setAttribute("version", 1);
    root.setAttribute("name", name);
    root.setAttribute("audioPath", filePath.getFullPathName());
    root.setAttribute("sampleRate", audioData.sampleRate);
    root.setAttribute("globalPitchOffset", globalPitchOffset);
    root.setAttribute("formantShift", formantShift);
    root.setAttribute("volume", volume);

    // Notes
    auto* notesElem = root.createNewChildElement("Notes");
    for (const auto& note : notes)
    {
        auto* n = notesElem->createNewChildElement("Note");
        n->setAttribute("startFrame", note.getStartFrame());
        n->setAttribute("endFrame", note.getEndFrame());
        n->setAttribute("midiNote", note.getMidiNote());
        n->setAttribute("pitchOffset", note.getPitchOffset());

        n->setAttribute("vibratoEnabled", note.isVibratoEnabled() ? 1 : 0);
        n->setAttribute("vibratoRateHz", note.getVibratoRateHz());
        n->setAttribute("vibratoDepthSemitones", note.getVibratoDepthSemitones());
        n->setAttribute("vibratoPhaseRadians", note.getVibratoPhaseRadians());
    }

    // F0
    auto* f0Elem = root.createNewChildElement("F0");
    {
        juce::StringArray parts;
        parts.ensureStorageAllocated(static_cast<int>(audioData.f0.size()));
        for (float v : audioData.f0)
            parts.add(juce::String(v, 6));
        f0Elem->addTextElement(parts.joinIntoString(" "));
    }

    // VoicedMask
    auto* voicedElem = root.createNewChildElement("VoicedMask");
    {
        juce::String mask;
        mask.preallocateBytes(static_cast<size_t>(audioData.voicedMask.size()));
        for (bool b : audioData.voicedMask)
            mask << (b ? '1' : '0');
        voicedElem->addTextElement(mask);
    }

    const bool ok = root.writeTo(file);
    return ok;
}

Note* Project::getNoteAtFrame(int frame)
{
    for (auto& note : notes)
    {
        if (note.containsFrame(frame))
            return &note;
    }
    return nullptr;
}

std::vector<Note*> Project::getNotesInRange(int startFrame, int endFrame)
{
    std::vector<Note*> result;
    for (auto& note : notes)
    {
        if (note.getStartFrame() < endFrame && note.getEndFrame() > startFrame)
            result.push_back(&note);
    }
    return result;
}

std::vector<Note*> Project::getSelectedNotes()
{
    std::vector<Note*> result;
    for (auto& note : notes)
    {
        if (note.isSelected())
            result.push_back(&note);
    }
    return result;
}

void Project::deselectAllNotes()
{
    for (auto& note : notes)
        note.setSelected(false);
}

std::vector<Note*> Project::getDirtyNotes()
{
    std::vector<Note*> result;
    for (auto& note : notes)
    {
        if (note.isDirty())
            result.push_back(&note);
    }
    return result;
}

void Project::clearAllDirty()
{
    for (auto& note : notes)
        note.clearDirty();
    // Also clear F0 dirty range
    f0DirtyStart = -1;
    f0DirtyEnd = -1;
}

bool Project::hasDirtyNotes() const
{
    for (const auto& note : notes)
    {
        if (note.isDirty())
            return true;
    }
    return false;
}

void Project::setF0DirtyRange(int startFrame, int endFrame)
{
    if (f0DirtyStart < 0 || startFrame < f0DirtyStart)
        f0DirtyStart = startFrame;
    if (f0DirtyEnd < 0 || endFrame > f0DirtyEnd)
        f0DirtyEnd = endFrame;
}

void Project::clearF0DirtyRange()
{
    f0DirtyStart = -1;
    f0DirtyEnd = -1;
}

bool Project::hasF0DirtyRange() const
{
    return f0DirtyStart >= 0 && f0DirtyEnd >= 0;
}

std::pair<int, int> Project::getF0DirtyRange() const
{
    return {f0DirtyStart, f0DirtyEnd};
}

std::pair<int, int> Project::getDirtyFrameRange() const
{
    int minStart = -1;
    int maxEnd = -1;
    
    // Check dirty notes
    for (const auto& note : notes)
    {
        if (note.isDirty())
        {
            if (minStart < 0 || note.getStartFrame() < minStart)
                minStart = note.getStartFrame();
            if (maxEnd < 0 || note.getEndFrame() > maxEnd)
                maxEnd = note.getEndFrame();
        }
    }
    
    // Also include F0 dirty range from Draw mode edits
    if (f0DirtyStart >= 0)
    {
        if (minStart < 0 || f0DirtyStart < minStart)
            minStart = f0DirtyStart;
    }
    if (f0DirtyEnd >= 0)
    {
        if (maxEnd < 0 || f0DirtyEnd > maxEnd)
            maxEnd = f0DirtyEnd;
    }
    
    return {minStart, maxEnd};
}

std::vector<float> Project::getAdjustedF0() const
{
    if (audioData.f0.empty())
        return {};
    
    // Start with copy of original F0
    std::vector<float> adjustedF0 = audioData.f0;
    
    // Apply global pitch offset
    if (globalPitchOffset != 0.0f)
    {
        float globalRatio = std::pow(2.0f, globalPitchOffset / 12.0f);
        for (auto& f : adjustedF0)
        {
            if (f > 0.0f)
                f *= globalRatio;
        }
    }
    
    // Calculate per-frame ratios from notes
    std::vector<float> frameRatios(adjustedF0.size(), 1.0f);
    
    for (const auto& note : notes)
    {
        const bool hasPitchOffset = std::abs(note.getPitchOffset()) > 0.0001f;
        const bool hasVibrato = note.isVibratoEnabled() && note.getVibratoDepthSemitones() > 0.0001f && note.getVibratoRateHz() > 0.0001f;

        if (hasPitchOffset || hasVibrato)
        {
            int start = note.getStartFrame();
            int end = std::min(note.getEndFrame(), static_cast<int>(adjustedF0.size()));

            for (int i = start; i < end; ++i)
            {
                float ratio = 1.0f;
                if (hasPitchOffset)
                    ratio *= std::pow(2.0f, note.getPitchOffset() / 12.0f);

                if (hasVibrato)
                {
                    const float t = framesToSeconds(i - start);
                    const float vib = note.getVibratoDepthSemitones() * std::sin(twoPi * note.getVibratoRateHz() * t + note.getVibratoPhaseRadians());
                    ratio *= std::pow(2.0f, vib / 12.0f);
                }

                frameRatios[i] = ratio;
            }
        }
    }
    
    // Apply smoothing at transitions
    const int smoothFrames = 5;
    
    // Find transition points
    for (size_t i = 1; i < frameRatios.size(); ++i)
    {
        if (std::abs(frameRatios[i] - frameRatios[i-1]) > 0.001f)
        {
            // Smooth transition
            int startIdx = std::max(0, static_cast<int>(i) - smoothFrames / 2);
            int endIdx = std::min(static_cast<int>(frameRatios.size()), 
                                  static_cast<int>(i) + smoothFrames / 2 + 2);
            
            if (endIdx - startIdx > 1)
            {
                float valBefore = frameRatios[startIdx];
                float valAfter = frameRatios[endIdx - 1];
                
                for (int j = startIdx; j < endIdx; ++j)
                {
                    float t = static_cast<float>(j - startIdx) / (endIdx - startIdx - 1);
                    frameRatios[j] = valBefore + t * (valAfter - valBefore);
                }
            }
        }
    }
    
    // Apply ratios only to voiced regions
    for (size_t i = 0; i < adjustedF0.size(); ++i)
    {
        if (i < audioData.voicedMask.size() && audioData.voicedMask[i])
        {
            adjustedF0[i] *= frameRatios[i];
        }
    }
    
    return adjustedF0;
}

std::vector<float> Project::getAdjustedF0ForRange(int startFrame, int endFrame) const
{
    if (audioData.f0.empty())
        return {};
    
    // Clamp range
    startFrame = std::max(0, startFrame);
    endFrame = std::min(endFrame, static_cast<int>(audioData.f0.size()));
    
    if (startFrame >= endFrame)
        return {};
    
    int rangeSize = endFrame - startFrame;
    
    // Get slice of original F0
    std::vector<float> adjustedF0(audioData.f0.begin() + startFrame, 
                                   audioData.f0.begin() + endFrame);
    
    // Apply global pitch offset
    if (globalPitchOffset != 0.0f)
    {
        float globalRatio = std::pow(2.0f, globalPitchOffset / 12.0f);
        for (auto& f : adjustedF0)
        {
            if (f > 0.0f)
                f *= globalRatio;
        }
    }
    
    // Calculate per-frame ratios from notes (for the range)
    std::vector<float> frameRatios(rangeSize, 1.0f);
    
    for (const auto& note : notes)
    {
        const bool hasPitchOffset = std::abs(note.getPitchOffset()) > 0.0001f;
        const bool hasVibrato = note.isVibratoEnabled() && note.getVibratoDepthSemitones() > 0.0001f && note.getVibratoRateHz() > 0.0001f;

        if (hasPitchOffset || hasVibrato)
        {
            int noteStart = note.getStartFrame();
            int noteEnd = note.getEndFrame();
            
            // Calculate overlap with our range
            int overlapStart = std::max(noteStart, startFrame) - startFrame;
            int overlapEnd = std::min(noteEnd, endFrame) - startFrame;
            
            for (int i = overlapStart; i < overlapEnd; ++i)
            {
                if (i >= 0 && i < rangeSize)
                {
                    const int globalFrame = startFrame + i;
                    float ratio = 1.0f;

                    if (hasPitchOffset)
                        ratio *= std::pow(2.0f, note.getPitchOffset() / 12.0f);

                    if (hasVibrato)
                    {
                        const float t = framesToSeconds(globalFrame - noteStart);
                        const float vib = note.getVibratoDepthSemitones() * std::sin(twoPi * note.getVibratoRateHz() * t + note.getVibratoPhaseRadians());
                        ratio *= std::pow(2.0f, vib / 12.0f);
                    }

                    frameRatios[i] = ratio;
                }
            }
        }
    }
    
    // Apply smoothing at transitions (simplified for range)
    const int smoothFrames = 5;
    
    for (int i = 1; i < rangeSize; ++i)
    {
        if (std::abs(frameRatios[i] - frameRatios[i-1]) > 0.001f)
        {
            int startIdx = std::max(0, i - smoothFrames / 2);
            int endIdx = std::min(rangeSize, i + smoothFrames / 2 + 2);
            
            if (endIdx - startIdx > 1)
            {
                float valBefore = frameRatios[startIdx];
                float valAfter = frameRatios[endIdx - 1];
                
                for (int j = startIdx; j < endIdx; ++j)
                {
                    float t = static_cast<float>(j - startIdx) / (endIdx - startIdx - 1);
                    frameRatios[j] = valBefore + t * (valAfter - valBefore);
                }
            }
        }
    }
    
    // Apply ratios only to voiced regions
    for (int i = 0; i < rangeSize; ++i)
    {
        size_t globalIdx = static_cast<size_t>(startFrame + i);
        if (globalIdx < audioData.voicedMask.size() && audioData.voicedMask[globalIdx])
        {
            adjustedF0[i] *= frameRatios[i];
        }
    }
    
    return adjustedF0;
}
