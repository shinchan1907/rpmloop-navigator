import hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
rows=(root/'SHA256SUMS.txt').read_text().splitlines()
for row in rows:
    expected,name=row.split('  ',1)
    path=root/name
    assert path.is_file(),name
    assert hashlib.sha256(path.read_bytes()).hexdigest()==expected,name
print(f'PASS: {len(rows)} files match the frozen P3 package hashes.')
