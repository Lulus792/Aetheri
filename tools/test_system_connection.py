"""Private component bus; process context is mocked, no installed policy change."""
from pathlib import Path
import subprocess
import sys
import tempfile
from test_launch_native import private_bus

def main():
    executable, build = map(Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="system-connection-", dir=build) as temporary:
        with private_bus(Path(temporary)) as environment:
            subprocess.run([str(executable)], env=environment, check=True, timeout=50)

if __name__ == "__main__":
    main()
