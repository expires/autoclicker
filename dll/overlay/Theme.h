#pragma once
#include <cstdint>

namespace Theme
{
    struct Col { uint32_t hex; float a; };

    inline constexpr float Scale = 1.2f;

    inline constexpr float px(float v) { return v * Scale; }

    namespace M
    {
        // Window
        inline constexpr float WindowW       = px(638.0f);
        inline constexpr float WindowH       = px(420.0f * 0.90f);   // 10% shorter

        // Fonts
        inline constexpr float FontBody      = px(16.0f);
        inline constexpr float FontTitle     = px(19.0f);

        // Chrome / layout
        inline constexpr float Margin        = px(10.0f);
        inline constexpr float TopbarH       = px(52.0f);
        inline constexpr float SidebarW      = px(150.0f);
        inline constexpr float TitlePadX     = px(15.0f);
        inline constexpr float LogoH         = px(26.0f);
        inline constexpr float LogoGap       = px(9.0f);
        inline constexpr float SidebarTopPad = px(20.0f);
        inline constexpr float BodyPad       = px(22.0f);

        // Sidebar tab
        inline constexpr float TabH          = px(32.0f);
        inline constexpr float TabMarginX    = px(10.0f);
        inline constexpr float TabMarginY    = px(1.0f);
        inline constexpr float TabRound      = px(7.0f);
        inline constexpr float TabTextPadX   = px(20.0f);

        // Checkbox row (toggle pill)
        inline constexpr float CheckRowH     = px(30.0f);
        inline constexpr float PillW         = px(42.0f);
        inline constexpr float PillH         = px(22.0f);
        inline constexpr float KnobInset     = px(3.0f);

        // Keybind row
        inline constexpr float KeybindH      = px(24.0f);
        inline constexpr float KeybindInlineW= px(80.0f);
        inline constexpr float KeybindPillW  = px(80.0f);
        inline constexpr float KeybindPillPad= px(0.0f);
        inline constexpr float KeybindRound  = px(6.0f);
        inline constexpr float KeybindGap    = px(8.0f);

        // Slider row
        inline constexpr float SliderH       = px(24.0f);
        inline constexpr float SliderLabelGap= px(4.0f);
        inline constexpr float SliderTrackTop= px(6.0f);
        inline constexpr float SliderTrackBot= px(7.0f);
        inline constexpr float SliderGrabPad = px(6.5f);
        inline constexpr float SliderKnobGlow= px(9.5f);
        inline constexpr float SliderKnob    = px(6.5f);

        // Numeric input rows
        inline constexpr float InputRowH     = px(36.0f);
        inline constexpr float InputW        = px(160.0f);
        inline constexpr float InputPairW    = px(110.0f);
        inline constexpr float InputPairGap  = px(6.0f);

        // List rows (friends / macros)
        inline constexpr float ListBtnW      = px(24.0f);
        inline constexpr float ListGap       = px(6.0f);
        inline constexpr float ListBindW     = px(80.0f);
        inline constexpr float AddBtnH       = px(32.0f);
        inline constexpr float FriendAddBtnW = px(74.0f);
        inline constexpr float MacroIndent   = px(12.0f);
        inline constexpr float RowSpacing    = px(6.0f);
    }

    inline constexpr Col Transparent       { 0x000000, 0.00f };
    inline constexpr Col White             { 0xffffff, 1.00f };

    inline constexpr Col WindowBg          { 0x19191c, 0.95f };
    inline constexpr Col ChildBg           { 0x19191c, 0.18f };
    inline constexpr Col PopupBg           { 0x101012, 0.96f };
    inline constexpr Col Border            { 0xffffff, 0.08f };

    inline constexpr Col FrameBg           { 0x2a2a2e, 0.55f };
    inline constexpr Col FrameBgHovered    { 0x35353a, 0.66f };
    inline constexpr Col FrameBgActive     { 0x3f3f45, 0.78f };

    inline constexpr Col Button            { 0x36363b, 0.50f };
    inline constexpr Col ButtonHovered     { 0x434348, 0.66f };
    inline constexpr Col ButtonActive      { 0x4f4f56, 0.80f };

    inline constexpr Col Header            { 0x9a9a9f, 0.22f };
    inline constexpr Col HeaderHovered     { 0x9a9a9f, 0.40f };
    inline constexpr Col HeaderActive      { 0x9a9a9f, 0.60f };

    inline constexpr Col Separator         { 0x9a9a9f, 0.20f };
    inline constexpr Col SeparatorHovered  { 0xb5b5bb, 0.40f };
    inline constexpr Col SeparatorActive   { 0xcfcfd4, 0.70f };

    inline constexpr Col TabBarHovered     { 0x2c2c31, 0.50f };
    inline constexpr Col TabBarActive      { 0x2a2a2e, 0.60f };

    inline constexpr Col ScrollGrab        { 0x9a9a9f, 0.28f };
    inline constexpr Col ScrollGrabHovered { 0xb5b5bb, 0.45f };
    inline constexpr Col ScrollGrabActive  { 0xcfcfd4, 0.65f };

    inline constexpr Col ResizeGrip        { 0x9a9a9f, 0.15f };
    inline constexpr Col ResizeGripHovered { 0xb5b5bb, 0.50f };
    inline constexpr Col ResizeGripActive  { 0xcfcfd4, 0.80f };

    inline constexpr Col Text              { 0xdadade, 1.00f };
    inline constexpr Col TextDim           { 0x808086, 1.00f };

    inline constexpr Col Accent            { 0x9aa0a8, 0.33f };
    inline constexpr Col AccentTrack       { 0x9aa0a8, 0.33f };
    inline constexpr Col AccentGlow        { 0xb7bac1, 0.30f };
    inline constexpr Col Knob              { 0xf0f0f2, 1.00f };
    inline constexpr Col KnobActive        { 0xffffff, 1.00f };
    inline constexpr Col KeybindListening  { 0x4a4a50, 0.95f };

    inline constexpr Col TabSelectedFill   { 0x9a9aa2, 0.18f };
    inline constexpr Col TabSelectedBorder { 0xc4c6cc, 0.20f };
    inline constexpr Col TabHover          { 0x9a9aa2, 0.10f };
    inline constexpr Col TabTextActive     { 0xffffff, 1.00f };
    inline constexpr Col TabTextInactive   { 0x8c8c93, 1.00f };

    inline constexpr Col ListBtnHovered    { 0x9a9aa2, 0.18f };
    inline constexpr Col ListBtnActive     { 0x9a9aa2, 0.28f };
    inline constexpr Col AddBtn            { 0x9a9aa2, 0.18f };
    inline constexpr Col AddBtnHovered     { 0x9a9aa2, 0.34f };
    inline constexpr Col AddBtnActive      { 0x9a9aa2, 0.52f };

    inline constexpr Col Danger            { 0x9b1c1c, 1.00f };
    inline constexpr Col DangerHovered     { 0xb91c1c, 1.00f };
    inline constexpr Col DangerActive      { 0x7a1414, 1.00f };
    inline constexpr Col Warning           { 0xf2c14e, 1.00f };
}
