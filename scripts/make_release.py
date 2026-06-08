import shutil, optparse, os, re, sys, zipfile, subprocess
os.chdir(os.path.dirname(os.path.realpath(sys.argv[0])) + "/..")
import twlib
from twlib import copy_tree
from release_manifest import ReleaseManifest, ManifestError

arguments = optparse.OptionParser(usage="usage: %prog VERSION PLATFORM [options]\n\nVERSION  - Version number\nPLATFORM - Target platform (f.e. linux_x86, linux_x86_64, macos, src, win32, win64)")
arguments.add_option("-l", "--url-languages", default = "http://github.com/teeworlds/teeworlds-translation/archive/master.zip", help = "URL from which the teeworlds language files will be downloaded")
arguments.add_option("-m", "--url-maps", default = "http://github.com/teeworlds/teeworlds-maps/archive/master.zip", help = "URL from which the teeworlds maps files will be downloaded")
arguments.add_option("-s", "--source-dir", help = "Source directory which is used for building the package")
arguments.add_option("--include-optional", action="store_true", default=False, help = "Include optional debug symbols and files (must be explicitly enabled)")
arguments.add_option("--include-tools", action="store_true", default=False, help = "Include development tools in the package (must be explicitly enabled)")
arguments.add_option("--allow-extra-files", action="store_true", default=False, help = "Allow undeclared files in the staging directory (default: strict - fail on undeclared)")
arguments.add_option("--no-strict", action="store_true", default=False, help = "Do not fail on missing required items (not recommended)")
(options, arguments) = arguments.parse_args()
if len(arguments) != 2:
	print("wrong number of arguments")
	print(sys.argv[0], "VERSION PLATFORM")
	sys.exit(-1)
if options.source_dir != None:
	if os.path.exists(options.source_dir) == False:
		print("Source directory " + options.source_dir + " doesn't exist")
		exit(1)
	os.chdir(options.source_dir)

manifest = ReleaseManifest()
valid_platforms = manifest.get_valid_platforms()

name = "teeworlds"
version = sys.argv[1]
platform = sys.argv[2]
exe_ext = ""
use_zip = 0
use_gz = 1
use_dmg = 0
use_bundle = 0
include_data = True
include_exe = True
include_src = False

if platform not in valid_platforms:
	print("not a valid platform")
	print(valid_platforms)
	sys.exit(-1)

pkg_format = manifest.get_package_format(platform)
if pkg_format.get("format") == "zip":
	use_zip = 1
	use_gz = 0
elif pkg_format.get("format") == "tar.gz":
	use_zip = 0
	use_gz = 1
elif pkg_format.get("format") == "dmg":
	use_dmg = 1
	use_gz = 0
	use_bundle = pkg_format.get("use_bundle", True)

if platform == "src":
	include_exe = False
	include_src = True
	use_zip = 1
elif platform == 'win32' or platform == 'win64':
	exe_ext = ".exe"

print("=" * 60)
print(f"Teeworlds {version} release build for platform: {platform}")
print(f"  Package format: {pkg_format.get('format', '?')}")
print(f"  Include optional debug: {options.include_optional}")
print(f"  Include tools: {options.include_tools}")
print(f"  Strict mode: {not options.no_strict}")
print(f"  Allow undeclared files: {options.allow_extra_files}")
print("=" * 60)

def unzip(filename, where):
	try:
		z = zipfile.ZipFile(filename, "r")
	except:
		return False
	for name in z.namelist():
		z.extract(name, where)
	z.close()
	return z.namelist()[0]

def copydir(src, dst, excl=[]):
	for root, dirs, files in os.walk(src, topdown=True):
		if "/." in root or "\\." in root:
			continue
		for name_dir in dirs:
			if name_dir[0] != '.':
				target = os.path.join(dst, root, name_dir)
				if not os.path.exists(target):
					os.makedirs(target, exist_ok=True)
		for name_file in files:
			if name_file[0] != '.':
				src_file = os.path.join(root, name_file)
				dst_file = os.path.join(dst, root, name_file)
				dst_dir = os.path.dirname(dst_file)
				if not os.path.exists(dst_dir):
					os.makedirs(dst_dir, exist_ok=True)
				shutil.copy(src_file, dst_file)

