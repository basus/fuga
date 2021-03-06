#include "method.h"
#include "test.h"

const FugaType FugaMethod_type = {
    "Method"
};

void* _FugaMethod_str(void* self) {
    FUGA_NEED(self);
    return FUGA_STRING("method(...)");
}


void FugaMethod_init(void* self)
{
    Fuga_setS(FUGA->Method, "_name", FUGA_STRING("Method"));
    Fuga_setS(FUGA->Method, "str",   FUGA_METHOD_STR(_FugaMethod_str));
}

struct FugaMethod {
    void* (*method)(void*, void*, void*);
};

void* FugaMethod_new_(void* self, void* (*method)(void*, void*, void*))
{
    ALWAYS(self); ALWAYS(method);
    FugaMethod* result = Fuga_clone_(FUGA->Method, sizeof(FugaMethod));
    Fuga_type_(result, &FugaMethod_type);
    result->method = method;
    return result;
}

void* FugaMethod_call(void* _self, void* recv, void* args)
{
    FugaMethod* self = _self;
    ALWAYS(self); ALWAYS(recv); ALWAYS(args);
    ALWAYS(Fuga_isMethod(self));
    FUGA_CHECK(recv); FUGA_CHECK(args);
    return self->method(self, recv, args);
}

// arg methods

typedef struct {
    void* (*call)   (void*, void*, void*);
    void* (*method) (void*, void*);
} FugaMethodN;


void* FugaMethodN_call(void* _self, void* recv, void* args)
{
    FugaMethodN* self = _self;
    return self->method(recv, args);
}

void* FugaMethodN_new_(void* self, void* (*method)(void*, void*))
{
    FugaMethodN* result = Fuga_clone_(FUGA->Method, sizeof(FugaMethodN));
    Fuga_type_(result, &FugaMethod_type);
    result->call   = FugaMethodN_call;
    result->method = method;
    return result;
}

// str method

typedef struct {
    void* (*call)   (void*, void*, void*);
    void* (*method) (void*);
} FugaMethodStr;

void* FugaMethodStr_call(void* _self, void* recv, void* args)
{
    FugaMethodStr* self = _self;
    FUGA_NEED(args);
    if (!Fuga_hasLength_(args, 0))
        FUGA_RAISE(FUGA->TypeError, "str: expected no arguments");
    if (Fuga_isTrue(Fuga_hasRawS(recv, "_name")))
        return Fuga_getRawS(recv, "_name");
    
    FugaInt* depth = Fuga_getS(FUGA->String, "_depth");
    if (Fuga_isInt(depth)) {
        long depthI = FugaInt_value(depth);
        if (depthI == 0)
            return FUGA_STRING("...");
        FUGA_CHECK(Fuga_setS(FUGA->String,"_depth", FUGA_INT(depthI-1)));
    }

    void* result = self->method(recv);

    if (Fuga_isInt(depth)) {
        FUGA_CHECK(Fuga_setS(FUGA->String, "_depth", depth));
    }
    return result;
}

void* FugaMethodStr_new_(void* self, void* (*method)(void*))
{
    FugaMethodStr* result = Fuga_clone_(FUGA->Method, sizeof(*result));
    Fuga_type_(result, &FugaMethod_type);
    result->call   = FugaMethodStr_call;
    result->method = method;
    return result;
}

// 0 arg methods 

typedef struct {
    void* (*call)   (void*, void*, void*);
    void* (*method) (void*);
} FugaMethod0;

void* FugaMethod0_call(void* _self, void* recv, void* args)
{
    FugaMethod0* self = _self;
    FUGA_NEED(args);
    if (!Fuga_hasLength_(args, 0))
        FUGA_RAISE(FUGA->TypeError, "expected no arguments");
    return self->method(recv);
}

void* FugaMethod0_new_(void* self, void* (*method)(void*))
{
    FugaMethod0* result = Fuga_clone_(FUGA->Method, sizeof(*result));
    Fuga_type_(result, &FugaMethod_type);
    result->call   = FugaMethod0_call;
    result->method = method;
    return result;
}

// op method (i.e., converts +(a,b) into a +(b), or +(a) into a \+).

typedef struct {
    void* (*call) (void*, void*, void*);
    void* op;
} FugaMethodOp;

void* FugaMethodOp_call(void* _self, void* recv, void* args)
{
    FugaMethodOp* self = _self;
    args = Fuga_lazySlots(args);
    FUGA_CHECK(self); FUGA_CHECK(recv); FUGA_NEED(args);
    if (Fuga_hasLength_(args, 1)) {
        FUGA_CHECK(recv = Fuga_getI(args, 0));
        FUGA_CHECK(args = Fuga_clone(FUGA->Object));
        return Fuga_send(recv, self->op, args);
    } else if (Fuga_hasLength_(args, 2)) {
        void* arg;
        FUGA_CHECK(recv = Fuga_getI(args, 0));
        FUGA_CHECK(arg  = Fuga_getI(args, 1));
        FUGA_CHECK(args = Fuga_clone(FUGA->Object));
        FUGA_CHECK(Fuga_append_(args, arg));
        return Fuga_send(recv, self->op, args);
    } else {
        FUGA_RAISE(FUGA->TypeError, "op expected 1 or 2 arguments");
    }
}

