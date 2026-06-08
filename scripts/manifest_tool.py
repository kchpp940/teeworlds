#!/usr/bin/env python3
import sys
import os
import argparse

DEFAULT_DATASRC_DIR = "datasrc"
DEFAULT_MANIFEST_FILE = "data_manifest.txt"


def content_py_files(datasrc_dir):
    sys.path.insert(0, datasrc_dir)
    import importlib
    if "content" in sys.modules:
        importlib.reload(sys.modules["content"])
    import content
    files = set()

    for img in content.container.images.items:
        fn = img.filename.value
        if fn:
            files.add(fn)

    for sset in content.container.sounds.items:
        for snd in sset.sounds.items:
            fn = snd.filename.value
            if fn:
                files.add(fn)

    return files


def scan_directory(datasrc_dir, relpath, extensions):
    results = set()
    full = os.path.join(datasrc_dir, relpath)
    if not os.path.isdir(full):
        return results
    for root, _dirs, fnames in os.walk(full):
        for f in fnames:
            if any(f.endswith("." + ext) for ext in extensions) or "." not in f:
                rel = os.path.relpath(os.path.join(root, f), datasrc_dir)
                rel = rel.replace(os.sep, "/")
                results.add(rel)
    return results


def collect_additional_files(datasrc_dir):
    files = set()
    dir_specs = [
        ("countryflags", ["png", "json"]),
        ("editor", ["png", "json"]),
        ("fonts", ["ttf", "ttc", "json"]),
        ("languages", ["json", "txt"]),
        ("mapres", ["png"]),
        ("maps", ["map", "txt"]),
        ("skins", ["png", "json"]),
        ("ui", ["png", "map"]),
        ("audio", ["wv"]),
    ]
    for d, exts in dir_specs:
        files.update(scan_directory(datasrc_dir, d, exts))

    toplevel_extensions = ["png"]
    if os.path.isdir(datasrc_dir):
        for f in os.listdir(datasrc_dir):
            full = os.path.join(datasrc_dir, f)
            if os.path.isfile(full) and any(f.endswith("." + e) for e in toplevel_extensions):
                files.add(f)

    return files


def all_resource_files_on_disk(datasrc_dir):
    files = set()
    dir_specs = [
        ("countryflags", ["png", "json"]),
        ("editor", ["png", "json"]),
        ("fonts", ["ttf", "ttc", "json"]),
        ("languages", ["json", "txt"]),
        ("mapres", ["png"]),
        ("maps", ["map", "txt"]),
        ("skins", ["png", "json"]),
        ("ui", ["png", "map"]),
        ("audio", ["wv"]),
    ]
    for d, exts in dir_specs:
        files.update(scan_directory(datasrc_dir, d, exts))

    toplevel_extensions = ["png"]
    if os.path.isdir(datasrc_dir):
        for f in os.listdir(datasrc_dir):
            full = os.path.join(datasrc_dir, f)
            if os.path.isfile(full) and any(f.endswith("." + e) for e in toplevel_extensions):
                files.add(f)
    return files


def generate_manifest(datasrc_dir, manifest_path):
    files = set()
    files.update(content_py_files(datasrc_dir))
    files.update(collect_additional_files(datasrc_dir))
    files.discard("")
    sorted_files = sorted(files)

    os.makedirs(os.path.dirname(os.path.abspath(manifest_path)), exist_ok=True)
    with open(manifest_path, "w") as f:
        for p in sorted_files:
            f.write(p + "\n")
    print("manifest_tool: wrote %d entries to %s" % (len(sorted_files), manifest_path))
    return 0


def read_manifest(manifest_path):
    with open(manifest_path, "r") as f:
        return [line.strip() for line in f if line.strip()]


def validate_manifest(datasrc_dir, manifest_path):
    if not os.path.exists(manifest_path):
        print("ERROR: manifest file '%s' not found. Run 'manifest_tool.py generate' first." % manifest_path)
        return 1
    entries = read_manifest(manifest_path)
    missing = []
    for e in entries:
        full = os.path.join(datasrc_dir, e)
        if not os.path.exists(full):
            missing.append(e)
    if missing:
        print("ERROR: %d file(s) listed in manifest are missing from %s/:" % (len(missing), datasrc_dir))
        for m in missing[:20]:
            print("  - %s/%s" % (datasrc_dir, m))
        if len(missing) > 20:
            print("  ... and %d more" % (len(missing) - 20))
        return 1
    print("manifest_tool: all %d files present in %s/" % (len(entries), datasrc_dir))
    return 0


