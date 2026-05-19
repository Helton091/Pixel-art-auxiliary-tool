#pragma once

#include "CanvasTypes.h"

#include <filesystem>

namespace canvas
{
    enum class PixelizeMode
    {
        Average,
        CenterSample,
        DominantColor,
        EdgeAware
    };

    class ImagePixelizer
    {
    public:
        static bool PixelizeImage(const std::filesystem::path &path,
                                  CanvasDocument &doc,
                                  int targetWidth,
                                  int targetHeight,
                                  PixelizeMode mode = PixelizeMode::Average,
                                  bool useLinearColor = true);
    };
}
