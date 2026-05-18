#pragma once

#include <string>

namespace canvas
{
    class WindowsFileDialog
    {
    public:
        static std::string OpenPngFile();
        static std::string SavePngFile();
    };
}
