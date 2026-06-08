import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import zipfile

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
from release_manifest import ReleaseManifest, ManifestError


def detect_platform_from_system():
    import platform
    system = platform.system()
    machine = platform.machine()
    is_64 = sys.maxsize > 2 ** 32
    if system == "Darwin":
        return "macos"
    elif system == "Windows":
        return "win64" if is_64 else "win32"
    elif system == "Linux":
        return "linux_x86_64" if is_64 else "linux_x86"
    return None


def run_cmd(cmd, cwd=None, env=None):
    print(f"  $ {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed: {cmd}")
    return result


def download_external_resources(url, dest_dir):
    import tempfile
    import zipfile
    try:
        import urllib.request
        tmp_fd, tmp_path = tempfile.mkstemp(suffix='.zip')
        os.close(tmp_fd)
        print(f"  Downloading {url} ...")
        urllib.request.urlretrieve(url, tmp_path)
        with zipfile.ZipFile(tmp_path, 'r') as zf:
            zf.extractall(dest_dir)
            top = zf.namelist()[0].split('/')[0]
        os.unlink(tmp_path)
        return os.path.join(dest_dir, top)
    except Exception as e:
        print(f"  WARNING: Download failed: {e}")
        return None


def do_stage(args):
    manifest = ReleaseManifest(args.manifest)
    version = manifest.get_version()

    if not args.version:
        args.version = version
    if not args.platform:
        args.platform = detect_platform_from_system()
        if not args.platform:
            print("ERROR: Could not detect platform, use --platform")
            sys.exit(1)

    if args.platform not in manifest.get_valid_platforms():
        print(f"ERROR: Invalid platform '{args.platform}'. Valid: {manifest.get_valid_platforms()}")
        sys.exit(1)

    print(f"\n=== Stage: teeworlds {args.version} [{args.platform}] ===")
    print(f"  Include optional debug: {args.include_optional}")
    print(f"  Include tools: {args.include_tools}")
    print(f"  Strict: {args.strict}")
    print(f"  Allow extra files: {args.allow_extra_files}")
    print(f"  Output dir: {args.output}")

    if os.path.exists(args.output):
        print(f"  Cleaning existing {args.output}")
        shutil.rmtree(args.output)
    os.makedirs(args.output, exist_ok=True)

    external_dirs = {}
    if args.download_external:
        print("  Downloading external resources...")
        tmp_ext = os.path.join(args.output, "_tmp_ext")
        os.makedirs(tmp_ext, exist_ok=True)
        if args.url_languages:
            lang_dir = download_external_resources(args.url_languages, tmp_ext)
            if lang_dir and os.path.isdir(lang_dir):
                external_dirs["languages"] = lang_dir
        if args.url_maps:
            maps_dir = download_external_resources(args.url_maps, tmp_ext)
            if maps_dir and os.path.isdir(maps_dir):
                external_dirs["maps"] = maps_dir

    try:
        collected = manifest.collect_files(
            args.platform,
            build_dir=args.build_dir,
            include_optional=args.include_optional,
            include_tools=args.include_tools,
            verify_exists=True,
            strict=args.strict
        )
    except ManifestError as e:
        print(f"\nFATAL [ManifestError]: {e}")
        print("Aborted: required files are missing.")
        sys.exit(1)

    total = sum(len(v) for v in collected["items"].values()) + len(collected["data_files"])
    print(f"  Items: {sum(len(v) for v in collected['items'].values())} manifest items + "
          f"{len(collected['data_files'])} data files = {total} total")

    manifest.copy_files_to_package(
        collected,
        args.output,
        args.platform,
        use_bundle=args.use_bundle,
        external_dirs=external_dirs
    )

    staged_count = len(manifest._staged_files)
    print(f"  Staged {staged_count} files to {args.output}")

    errors = manifest.validate_staging_directory(
        args.output,
        collected,
        args.platform,
        allow_extra=args.allow_extra_files
    )
    if errors:
        print("\nVALIDATION FAILED - undeclared files detected in package:")
        for e in errors:
            print(f"  {e}")
        if not args.allow_extra_files:
            print("\nUse --allow-extra-files to bypass (not recommended).")
            sys.exit(1)
    else:
        print(f"  Validation OK - all {staged_count} files declared in manifest")

    manifest_info = {
        "version": args.version,
        "platform": args.platform,
        "include_optional": args.include_optional,
        "include_tools": args.include_tools,
        "staged_files_count": staged_count,
        "categories": {},
    }
    for cat, items in collected["items"].items():
        manifest_info["categories"][cat] = [
            {"name": it["name"], "dest": it.get("dest", "")}
            for it in items if not it.get("expanded")
        ]
    manifest_info["categories"]["data_manifest_entries"] = len(collected["data_files"])
    info_path = os.path.join(args.output, ".release_manifest.json")
    with open(info_path, 'w', encoding='utf-8') as f:
        json.dump(manifest_info, f, indent=2, ensure_ascii=False)
    manifest._staged_files.add(os.path.relpath(os.path.realpath(info_path), os.path.realpath(args.output)))

    print(f"\n=== Stage complete: {args.output} ===")
    return args.output


