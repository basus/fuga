#include "token.h"
#include "parser.h"
#include "test.h"

struct FugaParser {
    Fuga* operators;
    FugaLexer* lexer;
};

void _FugaParser_mark(
    void *_parser
) {
    FugaParser* parser = _parser;
    FugaGC_mark(parser, parser->operators);
    FugaGC_mark(parser, parser->lexer);
}

FugaParser* FugaParser_new(
    Fuga* self
) {
    FugaParser* parser = FugaGC_alloc(self, sizeof(FugaParser));
    FugaGC_onMark(parser, _FugaParser_mark);
    parser->operators = Fuga_clone(FUGA->Object);

    // relational: 500s
    FugaParser_infix_precedence_(parser, "==", 500);
    FugaParser_infix_precedence_(parser, "<=", 500);
    FugaParser_infix_precedence_(parser, ">=", 500);
    FugaParser_infix_precedence_(parser, "<",  500);
    FugaParser_infix_precedence_(parser, ">",  500);
    // collections: 600s
    FugaParser_infix_precedence_(parser, "++", 600);
    // default: 1000s
    // arithmetic: 1500s
    FugaParser_infix_precedence_(parser, "+",  1500);
    FugaParser_infix_precedence_(parser, "-",  1500);
    FugaParser_infix_precedence_(parser, "*",  1510);
    FugaParser_infix_precedence_(parser, "/",  1510);
    FugaParser_infix_precedence_(parser, "//", 1510);
    FugaParser_infix_precedence_(parser, "%",  1510);
    FugaParser_infix_precedence_(parser, "**", 1520);

    return parser;
}

void FugaParser_readCode_(
    FugaParser* parser,
    const char* code
) {
    parser->lexer = FugaLexer_new(parser);
    FugaLexer_readCode_(parser->lexer, code);
}

bool FugaParser_readFile_(
    FugaParser* parser,
    const char* filename
) {
    parser->lexer = FugaLexer_new(parser);
    return FugaLexer_readFile_(parser->lexer, filename);
}

void FugaParser_parseTokens_(
    FugaParser* parser,
    FugaLexer* lexer
) {
    parser->lexer = lexer;
}

Fuga* FugaParser_operators(
    FugaParser* self
) {
    return self->operators;
}

void FugaParser_infix_precedence_(
    FugaParser* parser,
    const char* op,
    size_t precedence
) {
    Fuga* self = parser->operators;
    Fuga* opSymbol = FUGA_SYMBOL(op);
    Fuga* opData;
    Fuga* precedenceSymbol = FUGA_SYMBOL("precedence");
    if (Fuga_hasSlot(self, opSymbol)) {
        opData = Fuga_getSlot(self, opSymbol);
        opData = Fuga_need(opData);
        if (!Fuga_isRaised(opData)) {
            Fuga_setSlot(opData, precedenceSymbol, FUGA_INT(precedence));
            return;
        }
    } 
    opData = Fuga_clone(FUGA->Object);
    Fuga_setSlot(opData, precedenceSymbol, FUGA_INT(precedence));
    Fuga_setSlot(self, opSymbol, opData);
}

size_t FugaParser_precedence_(
    FugaParser* parser,
    const char* op
) {
    Fuga* self = parser->operators;
    Fuga* opdata = Fuga_getSlot(self, FUGA_SYMBOL(op));
    if (!Fuga_isRaised(opdata)) {
        Fuga* precedence = Fuga_getSlot(opdata, FUGA_SYMBOL("precedence"));
        precedence = Fuga_need(precedence);
        if (Fuga_isInt(precedence)) {
            long p = FugaInt_value(precedence);
            if (p > 1999) p = 1999;
            if (p < 2)    p = 2;
            return p;
        }
    }
    return 1000; // default precedence
}

/**
*** ### _FugaParser_lbp_
*** 
*** Get the left binding power of a given token (within a parser
*** context). This tells us which kinds of expressions the token can be
*** a part of.
**/
size_t _FugaParser_lbp_(
    FugaParser* parser,
    FugaToken* token
) {
    switch (token->type) {
    case FUGA_TOKEN_ERROR:    return 9999;
    case FUGA_TOKEN_LPAREN:   return 2000;
    case FUGA_TOKEN_LBRACKET: return 2000;
    case FUGA_TOKEN_LCURLY:   return 2000;
    case FUGA_TOKEN_INT:      return 2000;
    case FUGA_TOKEN_STRING:   return 2000;
    case FUGA_TOKEN_SYMBOL:   return 2000;
    case FUGA_TOKEN_NAME:     return 2000;
    case FUGA_TOKEN_EQUALS:   return 1;
    case FUGA_TOKEN_OP:
        return FugaParser_precedence_(parser, token->value);
    default: return 0;
    }
}

