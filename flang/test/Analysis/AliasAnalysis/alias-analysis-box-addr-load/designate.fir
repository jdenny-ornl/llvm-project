// Check aliasing with the address passed via a pointer dummy argument.

// Use --mlir-disable-threading so that the AA queries are serialized
// as well as its diagnostic output.
// RUN: fir-opt %s \
// RUN:   -pass-pipeline='builtin.module(func.func(test-fir-alias-analysis))' \
// RUN:   --mlir-disable-threading 2>&1 | FileCheck %s

// module m
//   type :: ty
//     real, pointer :: p0, p1
//     real :: arr(2)
//     real, allocatable :: alloc
//     ! target attribute on components is not supported
//   end type ty
// end module m
// subroutine test()
//   use m
//   real, target :: t
//   real :: v
//   type(ty) :: obj
//   type(ty), target :: t_obj
//   v = obj%p0
//   v = obj%p1
//   v = obj%arr(1)
//   v = obj%alloc
//   v = t_obj%arr(1)
//   v = t_obj%alloc
// end subroutine test

// CHECK-LABEL: Testing : "_QPtest"

// The address in a pointer can alias the address in another pointer or the
// address of a target but not the address of other variables.
// CHECK-DAG: t.addr#0 <-> obj%p0.tgt_addr#0: MayAlias
// CHECK-DAG: t.addr#0 <-> obj%p1.tgt_addr#0: MayAlias
// CHECK-DAG: v.addr#0 <-> obj%p0.tgt_addr#0: NoAlias
// CHECK-DAG: v.addr#0 <-> obj%p1.tgt_addr#0: NoAlias
// CHECK-DAG: obj%p0.tgt_addr#0 <-> obj%p1.tgt_addr#0: MayAlias

// Determining whether the address *in* a pointer can alias the address *of* a
// pointer is not yet handled.  For now, make sure it's not MustAlias.
// CHECK-DAG: obj%p0.addr#0 <-> obj%p0.tgt_addr#0: MayAlias
// CHECK-DAG: obj%p0.addr#0 <-> obj%p1.tgt_addr#0: MayAlias
// CHECK-DAG: obj%p0.tgt_addr#0 <-> obj%p1.addr#0: MayAlias
// CHECK-DAG: obj%p1.addr#0 <-> obj%p1.tgt_addr#0: MayAlias

// For some cases, AliasAnalysis analyzes hlfir.designate like fir.box_addr, so
// make sure it doesn't mistakenly see obj%arr(1).addr as an address that was
// loaded from a pointer and that could alias something.  However, t_obj%arr is
// a target.
// FIXME: Thus, we expect the first case below to be NoAlias.  However, there's
// currently logic for components that decides the address in any pointer can
// alias any object of derived type containing a pointer.  That logic seems
// wrong.
// CHECK-DAG: obj%p0.tgt_addr#0 <-> obj%arr(1).addr#0: MayAlias
// CHECK-DAG: obj%p0.tgt_addr#0 <-> t_obj%arr(1).addr#0: MayAlias

// Like a pointer, an allocatable contains an address, but an allocatable is not
// a pointer and so cannot alias pointers.  However, t_obj%alloc is a target.
// FIXME: Thus, we expect the first case below to be NoAlias.  However, there's
// currently logic for components that decides the address in any pointer can
// alias any object of derived type containing a pointer.  That logic seems
// wrong.
// CHECK-DAG: obj%p0.tgt_addr#0 <-> obj%alloc.tgt_addr#0: MayAlias
// CHECK-DAG: obj%p0.tgt_addr#0 <-> t_obj%alloc.tgt_addr#0: MayAlias

