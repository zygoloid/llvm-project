// RUN: %clang_cc1 -fsyntax-only -verify -std=c++20 %s

using size_t = __SIZE_TYPE__;

// floating-point arguments
template<float> struct Float {};
using F1 = Float<1.0f>; // FIXME expected-error {{sorry}}
using F1 = Float<2.0f / 2>; // FIXME expected-error {{sorry}}

struct S { int n[3]; } s; // expected-note 1+{{here}}
union U { int a, b; } u;
int n; // expected-note 1+{{here}}

// pointers to subobjects
template<int *> struct IntPtr {};
using IPn = IntPtr<&n + 1>; // FIXME expected-error {{refers to subobject}}
using IPn = IntPtr<&n + 1>; // FIXME expected-error {{refers to subobject}}

using IP2 = IntPtr<&s.n[2]>; // FIXME expected-error {{refers to subobject}}
using IP2 = IntPtr<s.n + 2>; // FIXME expected-error {{refers to subobject}}

using IP3 = IntPtr<&s.n[3]>; // FIXME expected-error {{refers to subobject}}
using IP3 = IntPtr<s.n + 3>; // FIXME expected-error {{refers to subobject}}

template<int &> struct IntRef {};
using IPn = IntRef<*(&n + 1)>; // expected-error {{not a constant expression}} expected-note {{dereferenced pointer past the end of 'n'}}
using IPn = IntRef<*(&n + 1)>; // expected-error {{not a constant expression}} expected-note {{dereferenced pointer past the end of 'n'}}

using IP2 = IntRef<s.n[2]>; // FIXME expected-error {{refers to subobject}}
using IP2 = IntRef<*(s.n + 2)>; // FIXME expected-error {{refers to subobject}}

using IP3 = IntRef<s.n[3]>; // expected-error {{not a constant expression}} expected-note {{dereferenced pointer past the end of subobject of 's'}}
using IP3 = IntRef<*(s.n + 3)>; // expected-error {{not a constant expression}} expected-note {{dereferenced pointer past the end of subobject of 's'}}

// classes
template<S> struct Struct {};
using S123 = Struct<S{1, 2, 3}>;
using S123 = Struct<S{1, 2, 3}>; // expected-note {{previous}}
using S123 = Struct<S{1, 2, 4}>; // expected-error {{different types}}
template<U> struct Union {};
using U1 = Union<U{1}>;
using U1 = Union<U{.a = 1}>; // expected-note {{previous}}
using U1 = Union<U{.b = 1}>; // expected-error {{different types}}

// miscellaneous scalar types
template<_Complex int> struct ComplexInt {};
using CI = ComplexInt<1 + 3i>; // FIXME: expected-error {{sorry}}
using CI = ComplexInt<1 + 3i>; // FIXME: expected-error {{sorry}}

template<_Complex float> struct ComplexFloat {};
using CF = ComplexFloat<1.0f + 3.0fi>; // FIXME: expected-error {{sorry}}
using CF = ComplexFloat<1.0f + 3.0fi>; // FIXME: expected-error {{sorry}}

namespace ClassNTTP {
  struct A {
    int x, y;
  };
  template<A a> constexpr int f() { return a.y; }
  static_assert(f<A{1,2}>() == 2);

  template<A a> int id;
  constexpr A a = {1, 2};
  static_assert(&id<A{1,2}> == &id<a>);
  static_assert(&id<A{1,3}> != &id<a>);
}

namespace ConvertedConstant {
  struct A {
    constexpr A(float) {}
  };
  template <A> struct X {};
  void f(X<1.0f>) {} // OK, user-defined conversion
  void f(X<2>) {} // expected-error {{conversion from 'int' to 'ConvertedConstant::A' is not allowed in a converted constant expression}}
}

namespace CopyCounting {
  // Make sure we don't use the copy constructor when transferring the "same"
  // template parameter object around.
  struct A { int n; constexpr A(int n = 0) : n(n) {} constexpr A(const A &a) : n(a.n+1) {} };
  template<A a> struct X {};
  template<A a> constexpr int f(X<a> x) { return a.n; }

  static_assert(f(X<A{}>()) == 0);

  template<A a> struct Y { void f(); };
  template<A a> void g(Y<a> y) { y.Y<a>::f(); }
  void h() { constexpr A a; g<a>(Y<a>{}); }

  template<A a> struct Z {
    constexpr int f() {
      constexpr A v = a; // this is {a.n+1}
      return Z<v>().f() + 1; // this is Z<{a.n+2}>
    }
  };
  template<> struct Z<A{20}> {
    constexpr int f() {
      return 32;
    }
  };
  static_assert(Z<A{}>().f() == 42);
}

namespace StableAddress {
  template<size_t N> struct str {
    char arr[N];
  };
  // FIXME: Deduction guide not needed with P1816R0.
  template<size_t N> str(const char (&)[N]) -> str<N>;

  // FIXME: Explicit size not needed.
  template<str<15> s> constexpr int sum() {
    int n = 0;
    for (char c : s.arr)
      n += c;
    return n;
  }
  static_assert(sum<str{"$hello $world."}>() == 1234);
}
