#include "ImagePixelizer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace canvas
{
    namespace
    {
        float SrgbToLinear(float v)
        {
            const float c = v / 255.0f;
            if (c <= 0.04045f)
            {
                return c / 12.92f;
            }
            return std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        unsigned char LinearToSrgb(float v)
        {
            v = std::clamp(v, 0.0f, 1.0f);
            float c = 0.0f;
            if (v <= 0.0031308f)
            {
                c = 12.92f * v;
            }
            else
            {
                c = 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
            }
            return static_cast<unsigned char>(std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f));
        }

        ColorRGBA MakeColorFromAccum(double r, double g, double b, double a, double weight)
        {
            if (weight <= 0.0)
            {
                return ColorRGBA{0, 0, 0, 0};
            }
            return ColorRGBA{LinearToSrgb(static_cast<float>(r / weight)),
                             LinearToSrgb(static_cast<float>(g / weight)),
                             LinearToSrgb(static_cast<float>(b / weight)),
                             static_cast<unsigned char>(std::lround(std::clamp(a / weight, 0.0, 255.0)))};
        }

        float Luma(const Color &c)
        {
            return 0.2126f * SrgbToLinear(static_cast<float>(c.r)) +
                   0.7152f * SrgbToLinear(static_cast<float>(c.g)) +
                   0.0722f * SrgbToLinear(static_cast<float>(c.b));
        }

        float SampleEdgeMagnitude(const Color *pixels, int w, int h, int x, int y)
        {
            const auto get = [pixels, w, h](int ix, int iy) -> float {
                ix = std::clamp(ix, 0, w - 1);
                iy = std::clamp(iy, 0, h - 1);
                return Luma(pixels[iy * w + ix]);
            };

            const float gx = -get(x - 1, y - 1) + get(x + 1, y - 1) - 2.0f * get(x - 1, y) + 2.0f * get(x + 1, y) - get(x - 1, y + 1) + get(x + 1, y + 1);
            const float gy = -get(x - 1, y - 1) - 2.0f * get(x, y - 1) - get(x + 1, y - 1) + get(x - 1, y + 1) + 2.0f * get(x, y + 1) + get(x + 1, y + 1);
            return std::sqrt(gx * gx + gy * gy);
        }

        struct RGBAKey
        {
            unsigned char r{};
            unsigned char g{};
            unsigned char b{};
            unsigned char a{};

            bool operator<(const RGBAKey &rhs) const
            {
                if (r != rhs.r) return r < rhs.r;
                if (g != rhs.g) return g < rhs.g;
                if (b != rhs.b) return b < rhs.b;
                return a < rhs.a;
            }
        };
    }

    bool ImagePixelizer::PixelizeImage(const std::filesystem::path &path,
                                       CanvasDocument &doc,
                                       int targetWidth,
                                       int targetHeight,
                                       PixelizeMode mode,
                                       bool useLinearColor)
    {
        Image image = LoadImage(path.string().c_str());
        if (image.data == nullptr)
        {
            return false;
        }

        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Color *pixels = LoadImageColors(image);
        if (pixels == nullptr)
        {
            UnloadImage(image);
            return false;
        }

        targetWidth = std::clamp(targetWidth, 1, 1024);
        targetHeight = std::clamp(targetHeight, 1, 1024);

        doc.Clear();
        doc.settings.width = targetWidth;
        doc.settings.height = targetHeight;
        doc.settings.cellSize = 1;
        doc.pixels.reserve(static_cast<size_t>(targetWidth * targetHeight));

        std::vector<float> edgeWeights;
        if (mode == PixelizeMode::EdgeAware)
        {
            edgeWeights.resize(static_cast<size_t>(image.width * image.height), 0.0f);
            for (int y = 0; y < image.height; ++y)
            {
                for (int x = 0; x < image.width; ++x)
                {
                    const float edge = SampleEdgeMagnitude(pixels, image.width, image.height, x, y);
                    edgeWeights[static_cast<size_t>(y * image.width + x)] = std::clamp(edge * 2.2f, 0.0f, 1.0f);
                }
            }
        }

        const float srcW = static_cast<float>(image.width);
        const float srcH = static_cast<float>(image.height);

        for (int y = 0; y < targetHeight; ++y)
        {
            const float y0f = static_cast<float>(y) * srcH / static_cast<float>(targetHeight);
            const float y1f = static_cast<float>(y + 1) * srcH / static_cast<float>(targetHeight);
            const int y0 = std::clamp(static_cast<int>(std::floor(y0f)), 0, image.height - 1);
            const int y1 = std::clamp(static_cast<int>(std::ceil(y1f)), y0 + 1, image.height);

            for (int x = 0; x < targetWidth; ++x)
            {
                const float x0f = static_cast<float>(x) * srcW / static_cast<float>(targetWidth);
                const float x1f = static_cast<float>(x + 1) * srcW / static_cast<float>(targetWidth);
                const int x0 = std::clamp(static_cast<int>(std::floor(x0f)), 0, image.width - 1);
                const int x1 = std::clamp(static_cast<int>(std::ceil(x1f)), x0 + 1, image.width);

                ColorRGBA out{0, 0, 0, 255};

                if (mode == PixelizeMode::CenterSample)
                {
                    const int sx = std::clamp(static_cast<int>(std::lround((x0f + x1f) * 0.5f)), 0, image.width - 1);
                    const int sy = std::clamp(static_cast<int>(std::lround((y0f + y1f) * 0.5f)), 0, image.height - 1);
                    out = ToRGBA(pixels[sy * image.width + sx]);
                }
                else if (mode == PixelizeMode::DominantColor)
                {
                    std::map<RGBAKey, int> histogram;
                    for (int sy = y0; sy < y1; ++sy)
                    {
                        for (int sx = x0; sx < x1; ++sx)
                        {
                            const Color c = pixels[sy * image.width + sx];
                            ++histogram[RGBAKey{c.r, c.g, c.b, c.a}];
                        }
                    }

                    int bestCount = -1;
                    RGBAKey bestKey{};
                    for (const auto &[key, count] : histogram)
                    {
                        if (count > bestCount)
                        {
                            bestCount = count;
                            bestKey = key;
                        }
                    }
                    out = ColorRGBA{bestKey.r, bestKey.g, bestKey.b, bestKey.a};
                }
                else if (mode == PixelizeMode::EdgeAware)
                {
                    double accR = 0.0;
                    double accG = 0.0;
                    double accB = 0.0;
                    double accA = 0.0;
                    double weight = 0.0;
                    double edgeWeightSum = 0.0;

                    for (int sy = y0; sy < y1; ++sy)
                    {
                        for (int sx = x0; sx < x1; ++sx)
                        {
                            const int idx = sy * image.width + sx;
                            const Color c = pixels[idx];
                            const double a = static_cast<double>(c.a) / 255.0;
                            const double edgeW = 1.0 + std::max(0.0f, edgeWeights[idx]) * 3.0;
                            const double w = std::max(0.0, a) * edgeW;
                            if (w <= 0.0)
                            {
                                continue;
                            }

                            if (useLinearColor)
                            {
                                accR += SrgbToLinear(static_cast<float>(c.r)) * w;
                                accG += SrgbToLinear(static_cast<float>(c.g)) * w;
                                accB += SrgbToLinear(static_cast<float>(c.b)) * w;
                            }
                            else
                            {
                                accR += static_cast<double>(c.r) * w;
                                accG += static_cast<double>(c.g) * w;
                                accB += static_cast<double>(c.b) * w;
                            }
                            accA += static_cast<double>(c.a) * w;
                            weight += w;
                            edgeWeightSum += edgeW;
                        }
                    }

                    if (weight <= 0.0)
                    {
                        out = ColorRGBA{0, 0, 0, 0};
                    }
                    else if (useLinearColor)
                    {
                        out = MakeColorFromAccum(accR, accG, accB, accA, weight);
                    }
                    else
                    {
                        out = ColorRGBA{static_cast<unsigned char>(std::lround(accR / weight)),
                                        static_cast<unsigned char>(std::lround(accG / weight)),
                                        static_cast<unsigned char>(std::lround(accB / weight)),
                                        static_cast<unsigned char>(std::lround(std::clamp(accA / weight, 0.0, 255.0)))};
                    }
                }
                else
                {
                    double accR = 0.0;
                    double accG = 0.0;
                    double accB = 0.0;
                    double accA = 0.0;
                    double weight = 0.0;

                    for (int sy = y0; sy < y1; ++sy)
                    {
                        for (int sx = x0; sx < x1; ++sx)
                        {
                            const Color c = pixels[sy * image.width + sx];
                            const double a = static_cast<double>(c.a) / 255.0;
                            const double w = std::max(0.0, a);
                            if (w <= 0.0)
                            {
                                continue;
                            }

                            if (useLinearColor)
                            {
                                accR += SrgbToLinear(static_cast<float>(c.r)) * w;
                                accG += SrgbToLinear(static_cast<float>(c.g)) * w;
                                accB += SrgbToLinear(static_cast<float>(c.b)) * w;
                            }
                            else
                            {
                                accR += static_cast<double>(c.r) * w;
                                accG += static_cast<double>(c.g) * w;
                                accB += static_cast<double>(c.b) * w;
                            }
                            accA += static_cast<double>(c.a);
                            weight += w;
                        }
                    }

                    if (weight <= 0.0)
                    {
                        out = ColorRGBA{0, 0, 0, 0};
                    }
                    else if (useLinearColor)
                    {
                        out = MakeColorFromAccum(accR, accG, accB, accA, weight);
                    }
                    else
                    {
                        out = ColorRGBA{static_cast<unsigned char>(std::lround(accR / weight)),
                                        static_cast<unsigned char>(std::lround(accG / weight)),
                                        static_cast<unsigned char>(std::lround(accB / weight)),
                                        static_cast<unsigned char>(std::lround(std::clamp(accA / weight, 0.0, 255.0)))};
                    }
                }

                doc.pixels.push_back(PixelPaint{x, y, out});
            }
        }

        UnloadImageColors(pixels);
        UnloadImage(image);
        return true;
    }
}
