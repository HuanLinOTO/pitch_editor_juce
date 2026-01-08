#include "ToolbarComponent.h"
#include "PianoRollComponent.h"  // For EditMode enum

ToolbarComponent::ToolbarComponent()
{
    // Configure buttons
    addAndMakeVisible(openButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(resynthButton);
    addAndMakeVisible(settingsButton);
    addAndMakeVisible(selectModeButton);
    addAndMakeVisible(drawModeButton);
    
    openButton.addListener(this);
    exportButton.addListener(this);
    playButton.addListener(this);
    stopButton.addListener(this);
    resynthButton.addListener(this);
    settingsButton.addListener(this);
    selectModeButton.addListener(this);
    drawModeButton.addListener(this);
    
    // Style buttons
    auto buttonColor = juce::Colour(0xFF3D3D47);
    auto textColor = juce::Colours::white;
    
    for (auto* btn : { &openButton, &exportButton, &playButton, &stopButton, 
                       &resynthButton, &settingsButton, &selectModeButton, &drawModeButton })
    {
        btn->setColour(juce::TextButton::buttonColourId, buttonColor);
        btn->setColour(juce::TextButton::textColourOffId, textColor);
    }
    
    // Highlight select mode as default active
    selectModeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(COLOR_PRIMARY));
    
    // Time label
    addAndMakeVisible(timeLabel);
    timeLabel.setText("00:00.000 / 00:00.000", juce::dontSendNotification);
    timeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    timeLabel.setJustificationType(juce::Justification::centred);
    
    // Zoom slider
    addAndMakeVisible(zoomLabel);
    addAndMakeVisible(zoomSlider);
    
    zoomLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    
    zoomSlider.setRange(MIN_PIXELS_PER_SECOND, MAX_PIXELS_PER_SECOND, 1.0);
    zoomSlider.setValue(100.0);
    zoomSlider.setSkewFactorFromMidPoint(200.0);
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.addListener(this);
    
    zoomSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xFF3D3D47));
    zoomSlider.setColour(juce::Slider::thumbColourId, juce::Colour(COLOR_PRIMARY));
    
    // Progress bar (hidden by default)
    addChildComponent(progressBar);
    addChildComponent(progressLabel);
    
    progressLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    progressLabel.setJustificationType(juce::Justification::centredLeft);
    progressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour(COLOR_PRIMARY));
    progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xFF2D2D37));
}

ToolbarComponent::~ToolbarComponent()
{
}

void ToolbarComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1A1A24));
    
    // Bottom border
    g.setColour(juce::Colour(0xFF3D3D47));
    g.drawHorizontalLine(getHeight() - 1, 0, static_cast<float>(getWidth()));
}

void ToolbarComponent::resized()
{
    auto bounds = getLocalBounds().reduced(8, 4);
    
    // Left side - file operations
    openButton.setBounds(bounds.removeFromLeft(70));
    bounds.removeFromLeft(4);
    exportButton.setBounds(bounds.removeFromLeft(70));
    bounds.removeFromLeft(20);
    
    // Center - playback controls
    playButton.setBounds(bounds.removeFromLeft(70));
    bounds.removeFromLeft(4);
    stopButton.setBounds(bounds.removeFromLeft(70));
    bounds.removeFromLeft(4);
    resynthButton.setBounds(bounds.removeFromLeft(80));
    bounds.removeFromLeft(20);
    
    // Edit mode buttons
    selectModeButton.setBounds(bounds.removeFromLeft(60));
    bounds.removeFromLeft(4);
    drawModeButton.setBounds(bounds.removeFromLeft(60));
    bounds.removeFromLeft(20);
    
    // Time display
    timeLabel.setBounds(bounds.removeFromLeft(180));
    bounds.removeFromLeft(20);
    
    // Right side - settings and zoom
    settingsButton.setBounds(bounds.removeFromRight(80));
    bounds.removeFromRight(10);
    zoomLabel.setBounds(bounds.removeFromRight(50));
    bounds.removeFromRight(4);
    zoomSlider.setBounds(bounds.removeFromRight(150));
    
    // Progress bar (use the remaining middle area so it won't cover buttons)
    if (showingProgress)
    {
        auto progressArea = bounds;
        if (progressArea.getWidth() < 220)
            progressArea = getLocalBounds().reduced(200, 6);

        const int labelWidth = std::min(160, std::max(80, progressArea.getWidth() / 4));
        progressLabel.setBounds(progressArea.removeFromLeft(labelWidth));
        progressBar.setBounds(progressArea);
    }
}

