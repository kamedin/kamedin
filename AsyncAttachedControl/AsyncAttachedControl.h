#pragma once
#include "../JuceLibraryCode/JuceHeader.h"

// AsyncAttachedControl.h
// custom attachment
// changes are withheld until confirmed

class AsyncAttachedControlBase;

class AsyncAttachedControlGroup : AudioProcessorValueTreeState::Listener, AsyncUpdater
{
public:
    AsyncAttachedControlGroup (AudioProcessorValueTreeState& s, const String& id);
    virtual ~AsyncAttachedControlGroup();
    void addControl (AsyncAttachedControlBase* c);
    void removeControl (AsyncAttachedControlBase* c);
    void controlValueChanged (AsyncAttachedControlBase* c);
    void beginParameterChange();
    void endParameterChange();
    void updateValue (float value);
    float getLastValue() const;
    RangedAudioParameter* getParameter() const;

    std::function<void (const String&, float)> notifyNewValue;

private:
    void parameterChanged (const String&, float value) override;
    void handleAsyncUpdate() override;

    AudioProcessorValueTreeState& state;
    String paramID;
    bool ignoreCallbacks{};
    std::atomic<float> lastValue{};
    std::vector<AsyncAttachedControlBase*> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsyncAttachedControlGroup)
};

//==============================================================================
class AsyncAttachedControlBase
{
public:
    AsyncAttachedControlBase (AsyncAttachedControlGroup& g);
    virtual ~AsyncAttachedControlBase();
    virtual float getValue() const = 0;
    virtual void setValue (float) = 0;

    AsyncAttachedControlGroup& group;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsyncAttachedControlBase)
};

//==============================================================================
class AsyncSliderAttachment : AsyncAttachedControlBase, Slider::Listener
{
public:
    AsyncSliderAttachment (AsyncAttachedControlGroup& g, Slider& sl);
    virtual ~AsyncSliderAttachment();

    Slider& slider;

private:
    float getValue() const override;
    void setValue (float newValue) override;
    void sliderValueChanged (Slider*) override;
    void sliderDragStarted (Slider*) override;
    void sliderDragEnded (Slider*) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AsyncSliderAttachment)
};
