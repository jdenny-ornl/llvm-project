; RUN: opt -pass-remarks=kernel-info -passes=kernel-info -disable-output %s |& \
; RUN:   FileCheck -match-full-lines %s

; $ cat test.c
; #pragma omp declare target
; void f();
; void g() {
;   int i;
;   int a[2];
;   f();
;   g();
; }
; #pragma omp end declare target
;
; void h(int i) {
;   #pragma omp target map(tofrom:i)
;   {
;     int i;
;     int a[2];
;     f();
;     g();
;   }
; }
;
; $ clang -g -fopenmp -fopenmp-targets=nvptx64-nvidia-cuda -save-temps test.c
; $ llvm-dis test-openmp-nvptx64-nvidia-cuda.bc
;
; Copied test-openmp-nvptx64-nvidia-cuda.ll here but trimmed code not relevant
; for testing.

;  CHECK-NOT: {{.}}

;      CHECK: remark: test.c:0:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', artificial alloca 'dyn_ptr' with static size of 8 bytes
; CHECK-NEXT: remark: test.c:14:9: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', alloca 'i' with static size of 4 bytes
; CHECK-NEXT: remark: test.c:15:9: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', alloca 'a' with static size of 8 bytes
; CHECK-NEXT: remark: test.c:13:3: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', direct call to defined function, callee is '__kmpc_target_init'
; CHECK-NEXT: remark: test.c:16:5: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', direct call, callee is 'f'
; CHECK-NEXT: remark: test.c:17:5: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', direct call to defined function, callee is 'g'
; CHECK-NEXT: remark: test.c:18:3: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', direct call to defined function, callee is '__kmpc_target_deinit'
; CHECK-NEXT: remark: test.c:13:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', AllocaCount = 3
; CHECK-NEXT: remark: test.c:13:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', AllocaStaticSizeSum = 20
; CHECK-NEXT: remark: test.c:13:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', AllocaDynCount = 0
; CHECK-NEXT: remark: test.c:13:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', DirectCallCount = 4
; CHECK-NEXT: remark: test.c:13:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', IndirectCallCount = 0
; CHECK-NEXT: remark: test.c:13:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12_debug__', DirectCallsToDefinedFunctions = 3

; CHECK-NEXT: remark: test.c:0:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', artificial alloca 'dyn_ptr' with static size of 8 bytes
; CHECK-NEXT: remark: test.c:12:1: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', direct call to defined function, callee is artificial '__omp_offloading_10305_9a7e3f_h_l12_debug__'
; CHECK-NEXT: remark: test.c:12:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', AllocaCount = 1
; CHECK-NEXT: remark: test.c:12:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', AllocaStaticSizeSum = 8
; CHECK-NEXT: remark: test.c:12:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', AllocaDynCount = 0
; CHECK-NEXT: remark: test.c:12:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', DirectCallCount = 1
; CHECK-NEXT: remark: test.c:12:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', IndirectCallCount = 0
; CHECK-NEXT: remark: test.c:12:0: in artificial kernel function '__omp_offloading_10305_9a7e3f_h_l12', DirectCallsToDefinedFunctions = 1

; CHECK-NEXT: remark: test.c:4:7: in kernel function 'g', alloca 'i' with static size of 4 bytes
; CHECK-NEXT: remark: test.c:5:7: in kernel function 'g', alloca 'a' with static size of 8 bytes
; CHECK-NEXT: remark: test.c:6:3: in kernel function 'g', direct call, callee is 'f'
; CHECK-NEXT: remark: test.c:7:3: in kernel function 'g', direct call to defined function, callee is 'g'
; CHECK-NEXT: remark: test.c:3:0: in kernel function 'g', AllocaCount = 2
; CHECK-NEXT: remark: test.c:3:0: in kernel function 'g', AllocaStaticSizeSum = 12
; CHECK-NEXT: remark: test.c:3:0: in kernel function 'g', AllocaDynCount = 0
; CHECK-NEXT: remark: test.c:3:0: in kernel function 'g', DirectCallCount = 2
; CHECK-NEXT: remark: test.c:3:0: in kernel function 'g', IndirectCallCount = 0
; CHECK-NEXT: remark: test.c:3:0: in kernel function 'g', DirectCallsToDefinedFunctions = 1

