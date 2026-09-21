#!/usr/bin/env python3
"""config/west.yml のリビジョンを SHA に固定する / 追跡ブランチに戻す。

  tools/west-pins.py            固定する (既定)
  tools/west-pins.py --unpin    追跡ブランチ名に戻す
  tools/west-pins.py --check    固定されている SHA が追跡先の先頭かどうか見るだけ

固定した行には `# track: <ref>` が付く。どのブランチ由来かはこれで分かり、
--unpin と次回の固定はこのコメントを読む。

コメントを壊さないよう、YAML として読み書きせず行単位で書き換える。
"""
import argparse, re, subprocess, sys
from pathlib import Path

WEST = Path(__file__).resolve().parent.parent / "config" / "west.yml"
# 行末コメントを許す。remotes: の各行に説明を書いている manifest があるため
# (例: `- name: cormoran   # DYA Studio 本体`)。これを許さないと remotes が
# 1つも拾えず、projects 側で KeyError になる。
_C = r"\s*(?:#.*)?$"
RE_REMOTE = re.compile(r"^\s*- name:\s*(\S+)" + _C)
RE_URLBASE = re.compile(r"^\s*url-base:\s*(\S+)" + _C)
RE_NAME = re.compile(r"^(\s*)- name:\s*(\S+)" + _C)
RE_REMOTE_OF = re.compile(r"^\s*remote:\s*(\S+)" + _C)
# revision 行だけは緩めない。`# track:` 以外のコメントが付いた行は
# 「読み切れなかった」として素通しする。緩めると書き換え時にその
# コメントを消してしまう。
RE_REV = re.compile(r"^(\s*revision:\s*)(\S+)(\s*#\s*track:\s*(\S+))?\s*$")
SHA = re.compile(r"^[0-9a-f]{40}$")


def load_remotes(lines):
    """remotes: ブロックだけを読んで name -> url-base を作る。"""
    remotes, cur, in_remotes = {}, None, False
    for ln in lines:
        if ln.startswith("  remotes:"):
            in_remotes = True
            continue
        if ln.startswith("  projects:"):
            break
        if not in_remotes:
            continue
        m = RE_REMOTE.match(ln)
        if m:
            cur = m.group(1)
        m = RE_URLBASE.match(ln)
        if m and cur:
            remotes[cur] = m.group(1)
    return remotes


def resolve(url, ref):
    r = subprocess.run(
        ["git", "ls-remote", url, f"refs/heads/{ref}", f"refs/tags/{ref}"],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        raise SystemExit(f"ls-remote failed for {url}: {r.stderr.strip()}")
    head = tag = None
    for line in r.stdout.splitlines():
        sha, name = line.split("\t")
        if name == f"refs/heads/{ref}":
            head = sha
        elif name == f"refs/tags/{ref}":
            tag = sha
    sha = head or tag
    if not sha:
        raise SystemExit(f"{url}: ref '{ref}' not found")
    return sha


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--unpin", action="store_true")
    g.add_argument("--check", action="store_true")
    args = ap.parse_args()

    lines = WEST.read_text(encoding="utf-8").splitlines()
    remotes = load_remotes(lines)

    out, name, remote, changed, stale = [], None, None, 0, 0
    in_projects = False
    for ln in lines:
        if ln.startswith("  projects:"):
            in_projects = True
        if in_projects:
            m = RE_NAME.match(ln)
            if m:
                name, remote = m.group(2), None
            m = RE_REMOTE_OF.match(ln)
            if m:
                remote = m.group(1)
            m = RE_REV.match(ln)
            if m and name and remote:
                indent, value, track = m.group(1), m.group(2), m.group(4)
                ref = track or value
                if args.unpin:
                    new = f"{indent}{ref}"
                else:
                    if remote not in remotes:
                        raise SystemExit(
                            f"remote '{remote}' ({name}) の url-base が読めない。"
                            " remotes: ブロックの書式を確認すること"
                        )
                    sha = resolve(f"{remotes[remote]}/{name}", ref)
                    if args.check:
                        if SHA.match(value) and value != sha:
                            print(f"  更新あり  {name:<44} {ref:<28} {value[:8]} -> {sha[:8]}")
                            stale += 1
                        out.append(ln)
                        continue
                    new = f"{indent}{sha}  # track: {ref}"
                if new != ln:
                    changed += 1
                out.append(new)
                continue
        out.append(ln)

    if args.check:
        print(f"追跡先より古いもの: {stale}")
        return 1 if stale else 0

    WEST.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"{'戻した' if args.unpin else '固定した'}行: {changed}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
