import json
import os
import re
import shutil
from typing import Dict, List, Optional, Tuple, Set


class ManifestError(Exception):
    pass


class ManifestSchemaError(ManifestError):
    """Raised when release_manifest.json itself violates the data contract schema."""
    pass


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
        self._staged_files: Set[str] = set()

    def _load(self):
        with open(self.manifest_path, 'r', encoding='utf-8') as f:
            self.data = json.load(f)
        self.validate_manifest_schema()

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
        raise ManifestError("Could not determine version from manifest")

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

    def get_data_base_dir(self) -> str:
        dm = self.data.get("data_manifest", {})
        return dm.get("base_dir", "data")

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
    ) -> Tuple[object, str]:
        source_path = None
        dest_path = None

        if item.get("source_build"):
            if build_dir is None:
                build_dir = self.get_build_output_dir(platform)
            base_dir = os.path.join(self.project_root, build_dir)

            if "source_subdir" in item:
                source_path = os.path.join(base_dir, item["source_subdir"])
            elif "platform_names" in item:
                filename = item["platform_names"].get(platform, item.get("name"))
                source_path = os.path.join(base_dir, filename)
            elif "path" in item:
                source_path = os.path.join(base_dir, item["path"])
            else:
                filename = item["name"]
                if platform.startswith("win") and item.get("type") == "binary":
                    filename += ".exe"
                source_path = os.path.join(base_dir, filename)

        elif item.get("system_path"):
            source_path = item["path"]

        else:
            if "path" in item:
                source_path = os.path.join(self.project_root, item["path"])
            elif "paths" in item:
                source_path = [os.path.join(self.project_root, p) for p in item["paths"]]

        if "dest_bundle" in item:
            dest_path = item["dest_bundle"]
        elif "dest" in item:
            dest_path = item["dest"]
        elif "path" in item and not item.get("source_build") and not item.get("system_path"):
            dest_path = os.path.basename(item["path"])
        elif "platform_names" in item:
            dest_path = item["platform_names"].get(platform, item.get("name"))
        else:
            dest_path = item.get("name", "")

        return source_path, dest_path

    def render_cmake_template(self, template_path: str, context: Dict[str, str]) -> str:
        with open(template_path, 'r', encoding='utf-8') as f:
            content = f.read()
        import re
        def repl(m):
            key = m.group(1)
            return context.get(key, m.group(0))
        return re.sub(r'\$\{([A-Za-z0-9_]+)\}', repl, content)

    def _check_exists(self, path: str, item_type: str) -> bool:
        if not path:
            return False
        if item_type in ("directory", "directory_list"):
            return os.path.isdir(path)
        return os.path.isfile(path)

    def _expand_data_files(self, build_dir: Optional[str]) -> List[Tuple[str, str]]:
        data_base = self.get_data_base_dir()
        data_files = self.get_data_files()
        result = []

        build_data_dir = None
        if build_dir:
            candidate = os.path.join(self.project_root, build_dir, data_base)
            if os.path.isdir(candidate):
                build_data_dir = candidate

        source_data_dir = os.path.join(self.project_root, "datasrc")
        plain_data_dir = os.path.join(self.project_root, data_base)

        for rel_path in data_files:
            src = None
            if build_data_dir:
                candidate = os.path.join(build_data_dir, rel_path)
                if os.path.isfile(candidate):
                    src = candidate
            if src is None and os.path.isdir(source_data_dir):
                candidate = os.path.join(source_data_dir, rel_path)
                if os.path.isfile(candidate):
                    src = candidate
            if src is None:
                candidate = os.path.join(plain_data_dir, rel_path)
                if os.path.isfile(candidate):
                    src = candidate
            if src is None:
                if build_data_dir:
                    src = os.path.join(build_data_dir, rel_path)
                elif os.path.isdir(source_data_dir):
                    src = os.path.join(source_data_dir, rel_path)
                else:
                    src = os.path.join(plain_data_dir, rel_path)

            dst = os.path.join(data_base, rel_path)
            result.append((src, dst))

        return result

    def collect_files(
        self,
        platform: str,
        build_dir: Optional[str] = None,
        include_optional: bool = False,
        include_tools: bool = False,
        verify_exists: bool = True,
        strict: bool = True
    ) -> Dict:
        items_by_category = self.get_items_for_platform(platform, include_optional, include_tools)
        collected: Dict = {
            "items": {},
            "data_files": [],
            "missing_required": [],
            "missing_optional": [],
        }

        version = self.get_version()
        template_context = {
            "PROJECT_VERSION": version,
            "TARGET_CLIENT": "teeworlds",
            "TARGET_SERVER": "teeworlds_srv",
            "TARGET_SERVER_LAUNCHER": "teeworlds_server",
        }

        for category, items in items_by_category.items():
            collected["items"][category] = []
            for item in items:
                if item.get("external"):
                    continue

                if item["name"] == "data_directory":
                    data_pairs = self._expand_data_files(build_dir)
                    missing_data = []
                    valid_data = []
                    for src, dst in data_pairs:
                        if verify_exists and not os.path.isfile(src):
                            missing_data.append(src)
                        else:
                            valid_data.append((src, dst))
                    collected["data_files"] = valid_data
                    if missing_data and strict:
                        raise ManifestError(
                            f"Missing {len(missing_data)} data files declared in data_manifest.txt. "
                            f"First missing: {missing_data[0]}"
                        )
                    collected["items"][category].append({
                        "name": item["name"],
                        "source": None,
                        "dest": self.get_data_base_dir(),
                        "type": item.get("type", "directory"),
                        "expanded": True,
                    })
                    continue

                source, dest = self.resolve_item_path(item, platform, build_dir)

                if isinstance(source, list):
                    valid_sources = []
                    for s in source:
                        if verify_exists and not self._check_exists(s, item.get("type")):
                            if category == "required" and strict:
                                raise ManifestError(
                                    f"Required {category} item '{item['name']}' missing at: {s}"
                                )
                            collected["missing_required" if category == "required" else "missing_optional"].append((item["name"], s))
                            continue
                        valid_sources.append(s)
                    for s in valid_sources:
                        d = os.path.basename(s) if isinstance(dest, list) else dest
                        collected["items"][category].append({
                            "name": item["name"],
                            "source": s,
                            "dest": d,
                            "type": item.get("type", "file"),
                        })
                else:
                    entry = {
                        "name": item["name"],
                        "source": source,
                        "dest": dest,
                        "type": item.get("type", "file"),
                    }
                    if item.get("template"):
                        entry["template"] = True
                        entry["template_context"] = template_context
                    if item.get("dest_bundle"):
                        entry["dest"] = item["dest_bundle"]
                    if verify_exists and not item.get("template") and not self._check_exists(source, item.get("type")):
                        if category == "required" and strict:
                            raise ManifestError(
                                f"Required {category} item '{item['name']}' missing at: {source}"
                            )
                        collected["missing_required" if category == "required" else "missing_optional"].append((item["name"], source))
                        continue
                    collected["items"][category].append(entry)

        return collected

    def copy_files_to_package(
        self,
        collected: Dict,
        package_dir: str,
        platform: str,
        use_bundle: bool = False,
        external_dirs: Optional[Dict[str, str]] = None
    ):
        external_dirs = external_dirs or {}
        self._staged_files.clear()

        if not os.path.exists(package_dir):
            os.makedirs(package_dir, exist_ok=True)

        for category, items in collected["items"].items():
            for entry in items:
                if entry.get("expanded"):
                    continue

                source = entry["source"]
                dest = entry["dest"]
                target_path = os.path.join(package_dir, dest) if dest else package_dir

                target_dir = os.path.dirname(target_path)
                if target_dir and not os.path.exists(target_dir):
                    os.makedirs(target_dir, exist_ok=True)

                if entry.get("template"):
                    ctx = entry.get("template_context", {})
                    rendered = self.render_cmake_template(source, ctx)
                    with open(target_path, 'w', encoding='utf-8') as f:
                        f.write(rendered)
                    rel = os.path.relpath(os.path.realpath(target_path), os.path.realpath(package_dir))
                    self._staged_files.add(rel)
                elif os.path.isdir(source):
                    self._copy_dir_tracked(source, target_path, package_dir)
                else:
                    self._copy_file_tracked(source, target_path, package_dir)

        for src, dst in collected["data_files"]:
            target_path = os.path.join(package_dir, dst)
            target_dir = os.path.dirname(target_path)
            if target_dir and not os.path.exists(target_dir):
                os.makedirs(target_dir, exist_ok=True)
            self._copy_file_tracked(src, target_path, package_dir)

        items_by_category = self.get_items_for_platform(platform)
        for category, items in items_by_category.items():
            for item in items:
                if not item.get("external"):
                    continue
                ext_key = item["name"]
                if ext_key not in external_dirs:
                    continue
                ext_src = external_dirs[ext_key]
                if item["name"] == "languages":
                    dst_base = os.path.join(package_dir, "data", "languages")
                elif item["name"] == "maps":
                    dst_base = os.path.join(package_dir, "data", "maps")
                else:
                    dst_base = os.path.join(package_dir, item.get("dest", os.path.basename(item["path"])))
                if not os.path.exists(dst_base):
                    os.makedirs(dst_base, exist_ok=True)
                self._copy_dir_tracked(ext_src, dst_base, package_dir)

    def _copy_file_tracked(self, src: str, dst: str, package_root: str):
        shutil.copy2(src, dst)
        rel = os.path.relpath(os.path.realpath(dst), os.path.realpath(package_root))
        self._staged_files.add(rel)

    def _copy_dir_tracked(self, src: str, dst: str, package_root: str):
        if os.path.exists(dst):
            shutil.rmtree(dst)
        shutil.copytree(src, dst)
        for root, _dirs, files in os.walk(dst):
            for fname in files:
                full = os.path.join(root, fname)
                rel = os.path.relpath(os.path.realpath(full), os.path.realpath(package_root))
                self._staged_files.add(rel)

    def validate_staging_directory(
        self,
        package_dir: str,
        collected: Dict,
        platform: str,
        allow_extra: bool = False
    ) -> List[str]:
        errors = []
        actual_files: Set[str] = set()

        pkg_real = os.path.realpath(package_dir)
        for root, _dirs, files in os.walk(pkg_real):
            for fname in files:
                full = os.path.join(root, fname)
                rel = os.path.relpath(full, pkg_real)
                actual_files.add(rel)

        if not allow_extra:
            extras = actual_files - self._staged_files
            for extra in sorted(extras):
                errors.append(f"UNDECLARED FILE in package: {extra}")

        return errors

    def render_template(
        self,
        item_name: str,
        platform: str,
        context: Dict[str, str]
    ) -> Optional[str]:
        categories = self.data.get("categories", {})
        for cat_data in categories.values():
            for item in cat_data.get("items", []):
                if item.get("name") == item_name and item.get("template"):
                    if platform not in item.get("platforms", []):
                        return None
                    src_path = os.path.join(self.project_root, item["path"])
                    with open(src_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                    for key, value in context.items():
                        content = content.replace(f"@{key}@", value)
                    return content
        return None

    def get_declared_dest_paths(
        self,
        platform: str,
        include_optional: bool = False,
        include_tools: bool = False,
        use_bundle: bool = False
    ) -> Set[str]:
        paths: Set[str] = set()
        items_by_category = self.get_items_for_platform(platform, include_optional, include_tools)

        for category, items in items_by_category.items():
            for item in items:
                if item.get("template"):
                    continue
                if item.get("external"):
                    if item["name"] == "languages":
                        paths.add("data/languages")
                    elif item["name"] == "maps":
                        paths.add("data/maps")
                    continue
                if item["name"] == "data_directory":
                    data_base = self.get_data_base_dir()
                    for df in self.get_data_files():
                        paths.add(os.path.join(data_base, df))
                    continue

                _src, dest = self.resolve_item_path(item, platform)
                if dest:
                    paths.add(dest)

        return paths

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
                                       include_tools=include_tools, verify_exists=False, strict=False)
        cmake_vars = {
            "CPACK_TARGETS": [],
            "CPACK_DIRS": [],
            "CPACK_FILES": [],
        }

        for category, items in collected["items"].items():
            for entry in items:
                name = entry["name"]
                source = entry["source"]
                if not source:
                    continue
                rel_source = os.path.relpath(source, self.project_root) if os.path.isabs(source) else source
                if category == "required" and name in ("client_binary", "server_binary"):
                    cmake_vars["CPACK_TARGETS"].append(name.replace("_binary", ""))
                elif entry["type"] in ("directory", "directory_list"):
                    cmake_vars["CPACK_DIRS"].append(rel_source)
                else:
                    cmake_vars["CPACK_FILES"].append(rel_source)

        return cmake_vars

    # ------------------------------------------------------------------
    # Strong schema validation — release manifest data contract
    # ------------------------------------------------------------------

    VALID_ITEM_TYPES = {"binary", "file", "directory", "directory_list", "file_list"}
    REQUIRED_TOP_FIELDS = {"manifest_version", "version_source", "data_manifest", "categories", "package_formats"}
    REQUIRED_CATEGORIES = {"required", "platform_dependencies", "optional_debug"}
    REQUIRED_ITEM_FIELDS = {"name", "type", "platforms"}
    CATEGORIES_ALLOW_DEFAULT_INCLUDE = {"optional_debug"}
    CATEGORIES_ALLOW_EXCLUDE_FROM_ALL = {"tools"}

    def validate_manifest_schema(self):
        """Validate the manifest JSON itself against the data contract.
        Raises ManifestSchemaError on any violation. This runs BEFORE any
        staging / collection happens — it validates the contract definition.
        """
        errors = []
        data = self.data

        missing_top = self.REQUIRED_TOP_FIELDS - set(data.keys())
        if missing_top:
            errors.append(f"Missing required top-level fields: {sorted(missing_top)}")

        if not isinstance(data.get("manifest_version"), str):
            errors.append("'manifest_version' must be a string")
        elif not re.match(r'^\d+\.\d+$', data["manifest_version"]):
            errors.append(f"'manifest_version' must be X.Y format, got {data['manifest_version']!r}")

        vs = data.get("version_source", {})
        if not isinstance(vs, dict):
            errors.append("'version_source' must be an object")
        else:
            missing_vs = {"type", "file", "macro"} - set(vs.keys())
            if missing_vs:
                errors.append(f"'version_source' missing fields: {sorted(missing_vs)}")
            else:
                header_path = os.path.join(self.project_root, vs["file"])
                if not os.path.isfile(header_path):
                    errors.append(f"version_source header not found: {header_path}")
                else:
                    try:
                        v = self.get_version()
                        if not re.match(r'^\d+\.\d+\.\d+$', v):
                            errors.append(f"version_source resolved to invalid version: {v!r}")
                    except ManifestError as e:
                        errors.append(f"version_source could not read version: {e}")

        dm = data.get("data_manifest", {})
        if not isinstance(dm, dict):
            errors.append("'data_manifest' must be an object")
        else:
            missing_dm = {"type", "file", "base_dir"} - set(dm.keys())
            if missing_dm:
                errors.append(f"'data_manifest' missing fields: {sorted(missing_dm)}")
            else:
                dm_path = os.path.join(self.project_root, dm["file"])
                if not os.path.isfile(dm_path):
                    errors.append(f"data_manifest file not found: {dm_path}")

        cats = data.get("categories", {})
        if not isinstance(cats, dict):
            errors.append("'categories' must be an object")
        else:
            missing_cats = self.REQUIRED_CATEGORIES - set(cats.keys())
            if missing_cats:
                errors.append(f"Missing required categories: {sorted(missing_cats)}")

            known_categories = {"required", "platform_dependencies", "optional_debug",
                                "tools", "macos_bundle", "source_package"}
            unknown_cats = set(cats.keys()) - known_categories
            if unknown_cats:
                errors.append(f"Unknown categories (not in data contract): {sorted(unknown_cats)}")

            for cat_name, cat_data in cats.items():
                if not isinstance(cat_data, dict):
                    errors.append(f"categories.{cat_name} must be an object")
                    continue
                if "items" not in cat_data:
                    errors.append(f"categories.{cat_name} missing 'items' list")
                    continue
                if not isinstance(cat_data["items"], list):
                    errors.append(f"categories.{cat_name}.items must be a list")
                    continue

                item_names = set()
                for idx, item in enumerate(cat_data["items"]):
                    prefix = f"categories.{cat_name}.items[{idx}]"
                    if not isinstance(item, dict):
                        errors.append(f"{prefix} must be an object")
                        continue
                    missing = self.REQUIRED_ITEM_FIELDS - set(item.keys())
                    if missing:
                        errors.append(f"{prefix} missing required fields: {sorted(missing)}")
                        continue

                    if item["name"] in item_names:
                        errors.append(f"{prefix} duplicate item name: {item['name']!r}")
                    item_names.add(item["name"])
                    if not isinstance(item["name"], str) or not re.match(r'^[a-z][a-z0-9_]*$', item["name"]):
                        errors.append(f"{prefix}.name must be snake_case identifier, got {item['name']!r}")

                    if item["type"] not in self.VALID_ITEM_TYPES:
                        errors.append(f"{prefix}.type={item['type']!r} not in {sorted(self.VALID_ITEM_TYPES)}")

                    valid_platforms = set(self.get_valid_platforms())
                    platforms = item.get("platforms", [])
                    if not isinstance(platforms, list) or not platforms:
                        errors.append(f"{prefix}.platforms must be a non-empty list")
                    else:
                        bad = set(platforms) - valid_platforms
                        if bad:
                            errors.append(f"{prefix}.platforms has invalid values {sorted(bad)} (valid: {sorted(valid_platforms)})")

                    source_build = bool(item.get("source_build"))
                    system_path = bool(item.get("system_path"))
                    has_path = "path" in item
                    has_paths = "paths" in item

                    if source_build and system_path:
                        errors.append(
                            f"{prefix}: source_build and system_path are mutually exclusive (both True)"
                        )

                    if not item.get("external") and not item.get("template"):
                        if not source_build and not system_path and not has_path and not has_paths:
                            errors.append(
                                f"{prefix} has no source defined. Must set exactly one of: "
                                "source_build, system_path, path/paths, or mark external=True"
                            )

                    if system_path and not has_path:
                        errors.append(f"{prefix}: system_path=True requires absolute 'path' to system library")

                    if item["type"] in ("directory_list", "file_list") and not has_paths:
                        errors.append(f"{prefix} is type={item['type']!r} but missing 'paths' list")
                    if item["type"] in ("binary", "file", "directory") and has_paths and not has_path:
                        errors.append(f"{prefix} is type={item['type']!r} but uses 'paths' (should use 'path')")

                    if "default_include" in item and cat_name not in self.CATEGORIES_ALLOW_DEFAULT_INCLUDE:
                        errors.append(
                            f"{prefix} sets 'default_include' but category '{cat_name}' does not allow it. "
                            f"Only {sorted(self.CATEGORIES_ALLOW_DEFAULT_INCLUDE)} may use default_include."
                        )
                    if "default_include" in item and not isinstance(item["default_include"], bool):
                        errors.append(f"{prefix}.default_include must be boolean")

                    if "exclude_from_all" in item and cat_name not in self.CATEGORIES_ALLOW_EXCLUDE_FROM_ALL:
                        errors.append(
                            f"{prefix} sets 'exclude_from_all' but category '{cat_name}' does not allow it. "
                            f"Only {sorted(self.CATEGORIES_ALLOW_EXCLUDE_FROM_ALL)} may use exclude_from_all."
                        )

                    if "dest_bundle" in item and "macos" not in platforms:
                        errors.append(f"{prefix}.dest_bundle set but 'macos' not in platforms")

                    if item.get("template") and not has_path:
                        errors.append(f"{prefix} is template=True but missing 'path' to .in template file")
                    if item.get("template") and cat_name != "macos_bundle":
                        errors.append(f"{prefix} is template=True but only macos_bundle category supports templates")

                    if "platform_names" in item:
                        pn = item["platform_names"]
                        if not isinstance(pn, dict):
                            errors.append(f"{prefix}.platform_names must be an object")
                        else:
                            bad_plat = set(pn.keys()) - set(platforms)
                            if bad_plat:
                                errors.append(f"{prefix}.platform_names keys {sorted(bad_plat)} not in item.platforms {sorted(platforms)}")

        pf = data.get("package_formats", {})
        if not isinstance(pf, dict):
            errors.append("'package_formats' must be an object")
        else:
            for plat_name, plat_fmt in pf.items():
                if not isinstance(plat_fmt, dict):
                    errors.append(f"package_formats.{plat_name} must be an object")
                    continue
                if "format" not in plat_fmt:
                    errors.append(f"package_formats.{plat_name} missing 'format' field")

        if errors:
            raise ManifestSchemaError(
                f"release_manifest.json failed schema validation — {len(errors)} error(s):\n  "
                + "\n  ".join(errors)
            )

    def validate_collected_contract(
        self,
        collected: Dict,
        platform: str,
        include_optional: bool = False,
        include_tools: bool = False
    ) -> List[str]:
        """After collect_files(), validate the contract per-category:
        - Every required category item resolved to a source
        - No extra / undeclared entries per category
        - Category counts match expected per the manifest for this platform
        Returns list of errors (empty if clean).
        """
        errors = []

        expected = self.get_items_for_platform(platform, include_optional, include_tools)

        for cat_name, expected_items in expected.items():
            actual_items = collected["items"].get(cat_name, [])

            expected_names = {
                it["name"] for it in expected_items
                if not it.get("external")
            }
            actual_names = set()
            has_expanded_data = False
            for it in actual_items:
                if it.get("expanded"):
                    if it["name"] == "data_directory":
                        has_expanded_data = True
                    continue
                actual_names.add(it["name"])

            if has_expanded_data and "data_directory" in expected_names:
                expected_names.discard("data_directory")

            missing = expected_names - actual_names
            extra = actual_names - expected_names

            if cat_name == "required" and missing:
                errors.append(f"REQUIRED category missing items: {sorted(missing)}")
            elif missing and cat_name != "optional_debug":
                errors.append(f"Category '{cat_name}' missing declared items: {sorted(missing)}")

            if extra:
                errors.append(f"Category '{cat_name}' has UNDECLARED items not in manifest: {sorted(extra)}")

        for cat_name in collected["items"]:
            if cat_name not in expected and cat_name != "data_files":
                errors.append(f"Collected category '{cat_name}' is NOT declared in manifest categories")

        if collected.get("missing_required"):
            errors.append(f"Missing required files on disk: {collected['missing_required']}")

        if collected.get("data_files"):
            expected_count = len(self.get_data_files())
            actual_count = len(collected["data_files"])
            if actual_count != expected_count:
                errors.append(
                    f"data_manifest declared {expected_count} files but only {actual_count} resolved"
                )

        return errors


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
    parser.add_argument("--collect", action="store_true", help="Collect and list all files for platform")
    parser.add_argument("--copy", metavar="PACKAGE_DIR", help="Copy collected files to PACKAGE_DIR")
    parser.add_argument("--validate", metavar="PACKAGE_DIR", help="Validate package dir against manifest")
    parser.add_argument("--languages-dir", help="Path to downloaded languages directory")
    parser.add_argument("--maps-dir", help="Path to downloaded maps directory")
    parser.add_argument("--no-strict", action="store_true", help="Do not fail on missing required items")

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

    if args.collect or args.copy:
        collected = manifest.collect_files(
            args.platform,
            build_dir=args.build_dir,
            include_optional=args.include_optional,
            include_tools=args.include_tools,
            verify_exists=True,
            strict=not args.no_strict
        )
        if args.collect:
            for category, items in collected["items"].items():
                print(f"\n[{category}]")
                for entry in items:
                    print(f"  {entry['name']}: {entry['source']} -> {entry['dest']}")
            if collected["data_files"]:
                print(f"\n[data_manifest.txt] {len(collected['data_files'])} files")
            if collected["missing_required"]:
                print(f"\n[MISSING REQUIRED] {collected['missing_required']}")
            if collected["missing_optional"]:
                print(f"\n[MISSING OPTIONAL] {collected['missing_optional']}")

        if args.copy:
            ext_dirs = {}
            if args.languages_dir:
                ext_dirs["languages"] = args.languages_dir
            if args.maps_dir:
                ext_dirs["maps"] = args.maps_dir
            use_bundle = manifest.get_package_format(args.platform).get("use_bundle", False)
            manifest.copy_files_to_package(collected, args.copy, args.platform, use_bundle, ext_dirs)
            print(f"Copied files to {args.copy}")

    if args.validate:
        collected = manifest.collect_files(
            args.platform,
            build_dir=args.build_dir,
            include_optional=args.include_optional,
            include_tools=args.include_tools,
            verify_exists=False,
            strict=False
        )
        errors = manifest.validate_staging_directory(args.validate, collected, args.platform)
        if errors:
            print("VALIDATION ERRORS:")
            for e in errors:
                print(f"  ERROR: {e}")
            import sys
            sys.exit(1)
        else:
            print("Validation OK - all files declared in manifest")


if __name__ == "__main__":
    main()
