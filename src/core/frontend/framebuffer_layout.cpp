// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>

#include "common/assert.h"
#include "core/3ds.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/settings.h"

namespace Layout {

static const float TOP_SCREEN_ASPECT_RATIO =
    static_cast<float>(Core::kScreenTopHeight) / Core::kScreenTopWidth;
static const float BOT_SCREEN_ASPECT_RATIO =
    static_cast<float>(Core::kScreenBottomHeight) / Core::kScreenBottomWidth;

u16 FramebufferLayout::GetScalingRatio() const {
    return static_cast<u16>(((top_screen.GetWidth() - 1) / Core::kScreenTopWidth) + 1);
}

// Finds the largest size subrectangle contained in window area that is confined to the aspect ratio
template <class T>
static MathUtil::Rectangle<T> maxRectangle(MathUtil::Rectangle<T> window_area,
                                           float screen_aspect_ratio) {
    float scale = std::min(static_cast<float>(window_area.GetWidth()),
                           window_area.GetHeight() / screen_aspect_ratio);
    return MathUtil::Rectangle<T>{0, 0, static_cast<T>(std::round(scale)),
                                  static_cast<T>(std::round(scale * screen_aspect_ratio))};
}

static FramebufferLayout FrameLayout(unsigned width, unsigned height, bool swapped, float scale, FramebufferLayout &res) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    unsigned top_height, top_width, bottom_height, bottom_width, viewport_height, viewport_width;

    if (!swapped) {
        bottom_height = Core::kScreenBottomHeight * scale;
        bottom_width = Core::kScreenBottomWidth * scale;
        viewport_height = (width - bottom_width) * TOP_SCREEN_ASPECT_RATIO;
        viewport_width = (height / TOP_SCREEN_ASPECT_RATIO) + bottom_width;
        top_height = viewport_height + (height - viewport_height) / 2;
        top_width = (height / TOP_SCREEN_ASPECT_RATIO) + (width - viewport_width) / 2;

        if (height > viewport_height) {
            res.top_screen = {0, (height - viewport_height) / 2, width - bottom_width, top_height};
            res.bottom_screen = {width - bottom_width, top_height - bottom_height, width, top_height};
        } else {
            res.top_screen = {(width - viewport_width) / 2, 0, top_width, height};
            res.bottom_screen = {top_width, height - bottom_height, top_width + bottom_width, height};
        }
    } else {
        top_height = Core::kScreenTopHeight * scale;
        top_width = Core::kScreenTopWidth * scale;
        viewport_height = (width - top_width) * BOT_SCREEN_ASPECT_RATIO;
        viewport_width = (height / BOT_SCREEN_ASPECT_RATIO) + top_width;
        bottom_height = viewport_height + (height - viewport_height) / 2;
        bottom_width = (height / BOT_SCREEN_ASPECT_RATIO) + (width - viewport_width) / 2;

        if (height > viewport_height) {
            res.top_screen = {width - top_width, bottom_height - top_height, width, bottom_height};
            res.bottom_screen = {0, (height - viewport_height) / 2, width - top_width, bottom_height};
        } else {
            res.top_screen = {bottom_width, height - top_height, bottom_width + top_width, height};
            res.bottom_screen = {(width - viewport_width) / 2, 0, bottom_width, height};
        }
    }
    // The drawing code needs at least somewhat valid values for both screens even if one isn't showing.
    if (swapped && scale == 0) res.top_screen = res.bottom_screen;
    return res;
}