bool FugaParser_advance_(
    FugaParser* parser,
    FugaTokenType tokenType
) {
    ALWAYS(parser);
    if (tokenType == FugaLexer_peek(parser->lexer)->type) {
        FugaLexer_next(parser->lexer);
        return true;
    }
    return false;
}

Fuga* FugaParser_error_(
    FugaParser* parser,
    const char* message
) {
    ALWAYS(parser); ALWAYS(message);
    Fuga* self = parser->operators;
    ALWAYS(self);
    FUGA_RAISE(FUGA->SyntaxError, message);
}

#define FUGA_PARSER_EXPECT(parser, tokenType, name)             \
    do {                                                        \
        if (!FugaParser_advance_(parser, tokenType))            \
            return FugaParser_error_(parser, "expected " name); \
    } while(0)

Fuga* _FugaParser_buildExpr_(
    Fuga* self,
    Fuga* msg
) {
    FUGA_CHECK(self);
    FUGA_CHECK(msg);
    if (Fuga_isExpr(self)) {
        FUGA_CHECK(Fuga_append(self, msg));
        return self;
    } else { 
        Fuga* expr = Fuga_clone(FUGA->Expr);
        FUGA_CHECK(Fuga_append(expr, self));
        FUGA_CHECK(Fuga_append(expr, msg));
        return expr;
    }
}

Fuga* _FugaParser_buildMethodBody(
    Fuga* self
) {
    FUGA_NEED(self);
    if(Fuga_hasNumSlots(self, 1)) {
        return Fuga_getSlot(self, FUGA_INT(0));
    } else {
        Fuga* body = FUGA_MSG("do");
        body->slots = self->slots;
        return body;
    }
}

Fuga* _FugaParser_buildMethod_(
    Fuga* prev,
    Fuga* block
) {
    Fuga* self = block;
    FUGA_CHECK(self);

    Fuga* body = _FugaParser_buildMethodBody(block);
    Fuga* args = NULL;


    // separate args from prev.
    if (prev) {
        FUGA_CHECK(prev);
        if (Fuga_isExpr(prev)) {
            size_t last = FugaInt_value(Fuga_numSlots(prev)) - 1;
            Fuga* lastF = FUGA_INT(last);
            Fuga*  msg  = Fuga_getSlot(prev, lastF);
            args = FugaMsg_args(msg);
            Fuga* name = FugaMsg_fromSymbol(FugaMsg_name(msg));
            FUGA_CHECK(Fuga_setSlot(prev, lastF, name));
        } else if (Fuga_isMsg(prev)) {
            args = FugaMsg_args(prev);
            prev = FugaMsg_fromSymbol(FugaMsg_name(prev));
        } else {
            args = prev;
            prev = NULL;
        }
    } else {
        args = Fuga_clone(FUGA->Object);
    }

    Fuga* rhs = FUGA_MSG("method");
    FUGA_CHECK(Fuga_append(rhs, args));
    FUGA_CHECK(Fuga_append(rhs, body));

    if (prev) {
        Fuga* expr = FUGA_MSG("=");
        FUGA_CHECK(Fuga_append(expr, prev));
        FUGA_CHECK(Fuga_append(expr, rhs));
        return expr;
    } else {
        return rhs;
    }
}

/**
*** ### _FugaParser_derive_
*** 
*** Derive an expression or a value that begins with the given token.
**/
Fuga* _FugaParser_derive_(
    FugaParser* parser,
    FugaToken* token
) {
    // FIXME: use more descriptive error messages.
    Fuga* self = parser->operators;
    switch (token->type) {
    case FUGA_TOKEN_ERROR:
        FUGA_RAISE(FUGA->SyntaxError, "invalid token");
    
    case FUGA_TOKEN_LPAREN:
        self = FugaParser_block(parser);
        FUGA_CHECK(self);
        FUGA_PARSER_EXPECT(parser, FUGA_TOKEN_RPAREN, ")");
        return self;

    case FUGA_TOKEN_LBRACKET:
        self = FugaParser_expression(parser);
        FUGA_CHECK(self);
        FUGA_PARSER_EXPECT(parser, FUGA_TOKEN_RBRACKET, "]");
        return self;

    case FUGA_TOKEN_LCURLY:
        self = FugaParser_block(parser);
        FUGA_CHECK(self);
        FUGA_PARSER_EXPECT(parser, FUGA_TOKEN_RCURLY, "}");
        return _FugaParser_buildMethod_(NULL, self);

    case FUGA_TOKEN_INT:    return FugaToken_int_(token, self);
    case FUGA_TOKEN_STRING: return FugaToken_string_(token, self);
    case FUGA_TOKEN_SYMBOL: return FugaToken_symbol_(token, self);

    case FUGA_TOKEN_NAME:
        self = FugaMsg_fromSymbol(FugaToken_symbol_(token, self));
        FUGA_CHECK(self);
        if (FugaParser_advance_(parser, FUGA_TOKEN_LPAREN)) {
            Fuga* slots = FugaParser_block(parser);
            FUGA_CHECK(slots);
            FUGA_PARSER_EXPECT(parser, FUGA_TOKEN_RPAREN, ")");
            self->slots = slots->slots;
        }
        return self;

    case FUGA_TOKEN_OP:
        self = FugaMsg_fromSymbol(FugaToken_symbol_(token, self));
        FUGA_CHECK(self);
        Fuga* part = FugaParser_expression_(parser, 2000);
        return _FugaParser_buildExpr_(part, self);

    default:
        FUGA_RAISE(FUGA->SyntaxError, "invalid syntax");
    }

}