def clean():
	print("*** cleaning ***")
	try:
		shutil.rmtree(package_dir)
		shutil.rmtree(languages_dir)
		shutil.rmtree(maps_dir)
		os.remove(src_package_languages)
		os.remove(src_package_maps)
	except: pass

def shell(cmd):
	if os.system(cmd) != 0:
		clean()
		print("Non zero exit code on: os.system(%s)" % cmd)
		sys.exit(1)

package = "%s-%s-%s" %(name, version, platform)
package_dir = package

source_package_dir = manifest.get_build_output_dir(platform, cmake_build=False)

print("cleaning target")
shutil.rmtree(package_dir, True)
os.makedirs(package_dir, exist_ok=True)

print("download and extract languages")
src_package_languages = twlib.fetch_file(options.url_languages)
if not src_package_languages:
	print("couldn't download languages")
	sys.exit(-1)
languages_dir = unzip(src_package_languages, ".")
if not languages_dir:
	print("couldn't unzip languages")
	sys.exit(-1)

print("download and extract maps")
src_package_maps = twlib.fetch_file(options.url_maps)
if not src_package_maps:
	print("couldn't download maps")
	sys.exit(-1)
maps_dir = unzip(src_package_maps, ".")
if not maps_dir:
	print("couldn't unzip maps")
	sys.exit(-1)

external_dirs = {}
if os.path.isdir(languages_dir):
	external_dirs["languages"] = languages_dir
if os.path.isdir(maps_dir):
	external_dirs["maps"] = maps_dir

print("\n*** Collecting files from release manifest ***")
try:
	collected = manifest.collect_files(
		platform,
		build_dir=source_package_dir,
		include_optional=options.include_optional,
		include_tools=options.include_tools,
		verify_exists=True,
		strict=not options.no_strict
	)
except ManifestError as e:
	print(f"\nFATAL [ManifestError]: {e}")
	print("Release build aborted - required files are missing.")
	clean()
	sys.exit(1)

total_data = len(collected["data_files"])
total_items = sum(len(v) for v in collected["items"].values())
print(f"  Manifest items: {total_items}")
print(f"  Data files (from data_manifest.txt): {total_data}")
if collected["missing_optional"]:
	print(f"  Missing optional items: {len(collected['missing_optional'])}")

print("\n*** Staging files to package directory ***")
manifest.copy_files_to_package(
	collected,
	package_dir,
	platform,
	use_bundle=False,
	external_dirs=external_dirs
)

if include_src:
	print("  Adding source package files...")
	src_items = collected["items"].get("source_package", [])
	for entry in src_items:
		if entry["name"] == "source_root":
			for p in ["src", "scripts", "datasrc", "other", "objs"]:
				if os.path.exists(p):
					target = os.path.join(package_dir, p)
					if not os.path.exists(target):
						os.makedirs(target, exist_ok=True)
					copydir(p, package_dir)
		elif entry["name"] == "build_scripts":
			for p in ["CMakeLists.txt", "bam.lua", "configure.lua"]:
				if os.path.exists(p):
					shutil.copy(p, package_dir)
					rel = os.path.relpath(os.path.realpath(os.path.join(package_dir, p)), os.path.realpath(package_dir))
					manifest._staged_files.add(rel)

