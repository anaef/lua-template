# Lua Template Documentation

This page describes the template syntax and functions provided by Lua Template.


## Template Syntax

### Conditional Rendering

Syntax: `<l:if cond="exp">...<l:elseif cond="exp"/>...<l:else/>...</l:if>`

The `if`, `elseif`, and `else` elements support conditional rendering. Only the `if` element is
required; the `elseif` element is repeatable. Each expression *exp* is evaluated as a boolean
value following Lua semantics.

Example: `<l:if cond="a or b">a or b is true, possibly both</l:if>`


### Loops

Syntax: `<l:for names="namelist" in="explist">...</l:for>`

The `for` element supports loops. The *namelist* provides the names of the variables of the loop.
The *explist* must follow Lua *generic for* semantics and often involves a helper function, such as
`ipairs`.

Example: `<l:for names="_, v" in="ipairs(t)">${v}</l:for>`


### Assignment

Syntax: `<l:set names="namelist" expressions="explist"/>`

The `set` element assigns variables. The variables in *namelist* are assigned the expressions in
*explist*.

Example: `<l:set names="a, b" expressions="b, a"/>`


### Include

Syntax: `<l:include filename="exp"/>`

The `include` element includes another template. The template included is determined by
the expression *exp*.

Example: `<l:include filename="path .. '/subtemplate.html'"/>`


### Substitution

Syntax: `$[flags]{exp}`

The `$` operator supports the substitution of expressions. The expression *exp* must result in a
string value following Lua semantics. If the expression does not result in a string value, it is
converted to its type in round brackets, e.g., `(table)`.

The optional *flags* control the processing of the string result. The flags can contain one of
the letters `x`, `u`, or `j` to perform XML/HTML escaping, URL escaping, or JavaScript string
escaping, respectively. If the flags contain the letter `n`, a `nil` result is suppressed, i.e.,
it results in an empty string instead of `(nil)`. If `[flags]` is omitted, i.e., the `${exp}`
syntax is used, the substitution is processed as if the flags were given as `x`.

Examples: `$[nx]{name}`, `${string.upper(name)}`


## Functions

### `template.render (filename, env [, file])`

Renders the template identified by `filename` using `env` as the Lua environment for expressions
and variables. If the optional `file` argument is present, is must be a Lua file handle, and the
output of the rendering operation is streamed into it; in this case, the function returns no
result. If `file` is not present, the function returns the output of the rendering operation as a
string.


### `template.getresolver ()`

Returns the configured resolver function, or `nil` if none is set. Please see below for more
information on resolver functions.


### `template.setresolver (func)`

Sets `func` as the resolver function. When the content of a template must be resolved, the
function is called with the file name of the template as the sole argument. The function must
return the content of the template as a string value, or `nil` if the template is not present.

By default, no resolver function is set and templates are resolved via the file system. After
setting a resolver function, the default behavior can be restored by calling `setresolver` with a
`nil` argument.


### `template.clear ()`

Clears the cached templates. The library resolves each template file name only once, and then
stores an internal representation of the template for efficient rendering. The function clears the
cache of internal representations and causes templates to be resolved anew on next use.
