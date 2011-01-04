
#include "slots.h"
#include "gc.h"
#include "test.h"


/**
*** # The underlying data structure: an associative list
*** ### _FugaSlotsList
***
*** This is a highly inefficient way to implement slots! 'Tis an
*** an associative list. All operations are O(n), which can be problematic.
*** This is such a bad data structure to use that I will eventually force
*** myself to implement something a little nicer.
**/
typedef struct _FugaSlotsList _FugaSlotsList;
struct _FugaSlotsList {
    _FugaSlotsList* next;
    FugaIndex index;
    FugaSlot slot;
};

/**
*** ### _FugaSlotsList_mark
***
*** Mark the list's dependencies. This is the FugaGCMarkFn for
*** the _FugaSlotsList.
**/
void _FugaSlotsList_mark(void* self) {
    _FugaSlotsList* list = self;
    FugaGC_mark(list, list->next);
    FugaGC_mark(list, list->slot.name);
    FugaGC_mark(list, list->slot.value);
}

/**
*** ### _FugaSlotsList_new
***
*** Return a new list, without any fields allocated.
**/
_FugaSlotsList* _FugaSlotsList_new(void* gc) {
    ALWAYS(gc);
    _FugaSlotsList *list = FugaGC_alloc(gc, sizeof(_FugaSlotsList));
    FugaGC_onMark(list, _FugaSlotsList_mark);
    return list;
}

/**
*** ### _FugaSlotsList_has
**/
bool _FugaSlotsList_has(_FugaSlotsList* list, FugaIndex index) {
    for (; list; list = list->next)
        if (list->index == index)
            return true;
    return false;
}

#ifdef TESTING
TESTS(_FugaSlotsList_has) {
    FugaGC *gc = FugaGC_start();
    
    _FugaSlotsList *a = _FugaSlotsList_new(gc);
    _FugaSlotsList *b = _FugaSlotsList_new(gc);

    TEST(!_FugaSlotsList_has(NULL, 0));
    TEST(!_FugaSlotsList_has(NULL, 1));

    a->next = NULL;
    a->index = 1;
    TEST(!_FugaSlotsList_has(a, 0));
    TEST(_FugaSlotsList_has(a, 1));

    b->next = NULL;
    b->index = 0;
    TEST(_FugaSlotsList_has(b, 0));
    TEST(!_FugaSlotsList_has(b, 1));
    
    b->next = a;
    TEST(_FugaSlotsList_has(b, 0));
    TEST(_FugaSlotsList_has(b, 1));
    TEST(!_FugaSlotsList_has(b, 2));

    FugaGC_end(gc);
}
#endif

/**
*** ### _FugaSlotsList_get
**/
FugaSlot* _FugaSlotsList_get(_FugaSlotsList* list, FugaIndex index) {
    for (; list; list = list->next)
        if (list->index == index)
            return &(list->slot);
    return NULL;
}

#ifdef TESTING
TESTS(_FugaSlotsList_get) {
    FugaGC *gc = FugaGC_start();
    
    _FugaSlotsList *a = _FugaSlotsList_new(gc);
    _FugaSlotsList *b = _FugaSlotsList_new(gc);

    TEST(_FugaSlotsList_get(NULL, 0) == NULL);
    TEST(_FugaSlotsList_get(NULL, 1) == NULL);

    a->next = NULL;
    a->index = 1;
    TEST(_FugaSlotsList_get(a, 0) == NULL);
    TEST(_FugaSlotsList_get(a, 1) == &a->slot);

    b->next = NULL;
    b->index = 0;
    TEST(_FugaSlotsList_get(b, 0) == &b->slot);
    TEST(_FugaSlotsList_get(b, 1) == NULL);
    
    b->next = a;
    TEST(_FugaSlotsList_get(b, 0) == &b->slot);
    TEST(_FugaSlotsList_get(b, 1) == &a->slot);
    TEST(_FugaSlotsList_get(b, 2) == NULL);

    FugaGC_end(gc);
}
#endif

/**
*** ### _FugaSlotsList_set
***
*** Return a new list with the given slot.
**/
_FugaSlotsList* _FugaSlotsList_set(
    _FugaSlotsList* oldlist,
    FugaIndex index,
    FugaSlot slot
) {
    ALWAYS(slot.name);
    ALWAYS(slot.value);

    _FugaSlotsList* newlist;
    
    for (newlist = oldlist; newlist; newlist = newlist->next) {
        if (newlist->index == index) {
            newlist->slot = slot;
            return oldlist;
        }
    }
    
    newlist = _FugaSlotsList_new(slot.name);
    newlist->next = oldlist;
    newlist->index = index;
    newlist->slot = slot;
    return newlist;
}

