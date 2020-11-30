// RUN: %clang_cc1 -fsyntax-only -verify %s

namespace rdar8436162 {
  class ClsA {
  public:
    static void f();
    void g();
  };

  class ClsB : virtual private ClsA {
  public:
    using ClsA::f;
    using ClsA::g;
  };

  class ClsF : virtual private ClsA {
  public:
    using ClsA::f;
    using ClsA::g;
  };

  class ClsE : public ClsB, public ClsF {
    void test() {
      f();
      g();
    }
  };

  struct A {
    static void f();
    void g();
  };
  struct B : private A {
    using A::f;
    using A::g;
  };
  struct C : private A {
    using A::f;
    using A::g;
  };
  struct D : B, C {
    void test() {
      f();
      g(); // expected-error {{ambiguous conversion from derived class}}
    }
  };
}
