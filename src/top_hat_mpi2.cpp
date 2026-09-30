#include <mpi.h>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include <string>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

struct img_dim_t {
    int w;
    int h;
    int ch;
};

// ------------------- IMAGE FUNCTIONS -------------------
void imagePadding(const unsigned char *img, unsigned char *img_p, img_dim_t img_dim, img_dim_t img_p_dim, int pad, uint8_t pad_value) {
    size_t total_p_size = (size_t)img_p_dim.w * img_p_dim.h * img_p_dim.ch;
    memset(img_p, pad_value, total_p_size);

    for (int row = 0; row < img_dim.h; row++) {
        unsigned char *dst_row = img_p + ((row + pad) * img_p_dim.w + pad) * img_dim.ch;
        const unsigned char *src_row = img + row * img_dim.w * img_dim.ch;
        memcpy(dst_row, src_row, (size_t)img_dim.w * img_dim.ch);
    }
}

void imageErosion(const unsigned char *img_src, unsigned char *img_dst, img_dim_t img_src_dim, int operation_size) {
    int pad = operation_size / 2;
    
    const unsigned char* px_src;
    unsigned char* px_dst;

    int content_w = img_src_dim.w - 2 * pad;
    int content_h = img_src_dim.h - 2 * pad;
    
    for (int y = 0; y < content_h; y++) {
        for (int x = 0; x < content_w; x++) {
            uint8_t temp_r = 255, temp_g = 255, temp_b = 255;
            for (int ky = 0; ky < operation_size; ky++) {
                for (int kx = 0; kx < operation_size; kx++) {
                    px_src = img_src + ((y + ky) * img_src_dim.w + (x + kx)) * img_src_dim.ch;
                    temp_r = std::min(temp_r, px_src[0]);
                    temp_g = std::min(temp_g, px_src[1]);
                    temp_b = std::min(temp_b, px_src[2]);
                }
            }
            unsigned char *px_dst = img_dst + ((y + pad) * img_src_dim.w + (x + pad)) * img_src_dim.ch;
            px_dst[0] = temp_r;
            px_dst[1] = temp_g;
            px_dst[2] = temp_b;
        }
    }
}

void imageDilation(const unsigned char *img_src, unsigned char *img_dst, img_dim_t img_src_dim, int operation_size) {
    int pad = operation_size / 2;
    
    const unsigned char* px_src;
    unsigned char* px_dst;

    int content_w = img_src_dim.w - 2 * pad;
    int content_h = img_src_dim.h - 2 * pad;
    
    for (int y = 0; y < content_h; y++) {
        for (int x = 0; x < content_w; x++) {
            uint8_t temp_r = 0, temp_g = 0, temp_b = 0;
            for (int ky = 0; ky < operation_size; ky++) {
                for (int kx = 0; kx < operation_size; kx++) {
                    px_src = img_src + ((y + ky) * img_src_dim.w + (x + kx)) * img_src_dim.ch;
                    temp_r = std::max(temp_r, px_src[0]);
                    temp_g = std::max(temp_g, px_src[1]);
                    temp_b = std::max(temp_b, px_src[2]);
                }
            }
            unsigned char *px_dst = img_dst + ((y + pad) * img_src_dim.w + (x + pad)) * img_src_dim.ch;
            px_dst[0] = temp_r;
            px_dst[1] = temp_g;
            px_dst[2] = temp_b;
        }
    }
}

void imageTopHat(const unsigned char *img_src, const unsigned char *img_eros_dil, unsigned char *img_dst, img_dim_t img_src_dim, img_dim_t img_dst_dim, int operation_size) {
    int pad = operation_size / 2;
    
    const unsigned char *px_src, *px_eros_dil;
    unsigned char* px_dst;

    int r, g, b;

    for (int y = 0; y < img_dst_dim.h; y++) {
        for (int x = 0; x < img_dst_dim.w; x++) {
            px_src = img_src + ((y + pad) * img_src_dim.w + (x + pad)) * img_src_dim.ch;
            px_eros_dil = img_eros_dil + ((y + pad) * img_src_dim.w + (x + pad)) * img_src_dim.ch;
            px_dst = img_dst + (y * img_dst_dim.w + x) * img_dst_dim.ch;

            r = (int)px_src[0] - (int)px_eros_dil[0];
            g = (int)px_src[1] - (int)px_eros_dil[1];
            b = (int)px_src[2] - (int)px_eros_dil[2];

            px_dst[0] = (unsigned char)std::clamp(r, 0, 255);
            px_dst[1] = (unsigned char)std::clamp(g, 0, 255);
            px_dst[2] = (unsigned char)std::clamp(b, 0, 255);
        }
    }
}

