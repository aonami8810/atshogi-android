seed_path = r"c:\VS\Workspace\atshogi\atshogi_project\asymmetric_seed_atlas.txt"
with open(seed_path, "r", encoding="utf-8") as f:
    lines = f.readlines()
print(f"Total lines in asymmetric_seed_atlas.txt: {len(lines)}")
for i in range(min(15, len(lines))):
    print(lines[i].strip())
