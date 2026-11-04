"""Execute the actual plugin under Lua 5.4 with a MA API test adapter.
Uses the installed shared Lua runtime, no additional packages required.
"""
import ctypes as C
import ctypes.util
import os
from pathlib import Path
root=Path(__file__).resolve().parent
from check_loop_controls import check
check(root.parent/'source'/'medialink_core.lua')
lib=C.CDLL(ctypes.util.find_library('lua5.4'))
lib.luaL_newstate.restype=C.c_void_p
lib.luaL_openlibs.argtypes=[C.c_void_p]
lib.luaL_loadfilex.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p]
lib.lua_pcallk.argtypes=[C.c_void_p,C.c_int,C.c_int,C.c_int,C.c_longlong,C.c_void_p]
lib.lua_tolstring.argtypes=[C.c_void_p,C.c_int,C.POINTER(C.c_size_t)];lib.lua_tolstring.restype=C.c_char_p
lib.lua_close.argtypes=[C.c_void_p]
os.chdir(root.parent)
state=lib.luaL_newstate();assert state
try:
    lib.luaL_openlibs(state)
    status=lib.luaL_loadfilex(state,b'tests/test_medialink.lua',None)
    if status==0:status=lib.lua_pcallk(state,0,-1,0,0,None)
    if status:raise SystemExit(lib.lua_tolstring(state,-1,None).decode())
finally:lib.lua_close(state)
