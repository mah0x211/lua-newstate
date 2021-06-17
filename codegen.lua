local TMPL = [=[
#if defined(%s)
    lua_pushstring(L, "%s");
    lua_pushinteger(L, %s);
    lua_rawset(L, -3);
#endif
]=]
local decls = {}

-- load declaration
for _, path in ipairs(_G.arg) do
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
for _, path in ipairs(_G.arg) do
    if path:find('%.c$') then
        local src = assert(io.open(path)):read('*a')
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
            assert(io.open(path, 'w')):write(src)
        end
    end
end
