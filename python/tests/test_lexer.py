import pytest

from minic.lexer import Lexer


def lex(source):
    return Lexer(source).tokenize()


def kinds(source):
    tokens, _ = lex(source)
    return [t.kind for t in tokens]


def texts(source):
    tokens, _ = lex(source)
    return [t.text for t in tokens]


def test_empty_source_yields_only_eof():
    tokens, errs = lex("")
    assert [t.kind for t in tokens] == ["EOF"]
    assert errs == []


def test_keywords_and_identifiers():
    assert kinds("int float if else while print foo _bar x1") == [
        "KEYWORD",
        "KEYWORD",
        "KEYWORD",
        "KEYWORD",
        "KEYWORD",
        "KEYWORD",
        "IDENTIFIER",
        "IDENTIFIER",
        "IDENTIFIER",
        "EOF",
    ]


def test_keyword_prefix_is_identifier():
    assert kinds("integer iffy") == ["IDENTIFIER", "IDENTIFIER", "EOF"]


def test_integer_and_float_literals():
    assert kinds("12 3.75 0 0.5") == ["INTEGER", "FLOAT", "INTEGER", "FLOAT", "EOF"]


@pytest.mark.parametrize("op", ["==", "!=", "<=", ">=", "&&", "||"])
def test_two_character_operators(op):
    tokens, errs = lex(f"a {op} b")
    assert errs == []
    assert tokens[1].text == op and tokens[1].kind == "OPERATOR"


@pytest.mark.parametrize("op", list("+-*/%=<>!"))
def test_single_character_operators(op):
    tokens, _ = lex(f"a {op} b")
    assert tokens[1].text == op


def test_longest_match_prefers_two_characters():
    assert texts("a<=b") == ["a", "<=", "b", "$"]
    assert texts("a<b") == ["a", "<", "b", "$"]
    assert texts("a=b") == ["a", "=", "b", "$"]
    assert texts("a==b") == ["a", "==", "b", "$"]


def test_delimiters():
    assert texts("(){};") == ["(", ")", "{", "}", ";", "$"]


def test_line_and_column_tracking():
    tokens, _ = lex("int x;\n  y = 2;\n")
    positions = [(t.line, t.col) for t in tokens]
    assert positions[0] == (1, 1)
    assert positions[1] == (1, 5)
    assert positions[3] == (2, 3)
    assert positions[-1] == (3, 1)


def test_line_comment_is_skipped():
    assert texts("a // ignore me\nb") == ["a", "b", "$"]


def test_block_comment_is_skipped_across_lines():
    tokens, errs = lex("a /* one\n two */ b")
    assert errs == []
    assert [t.text for t in tokens] == ["a", "b", "$"]
    assert tokens[1].line == 2


def test_unterminated_block_comment():
    _, errs = lex("a /* oops")
    assert [(e.line, e.col, e.message) for e in errs] == [(1, 3, "unterminated block comment")]


def test_division_is_not_a_comment():
    assert texts("a / b") == ["a", "/", "b", "$"]


def test_unexpected_character_is_reported_and_lexing_continues():
    tokens, errs = lex("a $ b")
    assert len(errs) == 1 and "'$'" in errs[0].message
    assert [t.text for t in tokens] == ["a", "b", "$"]


def test_single_ampersand_hint():
    _, errs = lex("a & b")
    assert "&&" in errs[0].message


def test_single_pipe_hint():
    _, errs = lex("a | b")
    assert "||" in errs[0].message


def test_number_followed_by_letters_is_bad_identifier():
    _, errs = lex("9abc")
    assert "cannot start with a digit" in errs[0].message
    assert (errs[0].line, errs[0].col) == (1, 1)


def test_float_without_fraction_digits():
    _, errs = lex("3.")
    assert "missing digits" in errs[0].message


def test_integer_literal_too_large():
    _, errs = lex("9223372036854775808")
    assert "too large" in errs[0].message


def test_largest_integer_literal_is_accepted():
    tokens, errs = lex("9223372036854775807")
    assert errs == [] and tokens[0].kind == "INTEGER"


def test_multiple_lexical_errors_are_collected():
    _, errs = lex("$ # @")
    assert len(errs) == 3


def test_tabs_and_carriage_returns_are_whitespace():
    assert texts("a\t\r\nb") == ["a", "b", "$"]