void FugaMethodOp_mark(void* _self) {
    FugaMethodOp* self = _self;
    Fuga_mark_(self, self->op);
}

void* FugaMethodOp_new_(void* self, const char* name)
{
    FugaMethodOp* result = Fuga_clone_(FUGA->Method, sizeof(*result));
    Fuga_type_(result, &FugaMethod_type);
    result->call = FugaMethodOp_call;
    result->op   = FUGA_SYMBOL(name);
    Fuga_onMark_(result, FugaMethodOp_mark);
    return result;
}

// 1 arg methods 

typedef struct {
    void* (*call)   (void*, void*, void*);
    void* (*method) (void*, void*);
} FugaMethod1;

void* FugaMethod1_call(void* _self, void* recv, void* args)
{
    FugaMethod1* self = _self;
    FUGA_NEED(args);
    if (!Fuga_hasLength_(args, 1))
        FUGA_RAISE(FUGA->TypeError, "expected 1 argument");
    void* arg0 = Fuga_getI(args, 0);
    return self->method(recv, arg0);
}

void* FugaMethod1_new_(void* self, void* (*method)(void*, void*))
{
    FugaMethod1* result = Fuga_clone_(FUGA->Method, sizeof(*result));
    Fuga_type_(result, &FugaMethod_type);
    result->call   = FugaMethod1_call;
    result->method = method;
    return result;
}

// 2 arg methods 

typedef struct {
    void* (*call)   (void*, void*, void*);
    void* (*method) (void*, void*, void*);
} FugaMethod2;

void* FugaMethod2_call(void* _self, void* recv, void* args)
{
    FugaMethod2* self = _self;
    FUGA_NEED(args);
    if (!Fuga_hasLength_(args, 2))
        FUGA_RAISE(FUGA->TypeError, "expected 2 arguments");
    void* arg0 = Fuga_getI(args, 0);
    void* arg1 = Fuga_getI(args, 1);
    return self->method(recv, arg0, arg1);
}

void* FugaMethod2_new_(void* self, void* (*method)(void*, void*, void*))
{
    FugaMethod2* result = Fuga_clone_(FUGA->Method, sizeof(*result));
    Fuga_type_(result, &FugaMethod_type);
    result->call   = FugaMethod2_call;
    result->method = method;
    return result;
}

// fuga method

typedef struct {
    void* (*call)   (void*, void*, void*);
} FugaMethodFuga;

void* FugaMethodFuga_scope(void* self)
{
    return Fuga_getS(self, "scope");
}

void* FugaMethodFuga_args(void* self)
{
    return Fuga_getS(self, "args");
}

void* FugaMethodFuga_body(void* self)
{
    return Fuga_getS(self, "body");
}

void* FugaMethodFuga_call(void* self, void* recv, void* args)
{
    FUGA_CHECK(self); FUGA_CHECK(recv); FUGA_CHECK(args);
    void* scope   = FugaMethodFuga_scope (self);
    void* argss   = FugaMethodFuga_args  (self);
    void* bodys   = FugaMethodFuga_body  (self);
    FUGA_CHECK(scope); FUGA_CHECK(bodys); FUGA_CHECK(argss);
    ALWAYS(Fuga_isMethod(self));
    FUGA_CHECK(scope = Fuga_clone(scope));
    FUGA_CHECK(Fuga_setS(scope, "self", recv));

    FUGA_FOR(i, formals, argss) {
        void* match = Fuga_match_(formals, args);
        FUGA_TRY(match) {
            match = NULL;
            FUGA_CATCH(FUGA->MatchError)
                break; 
            FUGA_RERAISE;
        }
        if (match) {
            void* body = Fuga_getI(bodys, i);
            FUGA_CHECK(body);
            FUGA_CHECK(Fuga_update_(scope, match));
            return Fuga_eval(body, scope, scope);
        }
    }

    FUGA_RAISE(FUGA->TypeError, "no patterns match");
}

void* FugaMethod_method(void* self, void* args, void* body)
{
    FUGA_NEED(self);
    FUGA_NEED(args);
    FUGA_NEED(body);
    FugaMethodFuga* result = Fuga_clone_(FUGA->Method, sizeof *result);
    Fuga_type_(result, &FugaMethod_type);
    result->call = FugaMethodFuga_call;
    void* argss = Fuga_clone(FUGA->Object); Fuga_append_(argss, args);
    void* bodys = Fuga_clone(FUGA->Object); Fuga_append_(bodys, body);
    FUGA_CHECK(Fuga_setS(result, "scope", self));
    FUGA_CHECK(Fuga_setS(result, "args",  argss));
    FUGA_CHECK(Fuga_setS(result, "body",  bodys));
    return result;
}

void* FugaMethod_addPattern(void* self, void* args, void* body)
{
    FUGA_NEED(self);
    FUGA_NEED(args);
    FUGA_NEED(body);
    FUGA_IF(Fuga_hasS(self, "args")) {
        FUGA_IF(Fuga_hasS(self, "body")) {
            FUGA_CHECK(Fuga_append_(Fuga_getS(self, "args"), args));
            FUGA_CHECK(Fuga_append_(Fuga_getS(self, "body"), body));
            return FUGA->nil;
        }
    }
    FUGA_RAISE(FUGA->TypeError,
        "def: Can't add pattern to built-in function."
    );
}