/**
*** ### _FugaParser_derive_after_
*** 
*** Derive the continuation of an expression or value.
**/
Fuga* _FugaParser_derive_after_(
    FugaParser* parser,
    FugaToken* token,
    Fuga* expr
) {
    Fuga* self = parser->operators;
    Fuga* name;
    Fuga* arg;
    Fuga* msg;
    switch (token->type) {
    case FUGA_TOKEN_ERROR:
        FUGA_RAISE(FUGA->SyntaxError, "invalid token");
    
    case FUGA_TOKEN_NAME:
        self = _FugaParser_derive_(parser, token);
        return _FugaParser_buildExpr_(expr, self);

    case FUGA_TOKEN_EQUALS:
        msg = FUGA_MSG("=");
        Fuga_append(msg, expr);
        Fuga_append(msg, FugaParser_expression_(parser, 1));
        return msg;

    case FUGA_TOKEN_OP:
        name = FugaToken_symbol_(token, self);
        arg  = FugaParser_expression_(parser,
            FugaParser_precedence_(parser, token->value));
        msg  = FugaMsg_fromSymbol(name);
        Fuga_append(msg, arg);
        return _FugaParser_buildExpr_(expr, msg);

    case FUGA_TOKEN_LCURLY:
        self = FugaParser_block(parser);
        FUGA_CHECK(self);
        FUGA_PARSER_EXPECT(parser, FUGA_TOKEN_RCURLY, "}");
        return _FugaParser_buildMethod_(expr, self);

    default:
        FUGA_RAISE(FUGA->SyntaxError, "invalid syntax");
    }
}

/**
*** ### FugaParser_expression_
*** 
*** Get the next expression in the stream.
*** 
*** - Params:
***     - FugaParser* parser
***     - size_t rbp: the right binding power. Determines which kinds of
***     tokens we consider to be part of our expression.
*** - Return: the expression as a Fuga object.
**/
Fuga* FugaParser_expression_(
    FugaParser* parser,
    size_t rbp
) {
    ALWAYS(parser);
    ALWAYS(parser->lexer);

    FugaToken* token = FugaLexer_next(parser->lexer);
    Fuga* self = _FugaParser_derive_(parser, token);
    FUGA_CHECK(self);
    token = FugaLexer_peek(parser->lexer);
    while (rbp < _FugaParser_lbp_(parser, token)) {
        token = FugaLexer_next(parser->lexer);
        self = _FugaParser_derive_after_(parser, token, self);
        FUGA_CHECK(self);
        token = FugaLexer_peek(parser->lexer);
    }
    if (token->type == FUGA_TOKEN_ERROR)
        FUGA_RAISE(FUGA->SyntaxError, "invalid token");
    return self;
}

Fuga* FugaParser_expression(
    FugaParser* parser
) {
    return FugaParser_expression_(parser, 0);
}

Fuga* FugaParser_block(
    FugaParser* parser
) {
    Fuga* self = Fuga_clone(parser->operators->root->Object);
    FugaToken* token;
    bool needsep = false;
    while (1) {
        if (FugaParser_advance_(parser, FUGA_TOKEN_SEPARATOR))
            needsep = false;
        while (FugaParser_advance_(parser, FUGA_TOKEN_SEPARATOR));

        token = FugaLexer_peek(parser->lexer);
        if ((token->type == FUGA_TOKEN_RPAREN) ||
            (token->type == FUGA_TOKEN_RCURLY) ||
            (token->type == FUGA_TOKEN_END))
            break;
        
        if (needsep)
            FUGA_RAISE(FUGA->SyntaxError,
                "expected separator between slots"
            );

        Fuga* slot = FugaParser_expression(parser);
        FUGA_CHECK(slot);
        Fuga_append(self, slot);
        needsep = true;
    }
    return self;
}

#ifdef TESTING

