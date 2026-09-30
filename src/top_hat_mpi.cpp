#include <iostream> 
#include <stdlib.h>
#include <string>
#include <mpi.h>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// struktura dla wymiarów obrazu
struct img_dim_t {
    int w;
    int h;
    int ch;
};

// -----------------------------------------------------------------------------
// Proste min/max i clamp (nie używamy C++17)
inline unsigned char my_min(unsigned char a, unsigned char b) { return (a < b) ? a : b; }
inline unsigned char my_max(unsigned char a, unsigned char b) { return (a > b) ? a : b; }
inline unsigned char my_clamp(int val) { if(val<0) return 0; if(val>255) return 255; return (unsigned char)val; }

// -----------------------------------------------------------------------------
// PADDING
void imagePadding(img_dim_t img_dim, img_dim_t img_p_dim, int pad,
                  unsigned char *img, unsigned char *img_p, unsigned char pad_value) {
    size_t total_p_size = (size_t)(img_p_dim.w * img_p_dim.h * img_p_dim.ch);
    memset(img_p, pad_value, total_p_size);
    for (int r = 0; r < img_dim.h; r++) {
        unsigned char* dst_row = img_p + ((r + pad) * img_p_dim.w + pad) * img_dim.ch;
        unsigned char* src_row = img + (r * img_dim.w * img_dim.ch);
        memcpy(dst_row, src_row, (size_t)img_dim.w * img_dim.ch);
    }
}

// -----------------------------------------------------------------------------
// EROZJA
void imageErosion(const unsigned char *img_in, unsigned char *img_out,
                  int operation_size, img_dim_t img_in_dim, img_dim_t img_out_dim,
                  int start_row, int end_row) {

    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < img_out_dim.w; x++) {
            unsigned char temp_r = 255, temp_g = 255, temp_b = 255;
            const unsigned char *px_p;
            unsigned char *px_f;

            for (int ky = 0; ky < operation_size; ky++) {
                for (int kx = 0; kx < operation_size; kx++) {
                    px_p = img_in + ((y + ky) * img_in_dim.w + (x + kx)) * img_in_dim.ch;
                    temp_r = my_min(temp_r, px_p[0]);
                    temp_g = my_min(temp_g, px_p[1]);
                    temp_b = my_min(temp_b, px_p[2]);
                }
            }

            px_f = img_out + ((y - start_row) * img_out_dim.w + x) * img_out_dim.ch;
            px_f[0] = temp_r;
            px_f[1] = temp_g;
            px_f[2] = temp_b;
        }
    }
}

// -----------------------------------------------------------------------------
// DYLATACJA
void imageDilation(const unsigned char *img_in, unsigned char *img_out,
                  int operation_size, img_dim_t img_in_dim, img_dim_t img_out_dim,
                  int start_row, int end_row) {

    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < img_out_dim.w; x++) {
            unsigned char temp_r = 0, temp_g = 0, temp_b = 0;
            const unsigned char *px_p;
            unsigned char *px_f;

            for (int ky = 0; ky < operation_size; ky++) {
                for (int kx = 0; kx < operation_size; kx++) {
                    px_p = img_in + ((y + ky) * img_in_dim.w + (x + kx)) * img_in_dim.ch;
                    temp_r = my_max(temp_r, px_p[0]);
                    temp_g = my_max(temp_g, px_p[1]);
                    temp_b = my_max(temp_b, px_p[2]);
                }
            }

            px_f = img_out + ((y - start_row) * img_out_dim.w + x) * img_out_dim.ch;
            px_f[0] = temp_r;
            px_f[1] = temp_g;
            px_f[2] = temp_b;
        }
    }
}

