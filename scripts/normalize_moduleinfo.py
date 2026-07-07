#!/usr/bin/env python3

import json
import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: normalize_moduleinfo.py <moduleinfo.json>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")
    normalized = re.sub(r",(\s*[}\]])", r"\1", text)
    parsed = json.loads(normalized)
    path.write_text(json.dumps(parsed, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
