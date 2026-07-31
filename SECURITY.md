# Security

## Security Posture

Lua Template trusts callers, template sources, the Lua runtime, and capabilities exposed by render
environments. Templates execute Lua expressions with those capabilities. Applications are
responsible for sandboxing, access control, process isolation, resource limits, and
context-appropriate escaping. Issues requiring these assumptions to be violated are outside the
project's security scope, but may remain robustness or correctness issues.
