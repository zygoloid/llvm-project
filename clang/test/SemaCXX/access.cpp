// RUN: %clang_cc1 -std=c++11 -fsyntax-only -verify %s

class C {
    struct S; // expected-note {{previously declared 'private' here}}
public:
    
    struct S {}; // expected-error {{'S' redeclared with 'public' access}}
};

struct S {
    class C; // expected-note {{previously declared 'public' here}}
    
private:
    class C { }; // expected-error {{'C' redeclared with 'private' access}}
};

class T {
protected:
    template<typename T> struct A; // expected-note {{previously declared 'protected' here}}
    
private:
    template<typename T> struct A {}; // expected-error {{'A' redeclared with 'private' access}}
};

// PR5573
namespace test1 {
  class A {
  private:
    class X; // expected-note {{previously declared 'private' here}} \
             // expected-note {{previous declaration is here}}
  public:
    class X; // expected-error {{'X' redeclared with 'public' access}} \
             // expected-warning {{class member cannot be redeclared}}
    class X {};
  };
}

// PR15209
namespace PR15209 {
  namespace alias_templates {
    template<typename T1, typename T2> struct U { };
    template<typename T1> using W = U<T1, float>;

    class A {
      typedef int I;
      static constexpr I x = 0; // expected-note {{implicitly declared private here}}
      static constexpr I y = 42; // expected-note {{implicitly declared private here}}
      friend W<int>;
    };

    template<typename T1>
    struct U<T1, float>  {
      int v_;
      // the following will trigger for U<float, float> instantiation, via W<float>
      U() : v_(A::x) { } // expected-error {{'x' is a private member of 'PR15209::alias_templates::A'}}
    };

    template<typename T1>
    struct U<T1, int> {
      int v_;
      U() : v_(A::y) { } // expected-error {{'y' is a private member of 'PR15209::alias_templates::A'}}
    };

    template struct U<int, int>; // expected-note {{in instantiation of member function 'PR15209::alias_templates::U<int, int>::U' requested here}}

    void f()
    {
      W<int>();
      // we should issue diagnostics for the following
      W<float>(); // expected-note {{in instantiation of member function 'PR15209::alias_templates::U<float, float>::U' requested here}}
    }
  }

  namespace templates {
    class A {
      typedef int I;  // expected-note {{implicitly declared private here}}
      static constexpr I x = 0; // expected-note {{implicitly declared private here}}

      template<int> friend struct B;
      template<int> struct C;
      template<template<int> class T> friend struct TT;
      template<typename T> friend void funct(T);
    };
    template<A::I> struct B { };

    template<A::I> struct A::C { };

    template<template<A::I> class T> struct TT {
      T<A::x> t;
    };

    template struct TT<B>;
    template<A::I> struct D { };  // expected-error {{'I' is a private member of 'PR15209::templates::A'}}
    template struct TT<D>;

    // function template case
    template<typename T>
    void funct(T)
    {
      (void)A::x;
    }

    template void funct<int>(int);

    void f()
    {
      (void)A::x;  // expected-error {{'x' is a private member of 'PR15209::templates::A'}}
    }
  }
}

namespace PR7434 {
  namespace comment0 {
    template <typename T> struct X;
    namespace N {
    class Y {
      template<typename T> friend struct X;
      int t; // expected-note {{here}}
    };
    }
    template<typename T> struct X {
      X() { (void)N::Y().t; } // expected-error {{private}}
    };
    X<char> x;
  }
  namespace comment2 {
    struct X;
    namespace N {
    class Y {
      friend struct X;
      int t; // expected-note {{here}}
    };
    }
    struct X {
      X() { (void)N::Y().t; } // expected-error {{private}}
    };
  }
}

namespace LocalExternVar {
  class test {
  private:
    struct private_struct { // expected-note 2{{here}}
      int x;
    };
    int use_private();
  };

  int test::use_private() {
    extern int array[sizeof(test::private_struct)]; // ok
    return array[0];
  }

  int f() {
    extern int array[sizeof(test::private_struct)]; // expected-error {{private}}
    return array[0];
  }

  int array[sizeof(test::private_struct)]; // expected-error {{private}}
}

namespace ThisLambdaIsNotMyFriend {
  class A {
    friend class D;
    static void foo(); // expected-note {{here}}
  };
  template <class T> void foo() {
    []() { A::foo(); }(); // expected-error {{private}}
  }
  void bar() { foo<void>(); }
}

namespace OverloadedMemberFunctionPointer {
  template<class T, void(T::*pMethod)()>
  void func0() {}

  template<class T, void(T::*pMethod)(int)>
  void func1() {}

