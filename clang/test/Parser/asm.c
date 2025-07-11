// RUN: %clang_cc1 -fsyntax-only -verify %s

#if !__has_extension(gnu_asm)
#error Extension 'gnu_asm' should be available by default
#endif

void f1(void) {
  // PR7673: Some versions of GCC support an empty clobbers section.
  asm ("ret" : : :);
}

void f2(void) {
  asm("foo" : "=r" (a)); // expected-error {{use of undeclared identifier 'a'}}
  asm("foo" : : "r" (b)); // expected-error {{use of undeclared identifier 'b'}} 

  [[]] asm("");
  [[gnu::deprecated]] asm(""); // expected-warning {{'gnu::deprecated' attribute ignored}}
}

void a(void) __asm__(""); // expected-error {{cannot use an empty string literal in 'asm'}}
void a(void) {
  __asm__(""); // ok
}

__asm ; // expected-error {{expected '(' after 'asm'}}

// Don't crash on wide string literals in 'asm'.
int foo asm (L"bar"); // expected-error {{cannot use wide string literal in 'asm'}}

asm() // expected-error {{expected string literal in 'asm'}}
// expected-error@-1 {{expected ';' after top-level asm block}}

asm(; // expected-error {{expected string literal in 'asm'}}

asm("") // expected-error {{expected ';' after top-level asm block}}

// Unterminated asm strings at the end of the file were causing us to crash, so
// this needs to be last.
// expected-warning@+3 {{missing terminating '"' character}}
// expected-error@+2 {{expected string literal in 'asm'}}
// expected-error@+1 {{expected ';' after top-level asm block}}
asm("