;  CHECK-NOT: {{.}}

; ModuleID = 'test-openmp-nvptx64-nvidia-cuda.bc'
source_filename = "test.c"
target datalayout = "e-i64:64-i128:128-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

%struct.ident_t = type { i32, i32, i32, i32, ptr }
%struct.DynamicEnvironmentTy = type { i16 }
%struct.KernelEnvironmentTy = type { %struct.ConfigurationEnvironmentTy, ptr, ptr }
%struct.ConfigurationEnvironmentTy = type { i8, i8, i8, i32, i32, i32, i32, i32, i32 }

@0 = private unnamed_addr constant [59 x i8] c";test.c;__omp_offloading_10305_9a7e3f_h_l12_debug__;13;3;;\00", align 1
@1 = private unnamed_addr constant %struct.ident_t { i32 0, i32 2, i32 0, i32 61, ptr @0 }, align 8
@__omp_offloading_10305_9a7e3f_h_l12_dynamic_environment = weak_odr protected global %struct.DynamicEnvironmentTy zeroinitializer
@__omp_offloading_10305_9a7e3f_h_l12_kernel_environment = weak_odr protected constant %struct.KernelEnvironmentTy { %struct.ConfigurationEnvironmentTy { i8 1, i8 1, i8 1, i32 1, i32 128, i32 -1, i32 -1, i32 0, i32 0 }, ptr @1, ptr @__omp_offloading_10305_9a7e3f_h_l12_dynamic_environment }

; Function Attrs: convergent noinline norecurse nounwind optnone
define internal void @__omp_offloading_10305_9a7e3f_h_l12_debug__(ptr noalias noundef %dyn_ptr) #0 !dbg !17 {
entry:
  %dyn_ptr.addr = alloca ptr, align 8
  %i = alloca i32, align 4
  %a = alloca [2 x i32], align 4
  store ptr %dyn_ptr, ptr %dyn_ptr.addr, align 8
  tail call void @llvm.dbg.declare(metadata ptr %dyn_ptr.addr, metadata !25, metadata !DIExpression()), !dbg !26
  %0 = call i32 @__kmpc_target_init(ptr @__omp_offloading_10305_9a7e3f_h_l12_kernel_environment, ptr %dyn_ptr), !dbg !27
  %exec_user_code = icmp eq i32 %0, -1, !dbg !27
  br i1 %exec_user_code, label %user_code.entry, label %worker.exit, !dbg !27

user_code.entry:                                  ; preds = %entry
  tail call void @llvm.dbg.declare(metadata ptr %i, metadata !28, metadata !DIExpression()), !dbg !31
  tail call void @llvm.dbg.declare(metadata ptr %a, metadata !32, metadata !DIExpression()), !dbg !36
  call void @f() #16, !dbg !37
  call void @g() #16, !dbg !38
  call void @__kmpc_target_deinit(), !dbg !39
  ret void, !dbg !40

worker.exit:                                      ; preds = %entry
  ret void, !dbg !27
}

; Function Attrs: convergent
declare void @f(...) #1

; Function Attrs: convergent mustprogress noinline norecurse nounwind optnone
define weak_odr protected void @__omp_offloading_10305_9a7e3f_h_l12(ptr noalias noundef %dyn_ptr) #2 !dbg !41 {
entry:
  %dyn_ptr.addr = alloca ptr, align 8
  store ptr %dyn_ptr, ptr %dyn_ptr.addr, align 8
  tail call void @llvm.dbg.declare(metadata ptr %dyn_ptr.addr, metadata !42, metadata !DIExpression()), !dbg !43
  %0 = load ptr, ptr %dyn_ptr.addr, align 8, !dbg !44
  call void @__omp_offloading_10305_9a7e3f_h_l12_debug__(ptr %0) #17, !dbg !44
  ret void, !dbg !44
}

; Function Attrs: convergent noinline nounwind optnone
define hidden void @g() #3 !dbg !45 {
entry:
  %i = alloca i32, align 4
  %a = alloca [2 x i32], align 4
  tail call void @llvm.dbg.declare(metadata ptr %i, metadata !48, metadata !DIExpression()), !dbg !49
  tail call void @llvm.dbg.declare(metadata ptr %a, metadata !50, metadata !DIExpression()), !dbg !51
  call void @f() #16, !dbg !52
  call void @g() #16, !dbg !53
  ret void, !dbg !54
}

