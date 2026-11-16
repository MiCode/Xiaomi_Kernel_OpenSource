# SPDX-License-Identifier: GPL-2.0-only
"""Tests for rothko_gpl_audit.py. Run: python -m unittest discover -s tools/gpl-audit/tests"""

import contextlib
import csv
import gzip
import io
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import rothko_gpl_audit as audit  # noqa: E402


def build_ko(fields: dict, elf_class: int = 2) -> bytes:
    modinfo = b"".join(
        key.encode("utf-8") + b"=" + value.encode("utf-8") + b"\0"
        for key, value in fields.items()
    )
    shstr = b"\0.shstrtab\0.modinfo\0"
    shstr_name = shstr.index(b".shstrtab")
    modinfo_name = shstr.index(b".modinfo")

    if elf_class == 2:
        header_size, shentsize = 64, 64
        ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + b"\0" * 8
    elif elf_class == 1:
        header_size, shentsize = 52, 40
        ident = b"\x7fELF" + bytes([1, 1, 1, 0]) + b"\0" * 8
    else:
        raise ValueError("unsupported class")

    off_modinfo = header_size
    off_shstr = off_modinfo + len(modinfo)
    alignment = 8 if elf_class == 2 else 4
    off_sections = (off_shstr + len(shstr) + alignment - 1) & ~(alignment - 1)

    if elf_class == 2:
        header = ident + struct.pack(
            "<HHIQQQIHHHHHH",
            1,
            183,
            1,
            0,
            0,
            off_sections,
            0,
            header_size,
            0,
            0,
            shentsize,
            3,
            1,
        )

        def section(name, kind, offset, size):
            return struct.pack(
                "<IIQQQQIIQQ", name, kind, 0, 0, offset, size, 0, 0, 1, 0
            )

    else:
        header = ident + struct.pack(
            "<HHIIIIIHHHHHH",
            1,
            183,
            1,
            0,
            0,
            off_sections,
            0,
            header_size,
            0,
            0,
            shentsize,
            3,
            1,
        )

        def section(name, kind, offset, size):
            return struct.pack(
                "<IIIIIIIIII", name, kind, 0, 0, offset, size, 0, 0, 1, 0
            )

    total = off_sections + shentsize * 3
    blob = bytearray(total)
    blob[0 : len(header)] = header
    blob[off_modinfo : off_modinfo + len(modinfo)] = modinfo
    blob[off_shstr : off_shstr + len(shstr)] = shstr
    sections = (
        b"\0" * shentsize
        + section(shstr_name, 3, off_shstr, len(shstr))
        + section(modinfo_name, 1, off_modinfo, len(modinfo))
    )
    blob[off_sections : off_sections + len(sections)] = sections
    return bytes(blob)


class WorkspaceTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def write_module(self, relative_path, fields, compressed=False):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        blob = build_ko(fields)
        if compressed:
            with gzip.open(path, "wb") as handle:
                handle.write(blob)
        else:
            path.write_bytes(blob)
        return path

    def run_cli(self, argv):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = audit.main(argv)
        return code, stdout.getvalue(), stderr.getvalue()


class ModinfoTests(WorkspaceTest):
    def test_reads_license_and_vermagic_elf64(self):
        data = build_ko({"license": "GPL v2", "vermagic": "6.1.138-test", "name": "demo"})
        fields = audit.parse_modinfo(data)
        self.assertEqual(fields["license"], "GPL v2")
        self.assertEqual(fields["vermagic"], "6.1.138-test")
        self.assertEqual(audit.classify_license(fields["license"]), "gpl")

    def test_reads_modinfo_elf32(self):
        data = build_ko({"license": "Dual BSD/GPL"}, elf_class=1)
        self.assertEqual(audit.parse_modinfo(data)["license"], "Dual BSD/GPL")

    def test_module_without_license_fields(self):
        self.assertEqual(audit.parse_modinfo(build_ko({})), {})
        self.assertEqual(audit.classify_license(None), "unknown")
        self.assertEqual(audit.classify_license("Proprietary"), "non-gpl")

    def test_rejects_non_elf(self):
        with self.assertRaises(ValueError):
            audit.parse_modinfo(b"this is not an elf file")


