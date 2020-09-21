// RUN: %clang_cc1 -std=c++20 %s -triple x86_64-linux-gnu -emit-llvm -o - | FileCheck %s

#define fold(x) (__builtin_constant_p(x) ? (x) : (x))

struct A { int a, b; };
template<A> void f() {}

// CHECK: define weak_odr void @_Z1fIXtl1ALi1ELi2EEEEvv(
template void f<A{1, 2}>();

struct B { int *p; };
template<B> void f() {}

int n = 0;
// CHECK: define weak_odr void @_Z1fIXtl1BadL_Z1nEEEEvv(
template void f<B{&n}>();
// CHECK: define weak_odr void @_Z1fIXtl1BLPi0EEEEvv(
template void f<B{nullptr}>();
// CHECK: define weak_odr void @_Z1fIXtl1BLPi32EEEEvv(
template void f<B{fold((int*)32)}>();
// CHECK: define weak_odr void @_Z1fIXtl1BrcPiLi0EEEEEvv(
template void f<B{fold(reinterpret_cast<int*>(0))}>();

// Pointers to subobjects.
struct Nested { union { int k; int arr[2]; }; } nested[2];
struct Derived : A, Nested {} derived;
// CHECK: define weak_odr void @_Z1fIXtl1BadsoiL_Z6nestedE_EEEEvv
template void f<B{&nested[0].k}>();
// CHECK: define weak_odr void @_Z1fIXtl1BadsoiL_Z6nestedE16_0pEEEEvv
template void f<B{&nested[1].arr[2]}>();
// CHECK: define weak_odr void @_Z1fIXtl1BadsoiL_Z7derivedE8pEEEEvv
template void f<B{&derived.b + 1}>();
// CHECK: define weak_odr void @_Z1fIXtl1BcvPiplcvPcadL_Z7derivedELl16EEEEvv
template void f<B{fold(&derived.b + 3)}>();

// References to subobjects.
struct BR { int &r; };
template<BR> void f() {}
// CHECK: define weak_odr void @_Z1fIXtl2BRsoiL_Z6nestedE_EEEEvv
template void f<BR{nested[0].k}>();
// CHECK: define weak_odr void @_Z1fIXtl2BRsoiL_Z6nestedE12_0EEEEvv
template void f<BR{nested[1].arr[1]}>();
// CHECK: define weak_odr void @_Z1fIXtl2BRsoiL_Z7derivedE4EEEEvv
template void f<BR{derived.b}>();
// CHECK: define weak_odr void @_Z1fIXtl2BRdecvPiplcvPcadL_Z7derivedELl16EEEEvv
template void f<BR{fold(*(&derived.b + 3))}>();

// Qualification conversions.
struct C { const int *p; };
template<C> void f() {}
// CHECK: define weak_odr void @_Z1fIXtl1CadsoKiL_Z7derivedE4EEEEvv
template void f<C{&derived.b}>();

// Pointers to members.
struct D { const int Derived::*p; };
template<D> void f() {}
// CHECK: define weak_odr void @_Z1fIXtl1DLM7DerivedKi0EEEEvv
template void f<D{nullptr}>();
// CHECK: define weak_odr void @_Z1fIXtl1DmcM7DerivedKiadL_ZN1A1aEEEEEEvv
template void f<D{&A::a}>();
// CHECK: define weak_odr void @_Z1fIXtl1DmcM7DerivedKiadL_ZN1A1bEEEEEEvv
template void f<D{&A::b}>();
// FIXME: Is the Ut_1 mangling here correct?
// CHECK: define weak_odr void @_Z1fIXtl1DmcM7DerivedKiadL_ZN6NestedUt_1kEE8EEEEvv
template void f<D{&Nested::k}>();
struct MoreDerived : A, Derived { int z; };
// CHECK: define weak_odr void @_Z1fIXtl1DmcM7DerivedKiadL_ZN11MoreDerived1zEEn8EEEEvv
template void f<D{(int Derived::*)&MoreDerived::z}>();

union E {
  int n;
  float f;
  constexpr E() {}
  constexpr E(int n) : n(n) {}
  constexpr E(float f) : f(f) {}
};
template<E> void f() {}

// Union members.
// CHECK: define weak_odr void @_Z1fIXL1EEEEvv(
template void f<E{}>();
// CHECK: define weak_odr void @_Z1fIXtl1Edi1nLi0EEEEvv(
template void f<E(0)>();
// CHECK: define weak_odr void @_Z1fIXtl1Edi1fLf00000000EEEEvv(
template void f<E(0.f)>();

// Extensions.
typedef int __attribute__((ext_vector_type(3))) VI3;
struct F { VI3 v; _Complex int ci; _Complex float cf; };
template<F> void f() {}
// CHECK: define weak_odr void @_Z1fIXtl1FtlDv3_iLi1ELi2ELi3EEtlCiLi4ELi5EEtlCfLf40c00000ELf40e00000EEEEEvv
template void f<F{{1, 2, 3}, {4, 5}, {6, 7}}>();
