### Vex Syntax [Work in Progress]

Vex is a small statically typed procedural language built for compiler and IR experiments.

## Current language shape

- Explicit declarations
- Semicolon-terminated statements
- Block scopes with `{ ... }`
- `if` / `elif` / `else`
- `while` and `for`
- Functions with fixed typed parameters
- `i32` and `bool`
- Arrays
- Built-in `read`, `print`, `println`, `exit`

## Example

```c
fun fib(i32 n) -> i32 {
    if n <= 1:
        return n;
    else: {
        i32 a = fib(n - 1), b = fib(n - 2);
        return a + b;
    }
}

for i in [1..15]:
    println(fib(i));
```

## Types

- `i32`
- `bool`
- `nil` for no-return functions

Arrays are homogeneous and fixed-size in the current language.

## Operators

- Unary: `-`, `!`
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `>`, `>=`, `<`, `<=`, `==`, `!=`
- Logical: `&&`, `||`

## Control flow

```python
if cond: {
    ...
}
elif other_cond: {
    ...
}
else: {
    ...
}
```

```c
while cond: {
    ...
}
```

```python
for x in arr:
    ...
```

```python
for x in [1..10]:
    ...
```

`break` and `continue` are supported.

## Functions

```c
fun sum_2(i32 a, i32 b) -> i32 {
    i32 total = a + b;
    return total;
}
```

## Arrays

```c
i32[] arr1 = [1, 2, 3];
i32[] arr2 = [1..10];
```

```c
fun sum_4(i32[] arr, i32 n) -> i32 {
    if n < 4: exit(1);
    i32 total = arr[0] + arr[1];
    total = total + arr[2];
    total = total + arr[3];
    return total;
}
```

## I/O

```c
read(a, b);
print(a, b);
println(a, b);
```

String literals are currently meant for `print` / `println` style output.

## What is already done

- Lexing
- Parsing
- Semantic analysis
- Typed AST generation
- Lowering to SSA-style VexIR
