#!/usr/bin/env python3
"""Unit and integration coverage for the reusable Meshy character pipeline."""

import copy
import json
import math
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

from character_pipeline.glb import load_glb, write_glb  # noqa: E402
from character_pipeline.rig import (  # noqa: E402
    CANONICAL_CLIPS,
    assert_compatible_rigs,
    extract_rig,
    validate_character_model,
)
from character_pipeline.retarget import (  # noqa: E402
    bake_character,
    quat_multiply,
    quat_normalize,
    retarget_rotation,
    retarget_translation,
    validate_animation_library,
    validate_generated_character,
)


def assert_tuple_close(test, actual, expected, places=5):
    test.assertEqual(len(actual), len(expected))
    for left, right in zip(actual, expected):
        test.assertAlmostEqual(left, right, places=places)


class RetargetMathTests(unittest.TestCase):
    def test_rotation_delta_is_applied_relative_to_target_rest(self):
        identity = (0.0, 0.0, 0.0, 1.0)
        half = math.sin(math.pi / 8.0)
        source_motion = (half, 0.0, 0.0, math.cos(math.pi / 8.0))
        target_rest = (0.0, math.sin(math.pi / 4.0), 0.0, math.cos(math.pi / 4.0))
        expected = quat_normalize(quat_multiply(target_rest, source_motion))
        actual = retarget_rotation(identity, target_rest, source_motion)
        assert_tuple_close(self, actual, expected)

    def test_translation_preserves_target_length_and_maps_parent_axes(self):
        identity = (0.0, 0.0, 0.0, 1.0)
        turn_180_y = (0.0, 1.0, 0.0, 0.0)
        actual = retarget_translation(
            (0.0, 10.0, 0.0),
            (0.0, 20.0, 0.0),
            (1.0, 11.0, 0.0),
            identity,
            turn_180_y,
            2.0,
        )
        assert_tuple_close(self, actual, (-2.0, 22.0, 0.0))


class CharacterAssetIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads(
            (PROJECT_ROOT / "data" / "characters" / "asset_manifest.json").read_text()
        )
        cls.base_path = PROJECT_ROOT / cls.manifest["animation_library"]
        cls.base_document, cls.base_binary = load_glb(cls.base_path)
        cls.base_rig, cls.base_clips = validate_animation_library(
            cls.base_document, cls.base_binary, CANONICAL_CLIPS
        )

    def test_every_model_and_generated_output_satisfies_contract(self):
        for entry in self.manifest["characters"]:
            with self.subTest(character=entry["id"]):
                model_document, model_binary = load_glb(PROJECT_ROOT / entry["model"])
                model_rig, dimensions = validate_character_model(
                    model_document, model_binary, self.base_rig
                )
                output_document, output_binary = load_glb(PROJECT_ROOT / entry["output"])
                output_rig, output_dimensions, clips = validate_generated_character(
                    output_document, output_binary, entry["id"]
                )
                assert_compatible_rigs(model_rig, output_rig)
                self.assertEqual(dimensions, output_dimensions)
                self.assertEqual(clips, list(CANONICAL_CLIPS))

    def test_topology_mismatch_is_rejected(self):
        changed = copy.deepcopy(self.base_document)
        joint_node = changed["skins"][0]["joints"][0]
        changed["nodes"][joint_node]["name"] += "_wrong"
        changed_rig = extract_rig(changed)
        with self.assertRaisesRegex(ValueError, "rig topology mismatch"):
            assert_compatible_rigs(self.base_rig, changed_rig)

    def test_baking_is_deterministic(self):
        entry = next(
            item for item in self.manifest["characters"]
            if item["id"] == "ironclad_guardian"
        )
        model_document, model_binary = load_glb(PROJECT_ROOT / entry["model"])
        libraries = [
            (self.base_document, self.base_binary, self.manifest["animation_library"])
        ]
        first_document, first_binary, _ = bake_character(
            model_document, model_binary, libraries, entry["id"]
        )
        second_document, second_binary, _ = bake_character(
            model_document, model_binary, libraries, entry["id"]
        )
        with tempfile.TemporaryDirectory(prefix="brawl-character-test-") as directory:
            first = Path(directory) / "first.glb"
            second = Path(directory) / "second.glb"
            write_glb(first, first_document, first_binary)
            write_glb(second, second_document, second_binary)
            self.assertEqual(first.read_bytes(), second.read_bytes())


if __name__ == "__main__":
    unittest.main(verbosity=2)
