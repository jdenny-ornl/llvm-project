// Check aliasing with the address in a local pointer variable.

// Use --mlir-disable-threading so that the AA queries are serialized
// as well as its diagnostic output.
// RUN: fir-opt %s \
// RUN:   -pass-pipeline='builtin.module(func.func(test-fir-alias-analysis))' \
// RUN:   --mlir-disable-threading 2>&1 | FileCheck %s

// subroutine test(n)
//   integer :: n
//   real, pointer :: p0, p1
//   real :: arr(n)
//   real, target :: t_arr(n)
//   real, allocatable :: alloc
//   real, allocatable, target :: t_alloc
//   real, target :: t
//   real :: v
//   v = p0
//   v = p1
//   v = arr(1)
//   v = t_arr(1)
//   v = alloc
//   v = t_alloc
// end subroutine test

// CHECK-LABEL: Testing : "_QPtest"

// The address in a pointer can alias the address in another pointer or the
// address of a target but not the address of other variables.
// CHECK-DAG: t.addr#0 <-> p0.tgt_addr#0: MayAlias
// CHECK-DAG: t.addr#0 <-> p1.tgt_addr#0: MayAlias
// CHECK-DAG: v.addr#0 <-> p0.tgt_addr#0: NoAlias
// CHECK-DAG: v.addr#0 <-> p1.tgt_addr#0: NoAlias
// CHECK-DAG: p0.tgt_addr#0 <-> p1.tgt_addr#0: MayAlias

// Determining whether the address *in* a pointer can alias the address *of* a
// pointer is not yet handled.  For now, make sure it's not MustAlias.
// CHECK-DAG: p0.addr#0 <-> p0.tgt_addr#0: MayAlias
// CHECK-DAG: p0.addr#0 <-> p1.tgt_addr#0: MayAlias
// CHECK-DAG: p1.addr#0 <-> p0.tgt_addr#0: MayAlias
// CHECK-DAG: p1.addr#0 <-> p1.tgt_addr#0: MayAlias

// For some cases, AliasAnalysis analyzes hlfir.designate like fir.box_addr, so
// make sure it doesn't mistakenly see arr(1).addr as an address that was loaded
// from a pointer and that could alias something.  However, t_arr is a target.
// CHECK-DAG: p0.tgt_addr#0 <-> arr(1).addr#0: NoAlias
// CHECK-DAG: p0.tgt_addr#0 <-> t_arr(1).addr#0: MayAlias

// Like a pointer, an allocatable contains an address, but an allocatable is not
// a pointer and so cannot alias pointers.  However, t_alloc is a target.
// CHECK-DAG: p0.tgt_addr#0 <-> alloc.tgt_addr#0: NoAlias
// CHECK-DAG: p0.tgt_addr#0 <-> t_alloc.tgt_addr#0: MayAlias

