// RUN: %webcl-validator %s 2>&1 | grep -v CHECK | %FileCheck %s

// CHECK: sampler_t global_const_sampler = /* transformed */ CLK_ADDRESS_MIRRORED_REPEAT | CLK_FILTER_NEAREST | CLK_NORMALIZED_COORDS_FALSE;
sampler_t global_const_sampler = 1;

__kernel void unsafe_builtins(image2d_t image)
{
    read_imagef(image, global_const_sampler, (float2)(0, 0));
}
