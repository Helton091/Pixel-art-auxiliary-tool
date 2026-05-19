#pragma once

#include "CanvasTypes.h"

#include <filesystem>
#include <string>

namespace canvas
{
    class CanvasExporter
    {
    public:
        static std::string ExportCpp(const CanvasDocument &doc, const std::string &functionName);
        static bool SaveCpp(const std::filesystem::path &path, const CanvasDocument &doc, const std::string &functionName);
        static bool LoadImage(const std::filesystem::path &path, CanvasDocument &doc, std::string *errorMessage = nullptr);
        static bool SavePng(const std::filesystem::path &path, const CanvasDocument &doc);
    };
}