; Function Attrs: convergent mustprogress nounwind
define internal noundef i32 @__kmpc_target_init(ptr nofree noundef nonnull align 8 dereferenceable(48) %KernelEnvironment, ptr nofree noundef nonnull align 8 dereferenceable(16) %KernelLaunchEnvironment) #4 {
  ; The alloca and call instructions here should not be reported because this
  ; function is not associated with source code.
entry:
  %WorkFn.i = alloca ptr, align 8
  %ExecMode = getelementptr inbounds i8, ptr %KernelEnvironment, i64 2
  %0 = load i8, ptr %ExecMode, align 2, !tbaa !55
  %1 = and i8 %0, 2
  %tobool.not = icmp eq i8 %1, 0
  %2 = load i8, ptr %KernelEnvironment, align 8, !tbaa !61
  %tobool3.not = icmp ne i8 %2, 0
  br i1 %tobool.not, label %if.else, label %if.then

  ; Code not relevant to the test has been trimmed below.

if.then:                                          ; preds = %entry
  %3 = tail call i32 @llvm.nvvm.read.ptx.sreg.tid.x() #18
  br label %end

if.else:                                          ; preds = %entry
  %6 = tail call i32 @llvm.nvvm.read.ptx.sreg.ntid.x() #18
  br label %end

end:                                          ; preds = %if.end12, %_ZL19genericStateMachineP7IdentTy.exit, %if.end9, %_ZN4ompx5state18assumeInitialStateEb.exit
  ret i32 0
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare noundef i32 @llvm.nvvm.read.ptx.sreg.tid.x() #5

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare noundef i32 @llvm.nvvm.read.ptx.sreg.ntid.x() #5

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write)
declare void @llvm.assume(i1 noundef) #8

; Function Attrs: convergent mustprogress nounwind
define internal void @__kmpc_target_deinit() #4 {
  ; Body trimmed as not relevant to the test.
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.declare(metadata, metadata, metadata) #5

