# Lua Template 


## Introduction

Lua Template is an efficient template engine with support for conditional rendering, loops,
includes, and substitutions with escaping.

Here is a quick example.

Template:

```html
<p>Hello, ${name}.</p>

<l:if cond="hour < 12"><p>It is morning.</p><l:else/><p>It is afternoon.</l:if>

<p>Here are your TODOs:</p>
<ul>
	<l:for names="_, todo" in="ipairs(todos)">
	<li><a href="/todos/$[u]{todo.id}">${todo.text}</a></li></l:for>
</ul>
```

Code:

```lua
local template = require("template")

-- Prepare substitutions
local env = setmetatable({
	name = "Peter",
	hour = 8,
	todos = {
		{ id = 1, name = "TODO #1" },
		{ id = 2, name = "TODO #2" },
		{ id = 3, name = "TODO #3" },
	}
}, { __index = _G })

-- Render to the standard output
template.render("template.html", env, io.stdout)
```

Output:

```html
<p>Hello, Peter.</p>

<p>It is morning.</p>

<p>Here are your TODOs:</p>
<ul>

        <li><a href="/todos/1">TODO #1</a></li>
        <li><a href="/todos/2">TODO #2</a></li>
        <li><a href="/todos/3">TODO #3</a></li>
</ul>
```


## Build, Test, and Install

### Building, Testing and Installing with Make

Lua Template comes with a simple Makefile. Please adapt the Makefile to your environment, and then
run:

```
make
make test
make install
```

## Release Notes

Please see the [release notes](NEWS.md) document.


## Documentation

Please see the [documentation](doc/) folder.


## Limitations

Lua Template supports Lua 5.3 and Lua 5.4.

Lua Template has been built and tested on Ubuntu Linux (64-bit).


## License

Lua Template is released under the MIT license. See LICENSE for license terms.
