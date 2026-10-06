// REQUIRES: edg-enabled
// RUN: %clang -fedg -std=c++20 -S -emit-llvm -O2 -o - %s | FileCheck %s
// RUN: %clang -fedg -std=c++20 -fsyntax-only %s
// RUN: not %clang -fedg -std=c++20 -fsyntax-only -DERROR_CASE %s 2>&1 | FileCheck %s --check-prefix=ERR

#ifndef __EDG__
#error "Expected __EDG__ to be defined when compiling with -fedg"
#endif

template <typename T>
constexpr T square(T x) {
  return x * x;
}

static_assert(square(7) == 49);

extern "C" int compute_edg_value(int x) {
  auto add_const = [y = square(3)](int v) { return v + y; };
  return add_const(x);
}

// CHECK-LABEL: define {{.*}}i32 @compute_edg_value(i32 {{.*}})
// CHECK: add nsw i32 {{.*}}, 9
// CHECK: ret i32

#ifdef ERROR_CASE
int bad_type = "not an int";
// ERR: error: a value of type "const char *" cannot be used
#endif
