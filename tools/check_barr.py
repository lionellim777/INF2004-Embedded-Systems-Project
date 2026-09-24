"""Check the mechanical BARR-C rules used by project-owned C files."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIRS = ("include", "src", "port", "config", "tests")
FORBIDDEN = re.compile(r"\b(?:auto|register|abort|exit|setjmp|longjmp)\b")


def main() -> int:
    problems: list[str] = []
    for directory in SOURCE_DIRS:
        for path in (ROOT / directory).rglob("*"):
            if path.suffix not in (".c", ".h") or path.name == "comm_secrets.h":
                continue
            text = path.read_text(encoding="utf-8")
            if not text.endswith("\n"):
                problems.append(f"{path}: missing final newline")
            for number, line in enumerate(text.splitlines(), 1):
                if len(line) > 80:
                    problems.append(f"{path}:{number}: over 80 columns")
                if "\t" in line:
                    problems.append(f"{path}:{number}: tab character")
                if FORBIDDEN.search(line) and not line.lstrip().startswith(("//", "*")):
                    problems.append(f"{path}:{number}: forbidden token")
    print("\n".join(problems) if problems else "Mechanical BARR-C checks passed")
    return bool(problems)


if __name__ == "__main__":
    sys.exit(main())