FramebufferLayout DefaultFrameLayout(unsigned width, unsigned height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};
    // Default layout gives equal screen sizes to the top and bottom screen
    MathUtil::Rectangle<unsigned> screen_window_area{0, 0, width, height / 2};
    MathUtil::Rectangle<unsigned> top_screen =
        maxRectangle(screen_window_area, TOP_SCREEN_ASPECT_RATIO);
    MathUtil::Rectangle<unsigned> bot_screen =
        maxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);

    float window_aspect_ratio = static_cast<float>(height) / width;
    // both screens height are taken into account by multiplying by 2
    float emulation_aspect_ratio = TOP_SCREEN_ASPECT_RATIO * 2;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Apply borders to the left and right sides of the window.
        top_screen =
            top_screen.TranslateX((screen_window_area.GetWidth() - top_screen.GetWidth()) / 2);
        bot_screen =
            bot_screen.TranslateX((screen_window_area.GetWidth() - bot_screen.GetWidth()) / 2);
    } else {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        // Recalculate the bottom screen to account for the width difference between top and bottom
        screen_window_area = {0, 0, width, top_screen.GetHeight()};
        bot_screen = maxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);
        bot_screen = bot_screen.TranslateX((top_screen.GetWidth() - bot_screen.GetWidth()) / 2);
        if (swapped) {
            bot_screen = bot_screen.TranslateY(height / 2 - bot_screen.GetHeight());
        } else {
            top_screen = top_screen.TranslateY(height / 2 - top_screen.GetHeight());
        }
    }
    // Move the top screen to the bottom if we are swapped.
    res.top_screen = swapped ? top_screen.TranslateY(height / 2) : top_screen;
    res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateY(height / 2);
    return res;
}

FramebufferLayout SingleFrameLayout(unsigned width, unsigned height, bool swapped) {
    FramebufferLayout res{width, height, !swapped, swapped, {}, {}};
    return FrameLayout(width, height, swapped, 0.0, res);
}

FramebufferLayout LargeFrameLayout(unsigned width, unsigned height, bool swapped) {
    // TODO: use a custom scale
    FramebufferLayout res{width, height, true, true, {}, {}};
    return FrameLayout(width, height, swapped, 1.0, res);
}

FramebufferLayout SideFrameLayout(unsigned width, unsigned height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};
    // Aspect ratio of both screens side by side
    const float emulation_aspect_ratio = static_cast<float>(Core::kScreenTopHeight) /
                                         (Core::kScreenTopWidth + Core::kScreenBottomWidth);
    float window_aspect_ratio = static_cast<float>(height) / width;
    MathUtil::Rectangle<unsigned> screen_window_area{0, 0, width, height};
    // Find largest Rectangle that can fit in the window size with the given aspect ratio
    MathUtil::Rectangle<unsigned> screen_rect =
        maxRectangle(screen_window_area, emulation_aspect_ratio);
    // Find sizes of top and bottom screen
    MathUtil::Rectangle<unsigned> top_screen = maxRectangle(screen_rect, TOP_SCREEN_ASPECT_RATIO);
    MathUtil::Rectangle<unsigned> bot_screen = maxRectangle(screen_rect, BOT_SCREEN_ASPECT_RATIO);

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Apply borders to the left and right sides of the window.
        u32 shift_horizontal = (screen_window_area.GetWidth() - screen_rect.GetWidth()) / 2;
        top_screen = top_screen.TranslateX(shift_horizontal);
        bot_screen = bot_screen.TranslateX(shift_horizontal);
    } else {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        u32 shift_vertical = (screen_window_area.GetHeight() - screen_rect.GetHeight()) / 2;
        top_screen = top_screen.TranslateY(shift_vertical);
        bot_screen = bot_screen.TranslateY(shift_vertical);
    }
    // Move the top screen to the right if we are swapped.
    res.top_screen = swapped ? top_screen.TranslateX(bot_screen.GetWidth()) : top_screen;
    res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateX(top_screen.GetWidth());
    return res;
}

FramebufferLayout CustomFrameLayout(unsigned width, unsigned height) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};

    MathUtil::Rectangle<unsigned> top_screen{
        Settings::values.custom_top_left, Settings::values.custom_top_top,
        Settings::values.custom_top_right, Settings::values.custom_top_bottom};
    MathUtil::Rectangle<unsigned> bot_screen{
        Settings::values.custom_bottom_left, Settings::values.custom_bottom_top,
        Settings::values.custom_bottom_right, Settings::values.custom_bottom_bottom};

    res.top_screen = top_screen;
    res.bottom_screen = bot_screen;
    return res;
}
} // namespace Layout