void ToolbarComponent::buttonClicked(juce::Button* button)
{
    if (button == &openButton && onOpenFile)
        onOpenFile();
    else if (button == &exportButton && onExportFile)
        onExportFile();
    else if (button == &playButton)
    {
        if (isPlaying)
        {
            if (onPause)
                onPause();
        }
        else
        {
            if (onPlay)
                onPlay();
        }
    }
    else if (button == &stopButton && onStop)
        onStop();
    else if (button == &resynthButton && onResynthesize)
        onResynthesize();
    else if (button == &settingsButton && onSettings)
        onSettings();
    else if (button == &selectModeButton)
    {
        setEditMode(EditMode::Select);
        if (onEditModeChanged)
            onEditModeChanged(EditMode::Select);
    }
    else if (button == &drawModeButton)
    {
        setEditMode(EditMode::Draw);
        if (onEditModeChanged)
            onEditModeChanged(EditMode::Draw);
    }
}

void ToolbarComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &zoomSlider && onZoomChanged)
        onZoomChanged(static_cast<float>(slider->getValue()));
}

void ToolbarComponent::setPlaying(bool playing)
{
    isPlaying = playing;
    playButton.setButtonText(playing ? "Pause" : "Play");
}

void ToolbarComponent::setCurrentTime(double time)
{
    currentTime = time;
    updateTimeDisplay();
}

void ToolbarComponent::setTotalTime(double time)
{
    totalTime = time;
    updateTimeDisplay();
}

void ToolbarComponent::setEditMode(EditMode mode)
{
    currentEditModeInt = (mode == EditMode::Draw) ? 1 : 0;
    
    auto buttonColor = juce::Colour(0xFF3D3D47);
    auto activeColor = juce::Colour(COLOR_PRIMARY);
    
    selectModeButton.setColour(juce::TextButton::buttonColourId, 
        mode == EditMode::Select ? activeColor : buttonColor);
    drawModeButton.setColour(juce::TextButton::buttonColourId, 
        mode == EditMode::Draw ? activeColor : buttonColor);
    
    repaint();
}

void ToolbarComponent::setZoom(float pixelsPerSecond)
{
    // Update slider without triggering callback
    zoomSlider.setValue(pixelsPerSecond, juce::dontSendNotification);
}

void ToolbarComponent::showProgress(const juce::String& message)
{
    showingProgress = true;
    progressLabel.setText(message, juce::dontSendNotification);
    progressLabel.setVisible(true);
    progressBar.setVisible(true);
    progressValue = -1.0;  // Indeterminate
    resized();
    repaint();
}

void ToolbarComponent::hideProgress()
{
    showingProgress = false;
    progressLabel.setVisible(false);
    progressBar.setVisible(false);
    resized();
    repaint();
}

void ToolbarComponent::setProgress(float progress)
{
    if (progress < 0)
        progressValue = -1.0;  // Indeterminate
    else
        progressValue = static_cast<double>(juce::jlimit(0.0f, 1.0f, progress));
}

void ToolbarComponent::updateTimeDisplay()
{
    timeLabel.setText(formatTime(currentTime) + " / " + formatTime(totalTime),
                      juce::dontSendNotification);
}

juce::String ToolbarComponent::formatTime(double seconds)
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int ms = static_cast<int>((seconds - std::floor(seconds)) * 1000);
    
    return juce::String::formatted("%02d:%02d.%03d", mins, secs, ms);
}
