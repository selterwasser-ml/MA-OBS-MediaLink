"""Package explicitly listed runtime files, MA imports and documentation.

Run after OBS/Build_OBS33_Release.ps1. Produces two ZIPs and SHA256SUMS.txt in
dist/. Use --finalize-installer after installer/Build_Installer.ps1 to include
the setup executable in SHA256SUMS.txt. OBS/Qt runtimes and settings are excluded.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import struct
import zipfile
from html.parser import HTMLParser
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
PLUGIN = "obs-sacn-crossfade"


def verify_local_guide(folder):
    class Links(HTMLParser):
        def handle_starttag(self, tag, attrs):
            for key, value in attrs:
                if key != 'href' or not value:
                    continue
                link = urlsplit(value)
                if link.scheme or link.netloc or not link.path:
                    continue
                target = (folder / unquote(link.path)).resolve()
                if not target.is_relative_to(folder.resolve()) or not target.is_file():
                    raise ValueError('Missing or unsafe local guide link: ' + value)
    Links().feed((folder / 'START_HIER.html').read_text(encoding='utf-8'))


def verify_x64_dll(path):
    data = path.read_bytes()
    if data[:2] != b"MZ" or len(data) < 64:
        raise ValueError(f"Not a Windows DLL: {path}")
    offset = struct.unpack_from("<I", data, 60)[0]
    if data[offset:offset + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    machine = struct.unpack_from("<H", data, offset + 4)[0]
    characteristics = struct.unpack_from("<H", data, offset + 22)[0]
    if machine != 0x8664 or not characteristics & 0x2000:
        raise ValueError("Expected an x64 DLL")


def write_zip(path, files, metadata):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        for source, destination in files:
            archive.write(source, destination)
        archive.writestr("BUILD-INFO.json", json.dumps(metadata, indent=2) + "\n")
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise ValueError(f"Invalid ZIP: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--finalize-installer', action='store_true',
                        help='Write checksums for existing ZIPs and setup')
    args = parser.parse_args()
    dist = ROOT / "dist"
    packages = [dist / "MA_OBS_MediaLink_v046_Windows_x64.zip",
                dist / "MA_OBS_MediaLink_MA043.zip"]
    if args.finalize_installer:
        packages.append(dist / "MA_OBS_MediaLink_v046_Setup_x64.exe")
        for path in packages:
            if not path.is_file() or path.stat().st_size == 0:
                raise FileNotFoundError(path)
        write_checksums(dist, packages)
        return
    spec_file = ROOT / "OBS/dependencies.json"
    spec = json.loads(spec_file.read_text(encoding="utf-8"))
    dll = ROOT / f"OBS/Release/{PLUGIN}/bin/64bit/{PLUGIN}.dll"
    verify_x64_dll(dll)
    verify_local_guide(ROOT / f'OBS/Release/{PLUGIN}/data/ma-paket')
    common = [(ROOT / name, name) for name in
              ("LICENSE", "CHANGELOG.md", "THIRD_PARTY.md", "OBS/dependencies.json")]
    windows = common + [
        (dll, f"{PLUGIN}/bin/64bit/{PLUGIN}.dll"),
        (ROOT / f"OBS/Release/{PLUGIN}/data/crossfade.effect", f"{PLUGIN}/data/crossfade.effect"),
        (ROOT / f"OBS/Release/{PLUGIN}/data/medialink.ico", f"{PLUGIN}/data/medialink.ico"),
        (ROOT / "docs/WINDOWS_ZIP.md", "README.md"),
        (ROOT / "OBS/README.md", "OBS/README.md"),
        (ROOT / "docs/BUILD.md", "docs/BUILD.md"),
        (ROOT / "docs/WINDOWS_ZIP.md", "docs/WINDOWS_ZIP.md"),
        (ROOT / "docs/WINDOWS_INSTALLER.md", "docs/WINDOWS_INSTALLER.md"),
    ]
    for relative in (
        'START_HIER.html', 'grandMA3/MA3/MA_OBS_MediaLink.xml',
        'grandMA3/MA3/MA_OBS_MediaLink.lua', 'grandMA3/README_START_HIER.md',
        'grandMA3/REFERENZEN.md', 'fixtures/MA_OBS_MediaLink_Fade.gdtf',
        'fixtures/MA_OBS_MediaLink_Steuerung.gdtf', 'fixtures/README.md',
        'OBS/README.md', 'docs/WINDOWS_INSTALLER.md', 'docs/WINDOWS_ZIP.md', 'docs/BUILD.md',
        'LICENSE', 'THIRD_PARTY.md'):
        windows.append((ROOT / f'OBS/Release/{PLUGIN}/data/ma-paket/{relative}',
                        f'{PLUGIN}/data/ma-paket/{relative}'))
    ma = common + [(ROOT / name, name) for name in (
        "grandMA3/MA3/MA_OBS_MediaLink.xml", "grandMA3/MA3/MA_OBS_MediaLink.lua",
        "grandMA3/README_START_HIER.md", "grandMA3/REFERENZEN.md",
        "fixtures/README.md", "fixtures/MA_OBS_MediaLink_Fade.gdtf",
        "fixtures/MA_OBS_MediaLink_Steuerung.gdtf")]
    for source, _ in windows + ma:
        if not source.is_file():
            raise FileNotFoundError(source)
    metadata = {
        "obsPluginVersion": "0.46.0", "maPluginVersion": "0.43",
        "obsSdkVersion": spec["obsVersion"], "qtVersion": spec["qtVersion"],
        "architecture": spec["architecture"],
        "revision": os.environ.get("MEDIALINK_REVISION", "local-unversioned"),
        "dependenciesManifestSha256": hashlib.sha256(spec_file.read_bytes()).hexdigest(),
        "dllSha256": hashlib.sha256(dll.read_bytes()).hexdigest(),
        "dependencies": spec["archives"],
    }
    dist.mkdir(exist_ok=True)
    (dist / "BUILD-INFO.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    for path, files in zip(packages, (windows, ma)):
        write_zip(path, files, metadata)
        print(f"Created {path.name}")
    write_checksums(dist, packages)


def write_checksums(dist, packages):
    sums = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in packages)
    (dist / "SHA256SUMS.txt").write_text(sums, encoding="utf-8")
    print(sums, end="")


if __name__ == "__main__":
    main()