func.func @_QPtest() {
  %0 = fir.alloca !fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}> {bindc_name = "obj", uniq_name = "_QFtestEobj"}
  %1:2 = hlfir.declare %0 {uniq_name = "_QFtestEobj"} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>, !fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>)
  %2 = fir.embox %1#1 : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>
  %3 = fir.address_of(@_QQclX07f50799ad48185df619042785bdb39b) : !fir.ref<!fir.char<1,45>>
  %c9_i32 = arith.constant 9 : i32
  %4 = fir.convert %2 : (!fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<none>
  %5 = fir.convert %3 : (!fir.ref<!fir.char<1,45>>) -> !fir.ref<i8>
  %6 = fir.call @_FortranAInitialize(%4, %5, %c9_i32) fastmath<contract> : (!fir.box<none>, !fir.ref<i8>, i32) -> none
  %7 = fir.alloca f32 {bindc_name = "t", fir.target, uniq_name = "_QFtestEt"}
  %8:2 = hlfir.declare %7 {fortran_attrs = #fir.var_attrs<target>, uniq_name = "_QFtestEt", test.ptr="t.addr"} : (!fir.ref<f32>) -> (!fir.ref<f32>, !fir.ref<f32>)
  %9 = fir.alloca !fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}> {bindc_name = "t_obj", fir.target, uniq_name = "_QFtestEt_obj"}
  %10:2 = hlfir.declare %9 {fortran_attrs = #fir.var_attrs<target>, uniq_name = "_QFtestEt_obj"} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>, !fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>)
  %11 = fir.embox %10#1 : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>
  %12 = fir.address_of(@_QQclX07f50799ad48185df619042785bdb39b) : !fir.ref<!fir.char<1,45>>
  %c9_i32_0 = arith.constant 9 : i32
  %13 = fir.convert %11 : (!fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<none>
  %14 = fir.convert %12 : (!fir.ref<!fir.char<1,45>>) -> !fir.ref<i8>
  %15 = fir.call @_FortranAInitialize(%13, %14, %c9_i32_0) fastmath<contract> : (!fir.box<none>, !fir.ref<i8>, i32) -> none
  %16 = fir.alloca f32 {bindc_name = "v", uniq_name = "_QFtestEv"}
  %17:2 = hlfir.declare %16 {uniq_name = "_QFtestEv", test.ptr="v.addr"} : (!fir.ref<f32>) -> (!fir.ref<f32>, !fir.ref<f32>)
  %18 = hlfir.designate %1#0{"p0"}   {fortran_attrs = #fir.var_attrs<pointer>, test.ptr="obj%p0.addr"} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.ref<!fir.box<!fir.ptr<f32>>>
  %19 = fir.load %18 : !fir.ref<!fir.box<!fir.ptr<f32>>>
  %20 = fir.box_addr %19 {test.ptr="obj%p0.tgt_addr"} : (!fir.box<!fir.ptr<f32>>) -> !fir.ptr<f32>
  %21 = fir.load %20 : !fir.ptr<f32>
  hlfir.assign %21 to %17#0 : f32, !fir.ref<f32>
  %22 = hlfir.designate %1#0{"p1"}   {fortran_attrs = #fir.var_attrs<pointer>, test.ptr="obj%p1.addr"} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.ref<!fir.box<!fir.ptr<f32>>>
  %23 = fir.load %22 : !fir.ref<!fir.box<!fir.ptr<f32>>>
  %24 = fir.box_addr %23 {test.ptr="obj%p1.tgt_addr"}: (!fir.box<!fir.ptr<f32>>) -> !fir.ptr<f32>
  %25 = fir.load %24 : !fir.ptr<f32>
  hlfir.assign %25 to %17#0 : f32, !fir.ref<f32>
  %c2 = arith.constant 2 : index
  %26 = fir.shape %c2 : (index) -> !fir.shape<1>
  %c1 = arith.constant 1 : index
  %27 = hlfir.designate %1#0{"arr"} <%26> (%c1) {test.ptr="obj%arr(1).addr"} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>, !fir.shape<1>, index) -> !fir.ref<f32>
  %28 = fir.load %27 : !fir.ref<f32>
  hlfir.assign %28 to %17#0 : f32, !fir.ref<f32>
  %29 = hlfir.designate %1#0{"alloc"}   {fortran_attrs = #fir.var_attrs<allocatable>} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.ref<!fir.box<!fir.heap<f32>>>
  %30 = fir.load %29 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %31 = fir.box_addr %30 {test.ptr="obj%alloc.tgt_addr"}: (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
  %32 = fir.load %31 : !fir.heap<f32>
  hlfir.assign %32 to %17#0 : f32, !fir.ref<f32>
  %c2_1 = arith.constant 2 : index
  %33 = fir.shape %c2_1 : (index) -> !fir.shape<1>
  %c1_2 = arith.constant 1 : index
  %34 = hlfir.designate %10#0{"arr"} <%33> (%c1_2) {test.ptr="t_obj%arr(1).addr"} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>, !fir.shape<1>, index) -> !fir.ref<f32>
  %35 = fir.load %34 : !fir.ref<f32>
  hlfir.assign %35 to %17#0 : f32, !fir.ref<f32>
  %36 = hlfir.designate %10#0{"alloc"}   {fortran_attrs = #fir.var_attrs<allocatable>} : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.ref<!fir.box<!fir.heap<f32>>>
  %37 = fir.load %36 : !fir.ref<!fir.box<!fir.heap<f32>>>
  %38 = fir.box_addr %37 {test.ptr="t_obj%alloc.tgt_addr"} : (!fir.box<!fir.heap<f32>>) -> !fir.heap<f32>
  %39 = fir.load %38 : !fir.heap<f32>
  hlfir.assign %39 to %17#0 : f32, !fir.ref<f32>
  %40 = fir.embox %10#1 : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>
  %41 = fir.convert %40 : (!fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<none>
  %42 = fir.call @_FortranADestroy(%41) fastmath<contract> : (!fir.box<none>) -> none
  %43 = fir.embox %1#1 : (!fir.ref<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>
  %44 = fir.convert %43 : (!fir.box<!fir.type<_QMmTty{p0:!fir.box<!fir.ptr<f32>>,p1:!fir.box<!fir.ptr<f32>>,arr:!fir.array<2xf32>,alloc:!fir.box<!fir.heap<f32>>}>>) -> !fir.box<none>
  %45 = fir.call @_FortranADestroy(%44) fastmath<contract> : (!fir.box<none>) -> none
  return
}
