import optparse
import os
import sys

os.chdir(os.path.dirname(os.path.realpath(sys.argv[0])) + "/..")

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, THIS_DIR)

arguments = optparse.OptionParser(
    usage="usage: %prog VERSION PLATFORM [options]\n\n"
          "This script is a COMPATIBILITY WRAPPER that delegates entirely to package_manifest.py.\n"
          "ALL staging, validation and packaging now go through the unified release manifest.\n"
          "No legacy code paths remain — every file is audited against the manifest.\n\n"
          "VERSION  - Version number (ignored, read from src/game/version.h)\n"
          "PLATFORM - Target platform (linux_x86, linux_x86_64, macos, src, win32, win64)"
)
arguments.add_option("-l", "--url-languages", default="http://github.com/teeworlds/teeworlds-translation/archive/master.zip",
                      help="URL from which the teeworlds language files will be downloaded")
arguments.add_option("-m", "--url-maps", default="http://github.com/teeworlds/teeworlds-maps/archive/master.zip",
                      help="URL from which the teeworlds maps files will be downloaded")
arguments.add_option("-s", "--source-dir",
                      help="Source directory which is used for building the package")
arguments.add_option("-b", "--build-dir",
                      help="CMake build output directory (where teeworlds/teeworlds_srv binaries are)")
arguments.add_option("--include-optional", action="store_true", default=False,
                      help="Include optional debug symbols and files (MUST be explicitly enabled)")
arguments.add_option("--include-tools", action="store_true", default=False,
                      help="Include development tools in the package (MUST be explicitly enabled)")
arguments.add_option("--allow-extra-files", action="store_true", default=False,
                      help="Allow undeclared files in the staging directory (NOT recommended)")
arguments.add_option("--no-strict", action="store_true", default=False,
                      help="Do not fail on missing required items (NOT recommended)")
(options, args) = arguments.parse_args()

if len(args) != 2:
    print("wrong number of arguments")
    print(sys.argv[0], "VERSION PLATFORM")
    sys.exit(-1)

if options.source_dir is not None:
    if not os.path.exists(options.source_dir):
        print("Source directory " + options.source_dir + " doesn't exist")
        sys.exit(1)
    os.chdir(options.source_dir)

version_arg = args[0]
platform = args[1]

from release_manifest import ReleaseManifest
manifest = ReleaseManifest()
valid_platforms = manifest.get_valid_platforms()

if platform not in valid_platforms:
    print("not a valid platform")
    print(valid_platforms)
    sys.exit(-1)

pkg_format = manifest.get_package_format(platform)
fmt = pkg_format.get("format", "tar.gz")

print("=" * 60)
print(f"Teeworlds release build [{platform}]")
print(f"  Packager: package_manifest.py (unified release manifest)")
print(f"  Archive format: {fmt}")
print(f"  Include optional debug: {options.include_optional}")
print(f"  Include tools: {options.include_tools}")
print(f"  Strict: {not options.no_strict}")
print(f"  Allow undeclared files: {options.allow_extra_files}")
print("=" * 60)

from package_manifest import do_stage, do_package
import tempfile
import shutil

stage_dir = tempfile.mkdtemp(prefix=f"tw_stage_{platform}_")
package_name = f"teeworlds-{manifest.get_version()}-{platform}"

class _StageArgs:
    def __init__(self):
        self.platform = platform
        self.output = stage_dir
        self.build_dir = options.build_dir if options.build_dir else manifest.get_build_output_dir(platform)
        self.include_optional = options.include_optional
        self.include_tools = options.include_tools
        self.strict = not options.no_strict
        self.allow_extra_files = options.allow_extra_files
        self.use_bundle = pkg_format.get("use_bundle", False)
        self.url_languages = options.url_languages
        self.url_maps = options.url_maps
        self.download_external = True
        self.manifest = None
        self.version = None

class _PackageArgs:
    def __init__(self):
        self.stage_dir = stage_dir
        self.platform = platform
        self.format = fmt
        self.output_dir = os.getcwd()
        self.manifest = None
        self.version = None

try:
    do_stage(_StageArgs())
    do_package(_PackageArgs())
except SystemExit as e:
    try:
        shutil.rmtree(stage_dir, ignore_errors=True)
    except Exception:
        pass
    sys.exit(e.code)

shutil.rmtree(stage_dir, ignore_errors=True)

print("\n" + "=" * 60)
print("Release build complete (via unified release manifest)")
print(f"  Package: {os.path.join(os.getcwd(), package_name)}.{fmt}")
print(f"  Platform: {platform}")
if options.include_optional:
    print("  Included optional debug: YES (explicit)")
if options.include_tools:
    print("  Included tools: YES (explicit)")
print("=" * 60)
