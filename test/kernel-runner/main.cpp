/*
** Copyright (c) 2013 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#include "kernelargs.hpp"
#include "driver_blacklist.hpp"

#include <stdlib.h>

#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <iterator>

#include <cstring>

#include "llvm/Support/TimeValue.h"

#define SCALAR 0

#define DEVICE_TYPE CL_DEVICE_TYPE_ALL


namespace
{
    typedef std::vector<cl_platform_id> platform_vector;
    typedef std::vector<cl_device_id> device_vector;
}

platform_vector getPlatformsIDs()
{
    const platform_vector::size_type maxPlatforms = 10;
    platform_vector platformIds(maxPlatforms);

    cl_uint numOfPlatforms = 0;
    cl_int ret = clGetPlatformIDs(maxPlatforms, platformIds.data(), &numOfPlatforms);
    if (ret != CL_SUCCESS)
    {
        std::cerr << "Failed to get platform ids." << std::endl;
        numOfPlatforms = 0;
    }
    platformIds.resize(numOfPlatforms);
    return platformIds;
}

device_vector getDevices(cl_platform_id const& platformId)
{
    cl_uint num_devices = 0;
    if (CL_SUCCESS != clGetDeviceIDs(platformId, DEVICE_TYPE, 0, NULL, &num_devices))
    {
        std::cerr << "Failed to get number of devices." << std::endl;
        return device_vector();
    }

    device_vector devices(num_devices);
    if (CL_SUCCESS != clGetDeviceIDs(platformId, DEVICE_TYPE, num_devices, devices.data(), NULL))
    {
        std::cerr << "clGetDeviceIDs failed." << std::endl;
        num_devices = 0;
    }
    devices.resize(num_devices);
    return devices;
}

bool checkDevInfo(cl_device_id device)
{
    char devbuf[128];
    char verbuf[128];
    clGetDeviceInfo(device, CL_DEVICE_NAME, 128, devbuf, NULL);
    clGetDeviceInfo(device, CL_DEVICE_VERSION, 128, verbuf, NULL);
    std::cout << "Device " << devbuf << ", version " << verbuf << std::endl;

    return is_blacklisted(devbuf);
}

std::string readAllInput()
{
    // don't skip the whitespace while reading
    std::cin >> std::noskipws;

    // use stream iterators to copy the stream to a string
    std::istream_iterator<char> begin(std::cin);
    std::istream_iterator<char> end;
    return std::string(begin, end);
}

typedef struct { 
    int initType; 
    int as; 
    int size; 
    int length; 
    float init; } BufferArg;

bool testSource(int id, cl_device_id device, std::string const& source, 
    std::string &kernelName, int globalWorkSize, int loopCount, std::vector<BufferArg> &buffers, bool isTransformed, 
    char* programOutput, bool debug, bool hasOutput)
{
    using llvm::sys::TimeValue;

    cl_int ret = CL_SUCCESS;

    // Create an OpenCL context
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &ret);
    if (debug) std::cerr << "Creating context.\n";
    if (ret != CL_SUCCESS)
    {
        std::cerr << "Failed to create OpenCL context." << std::endl;
        return false;
    }

    // Create the program
    const char *buf = source.c_str();
    if (debug) std::cerr << "Creating program.\n";
    cl_program program = clCreateProgramWithSource(context, 1, (const char **)&buf, NULL, &ret);

    // Build the program
    if (debug) std::cerr << "Building program.\n";
    if (CL_SUCCESS != clBuildProgram(program, 1, &device, NULL, NULL, NULL))
    {
        std::cerr << "Failed to build program." << std::endl;
        return false;
    }

    if (debug) std::cerr << "Creating command queue.\n";
    cl_command_queue command_queue = clCreateCommandQueue(context, device, 0, &ret);
    if (debug) std::cerr << "Creating kernel.\n";
    cl_kernel kernel = clCreateKernel(program, kernelName.c_str(), &ret);
    KernelArgs args(kernel, isTransformed);

    std::vector<cl_mem> cleanUpVec;
    char retVal[1024] = {0};
    cl_mem retBuf = clCreateBuffer(context, 
        CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR, 
        1024 * sizeof(cl_char), &retVal, &ret);
    cleanUpVec.push_back(retBuf);
    if (hasOutput) {
        if (debug) std::cerr << "Adding char* output buffer\n";
        args.appendArray(&retBuf, 1024);
    } else {
        if (debug) std::cerr << "Skipping char* output buffer\n";
    }

    // init data for float / int types
    std::vector<cl_float> floatInit(128000);
    std::vector<cl_int> intInit(128000);
    std::vector<cl_uchar> ucharInit(128000);
    for (int i = 0; i < 128000; i++) { 
        intInit[i] = i;
        floatInit[i] = i;
        ucharInit[i] = i;
    }

    TimeValue allocate_buffers_begin = TimeValue::now();

    for (unsigned i = 0; i < buffers.size(); i++) {        
        if (buffers[i].size == SCALAR) {
            switch (buffers[i].initType) {
            case 0:
                args.appendInt((int)buffers[i].init);
                break;
            case 1:
                args.appendFloat(buffers[i].init);
                break;
            default:
                std::cerr << "Unhandled argument type: " << buffers[i].initType << std::endl;
                return false;
            }
        } else {
            if (buffers[i].as == 1) {
                args.appendArray(buffers[i].size, NULL, buffers[i].length);
            } else {
                void *initBuffer = NULL;
                switch(buffers[i].initType) {
                case 0:
                    initBuffer = &intInit[0];
                    break;
                case 1:
                    initBuffer = &floatInit[0];
                    break;
                case 2:
                    initBuffer = &ucharInit[0];
                    break;
                default:
                    std::cerr << "Unhandled buffer type: " << buffers[i].initType << std::endl;
                    return false;
                }
                cl_mem memBuf = clCreateBuffer(context, 
                    CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, 
                    buffers[i].size, initBuffer, &ret);
                cleanUpVec.push_back(memBuf);
                args.appendArray(&memBuf, buffers[i].length);
            }        
        }
    }

    ret = clFinish(command_queue);

    TimeValue enqueue_kernel_begin = TimeValue::now();

    // Execute the OpenCL kernel on the list
    size_t global_item_size = globalWorkSize; // Process the entire lists
    if (debug) std::cerr << "Enqueue kernel.\n";

    for ( int loops = 0; loops < loopCount; loops++) {
        ret = clEnqueueNDRangeKernel(command_queue, kernel, 1,
                                     NULL, &global_item_size, NULL,
                                     0, NULL, NULL);

        if (ret != CL_SUCCESS) {
            std::cerr << "clEnqueueNDRangeKernel failed with code " << ret << std::endl;
            return false;
        }
    }
    
    ret = clFinish(command_queue);

    TimeValue enqueue_kernel_end = TimeValue::now();

    double allocate_buffers_ms = (enqueue_kernel_begin - allocate_buffers_begin).msec();
    double enqueue_kernel_ms = (enqueue_kernel_end - enqueue_kernel_begin).msec();

    double elapsed_ms = allocate_buffers_ms + enqueue_kernel_ms;
    std::cerr << "allocate_buffers: " << allocate_buffers_ms << "ms\n"
              << "enqueue_kernel: " << enqueue_kernel_ms << "ms\n"
              << "total:" << elapsed_ms << "ms\n";

    bool testPass = true;
    if (hasOutput)
    {
        if (debug) std::cerr << "Finish queue.\n";
        ret = clFinish(command_queue);
        if (debug) std::cerr << "Reading retval.\n";
        ret = clEnqueueReadBuffer(command_queue, retBuf, CL_TRUE, 0, 1024, retVal, 0, NULL, NULL);
        ret = clFinish(command_queue);

        if (id == 0) {
            memcpy(programOutput, retVal, 1024);
        } else {
            // if ran with multiple implementations verify that all results are equal
            testPass = memcmp(programOutput, retVal, 1024) == 0;
        }
    }

    // Clean up
    bool cleanupOk = true;
    if (debug) std::cerr << "Cleaning up everything.\n";
    cleanupOk &= CL_SUCCESS == clFinish(command_queue);
    cleanupOk &= CL_SUCCESS == clReleaseKernel(kernel);
    cleanupOk &= CL_SUCCESS == clReleaseProgram(program);
    for (unsigned i = 0; i < cleanUpVec.size(); i++) {
        cleanupOk &= CL_SUCCESS == clReleaseMemObject(cleanUpVec[i]);
    }
    cleanupOk &= CL_SUCCESS == clReleaseCommandQueue(command_queue);
    cleanupOk &= CL_SUCCESS == clReleaseContext(context);
    if (!cleanupOk) {
        std::cerr << "OpenCL program run was not cleaned up properly." << std::endl;
    }

    return testPass;
}

std::string usage = 
"/// Runs opencl / webcl kernel and prints out the return value table.\n"
"/// \n"
"/// __kernel <kernel_name>(__global char *ret_val, <rest of the args> );\n"
"///\n"
"/// ret_val buffer is 1024 bytes and it is read as null terminated string.\n"
"///\n"
"/// --gcount   <size> Number of global work items. Default: 1\n"
"/// --webcl    Runs instrumented kernel; adds also required size \n"
"///            arguments when running kernel.\n"
"/// --kernel   <kernel_name> Name of the kernel to run. default: test_kernel\n"
"/// --global   <type> <size> Adds global address space buffer.\n"
"/// --local    <type> <size> Adds local address space buffer.\n"
"/// --constant <type> <size> Adds constant address space buffer.\n"
"/// --scalar   <type> <val> Add scalar argument to kernel.\n"
"/// -d         Print out parsed options and other debug.\n"
"/// --nooutput Do not require char* ret_val argument..\n"
"/// --loop     <int> How many times kernel will be called.\n"
"///\n"
"/// All buffers are initialized with values from 0 to buffer size.\n"
"/// <type> can be one of int,float,int<2-16>,float<2-16>\n"
"///\n";
int main(int argc, char const* argv[])
{
    std::set<std::string> help;
    help.insert("-h");
    help.insert("-help");
    help.insert("--help");

    if ((argc < 2) || help.count(argv[1])) {
        std::cerr << "Usage: cat FILE | " << argv[0] << " [OPTIONS]"
                  << std::endl;
        std::cerr << "Runs OpenCL kernel."
                  << std::endl
                  << usage
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::string source = readAllInput();
    if (source.size() == 0)
    {
        std::cerr << "No OpenCL source file read from stdin." << std::endl;
        return EXIT_FAILURE;
    }

    // commandline options
    std::set<std::string> debug;
    debug.insert("--debug");
    debug.insert("-d");
    std::set<std::string> webcl;
    webcl.insert("--webcl");
    std::set<std::string> kernelName;
    kernelName.insert("--kernel");
    std::set<std::string> globalBuffer;
    globalBuffer.insert("--global");
    std::set<std::string> localBuffer;
    localBuffer.insert("--local");
    std::set<std::string> constantBuffer;
    constantBuffer.insert("--constant");
    std::set<std::string> gcount;
    gcount.insert("--gcount");
    std::set<std::string> nooutput;
    nooutput.insert("--nooutput");
    std::set<std::string> scalar;
    scalar.insert("--scalar");
    std::set<std::string> loopcount;
    loopcount.insert("--loop");

    std::vector<BufferArg> buffers;
    std::string kernel = "test_kernel";
    bool useWebCL = false;
    bool printDebug = false;
    int globalWorkItemCount = 1;
    bool addOutput = true;
    int loopCount = 1;

    std::map<std::string, int> atotype;
    atotype["int"]   = 0;
    atotype["int2"]  = 0;
    atotype["int3"]  = 0;
    atotype["int4"]  = 0;
    atotype["int8"]  = 0;
    atotype["int16"] = 0;
    atotype["float"]   = 1;
    atotype["float2"]  = 1;
    atotype["float3"]  = 1;
    atotype["float4"]  = 1;
    atotype["float8"]  = 1;
    atotype["float16"] = 1;
    atotype["uchar"]   = 2;
    atotype["uchar2"]  = 2;
    atotype["uchar3"]  = 2;
    atotype["uchar4"]  = 2;
    atotype["uchar8"]  = 2;
    atotype["uchar16"] = 2;

    std::map<std::string, int> atotypesize;
    atotypesize["int"]   = sizeof(int);
    atotypesize["int2"]  = 2*sizeof(int);
    atotypesize["int3"]  = 4*sizeof(int);
    atotypesize["int4"]  = 4*sizeof(int);
    atotypesize["int8"]  = 8*sizeof(int);
    atotypesize["int16"] = 16*sizeof(int);
    atotypesize["float"]   = 1*sizeof(float);
    atotypesize["float2"]  = 2*sizeof(float);
    atotypesize["float3"]  = 4*sizeof(float);
    atotypesize["float4"]  = 4*sizeof(float);
    atotypesize["float8"]  = 8*sizeof(float);
    atotypesize["float16"] = 16*sizeof(float);
    atotypesize["uchar"]   = 1*sizeof(cl_uchar);
    atotypesize["uchar2"]  = 2*sizeof(cl_uchar);
    atotypesize["uchar3"]  = 4*sizeof(cl_uchar);
    atotypesize["uchar4"]  = 4*sizeof(cl_uchar);
    atotypesize["uchar8"]  = 8*sizeof(cl_uchar);
    atotypesize["uchar16"] = 16*sizeof(cl_uchar);

    // parse commandline and expect that user is not hostile
    for (int i = 1; i < argc; ++i) {
        if (webcl.count(argv[i])) {
            useWebCL = true;
        } else if (kernelName.count(argv[i])) {
            kernel = argv[i+1];
            i++;
        } else if (globalBuffer.count(argv[i])) {
            BufferArg buf = { atotype[argv[i+1]], 3, atotypesize[argv[i+1]] * atoi(argv[i+2]), atoi(argv[i+2]), 0 };
            buffers.push_back(buf);
            i+=2;
        } else if (constantBuffer.count(argv[i])) {
            BufferArg buf = { atotype[argv[i+1]], 2, atotypesize[argv[i+1]] * atoi(argv[i+2]), atoi(argv[i+2]), 0 };
            buffers.push_back(buf);
            i+=2;
        } else if (localBuffer.count(argv[i])) {
            BufferArg buf = { atotype[argv[i+1]], 1, atotypesize[argv[i+1]] * atoi(argv[i+2]), atoi(argv[i+2]), 0 };
            buffers.push_back(buf);
            i+=2;
        } else if (gcount.count(argv[i])) {
            globalWorkItemCount = atoi(argv[i+1]);
            i++;
        } else if (debug.count(argv[i])) {
            printDebug = true;
        } else if (nooutput.count(argv[i])) {
            addOutput = false;
        } else if (scalar.count(argv[i])) {
            BufferArg buf = { atotype[argv[i+1]], 0, SCALAR, 1, atof(argv[i+2]) };
            buffers.push_back(buf);
            i+=2;
        } else if (loopcount.count(argv[i])) {
            loopCount = atoi(argv[i+1]);
            i++;
        }
    }
    if (printDebug) {
        std::cerr << "webcl: " << useWebCL << std::endl
                  << "kernel:" << kernel << std::endl
                  << "gcount:" << globalWorkItemCount << std::endl;
        for (unsigned int i = 0; i < buffers.size(); i++) {
            std::cerr << "buffer: " << buffers[i].as << ":" << buffers[i].size << ":" << buffers[i].length << "\n";
        }
    }

    std::string retVal(1024, '\0');
    platform_vector const& platforms = getPlatformsIDs();
    int id = 0;
    for (platform_vector::const_iterator platform = platforms.begin();
         platform != platforms.end(); ++platform)
    {
        std::string platName(1024, '\0');
        std::size_t platNameLen = 0U;
        clGetPlatformInfo(*platform, CL_PLATFORM_NAME, 1024, &platName[0], &platNameLen);
        platName.resize(platNameLen);
        std::cerr << "Platform: " << platName << std::endl;

        device_vector devices = getDevices(*platform);
        for (device_vector::const_iterator device = devices.begin();
             device != devices.end(); ++device)
        {        
            if (!checkDevInfo(*device)) {
                if (!testSource(id, *device, source, kernel, globalWorkItemCount, loopCount, buffers, useWebCL, &retVal[0], printDebug, addOutput))
                {
                    return EXIT_FAILURE;
                }
            } else {
                std::cout << "Driver blacklisted... skipping\n";
            }
            id++;
        }
    }

    // print out results
    if (addOutput) {
        for (unsigned i = 0; i < 1024; i++) {
            std::cout << (int)(retVal[i]) << ",";
        }
        std::cout << std::endl;
    }

    return EXIT_SUCCESS;
}