def do_package(args):
    if not os.path.isdir(args.stage_dir):
        print(f"ERROR: Stage directory not found: {args.stage_dir}")
        sys.exit(1)

    manifest = ReleaseManifest(args.manifest)
    if not args.platform:
        args.platform = detect_platform_from_system()
    if not args.version:
        args.version = manifest.get_version()

    fmt = args.format
    if not fmt:
        pkg_fmt = manifest.get_package_format(args.platform or "")
        fmt = pkg_fmt.get("format", "tar.gz")

    ext = fmt if fmt != "tgz" else "tar.gz"
    if ext == "txz":
        ext = "tar.xz"

    out_name = f"teeworlds-{args.version}-{args.platform}.{ext}"
    out_file = os.path.join(os.path.dirname(os.path.abspath(args.stage_dir)) or ".", out_name)

    stage_dir = args.stage_dir.rstrip('/')
    stage_parent = os.path.dirname(stage_dir) or "."
    stage_base = os.path.basename(stage_dir)

    print(f"\n=== Package: {fmt} -> {out_file} ===")

    if os.path.exists(out_file):
        os.unlink(out_file)

    if fmt == "zip":
        with zipfile.ZipFile(out_file, 'w', zipfile.ZIP_DEFLATED) as zf:
            for root, _dirs, files in os.walk(stage_dir):
                for fn in files:
                    full = os.path.join(root, fn)
                    arc = os.path.join(stage_base, os.path.relpath(full, stage_dir))
                    zf.write(full, arc)
    elif fmt in ("tar.gz", "tgz"):
        with tarfile.open(out_file, 'w:gz') as tf:
            tf.add(stage_dir, arcname=stage_base)
    elif fmt in ("tar.xz", "txz"):
        with tarfile.open(out_file, 'w:xz') as tf:
            tf.add(stage_dir, arcname=stage_base)
    elif fmt == "tar":
        with tarfile.open(out_file, 'w') as tf:
            tf.add(stage_dir, arcname=stage_base)
    elif fmt == "dmg":
        dmg_candidates = [
            (["hdiutil"], "--hdiutil"),
        ]
        dmg_cmd = None
        for tool_list, flag in dmg_candidates:
            for tool in tool_list:
                if shutil.which(tool):
                    dmg_cmd = ["python3", os.path.join(os.path.dirname(__file__), "dmg.py"),
                               "create", flag, tool]
                    break
            if dmg_cmd:
                break
        if not dmg_cmd:
            print("ERROR: dmg requires hdiutil (macOS)")
            sys.exit(1)
        tmp_root = os.path.join(os.path.dirname(out_file), "_dmg_tmp")
        if os.path.exists(tmp_root):
            shutil.rmtree(tmp_root)
        os.makedirs(tmp_root)
        inner = os.path.join(tmp_root, f"teeworlds-{args.version}")
        shutil.copytree(stage_dir, inner)
        dmg_cmd.extend([out_file, f"Teeworlds {args.version}", tmp_root])
        run_cmd(dmg_cmd)
        shutil.rmtree(tmp_root)
    else:
        print(f"ERROR: Unknown format '{fmt}'")
        sys.exit(1)

    size_kb = os.path.getsize(out_file) / 1024
    print(f"=== Package complete: {out_file} ({size_kb:.0f} KB) ===")
    return out_file


