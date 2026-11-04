"""Reject writes to for-loop control variables using Lua 5.4's const checker.

Newer/stricter Lua compilers make the first variable of each for loop const.
Insert an equivalent const shadow at the start of every loop body for this
compile-only check. The shipped code is not transformed. This also checks
assignments inside nested closures, without pattern-matching assignments.
"""
import ctypes as C
import ctypes.util
from pathlib import Path
import re


def tokens(source):
    """Only identifiers/punctuation matter; skip comments and Lua string forms."""
    pos = 0
    while pos < len(source):
        if source[pos].isspace():
            pos += 1
            continue
        comment = source.startswith('--', pos)
        start = pos + 2 if comment else pos
        long = re.match(r'\[(=*)\[', source[start:])
        if long:
            close = ']' + long[1] + ']'
            end = source.find(close, start + len(long[0]))
            assert end >= 0, 'Unterminated long string/comment'
            pos = end + len(close)
            continue
        if comment:
            end = source.find('\n', pos)
            pos = len(source) if end < 0 else end + 1
            continue
        if source[pos] in "\"'":
            quote = source[pos]
            pos += 1
            while pos < len(source):
                if source[pos] == '\\':
                    pos += 2
                elif source[pos] == quote:
                    pos += 1
                    break
                else:
                    pos += 1
            continue
        name = re.match(r'[A-Za-z_][A-Za-z_0-9]*', source[pos:])
        size = len(name[0]) if name else 1
        yield source[pos:pos + size], pos, pos + size
        pos += size


def strict_source(source):
    parsed = list(tokens(source))
    edits = []
    for index, (token, _, _) in enumerate(parsed):
        if token != 'for':
            continue
        name = parsed[index + 1][0]
        assert re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', name)
        for keyword, _, end in parsed[index + 2:]:
            # Loop headers in this project have no function literals.
            # Fail explicitly if that changes instead of silently skipping one.
            assert keyword != 'function', 'Unsupported function literal in loop header'
            if keyword == 'do':
                edits.append((end, ' local ' + name + ' <const> = ' + name + '; '))
                break
        else:
            raise AssertionError('Loop body not found')
    for offset, text in reversed(edits):
        source = source[:offset] + text + source[offset:]
    return source, len(edits)


def check(path):
    lib = C.CDLL(ctypes.util.find_library('lua5.4'))
    lib.luaL_newstate.restype = C.c_void_p
    lib.luaL_loadstring.argtypes = [C.c_void_p, C.c_char_p]
    lib.lua_tolstring.argtypes = [C.c_void_p, C.c_int, C.POINTER(C.c_size_t)]
    lib.lua_tolstring.restype = C.c_char_p
    lib.lua_close.argtypes = [C.c_void_p]
    state = lib.luaL_newstate()
    assert state

    def compile_only(text):
        # Compile in a fresh state; no plugin code or MA API is executed.
        item = lib.luaL_newstate()
        assert item
        try:
            code = lib.luaL_loadstring(item, text.encode())
            return lib.lua_tolstring(item, -1, None).decode() if code else None
        finally:
            lib.lua_close(item)

    try:
        bad = "for line in ('note\\n'):gmatch('(.-)\\n') do line=line:gsub('\\r$','') end"
        error = compile_only(strict_source(bad)[0])
        assert error and "attempt to assign to const variable 'line'" in error
        good = "for raw_line in ('note\\n'):gmatch('(.-)\\n') do local line=raw_line:gsub('\\r$','') end"
        assert compile_only(strict_source(good)[0]) is None
        transformed, count = strict_source(path.read_text(encoding='utf-8'))
        error = compile_only(transformed)
        assert error is None, error
        print(f'PASS: strict control-variable compile check ({count} loops); reported const-line failure reproduced and fixed')
    finally:
        lib.lua_close(state)


if __name__ == '__main__':
    import sys
    check(Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / 'source' / 'medialink_core.lua')
