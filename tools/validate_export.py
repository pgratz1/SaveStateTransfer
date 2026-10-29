#!/usr/bin/env python3
"""Validate a SaveStateTransfer export and predict what a target load order cannot resolve.

Usage:
  python3 -I tools/validate_export.py EXPORT.json [--target-plugins /path/to/plugins.txt] [--schema tools/schema/v1.schema.json]

Without the `jsonschema` package only structural checks (FormKey format, required sections) run.
"""
import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

FORM_ID_RE = re.compile(r"^0x[0-9A-Fa-f]{1,6}$")


def iter_form_keys(node, path="$"):
    """Yield (path, formkey_dict) for every object that looks like a FormKey."""
    if isinstance(node, dict):
        if set(node.keys()) == {"plugin", "id"}:
            yield path, node
            return
        for k, v in node.items():
            yield from iter_form_keys(v, f"{path}.{k}")
    elif isinstance(node, list):
        for i, v in enumerate(node):
            yield from iter_form_keys(v, f"{path}[{i}]")


# Vanilla masters are force-loaded and never listed in MO2's plugins.txt.
IMPLICIT_MASTERS = {"skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm", "dragonborn.esm", "skyrimvr.esm"}


def load_plugins_txt(path: Path):
    enabled = list(IMPLICIT_MASTERS)
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if line.startswith("*"):
            enabled.append(line[1:].strip())
    return enabled


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("export", type=Path)
    ap.add_argument("--target-plugins", type=Path, help="plugins.txt of the destination MO2 profile")
    ap.add_argument("--schema", type=Path, default=Path(__file__).with_name("schema") / "v1.schema.json")
    args = ap.parse_args()

    data = json.loads(args.export.read_text(encoding="utf-8"))
    errors = []

    if data.get("schema") != 1:
        errors.append(f"schema field is {data.get('schema')!r}, expected 1")
    for section in ("tool", "source", "character", "world"):
        if section not in data:
            errors.append(f"missing top-level section '{section}'")

    try:
        import jsonschema  # type: ignore
        schema = json.loads(args.schema.read_text(encoding="utf-8"))
        validator = jsonschema.Draft202012Validator(schema)
        for err in sorted(validator.iter_errors(data), key=lambda e: list(e.path)):
            errors.append(f"schema: {'/'.join(str(p) for p in err.path)}: {err.message}")
    except ImportError:
        print("note: python 'jsonschema' not installed; structural checks only", file=sys.stderr)

    keys = list(iter_form_keys(data))
    for path, fk in keys:
        if not isinstance(fk.get("plugin"), str) or not fk["plugin"]:
            errors.append(f"{path}: empty plugin name")
        if not isinstance(fk.get("id"), str) or not FORM_ID_RE.match(fk["id"]):
            errors.append(f"{path}: bad form id {fk.get('id')!r}")

    by_plugin = Counter(fk["plugin"].lower() for _, fk in keys if isinstance(fk.get("plugin"), str))
    print(f"{args.export.name}: {len(keys)} form references across {len(by_plugin)} plugins")

    source_lo = [p.lower() for p in data.get("source", {}).get("loadOrder", [])]
    for plugin in by_plugin:
        if source_lo and plugin not in source_lo:
            errors.append(f"plugin '{plugin}' referenced but not in source.loadOrder")

    if args.target_plugins:
        target = {p.lower() for p in load_plugins_txt(args.target_plugins)}
        missing = {p: n for p, n in by_plugin.items() if p not in target}
        if missing:
            print(f"predicted missing[] in target ({args.target_plugins}):")
            for p, n in sorted(missing.items(), key=lambda kv: -kv[1]):
                print(f"  {p:50s} {n:6d} references")
        else:
            print("all referenced plugins are present in the target profile")

    w = data.get("world", {})
    c = data.get("character", {})
    print(f"character: level {c.get('progression', {}).get('level')}, {len(c.get('inventory', {}).get('items', []))} items, "
          f"{len(c.get('progression', {}).get('perks', []))} perks, {len(c.get('magic', {}).get('spells', []))} spells")
    print(f"world: {len(w.get('quests', []))} quests, {len(w.get('factions', []))} factions, {len(w.get('globals', []))} globals, "
          f"{len(w.get('locationsCleared', []))} cleared locations, {len(w.get('actors', []))} actors, {len(w.get('references', []))} refs")

    if errors:
        print(f"{len(errors)} problem(s):")
        for e in errors[:200]:
            print("  " + e)
        return 1
    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
