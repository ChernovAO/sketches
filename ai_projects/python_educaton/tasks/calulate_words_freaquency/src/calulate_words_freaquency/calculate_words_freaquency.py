import sys
from collections import Counter
from os import path


def _parse_command_line() -> str | None:
    script_path = __file__
    script_name = path.basename(script_path)
    nr_args = len(sys.argv)
    if nr_args != 2:
        print(f"Usage: python {script_name} <file_name>")
        return None
    file_path = sys.argv[1]
    if path.isfile(file_path):
        return file_path

    print(f"{file_path} is not a regular file")
    return None


def calculate_words_freaquency(file_path: str) -> Counter[str]:
    with open(file_path) as file:
        words = file.read().split()

    words_freaquency: Counter[str] = Counter()
    words_freaquency.update(words)

    return words_freaquency

def main() -> None:
    file_path = _parse_command_line()
    if file_path is None:
        return

    print(f"Calculate words freaquency for {file_path}")
    words_freaquency = calculate_words_freaquency(file_path=file_path)
    print(words_freaquency.most_common(10))

if __name__ == "__main__":
    main()
