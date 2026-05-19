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

    bool SelectionMask::IsValid() const
    {
        return bounds.IsValid() && cells.size() == static_cast<size_t>(bounds.width * bounds.height);
    }

    void SelectionMask::Clear()
    {
        bounds = {};
        cells.clear();
    }

    bool SelectionMask::Contains(int x, int y) const
    {
        if (!IsValid() || x < bounds.x || y < bounds.y || x >= bounds.x + bounds.width || y >= bounds.y + bounds.height)
        {
            return false;
        }
        const int ix = x - bounds.x;
        const int iy = y - bounds.y;
        return cells[static_cast<size_t>(iy * bounds.width + ix)] != 0;
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

        template <typename Predicate>
        void ForEachSelectionCell(const SelectionRect &selection, Predicate &&pred)
        {
            for (int y = selection.y; y < selection.y + selection.height; ++y)
            {
                for (int x = selection.x; x < selection.x + selection.width; ++x)
                {
                    pred(x, y);
                }
            }
        }

        template <typename Predicate>
        void ForEachSelectionCell(const SelectionMask &selection, Predicate &&pred)
        {
            if (!selection.IsValid())
            {
                return;
            }
            for (int y = selection.bounds.y; y < selection.bounds.y + selection.bounds.height; ++y)
            {
                for (int x = selection.bounds.x; x < selection.bounds.x + selection.bounds.width; ++x)
                {
                    if (selection.Contains(x, y))
                    {
                        pred(x, y);
                    }
                }
            }
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

    void CanvasDocument::CropToSelection(const SelectionMask &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        std::vector<PixelPaint> croppedPixels;
        croppedPixels.reserve(pixels.size());
        ForEachSelectionCell(selection, [&](int x, int y) {
            for (const auto &pixel : pixels)
            {
                if (pixel.x == x && pixel.y == y)
                {
                    croppedPixels.push_back(PixelPaint{pixel.x - selection.bounds.x, pixel.y - selection.bounds.y, pixel.color});
                    break;
                }
            }
        });
        pixels = std::move(croppedPixels);
        settings.width = selection.bounds.width;
        settings.height = selection.bounds.height;
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

    size_t CanvasDocument::EraseSelection(const SelectionMask &selection)
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
            if (!selection.Contains(pixel.x, pixel.y))
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

    size_t CanvasDocument::KeepSelectionOnly(const SelectionMask &selection)
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
            if (selection.Contains(pixel.x, pixel.y))
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

    void CanvasDocument::FlipHorizontalInSelection(const SelectionMask &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        std::vector<PixelPaint> remapped = pixels;
        for (auto &pixel : remapped)
        {
            if (selection.Contains(pixel.x, pixel.y))
            {
                const int localX = pixel.x - selection.bounds.x;
                pixel.x = selection.bounds.x + selection.bounds.width - 1 - localX;
            }
        }
        pixels = std::move(remapped);
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

    void CanvasDocument::FlipVerticalInSelection(const SelectionMask &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        std::vector<PixelPaint> remapped = pixels;
        for (auto &pixel : remapped)
        {
            if (selection.Contains(pixel.x, pixel.y))
            {
                const int localY = pixel.y - selection.bounds.y;
                pixel.y = selection.bounds.y + selection.bounds.height - 1 - localY;
            }
        }
        pixels = std::move(remapped);
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

    void CanvasDocument::ShiftSelection(const SelectionMask &selection, int dx, int dy)
    {
        if (!selection.IsValid() || (dx == 0 && dy == 0))
        {
            return;
        }
        std::vector<PixelPaint> moved;
        moved.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (selection.Contains(pixel.x, pixel.y))
            {
                moved.push_back(PixelPaint{pixel.x + dx, pixel.y + dy, pixel.color});
            }
            else
            {
                moved.push_back(pixel);
            }
        }
        pixels = std::move(moved);
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

    void CanvasDocument::RemoveSelection(const SelectionMask &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        std::vector<PixelPaint> keptPixels;
        keptPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (!selection.Contains(pixel.x, pixel.y))
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

    void CanvasDocument::ClearOutsideSelection(const SelectionMask &selection)
    {
        if (!selection.IsValid())
        {
            return;
        }
        std::vector<PixelPaint> keptPixels;
        keptPixels.reserve(pixels.size());
        for (const auto &pixel : pixels)
        {
            if (selection.Contains(pixel.x, pixel.y))
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
