// RUN: %check_clang_tidy %s bugprone-suspicious-memory-comparison %t \
// RUN: -- -- -target x86_64-unknown-unknown

namespace std {
typedef __SIZE_TYPE__ size_t;
int memcmp(const void *lhs, const void *rhs, size_t count);
} // namespace std

namespace sei_cert_example_oop57_cpp {
class C {
  int i;

public:
  virtual void f();
};

void f(C &c1, C &c2) {
  if (!std::memcmp(&c1, &c2, sizeof(C))) {
    // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: comparing object representation
    // of non-standard-layout type sei_cert_example_oop57_cpp::C; consider
    // using a comparison operator instead
  }
}
} // namespace sei_cert_example_oop57_cpp

namespace inner_padding_64bit_only {
struct S {
  int x;
  int *y;
};

void test() {
  S a, b;
  std::memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // inner_padding_64bit_only::S; consider comparing the fields manually
}
} // namespace inner_padding_64bit_only

namespace padding_in_base {
class Base {
  char c;
  int i;
};

class Derived : public Base {};

class Derived2 : public Derived {};

void testDerived() {
  Derived a, b;
  std::memcmp(&a, &b, sizeof(char));
  std::memcmp(&a, &b, sizeof(Base));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // padding_in_base::Derived; consider comparing the fields manually
  std::memcmp(&a, &b, sizeof(Derived));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // padding_in_base::Derived; consider comparing the fields manually
}

void testDerived2() {
  Derived2 a, b;
  std::memcmp(&a, &b, sizeof(char));
  std::memcmp(&a, &b, sizeof(Base));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // padding_in_base::Derived2; consider comparing the fields manually
  std::memcmp(&a, &b, sizeof(Derived2));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // padding_in_base::Derived2; consider comparing the fields manually
}

} // namespace padding_in_base

namespace no_padding_in_base {
class Base {
  int a, b;
};

class Derived : public Base {};

class Derived2 : public Derived {};

void testDerived() {
  Derived a, b;
  std::memcmp(&a, &b, sizeof(Base));
  std::memcmp(&a, &b, sizeof(Derived));
}

void testDerived2() {
  Derived2 a, b;
  std::memcmp(&a, &b, sizeof(char));
  std::memcmp(&a, &b, sizeof(Base));
  std::memcmp(&a, &b, sizeof(Derived2));
}
} // namespace no_padding_in_base

namespace non_standard_layout {
class C {
private:
  int x;

public:
  int y;
};

void test() {
  C a, b;
  std::memcmp(&a, &b, sizeof(C));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing object representation
  // of non-standard-layout type non_standard_layout::C; consider using a
  // comparison operator instead
}

} // namespace non_standard_layout

namespace static_ignored {
struct S {
  static char c;
  int i;
};

void test() {
  S a, b;
  std::memcmp(&a, &b, sizeof(S));
}

} // namespace static_ignored
