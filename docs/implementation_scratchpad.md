### Current Stages / Logs

- Lexing is done, tokenization works smoothy :)
- Next, parsing

##### Working on parsing :

Implementing a basic version of the grammar first then expand later

```c++
Expr -> LiteralExpr | UnaryExpr | BinaryExpr | Grouping

LiteralExpr -> Number | String | "true" | "false" | "nil"

Grouping -> "(" Expr ")"

UnaryExpr -> ( "!" | "-" ) Expr

BinaryExpr -> Expr Operator Expr

Operator -> "*" | "/" | "%" | "+" | "-" | "<" | "<=" | ">" | ">=" | "==" | "!=" | "&&" | "||"
``` 

##### Plan
Gonna use a compile-time visitor pattern with `std::variant` and `std::visit` instead of a full-blown virtual Vistor class like in Crafting 
Interpreters. This will save on V-Table overhead.
 - Reff: [https://www.youtube.com/watch?v=et1fjd8X1ho](https://www.youtube.com/watch?v=et1fjd8X1ho).

##### Reference Order for Precedence of operators
Note : highest Binding power to lowest, Pratt style

Prefix          : Grouping aka. sub-expression `( ... )`                    : left  associative -> special, handle separately
Postfix         : function call `f_name_expr( ... )` (note: special parse*) : left  associative -> (100, --)
Postfix         : array index `a_name_expr[ ... ]`                          : left  associative -> (100, --)
Prefix          : Unary operators `-` and `!`                               : right associative -> (--, 80)
Infix           : Binary operators `*`, `/` and `%`                         : left  associative -> (70, 71)
Infix           : Binary operators `+` and `-`                              : left  associative -> (60, 61)
Infix           : Binary operators `>`, `>=`, `<`, `<=`                     : left  associative -> (50, 51)
Infix           : Binary operators `==` and `!=`                            : left  associative -> (40, 41)
Infix           : Binary operator `&&`                                      : left  associative -> (30, 31)
Infix           : Binary operator `||`                                      : left  associative -> (20, 21)
Infix           : Assignment `=`                                            : right associative -> (11, 10)

* - must parse `f_name_expr(expr, expr, ...)`. Note that it doesn't conflict with grouping since, grouping is prefix.
- Basic literals : i32, variables/id, string, bool(true / false), array, range-array
Note : I used C / C++ priorities acting as reference
- Reff: [https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html).

- On actual implementation - the postfix stuff has also been handled in a special way so no point of binding powers :0
- Same for the prefix stuff :0
- We can keep the idea for future extension tho >.<
- to-do way in the future : start using { ... } init style everywhere

- OKAY! Expression parsing is done... onward to statements!

##### AST structure for statements

```text
-> ExprStatement ---------------> leftover default (?)
-> Block -> just a group of statements ----> starts with {
-> VarDeclStatement ----> starts with a variable type name
-> OPrintStatement  ------> print(
-> IOPrintlnStatement -----> println(
-> IOReadStatement  -----> read(
-> IfElseStatement (Note: Elif will be lowered here) -----> if
-> FuncDeclStatement  ---> fun 
-> ReturnStatement ----> return
-> ForLoopStatement ---> for 
-> WhileLoopStatement ----> while 
-> BreakStatement ----> break
-> ContinueStatement ---> continue
-> ExitStatement ---> exit(
```

Note : empty statements can be ignored