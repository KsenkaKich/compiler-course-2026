; RUN: opt -load-pass-plugin %llvmshlibdir/kichanova_k_FIIT3_lab2_LLVM_IR%pluginext\
; RUN: -passes=decompose_remainder -S %s | FileCheck %s


; CHECK-LABEL: @test_frem
; CHECK-NOT: frem
; CHECK: %[[DIV:.*]] = fdiv double %a, %b
; CHECK: %[[MUL:.*]] = fmul double %[[DIV]], %b
; CHECK: %[[RES:.*]] = fsub double %a, %[[MUL]]
; CHECK-NEXT: ret double %[[RES]]

define double @test_frem(double %a, double %b) {
  %rem = frem double %a, %b
  ret double %rem
}


; CHECK-LABEL: @test_srem
; CHECK-NOT: srem
; CHECK: %[[DIV:.*]] = sdiv i32 %a, %b
; CHECK: %[[MUL:.*]] = mul i32 %[[DIV]], %b
; CHECK: %[[RES:.*]] = sub i32 %a, %[[MUL]]
; CHECK-NEXT: ret i32 %[[RES]]

define i32 @test_srem(i32 %a, i32 %b) {
  %rem = srem i32 %a, %b
  ret i32 %rem
}


; CHECK-LABEL: @test_urem
; CHECK-NOT: urem
; CHECK: %[[DIV:.*]] = udiv i32 %a, %b
; CHECK: %[[MUL:.*]] = mul i32 %[[DIV]], %b
; CHECK: %[[RES:.*]] = sub i32 %a, %[[MUL]]
; CHECK-NEXT: ret i32 %[[RES]]

define i32 @test_urem(i32 %a, i32 %b) {
  %rem = urem i32 %a, %b
  ret i32 %rem
}


; CHECK-LABEL: @complex_test
; CHECK: %[[DIV1:.*]] = fdiv double %x, %y
; CHECK: %[[MUL1:.*]] = fmul double %[[DIV1]], %y
; CHECK: %[[REM1:.*]] = fsub double %x, %[[MUL1]]
; CHECK: %[[DIV2:.*]] = fdiv double %[[REM1]], %z
; CHECK: %[[MUL2:.*]] = fmul double %[[DIV2]], %z
; CHECK: %[[REM2:.*]] = fsub double %[[REM1]], %[[MUL2]]
; CHECK-NEXT: ret double %[[REM2]]
; CHECK-NOT: frem

define double @complex_test(double %x, double %y, double %z) {
  %rem1 = frem double %x, %y
  %rem2 = frem double %rem1, %z
  ret double %rem2
}


; CHECK-LABEL: @vector_test
; CHECK-NOT: frem
; CHECK: %[[DIV:.*]] = fdiv <4 x float> %a, %b
; CHECK: %[[MUL:.*]] = fmul <4 x float> %[[DIV]], %b
; CHECK: %[[RES:.*]] = fsub <4 x float> %a, %[[MUL]]
; CHECK-NEXT: ret <4 x float> %[[RES]]

define <4 x float> @vector_test(<4 x float> %a, <4 x float> %b) {
  %rem = frem <4 x float> %a, %b
  ret <4 x float> %rem
}


; CHECK-LABEL: @check-next_test
; CHECK: %[[DIV:.*]] = fdiv float %a, %b
; CHECK-NEXT: %[[MUL:.*]] = fmul float %[[DIV]], %b
; CHECK-NEXT: %[[RES:.*]] = fsub float %a, %[[MUL]]
; CHECK-NEXT: ret float %[[RES]]

define float @check-next_test(float %a, float %b) {
  %rem = frem float %a, %b
  ret float %rem
}


; CHECK-LABEL: @check-dag_test
; CHECK-DAG: %[[DIV:.*]] = fdiv double %a, %b
; CHECK-DAG: %[[MUL:.*]] = fmul double %[[DIV]], %b
; CHECK-DAG: %[[RES:.*]] = fsub double %a, %[[MUL]]
; CHECK-NOT: frem
; CHECK: ret double %[[RES]]

define double @check-dag_test(double %a, double %b) {
  %rem = frem double %a, %b
  ret double %rem
}


; CHECK-LABEL: @check-label_test
; CHECK: start:
; CHECK-NEXT: %[[DIV:.*]] = fdiv double %a, %b
; CHECK-NEXT: %[[MUL:.*]] = fmul double %[[DIV]], %b
; CHECK-NEXT: %[[RES:.*]] = fsub double %a, %[[MUL]]
; CHECK: end:
; CHECK-NEXT: ret double %[[RES]]

define double @check-label_test(double %a, double %b) {
start:
  %rem = frem double %a, %b
  br label %end
end:
  ret double %rem
}


; CHECK-LABEL: @no_changes_test
; CHECK: %add = fadd double %a, %b
; CHECK: %sub = fsub double %a, %b
; CHECK: %mul = fmul double %a, %b
; CHECK: %div = fdiv double %a, %b
; CHECK-NOT: frem
; CHECK: ret double %add

define double @no_changes_test(double %a, double %b) {
  %add = fadd double %a, %b
  %sub = fsub double %a, %b
  %mul = fmul double %a, %b
  %div = fdiv double %a, %b
  ret double %add
}