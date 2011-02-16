#include "parser.h"
#include "test.h"

struct FugaParser {
    Fuga* operators;
    FugaLexer* lexer;
};

FugaParser* FugaParser_new(
    Fuga* self
) {
    FugaParser* parser = FugaGC_alloc(self, sizeof(FugaParser));
    parser->operators = Fuga_clone(FUGA->Object);
    // FIXME: add default operators
    return parser;
}

void FugaParser_readCode_(
    FugaParser* parser,
    const char* code
) {
    parser->lexer = FugaLexer_new(parser);
    FugaLexer_readCode_(parser->lexer, code);
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

void FugaParser_infixl_precedence_(
    FugaParser* self,
    const char* op,
    size_t precedence
);

void FugaParser_infixr_precedence_(
    FugaParser* self,
    const char* op,
    size_t precedence
);

size_t FugaParser_precedence_(
    FugaParser* parser,
    const char* op
) {
    Fuga* self = parser->operators;
    Fuga* opdata = Fuga_get(self, FUGA_SYMBOL(op));
    if (!Fuga_isRaised(opdata)) {
        Fuga* precedence = Fuga_get(opdata, FUGA_SYMBOL("precedence"));
        precedence = Fuga_need(precedence);
        if (Fuga_isInt(precedence)) {
            long p = FugaInt_value(precedence);
            if (p > 1999) p = 1999;
            if (p < 1)    p = 1;
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
    case FUGA_TOKEN_ERROR:  return 9999;
    case FUGA_TOKEN_LPAREN: return 2000;
    case FUGA_TOKEN_INT:    return 2000;
    case FUGA_TOKEN_STRING: return 2000;
    case FUGA_TOKEN_SYMBOL: return 2000;
    case FUGA_TOKEN_NAME:   return 2000;
    case FUGA_TOKEN_OP:
        return FugaParser_precedence_(parser, token->value);
    // FIXME: handle FUGA_TOKEN_OP
    default: return 0;
    }
}

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
        if (FugaLexer_peek(parser->lexer)->type != FUGA_TOKEN_RPAREN)
            FUGA_RAISE(FUGA->SyntaxError, "expected )");
        FugaLexer_next(parser->lexer);
        return self;
    
    case FUGA_TOKEN_INT:    return FugaToken_int_(token, self);
    case FUGA_TOKEN_STRING: return FugaToken_string_(token, self);
    case FUGA_TOKEN_SYMBOL: return FugaToken_symbol_(token, self);

    case FUGA_TOKEN_NAME:
        self = FugaMsg_fromSymbol(FugaToken_symbol_(token, self));
        FUGA_CHECK(self);
        token = FugaLexer_peek(parser->lexer);
        if (token->type == FUGA_TOKEN_LPAREN) {
            FugaLexer_next(parser->lexer);
            Fuga* slots = _FugaParser_derive_(parser, token);
            FUGA_CHECK(slots);
            self->slots = slots->slots;
        }
        return self;

    case FUGA_TOKEN_OP:
        // FIXME: handle send-to-scope style operators.
        self = FugaMsg_fromSymbol(FugaToken_symbol_(token, self));
        FUGA_CHECK(self);
        Fuga* part = FugaParser_expression_(parser, 2000);
        return _FugaParser_buildExpr_(part, self);

    // FIXME: handle FUGA_TOKEN_OP

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
    // FIXME: use more descriptive error messages.
    Fuga* self = parser->operators;
    switch (token->type) {
    case FUGA_TOKEN_ERROR:
        FUGA_RAISE(FUGA->SyntaxError, "invalid token");
    
    case FUGA_TOKEN_NAME:
        self = _FugaParser_derive_(parser, token);
        return _FugaParser_buildExpr_(expr, self);

    // FIXME: handle FUGA_TOKEN_INT
    // FIXME: handle FUGA_TOKEN_OP

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
    FugaToken* token = FugaLexer_peek(parser->lexer);
    bool needsep = false;
    while (1) {
        while (token->type == FUGA_TOKEN_SEPARATOR) {
            token = FugaLexer_next(parser->lexer);
            token = FugaLexer_peek(parser->lexer);
            needsep = false;
        }
        
        if ((token->type == FUGA_TOKEN_RPAREN) ||
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
        token = FugaLexer_peek(parser->lexer);
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
    FUGA_PARSER_TEST("()", FugaInt_isEqualTo(Fuga_length(self), 0));
    FUGA_PARSER_TEST("(10)",
           FugaInt_isEqualTo(Fuga_length(self), 1)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10));
    FUGA_PARSER_TEST("(10,20)",
           FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(1)), 20));
    FUGA_PARSER_TEST("(\n\n\n10\n20\n\n:do,)",
           FugaInt_isEqualTo(Fuga_length(self), 3)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(1)), 20)
        && Fuga_is(FUGA_SYMBOL("do"), Fuga_get(self, FUGA_INT(2))));
    FUGA_PARSER_TEST("do()", Fuga_isMsg(self));
    FUGA_PARSER_TEST("do(re, mi)",
           Fuga_isMsg(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2));
    FUGA_PARSER_TEST("do re",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(0)))
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("do re mi",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 3)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(0)))
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1)))
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(2))));
    FUGA_PARSER_TEST(":do re",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && (FUGA_SYMBOL("do") == Fuga_get(self, FUGA_INT(0)))
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("-42",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 42)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("!42",
           !Fuga_isRaised(self)
        && Fuga_isMsg(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 42));
    FUGA_PARSER_TEST("10 + 20",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("10 = 20",
           !Fuga_isRaised(self)
        && Fuga_isMsg(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(1)), 20));
    FUGA_PARSER_TEST("10 +> 20",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 20)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1))));
    FUGA_PARSER_TEST("10 * 20 * 30",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 3)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1)))
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(2))));
    FUGA_PARSER_TEST("10 ** 20 ** 30",
           !Fuga_isRaised(self)
        && Fuga_isExpr(self)
        && FugaInt_isEqualTo(Fuga_length(self), 2)
        && FugaInt_isEqualTo(Fuga_get(self, FUGA_INT(0)), 10)
        && Fuga_isMsg(Fuga_get(self, FUGA_INT(1))));

    Fuga_quit(self);
}
#endif

