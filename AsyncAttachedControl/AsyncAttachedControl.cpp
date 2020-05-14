#include "AsyncAttachedControl.h"

// AsyncAttachedControl.cpp

AsyncAttachedControlGroup::AsyncAttachedControlGroup (AudioProcessorValueTreeState& s, const String& id)
    : state{ s }, paramID{ id }
{
    s.addParameterListener (id, this);
    lastValue = s.getRawParameterValue (id)->load();
}

AsyncAttachedControlGroup::~AsyncAttachedControlGroup()
{
    state.removeParameterListener (paramID, this);
}

void AsyncAttachedControlGroup::addControl (AsyncAttachedControl* c)
{
    if (c != nullptr)
        controls.push_back (c);
}

void AsyncAttachedControlGroup::removeControl (AsyncAttachedControl* c)
{
    if (c != nullptr)
        controls.erase (std::remove (controls.begin(), controls.end(), c), controls.end());
}

void AsyncAttachedControlGroup::controlValueChanged (AsyncAttachedControl* c)
{
    if (ignoreCallbacks || c == nullptr)
        return;

    auto value{ c->getValue() };
    
    {
        ScopedValueSetter svs{ ignoreCallbacks, true };
        c->setValue (lastValue);
    }

    if (notifyNewValue)
        notifyNewValue (paramID, value);
}

void AsyncAttachedControlGroup::beginParameterChange()
{
    if (auto p{ getParameter() })
    {
        if (state.undoManager)
            state.undoManager->beginNewTransaction();

        p->beginChangeGesture();
    }
}

void AsyncAttachedControlGroup::endParameterChange()
{
    if (auto p{ getParameter() })
        p->endChangeGesture();
}

void AsyncAttachedControlGroup::updateValue (float value)
{
    lastValue = value;

    if (MessageManager::getInstance()->isThisTheMessageThread())
    {
        cancelPendingUpdate();
        handleAsyncUpdate();
    }
    else
    {
        triggerAsyncUpdate();
    }
}

float AsyncAttachedControlGroup::getLastValue() const
{
    return lastValue;
}

RangedAudioParameter* AsyncAttachedControlGroup::getParameter() const
{
    return state.getParameter (paramID);
}

void AsyncAttachedControlGroup::parameterChanged (const String&, float value)
{
    if (MessageManager::getInstance()->isThisTheMessageThread() && ignoreCallbacks)
        return;

    if (notifyNewValue)
        notifyNewValue (paramID, value);
}

void AsyncAttachedControlGroup::handleAsyncUpdate()
{
    if (auto p{ getParameter() })
    {
        auto unnormalised{ lastValue.load() }, normalised{ p->convertTo0to1 (unnormalised) };
        ScopedValueSetter svs{ ignoreCallbacks, true };

        if (normalised != p->getValue())
            p->setValueNotifyingHost (normalised);
        
        for (auto c : controls)
            c->setValue (unnormalised);
    }
}

//==============================================================================
AsyncAttachedControl::AsyncAttachedControl (AsyncAttachedControlGroup& g)
    : group{ g }
{
    group.addControl (this);
}

AsyncAttachedControl::~AsyncAttachedControl()
{
    group.removeControl (this);
}

//==============================================================================
AsyncSliderAttachment::AsyncSliderAttachment (AsyncAttachedControlGroup& g, Slider& sl)
    : AsyncAttachedControl{ g }, slider{ sl }
{
    if (auto param{ group.getParameter() })
    {
        auto range{ param->getNormalisableRange() };

        slider.valueFromTextFunction = [param](const String& text)
        {
            return double (param->convertFrom0to1 (param->getValueForText (text)));
        };

        slider.textFromValueFunction = [param](double value)
        {
            return param->getText (param->convertTo0to1 (float (value)), 0);
        };

        slider.setDoubleClickReturnValue (true, range.convertFrom0to1 (param->getDefaultValue()));

        auto convertFrom0To1 = [range](double start, double end, double value) mutable
        {
            range.start = float (start);
            range.end = float (end);
            return double (range.convertFrom0to1 (float (value)));
        };

        auto convertTo0To1 = [range](double start, double end, double value) mutable
        {
            range.start = float (start);
            range.end = float (end);
            return double (range.convertTo0to1 (float (value)));
        };

        auto snapToLegalValue = [range](double start, double end, double value) mutable
        {
            range.start = float (start);
            range.end = float (end);
            return double (range.snapToLegalValue (float (value)));
        };

        NormalisableRange<double> newRange{ double (range.start), double (range.end),
                                            convertFrom0To1, convertTo0To1, snapToLegalValue };
        newRange.interval = double (range.interval);
        newRange.skew = double (range.skew);
        slider.setNormalisableRange (newRange);
    }

    setValue (group.getLastValue());
    slider.addListener (this);
}

AsyncSliderAttachment::~AsyncSliderAttachment()
{
    slider.removeListener (this);
}

float AsyncSliderAttachment::getValue() const
{
    return float (slider.getValue());
}

void AsyncSliderAttachment::setValue (float newValue)
{
    slider.setValue (newValue, sendNotificationSync);
}

void AsyncSliderAttachment::sliderValueChanged (Slider*)
{
    group.controlValueChanged (this);
}

void AsyncSliderAttachment::sliderDragStarted (Slider*)
{
    group.beginParameterChange();
}

void AsyncSliderAttachment::sliderDragEnded (Slider*)
{
    group.endParameterChange();
}
