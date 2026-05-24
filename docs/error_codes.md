### Error codes

The `vexc` compiler command is designed to return standard error codes in UNIX convention.

##### Exit Code -> Meaning
- **0** → No error
- **64** → Incorrect usage (example, incorrect number of args)
- **65** → Data error (Lexer / Parser / Semantic errors)
- **66** → Input file didn't exist or was not readable
- **70** → Internal software error or bug
- **74** → I/O or filesystem error