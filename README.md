# PixelSort

High-performance command-line pixel sorting tool written in modern C++20.

PixelSort applies luminance-based glitch-art sorting to PNG and JPEG images by reordering contiguous pixel segments within configurable luminance thresholds.

Designed for:
- high runtime efficiency
- low memory overhead
- large-image processing
- multithreaded execution
- cache-friendly traversal

---

# Features

- PNG input/output
- JPEG input/output
- Horizontal and vertical pixel sorting
- Luminance-threshold interval sorting
- Multithreaded processing
- In-place image modification
- Modern C++20 architecture
- Minimal dependencies

---

# Example

Original image:

```bash
pixelsort.exe input.png output.png 40 180
```

Vertical sorting:

```bash
pixelsort.exe input.png output.png 20 240 --vertical
```

Full-image aggressive sorting:

```bash
pixelsort.exe input.png output.png 0 255
```

---

# Pixel Sorting Logic

Pixels are sorted using perceived luminance:

```text
L = 0.2126R + 0.7152G + 0.0722B
```

Only contiguous pixel segments whose luminance falls within the specified threshold range are sorted.

Pixels outside the range remain untouched.

This produces controlled glitch-art effects instead of globally destroying image structure.

---

# Build Requirements

- C++20 compiler
- pthread support
- stb_image.h
- stb_image_write.h

Tested with:
- GCC
- Clang

---

# Dependencies

Download:

- stb_image.h
- stb_image_write.h

From:

- https://github.com/nothings/stb

Place them in the same directory as:

```text
pixelsort.cpp
```

---

# Compilation

## GCC

```bash
g++ -std=c++20 -O3 -march=native -pthread pixelsort.cpp -o pixelsort
```

## Clang

```bash
clang++ -std=c++20 -O3 -march=native -pthread pixelsort.cpp -o pixelsort
```

---

# Usage

```bash
pixelsort <input> <output> <lower_threshold> <upper_threshold> [--vertical]
```

---

# Arguments

| Argument | Description |
|---|---|
| input | Input PNG/JPG image |
| output | Output PNG/JPG image |
| lower_threshold | Minimum luminance value (0-255) |
| upper_threshold | Maximum luminance value (0-255) |
| --vertical | Enables column-wise sorting |

---

# Examples

## Horizontal sorting

```bash
pixelsort image.png result.png 40 180
```

## Vertical sorting

```bash
pixelsort image.png result.png 40 180 --vertical
```

## Strong glitch effect

```bash
pixelsort image.png result.png 0 255
```

---

# Performance Notes

The implementation is optimized for:
- contiguous memory traversal
- low allocation frequency
- in-place processing
- parallel row/column execution

Each worker thread maintains its own scratch buffer to avoid synchronization overhead.

Horizontal sorting is significantly more cache-efficient than vertical sorting due to linear memory access.

---

# Complexity

For each sortable segment:

```text
O(k log k)
```

Where:
- `k` = segment length

Overall runtime depends on:
- image dimensions
- threshold range
- number of generated segments

---

# Recommended Images

Best visual results usually come from:
- high-contrast photography
- landscapes
- city lights
- portraits
- neon scenes
- colorful gradients

Very flat or already-uniform images may produce subtle results.

---

# License

Public domain / educational use.

stb libraries are public domain or MIT licensed depending on usage context.