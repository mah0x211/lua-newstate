-- codegen.lua expands var/*.txt constant lists into src/newstate.c
-- between GEN_<NAME>_DECL / <NAME>_END markers. Runs as a luarocks
-- build hook before compilation, without command-line arguments.

local VAR_FILES = {
    'var/errcode.txt',
    'var/gcwhat.txt',
}

local SRC_FILES = {
    'src/newstate.c',
}

local TMPL = [=[
#if defined(%s)
    lua_pushstring(L, "%s");
    lua_pushinteger(L, %s);
    lua_rawset(L, -3);
#endif
]=]
local decls = {}

-- load declaration
for _, path in ipairs(VAR_FILES) do
    local varname = path:match('/([%w_]+).txt$')
    if varname then
        local decl = ('GEN_%s_DECL'):format(varname:upper())
        local f = assert(io.open(path))
        local names = {}
        local defs = {}

        print('generate code for ' .. decl .. ' from ' .. path)
        for s in f:lines() do
            if not names[s] then
                local field = s:match('^LUA_(.+)$') or s

                names[s] = true
                if not s:match('^[A-Z0-9_]+$') then
                    error('invalid macro name: ' .. s)
                end
                print('export ' .. s .. ' as ' .. field)
                defs[#defs + 1] = TMPL:format(s, field, s)
            end
        end
        f:close()
        decls[#decls + 1] = {
            name = decl,
            head = '%s*//%s+' .. decl .. '%s*\n',
            tail = '%s*//%s*' .. decl .. '_END%s+',
            body = table.concat(defs, '\n'),
        }
    end
end

-- inject declaration
for _, path in ipairs(SRC_FILES) do
    if path:find('%.c$') then
        local f = assert(io.open(path))
        local src = f:read('*a')
        f:close()
        local len = #src

        for _, v in ipairs(decls) do
            local s, e = src:find(v.head)

            if s then
                local head = e

                s, e = src:find(v.tail)
                if s then
                    local tail = s
                    src = src:sub(1, head) .. v.body .. src:sub(tail)
                end
            end
        end

        if #src > len then
            local out = assert(io.open(path, 'w'))
            out:write(src)
            out:close()
        end
    end
end
