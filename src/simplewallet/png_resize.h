#ifndef RESIZE_PNG_H
#define RESIZE_PNG_H

#include <vector>
#include <cstdint>

// Resizes a PNG image represented as a vector of uint8_t
// Parameters:
// - input_png: A vector containing the original PNG image data.
// - new_width: The desired width of the resized image.
// - new_height: The desired height of the resized image.
// Returns:
// - A vector containing the resized PNG image data.
std::vector<uint8_t> resize_png(const std::vector<uint8_t>& input_png, int new_width, int new_height);

std::vector<uint8_t> resize_and_optimize_png(const std::vector<uint8_t>& input_png, int max_size_bytes);

#endif // RESIZE_PNG_H
