import json
import os
import re
import shutil
from typing import Dict, List, Optional, Tuple


class ReleaseManifest:
    def __init__(self, manifest_path: str = None):
        if manifest_path is None:
            manifest_path = os.path.join(
                os.path.dirname(os.path.realpath(__file__)),
                "release_manifest.json"
            )
        self.manifest_path = manifest_path
        self.manifest_dir = os.path.dirname(manifest_path)
        self.project_root = os.path.dirname(self.manifest_dir)
        self._load()

    def _load(self):
        with open(self.manifest_path, 'r', encoding='utf-8') as f:
            self.data = json.load(f)

    def get_version(self) -> str:
        version_source = self.data.get("version_source", {})
        if version_source.get("type") == "header":
            header_path = os.path.join(self.project_root, version_source["file"])
            macro = version_source["macro"]
            with open(header_path, 'r', encoding='utf-8') as f:
                for line in f:
                    match = re.search(rf'{macro}\s*\[\d+\]\s*=\s*"([^"]+)"', line)
                    if match:
                        return match.group(1)
                    match = re.search(rf'#define\s+{macro}\s+"([^"]+)"', line)
                    if match:
                        return match.group(1)
        raise RuntimeError("Could not determine version from manifest")

    def get_valid_platforms(self) -> List[str]:
        return list(self.data.get("package_formats", {}).keys())

    def get_package_format(self, platform: str) -> Dict:
        return self.data.get("package_formats", {}).get(platform, {})

    def get_build_output_dir(self, platform: str, cmake_build: bool = False) -> str:
        dirs = self.data.get("build_output_dirs", {})
        if cmake_build:
            return dirs.get("cmake_build", ".")
        if platform in ("win32", "linux_x86"):
            return dirs.get("x86_release", "build/x86/release")
        return dirs.get("x86_64_release", "build/x86_64/release")

    def get_data_manifest_path(self) -> str:
        dm = self.data.get("data_manifest", {})
        return os.path.join(self.project_root, dm.get("file", "data_manifest.txt"))

    def get_data_files(self) -> List[str]:
        dm_path = self.get_data_manifest_path()
        files = []
        if os.path.exists(dm_path):
            with open(dm_path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        files.append(line)
        return files

    def get_items_for_platform(
        self,
        platform: str,
        include_optional: bool = False,
        include_tools: bool = False
    ) -> Dict[str, List[Dict]]:
        result = {}
        categories = self.data.get("categories", {})

        for category_name, category_data in categories.items():
            items = []
            for item in category_data.get("items", []):
                if platform not in item.get("platforms", []):
                    continue

                if category_name == "optional_debug" and not include_optional:
                    if not item.get("default_include", False):
                        continue

                if category_name == "tools" and not include_tools:
                    continue

                items.append(item)

            if items:
                result[category_name] = items

        return result

    def resolve_item_path(
        self,
        item: Dict,
        platform: str,
        build_dir: Optional[str] = None
    ) -> Tuple[str, str]:
        source_path = None
        dest_path = None

        if item.get("source_build"):
            if build_dir is None:
                build_dir = self.get_build_output_dir(platform)
            base_dir = os.path.join(self.project_root, build_dir)

            if "platform_names" in item:
                filename = item["platform_names"].get(platform, item.get("name"))
            elif "path" in item:
                filename = item["path"]
            else:
                filename = item["name"]
                if platform.startswith("win") and item.get("type") == "binary":
                    filename += ".exe"

            source_path = os.path.join(base_dir, filename)
            if "source_subdir" in item:
                source_path = os.path.join(base_dir, item["source_subdir"])
        elif item.get("system_path"):
            source_path = item["path"]
        else:
            if "path" in item:
                source_path = os.path.join(self.project_root, item["path"])
            elif "paths" in item:
                source_path = [os.path.join(self.project_root, p) for p in item["paths"]]

        if "dest" in item:
            dest_path = item["dest"]
        elif "dest_bundle" in item:
            dest_path = item["dest_bundle"]
        elif "path" in item and not item.get("source_build"):
            dest_path = os.path.basename(item["path"])
        elif "platform_names" in item:
            dest_path = item["platform_names"].get(platform, item.get("name"))
        else:
            dest_path = item.get("name", "")

        return source_path, dest_path

    def collect_files(
        self,
        platform: str,
        build_dir: Optional[str] = None,
        include_optional: bool = False,
        include_tools: bool = False,
        verify_exists: bool = True
    ) -> Dict[str, List[Tuple[str, str, str]]]:
        items_by_category = self.get_items_for_platform(platform, include_optional, include_tools)
        collected = {}

        for category, items in items_by_category.items():
            collected[category] = []
            for item in items:
                if item.get("external"):
                    continue
                if item.get("template"):
                    continue

                source, dest = self.resolve_item_path(item, platform, build_dir)

                if isinstance(source, list):
                    for i, s in enumerate(source):
                        if verify_exists and not self._check_exists(s, item.get("type")):
                            print(f"WARNING: Missing {category} item '{item['name']}' at {s}")
                            continue
                        d = os.path.basename(s) if isinstance(dest, list) else dest
                        collected[category].append((item["name"], s, d))
                else:
                    if verify_exists and not self._check_exists(source, item.get("type")):
                        print(f"WARNING: Missing {category} item '{item['name']}' at {source}")
                        continue
                    collected[category].append((item["name"], source, dest))

        return collected

    def _check_exists(self, path: str, item_type: str) -> bool:
        if not path:
            return False
        if item_type in ("directory", "directory_list"):
            return os.path.isdir(path)
        return os.path.isfile(path)

    def copy_files_to_package(
        self,
        collected: Dict[str, List[Tuple[str, str, str]]],
        package_dir: str,
        platform: str,
        use_bundle: bool = False
    ):
        for category, items in collected.items():
            for name, source, dest in items:
                if use_bundle and category == "platform_dependencies" and platform == "macos":
                    target_path = os.path.join(package_dir, dest)
                else:
                    target_path = os.path.join(package_dir, dest) if dest else package_dir

                target_dir = os.path.dirname(target_path)
                if target_dir and not os.path.exists(target_dir):
                    os.makedirs(target_dir, exist_ok=True)

                if os.path.isdir(source):
                    if os.path.exists(target_path):
                        shutil.rmtree(target_path)
                    shutil.copytree(source, target_path)
                else:
                    shutil.copy2(source, target_path)

    def print_summary(
        self,
        platform: str,
        include_optional: bool = False,
        include_tools: bool = False
    ):
        items_by_category = self.get_items_for_platform(platform, include_optional, include_tools)
        version = self.get_version()

        print(f"Teeworlds {version} Release Manifest for platform: {platform}")
        print("=" * 70)

        total = 0
        for category, items in sorted(items_by_category.items()):
            category_desc = self.data["categories"][category].get("description", category)
            print(f"\n{category.upper()} ({category_desc}):")
            print(f"  {'Name':<25} {'Type':<12} {'Platforms'}")
            print(f"  {'-'*25} {'-'*12} {'-'*20}")
            for item in items:
                item_type = item.get("type", "file")
                platforms = ",".join(item.get("platforms", []))
                print(f"  {item['name']:<25} {item_type:<12} {platforms}")
                total += 1

        print(f"\n{'='*70}")
        print(f"Total items to include: {total}")

    def generate_cmake_variables(
        self,
        platform: str,
        include_optional: bool = False,
        include_tools: bool = False
    ) -> Dict[str, List[str]]:
        collected = self.collect_files(platform, include_optional=include_optional,
                                       include_tools=include_tools, verify_exists=False)
        cmake_vars = {
            "CPACK_TARGETS": [],
            "CPACK_DIRS": [],
            "CPACK_FILES": [],
        }

        for category, items in collected.items():
            for name, source, dest in items:
                rel_source = os.path.relpath(source, self.project_root) if os.path.isabs(source) else source
                if category == "required" and name in ("client_binary", "server_binary"):
                    cmake_vars["CPACK_TARGETS"].append(name.replace("_binary", ""))
                elif os.path.isdir(source) if os.path.exists(source) else name.endswith("_directory"):
                    cmake_vars["CPACK_DIRS"].append(rel_source)
                else:
                    cmake_vars["CPACK_FILES"].append(rel_source)

        return cmake_vars


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Teeworlds Release Manifest Tool")
    parser.add_argument("--platform", help="Target platform",
                        choices=["win32", "win64", "macos", "linux_x86", "linux_x86_64", "src"])
    parser.add_argument("--manifest", help="Path to release manifest JSON", default=None)
    parser.add_argument("--include-optional", action="store_true", help="Include optional debug files")
    parser.add_argument("--include-tools", action="store_true", help="Include development tools")
    parser.add_argument("--version", action="store_true", help="Print version and exit")
    parser.add_argument("--list-platforms", action="store_true", help="List valid platforms")
    parser.add_argument("--summary", action="store_true", help="Print summary of files for platform")
    parser.add_argument("--cmake-vars", action="store_true", help="Output CMake variables for platform")
    parser.add_argument("--build-dir", help="Build output directory override", default=None)

    args = parser.parse_args()

    manifest = ReleaseManifest(args.manifest)

    if args.version:
        print(manifest.get_version())
        return

    if args.list_platforms:
        print("Valid platforms:")
        for p in manifest.get_valid_platforms():
            fmt = manifest.get_package_format(p)
            print(f"  {p:<15} format={fmt.get('format', '?')}")
        return

    if not args.platform:
        parser.error("--platform is required for this operation")

    if args.summary:
        manifest.print_summary(args.platform, args.include_optional, args.include_tools)

    if args.cmake_vars:
        cmake_vars = manifest.generate_cmake_variables(args.platform, args.include_optional, args.include_tools)
        for var, values in cmake_vars.items():
            print(f"set({var}")
            for v in values:
                print(f"  {v}")
            print(")")


if __name__ == "__main__":
    main()
