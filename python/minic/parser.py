import sys

from .ast_nodes import (
    Assign,
    Binary,
    Block,
    ExprStmt,
    If,
    Number,
    Print,
    Program,
    Unary,
    Var,
    VarDecl,
    While,
)
from .errors import CompileError
from .tokens import TYPE_KEYWORDS

MAX_DEPTH = 200
RECURSION_LIMIT = 20000

BINARY_LEVELS = [
    ("||",),
    ("&&",),
    ("==", "!="),
    ("<", ">", "<=", ">="),
    ("+", "-"),
    ("*", "/", "%"),
]

SYNC_KEYWORDS = frozenset({"int", "float", "if", "while", "print"})


class ParseError(Exception):
    def __init__(self, message, token):
        super().__init__(message)
        self.message = message
        self.token = token


def describe_token(token):
    if token.kind == "EOF":
        return "end of input"
    return f"'{token.text}'"


class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0
        self.depth = 0
        self.errors = []

    def peek(self):
        return self.tokens[self.pos]

    def advance(self):
        token = self.tokens[self.pos]
        if token.kind != "EOF":
            self.pos += 1
        return token

    def check(self, text):
        token = self.tokens[self.pos]
        return token.kind in ("KEYWORD", "OPERATOR", "DELIMITER") and token.text == text

    def expect(self, text):
        token = self.peek()
        if self.check(text):
            return self.advance()
        raise ParseError(f"expected '{text}' before {describe_token(token)}", token)

    def expect_identifier(self):
        token = self.peek()
        if token.kind == "IDENTIFIER":
            return self.advance()
        raise ParseError(f"expected identifier before {describe_token(token)}", token)

    def enter(self):
        self.depth += 1
        if self.depth > MAX_DEPTH:
            raise ParseError("nesting is too deep", self.peek())

    def leave(self):
        self.depth -= 1

    def parse(self):
        if sys.getrecursionlimit() < RECURSION_LIMIT:
            sys.setrecursionlimit(RECURSION_LIMIT)
        stmts = self.parse_statements(in_block=False)
        return Program(line=1, col=1, stmts=stmts), self.errors

    def parse_statements(self, in_block):
        stmts = []
        while self.peek().kind != "EOF":
            if in_block and self.check("}"):
                break
            start = self.pos
            saved_depth = self.depth
            try:
                stmts.append(self.parse_statement())
            except ParseError as err:
                self.errors.append(CompileError(err.message, err.token.line, err.token.col))
                self.depth = saved_depth
                self.synchronize()
                if self.pos == start:
                    self.advance()
        return stmts

    def synchronize(self):
        while self.peek().kind != "EOF":
            token = self.peek()
            if token.kind == "DELIMITER" and token.text == ";":
                self.advance()
                return
            if token.kind == "DELIMITER" and token.text == "}":
                return
            if token.kind == "KEYWORD" and token.text in SYNC_KEYWORDS:
                return
            self.advance()

    def parse_statement(self):
        self.enter()
        try:
            token = self.peek()
            if token.kind == "KEYWORD":
                if token.text in TYPE_KEYWORDS:
                    return self.parse_declaration()
                if token.text == "if":
                    return self.parse_if()
                if token.text == "while":
                    return self.parse_while()
                if token.text == "print":
                    return self.parse_print()
            if self.check("else"):
                self.errors.append(
                    CompileError("'else' without a matching 'if'", token.line, token.col)
                )
                self.advance()
                self.parse_body("else")
                return Block(line=token.line, col=token.col, stmts=[])
            if self.check("{"):
                return self.parse_block()
            if token.kind == "IDENTIFIER":
                following = self.tokens[self.pos + 1]
                if following.kind == "OPERATOR" and following.text == "=":
                    return self.parse_assignment()
            return self.parse_expression_statement()
        finally:
            self.leave()

    def parse_body(self, owner):
        token = self.peek()
        if token.kind == "KEYWORD" and token.text in TYPE_KEYWORDS:
            raise ParseError(
                f"a declaration cannot be the body of '{owner}'; wrap it in a block", token
            )
        return self.parse_statement()

    def parse_declaration(self):
        keyword = self.advance()
        name = self.expect_identifier()
        init = None
        if self.check("="):
            self.advance()
            init = self.parse_expression()
        self.expect(";")
        return VarDecl(
            line=name.line, col=name.col, var_type=keyword.text, name=name.text, init=init
        )

    def parse_assignment(self):
        name = self.advance()
        self.expect("=")
        value = self.parse_expression()
        self.expect(";")
        return Assign(line=name.line, col=name.col, name=name.text, value=value)

    def parse_if(self):
        keyword = self.advance()
        self.expect("(")
        cond = self.parse_expression()
        self.expect(")")
        then_branch = self.parse_body("if")
        else_branch = None
        if self.check("else"):
            self.advance()
            else_branch = self.parse_body("else")
        return If(
            line=keyword.line,
            col=keyword.col,
            cond=cond,
            then_branch=then_branch,
            else_branch=else_branch,
        )

    def parse_while(self):
        keyword = self.advance()
        self.expect("(")
        cond = self.parse_expression()
        self.expect(")")
        body = self.parse_body("while")
        return While(line=keyword.line, col=keyword.col, cond=cond, body=body)

    def parse_print(self):
        keyword = self.advance()
        self.expect("(")
        value = self.parse_expression()
        self.expect(")")
        self.expect(";")
        return Print(line=keyword.line, col=keyword.col, value=value)

    def parse_block(self):
        brace = self.expect("{")
        stmts = self.parse_statements(in_block=True)
        self.expect("}")
        return Block(line=brace.line, col=brace.col, stmts=stmts)

    def parse_expression_statement(self):
        start = self.peek()
        expr = self.parse_expression()
        self.expect(";")
        return ExprStmt(line=start.line, col=start.col, expr=expr)

    def parse_expression(self):
        return self.parse_binary(0)

    def parse_binary(self, level):
        if level == len(BINARY_LEVELS):
            return self.parse_unary()
        left = self.parse_binary(level + 1)
        while self.peek().kind == "OPERATOR" and self.peek().text in BINARY_LEVELS[level]:
            op = self.advance()
            right = self.parse_binary(level + 1)
            left = Binary(line=op.line, col=op.col, op=op.text, left=left, right=right)
        return left

    def parse_unary(self):
        self.enter()
        try:
            token = self.peek()
            if token.kind == "OPERATOR" and token.text in ("-", "!"):
                self.advance()
                operand = self.parse_unary()
                return Unary(line=token.line, col=token.col, op=token.text, operand=operand)
            return self.parse_primary()
        finally:
            self.leave()

    def reject_adjacent_operand(self, previous):
        following = self.peek()
        if following.kind in ("IDENTIFIER", "INTEGER", "FLOAT") and following.line == previous.line:
            raise ParseError(f"missing an operator before '{following.text}'", following)

    def parse_primary(self):
        token = self.peek()
        if token.kind in ("INTEGER", "FLOAT"):
            self.advance()
            self.reject_adjacent_operand(token)
            return Number(
                line=token.line, col=token.col, text=token.text, is_float=token.kind == "FLOAT"
            )
        if token.kind == "IDENTIFIER":
            self.advance()
            self.reject_adjacent_operand(token)
            return Var(line=token.line, col=token.col, name=token.text)
        if self.check("("):
            self.advance()
            expr = self.parse_expression()
            self.expect(")")
            return expr
        if token.kind == "EOF":
            raise ParseError("unexpected end of input", token)
        raise ParseError(f"unexpected token '{token.text}'", token)
