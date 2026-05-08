// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/kichanova_k_FIIT3_lab4_MLIR.so --pass-pipeline="builtin.module(trace-condition)" %s | FileCheck %s

// CHECK: trace_condition_then_begin: 10
// CHECK-NEXT: trace_condition_then_end: 10
// CHECK-NEXT: trace_condition_else_begin: 3
// CHECK-NEXT: trace_condition_else_end: 3

module {
  // CHECK-LABEL: func.func @test_scf_if_with_else
  // CHECK: %[[ARG0:.*]] = arith.constant 1 : i32
  // CHECK: %[[ARG1:.*]] = arith.constant 0 : i32
  // CHECK-DAG: %[[COND:.*]] = arith.cmpi eq, %{{.*}}, %{{.*}} : i32
  // CHECK: %[[RES:.*]] = scf.if %[[COND]] -> (i32) {
  // CHECK:   func.call @trace_condition_then_begin() : () -> ()
  // CHECK:   func.call @trace_condition_then_end() : () -> ()
  // CHECK:   scf.yield %[[ARG0]] : i32
  // CHECK: } else {
  // CHECK:   func.call @trace_condition_else_begin() : () -> ()
  // CHECK:   func.call @trace_condition_else_end() : () -> ()
  // CHECK:   scf.yield %[[ARG1]] : i32
  // CHECK: }
  func.func @test_scf_if_with_else(%arg0: i32) -> i32 {
    %0 = arith.constant 1 : i32
    %1 = arith.constant 0 : i32
    %2 = arith.cmpi eq, %arg0, %1 : i32
    %3 = scf.if %2 -> i32 {
      scf.yield %0 : i32
    } else {
      scf.yield %1 : i32
    }
    return %3 : i32
  }

  // CHECK-LABEL: func.func @test_scf_if_without_else
  // CHECK: %[[ARG0:.*]] = arith.cmpi eq, %arg0, %arg1 : i32
  // CHECK: scf.if %[[ARG0]] {
  // CHECK:   func.call @trace_condition_then_begin() : () -> ()
  // CHECK:   %[[ADD:.*]] = arith.addi %arg0, %arg1 : i32
  // CHECK:   func.call @trace_condition_then_end() : () -> ()
  // CHECK: }
  func.func @test_scf_if_without_else(%arg0: i32, %arg1: i32) {
    %0 = arith.cmpi eq, %arg0, %arg1 : i32
    scf.if %0 {
      %1 = arith.addi %arg0, %arg1 : i32
    }
    return
  }

  // CHECK-LABEL: func.func @test_affine_if_without_else
  // CHECK: affine.for %{{.*}} = 0 to 10 {
  // CHECK:   affine.if #set(%{{.*}}) {
  // CHECK:     func.call @trace_condition_then_begin() : () -> ()
  // CHECK:     %[[LOAD:.*]] = affine.load %{{.*}}[%{{.*}}] : memref<10xi32>
  // CHECK:     func.call @trace_condition_then_end() : () -> ()
  // CHECK:   }
  // CHECK: }
  func.func @test_affine_if_without_else(%arg0: memref<10xi32>) {
    affine.for %i = 0 to 10 {
      affine.if affine_set<(d0) : (d0 >= 0)>(%i) {
        %0 = affine.load %arg0[%i] : memref<10xi32>
      }
    }
    return
  }

  // CHECK-LABEL: func.func @test_affine_if_with_else
  // CHECK: affine.if #set(%{{.*}}) {
  // CHECK:   func.call @trace_condition_then_begin() : () -> ()
  // CHECK:   %[[ONE:.*]] = arith.constant 1 : i32
  // CHECK:   func.call @trace_condition_then_end() : () -> ()
  // CHECK: } else {
  // CHECK:   func.call @trace_condition_else_begin() : () -> ()
  // CHECK:   %[[ZERO:.*]] = arith.constant 0 : i32
  // CHECK:   func.call @trace_condition_else_end() : () -> ()
  // CHECK: }
  func.func @test_affine_if_with_else(%arg0: memref<10xi32>, %arg1: index) {
    affine.if affine_set<(d0) : (d0 >= 0)>(%arg1) {
      %0 = arith.constant 1 : i32
    } else {
      %0 = arith.constant 0 : i32
    }
    return
  }

  // CHECK-LABEL: func.func @test_nested_scf_if
  // CHECK: %[[COND1:.*]] = arith.cmpi eq, %arg0, %arg1 : i32
  // CHECK: scf.if %[[COND1]] {
  // CHECK:   func.call @trace_condition_then_begin() : () -> ()
  // CHECK:   %[[COND2:.*]] = arith.cmpi eq, %arg2, %arg3 : i32
  // CHECK:   scf.if %[[COND2]] {
  // CHECK:     func.call @trace_condition_then_begin() : () -> ()
  // CHECK:     %[[MUL:.*]] = arith.muli %arg0, %arg2 : i32
  // CHECK:     func.call @trace_condition_then_end() : () -> ()
  // CHECK:   } else {
  // CHECK:     func.call @trace_condition_else_begin() : () -> ()
  // CHECK:     %[[ADD:.*]] = arith.addi %arg1, %arg3 : i32
  // CHECK:     func.call @trace_condition_else_end() : () -> ()
  // CHECK:   }
  // CHECK:   func.call @trace_condition_then_end() : () -> ()
  // CHECK: }
  func.func @test_nested_scf_if(%arg0: i32, %arg1: i32, %arg2: i32, %arg3: i32) {
    %0 = arith.cmpi eq, %arg0, %arg1 : i32
    scf.if %0 {
      %1 = arith.cmpi eq, %arg2, %arg3 : i32
      scf.if %1 {
        %2 = arith.muli %arg0, %arg2 : i32
      } else {
        %2 = arith.addi %arg1, %arg3 : i32
      }
    }
    return
  }

  // CHECK-LABEL: func.func @test_nested_affine_if
  // CHECK: affine.for %{{.*}} = 0 to 10 {
  // CHECK:   affine.if #set(%{{.*}}) {
  // CHECK:     func.call @trace_condition_then_begin() : () -> ()
  // CHECK:     affine.if #set1(%{{.*}}) {
  // CHECK:       func.call @trace_condition_then_begin() : () -> ()
  // CHECK:       %{{.*}} = affine.load %{{.*}}[%{{.*}}] : memref<10xi32>
  // CHECK:       func.call @trace_condition_then_end() : () -> ()
  // CHECK:     }
  // CHECK:     func.call @trace_condition_then_end() : () -> ()
  // CHECK:   }
  // CHECK: }
  func.func @test_nested_affine_if(%arg0: memref<10xi32>) {
    affine.for %i = 0 to 10 {
      affine.if affine_set<(d0) : (d0 >= 0)>(%i) {
        affine.if affine_set<(d0) : (d0 >= 5)>(%i) {
          %0 = affine.load %arg0[%i] : memref<10xi32>
        }
      }
    }
    return
  }

  // CHECK-LABEL: func.func @test_mixed_conditions
  // CHECK: %[[COND:.*]] = arith.cmpi eq, %arg0, %arg1 : i32
  // CHECK: scf.if %[[COND]] {
  // CHECK:   func.call @trace_condition_then_begin() : () -> ()
  // CHECK:   affine.for %{{.*}} = 0 to 10 {
  // CHECK:     affine.if #set(%{{.*}}) {
  // CHECK:       func.call @trace_condition_then_begin() : () -> ()
  // CHECK:       %{{.*}} = affine.load %{{.*}}[%{{.*}}] : memref<10xi32>
  // CHECK:       func.call @trace_condition_then_end() : () -> ()
  // CHECK:     }
  // CHECK:   }
  // CHECK:   func.call @trace_condition_then_end() : () -> ()
  // CHECK: }
  func.func @test_mixed_conditions(%arg0: i32, %arg1: i32, %arg2: memref<10xi32>) {
    %0 = arith.cmpi eq, %arg0, %arg1 : i32
    scf.if %0 {
      affine.for %i = 0 to 10 {
        affine.if affine_set<(d0) : (d0 >= 0)>(%i) {
          %1 = affine.load %arg2[%i] : memref<10xi32>
        }
      }
    }
    return
  }
}