#ifdef TESTING
TESTS(_FugaSlotsList_set) {
    FugaGC *gc = FugaGC_start();

    FugaSlot slot = {(void*)gc,(void*)gc};

    FugaIndex ia = 0;
    FugaIndex ib = 1;
    FugaIndex ic = 2;

    _FugaSlotsList* a = NULL;
    _FugaSlotsList* b = _FugaSlotsList_set(a, ia, slot);
    _FugaSlotsList* c = _FugaSlotsList_set(b, ib, slot);
    _FugaSlotsList* d = _FugaSlotsList_set(c, ic, slot);
    _FugaSlotsList* e = _FugaSlotsList_set(d, ib, slot);

    TEST(b != NULL);
    TEST(_FugaSlotsList_has(b, ia));

    TEST(c != b);
    TEST(_FugaSlotsList_has(c, ia));
    TEST(_FugaSlotsList_has(c, ib));

    TEST(d != c);
    TEST(_FugaSlotsList_has(d, ia));
    TEST(_FugaSlotsList_has(d, ib));
    TEST(_FugaSlotsList_has(d, ic));

    TEST(e == d);

    FugaGC_end(gc);
}
#endif

/**
*** # FugaSlots
*** ### FugaSlots
**/
struct FugaSlots {
    size_t length;
    struct _FugaSlotsList* byIndex;
    struct _FugaSlotsList* bySymbol;
};

/**
*** ### _FugaSlots_mark
***
*** Mark `FugaSlots`'s references.
**/
void _FugaSlots_mark(void* self) {
    FugaSlots* slots = self;
    FugaGC_mark(self, slots->byIndex);
    FugaGC_mark(self, slots->bySymbol);
}

/**
*** ## Constructors
*** ### FugaSlots_new
***
*** Create an empty `FugaSlots`.
**/
FugaSlots* FugaSlots_new(void* gc) {
    ALWAYS(gc);
    FugaSlots* slots = FugaGC_alloc(gc, sizeof(FugaSlots));
    FugaGC_onMark(slots, _FugaSlots_mark);
    slots->length = 0;
    slots->byIndex = NULL;
    slots->bySymbol = NULL;
    return slots;
}

/**
*** ## Properties
*** ### FugaSlots_length
***
*** Return the number of canonical slots. In other words, return the number
*** of contiguous indexed slots starting from 0.
**/
size_t FugaSlots_length(FugaSlots* slots) {
    ALWAYS(slots);
    return slots->length;
}

/**
*** ## Has
*** ### FugaSlots_hasByIndex
***
*** Determine whether there is a slot with the given index.
**/
bool FugaSlots_hasByIndex(FugaSlots* slots, FugaIndex index) {
    ALWAYS(slots);
    return _FugaSlotsList_has(slots->byIndex, index);
}

/**
*** ### FugaSlots_hasBySymbol
***
*** Determine whether there is a slot with the give symbol.
**/
bool FugaSlots_hasBySymbol(FugaSlots* slots, Fuga* symbol) {
    ALWAYS(slots);
    ALWAYS(symbol);
    FugaIndex index = (FugaIndex)symbol;
    return _FugaSlotsList_has(slots->bySymbol, index);
}

/**
*** ## Get
*** ### FugaSlots_getByIndex
***
*** Get the slot associated with a given index.
**/
FugaSlot* FugaSlots_getByIndex(FugaSlots* slots, FugaIndex index) {
    ALWAYS(slots);
    return _FugaSlotsList_get(slots->byIndex, index);
}

/**
*** ### FugaSlots_getBySymbol
***
*** Get the slot associated with a given symbol.
**/
FugaSlot* FugaSlots_getBySymbol(FugaSlots* slots, Fuga* symbol) {
    ALWAYS(slots);
    ALWAYS(symbol);
    FugaIndex index = (FugaIndex)symbol;
    return _FugaSlotsList_get(slots->bySymbol, index);
}

