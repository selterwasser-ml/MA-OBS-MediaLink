"""Stage an explicitly pinned, tested build as a GitHub draft prerelease.

Uses the runner's ordinary GH_TOKEN through GitHub CLI. Never publishes a
release, changes visibility, deletes an asset, or overwrites a published release.
"""
from pathlib import Path
import hashlib
import io
import json
import os
import subprocess
import urllib.parse
import zipfile

ROOT = Path(__file__).resolve().parent.parent
BINARY_NAMES = (
    'MA_OBS_MediaLink_v045_Setup_x64.exe',
    'MA_OBS_MediaLink_v045_Windows_x64.zip',
    'MA_OBS_MediaLink_MA042.zip',
)
SOURCE_NAME = 'MA_OBS_MediaLink_v045_MA042_Source.zip'


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def gh_api(path, *, method='GET', data=None, input_file=None, content_type=None):
    command = ['gh', 'api', '--hostname', 'github.com', '--method', method,
               '-H', 'X-GitHub-Api-Version: 2022-11-28']
    if content_type:
        command += ['-H', 'Content-Type: ' + content_type]
    payload = None
    if data is not None:
        command += ['--input', '-']
        payload = json.dumps(data).encode('utf-8')
    elif input_file is not None:
        command += ['--input', str(input_file)]
    command.append(path)
    result = subprocess.run(command, input=payload, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise RuntimeError(result.stderr.decode('utf-8', errors='replace').strip())
    return result.stdout


def api_json(path, **kwargs):
    return json.loads(gh_api(path, **kwargs))


def unpack_verified_artifact(data, destination, spec):
    if sha256(data) != spec['artifactSha256']:
        raise ValueError('Build artifact SHA256 mismatch')
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        expected = set(BINARY_NAMES) | {'SHA256SUMS.txt'}
        if len(archive.namelist()) != len(expected) or set(archive.namelist()) != expected:
            raise ValueError('Unexpected files in build artifact')
        if archive.testzip() is not None:
            raise ValueError('Damaged build artifact')
        sums = {}
        for line in archive.read('SHA256SUMS.txt').decode('utf-8-sig').splitlines():
            digest, name = line.split('  ', 1)
            if name in sums:
                raise ValueError('Duplicate checksum entry')
            sums[name] = digest
        if set(sums) != set(BINARY_NAMES):
            raise ValueError('Unexpected checksum filenames')
        for name in BINARY_NAMES:
            content = archive.read(name)
            if sha256(content) != sums[name]:
                raise ValueError('Package checksum mismatch: ' + name)
            if name.endswith('.exe') and sha256(content) != spec['setupSha256']:
                raise ValueError('Setup differs from tested setup')
            if name.endswith('.zip'):
                with zipfile.ZipFile(io.BytesIO(content)) as inner:
                    if inner.testzip() is not None:
                        raise ValueError('Damaged inner ZIP: ' + name)
                    info = json.loads(inner.read('BUILD-INFO.json'))
                    if (info['revision'] != spec['sourceCommit'] or
                            info['obsPluginVersion'] != '0.45.0' or
                            info['maPluginVersion'] != '0.42'):
                        raise ValueError('Package source or version mismatch')
                    if name == BINARY_NAMES[1]:
                        dll = inner.read('obs-sacn-crossfade/bin/64bit/obs-sacn-crossfade.dll')
                        if sha256(dll) != info['dllSha256']:
                            raise ValueError('DLL does not match build metadata')
            destination.mkdir(parents=True, exist_ok=True)
            (destination / name).write_bytes(content)


def find_draft(releases, spec):
    matching = [r for r in releases if r['tag_name'] == spec['tag'] or
                (spec.get('releaseId') is not None and r.get('id') == spec['releaseId'])]
    if len(matching) > 1:
        raise ValueError('Multiple releases use the requested tag')
    if not matching:
        return None
    release = matching[0]
    if not release['draft']:
        raise ValueError('Release is already published; refusing all changes')
    if release['target_commitish'] != spec['sourceCommit']:
        raise ValueError('Existing draft points to a different source revision')
    return release


def require_draft(release, spec, notes):
    if (not release['draft'] or not release['prerelease'] or
            release['target_commitish'] != spec['sourceCommit'] or
            release['tag_name'] != spec['tag'] or
            release['name'] != spec['title'] or release['body'] != notes):
        raise ValueError('Draft metadata differs; refusing to overwrite it')


def refresh_draft_notes(release, spec, notes, prefix):
    # Only the explicitly selected previous body may be replaced. Preserve
    # intervening manual edits and never alter assets or publication state.
    require_draft({**release, 'body': notes, 'tag_name': spec['tag']}, spec, notes)
    if release['tag_name'] != spec['tag'] and not (
            release['id'] == spec.get('releaseId') and
            release['tag_name'] == spec.get('previousTagName')):
        raise ValueError('Draft tag was edited; refusing to overwrite it')
    if release['body'] == notes and release['tag_name'] == spec['tag']:
        return release
    expected = spec.get('previousNotesSha256')
    if release['body'] != notes and (
            not expected or sha256(release['body'].encode('utf-8')) != expected):
        raise ValueError('Draft text was edited; refusing to overwrite it')
    # GitHub may reset an uncreated draft tag when tag_name is omitted from
    # PATCH. Resend the verified identity and keep publication flags explicit.
    updated = api_json(prefix + f'/releases/{release["id"]}', method='PATCH',
                       data={'body': notes, 'tag_name': spec['tag'],
                             'target_commitish': spec['sourceCommit'],
                             'name': spec['title'], 'draft': True, 'prerelease': True})
    require_draft(updated, spec, notes)
    return updated


def main():
    spec = json.loads((ROOT / 'releases/beta.json').read_text(encoding='utf-8'))
    notes = (ROOT / 'releases/v0.45.0-beta.1.md').read_text(encoding='utf-8')
    repo = spec['repository']
    if os.environ.get('GITHUB_REPOSITORY') != repo or not os.environ.get('GH_TOKEN'):
        raise RuntimeError('Run only in the intended repository workflow')
    prefix = f'repos/{repo}'
    # Check publication state before any draft mutation.
    releases = api_json(prefix + '/releases?per_page=100')
    if len(releases) == 100:
        raise ValueError('Release list needs pagination before staging')
    release = find_draft(releases, spec)
    if release is not None:
        release = refresh_draft_notes(release, spec, notes, prefix)
    run = api_json(prefix + f'/actions/runs/{spec["buildRunId"]}')
    if (run['head_sha'] != spec['sourceCommit'] or run['conclusion'] != 'success' or
            run['status'] != 'completed' or run['repository']['full_name'] != repo):
        raise ValueError('Pinned build did not succeed for the selected source')
    artifact = api_json(prefix + f'/actions/artifacts/{spec["artifactId"]}')
    if (artifact['expired'] or artifact['workflow_run']['id'] != spec['buildRunId'] or
            artifact['workflow_run']['head_sha'] != spec['sourceCommit'] or
            artifact['digest'] != 'sha256:' + spec['artifactSha256']):
        raise ValueError('Pinned build artifact is unavailable or inconsistent')

    destination = ROOT / 'dist/beta-release'
    data = gh_api(prefix + f'/actions/artifacts/{spec["artifactId"]}/zip')
    unpack_verified_artifact(data, destination, spec)
    source = gh_api(prefix + '/zipball/' + spec['sourceCommit'])
    with zipfile.ZipFile(io.BytesIO(source)) as archive:
        if archive.testzip() is not None:
            raise ValueError('Damaged source archive')
        paths = {name.split('/', 1)[1] for name in archive.namelist() if '/' in name}
        required = {'LICENSE', 'THIRD_PARTY.md', 'OBS/src/plugin.cpp',
                    'installer/MediaLink.iss', 'grandMA3/MA3/MA_OBS_MediaLink.lua',
                    'fixtures/MA_OBS_MediaLink_Fade.gdtf',
                    'fixtures/MA_OBS_MediaLink_Steuerung.gdtf'}
        if not required.issubset(paths):
            raise ValueError('Incomplete source archive')
    (destination / SOURCE_NAME).write_bytes(source)
    assets = [destination / name for name in (*BINARY_NAMES, SOURCE_NAME)]
    (destination / 'SHA256SUMS.txt').write_text(''.join(
        f'{sha256(path.read_bytes())}  {path.name}\n' for path in assets), encoding='utf-8')
    assets.append(destination / 'SHA256SUMS.txt')
    (destination / 'RELEASE_NOTES.md').write_text(notes, encoding='utf-8')
    if release is None:
        release = api_json(prefix + '/releases', method='POST', data={
            'tag_name': spec['tag'], 'target_commitish': spec['sourceCommit'],
            'name': spec['title'], 'body': notes, 'draft': True, 'prerelease': True,
            'make_latest': 'false',
        })
    require_draft(release, spec, notes)
    release_id = release['id']
    existing = {asset['name']: asset for asset in release.get('assets', [])}
    expected_names = {p.name for p in assets}
    if set(existing) - expected_names:
        raise ValueError('Existing draft has additional assets; refusing to change it')
    for path in assets:
        expected_digest = 'sha256:' + sha256(path.read_bytes())
        if path.name in existing:
            if existing[path.name].get('digest') != expected_digest:
                raise ValueError('Existing asset differs; refusing to replace: ' + path.name)
            continue
        upload = f'https://uploads.github.com/repos/{repo}/releases/{release_id}/assets?name='
        upload += urllib.parse.quote(path.name, safe='')
        asset = api_json(upload, method='POST', input_file=path,
                         content_type='application/octet-stream')
        if asset.get('digest') != expected_digest or asset['state'] != 'uploaded':
            raise ValueError('Uploaded asset failed verification: ' + path.name)
    release = api_json(prefix + f'/releases/{release_id}')
    require_draft(release, spec, notes)
    if {a['name'] for a in release['assets']} != expected_names:
        raise ValueError('Draft asset list is incomplete')
    report = {
        'draft': True, 'prerelease': True, 'releaseId': release_id,
        'releaseUrl': release['html_url'], 'tag': spec['tag'],
        'sourceCommit': spec['sourceCommit'], 'buildRunId': spec['buildRunId'],
        'testedOBS': spec['testedOBS'], 'testedGrandMA3': spec['testedGrandMA3'],
        'assets': {p.name: sha256(p.read_bytes()) for p in assets},
    }
    (destination / 'RELEASE-INFO.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as summary:
            summary.write(f'## Beta-Entwurf vorbereitet\n\n[Entwurf öffnen]({release["html_url"]})\n\n')
            summary.write('Unveröffentlicht; alle fünf Release-Dateien geprüft.\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