// -----------------------------------------------------------------------------
// TOP-HAT
void imageTopHat(const unsigned char *img_in, unsigned char *img_filtered, img_dim_t img_dim) {
    for(int y=0; y<img_dim.h; y++){
        for(int x=0; x<img_dim.w; x++){
            const unsigned char *px_in = img_in + (y*img_dim.w + x)*img_dim.ch;
            unsigned char *px_f = img_filtered + (y*img_dim.w + x)*img_dim.ch;

            int r = px_in[0] - px_f[0];
            int g = px_in[1] - px_f[1];
            int b = px_in[2] - px_f[2];

            px_f[0] = my_clamp(r);
            px_f[1] = my_clamp(g);
            px_f[2] = my_clamp(b);
        }
    }
}

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------
int main(int argc, char* argv[]) {

    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::string input_path = "../img/image.jpg";
    std::string output_path = "../img/image_tophat_MPI.jpg";

    img_dim_t img_dim;
    unsigned char *img = nullptr;

    if(rank==0){
        img = stbi_load(input_path.c_str(), &img_dim.w, &img_dim.h, &img_dim.ch, 0);
        if(!img){ std::cerr << "Error loading image\n"; MPI_Abort(MPI_COMM_WORLD,1);}
        printf("Loaded image: %dx%d, %d channels\n", img_dim.w, img_dim.h, img_dim.ch);
    }

    // Rozsyłamy wymiary obrazu do wszystkich procesów
    MPI_Bcast(&img_dim, 3, MPI_INT, 0, MPI_COMM_WORLD); // 3 inty: w, h, ch

    int operation_size = 5;
    int pad = operation_size/2;

    int rows_per_proc = img_dim.h / size;
    int extra_rows = img_dim.h % size;

    int start_row = rank * rows_per_proc + (rank < extra_rows ? rank : extra_rows);
    int local_rows = rows_per_proc + (rank < extra_rows ? 1 : 0);
    int end_row = start_row + local_rows;

    // Każdy proces przydziela pamięć na swój fragment obrazu
    if(rank != 0){
        img = (unsigned char*)malloc(local_rows * img_dim.w * img_dim.ch);
        if(!img) { std::cerr << "Memory allocation failed for img\n"; MPI_Abort(MPI_COMM_WORLD,1);}
    }

    // Rozsyłanie fragmentów obrazu do procesów
    if(rank==0){
        for(int p=1;p<size;p++){
            int start = p*rows_per_proc + (p < extra_rows ? p : extra_rows);
            int rows = rows_per_proc + (p < extra_rows ? 1 : 0);
            MPI_Send(img + start*img_dim.w*img_dim.ch, rows*img_dim.w*img_dim.ch,
                     MPI_UNSIGNED_CHAR, p, 0, MPI_COMM_WORLD);
        }
    } else {
        MPI_Recv(img, local_rows*img_dim.w*img_dim.ch, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // Każdy proces przydziela pamięć na padding i wynik filtrowania
    img_dim_t img_p_dim = {img_dim.w + 2*pad, local_rows + 2*pad, img_dim.ch};
    size_t img_p_size = (size_t)img_p_dim.w * img_p_dim.h * img_p_dim.ch;
    unsigned char *img_p = (unsigned char*)malloc(img_p_size);

    size_t local_size = (size_t)local_rows * img_dim.w * img_dim.ch;
    unsigned char *img_filtered = (unsigned char*)malloc(local_size);

    if(!img_p || !img_filtered) { std::cerr << "Memory allocation failed\n"; MPI_Abort(MPI_COMM_WORLD,1); }

    // EROZJA
    imagePadding({img_dim.w, local_rows, img_dim.ch}, img_p_dim, pad, img, img_p, 255);
    imageErosion(img_p, img_filtered, operation_size, img_p_dim, {img_dim.w, local_rows, img_dim.ch}, pad, pad+local_rows);

    // DYLATACJA
    imagePadding({img_dim.w, local_rows, img_dim.ch}, img_p_dim, pad, img_filtered, img_p, 0);
    imageDilation(img_p, img_filtered, operation_size, img_p_dim, {img_dim.w, local_rows, img_dim.ch}, pad, pad+local_rows);

    // Zbieranie wyników w procesie 0
    if(rank==0){
        unsigned char *img_final = (unsigned char*)malloc(img_dim.w*img_dim.h*img_dim.ch);
        memcpy(img_final, img_filtered, local_size);
        for(int p=1;p<size;p++){
            int start = p*rows_per_proc + (p < extra_rows ? p : extra_rows);
            int rows = rows_per_proc + (p < extra_rows ? 1 : 0);
            MPI_Recv(img_final + start*img_dim.w*img_dim.ch, rows*img_dim.w*img_dim.ch, MPI_UNSIGNED_CHAR, p, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        imageTopHat(img, img_final, img_dim);

        int err = stbi_write_jpg(output_path.c_str(), img_dim.w, img_dim.h, img_dim.ch, img_final, 100);
        if(err) printf("Output image written successfully.\n");
        free(img_final);
    } else {
        MPI_Send(img_filtered, local_size, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD);
    }

    free(img);
    free(img_p);
    free(img_filtered);

    MPI_Finalize();
    return 0;
}
