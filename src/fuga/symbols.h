#pragma once
#ifndef FUGA_SYMBOLS_H
#define FUGA_SYMBOLS_H

#include "common.h"

typedef struct FugaSymbols FugaSymbols;

FugaSymbols* FugaSymbols_new(void* gc);
Fuga* FugaSymbols_get(FugaSymbols* self, const char* name);
void FugaSymbols_set(FugaSymbols* self,
    const char* name,
    Fuga* value
);

#endif

