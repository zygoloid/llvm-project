// RUN: %clang_cc1 %s -fno-rtti-data -fsyntax-only -verify

namespace std {
struct type_info {};
} // namespace std
class B {
public:
  virtual ~B() = default;
};

class D1 : public B {
public:
  ~D1() = default;
};

void f() {
  B* b = new D1();
  auto d = dynamic_cast<D1 *>(b); // expected-warning{{dynamic_cast will not work since RTTI data is disabled by -fno-rtti-data}}
  void* v = dynamic_cast<void *>(b);
  (void)typeid(int);              // expected-warning{{typeid will not work since RTTI data is disabled by -fno-rtti-data}}
}