def do_list(args):
    manifest = ReleaseManifest(args.manifest)
    if not args.platform:
        print("Valid platforms:")
        for p in manifest.get_valid_platforms():
            pkg = manifest.get_package_format(p)
            print(f"  {p:<14} -> {pkg.get('format', '?')}")
        return
    manifest.print_summary(args.platform, args.include_optional, args.include_tools)


def main():
    parser = argparse.ArgumentParser(
        description="Teeworlds Unified Release Packager - single entry point for staging and packaging via release manifest",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Stage files for current platform with strict validation
  %(prog)s stage --output /tmp/tw_stage

  # Stage with optional debug symbols and tools
  %(prog)s stage --platform win64 --include-optional --include-tools

  # Package a staged directory as tar.gz
  %(prog)s package --stage-dir /tmp/tw_stage --platform linux_x86_64 --format tar.gz

  # List manifest contents for a platform
  %(prog)s list --platform macos
        """,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    stage_p = sub.add_parser("stage", help="Stage files to a directory using the manifest with strict validation")
    stage_p.add_argument("--platform", choices=["win32", "win64", "macos", "linux_x86", "linux_x86_64", "src"])
    stage_p.add_argument("--version")
    stage_p.add_argument("--output", "-o", default="teeworlds_stage", help="Staging output directory")
    stage_p.add_argument("--build-dir", help="Override build output dir (e.g. build/x86_64/release)")
    stage_p.add_argument("--include-optional", action="store_true", help="Include optional_debug (pdb, dSYM)")
    stage_p.add_argument("--include-tools", action="store_true", help="Include tools (mastersrv, versionsrv, ...)")
    stage_p.add_argument("--no-strict", dest="strict", action="store_false", default=True,
                         help="Do not fail on missing required items (not recommended)")
    stage_p.add_argument("--allow-extra-files", action="store_true", help="Allow undeclared files in staging dir")
    stage_p.add_argument("--use-bundle", action="store_true", help="Use macOS bundle structure")
    stage_p.add_argument("--manifest")
    stage_p.add_argument("--download-external", action="store_true", help="Download languages + maps")
    stage_p.add_argument("--url-languages", default="https://github.com/teeworlds/teeworlds-translation/archive/master.zip")
    stage_p.add_argument("--url-maps", default="https://github.com/teeworlds/teeworlds-maps/archive/master.zip")

    pkg_p = sub.add_parser("package", help="Package a staged directory into an archive")
    pkg_p.add_argument("--stage-dir", required=True)
    pkg_p.add_argument("--platform")
    pkg_p.add_argument("--version")
    pkg_p.add_argument("--format", choices=["zip", "tar.gz", "tgz", "tar.xz", "txz", "tar", "dmg"])
    pkg_p.add_argument("--manifest")

    list_p = sub.add_parser("list", help="List manifest contents for a platform")
    list_p.add_argument("--platform", choices=["win32", "win64", "macos", "linux_x86", "linux_x86_64", "src"])
    list_p.add_argument("--include-optional", action="store_true")
    list_p.add_argument("--include-tools", action="store_true")
    list_p.add_argument("--manifest")

    args = parser.parse_args()
    if args.command == "stage":
        do_stage(args)
    elif args.command == "package":
        do_package(args)
    elif args.command == "list":
        do_list(args)


if __name__ == "__main__":
    main()
