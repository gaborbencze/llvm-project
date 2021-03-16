// RUN: %clang_analyze_cc1 -analyzer-checker=alpha.cplusplus.ArrayDeleteThroughIncorrectType -verify %s

struct Base {
  virtual ~Base() = default;
};

struct Derived final : Base {};

void deleteThroughBasePointer() {
  Base *b = new Derived[10];
  delete[] b;
  // expected-warning@-1 {{Deleting an array through a pointer to the incorrect type}}
}

void nonArrayForm() {
  Base *b = new Derived();
  delete b;
}

void unknownType(Base *b) { delete[] b; }

void deleteThroughCorrectPointer() {
  Derived *b = new Derived[10];
  delete[] b;
}

void downcastAtDelete() {
  Base *b = new Derived[10];
  delete[](static_cast<Derived *>(b));
}
