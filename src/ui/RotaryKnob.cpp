#include "RotaryKnob.h"

RotaryKnob::RotaryKnob()
{
    // juce::Slider 原本的拖拽行为作为「指针落在圆心附近」时的兜底
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
}

juce::Slider::RotaryParameters RotaryKnob::rotaryParameters() const
{
    return getRotaryParameters();
}

juce::Rectangle<float> RotaryKnob::knobBounds() const
{
    // 优先用 LookAndFeel 自己的布局，和画出来的圆完全一致
    if (auto* v2 = dynamic_cast<juce::LookAndFeel_V2*> (&getLookAndFeel()))
        return v2->getSliderLayout (const_cast<RotaryKnob&> (*this)).sliderBounds.toFloat();

    // 兜底：扣掉文本框后的区域
    auto area = getLocalBounds();
    if (getTextBoxPosition() == TextBoxBelow)
        area.removeFromBottom (getTextBoxHeight());
    else if (getTextBoxPosition() == TextBoxAbove)
        area.removeFromTop (getTextBoxHeight());

    return area.toFloat();
}

float RotaryKnob::pointerAngle (juce::Point<float> position) const
{
    const auto centre = knobBounds().getCentre();

    // 0 = 正上方，顺时针为正（与 juce::Slider 内部的角度约定一致）
    auto angle = std::atan2 (position.x - centre.x, centre.y - position.y);
    if (angle < 0.0f)
        angle += juce::MathConstants<float>::twoPi;
    return angle;
}

void RotaryKnob::mouseDown (const juce::MouseEvent& event)
{
    lastPosition = event.position;
    lastAngle = pointerAngle (event.position);

    juce::Slider::mouseDown (event);
}

void RotaryKnob::mouseDrag (const juce::MouseEvent& event)
{
    const auto bounds = knobBounds();
    const auto radius = juce::jmax (1.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f);
    const auto centre = bounds.getCentre();
    const auto distance = event.position.getDistanceFrom (centre);

    const auto params = rotaryParameters();
    auto angle = pointerAngle (event.position);
    while (angle < params.startAngleRadians)
        angle += juce::MathConstants<float>::twoPi;

    const auto inDeadZone = angle > params.endAngleRadians;
    const auto nearCentre = distance < radius * kInnerRadius;
    const auto span = params.endAngleRadians - params.startAngleRadians;
    const auto range = getMaximum() - getMinimum();

    // ---- 情况 1：指针在圆心的内圈里（或者落在旋钮下方的缺口里）----------
    // 角度在这里没有意义（穿过圆心会有 ±180° 的跳变），改用竖直拖拽：
    // 上移增大、下移减小，和常见插件手感一致，也永远不会跳。
    if (nearCentre || inDeadZone || span <= 0.0f)
    {
        const auto dy = event.position.y - lastPosition.y;
        lastPosition = event.position;
        lastAngle = angle;

        if (dy != 0.0f)
            setValue (getValue() - (static_cast<double> (dy) / kVerticalPixels) * range,
                      juce::sendNotificationSync);
        return;
    }

    // ---- 情况 2：指针在旋钮的环上 ----------------------------------------
    // 只累加角度变化：按着圆弧拖多少度，旋钮就转多少度，dx/dy 不会互相抵消。
    lastPosition = event.position;

    auto delta = angle - lastAngle;
    if (delta > juce::MathConstants<float>::pi)
        delta -= juce::MathConstants<float>::twoPi;
    else if (delta < -juce::MathConstants<float>::pi)
        delta += juce::MathConstants<float>::twoPi;

    lastAngle = angle;

    if (delta == 0.0f)
        return;

    setValue (getValue() + (static_cast<double> (delta) / span) * range,
              juce::sendNotificationSync);
}