class ScanTests(WorkspaceTest):
    def test_scan_infers_partition_compression_and_license(self):
        self.write_module(
            "vendor_dlkm/lib/modules/gpl-mod.ko",
            {"license": "GPL", "vermagic": "6.1.138-android14-11-ga418eeb69cb6"},
        )
        self.write_module(
            "odm_dlkm/lib/modules/prop-mod.ko.gz",
            {"license": "Proprietary"},
            compressed=True,
        )
        (self.root / "odm_dlkm/lib/modules/notes.txt").write_text("ignore me")

        modules, errors = audit.scan_roots([str(self.root)])
        self.assertEqual(errors, [])
        by_name = {module["name"]: module for module in modules}
        self.assertEqual(set(by_name), {"gpl-mod", "prop-mod"})
        self.assertEqual(by_name["gpl-mod"]["partition"], "vendor_dlkm")
        self.assertEqual(by_name["gpl-mod"]["compression"], "none")
        self.assertEqual(by_name["prop-mod"]["partition"], "odm_dlkm")
        self.assertEqual(by_name["prop-mod"]["compression"], "gzip")
        self.assertEqual(by_name["prop-mod"]["license_class"], "non-gpl")
        self.assertEqual(len(by_name["gpl-mod"]["sha256"]), 64)
        self.assertEqual(by_name["gpl-mod"]["file"], "vendor_dlkm/lib/modules/gpl-mod.ko")

    def test_scan_records_unreadable_module(self):
        bad = self.root / "vendor_dlkm/lib/modules/broken.ko"
        bad.parent.mkdir(parents=True)
        bad.write_bytes(b"not an elf")
        modules, errors = audit.scan_roots([str(self.root)])
        self.assertEqual(modules, [])
        self.assertEqual(len(errors), 1)
        self.assertIn("broken.ko", errors[0]["file"])

    def test_normalizes_module_names(self):
        self.assertEqual(audit.normalize_name("mi_t1_gpio"), "mi-t1-gpio")
        self.assertEqual(audit.module_stem("mtk-vow.ko.xz"), "mtk-vow")
        self.assertEqual(audit.module_stem("a.ko.gz"), "a")


class InventoryTests(WorkspaceTest):
    def test_parse_makefile_handles_conditionals_continuations_and_comments(self):
        text = "\n".join(
            [
                "obj-m += alpha.o beta.o",
                "# obj-m += commented.o",
                "obj-$(CONFIG_FOO) += foo.o",
                "obj-m += multi.o \\",
                "         extra.o",
                "obj-m += dir/",
                "obj-y += builtin.o",
                'obj-m += "quoted.o"',
            ]
        )
        targets = audit.parse_makefile(text)
        self.assertIn(("alpha", None), targets)
        self.assertIn(("beta", None), targets)
        self.assertIn(("foo", "CONFIG_FOO"), targets)
        self.assertIn(("multi", None), targets)
        self.assertIn(("extra", None), targets)
        self.assertIn(("quoted", None), targets)
        names = [name for name, _ in targets]
        self.assertNotIn("commented", names)
        self.assertNotIn("builtin", names)

    def test_inventory_walks_tree_and_applies_excludes(self):
        (self.root / "drivers/foo").mkdir(parents=True)
        (self.root / "Makefile").write_text("obj-m += alpha.o\n")
        (self.root / "drivers/foo/Makefile").write_text("obj-$(CONFIG_FOO) += foo.o\n")
        (self.root / "tools").mkdir()
        (self.root / "tools/Makefile").write_text("obj-m += hidden.o\n")

        document = audit.build_published_inventory(str(self.root), branch="test-branch")
        names = {module["name"]: module for module in document["modules"]}
        self.assertEqual(set(names), {"alpha", "foo"})
        self.assertTrue(names["foo"]["conditional"])
        self.assertFalse(names["alpha"]["conditional"])
        self.assertEqual(names["foo"]["configs"], ["CONFIG_FOO"])
        self.assertEqual(names["alpha"]["sources"], ["Makefile"])
        self.assertNotIn("hidden", names)