if use_bundle:
	print("\n*** Building macOS application bundles ***")
	bins = [name, name+'_srv', 'serverlaunch']
	platforms_list = ('x86_64',)
	for bin_name in bins:
		to_lipo = []
		for p in platforms_list:
			fname = bin_name+'_'+p
			if os.path.isfile(fname):
				to_lipo.append(fname)
		if to_lipo:
			shell("lipo -create -output "+bin_name+" "+" ".join(to_lipo))

	clientbundle_content_dir = os.path.join(package_dir, "Teeworlds.app/Contents")
	clientbundle_bin_dir = os.path.join(clientbundle_content_dir, "MacOS")
	clientbundle_resource_dir = os.path.join(clientbundle_content_dir, "Resources")
	clientbundle_framework_dir = os.path.join(clientbundle_content_dir, "Frameworks")
	binary_path = clientbundle_bin_dir + "/" + name+exe_ext
	freetypelib_path = clientbundle_framework_dir + "/libfreetype.6.dylib"
	os.makedirs(clientbundle_content_dir, exist_ok=True)
	os.makedirs(clientbundle_bin_dir, exist_ok=True)
	os.makedirs(clientbundle_resource_dir, exist_ok=True)
	os.makedirs(clientbundle_framework_dir, exist_ok=True)

	copy_tree(source_package_dir+"data", clientbundle_resource_dir+"/data")
	copy_tree(languages_dir, clientbundle_resource_dir+"/data/languages")
	copy_tree(maps_dir, clientbundle_resource_dir+"/data/maps")

	bundle_tracked_root = os.path.realpath(package_dir)
	def _track_bundle_copy(src, dst):
		shutil.copy2(src, dst)
		rel = os.path.relpath(os.path.realpath(dst), bundle_tracked_root)
		manifest._staged_files.add(rel)
	def _track_bundle_copytree(src, dst):
		if os.path.exists(dst):
			shutil.rmtree(dst)
		shutil.copytree(src, dst)
		for root, _dirs, files in os.walk(dst):
			for fname in files:
				full = os.path.join(root, fname)
				rel = os.path.relpath(os.path.realpath(full), bundle_tracked_root)
				manifest._staged_files.add(rel)

	_track_bundle_copy("other/icons/Teeworlds.icns", os.path.join(clientbundle_resource_dir, "Teeworlds.icns"))
	_track_bundle_copy(source_package_dir+name+exe_ext, binary_path)

	shell("install_name_tool -change /usr/local/opt/freetype/lib/libfreetype.6.dylib @executable_path/../Frameworks/libfreetype.6.dylib " + binary_path)
	shell("install_name_tool -change /usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib @executable_path/../Frameworks/libSDL2-2.0.0.dylib  " + binary_path)

	_track_bundle_copy("/usr/local/opt/freetype/lib/libfreetype.6.dylib", freetypelib_path)
	_track_bundle_copy("/usr/local/opt/libpng/lib/libpng16.16.dylib", clientbundle_framework_dir + "/libpng16.16.dylib")
	_track_bundle_copy("/usr/local/opt/sdl2/lib/libSDL2-2.0.0.dylib", clientbundle_framework_dir + "/libSDL2-2.0.0.dylib")

	shell("install_name_tool -change /usr/local/opt/libpng/lib/libpng16.16.dylib @executable_path/../Frameworks/libpng16.16.dylib " + freetypelib_path)

	info_plist_content = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>English</string>
	<key>CFBundleExecutable</key>
	<string>teeworlds</string>
	<key>CFBundleIconFile</key>
	<string>Teeworlds</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>CFBundleVersion</key>
	<string>%s</string>
	<key>CFBundleIdentifier</key>
	<string>com.TeeworldsClient.app</string>
	<key>NSHighResolutionCapable</key>
	<true/>
