.. title:: clang-tidy - bugprone-suspicious-memory-comparison

bugprone-suspicious-memory-comparison
=====================================

Finds potentially incorrect calls to ``memcmp()`` that compare padding or
non-standard-layout classes.

This check corresponds to the CERT C Coding Standard rule
`EXP42-C. Do not compare padding data
<https://wiki.sei.cmu.edu/confluence/display/c/EXP42-C.+Do+not+compare+padding+data>`_.
and part of the CERT C++ Coding Standard rule
`OOP57-CPP. Prefer special member functions and overloaded operators to C Standard Library functions
<https://wiki.sei.cmu.edu/confluence/display/cplusplus/OOP57-CPP.+Prefer+special+member+functions+and+overloaded+operators+to+C+Standard+Library+functions>`_.
