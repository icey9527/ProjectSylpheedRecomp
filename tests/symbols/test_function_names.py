"""Regression tests for safe name configuration and address-based generated lookup."""

import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import tomllib
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import restore_function_names as names
import symbol_index
import check_function_renames


def row(address="0x82170000", raw="GameUpdate", offset=0):
    return dict(address=address, map_name=raw, object="Game.obj", segment=1, offset=offset)


class NameConfigurationTests(unittest.TestCase):
    def test_exact_position_and_name_required(self):
        candidate = row()
        for public in ({(1, 4): {"GameUpdate"}}, {(1, 0): {"OtherName"}}):
            with self.assertRaises(ValueError):
                names.choose_symbol([candidate], public, {})
        selected = names.choose_symbol([candidate], {}, {(1, 0): {("GameUpdate", 0x1132)}})
        self.assertEqual(selected, candidate)

    def test_alias_selection_does_not_depend_on_map_order(self):
        candidates = [row(raw="SecondAlias"), row(raw="FirstAlias")]
        public = {(1, 0): {"FirstAlias", "SecondAlias"}}
        self.assertEqual(names.choose_symbol(candidates, public, {}),
                         names.choose_symbol(candidates[::-1], public, {}))
        self.assertEqual(len(candidates), 2)

    def test_identifiers_distinguish_overloads_and_avoid_reserved_names(self):
        a, _ = names.cpp_name(row(raw="__Namespace::Method"))
        b, _ = names.cpp_name(row(address="0x82170004", raw="Namespace::Method"))
        self.assertNotEqual(a, b)
        self.assertNotIn("__", a)
        self.assertFalse(a.startswith("_"))
        self.assertEqual(a, "Namespace_Method_82170000")

    def test_catch_keeps_object_context(self):
        candidate = row(raw="__catch$123")
        candidate["object"] = "BaseLib:Scene.obj"
        self.assertEqual(names.cpp_name(candidate)[0], "Scene_Catch_123_82170000")

    def test_unresolved_decoration_is_rejected(self):
        with patch.object(names, "demangle", side_effect=lambda value, flags: value):
            with self.assertRaises(ValueError):
                names.cpp_name(row(raw="?Unresolved@@YAXXZ"))

    def test_only_name_changes_preserve_boundaries_and_comments(self):
        text = '# alias evidence\n[functions]\n"0x82170000" = { end = 0x82170020, share_registers = true }\n'
        result = names.edit_config(text, {"0x82170000": "GameUpdate_82170000"})
        self.assertIn("# alias evidence", result)
        entry = tomllib.loads(result)["functions"]["0x82170000"]
        self.assertEqual(entry, dict(name="GameUpdate_82170000", end=0x82170020,
                                     share_registers=True))
        self.assertEqual(names.edit_config(result, {"0x82170000": entry["name"]}), result)

    def test_no_new_entries_or_custom_name_overwrite(self):
        text = '[functions]\n"0x82170000" = { name = "UserChosenName" }\n'
        with self.assertRaises(ValueError):
            names.edit_config(text, {"0x82170004": "DifferentEntry"})
        with self.assertRaises(ValueError):
            names.edit_config(text, {"0x82170000": "GeneratedName"})

    def test_bad_later_symbol_does_not_write_earlier_config(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "config").mkdir()
            (root / "assets").mkdir()
            data = b"fixture"
            (root / "assets/input").write_bytes(data)
            manifest = {"files": [dict(name="input", size=len(data),
                                       sha256=hashlib.sha256(data).hexdigest())]}
            (root / "config/game-inputs.json").write_text(json.dumps(manifest))
            originals = {}
            for i, filename in enumerate(names.CONFIG_FILES):
                path = root / "config" / filename
                originals[path] = f'[functions]\n"0x{0x82170000 + i * 4:08X}" = {{}}\n'
                path.write_text(originals[path])
            with patch.object(names, "REPO", root), patch.object(sys, "argv", ["names"]), \
                    patch.object(names, "read_map", return_value=(None, [row()])), \
                    patch.object(names, "read_pdb", return_value=({}, {(1, 0): {"GameUpdate"}}, {})), \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(names.main(), 1)
            self.assertTrue(all(path.read_text() == text for path, text in originals.items()))
            (root / "assets/input").write_bytes(b"mismatched")
            with self.assertRaises(ValueError):
                names.verify_inputs(root)


class GeneratedLookupTests(unittest.TestCase):
    def test_names_follow_registration_not_their_address_suffix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game_register.cpp").write_text(
                "registrar->SetFunction(0x82170000, Readable_82900000);\n"
                "registrar->SetFunction(0x82170004, sub_82170004);\n"
                "registrar->SetFunction(0x82170008, rexcrt_Native);\n")
            (root / "game_recomp.0.cpp").write_text(
                "DEFINE_REX_FUNC(Readable_82900000) {}\nDEFINE_REX_FUNC(sub_82170004) {}\n")
            functions = symbol_index.generated_functions(root)
            self.assertEqual(set(functions), {"0x82170000", "0x82170004"})
            self.assertEqual(functions["0x82170000"][0], "Readable_82900000")

    def test_conflicting_or_misaligned_registrations_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            register = root / "game_register.cpp"
            register.write_text("registrar->SetFunction(0x82170000, First);\n"
                                "registrar->SetFunction(0x82170000, Second);\n")
            with self.assertRaises(ValueError):
                symbol_index.generated_functions(root)
            register.write_text("registrar->SetFunction(0x82170004, sub_82170000);\n")
            (root / "game_recomp.0.cpp").write_text("DEFINE_REX_FUNC(sub_82170000) {}\n")
            with self.assertRaises(ValueError):
                symbol_index.generated_functions(root)

    def test_legacy_sub_lookup_without_registrar(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "game_recomp.0.cpp").write_text("DEFINE_REX_FUNC(sub_82170000) {}\n")
            self.assertIn("0x82170000", symbol_index.generated_functions(root))


class RenameVerificationTests(unittest.TestCase):
    def test_declaration_sorting_allowed_but_execution_change_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            before, after = Path(directory) / "before", Path(directory) / "after"
            before.mkdir()
            after.mkdir()
            for root, name in ((before, "sub_82170000"), (after, "GameUpdate_82170000")):
                (root / "game_register.cpp").write_text(
                    f"registrar->SetFunction(0x82170000, {name});\n")
                (root / "game_recomp.0.cpp").write_text(
                    f"DEFINE_REX_FUNC({name}) {{ REX_STORE_U32(4, 1); }}\n")
            (before / "game_funcs.h").write_text(
                "#pragma once\nDECLARE_REX_FUNC(Other);\nDECLARE_REX_FUNC(sub_82170000);\n")
            (after / "game_funcs.h").write_text(
                "#pragma once\nDECLARE_REX_FUNC(GameUpdate_82170000);\nDECLARE_REX_FUNC(Other);\n")
            self.assertEqual(check_function_renames.compare(before, after)["restored_names"], 1)
            (after / "game_recomp.0.cpp").write_text(
                "DEFINE_REX_FUNC(GameUpdate_82170000) { REX_STORE_U32(8, 1); }\n")
            with self.assertRaises(ValueError):
                check_function_renames.compare(before, after)
            (after / "game_register.cpp").write_text(
                "registrar->SetFunction(0x82170004, GameUpdate_82170000);\n")
            with self.assertRaises(ValueError):
                check_function_renames.compare(before, after)


if __name__ == "__main__":
    unittest.main()
