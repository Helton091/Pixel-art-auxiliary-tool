#include "CanvasTypes.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace canvas
{
    bool SelectionRect::IsValid() const
    {
        return width > 0 && height > 0;
    }

    void SelectionRect::Normalize()
    {
        if (width < 0)
        {
            x += width;
            width = -width;
        }
        if (height < 0)
        {
            y += height;
            height = -height;
        }
    }

    void CanvasDocument::Clear()
    {
        pixels.clear();
    }

    namespace
    {
        bool InSelection(const SelectionRect &selection, int x, int y)
        {
            return x >= selection.x && y >= selection.y && x < selection.x + selection.width && y < selection.y + selection.height;
        }
    }

    void CanvasDocument::CropToSelection(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }

        std::vector<PixelPaint> croppedPixels;
        croppedPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (InSelection(selection, pixel.x, pixel.y))
            {
                croppedPixels.push_back(PixelPaint{pixel.x - selection.x, pixel.y - selection.y, pixel.color});
            }
        }
        pixels = std::move(croppedPixels);
        settings.width = selection.width;
        settings.height = selection.height;
    }

    size_t CanvasDocument::EraseSelection(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return 0;
        }

        const size_t before = pixels.size();
        std::vector<PixelPaint> keptPixels;
        keptPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (!InSelection(selection, pixel.x, pixel.y))
            {
                keptPixels.push_back(pixel);
            }
        }
        pixels = std::move(keptPixels);
        return before - pixels.size();
    }

    size_t CanvasDocument::KeepSelectionOnly(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return 0;
        }

        const size_t before = pixels.size();
        std::vector<PixelPaint> keptPixels;
        keptPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (InSelection(selection, pixel.x, pixel.y))
            {
                keptPixels.push_back(pixel);
            }
        }
        pixels = std::move(keptPixels);
        return before - pixels.size();
    }

    void CanvasDocument::FlipHorizontal()
    {
        for (auto &pixel : pixels)
        {
            pixel.x = settings.width - 1 - pixel.x;
        }
    }

    void CanvasDocument::FlipVertical()
    {
        for (auto &pixel : pixels)
        {
            pixel.y = settings.height - 1 - pixel.y;
        }
    }

    void CanvasDocument::FlipHorizontalInSelection(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        for (auto &pixel : pixels)
        {
            if (InSelection(selection, pixel.x, pixel.y))
            {
                pixel.x = selection.x + selection.width - 1 - (pixel.x - selection.x);
            }
        }
    }

    void CanvasDocument::FlipVerticalInSelection(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        for (auto &pixel : pixels)
        {
            if (InSelection(selection, pixel.x, pixel.y))
            {
                pixel.y = selection.y + selection.height - 1 - (pixel.y - selection.y);
            }
        }
    }

    void CanvasDocument::Shift(int dx, int dy)
    {
        for (auto &pixel : pixels)
        {
            pixel.x += dx;
            pixel.y += dy;
        }
    }

    void CanvasDocument::ShiftSelection(const SelectionRect &selection, int dx, int dy)
    {
        if (!selection.IsValid() || (dx == 0 && dy == 0))
        {
            return;
        }

        for (auto &pixel : pixels)
        {
            if (InSelection(selection, pixel.x, pixel.y))
            {
                pixel.x += dx;
                pixel.y += dy;
            }
        }
    }

    void CanvasDocument::RemoveSelection(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }

        std::vector<PixelPaint> keptPixels;
        keptPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (!InSelection(selection, pixel.x, pixel.y))
            {
                keptPixels.push_back(pixel);
            }
        }
        pixels = std::move(keptPixels);
    }

    void CanvasDocument::ClearOutsideSelection(const SelectionRect &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }

        std::vector<PixelPaint> keptPixels;
        keptPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (InSelection(selection, pixel.x, pixel.y))
            {
                keptPixels.push_back(pixel);
            }
        }
        pixels = std::move(keptPixels);
    }

    Color ToColor(const ColorRGBA &c)
    {
        return Color{c.r, c.g, c.b, c.a};
    }

    ColorRGBA ToRGBA(const Color &c)
    {
        return ColorRGBA{c.r, c.g, c.b, c.a};
    }
}
