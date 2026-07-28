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

from character_pipeline.decimate import decimate_character_mesh  # noqa: E402
from character_pipeline.glb import (  # noqa: E402
    MAX_TOTAL_VERTICES,
    _append_unsigned_short_indices,
    append_float_accessor,
    load_glb,
    validate_raylib_mesh_primitives,
    write_glb,
)
from character_pipeline.rig import (  # noqa: E402
    CANONICAL_CLIPS,
    assert_compatible_rigs,
    extract_rig,
    is_reference_clip_name,
    node_trs,
    validate_character_model,
)
from import_character import source_animation_reference_pose  # noqa: E402
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


class ReferenceClipDetectionTests(unittest.TestCase):
    def test_raw_meshy_reference_names_are_recognized(self):
        for name in ("Armature|clip0|baselayer", "tpose", "Character_output", "clip0"):
            self.assertTrue(is_reference_clip_name(name), name)

    def test_motion_clips_are_not_reference_clips(self):
        for name in ("Armature|running|baselayer", "Idle_11", "walking_man",
                     "Combat_Stance", "run_forward"):
            self.assertFalse(is_reference_clip_name(name), name)


class MeshBudgetTests(unittest.TestCase):
    def test_total_vertex_budget_rejects_dense_meshes(self):
        document = {
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}}]}],
            "accessors": [{"count": MAX_TOTAL_VERTICES + 1}],
        }
        with self.assertRaisesRegex(ValueError, "budget"):
            validate_raylib_mesh_primitives(document, b"")


class DecimationTests(unittest.TestCase):
    def _dense_grid(self, side):
        document = {
            "asset": {"version": "2.0"},
            "meshes": [{"primitives": [{"attributes": {}}]}],
            "buffers": [{"byteLength": 0}],
        }
        binary = bytearray()
        vertex_count = side * side
        positions = [
            (x * 0.1, 0.0, y * 0.1) for y in range(side) for x in range(side)
        ]
        normals = [(0.0, 1.0, 0.0)] * vertex_count
        uvs = [(x / side, y / side) for y in range(side) for x in range(side)]
        weights = [(1.0, 0.0, 0.0, 0.0)] * vertex_count
        primitive = document["meshes"][0]["primitives"][0]
        primitive["attributes"]["POSITION"] = append_float_accessor(
            document, binary, positions, "VEC3", include_bounds=True
        )
        primitive["attributes"]["NORMAL"] = append_float_accessor(
            document, binary, normals, "VEC3"
        )
        primitive["attributes"]["TEXCOORD_0"] = append_float_accessor(
            document, binary, uvs, "VEC2"
        )
        primitive["attributes"]["WEIGHTS_0"] = append_float_accessor(
            document, binary, weights, "VEC4"
        )

        while len(binary) % 4:
            binary.append(0)
        joints_start = len(binary)
        binary += bytes(4 * vertex_count)
        document.setdefault("bufferViews", []).append(
            {"buffer": 0, "byteOffset": joints_start, "byteLength": 4 * vertex_count}
        )
        document.setdefault("accessors", []).append(
            {
                "bufferView": len(document["bufferViews"]) - 1,
                "componentType": 5121,
                "count": vertex_count,
                "type": "VEC4",
            }
        )
        primitive["attributes"]["JOINTS_0"] = len(document["accessors"]) - 1

        indices = []
        for y in range(side - 1):
            for x in range(side - 1):
                a = y * side + x
                indices += [a, a + 1, a + side, a + 1, a + side + 1, a + side]
        primitive["indices"] = _append_unsigned_short_indices(
            document, binary, indices
        )
        document["buffers"] = [{"byteLength": len(binary)}]
        return document, bytes(binary)

    def test_decimation_hits_budget_and_preserves_attribute_layout(self):
        document, binary = self._dense_grid(60)
        result, result_binary, before, after = decimate_character_mesh(
            document, binary, target_triangles=500
        )
        self.assertEqual(before, 2 * 59 * 59)
        self.assertLessEqual(after, 520)
        primitive = result["meshes"][0]["primitives"][0]
        vertex_count = result["accessors"][primitive["attributes"]["POSITION"]]["count"]
        for semantic in ("NORMAL", "TEXCOORD_0", "WEIGHTS_0", "JOINTS_0"):
            self.assertEqual(
                result["accessors"][primitive["attributes"][semantic]]["count"],
                vertex_count,
                semantic,
            )
        self.assertEqual(
            result["accessors"][primitive["attributes"]["JOINTS_0"]]["componentType"],
            5121,
        )
        validate_raylib_mesh_primitives(result, result_binary)

    def test_meshes_inside_the_budget_are_untouched(self):
        document, binary = self._dense_grid(10)
        result, result_binary, before, after = decimate_character_mesh(
            document, binary, target_triangles=8000
        )
        self.assertIs(result, document)
        self.assertEqual(before, after)


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
                # Baking dedupes byte-identical embedded textures, so the output
                # may carry fewer images than a source model that embedded the
                # same PNG twice; the sizes themselves must survive unchanged.
                self.assertEqual(set(dimensions), set(output_dimensions))
                self.assertEqual(clips, list(CANONICAL_CLIPS))

    def test_topology_mismatch_is_rejected(self):
        changed = copy.deepcopy(self.base_document)
        joint_node = changed["skins"][0]["joints"][0]
        changed["nodes"][joint_node]["name"] += "_wrong"
        changed_rig = extract_rig(changed)
        with self.assertRaisesRegex(ValueError, "rig topology mismatch"):
            assert_compatible_rigs(self.base_rig, changed_rig)

    def test_merged_source_without_tpose_uses_original_joint_statics(self):
        merged = copy.deepcopy(self.base_document)
        merged.pop("extras", None)
        for index, animation in enumerate(merged["animations"]):
            animation["name"] = f"merged_motion_{index}"

        pose = source_animation_reference_pose(merged, self.base_binary)
        for name in self.base_rig.joint_names:
            expected = node_trs(
                merged["nodes"][self.base_rig.node_by_name[name]]
            )
            self.assertEqual(pose[name], expected)

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
