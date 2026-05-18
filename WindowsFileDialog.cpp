#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

#include "WindowsFileDialog.h"

namespace canvas
{
    namespace
    {
        std::string ShowDialog(bool save)
        {
            char fileName[MAX_PATH] = {};
            OPENFILENAMEA ofn{};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = "PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
            if (save)
            {
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_EXPLORER;
                std::string defaultName = "canvas.png";
                std::copy(defaultName.begin(), defaultName.end(), fileName);
            }

            const BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
            if (ok != TRUE)
            {
                return {};
            }
            return std::string(fileName);
        }
    }

    std::string WindowsFileDialog::OpenPngFile()
    {
        return ShowDialog(false);
    }

    std::string WindowsFileDialog::SavePngFile()
    {
        return ShowDialog(true);
    }
}
