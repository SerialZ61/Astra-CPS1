#define EXTRA(symbol,...) extern struct BurnDriver symbol;
#include "sets.inc"
#undef EXTRA
static struct BurnDriver *const pDriver[] = {
#define EXTRA(symbol,...) &symbol,
#include "sets.inc"
#undef EXTRA
};
