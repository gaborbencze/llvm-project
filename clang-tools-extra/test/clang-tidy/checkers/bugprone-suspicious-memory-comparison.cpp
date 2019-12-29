// RUN: %check_clang_tidy %s bugprone-suspicious-memory-comparison %t

typedef unsigned long long size_t;
int memcmp(const void *lhs, const void *rhs, size_t count);

namespace std {
int memcmp(const void *lhs, const void *rhs, size_t count);
}

namespace sei_cert_example_exp42_c {
struct s {
  char c;
  int i;
  char buffer[13];
};

void noncompliant(const struct s *left, const struct s *right) {
  if ((left && right) && (0 == memcmp(left, right, sizeof(struct s)))) {
    // CHECK-MESSAGES: :[[@LINE-1]]:32: warning: comparing padding bytes
  }
}

void compliant(const struct s *left, const struct s *right) {
  if ((left && right) && (left->c == right->c) && (left->i == right->i) &&
      (0 == memcmp(left->buffer, right->buffer, 13))) {
  }
}
} // namespace sei_cert_example_exp42_c

namespace sei_cert_example_exp42_c_ex1 {
#pragma pack(push, 1)
struct s {
  char c;
  int i;
  char buffer[13];
};
#pragma pack(pop)

void compare(const struct s *left, const struct s *right) {
  if ((left && right) && (0 == memcmp(left, right, sizeof(struct s)))) {
    // no-warning
  }
}
} // namespace sei_cert_example_exp42_c_ex1

namespace sei_cert_example_oop57_cpp {
class C {
  int i;

public:
  virtual void f();
};

void f(C &c1, C &c2) {
  if (!std::memcmp(&c1, &c2, sizeof(C))) {
    // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: comparing object representation
    // of non-standard-layout type 'sei_cert_example_oop57_cpp::C'; consider
    // using a comparison operator instead
  }
}
} // namespace sei_cert_example_oop57_cpp

namespace trailing_padding {
struct S {
  int i;
  char c;
};

void test() {
  S a, b;
  memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(int));
  memcmp(&a, &b, sizeof(int) + sizeof(char));
  memcmp(&a, &b, 2 * sizeof(int));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace trailing_padding

namespace inner_padding {
struct S {
  char c;
  int i;
};

void test() {
  S a, b;
  memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(char) + sizeof(int));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(char));
  memcmp(&a, &b, 2 * sizeof(char));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace inner_padding

namespace bitfield {
struct S {
  int x : 10;
  int y : 6;
};

void test() {
  S a, b;
  memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, 2); // no-warning: no padding in first 2 bytes
}
} // namespace bitfield

namespace bitfield_2 {
struct S {
  int x : 10;
  int y : 7;
};

void test() {
  S a, b;
  memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, 2);
  memcmp(&a, &b, 3);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace bitfield_2

namespace bitfield_3 {
struct S {
  int x : 2;
  int : 0;
  int y : 6;
};

void test() {
  S a, b;
  memcmp(&a, &b, 1);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace bitfield_3

namespace array_no_padding {
struct S {
  int x;
  int y;
};

void test() {
  S a[3];
  S b[3];
  memcmp(a, b, 3 * sizeof(S));
}
} // namespace array_no_padding

namespace array_with_padding {
struct S {
  int x;
  char y;
};

void test() {
  S a[3];
  S b[3];
  memcmp(a, b, 3 * sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace array_with_padding

namespace unknown_count {
struct S {
  char c;
  int i;
};

void test(size_t c) {
  S a;
  S b;
  memcmp(&a, &b, c); // no-warning: unknown count value
}
} // namespace unknown_count

namespace predeclared_type {
struct S;

void test(const S *lhs, const S *rhs) {
  memcmp(lhs, rhs, 1); // no-warning: predeclared type
}
} // namespace predeclared_type

namespace padding_after_union {
struct S {
  union {
    char c;
    short s;
  } x;

  int y;
};

void test() {
  S a;
  S b;
  memcmp(&a, &b, sizeof(short));
  memcmp(&a, &b, sizeof(int));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace padding_after_union

namespace union_no_padding {
struct S {
  union {
    int a;
    char b;
  } x;

  int y;
};

void test() {
  S a;
  S b;
  memcmp(&a, &b, 2 * sizeof(int));
  memcmp(&a, &b, sizeof(S));
}
} // namespace union_no_padding

namespace padding_in_nested_struct {
struct S {
  int a;
  char b;
};
struct T {
  S x;
  char y;
};

void test() {
  T a, b;
  memcmp(&a, &b, sizeof(int) + sizeof(char));
  memcmp(&a, &b, sizeof(int) + 2 * sizeof(char));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(T));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace padding_in_nested_struct

namespace padding_after_nested_struct {
struct S {
  char a;
};
struct T {
  S x;
  int y;
};

void test() {
  T a, b;
  memcmp(&a, &b, sizeof(char));
  memcmp(&a, &b, sizeof(S));
  memcmp(&a, &b, sizeof(T));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}
} // namespace padding_after_nested_struct

namespace padding_in_base {
class Base {
  char c;
  int i;
};

class Derived : public Base {};

class Derived2 : public Derived {};

void testDerived() {
  Derived a, b;
  memcmp(&a, &b, sizeof(char));
  memcmp(&a, &b, sizeof(Base));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(Derived));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
}

void testDerived2() {
  Derived2 a, b;
  memcmp(&a, &b, sizeof(char));
  memcmp(&a, &b, sizeof(Base));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
  memcmp(&a, &b, sizeof(Derived2));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding bytes
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
  memcmp(&a, &b, sizeof(Base));
  memcmp(&a, &b, sizeof(Derived));
}

void testDerived2() {
  Derived2 a, b;
  memcmp(&a, &b, sizeof(char));
  memcmp(&a, &b, sizeof(Base));
  memcmp(&a, &b, sizeof(Derived2));
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
  memcmp(&a, &b, sizeof(C));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing object representation
  // of non-standard-layout type 'non_standard_layout::C'; consider using a
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
  memcmp(&a, &b, sizeof(S));
}

} // namespace static_ignored