</dict>
</plist>
	""" % (version)
	info_plist_path = os.path.join(clientbundle_content_dir, "Info.plist")
	open(info_plist_path, "w").write(info_plist_content)
	manifest._staged_files.add(os.path.relpath(os.path.realpath(info_plist_path), bundle_tracked_root))

	pkginfo_path = os.path.join(clientbundle_content_dir, "PkgInfo")
	open(pkginfo_path, "w").write("APPL????")
	manifest._staged_files.add(os.path.relpath(os.path.realpath(pkginfo_path), bundle_tracked_root))

	serverbundle_content_dir = os.path.join(package_dir, "Teeworlds Server.app/Contents")
	serverbundle_bin_dir = os.path.join(serverbundle_content_dir, "MacOS")
	serverbundle_resource_dir = os.path.join(serverbundle_content_dir, "Resources")
	os.makedirs(serverbundle_content_dir, exist_ok=True)
	os.makedirs(serverbundle_bin_dir, exist_ok=True)
	os.makedirs(serverbundle_resource_dir, exist_ok=True)
	os.makedirs(os.path.join(serverbundle_resource_dir, "data"), exist_ok=True)
	os.makedirs(os.path.join(serverbundle_resource_dir, "data/maps"), exist_ok=True)
	os.makedirs(os.path.join(serverbundle_resource_dir, "data/mapres"), exist_ok=True)

	_track_bundle_copytree(maps_dir, serverbundle_resource_dir+"/data/maps")
	_track_bundle_copy("other/icons/Teeworlds_srv.icns", os.path.join(serverbundle_resource_dir, "Teeworlds_srv.icns"))
	_track_bundle_copy(source_package_dir+name+"_srv"+exe_ext, os.path.join(serverbundle_bin_dir, name+"_srv"+exe_ext))
	_track_bundle_copy(source_package_dir+"serverlaunch"+exe_ext, serverbundle_bin_dir + "/"+name+"_server")

	server_info_plist = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>English</string>
	<key>CFBundleExecutable</key>
	<string>teeworlds_server</string>
	<key>CFBundleIconFile</key>
	<string>Teeworlds_srv</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>CFBundleVersion</key>
	<string>%s</string>
</dict>
</plist>
	""" % (version)
	srv_info_plist_path = os.path.join(serverbundle_content_dir, "Info.plist")
	open(srv_info_plist_path, "w").write(server_info_plist)
	manifest._staged_files.add(os.path.relpath(os.path.realpath(srv_info_plist_path), bundle_tracked_root))

	srv_pkginfo_path = os.path.join(serverbundle_content_dir, "PkgInfo")
	open(srv_pkginfo_path, "w").write("APPL????")
	manifest._staged_files.add(os.path.relpath(os.path.realpath(srv_pkginfo_path), bundle_tracked_root))

print("\n*** Validating staging directory against manifest ***")
validation_errors = manifest.validate_staging_directory(
	package_dir,
	collected,
	platform,
	allow_extra=options.allow_extra_files
)
if validation_errors:
	print("VALIDATION FAILED - undeclared files found in package directory:")
	for err in validation_errors:
		print(f"  {err}")
	print("\nTo allow undeclared files, use --allow-extra-files")
	clean()
	sys.exit(1)

staged_count = len(manifest._staged_files)
print(f"  Validation OK - {staged_count} files staged, all declared in manifest")

if use_zip:
	print("\n*** Making zip archive ***")
	zf = zipfile.ZipFile("%s.zip" % package, 'w', zipfile.ZIP_DEFLATED)

	for root, dirs, files in os.walk(package_dir, topdown=True):
		for fname in files:
			n = os.path.join(root, fname)
			zf.write(n, n)
	zf.close()
	final_archive = f"{package}.zip"

if use_gz:
	print("\n*** Making tar.gz archive ***")
	shell("tar czf %s.tar.gz %s" % (package, package_dir))
	final_archive = f"{package}.tar.gz"

if use_dmg:
	print("\n*** Making disk image ***")
	shell("rm -f %s.dmg %s_temp.dmg" % (package, package))
	shell("hdiutil create -srcfolder %s -volname Teeworlds -quiet %s_temp" % (package_dir, package))
	shell("hdiutil convert %s_temp.dmg -format UDBZ -o %s.dmg -quiet" % (package, package))
	shell("rm -f %s_temp.dmg" % package)
	final_archive = f"{package}.dmg"

clean()

print("\n" + "=" * 60)
print("Release build complete!")
print(f"  Package: {final_archive}")
print(f"  Platform: {platform}")
print(f"  Files staged: {staged_count}")
if options.include_optional:
	print(f"  Included optional debug: yes")
if options.include_tools:
	print(f"  Included tools: yes")
print("=" * 60)