def check_extra_files(datasrc_dir, manifest_path):
    if not os.path.exists(manifest_path):
        print("ERROR: manifest file '%s' not found." % manifest_path)
        return 1
    manifest_entries = set(read_manifest(manifest_path))
    disk_entries = all_resource_files_on_disk(datasrc_dir)
    extra = disk_entries - manifest_entries
    if extra:
        print("WARNING: %d resource file(s) exist in %s/ but are NOT listed in the manifest:" % (len(extra), datasrc_dir))
        for m in sorted(extra):
            print("  - %s/%s" % (datasrc_dir, m))
        print("If these files should be shipped, add them to datasrc/content.py or the relevant subdirectory and re-run 'manifest_tool.py generate'.")
        return 0
    print("manifest_tool: no extra resource files found on disk outside manifest.")
    return 0


def check_staleness(inputs, outputs):
    newest_input = 0
    missing_inputs = []
    for i in inputs:
        if not os.path.exists(i):
            missing_inputs.append(i)
            continue
        mtime = os.path.getmtime(i)
        if mtime > newest_input:
            newest_input = mtime
    if missing_inputs:
        print("ERROR: staleness check: missing input file(s):")
        for m in missing_inputs:
            print("  - %s" % m)
        return 1

    oldest_output = float("inf")
    missing_outputs = []
    for o in outputs:
        if not os.path.exists(o):
            missing_outputs.append(o)
            continue
        mtime = os.path.getmtime(o)
        if mtime < oldest_output:
            oldest_output = mtime
    if missing_outputs:
        print("STALE: generated output(s) do not exist, need regeneration:")
        for m in missing_outputs:
            print("  - %s" % m)
        return 1

    if newest_input > oldest_output:
        print("STALE: one or more inputs are newer than the generated outputs.")
        print("  Newest input mtime: %s" % newest_input)
        print("  Oldest output mtime: %s" % oldest_output)
        return 1

    print("manifest_tool: generated outputs are up-to-date.")
    return 0


def validate_content_consistency(datasrc_dir, manifest_path):
    if not os.path.exists(manifest_path):
        print("ERROR: manifest not found, cannot validate content.py consistency")
        return 1
    manifest_entries = set(read_manifest(manifest_path))
    content_entries = content_py_files(datasrc_dir)
    content_entries.discard("")
    missing_from_manifest = content_entries - manifest_entries
    if missing_from_manifest:
        print("ERROR: %d file(s) referenced in %s/content.py are NOT in the manifest:" % (len(missing_from_manifest), datasrc_dir))
        for m in sorted(missing_from_manifest):
            print("  - %s" % m)
        print("Re-run 'manifest_tool.py generate' to refresh the manifest.")
        return 1
    missing_on_disk = []
    for e in sorted(content_entries):
        full = os.path.join(datasrc_dir, e)
        if not os.path.exists(full):
            missing_on_disk.append(e)
    if missing_on_disk:
        print("ERROR: %d file(s) referenced in %s/content.py are missing from disk:" % (len(missing_on_disk), datasrc_dir))
        for m in missing_on_disk:
            print("  - %s/%s" % (datasrc_dir, m))
        return 1
    print("manifest_tool: content.py references (%d files) are consistent with manifest and disk." % len(content_entries))
    return 0


CATEGORIES = [
    "audio",
    "countryflags",
    "editor",
    "fonts",
    "languages",
    "mapres",
    "maps",
    "skins",
    "ui",
    "shader",
    "root",
]


def categorize_entries(entries):
    result = {cat: [] for cat in CATEGORIES}
    for e in entries:
        if "/" in e:
            cat = e.split("/", 1)[0]
        else:
            cat = "root"
        if cat not in result:
            result[cat] = []
        result[cat].append(e)
    return result


def summarize_manifest(manifest_path):
    if not os.path.exists(manifest_path):
        print("ERROR: manifest file '%s' not found." % manifest_path)
        return 1
    entries = read_manifest(manifest_path)
    cats = categorize_entries(entries)
    print("manifest summary (%d total entries):" % len(entries))
    for cat in CATEGORIES:
        items = cats.get(cat, [])
        if items:
            print("  %-16s %4d" % (cat + ":", len(items)))
    empty = [c for c in CATEGORIES if c != "shader" and not cats.get(c)]
    if empty:
        print("WARNING: expected categories with ZERO entries: %s" % ", ".join(empty))
    return 0


