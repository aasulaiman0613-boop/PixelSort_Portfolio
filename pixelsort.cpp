#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

enum class Direction : uint8_t { horizontal, vertical };

struct Args {
    const char* in = nullptr;
    const char* out = nullptr;
    uint8_t lo = 0;
    uint8_t hi = 255;
    Direction dir = Direction::horizontal;
};

struct ImageDeleter {
    void operator()(uint8_t* p) const noexcept { stbi_image_free(p); }
};

class PixelSorter {
public:
    PixelSorter(uint8_t* data, int w, int h, int c, uint8_t lo, uint8_t hi) noexcept
        : img_data(data), img_size(static_cast<size_t>(w) * h * c), width(w), height(h), channels(c), lower(lo), upper(hi) {}

    void run(Direction d) {
        d == Direction::horizontal ? rows() : cols();
    }

private:
    uint8_t* img_data;
    size_t img_size;
    int width{}, height{}, channels{};
    uint8_t lower{}, upper{};

    static constexpr float rW = 0.2126f;
    static constexpr float gW = 0.7152f;
    static constexpr float bW = 0.0722f;

    float lum(const uint8_t* p) const noexcept {
        return rW * p[0] + gW * p[1] + bW * p[2];
    }

    bool inside(const uint8_t* p) const noexcept {
        const auto v = lum(p);
        return v >= lower && v <= upper;
    }

    uint8_t* px(int x, int y) noexcept {
        return img_data + (static_cast<size_t>(y) * width + x) * channels;
    }

    void sort_line(uint8_t* base, int count, int stride, std::vector<uint32_t>& tmp) {
        int i = 0;

        while (i < count) {
            while (i < count && !inside(base + static_cast<size_t>(i) * stride)) ++i;

            const int start = i;

            while (i < count && inside(base + static_cast<size_t>(i) * stride)) ++i;

            const int n = i - start;
            if (n < 2) continue;

            tmp.resize(static_cast<size_t>(n));

            for (int k = 0; k < n; ++k) {
                auto* p = base + static_cast<size_t>(start + k) * stride;
                uint32_t v = p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
                if (channels == 4) v |= uint32_t(p[3]) << 24;
                tmp[static_cast<size_t>(k)] = v;
            }

            std::sort(tmp.begin(), tmp.end(), [&](uint32_t a, uint32_t b) noexcept {
                const float la =
                    rW * float(a & 255u) +
                    gW * float((a >> 8) & 255u) +
                    bW * float((a >> 16) & 255u);

                const float lb =
                    rW * float(b & 255u) +
                    gW * float((b >> 8) & 255u) +
                    bW * float((b >> 16) & 255u);

                return la < lb;
            });

            for (int k = 0; k < n; ++k) {
                auto v = tmp[static_cast<size_t>(k)];
                auto* p = base + static_cast<size_t>(start + k) * stride;
                p[0] = uint8_t(v);
                p[1] = uint8_t(v >> 8);
                p[2] = uint8_t(v >> 16);
                if (channels == 4) p[3] = uint8_t(v >> 24);
            }
        }
    }

    template <class Fn>
    void parallel_for(int total, Fn&& fn) {
        const auto hw = std::max(1u, std::thread::hardware_concurrency());
        const int workers = std::min<int>(static_cast<int>(hw), total);
        std::atomic<int> cursor{0};
        std::vector<std::jthread> pool;
        pool.reserve(static_cast<size_t>(workers));

        for (int t = 0; t < workers; ++t) {
            pool.emplace_back([&] {
                std::vector<uint32_t> scratch;
                scratch.reserve(static_cast<size_t>(std::max(width, height)));

                for (;;) {
                    const int idx = cursor.fetch_add(1, std::memory_order_relaxed);
                    if (idx >= total) break;
                    fn(idx, scratch);
                }
            });
        }
    }

    void rows() {
        parallel_for(height, [&](int y, std::vector<uint32_t>& scratch) {
            sort_line(px(0, y), width, channels, scratch);
        });
    }

    void cols() {
        parallel_for(width, [&](int x, std::vector<uint32_t>& scratch) {
            sort_line(px(x, 0), height, width * channels, scratch);
        });
    }
};

static bool parse_u8(std::string_view s, uint8_t& out) {
    int v{};
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || p != s.data() + s.size() || v < 0 || v > 255) return false;
    out = static_cast<uint8_t>(v);
    return true;
}

static Args parse(int argc, char** argv) {
    if (argc < 5) throw std::runtime_error("usage: pixelsort <input> <output> <low> <high> [--vertical]");

    Args a;
    a.in = argv[1];
    a.out = argv[2];

    if (!parse_u8(argv[3], a.lo) || !parse_u8(argv[4], a.hi) || a.lo > a.hi)
        throw std::runtime_error("thresholds must be integers in range 0..255 and low <= high");

    for (int i = 5; i < argc; ++i) {
        std::string_view s(argv[i]);
        if (s == "--vertical" || s == "-v") a.dir = Direction::vertical;
        else if (s == "--horizontal" || s == "-h") a.dir = Direction::horizontal;
        else throw std::runtime_error("unknown argument: " + std::string(s));
    }

    return a;
}

static bool has_ext(std::string_view path, std::string_view ext) {
    if (path.size() < ext.size()) return false;
    auto tail = path.substr(path.size() - ext.size());
    return std::equal(tail.begin(), tail.end(), ext.begin(), ext.end(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
}

int main(int argc, char** argv) {
    try {
        const auto args = parse(argc, argv);

        int w{}, h{}, c{};
        std::unique_ptr<uint8_t, ImageDeleter> data(stbi_load(args.in, &w, &h, &c, 4));
c = 4;

        if (!data || w <= 0 || h <= 0)
    throw std::runtime_error(std::string("failed to load image: ") + stbi_failure_reason());

        PixelSorter sorter(data.get(), w, h, c, args.lo, args.hi);
        sorter.run(args.dir);

        const int stride = w * c;
        int ok = 0;

        std::string_view out(args.out);
        if (has_ext(out, ".png")) {
            ok = stbi_write_png(args.out, w, h, c, data.get(), stride);
        } else if (has_ext(out, ".jpg") || has_ext(out, ".jpeg")) {
            ok = stbi_write_jpg(args.out, w, h, c, data.get(), 95);
        } else {
            throw std::runtime_error("output must end with .png, .jpg, or .jpeg");
        }

        if (!ok) throw std::runtime_error("failed to write output image");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "pixelsort: " << e.what() << '\n';
        return 1;
    }
}