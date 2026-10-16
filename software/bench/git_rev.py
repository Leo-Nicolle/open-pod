# PlatformIO pre-script for [env:bench]: stamps the firmware with the git
# revision (OPENPOD_GIT_REV), so each bench report names the code it measured.
import subprocess

Import("env")  # noqa: F821 (provided by PlatformIO)

try:
    rev = subprocess.check_output(
        ["git", "describe", "--always", "--dirty", "--tags"],
        cwd=env.subst("$PROJECT_DIR"),  # noqa: F821
        text=True,
    ).strip()
except Exception:
    rev = "unknown"

env.Append(CPPDEFINES=[("OPENPOD_GIT_REV", env.StringifyMacro(rev))])  # noqa: F821
