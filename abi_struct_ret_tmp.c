typedef struct { int a; } S4;
typedef struct { int a; int b; } S8;
typedef struct { int a; int b; int c; } S12;
typedef struct { int a; void *p; } S16mix;
typedef struct { long long a; long long b; } S16ll;
S4 r4(void){ S4 x = {1}; return x; }
S8 r8(void){ S8 x = {1,2}; return x; }
S12 r12(void){ S12 x = {1,2,3}; return x; }
S16mix r16mix(void){ S16mix x = {1,0}; return x; }
S16ll r16ll(void){ S16ll x = {1,2}; return x; }
void p8(S8 x) {}
void p12(S12 x) {}
void p16mix(S16mix x) {}
void p16ll(S16ll x) {}
