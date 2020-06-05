; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -print-schedule -mcpu=x86-64 -mattr=+rdpid | FileCheck %s --check-prefix=CHECK --check-prefix=GENERIC
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -print-schedule -mcpu=icelake-client | FileCheck %s --check-prefix=CHECK --check-prefix=ICELAKE
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -print-schedule -mcpu=icelake-server | FileCheck %s --check-prefix=CHECK --check-prefix=ICELAKE

define i32 @test_rdpid() {
; GENERIC-LABEL: test_rdpid:
; GENERIC:       # %bb.0:
; GENERIC-NEXT:    rdpid %rax # sched: [100:0.33]
; GENERIC-NEXT:    # kill: def $eax killed $eax killed $rax
; GENERIC-NEXT:    retq # sched: [1:1.00]
;
; ICELAKE-LABEL: test_rdpid:
; ICELAKE:       # %bb.0:
; ICELAKE-NEXT:    rdpid %rax # sched: [100:0.25]
; ICELAKE-NEXT:    # kill: def $eax killed $eax killed $rax
; ICELAKE-NEXT:    retq # sched: [7:1.00]
  %1 = tail call i32 @llvm.x86.rdpid()
  ret i32 %1
}
declare i32 @llvm.x86.rdpid()