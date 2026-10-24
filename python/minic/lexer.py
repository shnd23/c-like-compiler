from .errors import CompileError
from .tokens import KEYWORDS, Token

INT_MAX = (1 << 63) - 1
WHITESPACE = frozenset(" \t\r\n\f\v")
TWO_CHAR_OPERATORS = frozenset({"==", "!=", "<=", ">=", "&&", "||"})
ONE_CHAR_OPERATORS = frozenset("+-*/%=<>!")
DELIMITERS = frozenset("(){};")


def is_digit(ch):
    return ch != "" and "0" <= ch <= "9"


def is_alpha(ch):
    return ch != "" and (ch == "_" or "a" <= ch <= "z" or "A" <= ch <= "Z")


def is_alnum(ch):
    return is_alpha(ch) or is_digit(ch)


class Lexer:
    def __init__(self, source):
        self.src = source
        self.pos = 0
        self.line = 1
        self.col = 1
        self.errors = []

    def peek(self, offset=0):
        index = self.pos + offset
        return self.src[index] if index < len(self.src) else ""

    def advance(self):
        ch = self.src[self.pos]
        self.pos += 1
        if ch == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def error(self, message, line, col):
        self.errors.append(CompileError(message, line, col))

    def tokenize(self):
        tokens = []
        while True:
            self.skip_trivia()
            if self.pos >= len(self.src):
                tokens.append(Token("EOF", "$", self.line, self.col))
                break
            token = self.next_token()
            if token is not None:
                tokens.append(token)
        return tokens, self.errors

    def skip_trivia(self):
        while self.pos < len(self.src):
            ch = self.src[self.pos]
            if ch in WHITESPACE:
                self.advance()
            elif ch == "/" and self.peek(1) == "/":
                while self.pos < len(self.src) and self.src[self.pos] != "\n":
                    self.advance()
            elif ch == "/" and self.peek(1) == "*":
                line, col = self.line, self.col
                self.advance()
                self.advance()
                closed = False
                while self.pos < len(self.src):
                    if self.src[self.pos] == "*" and self.peek(1) == "/":
                        self.advance()
                        self.advance()
                        closed = True
                        break
                    self.advance()
                if not closed:
                    self.error("unterminated block comment", line, col)
            else:
                break

    def next_token(self):
        line, col = self.line, self.col
        ch = self.peek()
        if is_alpha(ch):
            return self.read_word(line, col)
        if is_digit(ch):
            return self.read_number(line, col)
        two = ch + self.peek(1)
        if two in TWO_CHAR_OPERATORS:
            self.advance()
            self.advance()
            return Token("OPERATOR", two, line, col)
        if ch in ONE_CHAR_OPERATORS:
            self.advance()
            return Token("OPERATOR", ch, line, col)
        if ch in DELIMITERS:
            self.advance()
            return Token("DELIMITER", ch, line, col)
        self.advance()
        if ch in ("&", "|"):
            self.error(f"unexpected character '{ch}' (did you mean '{ch}{ch}'?)", line, col)
        else:
            self.error(f"unexpected character '{ch}'", line, col)
        return None

    def read_word(self, line, col):
        start = self.pos
        while is_alnum(self.peek()):
            self.advance()
        word = self.src[start : self.pos]
        kind = "KEYWORD" if word in KEYWORDS else "IDENTIFIER"
        return Token(kind, word, line, col)

    def read_number(self, line, col):
        start = self.pos
        while is_digit(self.peek()):
            self.advance()
        is_float = False
        missing_digits = False
        if self.peek() == ".":
            is_float = True
            self.advance()
            digits = 0
            while is_digit(self.peek()):
                self.advance()
                digits += 1
            missing_digits = digits == 0
        bad_suffix = False
        if is_alpha(self.peek()):
            while is_alnum(self.peek()):
                self.advance()
            bad_suffix = True
        word = self.src[start : self.pos]
        if bad_suffix:
            self.error(f"bad identifier '{word}': identifiers cannot start with a digit", line, col)
            return None
        if missing_digits:
            self.error(f"'{word}' is missing digits after the decimal point", line, col)
            return None
        if is_float:
            return Token("FLOAT", word, line, col)
        digits_only = word.lstrip("0")
        if len(digits_only) > 19 or (digits_only and int(digits_only) > INT_MAX):
            self.error(f"integer literal '{word}' is too large", line, col)
            return None
        return Token("INTEGER", word, line, col)
