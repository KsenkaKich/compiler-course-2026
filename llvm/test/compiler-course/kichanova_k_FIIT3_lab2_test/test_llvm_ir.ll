; RUN: opt -load-pass-plugin %llvmshlibdir/kichanova_k_FIIT3_lab2_LLVM_IR%pluginext\
; RUN: -passes=decompose_remainder -S %s | FileCheck %s


; CHECK-LABEL: @test_frem
; CHECK-NOT: frem
; CHECK: fdiv
; CHECK: fmul
; CHECK: fsub
; CHECK-NEXT: ret

define double @test_frem(double %a, double %b) {
  %rem = frem double %a, %b
  ret double %rem
}


; CHECK-LABEL: @test_srem
; CHECK-NOT: srem
; CHECK: sdiv
; CHECK: mul
; CHECK: sub
; CHECK-NEXT: ret

define i32 @test_srem(i32 %a, i32 %b) {
  %rem = srem i32 %a, %b
  ret i32 %rem
}


; CHECK-LABEL: @test_urem
; CHECK-NOT: urem
; CHECK: udiv
; CHECK: mul
; CHECK: sub
; CHECK-NEXT: ret

define i32 @test_urem(i32 %a, i32 %b) {
  %rem = urem i32 %a, %b
  ret i32 %rem
}


; CHECK-LABEL: @complex_test
; CHECK: fdiv
; CHECK: fmul
; CHECK: fsub
; CHECK: fdiv
; CHECK: fmul
; CHECK: fsub
; CHECK-NOT: frem

define double @complex_test(double %x, double %y, double %z) {
  %rem1 = frem double %x, %y
  %rem2 = frem double %rem1, %z
  ret double %rem2
}


; CHECK-LABEL: @vector_test
; CHECK-NOT: frem
; CHECK: fdiv <4 x float>
; CHECK: fmul <4 x float>
; CHECK: fsub <4 x float>
; CHECK-NEXT: ret

define <4 x float> @vector_test(<4 x float> %a, <4 x float> %b) {
  %rem = frem <4 x float> %a, %b
  ret <4 x float> %rem
}


; CHECK-LABEL: @check-next_test
; CHECK: fdiv
; CHECK-NEXT: fmul
; CHECK-NEXT: fsub
; CHECK-NEXT: ret

define float @check-next_test(float %a, float %b) {
  %rem = frem float %a, %b
  ret float %rem
}


; CHECK-LABEL: @check-dag_test
; CHECK-DAG: fdiv
; CHECK-DAG: fmul
; CHECK-DAG: fsub
; CHECK-NOT: frem

define double @check-dag_test(double %a, double %b) {
  %rem = frem double %a, %b
  ret double %rem
}


; CHECK-LABEL: @check-label_test
; CHECK: start:
; CHECK-NEXT: fdiv
; CHECK-NEXT: fmul
; CHECK-NEXT: fsub
; CHECK: end:
; CHECK-NEXT: ret

define double @check-label_test(double %a, double %b) {
start:
  %rem = frem double %a, %b
  br label %end
end:
  ret double %rem
}


; CHECK-LABEL: @no_changes_test
; CHECK: fadd
; CHECK: fsub
; CHECK: fmul
; CHECK: fdiv
; CHECK-NOT: frem

define double @no_changes_test(double %a, double %b) {
  %add = fadd double %a, %b
  %sub = fsub double %a, %b
  %mul = fmul double %a, %b
  %div = fdiv double %a, %b
  ret double %add
}