attributes #0 = { convergent noinline norecurse nounwind optnone "frame-pointer"="all" "no-trapping-math"="true" "omp_target_thread_limit"="128" "stack-protector-buffer-size"="8" "target-cpu"="sm_61" "target-features"="+ptx78,+sm_61" }
attributes #1 = { convergent "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_61" "target-features"="+ptx78,+sm_61" }
attributes #2 = { convergent mustprogress noinline norecurse nounwind optnone "frame-pointer"="all" "kernel" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_61" "target-features"="+ptx78,+sm_61" }
attributes #3 = { convergent noinline nounwind optnone "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_61" "target-features"="+ptx78,+sm_61" }
attributes #4 = { convergent mustprogress nounwind "frame-pointer"="all" "llvm.assume"="ompx_no_call_asm" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="sm_61" "target-features"="+ptx63,+ptx78,+sm_61" }
attributes #5 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #8 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write) }
attributes #16 = { convergent }
attributes #17 = { nounwind }
attributes #18 = { "llvm.assume"="ompx_no_call_asm" }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6, !7, !8, !9}
!llvm.dbg.cu = !{!10}
!nvvm.annotations = !{!12, !13}
!omp_offload.info = !{!14}
!llvm.ident = !{!15, !16, !15, !15, !15, !15, !15, !15, !15, !15, !15, !15, !15, !15, !15, !15}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 11, i32 8]}
!1 = !{i32 7, !"Dwarf Version", i32 2}
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !{i32 1, !"wchar_size", i32 4}
!4 = !{i32 7, !"openmp", i32 51}
!5 = !{i32 7, !"openmp-device", i32 51}
!6 = !{i32 8, !"PIC Level", i32 2}
!7 = !{i32 7, !"frame-pointer", i32 2}
!8 = !{i32 1, !"ThinLTO", i32 0}
!9 = !{i32 1, !"EnableSplitLTOUnit", i32 1}
!10 = distinct !DICompileUnit(language: DW_LANG_C11, file: !11, producer: "clang version 19.0.0git", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!11 = !DIFile(filename: "test.c", directory: "/home/jdenny")
!12 = !{ptr @__omp_offloading_10305_9a7e3f_h_l12_debug__, !"maxntidx", i32 128}
!13 = !{ptr @__omp_offloading_10305_9a7e3f_h_l12, !"kernel", i32 1}
!14 = !{i32 0, i32 66309, i32 10124863, !"h", i32 12, i32 0, i32 0}
!15 = !{!"clang version 19.0.0git"}
!16 = !{!"clang version 3.8.0 (tags/RELEASE_380/final)"}
!17 = distinct !DISubprogram(name: "__omp_offloading_10305_9a7e3f_h_l12_debug__", scope: !18, file: !18, line: 13, type: !19, scopeLine: 13, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !10, retainedNodes: !24)
!18 = !DIFile(filename: "test.c", directory: "/home/jdenny")
!19 = !DISubroutineType(types: !20)
!20 = !{null, !21}
!21 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !22)
!22 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !23)
!23 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!24 = !{}
!25 = !DILocalVariable(name: "dyn_ptr", arg: 1, scope: !17, type: !21, flags: DIFlagArtificial)
!26 = !DILocation(line: 0, scope: !17)
!27 = !DILocation(line: 13, column: 3, scope: !17)
!28 = !DILocalVariable(name: "i", scope: !29, file: !18, line: 14, type: !30)
!29 = distinct !DILexicalBlock(scope: !17, file: !18, line: 13, column: 3)
!30 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!31 = !DILocation(line: 14, column: 9, scope: !29)
!32 = !DILocalVariable(name: "a", scope: !29, file: !18, line: 15, type: !33)
!33 = !DICompositeType(tag: DW_TAG_array_type, baseType: !30, size: 64, elements: !34)
!34 = !{!35}
!35 = !DISubrange(count: 2)
!36 = !DILocation(line: 15, column: 9, scope: !29)
!37 = !DILocation(line: 16, column: 5, scope: !29)
!38 = !DILocation(line: 17, column: 5, scope: !29)
!39 = !DILocation(line: 18, column: 3, scope: !29)
!40 = !DILocation(line: 18, column: 3, scope: !17)
!41 = distinct !DISubprogram(name: "__omp_offloading_10305_9a7e3f_h_l12", scope: !18, file: !18, line: 12, type: !19, scopeLine: 12, flags: DIFlagArtificial | DIFlagPrototyped, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition, unit: !10, retainedNodes: !24)
!42 = !DILocalVariable(name: "dyn_ptr", arg: 1, scope: !41, type: !21, flags: DIFlagArtificial)
!43 = !DILocation(line: 0, scope: !41)
!44 = !DILocation(line: 12, column: 1, scope: !41)
!45 = distinct !DISubprogram(name: "g", scope: !18, file: !18, line: 3, type: !46, scopeLine: 3, spFlags: DISPFlagDefinition, unit: !10, retainedNodes: !24)
!46 = !DISubroutineType(types: !47)
!47 = !{null}
!48 = !DILocalVariable(name: "i", scope: !45, file: !18, line: 4, type: !30)
!49 = !DILocation(line: 4, column: 7, scope: !45)
!50 = !DILocalVariable(name: "a", scope: !45, file: !18, line: 5, type: !33)
!51 = !DILocation(line: 5, column: 7, scope: !45)
!52 = !DILocation(line: 6, column: 3, scope: !45)
!53 = !DILocation(line: 7, column: 3, scope: !45)
!54 = !DILocation(line: 8, column: 1, scope: !45)
!55 = !{!56, !59, i64 2}
!56 = !{!"_ZTS26ConfigurationEnvironmentTy", !57, i64 0, !57, i64 1, !59, i64 2, !60, i64 4, !60, i64 8, !60, i64 12, !60, i64 16, !60, i64 20, !60, i64 24}
!57 = !{!"omnipotent char", !58, i64 0}
!58 = !{!"Simple C++ TBAA"}
!59 = !{!"_ZTSN4llvm3omp19OMPTgtExecModeFlagsE", !57, i64 0}
!60 = !{!"int", !57, i64 0}
!61 = !{!56, !57, i64 0}
