#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "lib/sgbm_helper.hpp"
//#include "cuda_runtime.h"
#include "lib/sgbm_helper.cuh"
#include <iostream>
#include <chrono>

#define D 51

// pkg-config --cflags opencv4 --libs


using namespace cv;
using namespace std;

int main( int argc, char** argv )
{
    if( argc != 3)
    {
     cout <<" Usage: sgbm Image1 Image2" << endl;
     return -1;
    }

    Mat im1;
    Mat im2;
    im1 = imread(argv[1], IMREAD_GRAYSCALE);
    im2 = imread(argv[2], IMREAD_GRAYSCALE);
    resize(im1, im1, Size(480,480));
    resize(im2, im2, Size(480,480));

    if(! im1.data | ! im2.data )
    {
        cout <<  "Could not open or find the image" << std::endl ;
        return -1;
    }


    CV_Assert(im1.depth() == CV_8U);
    int nRows = im1.rows;
    int nCols = im1.cols;


    unsigned long long int * cim1;
    unsigned long long int * cim2;
    float * shifted_images;
    cim1 = new unsigned long long int [nCols * nRows];
    cim2 = new unsigned long long int [nCols * nRows];
    shifted_images = new float [nCols * nRows * D];
    
    // census transform both images
    census_transform_mat(&im1, cim1, nRows, nCols, 7);
    census_transform_mat(&im2, cim2, nRows, nCols, 7);

    // if shifted_images, cim1, cim2 were numpy arrays this would compute with dimensions row, col, disparity
    // shifted_images[i,j,D] = cim1[i,j+D] - cim2[i,j]
    // with padding where necessary
    shift_subtract_stack(cim1, cim2, shifted_images, nRows, nCols, D);
    

    //std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    //std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]" << std::endl;


    // cudaMalloc modifies this pointer to point to block of memory on device
    float* gpu_ptr_shifted_im;
    cudaMalloc((void **) &gpu_ptr_shifted_im, sizeof(float) * nCols * nRows * D);
    cudaMemcpy(gpu_ptr_shifted_im, shifted_images, sizeof(float) * nCols * nRows * D, cudaMemcpyHostToDevice);
      
    // aggregated image will go here
    float* gpu_ptr_agg_im;
    cudaMalloc((void **) &gpu_ptr_agg_im, sizeof(float) * nCols * nRows * D);
    cudaMemset(gpu_ptr_agg_im, 0, sizeof(float) * nCols * nRows * D);

    // each direction of aggregation
    gpu_ptr_agg_im = r_aggregate(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = l_aggregate(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = vertical_aggregate_down(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = vertical_aggregate_up(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = diagonal_tl_br_aggregate(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = diagonal_tr_bl_aggregate(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = diagonal_br_tl_aggregate(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);
    gpu_ptr_agg_im = diagonal_bl_tr_aggregate(nCols, nRows, gpu_ptr_shifted_im, gpu_ptr_agg_im);

    // argmin
    float * stereo_im;
    cudaMalloc((void **) &stereo_im, sizeof(float) * nCols * nRows);
    cudaMemset(stereo_im, 0, sizeof(float) * nCols * nRows);
    //argmin_3d_mat()





    return 0;
}