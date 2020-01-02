// RUN: %check_clang_tidy %s bugprone-suspicious-memory-comparison %t \
// RUN: -- -- -target x86_64-unknown-unknown -std=c99

typedef __SIZE_TYPE__ size_t;
int memcmp(const void *lhs, const void *rhs, size_t count);

// Examples from cert rule exp42-c

struct S {
  char c;
  int i;
  char buffer[13];
};

void noncompliant(const struct S *left, const struct S *right) {
  if ((left && right) && (0 == memcmp(left, right, sizeof(struct S)))) {
    // CHECK-MESSAGES: :[[@LINE-1]]:32: warning: comparing padding data in type
    // S; consider comparing the fields manually
  }
}

void compliant(const struct S *left, const struct S *right) {
  if ((left && right) && (left->c == right->c) && (left->i == right->i) &&
      (0 == memcmp(left->buffer, right->buffer, 13))) {
  }
}

#pragma pack(push, 1)
struct Packed_S {
  char c;
  int i;
  char buffer[13];
};
#pragma pack(pop)

void compliant_packed(const struct Packed_S *left,
                      const struct Packed_S *right) {
  if ((left && right) && (0 == memcmp(left, right, sizeof(struct Packed_S)))) {
    // no-warning
  }
}

struct PredeclaredType;

void Test_PredeclaredType(const struct PredeclaredType *lhs,
                          const struct PredeclaredType *rhs) {
  memcmp(lhs, rhs, 1); // no-warning: predeclared type
}

struct NoPadding {
  int x;
  int y;
};

void Test_NoPadding() {
  struct NoPadding a, b;
  memcmp(&a, &b, sizeof(struct NoPadding));
}

void TestArray_NoPadding() {
  struct NoPadding a[3], b[3];
  memcmp(a, b, 3 * sizeof(struct NoPadding));
}

struct TrailingPadding {
  int i;
  char c;
};

void Test_TrailingPadding() {
  struct TrailingPadding a, b;
  memcmp(&a, &b, sizeof(struct TrailingPadding));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // TrailingPadding; consider comparing the fields manually
  memcmp(&a, &b, sizeof(int));
  memcmp(&a, &b, sizeof(int) + sizeof(char));
  memcmp(&a, &b, 2 * sizeof(int));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // TrailingPadding; consider comparing the fields manually
}

void Test_UnknownCount(size_t count) {
  struct TrailingPadding a, b;
  memcmp(&a, &b, count); // no-warning: unknown count value
}

void TestArray_TrailingPadding() {
  struct TrailingPadding a[3], b[3];
  memcmp(a, b, 3 * sizeof(struct TrailingPadding));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // TrailingPadding; consider comparing the fields manually
}

struct InnerPadding {
  char c;
  int i;
};

void Test_InnerPadding() {
  struct InnerPadding a, b;
  memcmp(&a, &b, sizeof(struct InnerPadding));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // InnerPadding; consider comparing the fields manually
  memcmp(&a, &b, sizeof(char) + sizeof(int));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // InnerPadding; consider comparing the fields manually
  memcmp(&a, &b, sizeof(char));
  memcmp(&a, &b, 2 * sizeof(char));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // InnerPadding; consider comparing the fields manually
}

struct Bitfield_TrailingPaddingBytes {
  int x : 10;
  int y : 6;
};

void Test_Bitfield_TrailingPaddingBytes() {
  struct Bitfield_TrailingPaddingBytes a, b;
  memcmp(&a, &b, sizeof(struct S));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // Bitfield_TrailingPaddingBytes; consider comparing the fields manually
  memcmp(&a, &b, 2); // no-warning: no padding in first 2 bytes
}

struct Bitfield_TrailingPaddingBits {
  int x : 10;
  int y : 7;
};

void Test_Bitfield_TrailingPaddingBits() {
  struct Bitfield_TrailingPaddingBits a, b;
  memcmp(&a, &b, sizeof(struct Bitfield_TrailingPaddingBits));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // Bitfield_TrailingPaddingBits; consider comparing the fields manually
  memcmp(&a, &b, 2);
  memcmp(&a, &b, 3);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // Bitfield_TrailingPaddingBits; consider comparing the fields manually
}

struct Bitfield_InnerPaddingBits {
  int x : 2;
  int : 0;
  int y : 6;
};

void Test_Bitfield_InnerPaddingBits() {
  struct Bitfield_InnerPaddingBits a, b;
  memcmp(&a, &b, 1);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // Bitfield_InnerPaddingBits; consider comparing the fields manually
}

struct PaddingAfterUnion {
  union {
    char c;
    short s;
  } x;

  int y;
};

void Test_PaddingAfterUnion() {
  struct PaddingAfterUnion a, b;
  memcmp(&a, &b, sizeof(short));
  memcmp(&a, &b, sizeof(int));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // PaddingAfterUnion; consider comparing the fields manually
  memcmp(&a, &b, sizeof(struct PaddingAfterUnion));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // PaddingAfterUnion; consider comparing the fields manually
}

struct Union_NoPadding {
  union {
    int a;
    char b;
  } x;

  int y;
};

void Test_Union_NoPadding() {
  struct Union_NoPadding a, b;
  memcmp(&a, &b, 2 * sizeof(int));
  memcmp(&a, &b, sizeof(struct Union_NoPadding));
}

struct PaddingInNested {
  struct TrailingPadding x;
  char y;
};

void Test_PaddingInNested() {
  struct PaddingInNested a, b;
  memcmp(&a, &b, sizeof(int) + sizeof(char));
  memcmp(&a, &b, sizeof(int) + 2 * sizeof(char));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // PaddingInNested; consider comparing the fields manually
  memcmp(&a, &b, sizeof(struct TrailingPadding));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // PaddingInNested; consider comparing the fields manually
  memcmp(&a, &b, sizeof(struct PaddingInNested));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // PaddingInNested; consider comparing the fields manually
}

struct PaddingAfterNested {
  struct {
    char a;
    char b;
  } x;
  int y;
};

void Test_PaddingAfterNested() {
  struct PaddingAfterNested a, b;
  memcmp(&a, &b, 2 * sizeof(char));
  memcmp(&a, &b, sizeof(a.x));
  memcmp(&a, &b, sizeof(struct PaddingAfterNested));
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: comparing padding data in type
  // PaddingAfterNested; consider comparing the fields manually
}
