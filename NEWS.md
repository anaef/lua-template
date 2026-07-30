# Lua Template Release Notes


## Release 1.1.0

- Add Lua 5.5 to the tested versions.
- Add security posture.
- Fix URL escaping of non-ASCII bytes.
- Fix environment restoration during re-entrant template rendering.
- Fix handling of closed output streams.
- Fix memory-stream cleanup on error path.
- Fix substitution of strings containing embedded NUL bytes.
- Harden default template resolution against invalid or unrepresentable file sizes.
- Harden list capacity rounding.
- Harden table options, hashing, deleted-entry probing, and duplicate-key insertion.


## Release 1.0.0 (2024-04-06)

- Initial public release.
