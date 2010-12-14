from doctest import testmod
from peg import PEG

from prelude.core import *

class _parser(PEG):
    m_start = 'Module'

    # Hierarchical
    m_Module       = 'Spacing Block EndOfFile'
    m_Block        = 'Slot? (SEPARATOR+ Slot)* SEPARATOR*'
    m_Slot         = 'Slot_EQUALS / Slot_BECOMES / Expr'
    m_Slot_EQUALS  = 'Name+ EQUALS Expr'
    m_Slot_BECOMES = 'Name+ Object? BECOMES Expr'
    m_Expr         = 'Part' # a simplification for now
    m_Part         = 'PrefixOp* Root Msg*'
    m_Root         = 'PExpr / Object / Int / String / Msg / Symbol'
    m_PExpr        = 'LBRACKET Expr RBRACKET'
    m_Object       = 'LPAREN Block RPAREN'
    m_Msg          = 'Name Object? / DeceptiveOp Object?'

    def Module(self,v): return v[1]
    def Block(self,v):
        result = Object.clone()
        slots  = [v[0]] if v[0] is not None else []
        slots.extend(w[1] for w in v[1])
        for slot in slots:
            if isinstance(slot, tuple):
                result.set(slot[0], slot[1])
            else:
                result.append(slot)
        return result

    def Slot_EQUALS(self,v):
        if len(v[0]) == 1:
            return v[0][-1], v[2]
        msg = fgmsg('set', fgsym(v[0][-1]), v[2])
        msgs = list(map(fgmsg, v[0][:-1]))
        msgs.append(msg)
        return fgexpr(*msgs)

    def Slot_BECOMES(self, v):
        args = Object.clone() if v[1] is None else v[1]
        method = fgmsg('method', args, v[3])
        w = [v[0], '=', method]
        return self.Slot_EQUALS(w)

    def Part(self, v):
        if v[0]:
            return self.Part((
                v[0][:-1],
                fgmsg(v[0][-1], v[1]),
                v[2]
            ))
        if len(v[2]):
            return fgexpr(v[1], *v[2])
        return v[1]

    def Msg(self, v):
        result = fgmsg(v[0])
        if v[1] is not None:
            result._slots = v[1]._slots
            result._length = v[1]._length
        return result

    def PExpr(self, v): return v[1]
    def Object(self, v): return v[1]


    # Lexical
    m_LBRACKET     = '"[" LineSpacing'
    m_RBRACKET     = 'LineSpacing "]" Spacing'
    m_LPAREN       = '"(" Spacing'
    m_RPAREN       = '")" Spacing'
    m_SEPARATOR    = '[,\r\n] LineSpacing'
    m_EQUALS       = '"=" !RawOp LineSpacing'
    m_BECOMES      = '"=>" !RawOp LineSpacing'
    m_SYMPREFIX    = '":" !RawOp Spacing'
    
    m_Op           = '!EQUALS !BECOMES !SYMPREFIX RawOp LineSpacing'
    m_RawOp        = r'[`~!@$%^&*\-+=|:;.<>/?]+'
    m_PrefixOp     = '!DeceptiveOp Op'
    m_DeceptiveOp  = '&(RawOp "(") Op'
    
    m_Symbol       = 'SYMPREFIX RawName Spacing'
    m_Name         = '!Int RawName Spacing'
    m_Int          = 'RawInt !RawName Spacing'
    m_RawName      = r'[a-zA-Z0-9_][a-zA-Z0-9_?!]* / "\\" RawOp'

    def Op(self,v): return ''.join(v[3])
    def PrefixOp(self, v): return v[1]
    def DeceptiveOp(self, v): return v[1]
    def Symbol(self,v): return fgsym(v[1])
    def Name(self,v): return v[1]
    def Int(self,v): return v[0]
    def RawName(self,v): return v[0] + ''.join(v[1])

    m_RawInt       = 'BinInt / HexInt / DecInt'
    m_BinInt       = '"0b" [01]+'
    m_DecInt       = '"0d"? [0-9]+'
    m_HexInt       = '"0x" [0-9a-fA-F]+'

    def BinInt(self,v):
        total = 0
        digits = {'0':0, '1':1}
        base   = 2
        for c in v[1]:
            total *= base
            total += digits[c]
        return fgint(total)

    def HexInt(self,v):
        total = 0
        digits = {'0':0, '1':1, '2':2, '3':3, '4':4,
                  '5':5, '6':6, '7':7, '8':8, '9':9,
                  'a':10, 'b':11, 'c':12, 'd':13, 'e':14, 'f':15,
                  'A':10, 'B':11, 'C':12, 'D':13, 'E':14, 'F':15}
        base = 16
        for c in v[1]:
            total *= base
            total += digits[c]
        return fgint(total)

    def DecInt(self,v):
        total = 0
        digits = {'0':0, '1':1, '2':2, '3':3, '4':4,
                  '5':5, '6':6, '7':7, '8':8, '9':9}
        base = 10
        for c in v[1]:
            total *= base
            total += digits[c]
        return fgint(total)

    m_String       = 'StringFrag+'
    m_StringFrag   = r'"\"" (![\n\"] Char)* "\"" Spacing'
    m_Char         = r'"\\"? .'

    def String(self, v):
        return fgstr(''.join(v))

    def StringFrag(self, v):
        return ''.join([w[1] for w in v[1]])
        
    def Char(self, v):
        repls = {'n': '\n', 'r': '\r', 't': '\t'}
        if v[0] is not None:
            return repls.get(v[1], v[1])
        else:
            return v[1]

    m_Comment      = '"#" (!"\n" .)*'
    m_EscapedLine  = r'"\\" RawSpacing* [\r\n]+'
    m_RawSpacing   = '[ \t]+ / Comment'
    m_Spacing      = '(EscapedLine / RawSpacing)*'
    m_LineSpacing  = '(EscapedLine / [ \t\r\n]+ / Comment)*'
    m_EndOfFile    = '!.'

