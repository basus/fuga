#ifndef FUGA_INT_H
#define FUGA_INT_H

#include "fuga.h"

extern FugaType FugaType_Int;

#define FUGA_INT(x) FugaInt_new(self, (x))
Fuga* FugaInt_new(Fuga*, long);
long FugaInt_value(Fuga* self);

Fuga* FugaInt_add(Fuga*, Fuga*);
Fuga* FugaInt_sub(Fuga*, Fuga*);
Fuga* FugaInt_mul(Fuga*, Fuga*);
Fuga* FugaInt_div(Fuga*, Fuga*);

Fuga* FugaInt_str(Fuga*);
size_t FugaInt_cstr(Fuga*, char dest, size_t maxlen);

#endif

