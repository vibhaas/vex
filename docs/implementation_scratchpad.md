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
Gonna use a compile-time visitor pattern with `std::variant` and `std::visit` instead of a full-blown virtual Vistor class like in Crafting Interpreters. This will save on V-Table overhead.
 - Reff: [https://www.youtube.com/watch?v=et1fjd8X1ho](https://www.youtube.com/watch?v=et1fjd8X1ho).