// ------------------- MAIN MPI -------------------
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int mpi_rank, mpi_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    img_dim_t img_dim;
    unsigned char *img = nullptr;
    const std::string input_path = "../img/image.jpg";
    const std::string output_path = "../img/image_mpi.jpg";
    int operation_size = 5;
    int pad = operation_size / 2;

    if (0 == mpi_rank) {
        img = stbi_load(input_path.c_str(), &img_dim.w, &img_dim.h, &img_dim.ch, 0);
        if (!img) {
            std::cerr << "Error loading image\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        std::cout << "Loaded image " << img_dim.w << "x" << img_dim.h << "x" << img_dim.ch << "\n";
    }

    MPI_Bcast(&img_dim, sizeof(img_dim), MPI_BYTE, 0, MPI_COMM_WORLD);

    // ############################################################

    int base_rows = img_dim.h / mpi_size;
    int remainder = img_dim.h % mpi_size;
    // Local image should be: local rows + padding row + reminder for the last rank
    int local_rows = base_rows + 2 * pad + (mpi_rank < remainder ? 1 : 0);

    // Reserve memory for padded images: one for erosion padding and one for dilation padding
    // Then scatter it and send its fragments to nodes
    img_dim_t img_dim_padded, img_dim_local_padded, img_dim_local;

    img_dim_padded.w = img_dim.w + 2 * pad;
    img_dim_padded.h = img_dim.h + 2 * pad;
    img_dim_padded.ch = img_dim.ch;

    img_dim_local_padded.w = img_dim.w + 2 * pad;
    img_dim_local_padded.h = local_rows;
    img_dim_local_padded.ch = img_dim.ch;

    img_dim_local.w = img_dim.w;
    img_dim_local.h = local_rows - 2 * pad;
    img_dim_local.ch = img_dim.ch;

    int img_padded_size = img_dim_padded.w * img_dim_padded.h * img_dim_padded.ch;

    unsigned char *img_local_padded = new unsigned char[img_dim_padded.w * local_rows * img_dim_padded.ch];
    unsigned char *img_local_padded_eroded = new unsigned char[img_dim_padded.w * local_rows * img_dim_padded.ch];
    unsigned char *img_local_padded_dilated = new unsigned char[img_dim_padded.w * local_rows * img_dim_padded.ch];
    unsigned char *img_local_tophat = new unsigned char[img_dim_padded.w * local_rows * img_dim_padded.ch];

    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;

    if(0 == mpi_rank) {
        // Save start time
	    startTime = std::chrono::system_clock::now();
    }

    // ------------------- Scattering image -------------------
    if(0 == mpi_rank) {
        // Memory alocation for padded images
        unsigned char *img_padded = new unsigned char[img_padded_size];
        
        // Pad image with the value of 255 (min nautral element) before erosion
        imagePadding(img, img_padded, img_dim, img_dim_padded, pad, 255);

        // ------------------- Scatter original image fragments -------------------
        unsigned char *img_to_send;
        int rows_to_send;
        int rows_to_send_offset = 0;

        for (int rank = 0; rank < mpi_size; rank++) {
            rows_to_send = base_rows + (rank < remainder ? 1 : 0) + 2 * pad;
            // img_to_send = img_padded + rank * (rows_to_send - pad) * img_dim_padded.w * img_dim_padded.ch;
            img_to_send = img_padded + rows_to_send_offset * img_dim_padded.w * img_dim_padded.ch;
            if (0 == rank) {
                // Rank 0 should copy data to its local image - padded image
                memcpy(img_local_padded, img_to_send, rows_to_send * img_dim_padded.w * img_dim_padded.ch);
            } else {
                // Sending local images
                MPI_Send(img_to_send, rows_to_send * img_dim_padded.w * img_dim_padded.ch, MPI_UNSIGNED_CHAR, rank, 2, MPI_COMM_WORLD);
            }
            rows_to_send_offset += rows_to_send - 2 * pad;
        }
        delete[] img_padded;
    } else {
        // Receiving local image - padded image
        MPI_Recv(img_local_padded, local_rows * img_dim_padded.w * img_dim_padded.ch, MPI_UNSIGNED_CHAR, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // Padding eroded image
    memset(img_local_padded_eroded, 0, local_rows * img_dim_padded.w * img_dim_padded.ch);

    // ------------------- Erosion -> Dilation -> Top-hat -------------------
    imageErosion(img_local_padded, img_local_padded_eroded, img_dim_local_padded, operation_size);
    imageDilation(img_local_padded_eroded, img_local_padded_dilated, img_dim_local_padded, operation_size);
    imageTopHat(img_local_padded, img_local_padded_dilated, img_local_tophat, img_dim_local_padded, img_dim_local, operation_size);

    // ------------------- Gather results -------------------
    unsigned char *img_result = nullptr;
    if (0 == mpi_rank) {
        img_result = new unsigned char[img_dim.w * img_dim.h * img_dim.ch];
        memcpy(img_result, img_local_tophat, img_dim_local.w * img_dim_local.h * img_dim_local.ch);
        
        int rows_to_gather_offset = img_dim_local.h;

        for (int rank = 1; rank < mpi_size; rank++) {
            int rows_to_gather = base_rows + (rank < remainder ? 1 : 0);

            MPI_Recv(img_result + rows_to_gather_offset * img_dim.w * img_dim.ch, rows_to_gather * img_dim.w * img_dim.ch, MPI_UNSIGNED_CHAR, rank, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            rows_to_gather_offset += rows_to_gather;
        }
    } else {
        MPI_Send(img_local_tophat, img_dim_local.w * img_dim_local.h * img_dim_local.ch, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD);
    }

    if(0 == mpi_rank) {
        // Save end time    
        endTime = std::chrono::system_clock::now();

        // Print results
        auto duration = endTime - startTime;

        printf("----------------------------\n");
        printf("Time duration:  %.3f [ms]\n", std::chrono::duration <double, std::milli>(duration).count());

        stbi_write_jpg(output_path.c_str(), img_dim.w, img_dim.h, img_dim.ch, img_result, 100);
        std::cout << "Result saved to " << output_path << "\n";
    }

    // ------------------- Free memory -------------------
    delete[] img_local_padded;
    delete[] img_local_padded_eroded;
    delete[] img_local_padded_dilated;
    delete[] img_local_tophat;
    delete[] img_result;

    if (mpi_rank == 0 && img) stbi_image_free(img);

    MPI_Finalize();
    return 0;
}