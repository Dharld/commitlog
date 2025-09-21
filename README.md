# commitlog (C++) — tiny Git plumbing 
 
A compact Git re-implementation in modern C++17. It stores loose objects, keeps a simple on-disk staging area (index), and exposes a handful of plumbing commands. Great for learning Git internals without the kitchen sink. 
 
## Build 
 
Requirements: CMake, a C++17 compiler, zlib, OpenSSL (SHA-1). 
 
```bash 
cmake -S . -B build 
cmake --build build -j 
# binary: build/git 
``` 
 
## Quick start 
 
```bash 
# 1) Create a sandbox repo 
mkdir -p /tmp/toy_repo && cd /tmp/toy_repo 
/path/to/build/git init 
 
# 2) Stage a file 
echo "hello" > README.md 
/path/to/build/git add README.md 
cat .git/index 
# 100644 <40-hex-oid> README.md 
 
# 3) Hash & inspect objects 
oid=$(/path/to/build/git hash-object -w README.md) 
/path/to/build/git cat-file -t "$oid"  # -> blob 
/path/to/build/git cat-file -p "$oid"  # -> prints file bytes (binary-safe) 
``` 
 
## What’s implemented 
 
### Loose object storage 
 
* Layout: `.git/objects/aa/bbbbbbbbbbbbbbbbbbbbbbbb` 
* Each object is stored **compressed** (zlib). 
* OID (SHA-1) is computed over the **uncompressed** bytes: 
  `"type <size>\0" + <payload>`. 
 
### Staging area (index) 
 
* On disk: `.git/index` (text v1), one entry per line: 
 
  ``` 
  <mode> <40-hex-oid> <repo-root-relative-path> 
  ``` 
* In memory: `std::map<std::string, IndexEntry>` for deterministic order and fast upserts. 
* Atomic saves: write to `.git/index.tmp`, then `rename` → `.git/index`. 
 
### Commands 
 
* `init` — create `.git/` (objects, refs, HEAD → `refs/heads/main`) 
* `hash-object [-w] <path>` — print blob OID; with `-w` also store it 
* `cat-file (-p|-t) <oid>` — print payload (`-p`, binary-safe) or type (`-t`) 
* `ls-tree [--name-only] <tree-oid>` — list entries of a tree (parser included) 
* `add <path>` — stage **one file** (MVP): computes mode + blob OID and writes index 
 
## Design notes (concise) 
 
* **Paths in index** are **relative to the repository root** (the parent of `.git`) and use **forward slashes**. 
* **Modes**: executable bit → `100755`; otherwise `100644`. (Symlink `120000` planned.) 
* **Blobs vs trees**: blobs store only bytes; names & modes live in tree entries: 
  `"<mode> <name>\0<20 raw oid bytes>"`. 
* **Atomicity**: index and refs use “write to `.tmp` then `rename`” to avoid partial writes. 
 
## Limitations / Next steps 
 
* `add` handles a single file (no dir recursion yet). 
* `write-tree` (build trees from index) — **next** 
* `commit` (create commit object, update `refs/heads/<branch>`) — **next** 
* Symlink support (`120000`) and Windows exec-bit nuance — later 
* More robust repo discovery in commands (main already does discovery) 
 
## Troubleshooting 
 
* **Paths like `../file` in index** → you computed relative to `.git`. Use the **repo root** for `relative(abs, repo_root)`. 
* **“cannot open index temp file: .git/.git/index.tmp”** → you concatenated `.git` twice. 
* **`cat-file -p` shows gibberish** → that OID may be a **tree** (contains NULs). Use `-t` to check type. 
 
--- 
 
**Why this project?** 
It’s a clean, minimal path through Git’s core idea: **content-addressed storage**. Identical content → identical OID, so re-adding unchanged files is essentially free (index update only). ``
