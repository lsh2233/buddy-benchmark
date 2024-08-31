//===- GoogleBenchmarkMain.cpp---------------------------------------------===//
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
// This file implements the benchmark for conv2d(nhwc-fhwc) operation.
//
//===----------------------------------------------------------------------===//

#include <benchmark/benchmark.h>
#include <buddy/Core/Container.h>
#include <iostream>
#include <random>

// Define target layout.
#define INPUT_N 2
#define INPUT_H 32
#define INPUT_W 32
#define INPUT_C 18
#define FILTER_H 4
#define FILTER_W 4
#define FILTER_C 18
#define FILTER_F 66
#define OUTPUT_N 2
#define OUTPUT_H 29
#define OUTPUT_W 29
#define OUTPUT_F 66

// Helper functions and variables.
namespace {
const std::string PASS = "\033[32mPASS\033[0m";
const std::string FAIL = "\033[31mFAIL\033[0m";

#define ABS(a) ((a) > 0 ? (a) : -(a))

bool areArraysEqual(float array1[], float array2[], int size) {
  for (int i = 0; i < size; ++i) {
    if (ABS((array1[i] - array2[i]) / array2[i]) > 1e-2) {
      printf("array1[%d]:%f array2[%d]:%f\n", i, array1[i], i, array2[i]);
      return false;
    }
  }
  return true;
}
} // namespace

namespace {
// Declare the mobilenet C interface.
extern "C" {
void _mlir_ciface_conv_2d_nhwc_fhwc_scalar(MemRef<float, 4> *input,
                                           MemRef<float, 4> *filter,
                                           MemRef<float, 4> *output);
void _mlir_ciface_conv_2d_nhwc_fhwc_auto_vectorization(
    MemRef<float, 4> *input, MemRef<float, 4> *filter,
    MemRef<float, 4> *output);
void _mlir_ciface_conv_2d_nhwc_fhwc_vectorization(MemRef<float, 4> *input,
                                                  MemRef<float, 4> *filter,
                                                  MemRef<float, 4> *output);
}

template <typename Func>
void DL_OPS_CONV_2D_NHWC_FHWC(benchmark::State &state, Func func) {

  intptr_t sizesInput[4] = {INPUT_N, INPUT_H, INPUT_W, INPUT_C};
  intptr_t sizesFilter[4] = {FILTER_F, FILTER_H, FILTER_W, FILTER_C};
  intptr_t sizesOutput[4] = {OUTPUT_N, OUTPUT_H, OUTPUT_W, OUTPUT_F};

  MemRef<float, 4> input(sizesInput, 1.0);
  MemRef<float, 4> filter(sizesFilter, 1.0);
  MemRef<float, 4> output(sizesOutput, 0.0);

  for (auto _ : state) {
    func(&input, &filter, &output);
  }
}
} // namespace

// Register benchmarking function with different arguments.
BENCHMARK_CAPTURE(DL_OPS_CONV_2D_NHWC_FHWC, Scalar,
                  _mlir_ciface_conv_2d_nhwc_fhwc_scalar)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(DL_OPS_CONV_2D_NHWC_FHWC, AutoVectorization,
                  _mlir_ciface_conv_2d_nhwc_fhwc_auto_vectorization)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(DL_OPS_CONV_2D_NHWC_FHWC, Vectorization,
                  _mlir_ciface_conv_2d_nhwc_fhwc_vectorization)
    ->Unit(benchmark::kMillisecond);
// BENCHMARK_CAPTURE(DL_OPS_CONV_2D_NHWC_FHWC, RvvVectorization,
//                   _mlir_ciface_bvv_vectorization)
//     ->Unit(benchmark::kMillisecond);

/// Correctness Verification
/// The verification does not affect the performance.
/// - Set the scalar case as the criteria.
/// - Input elements are random numbers.
/// - Output elements are initialized to zero.
/// - Compare the output of various optimizations with the scalar version to
///   verify correctness.
void verification() {
  // Set the random number generator.
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<int> distribution(0.0, 1.0);

  // Set the layout sizes of input and output memref container.
  intptr_t sizesInput[4] = {INPUT_N, INPUT_H, INPUT_W, INPUT_C};
  intptr_t sizesFilter[4] = {FILTER_F, FILTER_H, FILTER_W, FILTER_C};
  intptr_t sizesOutput[4] = {OUTPUT_N, OUTPUT_H, OUTPUT_W, OUTPUT_F};

  // Generate input A and input B memref container with random numbers.
  const int inputSize = INPUT_N * INPUT_H * INPUT_W * INPUT_C;
  float inputRand[inputSize];
  for (int i = 0; i < inputSize; ++i) {
    inputRand[i] = distribution(generator);
  }
  MemRef<float, 4> inputMemRef(inputRand, sizesInput);

  const int filterSize = FILTER_F * FILTER_H * FILTER_W * FILTER_C;
  float filterRand[filterSize];
  for (int i = 0; i < filterSize; ++i) {
    filterRand[i] = distribution(generator);
  }
  MemRef<float, 4> filterMemRef(filterRand, sizesFilter);

  // Generate output memref container with zero.
  const int outputSize = OUTPUT_N * OUTPUT_H * OUTPUT_W * OUTPUT_F;
  MemRef<float, 4> outputScalar(sizesOutput, 0.0);
  MemRef<float, 4> outputAutoVectorization(sizesOutput, 0.0);
  MemRef<float, 4> outputVectorization(sizesOutput, 0.0);

  // Perform all the matmul implementation.
  _mlir_ciface_conv_2d_nhwc_fhwc_scalar(&inputMemRef, &filterMemRef,
                                        &outputScalar);
  _mlir_ciface_conv_2d_nhwc_fhwc_auto_vectorization(&inputMemRef, &filterMemRef,
                                                    &outputAutoVectorization);
  _mlir_ciface_conv_2d_nhwc_fhwc_vectorization(&inputMemRef, &filterMemRef,
                                               &outputVectorization);

  // Get the result array.
  auto resultScalar = outputScalar.getData();
  auto resultAutoVectorization = outputAutoVectorization.getData();
  auto resultVectorization = outputVectorization.getData();

  // Print the verfication result.
  std::cout << "-----------------------------------------------------------"
            << std::endl;
  std::cout << "Correctness Verification:" << std::endl;
  std::cout << "Transform case: "
            << (areArraysEqual(resultScalar, resultVectorization, outputSize)
                    ? PASS
                    : FAIL)
            << std::endl;
  std::cout << "-----------------------------------------------------------"
            << std::endl;
}

int main(int argc, char **argv) {
  // Run benchmark.
  ::benchmark::Initialize(&argc, argv);
  ::benchmark::RunSpecifiedBenchmarks();
  // Run correctness verification.
  verification();
  return 0;
}