func.func @_QPtest(%arg0: !fir.ref<i32> {fir.bindc_name = "n"}) {
  %0 = fir.alloca !fir.box<!fir.heap<f32>> {bindc_name = "alloc", uniq_name = "_QFtestEalloc"}
  %1 = fir.zero_bits !fir.heap<f32>
  %2 = fir.embox %1 : (!fir.heap<f32>) -> !fir.box<!fir.heap<f32>>
  fir.store %2 to %0 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %3:2 = hlfir.declare %0 {fortran_attrs = #fir.var_attrs<allocatable>, uniq_name = "_QFtestEalloc"} : (!fir.ref<!fir.box<!fir.heap<f32>>>) -> (!fir.ref<!fir.box<!fir.heap<f32>>>, !fir.ref<!fir.box<!fir.heap<f32>>>)
  %4:2 = hlfir.declare %arg0 {uniq_name = "_QFtestEn"} : (!fir.ref<i32>) -> (!fir.ref<i32>, !fir.ref<i32>)
  %5 = fir.alloca !fir.box<!fir.ptr<f32>> {bindc_name = "p0", uniq_name = "_QFtestEp0"}
  %6 = fir.zero_bits !fir.ptr<f32>
  %7 = fir.embox %6 : (!fir.ptr<f32>) -> !fir.box<!fir.ptr<f32>>
  fir.store %7 to %5 : !fir.ref<!fir.box<!fir.ptr<f32>>>
  %8:2 = hlfir.declare %5 {fortran_attrs = #fir.var_attrs<pointer>, uniq_name = "_QFtestEp0", test.ptr="p0.addr"} : (!fir.ref<!fir.box<!fir.ptr<f32>>>) -> (!fir.ref<!fir.box<!fir.ptr<f32>>>, !fir.ref<!fir.box<!fir.ptr<f32>>>)
  %9 = fir.alloca !fir.box<!fir.ptr<f32>> {bindc_name = "p1", uniq_name = "_QFtestEp1"}
  %10 = fir.zero_bits !fir.ptr<f32>
  %11 = fir.embox %10 : (!fir.ptr<f32>) -> !fir.box<!fir.ptr<f32>>
  fir.store %11 to %9 : !fir.ref<!fir.box<!fir.ptr<f32>>>
  %12:2 = hlfir.declare %9 {fortran_attrs = #fir.var_attrs<pointer>, uniq_name = "_QFtestEp1", test.ptr="p1.addr"} : (!fir.ref<!fir.box<!fir.ptr<f32>>>) -> (!fir.ref<!fir.box<!fir.ptr<f32>>>, !fir.ref<!fir.box<!fir.ptr<f32>>>)
  %13 = fir.alloca f32 {bindc_name = "t", fir.target, uniq_name = "_QFtestEt"}
  %14:2 = hlfir.declare %13 {fortran_attrs = #fir.var_attrs<target>, uniq_name = "_QFtestEt", test.ptr="t.addr"} : (!fir.ref<f32>) -> (!fir.ref<f32>, !fir.ref<f32>)
  %15 = fir.alloca !fir.box<!fir.heap<f32>> {bindc_name = "t_alloc", fir.target, uniq_name = "_QFtestEt_alloc"}
  %16 = fir.zero_bits !fir.heap<f32>
  %17 = fir.embox %16 : (!fir.heap<f32>) -> !fir.box<!fir.heap<f32>>
  fir.store %17 to %15 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %18:2 = hlfir.declare %15 {fortran_attrs = #fir.var_attrs<allocatable, target>, uniq_name = "_QFtestEt_alloc"} : (!fir.ref<!fir.box<!fir.heap<f32>>>) -> (!fir.ref<!fir.box<!fir.heap<f32>>>, !fir.ref<!fir.box<!fir.heap<f32>>>)
  %19 = fir.alloca f32 {bindc_name = "v", uniq_name = "_QFtestEv"}
  %20:2 = hlfir.declare %19 {uniq_name = "_QFtestEv", test.ptr="v.addr"} : (!fir.ref<f32>) -> (!fir.ref<f32>, !fir.ref<f32>)
  %21 = fir.load %4#0 : !fir.ref<i32>
  %22 = fir.convert %21 : (i32) -> i64
  %23 = fir.convert %22 : (i64) -> index
  %c0 = arith.constant 0 : index
  %24 = arith.cmpi sgt, %23, %c0 : index
  %25 = arith.select %24, %23, %c0 : index
  %26 = fir.alloca !fir.array<?xf32>, %25 {bindc_name = "arr", uniq_name = "_QFtestEarr"}
  %27 = fir.shape %25 : (index) -> !fir.shape<1>
  %28:2 = hlfir.declare %26(%27) {uniq_name = "_QFtestEarr"} : (!fir.ref<!fir.array<?xf32>>, !fir.shape<1>) -> (!fir.box<!fir.array<?xf32>>, !fir.ref<!fir.array<?xf32>>)
  %29 = fir.load %4#0 : !fir.ref<i32>
  %30 = fir.convert %29 : (i32) -> i64
  %31 = fir.convert %30 : (i64) -> index
  %c0_0 = arith.constant 0 : index
  %32 = arith.cmpi sgt, %31, %c0_0 : index
  %33 = arith.select %32, %31, %c0_0 : index
  %34 = fir.alloca !fir.array<?xf32>, %33 {bindc_name = "t_arr", fir.target, uniq_name = "_QFtestEt_arr"}
  %35 = fir.shape %33 : (index) -> !fir.shape<1>
  %36:2 = hlfir.declare %34(%35) {fortran_attrs = #fir.var_attrs<target>, uniq_name = "_QFtestEt_arr"} : (!fir.ref<!fir.array<?xf32>>, !fir.shape<1>) -> (!fir.box<!fir.array<?xf32>>, !fir.ref<!fir.array<?xf32>>)
  %37 = fir.load %8#0 : !fir.ref<!fir.box<!fir.ptr<f32>>>
  %38 = fir.box_addr %37 {test.ptr="p0.tgt_addr"} : (!fir.box<!fir.ptr<f32>>) -> !fir.ptr<f32>
  %39 = fir.load %38 : !fir.ptr<f32>
  hlfir.assign %39 to %20#0 : f32, !fir.ref<f32>
  %40 = fir.load %12#0 : !fir.ref<!fir.box<!fir.ptr<f32>>>
  %41 = fir.box_addr %40 {test.ptr="p1.tgt_addr"} : (!fir.box<!fir.ptr<f32>>) -> !fir.ptr<f32>
  %42 = fir.load %41 : !fir.ptr<f32>
  hlfir.assign %42 to %20#0 : f32, !fir.ref<f32>
  %c1 = arith.constant 1 : index
  %43 = hlfir.designate %28#0 (%c1) {test.ptr="arr(1).addr"} : (!fir.box<!fir.array<?xf32>>, index) -> !fir.ref<f32>
  %44 = fir.load %43 : !fir.ref<f32>
  hlfir.assign %44 to %20#0 : f32, !fir.ref<f32>
  %c1_1 = arith.constant 1 : index
  %45 = hlfir.designate %36#0 (%c1_1) {test.ptr="t_arr(1).addr"} : (!fir.box<!fir.array<?xf32>>, index) -> !fir.ref<f32>
  %46 = fir.load %45 : !fir.ref<f32>
  hlfir.assign %46 to %20#0 : f32, !fir.ref<f32>
  %47 = fir.load %3#0 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %48 = fir.box_addr %47 {test.ptr="alloc.tgt_addr"} : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
  %49 = fir.load %48 : !fir.heap<f32>
  hlfir.assign %49 to %20#0 : f32, !fir.ref<f32>
  %50 = fir.load %18#0 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %51 = fir.box_addr %50 {test.ptr="t_alloc.tgt_addr"} : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
  %52 = fir.load %51 : !fir.heap<f32>
  hlfir.assign %52 to %20#0 : f32, !fir.ref<f32>
  %53 = fir.load %18#1 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %54 = fir.box_addr %53 : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
  %55 = fir.convert %54 : (!fir.heap<f32>) -> i64
  %c0_i64 = arith.constant 0 : i64
  %56 = arith.cmpi ne, %55, %c0_i64 : i64
  fir.if %56 {
    %61 = fir.load %18#1 : !fir.ref<!fir.box<!fir.heap<f32>>>
    %62 = fir.box_addr %61 : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
    fir.freemem %62 : !fir.heap<f32>
    %63 = fir.zero_bits !fir.heap<f32>
    %64 = fir.embox %63 : (!fir.heap<f32>) -> !fir.box<!fir.heap<f32>>
    fir.store %64 to %18#1 : !fir.ref<!fir.box<!fir.heap<f32>>>
  }
  %57 = fir.load %3#1 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %58 = fir.box_addr %57 : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
  %59 = fir.convert %58 : (!fir.heap<f32>) -> i64
  %c0_i64_2 = arith.constant 0 : i64
  %60 = arith.cmpi ne, %59, %c0_i64_2 : i64
  fir.if %60 {
    %61 = fir.load %3#1 : !fir.ref<!fir.box<!fir.heap<f32>>>
    %62 = fir.box_addr %61 : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
    fir.freemem %62 : !fir.heap<f32>
    %63 = fir.zero_bits !fir.heap<f32>
    %64 = fir.embox %63 : (!fir.heap<f32>) -> !fir.box<!fir.heap<f32>>
    fir.store %64 to %3#1 : !fir.ref<!fir.box<!fir.heap<f32>>>
  }
  return
}
