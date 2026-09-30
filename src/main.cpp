#include <iostream>
#include <stdlib.h>
#include <numeric>
#include <algorithm>
#include <vector>
#include <string>
#include <exception>
#include <memory>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_THREADS_COUNT 16

using namespace std::chrono_literals;

struct img_dim_t {
	int w;		// Width
	int h;		// Height
	int ch;		// Channels
};

void imagePadding(img_dim_t img_dim, img_dim_t img_p_dim, int pad, unsigned char *img, unsigned char *img_p, uint8_t pad_value) {
    
    // Padding the whole image with given value
    size_t total_p_size = (size_t)(img_p_dim.w * img_p_dim.h * img_p_dim.ch);
    memset(img_p, pad_value, total_p_size);

    // Copy image inside the padded version
    // Iterating over rows of the original image
    for (int r = 0; r < img_dim.h; r++) {
        // Obliczamy wskaźnik do wiersza w obrazie z paddingiem (pomijamy 'pad' wierszy i 'pad' pikseli w lewo)
        unsigned char* dst_row = img_p + ((r + pad) * img_p_dim.w + pad) * img_dim.ch;
        
        // Obliczamy wskaźnik do wiersza w obrazie źródłowym
        unsigned char* src_row = img + (r * img_dim.w * img_dim.ch);
        
        // Kopiujemy cały wiersz za jednym razem
        memcpy(dst_row, src_row, (size_t)img_dim.w * img_dim.ch);
    }
}

void imageErosion(const unsigned char *img_in, unsigned char *img_out, const int operation_size, img_dim_t img_in_dim, img_dim_t img_out_dim, int start_row, int end_row) {

	// Sums (convolution), temporary kernel data values
	uint8_t temp_r, temp_g, temp_b;

	// Pointers; ensure that input image i read-only
	const unsigned char *px_p; 
	unsigned char *px_f;
	
	// Iteration over rows and columns of the resultant image (indexing over output image)
	for (int y = start_row; y < end_row; y++) {
		for (int x = 0; x < img_out_dim.w; x++) {
			
			// Termporary sums - convoltion results
			temp_r = 255;
			temp_g = 255;
			temp_b = 255;

			// Iteration over image pixels with the kernel
			for (int ky = 0; ky < operation_size; ky++) {
				for (int kx = 0; kx < operation_size; kx++) {

					// Value of pixel (indexing over padded image)
					px_p = img_in + ((y + ky) * img_in_dim.w + (x + kx)) * img_in_dim.ch;

					// Convolution - temporary sums
					temp_r = std::min(temp_r, px_p[0]);
					temp_g = std::min(temp_g, px_p[1]);
					temp_b = std::min(temp_b, px_p[2]);
				}
			}

			// Writing results of convolution, ensure proper clamping
			px_f = img_out + (y * img_out_dim.w + x) * img_out_dim.ch;
			px_f[0] = temp_r;
			px_f[1] = temp_g;
			px_f[2] = temp_b;
		}
	}
}

void imageDilation(const unsigned char *img_in, unsigned char *img_out, const int operation_size, img_dim_t img_in_dim, img_dim_t img_out_dim, int start_row, int end_row) {

	// Sums (convolution), temporary kernel data values
	uint8_t temp_r, temp_g, temp_b;

	// Pointers; ensure that input image i read-only
	const unsigned char *px_p; 
	unsigned char *px_f;
	
	// Iteration over rows and columns of the resultant image (indexing over output image)
	for (int y = start_row; y < end_row; y++) {
		for (int x = 0; x < img_out_dim.w; x++) {
			
			// Termporary sums - convoltion results
			temp_r = 0;
			temp_g = 0;
			temp_b = 0;

			// Iteration over image pixels with the kernel
			for (int ky = 0; ky < operation_size; ky++) {
				for (int kx = 0; kx < operation_size; kx++) {

					// Value of pixel (indexing over padded image)
					px_p = img_in + ((y + ky) * img_in_dim.w + (x + kx)) * img_in_dim.ch;

					// Convolution - temporary sums
					temp_r = std::max(temp_r, px_p[0]);
					temp_g = std::max(temp_g, px_p[1]);
					temp_b = std::max(temp_b, px_p[2]);
				}
			}

			// Writing results of convolution, ensure proper clamping
			px_f = img_out + (y * img_out_dim.w + x) * img_out_dim.ch;
			px_f[0] = temp_r;
			px_f[1] = temp_g;
			px_f[2] = temp_b;
		}
	}
}