parser = _parser()

def parse(code):
    r"""
    >>> parse('')
    ()
    >>> parse('soprano')
    (soprano)
    >>> parse('soprano=alto')
    (soprano=alto)
    >>> parse('soprano alto=tenor')
    (soprano set(:alto, tenor))
    >>> parse('soprano => alto')
    (soprano=method((), alto))
    >>> parse('soprano alto => tenor')
    (soprano set(:alto, method((), tenor)))
    >>> parse('soprano alto tenor bass')
    (soprano alto tenor bass)
    >>> parse('+soprano')
    (+(soprano))
    >>> parse('- -soprano')
    (-(-(soprano)))
    >>> parse('+soprano alto')
    (+(soprano) alto)
    >>> parse('()')
    (())
    >>> parse('(soprano)')
    ((soprano))
    >>> parse('(soprano, alto\ntenor\tbass)')
    ((soprano, alto, tenor bass))
    >>> parse('[soprano]')
    (soprano)
    >>> parse('+[soprano alto]')
    (+(soprano alto))
    >>> parse('foo?!?!')
    (foo?!?!)
    >>> parse('soprano(alto)')
    (soprano(alto))
    >>> parse('soprano(alto, tenor(bass))')
    (soprano(alto, tenor(bass)))
    >>> parse('+(soprano)')
    (+(soprano))
    >>> parse('10')
    (10)
    >>> parse('0xFF')
    (255)
    >>> parse('0b10')
    (2)
    >>> parse('"hello"')
    ("hello")
    >>> parse(r'"world\""')
    ("world\"")
    >>> parse(r'"\n\t\r\\\""')
    ("\n\t\r\\\"")
    >>> parse('"soprano" "alto"')
    ("sopranoalto")
    >>> parse('foo #bar\nbaz')
    (foo, baz)
    >>> parse('foo \\#bar\nbaz')
    (foo baz)
    >>> parse(':hello')
    (:hello)
    """
    return parser.parse(code)

if __name__ == '__main__':
    testmod()

