# MiniC language reference

MiniC is a small statically typed, C-like language with two numeric types, structured control flow and block scoping.

## Lexical structure

| Element | Form |
| --- | --- |
| Keywords | `int` `float` `if` `else` `while` `print` |
| Identifiers | `[A-Za-z_][A-Za-z0-9_]*` |
| Integer literals | decimal digits, must fit in a signed 64-bit integer |
| Float literals | digits `.` digits (a digit is required on both sides of the point) |
| Operators | `+ - * / % = < > <= >= == != && \|\| !` |
| Delimiters | `( ) { } ;` |
| Line comments | `// ...` to end of line |
| Block comments | `/* ... */`, not nestable |

A number immediately followed by letters (`9abc`) is a lexical error, as is `3.`.

## Grammar

```
program      = { statement } ;
statement    = declaration | assignment | if | while | print | block | expr_stmt ;
declaration  = ( "int" | "float" ) IDENT [ "=" expression ] ";" ;
assignment   = IDENT "=" expression ";" ;
if           = "if" "(" expression ")" body [ "else" body ] ;
while        = "while" "(" expression ")" body ;
body         = statement ;
print        = "print" "(" expression ")" ";" ;
block        = "{" { statement } "}" ;
expr_stmt    = expression ";" ;

expression   = or ;
or           = and { "||" and } ;
and          = equality { "&&" equality } ;
equality     = relational { ( "==" | "!=" ) relational } ;
relational   = additive { ( "<" | ">" | "<=" | ">=" ) additive } ;
additive     = term { ( "+" | "-" ) term } ;
term         = unary { ( "*" | "/" | "%" ) unary } ;
unary        = ( "-" | "!" ) unary | primary ;
primary      = INTEGER | FLOAT | IDENT | "(" expression ")" ;
```

The body of an `if`, `else` or `while` may not be a bare declaration; wrap it in a block. An `else` binds to the nearest `if`. A stray `else` is reported as an error.

Nesting deeper than 200 levels (parentheses, unary chains, statements) is rejected with `nesting is too deep`.

## Precedence (high to low)

| Level | Operators | Associativity |
| --- | --- | --- |
| 1 | unary `-` `!` | right |
| 2 | `*` `/` `%` | left |
| 3 | `+` `-` | left |
| 4 | `<` `>` `<=` `>=` | left |
| 5 | `==` `!=` | left |
| 6 | `&&` | left |
| 7 | `\|\|` | left |

## Types and conversions

- `int` is a signed 64-bit integer with two's-complement wraparound on overflow.
- `float` is an IEEE 754 double.
- `int` converts to `float` implicitly in arithmetic, comparisons, assignments and initializers.
- `float` never converts to `int` implicitly: `int n = 2.5;` is a compile error.
- Arithmetic on mixed operands yields `float`; on two ints yields `int`.
- Comparison, logical and `!` operators always yield `int` (`1` or `0`).
- `%` requires two `int` operands.

## Semantics

- A condition is true when it is non-zero (for floats, not equal to `0.0`).
- `&&` and `||` short-circuit.
- `/` on integers truncates toward zero; `%` takes the sign of the dividend. Dividing by zero is a runtime error.
- A declaration without an initializer sets the variable to zero each time it executes.
- A name is visible from its declaration to the end of the enclosing block. Inner blocks may shadow outer names; redeclaring a name in the same block is an error. A variable is not visible inside its own initializer.
- `print(expr)` writes one line. Integers print in decimal; floats print with up to 10 significant digits (`%.10g`) and keep a trailing `.0` when integral.

## Diagnostics

Errors carry a line and column and are printed with the source line and a caret:

```
prog.mc:3:1: error: cannot assign float to int variable 'm'
    3 | m = 1.5 + 1;
      | ^
```

| Phase | Reported errors |
| --- | --- |
| Lexical | unexpected character, bad identifier, missing fraction digits, integer too large, unterminated block comment |
| Syntax | expected token, unexpected token, missing operator, stray `else`, declaration as body, nesting too deep |
| Semantic | undeclared variable, redeclaration, float to int assignment, `%` on floats |

The lexer and parser report every error they find (the parser resynchronizes at `;`, `}` and statement keywords). Later phases run only when earlier phases succeed.

## Runtime errors

`division by zero` and `step limit exceeded (possible infinite loop)` (default limit 10,000,000 VM instructions, adjustable with `--max-steps`).

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | success |
| 1 | compilation failed |
| 2 | usage error or unreadable file |
| 3 | runtime error |
