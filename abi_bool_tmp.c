#include <stdbool.h>
extern bool foo(void);
extern void bar(bool x);
bool call_foo(void) { return foo(); }
void call_bar(bool x) { bar(x); }
