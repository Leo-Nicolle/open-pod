#!/usr/bin/env python3
import sys
import re
from pathlib import Path

# ANSI color codes
GREEN = "\033[32m"
RED = "\033[31m"
CYAN = "\033[36m"
RESET = "\033[0m"

# Unicode symbols
CHECK = "✔"
CROSS = "✖"

def extract_filename(line):
    """Extract test filename from Unity output line"""
    match = re.match(r'^test/(.*?\.cpp):', line)
    return Path(match.group(1)).name if match else None

def colorize_test_line(line):
    """Colorize individual test result lines"""
    if ':PASS' in line:
        return f"{GREEN}{CHECK} {line}{RESET}"
    elif ':FAIL' in line:
        return f"{RED}{CROSS} {line}{RESET}"
    return None

def is_noise(line):
    """Returns True if the line should be skipped"""
    return (
        not line.strip()
        or re.match(r'^(Processing|Building|Scanning|Error:|LDF:|Library Manager|Warning!|Verbose mode|Dependency Graph|Found \d+ compatible libraries)', line)
        or re.match(r'^[-=]{10,}', line)
        or re.match(r'^\d+ Tests \d+ Failures \d+ Ignored', line)
        or 'SUMMARY' in line
        or 'test cases:' in line
        or 'FAIL' == line.strip()
        or 'PASS' == line.strip()
        or line.startswith('Test ')
    )

def main():
    try:
        for line in sys.stdin:
            line = line.rstrip()

            if is_noise(line):
                continue

            if ':PASS' in line or ':FAIL' in line:
                colored = colorize_test_line(line)
                if colored:
                    print(colored)

    except BrokenPipeError:
        sys.stderr.close()
    except KeyboardInterrupt:
        sys.exit(1)

if __name__ == "__main__":
    main()
