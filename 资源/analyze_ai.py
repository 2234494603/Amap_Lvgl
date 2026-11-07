from pathlib import Path
from urllib.parse import unquote
from collections import Counter, defaultdict

rows = []
for line in Path("ai_objects.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    parts = line.split("\t")
    if len(parts) < 11:
        continue
    depth, typ, path, name, x, y, w, h, ab, hidden, locked = parts[:11]
    rows.append(dict(depth=int(depth), type=typ, path=unquote(path), name=unquote(name),
                     x=float(x) if x else 0, y=float(y) if y else 0,
                     w=float(w) if w else 0, h=float(h) if h else 0,
                     ab=int(ab) if ab else -1, hidden=int(hidden) if hidden else 0, locked=int(locked) if locked else 0))

top = [r for r in rows if r["depth"] == 1]
print("top-level items:", len(top))
print("by type:", Counter(r["type"] for r in top))
print("by artboard:", Counter(r["ab"] for r in top))
print("artboard 4 sample:")

# count descendants by path prefix
def children_count(r):
    prefix = r["path"] + "/"
    return sum(1 for x in rows if x["path"].startswith(prefix))

for ab in range(7):
    items = sorted([r for r in top if r["ab"] == ab], key=lambda r: (round(r["y"], 1), r["x"]))
    print(f"\n===== artboard {ab}: {len(items)} top-level =====")
    for r in items[:60]:
        cc = children_count(r)
        print(f"{r['type']:18s} x={r['x']:8.1f} y={r['y']:8.1f} w={r['w']:7.1f} h={r['h']:7.1f} kids={cc:3d} name={r['name']!r}")
    if len(items) > 60:
        print(f"... {len(items)-60} more")

