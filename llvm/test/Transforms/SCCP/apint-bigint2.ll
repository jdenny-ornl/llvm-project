; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt < %s -passes=sccp -S | FileCheck %s

@Y = constant [6 x i101] [ i101 12, i101 123456789000000, i101 -12,
  i101 -123456789000000, i101 0,i101 9123456789000000]

define i101 @array() {
; CHECK-LABEL: @array(
; CHECK-NEXT:    ret i101 123456789000000
;
  %A = getelementptr [6 x i101], ptr @Y, i32 0, i32 1
  %B = load i101, ptr %A
  %D = and i101 %B, 1
  %DD = or i101 %D, 1
  %E = trunc i101 %DD to i32
  %F = getelementptr [6 x i101], ptr @Y, i32 0, i32 %E
  %G = load i101, ptr %F

  ret i101 %G
}

define i101 @large_aggregate() {
; CHECK-LABEL: @large_aggregate(
; CHECK-NEXT:    [[D:%.*]] = and i101 undef, 1
; CHECK-NEXT:    [[DD:%.*]] = or i101 [[D]], 1
; CHECK-NEXT:    [[G:%.*]] = getelementptr i101, ptr getelementptr inbounds nuw (i8, ptr @Y, i64 80), i101 [[DD]]
; CHECK-NEXT:    [[L3:%.*]] = load i101, ptr [[G]], align 4
; CHECK-NEXT:    ret i101 [[L3]]
;
  %B = load i101, ptr undef
  %D = and i101 %B, 1
  %DD = or i101 %D, 1
  %F = getelementptr [6 x i101], ptr @Y, i32 0, i32 5
  %G = getelementptr i101, ptr %F, i101 %DD
  %L3 = load i101, ptr %G
  ret i101 %L3
}

define i101 @large_aggregate_2() {
; CHECK-LABEL: @large_aggregate_2(
; CHECK-NEXT:    [[D:%.*]] = and i101 undef, 1
; CHECK-NEXT:    [[DD:%.*]] = or i101 [[D]], 1
; CHECK-NEXT:    [[G:%.*]] = getelementptr i101, ptr getelementptr inbounds nuw (i8, ptr @Y, i64 80), i101 [[DD]]
; CHECK-NEXT:    [[L3:%.*]] = load i101, ptr [[G]], align 4
; CHECK-NEXT:    ret i101 [[L3]]
;
  %D = and i101 undef, 1
  %DD = or i101 %D, 1
  %F = getelementptr [6 x i101], ptr @Y, i32 0, i32 5
  %G = getelementptr i101, ptr %F, i101 %DD
  %L3 = load i101, ptr %G
  ret i101 %L3
}

define void @index_too_large() {
; CHECK-LABEL: @index_too_large(
; CHECK-NEXT:    store ptr getelementptr (i8, ptr @Y, i64 18014398509481952), ptr undef, align 8
; CHECK-NEXT:    ret void
;
  %ptr1 = getelementptr [6 x i101], ptr @Y, i32 0, i32 -1
  %ptr2 = getelementptr i101, ptr %ptr1, i101 9224497936761618431
  store ptr %ptr2, ptr undef
  ret void
}

; OSS-Fuzz #39197
; https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=39197
@0 = external dso_local unnamed_addr constant [16 x i8]
define void @ossfuzz_39197() {
; CHECK-LABEL: @ossfuzz_39197(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    ret void
;
entry:
  %B5 = or i72 0, 2361183241434822606847
  %i = add nuw nsw i72 %B5, 0
  %i1 = lshr i72 %i, 1
  %i2 = getelementptr inbounds [4 x [4 x i8]], ptr @0, i72 0, i72 0, i72 %i1
  ret void
}
