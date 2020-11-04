// RUN: not llvm-mc -triple=aarch64 -show-encoding -mattr=+sve  2>&1 < %s| FileCheck %s

brkpb  p15.b, p15/m, p15.b, p15.b
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid operand for instruction
// CHECK-NEXT: brkpb  p15.b, p15/m, p15.b, p15.b
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}:

brkpb  p15.s, p15/z, p15.s, p15.s
// CHECK: [[@LINE-1]]:{{[0-9]+}}: error: invalid predicate register
// CHECK-NEXT: brkpb  p15.s, p15/z, p15.s, p15.s
// CHECK-NOT: [[@LINE-1]]:{{[0-9]+}}: