// RUN: %clang_analyze_cc1 -analyzer-checker=alpha.cplusplus.ArrayDeleteThroughIncorrectType -verify -analyzer-output=text %s

struct Base {
  virtual ~Base() = default;
};

struct Derived : Base {};
struct DoubleDerived : Derived {};

void deleteThroughBasePointer() {
  Base *b = new Derived[10];
  // expected-note@-1 {{Allocated here}}
  delete[] b;
  // expected-warning@-1 {{Deleting an array through a pointer to the incorrect type}}
  // expected-note@-2 {{Deleting an array through a pointer to the incorrect type}}
}

void deleteThroughDerivedPointer() {
  Base *b = new Base[10];
  // expected-note@-1 {{Allocated here}}
  Derived *d = dynamic_cast<Derived *>(b);
  delete[] d;
  // expected-warning@-1 {{Deleting an array through a pointer to the incorrect type}}
  // expected-note@-2 {{Deleting an array through a pointer to the incorrect type}}
}

void deleteThroughIndirectBase() {
  Base *b = new DoubleDerived[10];
  // expected-note@-1 {{Allocated here}}
  delete[] b;
  // expected-warning@-1 {{Deleting an array through a pointer to the incorrect type}}
  // expected-note@-2 {{Deleting an array through a pointer to the incorrect type}}
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

void deleteNullptr() {
  Base *b = nullptr;
  delete[] b;
}
