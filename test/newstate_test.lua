pcall(require, 'luacov')
local testcase = require('testcase')
local assert = require('assert')
local newstate = require('newstate')

function testcase.new_returns_state()
    local L = newstate.new()
    assert.not_nil(L)
    assert.match(tostring(L), '^newstate: 0x', false)
end

function testcase.new_with_openlibs_disabled()
    local L = newstate.new(false)
    assert.not_nil(L)
    local ok, has_io = L:dostring('return io ~= nil')
    assert.is_true(ok)
    assert.is_false(has_io)
end

function testcase.error_constants_exported()
    assert.is_number(newstate.ERRRUN)
    assert.is_number(newstate.ERRSYNTAX)
    assert.is_number(newstate.ERRMEM)
    assert.is_number(newstate.ERRERR)
    assert.is_number(newstate.ERRFILE)
end

function testcase.gc_constants_exported()
    assert.is_number(newstate.GCSTOP)
    assert.is_number(newstate.GCRESTART)
    assert.is_number(newstate.GCCOLLECT)
    assert.is_number(newstate.GCCOUNT)
    assert.is_number(newstate.GCCOUNTB)
    assert.is_number(newstate.GCSTEP)
    assert.is_number(newstate.GCSETPAUSE)
    assert.is_number(newstate.GCSETSTEPMUL)
end

function testcase.dostring_returns_true_on_success()
    local L = newstate.new()
    assert.is_true(L:dostring('return 1'))
end

function testcase.dostring_returns_value()
    local L = newstate.new()
    local ok, v = L:dostring('return 42')
    assert.is_true(ok)
    assert.equal(v, 42)
end

function testcase.dostring_passes_args()
    local L = newstate.new()
    local ok, a, b = L:dostring('return ...', 1, 2)
    assert.is_true(ok)
    assert.equal(a, 1)
    assert.equal(b, 2)
end

function testcase.dostring_passes_exchangeable_types()
    local L = newstate.new()
    assert.is_true(L:dostring('return ...', nil, true, 1.5, 'str'))
end

function testcase.dostring_rejects_function_arg()
    local L = newstate.new()
    local ok, err, rc = L:dostring('return ...', function()
    end)
    assert.is_false(ok)
    assert.is_string(err)
    assert.is_number(rc)
end

function testcase.dostring_reports_syntax_error()
    local L = newstate.new()
    local ok, err, rc = L:dostring('if then end')
    assert.is_false(ok)
    assert.is_string(err)
    assert.is_number(rc)
end

function testcase.dofile_runs_script()
    local L = newstate.new()
    local tmpfile = os.tmpname()
    local f = assert(io.open(tmpfile, 'w'))
    f:write('return "fromfile"')
    f:close()
    local ok, v = L:dofile(tmpfile)
    assert.is_true(ok)
    assert.equal(v, 'fromfile')
    os.remove(tmpfile)
end

function testcase.loadstring_then_run()
    local L = newstate.new()
    assert.is_true(L:loadstring('return ...'))
    local ran, a, b = L:run(10, 20)
    assert.is_true(ran)
    assert.equal(a, 10)
    assert.equal(b, 20)
end

function testcase.loadfile_then_run()
    local L = newstate.new()
    local tmpfile = os.tmpname()
    local f = assert(io.open(tmpfile, 'w'))
    f:write('return ...')
    f:close()
    assert.is_true(L:loadfile(tmpfile))
    local ran, v = L:run('hello')
    assert.is_true(ran)
    assert.equal(v, 'hello')
    os.remove(tmpfile)
end

function testcase.gc_count_returns_non_negative()
    local L = newstate.new()
    local bytes = L:gc(newstate.GCCOUNT)
    assert.is_number(bytes)
    assert.greater_or_equal(bytes, 0)
end

function testcase.gc_collect_returns_zero()
    local L = newstate.new()
    assert.equal(L:gc(newstate.GCCOLLECT), 0)
end

function testcase.gc_stop_restart()
    local L = newstate.new()
    L:gc(newstate.GCSTOP)
    L:gc(newstate.GCRESTART)
end