#define FUGA_PARSER_TEST(code,exp) do{          \
        FugaParser_readCode_(parser, code);     \
        Fuga* self##__LINE__ = self;            \
        self = FugaParser_expression(parser);   \
        TEST(exp);                              \
        self = self##__LINE__;                  \
    } while(0)

TESTS(FugaParser) {
    Fuga* self = Fuga_init();
    FugaParser* parser = FugaParser_new(self);
    
    FUGA_PARSER_TEST("",  Fuga_isRaised(self));
    FUGA_PARSER_TEST("1", FugaInt_isEqualTo(self, 1));
    FUGA_PARSER_TEST("\"Hello World!\"", Fuga_isString(self));
    FUGA_PARSER_TEST(":doremi", Fuga_isSymbol(self));
    FUGA_PARSER_TEST("doremi", Fuga_isMsg(self));
    FUGA_PARSER_TEST("()", Fuga_hasNumSlots(self, 0));
    FUGA_PARSER_TEST("(10)",
           Fuga_hasNumSlots(self, 1)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10));
    FUGA_PARSER_TEST("(10,20)",
           Fuga_hasNumSlots(self, 2)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 20));
    FUGA_PARSER_TEST("(\n\n\n10\n20\n\n:do,)",
           Fuga_hasNumSlots(self, 3)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 20)
        && Fuga_is(FUGA_SYMBOL("do"), Fuga_getSlot(self, FUGA_INT(2))));
    FUGA_PARSER_TEST("do()", Fuga_isMsg(self));
    FUGA_PARSER_TEST("do(re, mi)",
           Fuga_isMsg(self)
        && Fuga_hasNumSlots(self, 2));
    FUGA_PARSER_TEST("do re",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 2)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(0)))
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("do re mi",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 3)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(0)))
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(2))));
    FUGA_PARSER_TEST(":do re",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 2)
        && (FUGA_SYMBOL("do") == Fuga_getSlot(self, FUGA_INT(0)))
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("-42",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 2)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 42)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("10 + 20",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 2)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("10 = 20",
           !Fuga_isRaised(self)
        && Fuga_isMsg(self)
        && Fuga_hasNumSlots(self, 2)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 20));
    FUGA_PARSER_TEST("10 * 20 * 30",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 3)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(2))));
    FUGA_PARSER_TEST("10 + 20 * 30",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 2)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("[10 + 20] * 30",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && Fuga_hasNumSlots(self, 3)
        && FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        && Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("[10, 20]", Fuga_isRaised(self));

    FUGA_PARSER_TEST("{10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 0)
        &&  FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 10));
    FUGA_PARSER_TEST("{}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 0)
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(1)), 0));
    FUGA_PARSER_TEST("{10, 20}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 0)
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(1)), 2));
    FUGA_PARSER_TEST("() {10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 0)
        &&  FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 10));
    FUGA_PARSER_TEST("(a) {10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 1)
        &&  FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 10));
    FUGA_PARSER_TEST("(a, b) {10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 2)
        &&  FugaInt_isEqualTo(Fuga_getSlot(self, FUGA_INT(1)), 10));
    FUGA_PARSER_TEST("foo {10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(0)))
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        &&  Fuga_hasNumSlots(
                Fuga_getSlot(Fuga_getSlot(self, FUGA_INT(1)),
                             FUGA_INT(0)),
                0
            )
        &&  FugaInt_isEqualTo(
                Fuga_getSlot(Fuga_getSlot(self, FUGA_INT(1)),
                             FUGA_INT(1)),
                10
            )
        );
    FUGA_PARSER_TEST("foo bar {10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_isExpr(Fuga_getSlot(self, FUGA_INT(0)))
        &&  Fuga_hasNumSlots(Fuga_getSlot(self, FUGA_INT(0)), 2)
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        &&  Fuga_hasNumSlots(
                Fuga_getSlot(Fuga_getSlot(self, FUGA_INT(1)),
                             FUGA_INT(0)),
                0
            )
        &&  FugaInt_isEqualTo(
                Fuga_getSlot(Fuga_getSlot(self, FUGA_INT(1)),
                             FUGA_INT(1)),
                10
            )
        );
    FUGA_PARSER_TEST("foo (bar, baz) {10}",
           !Fuga_isRaised(self)
        &&  Fuga_isMsg(self)
        &&  Fuga_hasNumSlots(self, 2)
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(0)))
        &&  Fuga_isMsg(Fuga_getSlot(self, FUGA_INT(1)))
        &&  Fuga_hasNumSlots(
                Fuga_getSlot(Fuga_getSlot(self, FUGA_INT(1)),
                             FUGA_INT(0)),
                2
            )
        &&  FugaInt_isEqualTo(
                Fuga_getSlot(Fuga_getSlot(self, FUGA_INT(1)),
                             FUGA_INT(1)),
                10
            )
        );

    Fuga_quit(self);
}
#endif

