#include "CanvasApp.h"
#include "CanvasExporter.h"
#include "CanvasTypes.h"
#include "WindowsFileDialog.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace canvas
{
    namespace
    {
        static float Distance(Vector2 a, Vector2 b)
        {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }
    }

    class CanvasAppImpl
    {
    public:
        CanvasAppImpl()
        {
            document_.settings = {48, 32, 18};
            palette_ = {BLACK,
                        WHITE,
                        Color{255, 0, 0, 255},
                        Color{0, 255, 0, 255},
                        Color{0, 0, 255, 255},
                        Color{255, 255, 0, 255},
                        Color{255, 165, 0, 255},
                        Color{128, 0, 128, 255},
                        Color{255, 105, 180, 255},
                        Color{135, 206, 235, 255},
                        Color{80, 80, 80, 255},
                        Color{139, 69, 19, 255}};
            activeColor_ = Color{255, 0, 0, 255};
            brushThickness_ = 1;
        }

        void Run()
        {
            SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
            InitWindow(1600, 980, "Canvas Export Tool");
            MaximizeWindow();
            SetTargetFPS(60);
            SetExitKey(KEY_NULL);
            ApplyWindowMode();
            EnsureCanvasTexture();

            while (!WindowShouldClose())
            {
                Update();
                BeginDrawing();
                ClearBackground(Color{24, 24, 28, 255});
                Draw();
                EndDrawing();
            }

            if (canvasTexture_.id != 0)
            {
                UnloadRenderTexture(canvasTexture_);
            }
            CloseWindow();
        }

    private:
        CanvasDocument document_{};
        std::vector<CanvasDocument> undoStack_{};
        static constexpr size_t kMaxUndoSteps = 32;
        bool undoCapturedThisAction_{false};
        ToolType tool_{ToolType::Pixel};
        Color activeColor_{RED};
        std::vector<Color> palette_;
        int brushSize_{1};
        int brushThickness_{1};
        bool settingsOpen_{true};
        float canvasZoom_{1.0f};
        Vector2 canvasPan_{0.0f, 0.0f};
        static constexpr float kMinCanvasZoom = 0.25f;
        static constexpr float kMaxCanvasZoom = 16.0f;
        enum class PointerMode
        {
            Idle,
            Drawing,
            Selecting,
            Panning
        };

        enum class AnchorMode
        {
            Center,
            TopLeft,
            Selection,
            Mouse
        };

        AnchorMode anchorMode_{AnchorMode::Center};
        PointerMode pointerMode_{PointerMode::Idle};
        bool hasSelection_{false};
        bool movingSelection_{false};
        Vector2 dragStartMouse_{};
        Vector2 dragStartPan_{};
        Vector2 selectionMoveStartGrid_{};
        Vector2 selectionStart_{};
        Vector2 selectionEnd_{};
        SelectionRect selection_{};
        Vector2 strokeStart_{};
        std::string status_ = "Ready";
        std::string pngPathInput_ = "assets/input.png";
        std::string pngExportPath_ = "exports/canvas.png";
        std::string debugHover_ = "";
        std::string debugPaint_ = "";
        bool editingOpenPath_{false};
        bool editingSavePath_{false};
        bool fullscreen_{false};
        RenderTexture2D canvasTexture_{};
        bool canvasDirty_{true};

        enum class SidebarAction
        {
            ResizeWPlus,
            ResizeWMinus,
            ResizeHPlus,
            ResizeHMinus,
            ZoomPlus,
            ZoomMinus,
            OpenPng,
            SavePng,
            ExportCpp,
            ClearCanvas
        };

        struct CanvasView
        {
            Rectangle bounds{};
            Vector2 origin{};
            float scale{1.0f};
        };

        CanvasView GetCanvasView() const
        {
            const float canvasW = static_cast<float>(document_.settings.width * document_.settings.cellSize);
            const float canvasH = static_cast<float>(document_.settings.height * document_.settings.cellSize);
            const float viewW = canvasW * canvasZoom_;
            const float viewH = canvasH * canvasZoom_;
            const float panelX = 320.0f;
            const float panelY = 24.0f;
            const float panelW = std::max(0.0f, static_cast<float>(GetScreenWidth()) - panelX - 24.0f);
            const float panelH = std::max(0.0f, static_cast<float>(GetScreenHeight()) - panelY - 24.0f);
            const float x = panelX + std::max(0.0f, (panelW - viewW) * 0.5f) + canvasPan_.x;
            const float y = panelY + std::max(0.0f, (panelH - viewH) * 0.5f) + canvasPan_.y;
            const Rectangle bounds{x, y, viewW, viewH};
            return CanvasView{bounds, Vector2{bounds.x, bounds.y}, canvasZoom_};
        }

        Rectangle CanvasBounds() const
        {
            return GetCanvasView().bounds;
        }

        void MarkCanvasDirty()
        {
            canvasDirty_ = true;
        }

        void PushUndoSnapshot()
        {
            undoStack_.push_back(document_);
            if (undoStack_.size() > kMaxUndoSteps)
            {
                undoStack_.erase(undoStack_.begin());
            }
            undoCapturedThisAction_ = true;
        }

        void BeginUndoIfNeeded()
        {
            if (!undoCapturedThisAction_)
            {
                PushUndoSnapshot();
            }
        }

        void EndUndoAction()
        {
            undoCapturedThisAction_ = false;
        }

        void ClearUndoStack()
        {
            std::vector<CanvasDocument>().swap(undoStack_);
        }

        void RestoreUndo()
        {
            if (undoStack_.empty())
            {
                SetStatus("Nothing to undo");
                return;
            }

            document_ = undoStack_.back();
            undoStack_.pop_back();
            selection_ = {};
            hasSelection_ = false;
            movingSelection_ = false;
            pointerMode_ = PointerMode::Idle;
            undoCapturedThisAction_ = false;
            EnsureCanvasTexture();
            MarkCanvasDirty();
            SetStatus(TextFormat("Undo: %zu step(s) left", undoStack_.size()));
        }

        void ApplyWindowMode()
        {
            if (fullscreen_)
            {
                if (!IsWindowMaximized())
                {
                    MaximizeWindow();
                }
            }
        }

        

        void EnsureCanvasTexture()
        {
            const int texW = std::max(1, document_.settings.width * document_.settings.cellSize);
            const int texH = std::max(1, document_.settings.height * document_.settings.cellSize);
            if (canvasTexture_.id != 0)
            {
                if (canvasTexture_.texture.width == texW && canvasTexture_.texture.height == texH)
                {
                    return;
                }
                UnloadRenderTexture(canvasTexture_);
                canvasTexture_ = {};
            }
            canvasTexture_ = LoadRenderTexture(texW, texH);
            SetTextureFilter(canvasTexture_.texture, TEXTURE_FILTER_POINT);
            canvasDirty_ = true;
            MarkCanvasDirty();
        }

        bool InCanvas(Vector2 p) const
        {
            return CheckCollisionPointRec(p, CanvasBounds());
        }

        void ZoomAtMouse(float factor, Vector2 mouse)
        {
            const Vector2 canvasBefore = ScreenToCanvasLocal(mouse);
            canvasZoom_ = std::clamp(canvasZoom_ * factor, kMinCanvasZoom, kMaxCanvasZoom);
            const CanvasView after = GetCanvasView();
            const Vector2 mouseAfter{after.bounds.x + canvasBefore.x * after.scale,
                                     after.bounds.y + canvasBefore.y * after.scale};
            canvasPan_.x += mouse.x - mouseAfter.x;
            canvasPan_.y += mouse.y - mouseAfter.y;
            MarkCanvasDirty();
        }

        Vector2 ScreenToCanvasLocal(Vector2 p) const
        {
            const CanvasView view = GetCanvasView();
            return Vector2{(p.x - view.bounds.x) / view.scale, (p.y - view.bounds.y) / view.scale};
        }

        Vector2 CanvasLocalToScreen(Vector2 p) const
        {
            const CanvasView view = GetCanvasView();
            return Vector2{view.bounds.x + p.x * view.scale, view.bounds.y + p.y * view.scale};
        }

        Vector2 CanvasToGrid(Vector2 p) const
        {
            const Vector2 local = ScreenToCanvasLocal(p);
            return Vector2{std::floor(local.x / document_.settings.cellSize), std::floor(local.y / document_.settings.cellSize)};
        }

        void SetStatus(std::string text)
        {
            status_ = std::move(text);
        }

        void SetHoverDebug(std::string text)
        {
            debugHover_ = std::move(text);
        }

        void SetPaintDebug(std::string text)
        {
            debugPaint_ = std::move(text);
        }

        static std::string ColorToHex(const Color &color)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
            return std::string(buf);
        }

        const PixelPaint *FindPixelAt(int gx, int gy) const
        {
            for (auto it = document_.pixels.rbegin(); it != document_.pixels.rend(); ++it)
            {
                if (it->x == gx && it->y == gy)
                {
                    return &(*it);
                }
            }
            return nullptr;
        }

        void PaintPixelAtGrid(int gx, int gy)
        {
            if (gx < 0 || gy < 0 || gx >= document_.settings.width || gy >= document_.settings.height)
            {
                return;
            }

            BeginUndoIfNeeded();
            const ColorRGBA rgba = ToRGBA(activeColor_);
            SetStatus(TextFormat("Paint pixel %d,%d = %s", gx, gy, ColorToHex(activeColor_).c_str()));
            SetPaintDebug(TextFormat("Paint RGB=%d,%d,%d,%d", rgba.r, rgba.g, rgba.b, rgba.a));
            for (auto &pixel : document_.pixels)
            {
                if (pixel.x == gx && pixel.y == gy)
                {
                    pixel.color = rgba;
                    MarkCanvasDirty();
                    return;
                }
            }

            document_.pixels.push_back(PixelPaint{gx, gy, rgba});
            MarkCanvasDirty();
        }

        void PaintPixelRect(int x, int y, int w, int h)
        {
            for (int iy = 0; iy < h; ++iy)
            {
                for (int ix = 0; ix < w; ++ix)
                {
                    PaintPixelAtGrid(x + ix, y + iy);
                }
            }
        }

        void PaintPixelCircle(int cx, int cy, int radius)
        {
            const int r = std::max(1, radius);
            for (int y = cy - r; y <= cy + r; ++y)
            {
                for (int x = cx - r; x <= cx + r; ++x)
                {
                    const int dx = x - cx;
                    const int dy = y - cy;
                    if (dx * dx + dy * dy <= r * r)
                    {
                        PaintPixelAtGrid(x, y);
                    }
                }
            }
        }

        void PaintPixelLine(Vector2 from, Vector2 to)
        {
            int x0 = static_cast<int>(std::lround(from.x));
            int y0 = static_cast<int>(std::lround(from.y));
            const int x1 = static_cast<int>(std::lround(to.x));
            const int y1 = static_cast<int>(std::lround(to.y));
            const int dx = std::abs(x1 - x0);
            const int sx = x0 < x1 ? 1 : -1;
            const int dy = -std::abs(y1 - y0);
            const int sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;
            while (true)
            {
                PaintPixelAtGrid(x0, y0);
                if (x0 == x1 && y0 == y1)
                {
                    break;
                }
                const int e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x0 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        void PaintCurrentTool(Vector2 mouse)
        {
            if (!InCanvas(mouse) || tool_ != ToolType::Pixel)
            {
                return;
            }
            const Vector2 grid = CanvasToGrid(mouse);
            const int gx = static_cast<int>(grid.x);
            const int gy = static_cast<int>(grid.y);
            const int s = std::max(1, brushThickness_);
            const int offset = (s - 1) / 2;
            for (int oy = 0; oy < s; ++oy)
            {
                for (int ox = 0; ox < s; ++ox)
                {
                    PaintPixelAtGrid(gx + ox - offset, gy + oy - offset);
                }
            }
        }

        SelectionRect MakeSelectionFromPoints(Vector2 a, Vector2 b) const
        {
            const Vector2 ga = CanvasToGrid(a);
            const Vector2 gb = CanvasToGrid(b);
            const int x0 = std::clamp(static_cast<int>(std::min(ga.x, gb.x)), 0, document_.settings.width - 1);
            const int y0 = std::clamp(static_cast<int>(std::min(ga.y, gb.y)), 0, document_.settings.height - 1);
            const int x1 = std::clamp(static_cast<int>(std::max(ga.x, gb.x)), 0, document_.settings.width - 1);
            const int y1 = std::clamp(static_cast<int>(std::max(ga.y, gb.y)), 0, document_.settings.height - 1);
            SelectionRect selection{x0, y0, x1 - x0 + 1, y1 - y0 + 1};
            selection.Normalize();
            return selection;
        }

        Rectangle SelectionToScreenRect(const SelectionRect &selection) const
        {
            const CanvasView view = GetCanvasView();
            const float x = view.bounds.x + selection.x * static_cast<float>(document_.settings.cellSize) * view.scale;
            const float y = view.bounds.y + selection.y * static_cast<float>(document_.settings.cellSize) * view.scale;
            const float w = selection.width * static_cast<float>(document_.settings.cellSize) * view.scale;
            const float h = selection.height * static_cast<float>(document_.settings.cellSize) * view.scale;
            return Rectangle{x, y, w, h};
        }

        void CommitShape(Vector2 begin, Vector2 end)
        {
            const Vector2 a = CanvasToGrid(begin);
            const Vector2 b = CanvasToGrid(end);
            const int ax = std::clamp(static_cast<int>(a.x), 0, document_.settings.width - 1);
            const int ay = std::clamp(static_cast<int>(a.y), 0, document_.settings.height - 1);
            const int bx = std::clamp(static_cast<int>(b.x), 0, document_.settings.width - 1);
            const int by = std::clamp(static_cast<int>(b.y), 0, document_.settings.height - 1);

            if (tool_ == ToolType::Rectangle)
            {
                const int x0 = std::min(ax, bx);
                const int y0 = std::min(ay, by);
                const int x1 = std::max(ax, bx);
                const int y1 = std::max(ay, by);
                PaintPixelRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
            }
            else if (tool_ == ToolType::Circle)
            {
                const int cx = ax;
                const int cy = ay;
                const int radius = std::max(std::abs(bx - ax), std::abs(by - ay));
                PaintPixelCircle(cx, cy, radius);
            }
            else if (tool_ == ToolType::Stroke)
            {
                PaintPixelLine(a, b);
            }
            MarkCanvasDirty();
        }

        void DrawPixelCircle(Vector2 center, float radius, Color color) const
        {
            const float cellSize = static_cast<float>(document_.settings.cellSize);
            const int minCellX = std::max(0, static_cast<int>(std::floor((center.x - radius) / cellSize)));
            const int maxCellX = std::min(document_.settings.width - 1, static_cast<int>(std::floor((center.x + radius) / cellSize)));
            const int minCellY = std::max(0, static_cast<int>(std::floor((center.y - radius) / cellSize)));
            const int maxCellY = std::min(document_.settings.height - 1, static_cast<int>(std::floor((center.y + radius) / cellSize)));
            const float radiusSq = radius * radius;

            for (int y = minCellY; y <= maxCellY; ++y)
            {
                for (int x = minCellX; x <= maxCellX; ++x)
                {
                    const float px = (static_cast<float>(x) + 0.5f) * cellSize;
                    const float py = (static_cast<float>(y) + 0.5f) * cellSize;
                    const float dx = px - center.x;
                    const float dy = py - center.y;
                    if (dx * dx + dy * dy <= radiusSq)
                    {
                        DrawRectangle(x * document_.settings.cellSize, y * document_.settings.cellSize,
                                      document_.settings.cellSize, document_.settings.cellSize, color);
                    }
                }
            }
        }

        void DrawCirclePreview(Vector2 start, Vector2 mouse) const
        {
            const CanvasView view = GetCanvasView();
            const Vector2 a = ScreenToCanvasLocal(start);
            const Vector2 b = ScreenToCanvasLocal(mouse);
            const int cx = std::clamp(static_cast<int>(std::lround(a.x / document_.settings.cellSize)), 0, document_.settings.width - 1);
            const int cy = std::clamp(static_cast<int>(std::lround(a.y / document_.settings.cellSize)), 0, document_.settings.height - 1);
            const int bx = std::clamp(static_cast<int>(std::lround(b.x / document_.settings.cellSize)), 0, document_.settings.width - 1);
            const int by = std::clamp(static_cast<int>(std::lround(b.y / document_.settings.cellSize)), 0, document_.settings.height - 1);
            const int radius = std::max(std::abs(bx - cx), std::abs(by - cy));
            const float radiusSq = static_cast<float>(radius * radius);
            const Color preview{activeColor_.r, activeColor_.g, activeColor_.b, 120};

            for (int y = std::max(0, cy - radius); y <= std::min(document_.settings.height - 1, cy + radius); ++y)
            {
                for (int x = std::max(0, cx - radius); x <= std::min(document_.settings.width - 1, cx + radius); ++x)
                {
                    const float dx = static_cast<float>(x - cx);
                    const float dy = static_cast<float>(y - cy);
                    if (dx * dx + dy * dy <= radiusSq)
                    {
                        DrawRectangle(static_cast<int>(view.bounds.x) + x * document_.settings.cellSize * canvasZoom_,
                                      static_cast<int>(view.bounds.y) + y * document_.settings.cellSize * canvasZoom_,
                                      document_.settings.cellSize * canvasZoom_, document_.settings.cellSize * canvasZoom_, preview);
                    }
                }
            }
        }

        void UpdateHoverStatus(const Vector2 &mouse)
        {
            if (CheckCollisionPointRec(mouse, CanvasBounds()))
            {
                const Vector2 grid = CanvasToGrid(mouse);
                const int gx = std::clamp(static_cast<int>(grid.x), 0, document_.settings.width - 1);
                const int gy = std::clamp(static_cast<int>(grid.y), 0, document_.settings.height - 1);
                const PixelPaint *pixel = FindPixelAt(gx, gy);
                const Color color = pixel ? ToColor(pixel->color) : BLANK;
                SetHoverDebug(TextFormat("Cell: %d,%d Color: %s", gx, gy, ColorToHex(color).c_str()));
                return;
            }

            for (size_t i = 0; i < palette_.size(); ++i)
            {
                const int row = static_cast<int>(i / 6);
                const int col = static_cast<int>(i % 6);
                const Rectangle cell{24.0f + col * 44.0f, 368.0f + row * 44.0f, 36.0f, 36.0f};
                if (CheckCollisionPointRec(mouse, cell))
                {
                    const Color c = palette_[i];
                    SetStatus(TextFormat("Palette: %s", ColorToHex(c).c_str()));
                    return;
                }
            }
        }

        void RebuildCanvasTexture()
        {
            if (canvasTexture_.id == 0)
            {
                return;
            }

            BeginTextureMode(canvasTexture_);
            ClearBackground(Color{240, 240, 242, 255});

            for (const auto &pixel : document_.pixels)
            {
                const Rectangle cell{static_cast<float>(pixel.x * document_.settings.cellSize),
                                     static_cast<float>(pixel.y * document_.settings.cellSize),
                                     static_cast<float>(document_.settings.cellSize), static_cast<float>(document_.settings.cellSize)};
                DrawRectangleRec(cell, ToColor(pixel.color));
            }

            EndTextureMode();
            canvasDirty_ = false;
        }

        
        void HandleExport()
        {
            const fs::path outDir = fs::current_path() / "utils";
            std::error_code ec;
            fs::create_directories(outDir, ec);
            const std::string functionName = "DrawGeneratedCanvas";
            const bool cppOk = CanvasExporter::SaveCpp(outDir / "generated_canvas.cpp", document_, functionName);
            if (cppOk)
            {
                SetClipboardText(CanvasExporter::ExportCpp(document_, functionName).c_str());
            }
            SetStatus(cppOk ? "Exported C++" : "Export failed");
        }

        void ImportPng()
        {
            if (pngPathInput_.empty())
            {
                SetStatus("PNG path empty");
                return;
            }

            const fs::path path = fs::path(pngPathInput_);
            if (CanvasExporter::LoadPng(path, document_))
            {
                const float fitW = static_cast<float>(std::max(1, GetScreenWidth() - 344)) / std::max(1, document_.settings.width * document_.settings.cellSize);
                const float fitH = static_cast<float>(std::max(1, GetScreenHeight() - 48)) / std::max(1, document_.settings.height * document_.settings.cellSize);
                canvasZoom_ = std::clamp(std::min(fitW, fitH), kMinCanvasZoom, kMaxCanvasZoom);
                canvasPan_ = {0.0f, 0.0f};
                EnsureCanvasTexture();
                SetStatus("PNG loaded and fit to view");
            }
            else
            {
                SetStatus(TextFormat("PNG load failed: %s", path.string().c_str()));
            }
        }

        SelectionRect AutoTrimTransparentBounds() const
        {
            int minX = document_.settings.width;
            int minY = document_.settings.height;
            int maxX = -1;
            int maxY = -1;

            for (const auto &pixel : document_.pixels)
            {
                if (pixel.color.a == 0)
                {
                    continue;
                }
                minX = std::min(minX, pixel.x);
                minY = std::min(minY, pixel.y);
                maxX = std::max(maxX, pixel.x);
                maxY = std::max(maxY, pixel.y);
            }

            if (maxX < minX || maxY < minY)
            {
                return SelectionRect{};
            }

            return SelectionRect{minX, minY, maxX - minX + 1, maxY - minY + 1};
        }

        void AutoCropTransparentBounds()
        {
            const SelectionRect selection = AutoTrimTransparentBounds();
            if (!selection.IsValid())
            {
                SetStatus("Auto crop skipped: no visible pixels");
                return;
            }

            document_.CropToSelection(selection);
            ClearUndoStack();
            EnsureCanvasTexture();
            MarkCanvasDirty();
            SetStatus(TextFormat("Auto-cropped to %dx%d", selection.width, selection.height));
        }

        void ExportPng()
        {
            if (pngExportPath_.empty())
            {
                SetStatus("PNG export path empty");
                return;
            }

            fs::path path = fs::path(pngExportPath_);
            if (path.has_extension() == false)
            {
                if (path.empty() || path.filename().empty() || path.string().back() == '\\' || path.string().back() == '/')
                {
                    path /= "canvas.png";
                }
                else
                {
                    path += ".png";
                }
            }

            std::error_code ec;
            if (path.has_parent_path())
            {
                fs::create_directories(path.parent_path(), ec);
            }

            if (CanvasExporter::SavePng(path, document_))
            {
                SetStatus(TextFormat("PNG exported: %s", path.string().c_str()));
                pngExportPath_ = path.string();
            }
            else
            {
                SetStatus(TextFormat("PNG export failed: %s", path.string().c_str()));
            }
        }

        std::string AnchorModeName() const
        {
            switch (anchorMode_)
            {
            case AnchorMode::Center:
                return "Center";
            case AnchorMode::TopLeft:
                return "Top-Left";
            case AnchorMode::Selection:
                return "Selection";
            case AnchorMode::Mouse:
                return "Mouse";
            }
            return "Center";
        }

        void CycleAnchorMode()
        {
            switch (anchorMode_)
            {
            case AnchorMode::Center:
                anchorMode_ = AnchorMode::TopLeft;
                break;
            case AnchorMode::TopLeft:
                anchorMode_ = AnchorMode::Selection;
                break;
            case AnchorMode::Selection:
                anchorMode_ = AnchorMode::Mouse;
                break;
            case AnchorMode::Mouse:
                anchorMode_ = AnchorMode::Center;
                break;
            }
            SetStatus(TextFormat("Anchor: %s", AnchorModeName().c_str()));
        }

        void ResizeCanvas(int newWidth, int newHeight, const Vector2 &mouse = Vector2{0.0f, 0.0f})
        {
            newWidth = std::clamp(newWidth, 4, 256);
            newHeight = std::clamp(newHeight, 4, 256);
            const int oldWidth = document_.settings.width;
            const int oldHeight = document_.settings.height;
            if (newWidth == oldWidth && newHeight == oldHeight)
            {
                return;
            }

            int anchorX = 0;
            int anchorY = 0;
            int newAnchorX = 0;
            int newAnchorY = 0;

            switch (anchorMode_)
            {
            case AnchorMode::Center:
                anchorX = oldWidth / 2;
                anchorY = oldHeight / 2;
                newAnchorX = newWidth / 2;
                newAnchorY = newHeight / 2;
                break;
            case AnchorMode::TopLeft:
                anchorX = 0;
                anchorY = 0;
                newAnchorX = 0;
                newAnchorY = 0;
                break;
            case AnchorMode::Selection:
                if (selection_.IsValid())
                {
                    anchorX = selection_.x + selection_.width / 2;
                    anchorY = selection_.y + selection_.height / 2;
                    newAnchorX = newWidth / 2;
                    newAnchorY = newHeight / 2;
                }
                else
                {
                    anchorX = oldWidth / 2;
                    anchorY = oldHeight / 2;
                    newAnchorX = newWidth / 2;
                    newAnchorY = newHeight / 2;
                }
                break;
            case AnchorMode::Mouse:
            {
                const Vector2 grid = CanvasToGrid(mouse);
                anchorX = std::clamp(static_cast<int>(grid.x), 0, oldWidth - 1);
                anchorY = std::clamp(static_cast<int>(grid.y), 0, oldHeight - 1);
                newAnchorX = std::clamp(static_cast<int>(grid.x), 0, newWidth - 1);
                newAnchorY = std::clamp(static_cast<int>(grid.y), 0, newHeight - 1);
                break;
            }
            }

            const int shiftX = newAnchorX - anchorX;
            const int shiftY = newAnchorY - anchorY;

            std::vector<PixelPaint> resizedPixels;
            resizedPixels.reserve(document_.pixels.size());
            for (const auto &pixel : document_.pixels)
            {
                const int nx = pixel.x + shiftX;
                const int ny = pixel.y + shiftY;
                if (nx >= 0 && ny >= 0 && nx < newWidth && ny < newHeight)
                {
                    resizedPixels.push_back(PixelPaint{nx, ny, pixel.color});
                }
            }

            document_.settings.width = newWidth;
            document_.settings.height = newHeight;
            document_.pixels = std::move(resizedPixels);
            if (!selection_.IsValid())
            {
                selection_ = {};
                hasSelection_ = false;
            }
            EnsureCanvasTexture();
            MarkCanvasDirty();
            SetStatus(TextFormat("Canvas resized (%s)", AnchorModeName().c_str()));
        }

        void ToggleFullscreenMode()
        {
            fullscreen_ = !fullscreen_;
            if (fullscreen_)
            {
                MaximizeWindow();
            }
            else
            {
                RestoreWindow();
            }
            MarkCanvasDirty();
        }

        void HandleSidebarClick(Vector2 mouse)
        {
            const Rectangle toolRects[] = {{24, 150, 260, 30}, {24, 186, 260, 30}, {24, 222, 260, 30}, {24, 258, 260, 30}, {24, 294, 260, 30}};
            for (int i = 0; i < 5; ++i)
            {
                if (CheckCollisionPointRec(mouse, toolRects[i]))
                {
                    tool_ = static_cast<ToolType>(i);
                    SetStatus("Tool changed");
                    return;
                }
            }

            const auto hit = [mouse](Rectangle rect) { return CheckCollisionPointRec(mouse, rect); };

            if (hit(Rectangle{24, 724, 260, 28}))
            {
                settingsOpen_ = !settingsOpen_;
                SetStatus(settingsOpen_ ? "Settings opened" : "Settings closed");
                return;
            }
            if (!settingsOpen_)
            {
                return;
            }
            if (hit(Rectangle{24, 792, 260, 28}))
            {
                brushThickness_ = brushThickness_ == 1 ? 2 : brushThickness_ == 2 ? 3 : 1;
                SetStatus(TextFormat("Brush thickness set to %dx%d", brushThickness_, brushThickness_));
                return;
            }
            if (hit(Rectangle{24, 826, 260, 28}))
            {
                CycleAnchorMode();
                return;
            }
            if (hit(Rectangle{24, 958, 120, 30}))
            {
                ResizeCanvas(document_.settings.width + 8, document_.settings.height, mouse);
                return;
            }
            if (hit(Rectangle{164, 958, 120, 30}))
            {
                ResizeCanvas(document_.settings.width - 8, document_.settings.height, mouse);
                return;
            }
            if (hit(Rectangle{24, 994, 120, 30}))
            {
                ResizeCanvas(document_.settings.width, document_.settings.height + 8, mouse);
                return;
            }
            if (hit(Rectangle{164, 994, 120, 30}))
            {
                ResizeCanvas(document_.settings.width, document_.settings.height - 8, mouse);
                return;
            }
            if (hit(Rectangle{24, 1030, 120, 30}))
            {
                ZoomAtMouse(1.1f, mouse);
                return;
            }
            if (hit(Rectangle{164, 1030, 120, 30}))
            {
                ZoomAtMouse(1.0f / 1.1f, mouse);
                return;
            }
            if (hit(Rectangle{24, 1066, 120, 28}))
            {
                const std::string path = WindowsFileDialog::OpenPngFile();
                if (!path.empty())
                {
                    pngPathInput_ = path;
                    ImportPng();
                }
                return;
            }
            if (hit(Rectangle{164, 1066, 120, 28}))
            {
                const std::string path = WindowsFileDialog::SavePngFile();
                if (!path.empty())
                {
                    pngExportPath_ = path;
                    ExportPng();
                }
                return;
            }
            if (hit(Rectangle{24, 1102, 120, 28}))
            {
                AutoCropTransparentBounds();
                return;
            }
            if (hit(Rectangle{164, 1102, 120, 28}))
            {
                if (selection_.IsValid())
                {
                    document_.CropToSelection(selection_);
                    selection_ = {};
                    hasSelection_ = false;
                    EnsureCanvasTexture();
                    MarkCanvasDirty();
                    SetStatus("Cropped to selection");
                }
                else
                {
                    SetStatus("No selection to crop");
                }
                return;
            }
            if (hit(Rectangle{24, 1138, 120, 28}))
            {
                if (selection_.IsValid())
                {
                    const size_t removed = document_.EraseSelection(selection_);
                    MarkCanvasDirty();
                    SetStatus(TextFormat("Erased %zu pixels", removed));
                }
                else
                {
                    SetStatus("No selection to erase");
                }
                return;
            }
            if (hit(Rectangle{164, 1138, 120, 28}))
            {
                if (selection_.IsValid())
                {
                    const size_t removed = document_.KeepSelectionOnly(selection_);
                    MarkCanvasDirty();
                    SetStatus(TextFormat("Removed %zu pixels outside selection", removed));
                }
                else
                {
                    SetStatus("No selection to keep");
                }
                return;
            }
            if (hit(Rectangle{24, 1174, 120, 28}))
            {
                if (selection_.IsValid())
                {
                    document_.FlipHorizontalInSelection(selection_);
                    MarkCanvasDirty();
                    SetStatus("Flipped selection horizontally");
                }
                else
                {
                    document_.FlipHorizontal();
                    MarkCanvasDirty();
                    SetStatus("Flipped horizontally");
                }
                return;
            }
            if (hit(Rectangle{164, 1174, 120, 28}))
            {
                if (selection_.IsValid())
                {
                    document_.FlipVerticalInSelection(selection_);
                    MarkCanvasDirty();
                    SetStatus("Flipped selection vertically");
                }
                else
                {
                    document_.FlipVertical();
                    MarkCanvasDirty();
                    SetStatus("Flipped vertically");
                }
                return;
            }
            if (hit(Rectangle{24, 1210, 120, 28}))
            {
                selection_ = {};
                hasSelection_ = false;
                movingSelection_ = false;
                SetStatus("Selection cleared");
                return;
            }
            if (hit(Rectangle{164, 1210, 120, 28}))
            {
                if (selection_.IsValid())
                {
                    document_.ShiftSelection(selection_, 1, 0);
                    selection_.x += 1;
                    MarkCanvasDirty();
                    SetStatus("Shifted selection right by 1");
                }
                else
                {
                    SetStatus("No selection to shift");
                }
                return;
            }
            if (hit(Rectangle{24, 1246, 260, 28}))
            {
                HandleExport();
                return;
            }
            if (hit(Rectangle{24, 1284, 260, 28}))
            {
                document_.Clear();
                selection_ = {};
                hasSelection_ = false;
                movingSelection_ = false;
                pointerMode_ = PointerMode::Idle;
                MarkCanvasDirty();
                SetStatus("Canvas cleared");
                return;
            }
            for (size_t i = 0; i < palette_.size(); ++i)
            {
                const int row = static_cast<int>(i / 6);
                const int col = static_cast<int>(i % 6);
                const Rectangle cell{24.0f + col * 44.0f, 368.0f + row * 44.0f, 36.0f, 36.0f};
                if (CheckCollisionPointRec(mouse, cell))
                {
                    activeColor_ = palette_[i];
                    SetStatus(TextFormat("Color selected: %s", ColorToHex(activeColor_).c_str()));
                    SetHoverDebug(TextFormat("Palette hover: %s", ColorToHex(activeColor_).c_str()));
                    return;
                }
            }
        }

        void Update()
        {
            const Vector2 mouse = GetMousePosition();
            UpdateHoverStatus(mouse);
            const bool panModifier = IsKeyDown(KEY_SPACE);
            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
            {
                if (IsKeyPressed(KEY_Z))
                {
                    RestoreUndo();
                    return;
                }
            }

            if (IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) || (panModifier && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
            {
                if (InCanvas(mouse))
                {
                    pointerMode_ = PointerMode::Panning;
                    dragStartMouse_ = mouse;
                    dragStartPan_ = canvasPan_;
                }
            }
            else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (InCanvas(mouse))
                {
                    dragStartMouse_ = mouse;
                    dragStartPan_ = canvasPan_;
                    const Vector2 grid = CanvasToGrid(mouse);
                    const int gx = static_cast<int>(grid.x);
                    const int gy = static_cast<int>(grid.y);
                    const bool insideSelection = hasSelection_ && selection_.IsValid() &&
                                                 gx >= selection_.x && gy >= selection_.y &&
                                                 gx < selection_.x + selection_.width &&
                                                 gy < selection_.y + selection_.height;

                    if (tool_ == ToolType::Select && insideSelection)
                    {
                        pointerMode_ = PointerMode::Panning;
                        movingSelection_ = true;
                        selectionMoveStartGrid_ = Vector2{static_cast<float>(selection_.x), static_cast<float>(selection_.y)};
                    }
                    else if (tool_ == ToolType::Select)
                    {
                        pointerMode_ = PointerMode::Selecting;
                        selectionStart_ = mouse;
                        selectionEnd_ = mouse;
                        movingSelection_ = false;
                    }
                    else
                    {
                        pointerMode_ = PointerMode::Drawing;
                        strokeStart_ = mouse;
                        if (tool_ == ToolType::Pixel)
                        {
                            PaintCurrentTool(mouse);
                        }
                    }
                }
                else
                {
                    HandleSidebarClick(mouse);
                }
            }
            if (pointerMode_ == PointerMode::Drawing && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && tool_ == ToolType::Pixel)
            {
                PaintCurrentTool(mouse);
            }
            if (pointerMode_ == PointerMode::Selecting && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            {
                selectionEnd_ = mouse;
            }
            if (pointerMode_ == PointerMode::Panning && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                if (movingSelection_)
                {
                    const float cellStep = static_cast<float>(document_.settings.cellSize) * canvasZoom_;
                    const int dx = static_cast<int>(std::lround((mouse.x - dragStartMouse_.x) / cellStep));
                    const int dy = static_cast<int>(std::lround((mouse.y - dragStartMouse_.y) / cellStep));
                    const int newX = std::clamp(static_cast<int>(selectionMoveStartGrid_.x) + dx, 0, document_.settings.width - selection_.width);
                    const int newY = std::clamp(static_cast<int>(selectionMoveStartGrid_.y) + dy, 0, document_.settings.height - selection_.height);
                    const int applyDx = newX - selection_.x;
                    const int applyDy = newY - selection_.y;
                    if (applyDx != 0 || applyDy != 0)
                    {
                        document_.ShiftSelection(selection_, applyDx, applyDy);
                        selection_.x = newX;
                        selection_.y = newY;
                        MarkCanvasDirty();
                    }
                }
                else
                {
                    canvasPan_.x = dragStartPan_.x + (mouse.x - dragStartMouse_.x);
                    canvasPan_.y = dragStartPan_.y + (mouse.y - dragStartMouse_.y);
                    MarkCanvasDirty();
                }
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
            {
                if (pointerMode_ == PointerMode::Selecting)
                {
                    selection_ = MakeSelectionFromPoints(selectionStart_, selectionEnd_);
                    hasSelection_ = selection_.IsValid();
                    SetStatus(hasSelection_ ? "Selection set" : "Selection cleared");
                }
                else if (pointerMode_ == PointerMode::Drawing && tool_ != ToolType::Pixel)
                {
                    BeginUndoIfNeeded();
                    CommitShape(strokeStart_, mouse);
                }
                else if (pointerMode_ == PointerMode::Panning && movingSelection_ && selection_.IsValid())
                {
                    SetStatus(TextFormat("Moved selection to %d,%d", selection_.x, selection_.y));
                }
                pointerMode_ = PointerMode::Idle;
                movingSelection_ = false;
                EndUndoAction();
            }
            if (IsKeyPressed(KEY_C))
            {
                BeginUndoIfNeeded();
                document_.Clear();
                selection_ = {};
                hasSelection_ = false;
                pointerMode_ = PointerMode::Idle;
                MarkCanvasDirty();
                SetStatus("Canvas cleared");
                EndUndoAction();
            }
            if (IsKeyPressed(KEY_E))
            {
                HandleExport();
            }
            if (IsKeyPressed(KEY_A))
            {
                BeginUndoIfNeeded();
                AutoCropTransparentBounds();
                EndUndoAction();
            }
            if (IsKeyPressed(KEY_DELETE))
            {
                BeginUndoIfNeeded();
                if (selection_.IsValid())
                {
                    const size_t removed = document_.EraseSelection(selection_);
                    selection_ = {};
                    hasSelection_ = false;
                    MarkCanvasDirty();
                    SetStatus(TextFormat("Deleted %zu pixels in selection", removed));
                    EndUndoAction();
                }
                else
                {
                    selection_ = {};
                    hasSelection_ = false;
                    SetStatus("Selection cleared");
                    EndUndoAction();
                }
            }
            if (IsKeyPressed(KEY_H))
            {
                BeginUndoIfNeeded();
                if (selection_.IsValid())
                {
                    document_.FlipHorizontalInSelection(selection_);
                    MarkCanvasDirty();
                    SetStatus("Flipped selection horizontally");
                }
                else
                {
                    document_.FlipHorizontal();
                    MarkCanvasDirty();
                    SetStatus("Flipped horizontally");
                }
                EndUndoAction();
            }
            if (IsKeyPressed(KEY_V))
            {
                BeginUndoIfNeeded();
                if (selection_.IsValid())
                {
                    document_.FlipVerticalInSelection(selection_);
                    MarkCanvasDirty();
                    SetStatus("Flipped selection vertically");
                }
                else
                {
                    document_.FlipVertical();
                    MarkCanvasDirty();
                    SetStatus("Flipped vertically");
                }
                EndUndoAction();
            }
            if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL))
            {
                brushSize_ = std::min(16, brushSize_ + 1);
            }
            if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS))
            {
                brushSize_ = std::max(1, brushSize_ - 1);
            }
            if (IsKeyPressed(KEY_LEFT_BRACKET))
            {
                ZoomAtMouse(1.0f / 1.1f, GetMousePosition());
            }
            if (IsKeyPressed(KEY_RIGHT_BRACKET))
            {
                ZoomAtMouse(1.1f, GetMousePosition());
            }
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
            {
                const Vector2 mouse = GetMousePosition();
                if (InCanvas(mouse))
                {
                    const Vector2 canvasLocalBefore = ScreenToCanvasLocal(mouse);
                    const float zoomStep = wheel > 0.0f ? 1.1f : 1.0f / 1.1f;
                    canvasZoom_ = std::clamp(canvasZoom_ * zoomStep, kMinCanvasZoom, kMaxCanvasZoom);
                    const CanvasView after = GetCanvasView();
                    const Vector2 mouseAfter{after.bounds.x + canvasLocalBefore.x * after.scale,
                                             after.bounds.y + canvasLocalBefore.y * after.scale};
                    canvasPan_.x += mouse.x - mouseAfter.x;
                    canvasPan_.y += mouse.y - mouseAfter.y;
                    MarkCanvasDirty();
                }
            }
            if (IsKeyPressed(KEY_F11))
            {
                ToggleFullscreenMode();
            }

            if (editingOpenPath_ || editingSavePath_)
            {
                if (IsKeyPressed(KEY_BACKSPACE))
                {
                    if (editingOpenPath_)
                    {
                        if (!pngPathInput_.empty())
                        {
                            pngPathInput_.pop_back();
                        }
                    }
                    else if (editingSavePath_)
                    {
                        if (!pngExportPath_.empty())
                        {
                            pngExportPath_.pop_back();
                        }
                    }
                }

                if (IsKeyPressed(KEY_ENTER))
                {
                    editingOpenPath_ = false;
                    editingSavePath_ = false;
                }

                int key = GetCharPressed();
                while (key > 0)
                {
                    if (key >= 32 && key < 127)
                    {
                        if (editingOpenPath_)
                        {
                            pngPathInput_.push_back(static_cast<char>(key));
                        }
                        else if (editingSavePath_)
                        {
                            pngExportPath_.push_back(static_cast<char>(key));
                        }
                    }
                    key = GetCharPressed();
                }
            }

            UpdateHoverStatus(mouse);

            if (canvasDirty_)
            {
                RebuildCanvasTexture();
            }
        }

        void DrawButton(const Rectangle &rect, const char *text, bool active = false) const
        {
            DrawRectangleRec(rect, active ? Color{66, 90, 120, 255} : Color{44, 48, 58, 255});
            DrawRectangleLinesEx(rect, 1.0f, Color{90, 96, 110, 255});
            DrawText(text, static_cast<int>(rect.x + 12), static_cast<int>(rect.y + 7), 18, RAYWHITE);
        }

        void DrawPanelHeader(const Rectangle &rect, const char *text) const
        {
            DrawRectangleRec(rect, Color{35, 38, 46, 255});
            DrawRectangleLinesEx(rect, 1.0f, Color{74, 78, 88, 255});
            DrawText(text, static_cast<int>(rect.x + 10), static_cast<int>(rect.y + 6), 18, RAYWHITE);
        }

        void DrawCanvas() const
        {
            const Rectangle bounds = CanvasBounds();
            DrawRectangleRec(bounds, Color{40, 40, 44, 255});
            DrawRectangleLinesEx(bounds, 2.0f, Color{120, 120, 130, 255});

            if (canvasTexture_.id != 0)
            {
                const Texture2D tex = canvasTexture_.texture;
                DrawTexturePro(tex, Rectangle{0, 0, static_cast<float>(tex.width), -static_cast<float>(tex.height)}, bounds, Vector2{0, 0}, 0.0f, WHITE);
            }

            const float gridStep = static_cast<float>(document_.settings.cellSize) * static_cast<float>(canvasZoom_);
            for (int y = 0; y < document_.settings.height; ++y)
            {
                for (int x = 0; x < document_.settings.width; ++x)
                {
                    DrawRectangleLinesEx(Rectangle{bounds.x + x * gridStep, bounds.y + y * gridStep, gridStep, gridStep}, 1.0f,
                                         Color{220, 220, 224, 120});
                }
            }

            if (hasSelection_)
            {
                const Rectangle sel = SelectionToScreenRect(selection_);
                DrawRectangleRec(sel, Color{80, 140, 220, 40});
                DrawRectangleLinesEx(sel, 2.0f, Color{120, 190, 255, 220});
            }

            if (pointerMode_ == PointerMode::Selecting && tool_ == ToolType::Select)
            {
                const Rectangle sel = SelectionToScreenRect(MakeSelectionFromPoints(selectionStart_, selectionEnd_));
                DrawRectangleLinesEx(sel, 2.0f, Color{255, 240, 160, 255});
            }

            if (pointerMode_ == PointerMode::Drawing && tool_ != ToolType::Pixel && tool_ != ToolType::Select)
            {
                const Vector2 mouse = GetMousePosition();
                if (tool_ == ToolType::Rectangle)
                {
                    const Vector2 a = ScreenToCanvasLocal(strokeStart_);
                    const Vector2 b = ScreenToCanvasLocal(mouse);
                    const float x = bounds.x + std::min(a.x, b.x) * canvasZoom_;
                    const float y = bounds.y + std::min(a.y, b.y) * canvasZoom_;
                    const float w = std::abs(b.x - a.x) * canvasZoom_;
                    const float h = std::abs(b.y - a.y) * canvasZoom_;
                    DrawRectangleLinesEx(Rectangle{x, y, w, h}, 2.0f, activeColor_);
                }
                else if (tool_ == ToolType::Circle)
                {
                    DrawCirclePreview(strokeStart_, mouse);
                }
                else if (tool_ == ToolType::Stroke)
                {
                    DrawLineEx(strokeStart_, mouse, static_cast<float>(brushSize_), activeColor_);
                }
            }
        }

        void Draw() const
        {
            DrawText("Canvas Export Tool", 24, 24, 28, RAYWHITE);
            DrawText("LMB: draw | E: export | C: clear | +/-: brush size | [ ]: zoom | F11: fullscreen", 24, 60, 18, Color{180, 185, 200, 255});
            DrawText(TextFormat("Mode: %s", fullscreen_ ? "Fullscreen" : "Windowed"), 24, 86, 16, Color{180, 185, 200, 255});

            DrawPanelHeader(Rectangle{24, 118, 260, 28}, "Tools");
            DrawButton(Rectangle{24, 150, 260, 30}, "Pixel Brush", tool_ == ToolType::Pixel);
            DrawButton(Rectangle{24, 186, 260, 30}, "Rectangle", tool_ == ToolType::Rectangle);
            DrawButton(Rectangle{24, 222, 260, 30}, "Circle", tool_ == ToolType::Circle);
            DrawButton(Rectangle{24, 258, 260, 30}, "Line", tool_ == ToolType::Stroke);
            DrawButton(Rectangle{24, 294, 260, 30}, "Select Tool", tool_ == ToolType::Select);

            DrawPanelHeader(Rectangle{24, 334, 260, 28}, "Palette");
            for (size_t i = 0; i < palette_.size(); ++i)
            {
                const int row = static_cast<int>(i / 6);
                const int col = static_cast<int>(i % 6);
                const Rectangle cell{24.0f + col * 44.0f, 368.0f + row * 44.0f, 36.0f, 36.0f};
                DrawRectangleRec(cell, palette_[i]);
                DrawRectangleLinesEx(cell, 2.0f, Color{60, 60, 70, 255});
            }

            DrawPanelHeader(Rectangle{24, 476, 260, 28}, "Info");
            DrawText(TextFormat("Brush: %d", brushSize_), 24, 512, 18, RAYWHITE);
            DrawText(TextFormat("Canvas: %dx%d", document_.settings.width, document_.settings.height), 24, 536, 18, RAYWHITE);
            DrawText(TextFormat("Zoom: %.2fx", canvasZoom_), 24, 560, 18, RAYWHITE);
            DrawText(TextFormat("Selection: %s", hasSelection_ ? "ON" : "OFF"), 24, 584, 16, hasSelection_ ? SKYBLUE : Color{180, 185, 200, 255});
            if (selection_.IsValid())
            {
                DrawText(TextFormat("Selection: %d,%d,%d,%d", selection_.x, selection_.y, selection_.width, selection_.height),
                         24, 606, 16, Color{140, 220, 255, 255});
            }
            if (!debugHover_.empty())
            {
                DrawText(debugHover_.c_str(), 24, 628, 14, Color{220, 220, 160, 255});
            }
            if (!debugPaint_.empty())
            {
                DrawText(debugPaint_.c_str(), 24, 648, 14, Color{220, 180, 160, 255});
            }
            DrawText(TextFormat("PNG open: %s", pngPathInput_.c_str()), 24, 670, 14, Color{180, 185, 200, 255});
            DrawText(TextFormat("PNG save: %s", pngExportPath_.c_str()), 24, 690, 14, Color{180, 185, 200, 255});

            DrawPanelHeader(Rectangle{24, 724, 260, 28}, "Settings");
            DrawButton(Rectangle{24, 758, 260, 28}, TextFormat("Settings: %s", settingsOpen_ ? "Open" : "Closed"), false);
            if (settingsOpen_)
            {
                DrawButton(Rectangle{24, 792, 260, 28}, TextFormat("Brush thickness: %dx%d", brushThickness_, brushThickness_), false);
                DrawButton(Rectangle{24, 826, 260, 28}, TextFormat("Anchor: %s", AnchorModeName().c_str()), false);
            }

            DrawPanelHeader(Rectangle{24, 888, 260, 28}, "Canvas Ops");
            DrawButton(Rectangle{24, 922, 260, 28}, "Fit Zoom", false);
            DrawButton(Rectangle{24, 958, 120, 30}, "+ W", false);
            DrawButton(Rectangle{164, 958, 120, 30}, "- W", false);
            DrawButton(Rectangle{24, 994, 120, 30}, "+ H", false);
            DrawButton(Rectangle{164, 994, 120, 30}, "- H", false);
            DrawButton(Rectangle{24, 1030, 120, 30}, "+ Zoom", false);
            DrawButton(Rectangle{164, 1030, 120, 30}, "- Zoom", false);
            DrawButton(Rectangle{24, 1066, 120, 28}, "Open PNG", false);
            DrawButton(Rectangle{164, 1066, 120, 28}, "Save PNG", false);
            DrawButton(Rectangle{24, 1102, 120, 28}, "Auto Crop", false);
            DrawButton(Rectangle{164, 1102, 120, 28}, "Crop Sel", false);
            DrawButton(Rectangle{24, 1138, 120, 28}, "Erase Sel", false);
            DrawButton(Rectangle{164, 1138, 120, 28}, "Keep Only", false);
            DrawButton(Rectangle{24, 1174, 120, 28}, "Flip H", false);
            DrawButton(Rectangle{164, 1174, 120, 28}, "Flip V", false);
            DrawButton(Rectangle{24, 1210, 120, 28}, "Clear Sel", false);
            DrawButton(Rectangle{164, 1210, 120, 28}, "Shift", false);
            DrawButton(Rectangle{24, 1246, 260, 28}, "Export C++", false);
            DrawButton(Rectangle{24, 1284, 260, 28}, "Clear", false);

            const char *selText = hasSelection_ ? TextFormat("Selection: %d,%d,%d,%d", selection_.x, selection_.y, selection_.width, selection_.height) : "Selection: OFF";

            DrawCanvas();
            DrawText(selText, 24, GetScreenHeight() - 48, 18, hasSelection_ ? SKYBLUE : Color{220, 220, 230, 255});
            DrawText(status_.c_str(), 24, GetScreenHeight() - 24, 18, Color{220, 220, 230, 255});
        }
    };

    CanvasApp::CanvasApp() = default;
    void CanvasApp::Run()
    {
        CanvasAppImpl app;
        app.Run();
    }
}
