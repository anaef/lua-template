local template = require("template")

-- Test resolvers and template clearing
assert(template.render("test/test.txt", _G) == "Test\n")
template.clear()
template.setresolver(function (filename) return "Test2" end)
assert(type(template.getresolver()) == "function")
assert(template.render("test/test.txt", _G) == "Test2")
template.setresolver(nil)
assert(type(template.getresolver()) == "nil")
template.clear()
assert(template.render("test/test.txt", _G) == "Test\n")

-- Configure a table-based resolver
local TEMPLATES = {
	test_if = "<l:if cond=\"cond\">True</l:if>",
	test_if_else = "<l:if cond=\"cond\">True<l:else />False</l:if>",
	test_if_elseif = "<l:if cond=\"value == 1\">1<l:elseif cond=\"value == 2\"/>2</l:if>",
	test_if_elseif_else = "<l:if cond=\"value == 1\">1<l:elseif cond=\"value == 2\"/>2"
			.. "<l:else/>3</l:if>",
	test_for = "<l:for in=\"ipairs(values)\" names=\"_, value\">${value}</l:for>",
	test_set = "<l:set names=\"x\" expressions=\"value\"/>${x}",
	test_include = "include: <l:include filename=\"'test_if'\"/>",
	test_sub_nil = "${undefined}",
	test_sub_nilsup = "$[n]{undefined}",
	test_sub_xml = "$[x]{xml}",
	test_sub_url = "$[u]{url}",
	test_sub_js = "$[j]{js}",
}
template.setresolver(function (key) return TEMPLATES[key] end)

-- Tests a template
local function test (key, env, result)
	setmetatable(env, { __index = _G })
	assert(template.render(key, env) == result)
end

-- Test elements
test("test_if", { cond = false }, "")
test("test_if", { cond = true }, "True")
test("test_if_else", { cond = false }, "False")
test("test_if_else", { cond = true }, "True")
test("test_if_elseif", { value = 1 }, "1")
test("test_if_elseif", { value = 2 }, "2")
test("test_if_elseif", { value = 3 }, "")
test("test_if_elseif_else", { value = 1 }, "1")
test("test_if_elseif_else", { value = 2 }, "2")
test("test_if_elseif_else", { value = 3 }, "3")
test("test_for", { values = { 3, 2, 1 } }, "321")
test("test_set", { value = 42 }, "42")
test("test_include", { cond = true }, "include: True")

-- Test substitution
test("test_sub_nil", { }, "(nil)")
test("test_sub_nilsup", { }, "")
test("test_sub_xml", { xml = "<test>" }, "&lt;test&gt;")
test("test_sub_url", { url = "a/b?c" }, "a%2Fb%3Fc")
test("test_sub_js", { js = "'a'" }, "\\'a\\'")
