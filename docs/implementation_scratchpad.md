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

Postfix         : function call `f_name_expr( ... )` (note: special parse*) : left  associative -> (100, 101)
Postfix         : array index `a_name_expr[ ... ]`                          : left  associative -> (100, 101)
Prefix          : Grouping aka. sub-expression `( ... )`                    : left  associative -> special, handle separately
Prefix          : Unary operators `-` and `!`                               : right associative -> rbp >= 80
Infix           : Binary operators `*`, `/` and `%`                         : left  associative -> (70, 71)
Infix           : Binary operators `+` and `-`                              : left  associative -> (60, 61)
Infix           : Binary operators `>`, `>=`, `<`, `<=`                     : left  associative -> (50, 51)
Infix           : Binary operators `==` and `!=`                            : left  associative -> (40, 41)
Infix           : Binary operator `&&`                                      : left  associative -> (30, 31)
Infix           : Binary operator `||`                                      : left  associative -> (20, 21)
Infix           : Assignment `=`                                            : right associative -> (10, 10)

* - must parse `f_name_expr(expr, expr, ...)`. Note that it doesn't conflict with grouping since, grouping is prefix.
- Basic literals : i32, variables/id, string, bool(true / false), array, range-array
Note : I used C / C++ priorities acting as reference
- Reff: [https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html](https://matklad.github.io/2020/04/13/simple-but-powerful-pratt-parsing.html).

