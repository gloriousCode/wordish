"""Ensure store captures use actual settings presets without losing test inputs."""
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[1]
CAPTURE = runpy.run_path(str(ROOT / 'scripts/capture_screenshots.py'))


class CapturePresetTests(unittest.TestCase):
    def test_modern_gallery_uses_curated_presets_and_retains_features(self):
        for platform in ('emery', 'gabbro'):
            shots = CAPTURE['capture_shots'](platform)
            self.assertEqual([shot[4] for shot in shots[:5]], ['lagoon', 'ember', 'sand', 'orchid', 'grove'])
            self.assertEqual(shots[3][1][95], 1)
            self.assertEqual(shots[4][1][17], 1)
            self.assertEqual(shots[4][1][18], 1)
            self.assertFalse(shots[4][3])
            self.assertEqual(shots[5][1][2], 0)  # Catalan stress case stays independent.
            self.assertEqual(shots[8][1][15], 0)
            self.assertEqual(len({shot[1][5] for shot in shots[:5]}), 5)

    def test_audit_covers_every_preset_and_legacy_gallery_stays_unchanged(self):
        audit = CAPTURE['capture_shots']('gabbro', True)
        self.assertEqual({shot[4] for shot in audit}, {'classic', 'paper', 'quiet', 'lagoon', 'ember', 'orchid', 'grove', 'sand'})
        for platform in ('aplite', 'basalt', 'chalk', 'diorite', 'flint'):
            shots = CAPTURE['capture_shots'](platform)
            self.assertTrue(all(shot[4] is None for shot in shots))
            if platform != 'flint':
                self.assertTrue(all(shot[3] for shot in shots))


if __name__ == '__main__':
    unittest.main()