class AuditCliTests(WorkspaceTest):
    def setUp(self):
        super().setUp()
        self.capture = self.root / "capture"
        self.write_module(
            "capture/vendor_dlkm/lib/modules/gpl-covered.ko",
            {"license": "GPL v2", "vermagic": "6.1.138-test"},
        )
        self.write_module(
            "capture/vendor_dlkm/lib/modules/gpl-missing.ko",
            {"license": "GPL", "vermagic": "6.1.138-test"},
        )
        self.write_module(
            "capture/odm_dlkm/lib/modules/prop-missing.ko",
            {"license": "Proprietary", "vermagic": "6.1.138-test"},
        )
        self.write_module(
            "capture/odm_dlkm/lib/modules/unknown-missing.ko",
            {"vermagic": "6.1.138-test"},
        )
        self.tree = self.root / "tree"
        (self.tree / "drivers").mkdir(parents=True)
        (self.tree / "Makefile").write_text("obj-m += gpl-covered.o\n")
        self.inventory_scans()

    def inventory_scans(self):
        code, _, _ = self.run_cli(
            ["scan", str(self.capture), "--out", str(self.root / "scan.json")]
        )
        self.assertEqual(code, audit.EXIT_OK)
        code, _, _ = self.run_cli(
            [
                "inventory",
                str(self.tree),
                "--branch",
                "bsp-test",
                "--out",
                str(self.root / "published.json"),
            ]
        )
        self.assertEqual(code, audit.EXIT_OK)

    def audit(self, outdir, *extra):
        return self.run_cli(
            [
                "audit",
                "--scan",
                str(self.root / "scan.json"),
                "--published",
                str(self.root / "published.json"),
                "--outdir",
                str(outdir),
                "--now",
                "2026-11-15T00:00:00Z",
                "--device",
                "Redmi K70 Ultra / Xiaomi 14T Pro (rothko)",
                *extra,
            ]
        )

    def test_audit_flags_uncovered_gpl_and_writes_reports(self):
        outdir = self.root / "reports"
        code, stdout, _ = self.audit(outdir)
        self.assertEqual(code, audit.EXIT_UNCOVERED)
        self.assertIn("1 uncovered", stdout)

        document = json.loads((outdir / "audit.json").read_text())
        self.assertEqual(document["schema"], audit.SCHEMA_AUDIT)
        self.assertEqual(document["summary"]["modules_scanned"], 4)
        self.assertEqual(document["summary"]["gpl_declared"], 2)
        self.assertEqual(document["summary"]["gpl_covered"], 1)
        self.assertEqual(document["summary"]["gpl_uncovered"], 1)
        self.assertEqual(document["uncovered_gpl"], ["gpl-missing"])
        self.assertEqual(document["license_breakdown"]["GPL v2"], 1)
        self.assertEqual(document["vermagic"]["dominant"], "6.1.138-test")
        self.assertEqual(document["metadata"]["device"], "Redmi K70 Ultra / Xiaomi 14T Pro (rothko)")

        report = (outdir / "report.md").read_text(encoding="utf-8")
        self.assertIn("GPL-declared modules without a published source target", report)
        self.assertIn("gpl-missing", report)
        self.assertIn("does not adjudicate legal obligations", report)

        with (outdir / "audit.csv").open(encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), 4)
        statuses = {row["module"]: row["coverage"] for row in rows}
        self.assertEqual(statuses["gpl-covered"], "covered")
        self.assertEqual(statuses["gpl-missing"], "missing")
        self.assertEqual(statuses["prop-missing"], "missing")

    def test_audit_returns_zero_when_all_gpl_modules_are_covered(self):
        (self.tree / "Makefile").write_text("obj-m += gpl-covered.o gpl-missing.o\n")
        self.inventory_scans()
        code, _, _ = self.audit(self.root / "clean")
        self.assertEqual(code, audit.EXIT_OK)

    def test_audit_blocks_on_incomplete_scan_unless_allowed(self):
        (self.root / "capture/vendor_dlkm/lib/modules/broken.ko").write_bytes(b"bad")
        self.inventory_scans()
        code, _, stderr = self.audit(self.root / "blocked")
        self.assertEqual(code, audit.EXIT_ERROR)
        self.assertIn("incomplete scan", stderr)

        (self.tree / "Makefile").write_text("obj-m += gpl-covered.o gpl-missing.o\n")
        self.inventory_scans()
        code, _, _ = self.audit(self.root / "allowed", "--allow-scan-errors")
        self.assertEqual(code, audit.EXIT_OK)

    def test_audit_output_is_deterministic(self):
        self.audit(self.root / "first")
        self.audit(self.root / "second")
        first = (self.root / "first/audit.json").read_bytes()
        second = (self.root / "second/audit.json").read_bytes()
        self.assertEqual(first, second)
        self.assertEqual(
            (self.root / "first/report.md").read_bytes(),
            (self.root / "second/report.md").read_bytes(),
        )

    def test_rejects_wrong_schema(self):
        bad = self.root / "bad.json"
        bad.write_text("{}")
        code, _, stderr = self.run_cli(
            [
                "audit",
                "--scan",
                str(bad),
                "--published",
                str(self.root / "published.json"),
                "--outdir",
                str(self.root / "nope"),
            ]
        )
        self.assertEqual(code, audit.EXIT_ERROR)
        self.assertIn("expected schema", stderr)


if __name__ == "__main__":
    unittest.main()
