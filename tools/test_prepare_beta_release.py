"""Checks the release preparation boundaries without contacting GitHub."""
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import prepare_beta_release as release


def make_zip(files):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, content in files.items():
            archive.writestr(name, content)
    return stream.getvalue()


class ReleasePreparationTests(unittest.TestCase):
    def setUp(self):
        self.spec = {'sourceCommit': 'a' * 40, 'tag': 'v0.45.0-beta.1', 'title': 'Beta'}
        dll = b'test DLL payload'
        info = {'revision': self.spec['sourceCommit'], 'obsPluginVersion': '0.45.0',
                'maPluginVersion': '0.42', 'dllSha256': hashlib.sha256(dll).hexdigest()}
        self.packages = {
            release.BINARY_NAMES[0]: b'MZ test setup payload',
            release.BINARY_NAMES[1]: make_zip({
                'BUILD-INFO.json': json.dumps(info),
                'obs-sacn-crossfade/bin/64bit/obs-sacn-crossfade.dll': dll}),
            release.BINARY_NAMES[2]: make_zip({'BUILD-INFO.json': json.dumps(info)}),
        }
        self.spec['setupSha256'] = release.sha256(self.packages[release.BINARY_NAMES[0]])

    def artifact(self, packages=None, checksums=None, extra=None):
        packages = packages or self.packages
        checksums = checksums if checksums is not None else ''.join(
            f'{release.sha256(data)}  {name}\n' for name, data in packages.items())
        data = make_zip({**packages, 'SHA256SUMS.txt': checksums, **(extra or {})})
        return data, {**self.spec, 'artifactSha256': release.sha256(data)}

    def unpack(self, data, spec):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            release.unpack_verified_artifact(data, path, spec)
            return {p.name: p.read_bytes() for p in path.iterdir()}

    def test_valid_packages_are_not_modified(self):
        data, spec = self.artifact()
        self.assertEqual(self.unpack(data, spec), self.packages)

    def test_wrong_artifact_hash_is_rejected(self):
        data, spec = self.artifact()
        spec['artifactSha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'artifact SHA256'):
            self.unpack(data, spec)

    def test_changed_package_is_rejected_even_in_readable_zip(self):
        checksums = ''.join(f'{release.sha256(data)}  {name}\n' for name, data in self.packages.items())
        data, spec = self.artifact({**self.packages, release.BINARY_NAMES[0]: b'changed'}, checksums)
        with self.assertRaisesRegex(ValueError, 'Package checksum'):
            self.unpack(data, spec)

    def test_unexpected_files_and_paths_are_rejected(self):
        data, spec = self.artifact(extra={'../unexpected.exe': b'payload'})
        with self.assertRaisesRegex(ValueError, 'Unexpected files'):
            self.unpack(data, spec)

    def test_different_source_revision_is_rejected(self):
        data, spec = self.artifact()
        spec['sourceCommit'] = 'b' * 40
        with self.assertRaisesRegex(ValueError, 'source or version'):
            self.unpack(data, spec)

    def test_other_setup_with_valid_archive_is_rejected(self):
        data, spec = self.artifact()
        spec['setupSha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'tested setup'):
            self.unpack(data, spec)

    def test_published_release_cannot_be_reused(self):
        with self.assertRaisesRegex(ValueError, 'already published'):
            release.find_draft([{'tag_name': self.spec['tag'], 'draft': False}], self.spec)

    def test_foreign_draft_source_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'different source'):
            release.find_draft([{'tag_name': self.spec['tag'], 'draft': True,
                                 'target_commitish': 'b' * 40}], self.spec)

    def test_published_state_and_changed_notes_are_rejected(self):
        draft = {'draft': True, 'prerelease': True, 'tag_name': self.spec['tag'],
                 'target_commitish': self.spec['sourceCommit'], 'name': 'Beta', 'body': 'Notes'}
        release.require_draft(draft, self.spec, 'Notes')
        for changes in ({'draft': False}, {'prerelease': False}, {'body': 'Manually edited'}):
            with self.assertRaisesRegex(ValueError, 'refusing to overwrite'):
                release.require_draft({**draft, **changes}, self.spec, 'Notes')

    def test_authorized_text_correction_changes_only_body(self):
        old = {'id': 123, 'draft': True, 'prerelease': True, 'tag_name': self.spec['tag'],
               'target_commitish': self.spec['sourceCommit'], 'name': 'Beta', 'body': 'Old notes',
               'assets': [{'id': 7, 'name': 'setup.exe'}]}
        spec = {**self.spec, 'previousNotesSha256': release.sha256(b'Old notes')}
        updated = {**old, 'body': 'New notes'}
        with patch.object(release, 'api_json', return_value=updated) as api:
            self.assertEqual(release.refresh_draft_notes(old, spec, 'New notes', 'repos/owner/repo'), updated)
            api.assert_called_once_with('repos/owner/repo/releases/123', method='PATCH',
                                        data={'body': 'New notes'})

    def test_manual_edits_after_text_correction_was_selected_are_preserved(self):
        draft = {'id': 123, 'draft': True, 'prerelease': True, 'tag_name': self.spec['tag'],
                 'target_commitish': self.spec['sourceCommit'], 'name': 'Beta', 'body': 'Manual edit'}
        spec = {**self.spec, 'previousNotesSha256': release.sha256(b'Old notes')}
        with patch.object(release, 'api_json') as api:
            with self.assertRaisesRegex(ValueError, 'text was edited'):
                release.refresh_draft_notes(draft, spec, 'New notes', 'repos/owner/repo')
            api.assert_not_called()


if __name__ == '__main__':
    unittest.main()
