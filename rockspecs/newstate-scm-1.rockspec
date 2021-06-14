package = "newstate"
version = "scm-1"
source = {
    url = "git://github.com/mah0x211/lua-newstate.git"
}
description = {
    summary = "newstate module to run scripts in the new lua_State.",
    homepage = "https://github.com/mah0x211/lua-newstate",
    license = "MIT/X11",
    maintainer = "Masatoshi Fukunaga"
}
dependencies = {
    "lua >= 5.1",
}
build = {
    type = "builtin",
    modules = {
        newstate = {
            sources = { "src/newstate.c" },
        },
    }
}
