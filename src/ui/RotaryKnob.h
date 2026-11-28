// ============================================================================
//  src/ui/RotaryKnob.h — 让旋钮真正「跟手」的 Slider
// ----------------------------------------------------------------------------
//  为什么要它：
//    juce::Slider 的 RotaryHorizontalVerticalDrag 把鼠标的 dx、dy **相加**折算成增量。
//    用户按着旋钮的圆弧轨迹拖时，dx 与 dy 会互相抵消（左半边 dx 为负、右半边为正，
//    dy 又在上下翻转），净位移常常接近 0 —— 表现就是「按轨迹拖不动，得拖到轨迹外面
//    才偶尔能拉到头」。这是相对式拖拽的固有行为，不是手感玄学。
//
//  这里的规则：
//    · 指针落在旋钮的**环上**（离圆心 > 0.45×半径，且在有效弧 startAngle..endAngle
//      之内）：按指针绕圆心的角度变化推进数值，转多少角度就变多少，1:1，不抵消；
//    · 指针落进**圆心内圈**或旋钮下方的**缺口（死区）**：角度没有意义（穿过圆心会
//      有 ±180° 跳变），改用竖直拖拽（上大下小），永远不跳；
//    · 两种模式之间切换时只重新同步基准、不补差值，所以来回切换不会跳变。
//
//  圆心与半径取自 LookAndFeel 自己的布局（getSliderLayout），和实际画出来的圆一致，
//  换 LookAndFeel 不会错位。
// ============================================================================
#pragma once

#include <JuceHeader.h>

class RotaryKnob : public juce::Slider
{
public:
    RotaryKnob();

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

private:
    /** 指针相对圆心的角度，0 = 正上方，顺时针为正，归一到 [0, 2π)。 */
    float pointerAngle (juce::Point<float> position) const;

    /** LookAndFeel 实际用来画旋钮的方块（用来取圆心与半径）。 */
    juce::Rectangle<float> knobBounds() const;

    juce::Slider::RotaryParameters rotaryParameters() const;

    /** 内圈阈值：离圆心小于「半径 × 这个系数」就退化成正交拖拽。 */
    static constexpr float kInnerRadius = 0.45f;

    /** 竖直兜底模式的灵敏度：拖多少像素走完整个取值范围。 */
    static constexpr float kVerticalPixels = 220.0f;

    juce::Point<float> lastPosition;
    float lastAngle = 0.0f;
};
