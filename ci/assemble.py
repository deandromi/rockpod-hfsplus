import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]).resolve()
expected = "951d17c0575fbb61f3c585b5f0c3a0a941dcb10f"
head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
if head != expected:
    raise SystemExit("Unexpected upstream commit: " + head)
manifest = json.loads((root / "ci/source-manifest.json").read_text())
for relative, hashes in manifest.items():
    original = source / relative
    override = root / "overrides" / relative
    if hashlib.sha256(override.read_bytes()).hexdigest() != hashes["updated"]:
        raise SystemExit("Override checksum mismatch: " + relative)
    if hashes["original"] is None:
        if original.exists():
            raise SystemExit("Unexpected upstream file: " + relative)
    elif hashlib.sha256(original.read_bytes()).hexdigest() != hashes["original"]:
        raise SystemExit("Upstream checksum mismatch: " + relative)
for relative in manifest:
    target = source / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / "overrides" / relative, target)
print(f"Applied {len(manifest)} verified source files to {head}")
