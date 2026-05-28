### VexIR [Work in Progress]

VexIR is the custom SSA-style IR used between semantic analysis and later backend lowering.

## Current shape

- Explicit basic blocks with `BLOCK`
- SSA temporaries like `t1`, `t2`, `t3`
- Explicit control flow with `JUMP`, `BRANCH`, `RETURN`, `RETURN_VOID`, `EXIT`
- `PHI` nodes at block tops for merges and loop headers
- Explicit array and address operations
- Explicit built-in I/O operations
- A small optimized vector IR surface for the first SLP pass

## Function form

```text
FUNCTION (func_name)
PARAM p1
PARAM p2
BLOCK entry
...
FUNC_END
```

## Core instructions

### Constants

- `CONST_I32 t1 (number)`
- `CONST_BOOL t1 (true/false)`
- `CONST_STR t1 (string value)`

### Memory

- `ALLOC_LOCAL x1`
- `STORE x1 t1`
- `LOAD t1 x1`

### Arrays / addresses

- `ALLOC_ARRAY a1 5`
- `ELEM_ADDR t2 a1 t1`
- `ADDR_OF t3 a1`

### Unary

- `NEG t2 t1`
- `NOT t2 t1`
- `MOV t2 t1`

### Binary

- `ADD t3 t1 t2`
- `SUB t3 t1 t2`
- `MUL t3 t1 t2`
- `DIV t3 t1 t2`
- `REM t3 t1 t2`
- `AND t3 t1 t2`
- `OR t3 t1 t2`
- `GT t3 t1 t2`
- `GEQ t3 t1 t2`
- `LT t3 t1 t2`
- `LEQ t3 t1 t2`
- `EQ t3 t1 t2`
- `NEQ t3 t1 t2`

### SSA merge

- `PHI t3 (pred1 t1) (pred2 t2) (...)`

### Control flow

- `BLOCK block_name`
- `JUMP target_block`
- `BRANCH t1 true_block false_block`
- `RETURN t1`
- `RETURN_VOID`
- `EXIT t1`

### Calls / built-ins

- `CALL t1 (func_name) t2 t3 (...)`
- `CALL_VOID (func_name) t2 t3 (...)`
- `PRINT t1 t2 (...)`
- `PRINTLN t1 t2 (...)`
- `READ x1 x2 (...)`

## Optimized vector IR

These show up after the current SLP pass:

- `PACK2 t3 t1 t2`
- `VADD t5 t3 t4`
- `VSUB t5 t3 t4`
- `VMUL t5 t3 t4`
- `VDIV t5 t3 t4`
- `VREM t5 t3 t4`
- `VAND t5 t3 t4`
- `VOR t5 t3 t4`
- `EXTRACT t1 t5 0`
- `EXTRACT t2 t5 1`

## What is already done

- Raw SSA VexIR emission
- Optimized SSA VexIR emission
- CFG construction
- `PHI` insertion for control-flow joins
- Loop-header phi nodes for current loop lowering
- Straight-line SLP-style vectorization for simple scalar pairs