  template<class T>
  void func2(void(*fn)()) {} // expected-note 2 {{candidate function template not viable: no overload of 'func}}

  class C {
  private:
    friend void friendFunc();
    void overloadedMethod();
  protected:
    void overloadedMethod(int);
  public:
    void overloadedMethod(int, int);
    void method() {
      func2<int>(&func0<C, &C::overloadedMethod>);
      func2<int>(&func1<C, &C::overloadedMethod>);
    }
  };

  void friendFunc() {
    func2<int>(&func0<C, &C::overloadedMethod>);
    func2<int>(&func1<C, &C::overloadedMethod>);
  }

  void nonFriendFunc() {
    func2<int>(&func0<C, &C::overloadedMethod>); // expected-error {{no matching function for call to 'func2'}}
    func2<int>(&func1<C, &C::overloadedMethod>); // expected-error {{no matching function for call to 'func2'}}
  }

  // r325321 caused an assertion failure when the following code was compiled.
  class A {
    template <typename Type> static bool foo1() { return true; }

  public:
    void init(bool c) {
      if (c) {
        auto f = foo1<int>;
      }
    }
  };
}

namespace MultiplePathsToSameMember {
  namespace NonVirtual {
    struct A {
      static int a; // expected-note {{declared here}}
      using T = int; // expected-note {{declared here}}
    };
    struct B : private A {}; // expected-note 2{{constrained by private inheritance here}}
    struct C : public A {
    private:
      using A::a; // expected-note {{declared private here}}
      using T = int; // expected-note {{declared private here}}
    };
    struct D : public A {};
    struct E : public A {
    public:
      using A::a;
      using T = int;
    };

    struct X1 : B, C {};
    struct X2 : C, B {};
    struct X3 : D, C {};
    struct X4 : C, D {};
    struct X5 : B, E {};
    struct X6 : E, B {};

    int k1 = X1::a; // expected-error {{'a' is a private member of 'MultiplePathsToSameMember::NonVirtual::A'}}
    int k2 = X2::a; // expected-error {{'a' is a private member of 'MultiplePathsToSameMember::NonVirtual::C'}}
    int k3 = X3::a;
    int k4 = X4::a;
    int k5 = X5::a;
    int k6 = X6::a;
    X1::T u1; // expected-error {{'T' is a private member of 'MultiplePathsToSameMember::NonVirtual::A'}}
    X2::T u2; // expected-error {{'T' is a private member of 'MultiplePathsToSameMember::NonVirtual::C'}}
    X3::T u3;
    X4::T u4;
    X5::T u5;
    X6::T u6;
  }

  namespace Virtual {
    struct A {
      int a;
      using T = int;
    };
    struct B : private virtual A {};
    struct C : public virtual A {
    private:
      using A::a; // expected-note 4{{declared private here}}
      using T = int; // expected-note 4{{declared private here}}
    };
    struct D : public virtual A {};
    struct E : public virtual A {
    public:
      using A::a;
      using T = int;
    };

    struct X1 : B, C {};
    struct X2 : C, B {};
    struct X3 : D, C {};
    struct X4 : C, D {};
    struct X5 : B, E {};
    struct X6 : E, B {};

    int k1 = X1().a; // expected-error {{'a' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    int k2 = X2().a; // expected-error {{'a' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    // Public declarations in vbase A (reached via D) are hidden by private
    // declarations in C.
    int k3 = X3().a; // expected-error {{'a' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    int k4 = X4().a; // expected-error {{'a' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    int k5 = X5().a;
    int k6 = X6().a;
    X1::T u1; // expected-error {{'T' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    X2::T u2; // expected-error {{'T' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    // Public declarations in vbase A (reached via D) are hidden by private
    // declarations in C.
    X3::T u3; // expected-error {{'T' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    X4::T u4; // expected-error {{'T' is a private member of 'MultiplePathsToSameMember::Virtual::C'}}
    X5::T u5;
    X6::T u6;
  }

  namespace Unrelated {
    struct A { using T = int; };
    class B { using T = int; };
    struct C { using T = int; }; // expected-note 3{{declared here}}
    struct D1 : A, B {};
    struct D2 : B, A {};
    struct D3 : private C, B {}; // expected-note {{constrained by}}
    struct D4 : B, private C {}; // expected-note {{constrained by}}
    struct D5 : B, private C {}; // expected-note {{constrained by}}

    using T = int;
    using T = D1::T;
    using T = D2::T;
    using T = D3::T; // expected-error {{private}}
    using T = D4::T; // expected-error {{private}}
    using T = D5::T; // expected-error {{private}}
  }
}
