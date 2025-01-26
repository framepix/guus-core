#include <iostream>
#include <vector>
#include <random>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <cairo/cairo.h>
#include "png_resize.h"
#include <png.h>

std::vector<uint8_t> resize_png(const std::vector<uint8_t>& input_png, int new_width, int new_height) {
    cairo_surface_t* image = cairo_image_surface_create_from_png_stream(
        [](void* closure, unsigned char* data, unsigned int length) -> cairo_status_t {
            auto* png_stream = reinterpret_cast<std::istringstream*>(closure);
            png_stream->read(reinterpret_cast<char*>(data), length);
            return png_stream->gcount() == length ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_READ_ERROR;
        },
        new std::istringstream(std::string(input_png.begin(), input_png.end()))
    );

    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(image);
        return {};
    }

    int original_width = cairo_image_surface_get_width(image);
    int original_height = cairo_image_surface_get_height(image);

    cairo_surface_t* resized_image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, new_width, new_height);
    cairo_t* cr = cairo_create(resized_image);
    cairo_scale(cr, static_cast<double>(new_width) / original_width, static_cast<double>(new_height) / original_height);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_paint(cr);

    // Save resized PNG
    std::vector<uint8_t> resized_png;
    cairo_surface_write_to_png_stream(
        resized_image,
        [](void* closure, const unsigned char* data, unsigned int length) -> cairo_status_t {
            auto* out_vec = reinterpret_cast<std::vector<uint8_t>*>(closure);
            out_vec->insert(out_vec->end(), data, data + length);
            return CAIRO_STATUS_SUCCESS;
        },
        &resized_png
    );

    // Clean up
    cairo_destroy(cr);
    cairo_surface_destroy(image);
    cairo_surface_destroy(resized_image);

    return resized_png;
}

std::vector<uint8_t> resize_and_optimize_png(const std::vector<uint8_t>& input_png, int max_size_bytes) {
    // Load PNG from memory
    cairo_surface_t* png_surface = nullptr;
    std::string temp_file = "/tmp/nft_temp.png";
    std::ofstream out(temp_file, std::ios::binary);
    out.write(reinterpret_cast<const char*>(input_png.data()), input_png.size());
    out.close();

    png_surface = cairo_image_surface_create_from_png(temp_file.c_str());
    if (!png_surface || cairo_surface_status(png_surface) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error("Failed to load PNG image");
    }

    int width = cairo_image_surface_get_width(png_surface);
    int height = cairo_image_surface_get_height(png_surface);
    double scale = 1.0;
    
    std::vector<uint8_t> output_png;
    cairo_surface_t* resized_surface = nullptr;
    cairo_t* cr = nullptr;

    // Iteratively reduce size until it fits within the max_size_bytes limit
    while (true) {
        resized_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width * scale, height * scale);
        cr = cairo_create(resized_surface);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, png_surface, 0, 0);
        cairo_paint(cr);

        // Write to memory
        std::vector<uint8_t> temp_output;
        cairo_status_t status = cairo_surface_write_to_png_stream(resized_surface,
            [](void* closure, const unsigned char* data, unsigned int length) -> cairo_status_t {
                std::vector<uint8_t>* buffer = static_cast<std::vector<uint8_t>*>(closure);
                buffer->insert(buffer->end(), data, data + length);
                return CAIRO_STATUS_SUCCESS;
            },
            &temp_output);

        if (status != CAIRO_STATUS_SUCCESS) {
            throw std::runtime_error("Failed to write PNG to memory");
        }

        if (temp_output.size() <= max_size_bytes) {
            output_png = std::move(temp_output);
            break;
        }

        scale *= 0.9; // Reduce by 10% and retry
        cairo_destroy(cr);
        cairo_surface_destroy(resized_surface);
    }

    // Cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(png_surface);
    cairo_surface_destroy(resized_surface);

    return output_png;
}
