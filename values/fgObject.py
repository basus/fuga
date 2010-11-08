from common import *

def Object_get(self, args, env):
    if len(args.slots) != 1:
        raise FugaError, "get expects 1 argument"
    args = env.eval(args)
    name = args.slots[0][1]
    if name.isA(String): name = name.value()
    elif name.isA(Message): name = name.value()
    elif name.isA(Int): name = name.value()
    else: raise FugaError, "unexpected type for argument to get"
    return self.get(name)

def Object_set(self, args, env):
    if len(args.slots) != 1 and len(args.slots) != 2:
        raise FugaError, "set expects 1 or 2 arguments"
    args = env.eval(args)
    if len(args.slots) == 1:
        name = None
        value = args.slots[0][1]
    else:
        name = args.slots[0][1]
        value = args.slots[1][1]
        if name.isA(String): name = name.value()
        elif name.isA(Message): name = name.value()
        else: raise FugaError, "unexpected type for first argument to set" 
    self.set(name, value)
    return value

def Object_has(self, args, env):
    if len(args.slots) != 1:
        raise FugaError, "get expects 1 argument"
    args = env.eval(args)
    name = args.slots[0][1]
    if name.isA(String): name = name.value()
    elif name.isA(Message): name = name.value()
    else: raise FugaError, "unexpected type for argument to get"
    return fgtrue if self.has(name) else fgfalse

def Object_proto(self, args, env):
    if len(args.slots) != 0:
        raise FugaError, "proto expects no arguments"
    return self.proto or fgnull

# Set up some globals

Object.set('Object', Object)
Object.set('Number', Number)
Object.set('Int',    Int)
Object.set('Real',   Real)
Object.set('String', String)
Object.set('List',   List)
Object.set('Bool',   Bool)
Object.set('true',   fgtrue)
Object.set('false',  fgfalse)
Object.set('get',    fgmethod(Object_get))
Object.set('set',    fgmethod(Object_set))
Object.set('has',    fgmethod(Object_has))
Object.set('proto',  fgmethod(Object_proto))

# Some Important Methods!

def Object_method(self, args, env):
    result = Method.clone(('scope', self))
    index = 0
    while index < len(args.slots):
        name  = args.slots[index][0]
        value = args.slots[index][1]
        if name:
            result.set(name, env.eval(value))
            index += 1
        elif value.isA(Message) and value.value() == 'method':
            result.set(None, self.eval(value))
            index += 1
        else:
            result.set(None, value)
            result.set(None, args.slots[index+1][1])
            index += 2
    return result
Object.set("method", fgmethod(Object_method))

def Object_clone(self, args, env):
    return self.clone(*env.eval(args).slots)
Object.set("clone", fgmethod(Object_clone))

def Object_eval(self, args, env):
    if len(args.slots) < 1 or len(args.slots) > 2:
        raise FugaError, "eval expects 1 or 2 arguments"
    args = env.eval(args)
    if len(args.slots) == 1:
        return self.eval(args.get(0))
    else:
        return self.eval(args.get(0), self, args.get(1))
Object.set("eval", fgmethod(Object_eval))

# Some Control Flow

def Object_do(self, args, env):
    if len(args.slots) == 0:
        raise FugaError, "do expects at least 1 argument"
    args = env.eval(args)
    return args.get(len(args.slots)-1)

def Object_if(self, args, env):
    if len(args.slots) !=  3:
        raise FugaError, "if expects 3 arguments exactly"
    value = env.eval(args.get(0))
    if value is fgtrue:
        return env.eval(args.get(1))
    elif value is fgfalse:
        return env.eval(args.get(2))
    else:
        raise FugaError, "if expects a Bool, not %s" % value

Object.set("do", fgmethod(Object_do))
Object.set("if", fgmethod(Object_if))

# IO
def Object_print(self, args, env):
    def _str(v):
        if v.isA(String):
            return v.value()
        else:
            return repr(v)
    args = env.eval(args)
    print ' '.join(_str(val) for (n, val) in args.slots)
    return fgvoid
Object.set('print', fgmethod(Object_print))
