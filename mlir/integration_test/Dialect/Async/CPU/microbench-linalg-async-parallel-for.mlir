// RUN:   mlir-opt %s                                                          \
// RUN:               -linalg-tile-to-parallel-loops="linalg-tile-sizes=256"   \
// RUN:               -async-parallel-for="num-concurrent-async-execute=4"     \
// RUN:               -async-ref-counting                                      \
// RUN:               -convert-async-to-llvm                                   \
// RUN:               -lower-affine                                            \
// RUN:               -convert-linalg-to-loops                                 \
// RUN:               -convert-scf-to-std                                      \
// RUN:               -std-expand                                              \
// RUN:               -convert-vector-to-llvm                                  \
// RUN:               -convert-std-to-llvm                                     \
// RUN: | mlir-cpu-runner                                                      \
// RUN:  -e entry -entry-point-result=void -O3                                 \
// RUN:  -shared-libs=%mlir_integration_test_dir/libmlir_runner_utils%shlibext \
// RUN:  -shared-libs=%mlir_integration_test_dir/libmlir_async_runtime%shlibext\
// RUN: | FileCheck %s --dump-input=always

// RUN:   mlir-opt %s                                                          \
// RUN:               -convert-linalg-to-loops                                 \
// RUN:               -convert-scf-to-std                                      \
// RUN:               -convert-vector-to-llvm                                  \
// RUN:               -convert-std-to-llvm                                     \
// RUN: | mlir-cpu-runner                                                      \
// RUN:  -e entry -entry-point-result=void -O3                                 \
// RUN:  -shared-libs=%mlir_integration_test_dir/libmlir_runner_utils%shlibext \
// RUN:  -shared-libs=%mlir_integration_test_dir/libmlir_async_runtime%shlibext\
// RUN: | FileCheck %s --dump-input=always

#map0 = affine_map<(d0, d1) -> (d0, d1)>

func @linalg_generic(%lhs: memref<?x?xf32>,
                     %rhs: memref<?x?xf32>,
                     %sum: memref<?x?xf32>) {
  linalg.generic {
    indexing_maps = [#map0, #map0, #map0],
    iterator_types = ["parallel", "parallel"]
  }
    ins(%lhs, %rhs : memref<?x?xf32>, memref<?x?xf32>)
    outs(%sum : memref<?x?xf32>)
  {
    ^bb0(%lhs_in: f32, %rhs_in: f32, %sum_out: f32):
      %0 = addf %lhs_in, %rhs_in : f32
      linalg.yield %0 : f32
  }

  return
}

func @entry() {
  %f1 = constant 1.0 : f32
  %f4 = constant 4.0 : f32
  %c0 = constant 0 : index
  %c1 = constant 1 : index
  %cM = constant 1000 : index

  //
  // Sanity check for the function under test.
  //

  %LHS10 = alloc() {alignment = 64} : memref<1x10xf32>
  %RHS10 = alloc() {alignment = 64} : memref<1x10xf32>
  %DST10 = alloc() {alignment = 64} : memref<1x10xf32>

  linalg.fill(%LHS10, %f1) : memref<1x10xf32>, f32
  linalg.fill(%RHS10, %f1) : memref<1x10xf32>, f32

  %LHS = memref_cast %LHS10 : memref<1x10xf32> to memref<?x?xf32>
  %RHS = memref_cast %RHS10 : memref<1x10xf32> to memref<?x?xf32>
  %DST = memref_cast %DST10 : memref<1x10xf32> to memref<?x?xf32>

  call @linalg_generic(%LHS, %RHS, %DST)
    : (memref<?x?xf32>, memref<?x?xf32>, memref<?x?xf32>) -> ()

  // CHECK: [2, 2, 2, 2, 2, 2, 2, 2, 2, 2]
  %U = memref_cast %DST10 :  memref<1x10xf32> to memref<*xf32>
  call @print_memref_f32(%U): (memref<*xf32>) -> ()

  dealloc %LHS10: memref<1x10xf32>
  dealloc %RHS10: memref<1x10xf32>
  dealloc %DST10: memref<1x10xf32>

  //
  // Allocate data for microbenchmarks.
  //

  %LHS1024 = alloc() {alignment = 64} : memref<1024x1024xf32>
  %RHS1024 = alloc() {alignment = 64} : memref<1024x1024xf32>
  %DST1024 = alloc() {alignment = 64} : memref<1024x1024xf32>

  %LHS0 = memref_cast %LHS1024 : memref<1024x1024xf32> to memref<?x?xf32>
  %RHS0 = memref_cast %RHS1024 : memref<1024x1024xf32> to memref<?x?xf32>
  %DST0 = memref_cast %DST1024 : memref<1024x1024xf32> to memref<?x?xf32>

  //
  // Warm up.
  //

  call @linalg_generic(%LHS0, %RHS0, %DST0)
    : (memref<?x?xf32>, memref<?x?xf32>, memref<?x?xf32>) -> ()

  //
  // Measure execution time.
  //

  %t0 = call @rtclock() : () -> f64
  scf.for %i = %c0 to %cM step %c1 {
    call @linalg_generic(%LHS0, %RHS0, %DST0)
      : (memref<?x?xf32>, memref<?x?xf32>, memref<?x?xf32>) -> ()
  }
  %t1 = call @rtclock() : () -> f64
  %t1024 = subf %t1, %t0 : f64

  // Print timings.
  vector.print %t1024 : f64

  // Free.
  dealloc %LHS1024: memref<1024x1024xf32>
  dealloc %RHS1024: memref<1024x1024xf32>
  dealloc %DST1024: memref<1024x1024xf32>

  return
}

func private @rtclock() -> f64

func private @print_memref_f32(memref<*xf32>)
  attributes { llvm.emit_c_interface }
