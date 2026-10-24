class CompileError(Exception):
    def __init__(self, message, line, col):
        super().__init__(message)
        self.message = message
        self.line = line
        self.col = col


class RuntimeFault(Exception):
    pass


def format_error(filename, source, error):
    lines = source.split("\n")
    text = ""
    if 1 <= error.line <= len(lines):
        text = lines[error.line - 1].rstrip("\r")
    pad = "".join("\t" if ch == "\t" else " " for ch in text[: max(error.col - 1, 0)])
    return (
        f"{filename}:{error.line}:{error.col}: error: {error.message}\n"
        f"{error.line:>5} | {text}\n"
        f"      | {pad}^"
    )
