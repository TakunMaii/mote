#include <stdbool.h>
extern bool foo(bool x);
extern bool baz(void);
bool test(void) { return foo(baz()); }