void imageTopHat(const unsigned char *img_in, unsigned char *img_out, img_dim_t img_in_dim, int start_row, int end_row) {
	// Pointers; ensure that input image i read-only
	const unsigned char *px_p; 
	unsigned char *px_f;
	
	// Iteration over rows and columns of the resultant image (indexing over output image)
	for (int y = start_row; y < end_row; y++) {
		for (int x = 0; x < img_in_dim.w; x++) {
			// Writing results of convolution, ensure proper clamping
			px_p = img_in + (y * img_in_dim.w + x) * img_in_dim.ch;
			px_f = img_out + (y * img_in_dim.w + x) * img_in_dim.ch;
			px_f[0] = (uint8_t)std::clamp((int)px_p[0] - (int)px_f[0], 0, 255);
			px_f[1] = (uint8_t)std::clamp((int)px_p[1] - (int)px_f[1], 0, 255);
			px_f[2] = (uint8_t)std::clamp((int)px_p[2] - (int)px_f[2], 0, 255);
		}
	}
}


int main() {

	// ----------------------------------------------------------------------------
	// ------------------------------- LOADING IMAGE ------------------------------
	// ----------------------------------------------------------------------------
	
	std::string input_path = "../img/image.jpg";
	std::string output_path;
	
	// Imgage dimensions
	img_dim_t img_dim;

	/*
	The image is loaded in the structure with the following order,
	marked as <COLOR>_<ROW><COLUMN>:
	R_00 - G_00 - B_00 - R_01 - G_01 - B_01 - ...
	*/
 	unsigned char *img = stbi_load(input_path.c_str(), &img_dim.w, &img_dim.h, &img_dim.ch, 0);
 	
	if(img == NULL) {
 		printf("Error in loading the image\n");
 		exit(1);
 	}
	
	printf("Loaded image with a width of %dpx, a height of %dpx and %d channels\n", img_dim.w, img_dim.h, img_dim.ch);

	// Kernel declaration - filtering an image - as 1D table of type signed int (kernel values can be negative)
	int operation_size = 5;

	output_path = "../img/image_sequential.jpg";


	// ----------------------------------------------------------------------------
	// --------------------------------- PADDING ----------------------------------
	// ----------------------------------------------------------------------------

	printf("Padding image...\n");

	// Padding is needed to keep original image shape
	// Padded image dimensions
	img_dim_t img_p_dim;

    int img_pad = operation_size / 2;
    img_p_dim.w = img_dim.w + 2 * img_pad;
    img_p_dim.h = img_dim.h + 2 * img_pad;
	img_p_dim.ch = img_dim.ch;

	// Allocate memory for padded image
    size_t img_p_size = img_p_dim.w * img_p_dim.h * img_p_dim.ch;
    unsigned char *img_p = (unsigned char*)malloc(img_p_size);

	// Allocate memory for fitered image
	size_t img_size = img_dim.w * img_dim.h * img_dim.ch;
	unsigned char *img_filtered = (unsigned char*)malloc(img_size);

	// ----------------------------------------------------------------------------
	// -------------------------------- FILTRATION --------------------------------
	// --------------------- EXECUTION TIME VS THREADS NUMBER ---------------------
	// ----------------------------------------------------------------------------

	printf("Filtering image...\n");

	// Save start time
	auto startTime = std::chrono::system_clock::now();

	// Image padding
	imagePadding(img_dim, img_p_dim, img_pad, img, img_p, 255);
	imageErosion(img_p, img_filtered, operation_size, img_p_dim, img_dim, 0, img_dim.h);

	// Image padding
	imagePadding(img_dim, img_p_dim, img_pad, img_filtered, img_p, 0);

	imageDilation(img_p, img_filtered, operation_size, img_p_dim, img_dim, 0, img_dim.h);
	imageTopHat(img, img_filtered, img_dim, 0, img_dim.h);


	// Save end time
	auto endTime = std::chrono::system_clock::now();

	// Print results
	auto duration = endTime - startTime;

	printf("----------------------------\n");
	printf("Time duration:  %.3f [ms]\n", std::chrono::duration <double, std::milli>(duration).count());
	
	// ----------------------------------------------------------------------------
	// -------------------- WRITING OUTPUT TO VALIDATE REASULT --------------------
	// ----------------------------------------------------------------------------
	
	printf("Writing output image...\n");

	// Writing filtered image
	int err = stbi_write_jpg(output_path.c_str(), img_dim.w, img_dim.h, img_dim.ch, img_filtered, 100);

	printf("\n\n----------------------------\n");

	if(err) {
		printf("Image has been writen correctly.");
	} else {
		printf("Error while writing image. Error code: %d\n", err);
	}

	// Free alocated memory
	free(img);
	free(img_p);
	free(img_filtered);
	
	return 0;
}