/**
*** ## Set
*** ### FugaSlots_setByIndex
***
*** Set or update the slot associated with a given index. You still
*** need to pass in a Fuga* object for the name because FugaSlots is
*** completely Fuga-agnostic. It doesn't know anything about Fuga
*** objects other than their addresses, and the fact that they need to
*** be garbage-collected, so FugaSlots can't fabricate its own Fuga
*** object for the name.
**/
void FugaSlots_setByIndex(
    FugaSlots* slots,
    FugaIndex index,
    FugaSlot  slot
) {
    ALWAYS(slots);
    ALWAYS(slot.name);
    ALWAYS(slot.value);

    slots->byIndex = _FugaSlotsList_set(slots->byIndex, index, slot);
    if (index == slots->length) {
        slots->length += 1;
        for (;;) {
            index = slots->length;
            if (!FugaSlots_hasByIndex(slots, index)) break;
            slots->length++;
        }
    }
}

#ifdef TESTING
TESTS(FugaSlots_setByIndex) {
    FugaGC* gc = FugaGC_start();
    FugaSlots* slots = FugaSlots_new(gc);
    Fuga* name  = FugaGC_alloc(gc, 1);
    Fuga* value1 = FugaGC_alloc(gc, 1);
    Fuga* value2 = FugaGC_alloc(gc, 1);
    FugaSlot slot1 = {name, value1};
    FugaSlot slot2 = {name, value2};

    bool h;

    TEST(FugaSlots_length(slots) == 0);
    TEST(!FugaSlots_hasByIndex(slots, 0));
    TEST(FugaSlots_getByIndex(slots, 0) == NULL)
    FugaSlots_setByIndex(slots, 0, slot1);
    TEST(h = FugaSlots_hasByIndex(slots, 0));
    if (h) {
        TEST(FugaSlots_getByIndex(slots, 0)->value == value1)
    }
    TEST(FugaSlots_length(slots) == 1);

    TEST(!FugaSlots_hasByIndex(slots, 1));
    TEST(FugaSlots_getByIndex(slots, 1) == NULL)
    FugaSlots_setByIndex(slots, 1, slot2);
    TEST(h = FugaSlots_hasByIndex(slots, 1));
    if (h) {
        TEST(FugaSlots_getByIndex(slots, 1)->value == value2)
        FugaSlots_setByIndex(slots, 1, slot1);
        TEST(FugaSlots_getByIndex(slots, 1)->value == value1)
    }
    TEST(FugaSlots_length(slots) == 2);

    FugaSlots_setByIndex(slots, 3, slot1);
    TEST(FugaSlots_length(slots) == 2);
    FugaSlots_setByIndex(slots, 2, slot1);
    TEST(FugaSlots_length(slots) == 4);

    FugaGC_end(gc);
}
#endif

/**
*** ### FugaSlots_setBySymbol
***
*** Set or update the slot associated with a given symbol.
**/
void FugaSlots_setBySymbol(
    FugaSlots* slots,
    Fuga* name,
    FugaSlot slot
) {
    ALWAYS(slots);
    ALWAYS(slot.name);
    ALWAYS(slot.value);
    ALWAYS(name);

    FugaIndex index = (FugaIndex)name;
    slots->bySymbol = _FugaSlotsList_set(slots->bySymbol, index, slot);
}

#ifdef TESTING
TESTS(FugaSlots_setBySymbol) {
    FugaGC* gc = FugaGC_start();
    Fuga* name1 = FugaGC_alloc(gc, 4);
    Fuga* name2 = FugaGC_alloc(gc, 4);
    Fuga* value1 = FugaGC_alloc(gc, 4);
    Fuga* value2 = FugaGC_alloc(gc, 4);
    FugaSlot slot1 = {name1, value1};
    FugaSlot slot2 = {name2, value2};
    bool h;

   
    FugaSlots* slots = FugaSlots_new(gc);

    TEST(!FugaSlots_hasBySymbol(slots, name1));
    TEST(FugaSlots_getBySymbol(slots, name1) == NULL)
    FugaSlots_setBySymbol(slots, name1, slot1);
    TEST(h = FugaSlots_hasBySymbol(slots, name1));
    if (h) {
        TEST(FugaSlots_getBySymbol(slots, name1)->value == value1)
    }

    TEST(!FugaSlots_hasBySymbol(slots, name2));
    TEST(FugaSlots_getBySymbol(slots, name2) == NULL)
    FugaSlots_setBySymbol(slots, name2, slot2);
    TEST(h = FugaSlots_hasBySymbol(slots, name2));
    if (h) {
        TEST(FugaSlots_getBySymbol(slots, name2)->value == value2)

        FugaSlots_setBySymbol(slots, name2, slot1);
        TEST(FugaSlots_getBySymbol(slots, name2)->value == value1)
    }

    FugaGC_end(gc);
}
#endif
