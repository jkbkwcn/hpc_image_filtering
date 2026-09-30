#include <iostream>
#include <stdlib.h>
#include <string>
#include <memory>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <omp.h>  // OpenMP

struct img_dim_t {
    int w;
    int h;
    int ch;
};

// -----------------------------------------------------------------------------
// PADDING
// -----------------------------------------------------------------------------
void imagePadding(img_dim_t img_dim, img_dim_t img_p_dim, int pad, unsigned char *img, unsigned char *img_p, unsigned char pad_value) {
    size_t total_p_size = (size_t)(img_p_dim.w * img_p_dim.h * img_p_dim.ch);
    memset(img_p, pad_value, total_p_size);

    unsigned char *src_row, *dst_row;

    for (int r = 0; r < img_dim.h; r++) {
        dst_row = img_p + ((r + pad) * img_p_dim.w + pad) * img_dim.ch;
        src_row = img + (r * img_dim.w * img_dim.ch);
        memcpy(dst_row, src_row, (size_t)img_dim.w * img_dim.ch);
    }
}

// -----------------------------------------------------------------------------
// EROZJA
// -----------------------------------------------------------------------------
void imageErosion(const unsigned char *img_in, unsigned char *img_out, int operation_size, img_dim_t img_in_dim, img_dim_t img_out_dim) {
    #pragma omp parallel for
    for (int y = 0; y < img_out_dim.h; y++) {
        for (int x = 0; x < img_out_dim.w; x++) {
            unsigned char temp_r = 255, temp_g = 255, temp_b = 255;
            const unsigned char *px_p;
            unsigned char *px_f;

            for (int ky = 0; ky < operation_size; ky++) {
                for (int kx = 0; kx < operation_size; kx++) {
                    px_p = img_in + ((y + ky) * img_in_dim.w + (x + kx)) * img_in_dim.ch;
                    if (px_p[0] < temp_r) temp_r = px_p[0];
                    if (px_p[1] < temp_g) temp_g = px_p[1];
                    if (px_p[2] < temp_b) temp_b = px_p[2];
                }
            }

            px_f = img_out + (y * img_out_dim.w + x) * img_out_dim.ch;
            px_f[0] = temp_r;
            px_f[1] = temp_g;
            px_f[2] = temp_b;
        }
    }
}

// -----------------------------------------------------------------------------
// DYLATACJA
// -----------------------------------------------------------------------------
void imageDilation(const unsigned char *img_in, unsigned char *img_out, int operation_size, img_dim_t img_in_dim, img_dim_t img_out_dim) {
    #pragma omp parallel for
    for (int y = 0; y < img_out_dim.h; y++) {
        for (int x = 0; x < img_out_dim.w; x++) {
            unsigned char temp_r = 0, temp_g = 0, temp_b = 0;
            const unsigned char *px_p;
            unsigned char *px_f;

            for (int ky = 0; ky < operation_size; ky++) {
                for (int kx = 0; kx < operation_size; kx++) {
                    px_p = img_in + ((y + ky) * img_in_dim.w + (x + kx)) * img_in_dim.ch;
                    if (px_p[0] > temp_r) temp_r = px_p[0];
                    if (px_p[1] > temp_g) temp_g = px_p[1];
                    if (px_p[2] > temp_b) temp_b = px_p[2];
                }
            }

            px_f = img_out + (y * img_out_dim.w + x) * img_out_dim.ch;
            px_f[0] = temp_r;
            px_f[1] = temp_g;
            px_f[2] = temp_b;
        }
    }
}

// -----------------------------------------------------------------------------
// TOP-HAT
// -----------------------------------------------------------------------------
void imageTopHat(const unsigned char *img_in, unsigned char *img_out, img_dim_t img_in_dim) {
    #pragma omp parallel for
    for (int y = 0; y < img_in_dim.h; y++) {
        for (int x = 0; x < img_in_dim.w; x++) {
            const unsigned char *px_p = img_in + (y * img_in_dim.w + x) * img_in_dim.ch;
            unsigned char *px_f = img_out + (y * img_in_dim.w + x) * img_in_dim.ch;

            int r = (int)px_p[0] - (int)px_f[0];
            int g = (int)px_p[1] - (int)px_f[1];
            int b = (int)px_p[2] - (int)px_f[2];

            if (r < 0) r = 0; if (r > 255) r = 255;
            if (g < 0) g = 0; if (g > 255) g = 255;
            if (b < 0) b = 0; if (b > 255) b = 255;

            px_f[0] = (unsigned char)r;
            px_f[1] = (unsigned char)g;
            px_f[2] = (unsigned char)b;
        }
    }
}

int main() {
    std::string input_path = "../img/image.jpg";
    std::string output_path = "../img/image_openmp.jpg";

    img_dim_t img_dim;
    unsigned char *img = stbi_load(input_path.c_str(), &img_dim.w, &img_dim.h, &img_dim.ch, 0);
    if (!img) { std::cerr << "Error loading image\n"; return 1; }

    printf("Loaded image: %dx%d, %d channels\n", img_dim.w, img_dim.h, img_dim.ch);

    int operation_size = 5;
    int img_pad = operation_size / 2;
    img_dim_t img_p_dim = { img_dim.w + 2*img_pad, img_dim.h + 2*img_pad, img_dim.ch };

    size_t img_size = (size_t)img_dim.w * img_dim.h * img_dim.ch;
    size_t img_p_size = (size_t)img_p_dim.w * img_p_dim.h * img_p_dim.ch;

    unsigned char *img_p = (unsigned char*)malloc(img_p_size);
    unsigned char *img_filtered = (unsigned char*)malloc(img_size);

    if(!img_p || !img_filtered) { std::cerr << "Memory allocation failed\n"; return 1; }

    auto startTime = std::chrono::system_clock::now();

    // EROZJA
    imagePadding(img_dim, img_p_dim, img_pad, img, img_p, 255);
    imageErosion(img_p, img_filtered, operation_size, img_p_dim, img_dim);

    // DYLATACJA
    imagePadding(img_dim, img_p_dim, img_pad, img_filtered, img_p, 0);
    imageDilation(img_p, img_filtered, operation_size, img_p_dim, img_dim);

    // TOP-HAT
    imageTopHat(img, img_filtered, img_dim);

    auto endTime = std::chrono::system_clock::now();
    printf("Time duration: %.3f ms\n", std::chrono::duration<double, std::milli>(endTime - startTime).count());

    int err = stbi_write_jpg(output_path.c_str(), img_dim.w, img_dim.h, img_dim.ch, img_filtered, 100);
    if(err) printf("Output image written successfully.\n");
    else printf("Error writing image.\n");

    free(img);
    free(img_p);
    free(img_filtered);

    return 0;
}