def check_install_dir(manifest_path, install_dir):
    if not os.path.exists(manifest_path):
        print("ERROR: manifest file '%s' not found." % manifest_path)
        return 1
    if not os.path.isdir(install_dir):
        print("ERROR: install data dir '%s' does not exist or is not a directory." % install_dir)
        return 1

    manifest_entries = set(read_manifest(manifest_path))
    cats = categorize_entries(manifest_entries)

    missing = []
    for e in sorted(manifest_entries):
        full = os.path.join(install_dir, e)
        if not os.path.isfile(full):
            missing.append(e)

    disk_files = set()
    for root, _dirs, fnames in os.walk(install_dir):
        for f in fnames:
            full = os.path.join(root, f)
            rel = os.path.relpath(full, install_dir)
            rel = rel.replace(os.sep, "/")
            if rel == "data_manifest.txt":
                continue
            disk_files.add(rel)

    extra = sorted(disk_files - manifest_entries)

    ret = 0
    if missing:
        ret = 1
        missing_cats = categorize_entries(missing)
        print("ERROR: %d file(s) from manifest are MISSING from install dir '%s':" % (len(missing), install_dir))
        for cat in CATEGORIES:
            items = missing_cats.get(cat, [])
            if items:
                print("  [%s] %d missing:" % (cat, len(items)))
                for m in items[:5]:
                    print("    - %s" % m)
                if len(items) > 5:
                    print("    ... and %d more" % (len(items) - 5))

    if extra:
        ret = 1
        extra_cats = categorize_entries(extra)
        print("ERROR: %d file(s) in install dir '%s' are NOT in manifest (stale/undeclared):" % (len(extra), install_dir))
        for cat in sorted(extra_cats.keys()):
            items = extra_cats[cat]
            if items:
                print("  [%s] %d extra:" % (cat, len(items)))
                for m in items[:5]:
                    print("    - %s" % m)
                if len(items) > 5:
                    print("    ... and %d more" % (len(items) - 5))

    if not ret:
        print("manifest_tool: install dir '%s' matches manifest exactly (%d files, 0 missing, 0 extra)" % (install_dir, len(manifest_entries)))
        print("  category breakdown:")
        for cat in CATEGORIES:
            items = cats.get(cat, [])
            if items:
                print("    %-16s %4d" % (cat + ":", len(items)))
    return ret


def main():
    parser = argparse.ArgumentParser(description="Teeworlds data resource manifest tool")
    parser.add_argument("--datasrc", default=DEFAULT_DATASRC_DIR, help="Path to datasrc directory (default: datasrc)")
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST_FILE, help="Path to manifest file (default: data_manifest.txt)")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("generate", help="Generate data_manifest.txt from content.py + datasrc directory scan")
    sub.add_parser("validate", help="Validate that all manifest entries exist on disk")
    sub.add_parser("validate_content", help="Validate that content.py references are in manifest and on disk")
    sub.add_parser("check_extra", help="Warn about resource files on disk that are not in manifest")
    sub.add_parser("summarize", help="Print per-category summary of manifest entries")

    sp_stale = sub.add_parser("check_staleness", help="Check if generated outputs are stale relative to inputs")
    sp_stale.add_argument("--inputs", nargs="+", required=True, help="Input files (e.g. content.py)")
    sp_stale.add_argument("--outputs", nargs="+", required=True, help="Generated output files (e.g. client_data.cpp)")

    sp_checkdir = sub.add_parser("check_install_dir", help="Verify an install/build data dir matches manifest exactly (no missing, no extra/stale files)")
    sp_checkdir.add_argument("--install-dir", required=True, help="Path to installed data directory (e.g. build/data)")

    args = parser.parse_args()

    if args.command == "generate":
        sys.exit(generate_manifest(args.datasrc, args.manifest))
    elif args.command == "validate":
        sys.exit(validate_manifest(args.datasrc, args.manifest))
    elif args.command == "validate_content":
        sys.exit(validate_content_consistency(args.datasrc, args.manifest))
    elif args.command == "check_extra":
        sys.exit(check_extra_files(args.datasrc, args.manifest))
    elif args.command == "check_staleness":
        sys.exit(check_staleness(args.inputs, args.outputs))
    elif args.command == "summarize":
        sys.exit(summarize_manifest(args.manifest))
    elif args.command == "check_install_dir":
        sys.exit(check_install_dir(args.manifest, args.install_dir))


if __name__ == "__main__":
    main()
