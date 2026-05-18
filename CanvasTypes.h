#pragma once

#include "raylib.h"

#include <string>
#include <vector>

namespace canvas
{
    struct ColorRGBA
    {
        unsigned char r{};
        unsigned char g{};
        unsigned char b{};
        unsigned char a{255};
    };

    struct CanvasSettings
    {
        int width{32};
        int height{32};
        int cellSize{18};
    };

    struct PixelPaint
    {
        int x{};
        int y{};
        ColorRGBA color{};
    };


    struct SelectionRect
    {
        int x{};
        int y{};
        int width{};
        int height{};

        bool IsValid() const;
        void Normalize();
    };

    struct CanvasDocument
    {
        CanvasSettings settings{};
        std::vector<PixelPaint> pixels;

        void Clear();
        void CropToSelection(const SelectionRect &selection);
        size_t EraseSelection(const SelectionRect &selection);
        size_t KeepSelectionOnly(const SelectionRect &selection);
        void FlipHorizontal();
        void FlipVertical();
        void FlipHorizontalInSelection(const SelectionRect &selection);
        void FlipVerticalInSelection(const SelectionRect &selection);
        void Shift(int dx, int dy);
        void ShiftSelection(const SelectionRect &selection, int dx, int dy);
        void RemoveSelection(const SelectionRect &selection);
        void ClearOutsideSelection(const SelectionRect &selection);
    };

    enum class ToolType
    {
        Pixel,
        Rectangle,
        Circle,
        Stroke,
        Select
    };

    Color ToColor(const ColorRGBA &c);
    ColorRGBA ToRGBA(const Color &c);
}
