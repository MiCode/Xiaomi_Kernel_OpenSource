#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Reproducible GPLv2 module audit for distributed vendor kernel modules.

Scans .ko files, reads the license and vermagic each module declares in its
.modinfo section, inventories module targets found in published kernel source
trees, and produces a per-module coverage report.
"""

from __future__ import annotations

import argparse
import csv
import datetime
import gzip
import hashlib
import io
import json
import lzma
import os
import re
import shlex
import shutil
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

TOOL_VERSION = "1.0.0"
SCHEMA_SCAN = "rothko-gpl-audit/scan/v1"
SCHEMA_PUBLISHED = "rothko-gpl-audit/published/v1"
SCHEMA_AUDIT = "rothko-gpl-audit/audit/v1"

EXIT_OK = 0
EXIT_UNCOVERED = 2
EXIT_ERROR = 3

SCOPE_NOTE = (
    "This report covers only the availability of module source targets and the "
    "licenses the scanned modules declare in their own .modinfo. It does not "
    "establish kernel source, device tree or build script availability, and it "
    "does not adjudicate legal obligations."
)

DEFAULT_EXCLUDES = (".git", "tools", "samples", "Documentation", "rust")
MODULE_SUFFIXES = (".ko.xz", ".ko.gz", ".ko.zst", ".ko.zstd", ".ko")

_OBJ_RULE = re.compile(r"^obj-(m|\$\(([A-Za-z0-9_]+)\))\s*(?:\+=|:=|=)\s*(.*)$")


class AuditError(Exception):
    pass


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def compression_of(filename: str) -> str:
    lower = filename.lower()
    if lower.endswith(".ko.xz"):
        return "xz"
    if lower.endswith(".ko.gz"):
        return "gzip"
    if lower.endswith(".ko.zst") or lower.endswith(".ko.zstd"):
        return "zstd"
    return "none"


def module_stem(filename: str) -> str:
    lower = filename.lower()
    for suffix in MODULE_SUFFIXES:
        if lower.endswith(suffix):
            return filename[: -len(suffix)]
    return ""


def normalize_name(name: str) -> str:
    return name.lower().replace("_", "-")


def classify_license(license_text):
    if not license_text:
        return "unknown"
    return "gpl" if "gpl" in license_text.lower() else "non-gpl"


def read_module_bytes(path: Path) -> bytes:
    lower = path.name.lower()
    if lower.endswith(".xz"):
        with lzma.open(path, "rb") as handle:
            return handle.read()
    if lower.endswith(".gz"):
        with gzip.open(path, "rb") as handle:
            return handle.read()
    if lower.endswith(".zst") or lower.endswith(".zstd"):
        zstd = shutil.which("zstd")
        if not zstd:
            raise AuditError("zstd binary not found; install zstd or scan an extracted copy")
        completed = subprocess.run([zstd, "-dc", "--", str(path)], capture_output=True)
        if completed.returncode != 0:
            message = completed.stderr.decode("utf-8", "replace").strip()
            raise AuditError(f"zstd failed: {message}")
        return completed.stdout
    return path.read_bytes()


def _parse_elf_sections(data: bytes):
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError("not an ELF file")
    elf_class = data[4]
    elf_data = data[5]
    if elf_data not in (1, 2):
        raise ValueError(f"unsupported ELF byte order {elf_data}")
    endian = "<" if elf_data == 1 else ">"

    if elf_class == 2:
        if len(data) < 64:
            raise ValueError("truncated ELF header")
        shoff = struct.unpack_from(endian + "Q", data, 0x28)[0]
        shentsize, shnum, shstrndx = struct.unpack_from(endian + "HHH", data, 0x3A)
        shdr_format = endian + "IIQQQQIIQQ"
    elif elf_class == 1:
        shoff = struct.unpack_from(endian + "I", data, 0x20)[0]
        shentsize, shnum, shstrndx = struct.unpack_from(endian + "HHH", data, 0x2E)
        shdr_format = endian + "IIIIIIIIII"
    else:
        raise ValueError(f"unsupported ELF class {elf_class}")

    shdr_size = struct.calcsize(shdr_format)
    if shentsize < shdr_size:
        raise ValueError("invalid section header entry size")
    if shoff == 0 or shnum == 0:
        raise ValueError("missing section table")
    if shstrndx == 0 or shstrndx >= shnum:
        raise ValueError("missing section-name table")
    if shoff + shentsize * shnum > len(data):
        raise ValueError("section table out of bounds")

    def section_header(index):
        return struct.unpack_from(shdr_format, data, shoff + index * shentsize)

    strtab_offset = section_header(shstrndx)[4]
    strtab_size = section_header(shstrndx)[5]
    if strtab_offset + strtab_size > len(data):
        raise ValueError("section-name table out of bounds")
    strtab = data[strtab_offset : strtab_offset + strtab_size]

    def section_name(offset):
        if offset >= len(strtab):
            return ""
        end = strtab.find(b"\0", offset)
        if end < 0:
            end = len(strtab)
        return strtab[offset:end].decode("utf-8", "replace")

    sections = []
    for index in range(shnum):
        fields = section_header(index)
        sections.append((section_name(fields[0]), fields[4], fields[5]))
    return sections


def parse_modinfo(data: bytes) -> dict:
    for name, offset, size in _parse_elf_sections(data):
        if name != ".modinfo":
            continue
        if offset + size > len(data):
            raise ValueError(".modinfo section out of bounds")
        fields = {}
        for item in data[offset : offset + size].split(b"\0"):
            if not item:
                continue
            key, separator, value = item.partition(b"=")
            if not separator:
                continue
            fields[key.decode("utf-8", "replace")] = value.decode("utf-8", "replace")
        return fields
    return {}


def partition_of(path: Path) -> str:
    for part in path.resolve().parts:
        lowered = part.lower()
        if lowered in ("vendor_dlkm", "odm_dlkm"):
            return lowered
    return "unknown"


def describe_module(path: Path, relative_path: str) -> dict:
    data = read_module_bytes(path)
    fields = parse_modinfo(data)
    name = module_stem(path.name)
    if not name:
        raise AuditError("cannot derive module name from filename")
    license_text = fields.get("license")
    return {
        "name": name,
        "modinfo_name": fields.get("name"),
        "file": relative_path,
        "partition": partition_of(path),
        "license": license_text,
        "license_class": classify_license(license_text),
        "vermagic": fields.get("vermagic"),
        "srcversion": fields.get("srcversion"),
        "depends": fields.get("depends"),
        "compression": compression_of(path.name),
        "size": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def scan_roots(roots):
    modules = []
    errors = []
    seen = set()
    for root in roots:
        root_path = Path(root)
        if not root_path.is_dir():
            raise AuditError(f"not a directory: {root}")
        for dirpath, dirnames, filenames in os.walk(root_path):
            dirnames.sort()
            for filename in sorted(filenames):
                if not filename.lower().endswith(MODULE_SUFFIXES):
                    continue
                path = Path(dirpath) / filename
                if path.is_symlink():
                    continue
                resolved = str(path.resolve())
                if resolved in seen:
                    continue
                seen.add(resolved)
                relative_path = path.relative_to(root_path).as_posix()
                try:
                    modules.append(describe_module(path, relative_path))
                except (AuditError, OSError, ValueError, EOFError, lzma.LZMAError) as exc:
                    errors.append({"file": path.as_posix(), "error": str(exc)})
    modules.sort(key=lambda module: (module["name"], module["file"]))
    return modules, errors


def parse_makefile(text: str):
    text = text.replace("\r\n", "\n").replace("\\\n", " ")
    targets = []
    for raw_line in text.split("\n"):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        match = _OBJ_RULE.match(line)
        if not match:
            continue
        config = match.group(2)
        for token in match.group(3).split():
            token = token.strip('"')
            if "$(" in token or token.startswith("$"):
                continue
            if not token.endswith(".o"):
                continue
            name = os.path.basename(token)[:-2]
            if name:
                targets.append((name, config))
    return targets


def _iter_makefiles(root: Path, excludes):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(
            name for name in dirnames if name not in excludes and not name.startswith(".")
        )
        for filename in ("Kbuild", "Makefile"):
            if filename in filenames:
                yield Path(dirpath) / filename


def build_published_inventory(tree: str, branch=None, label=None, excludes=None):
    root = Path(tree)
    if not root.is_dir():
        raise AuditError(f"not a directory: {tree}")
    excluded = set(excludes or DEFAULT_EXCLUDES)
    entries = {}
    for makefile in _iter_makefiles(root, excluded):
        relative = makefile.relative_to(root).as_posix()
        try:
            text = makefile.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise AuditError(f"cannot read {makefile}: {exc}") from exc
        for name, config in parse_makefile(text):
            key = normalize_name(name)
            entry = entries.setdefault(
                key,
                {"name": name, "sources": set(), "conditional": None, "configs": set()},
            )
            entry["sources"].add(relative)
            if config:
                entry["configs"].add(config)
            if entry["conditional"] is not False:
                entry["conditional"] = config is not None
    modules = []
    for key in sorted(entries):
        entry = entries[key]
        modules.append(
            {
                "name": entry["name"],
                "sources": sorted(entry["sources"]),
                "conditional": bool(entry["conditional"]),
                "configs": sorted(entry["configs"]),
            }
        )
    return {
        "schema": SCHEMA_PUBLISHED,
        "tool_version": TOOL_VERSION,
        "generated_at": utc_now(),
        "tree": Path(tree).as_posix(),
        "branch": branch,
        "label": label,
        "excluded_directories": sorted(excluded),
        "modules": modules,
    }


def load_json(path: str, expected_schema: str) -> dict:
    try:
        document = json.loads(Path(path).read_text(encoding="utf-8"))
    except OSError as exc:
        raise AuditError(f"cannot read {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise AuditError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(document, dict) or document.get("schema") != expected_schema:
        raise AuditError(f"{path}: expected schema {expected_schema!r}")
    return document


def write_json(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(document, handle, indent=2)
        handle.write("\n")


def build_audit(scan_documents, published_documents, metadata, timestamp):
    modules = []
    errors = []
    roots = []
    for document in scan_documents:
        modules.extend(document.get("modules", []))
        errors.extend(document.get("errors", []))
        roots.extend(document.get("roots", []))

    published = {}
    for document in published_documents:
        for entry in document.get("modules", []):
            key = normalize_name(entry["name"])
            current = published.setdefault(
                key,
                {
                    "name": entry["name"],
                    "sources": set(),
                    "configs": set(),
                    "conditional": True,
                },
            )
            current["sources"].update(entry.get("sources", []))
            current["configs"].update(entry.get("configs", []))
            current["conditional"] = current["conditional"] and bool(
                entry.get("conditional")
            )

    rows = []
    for module in modules:
        entry = published.get(normalize_name(module["name"]))
        row = dict(module)
        row["published"] = entry is not None
        row["publication_conditional"] = entry["conditional"] if entry else None
        rows.append(row)
    rows.sort(key=lambda row: (row["name"], row["file"]))

    gpl_rows = [row for row in rows if row["license_class"] == "gpl"]
    covered_gpl = [row for row in gpl_rows if row["published"]]
    uncovered_gpl = [row for row in gpl_rows if not row["published"]]
    uncovered_non_gpl = [
        row
        for row in rows
        if row["license_class"] != "gpl" and not row["published"]
    ]

    license_counter = Counter(
        row["license"] if row["license"] else "<absent>" for row in rows
    )
    license_breakdown = dict(
        sorted(license_counter.items(), key=lambda item: (-item[1], item[0]))
    )

    vermagic_counter = Counter(
        row["vermagic"] if row["vermagic"] else "<absent>" for row in rows
    )
    dominant_vermagic = vermagic_counter.most_common(1)[0][0] if rows else None
    vermagic_deviants = sorted(
        row["name"]
        for row in rows
        if (row["vermagic"] if row["vermagic"] else "<absent>") != dominant_vermagic
    )

    partitions = Counter(row["partition"] for row in rows)

    summary = {
        "modules_scanned": len(rows),
        "gpl_declared": len(gpl_rows),
        "non_gpl": sum(1 for row in rows if row["license_class"] == "non-gpl"),
        "unknown_license": sum(
            1 for row in rows if row["license_class"] == "unknown"
        ),
        "gpl_covered": len(covered_gpl),
        "gpl_uncovered": len(uncovered_gpl),
        "non_gpl_uncovered": len(uncovered_non_gpl),
        "published_module_targets": len(published),
        "scan_errors": len(errors),
    }

    return {
        "schema": SCHEMA_AUDIT,
        "tool_version": TOOL_VERSION,
        "generated_at": timestamp,
        "metadata": metadata,
        "scope_note": SCOPE_NOTE,
        "roots": sorted(set(roots)),
        "summary": summary,
        "license_breakdown": license_breakdown,
        "vermagic": {
            "counts": dict(
                sorted(vermagic_counter.items(), key=lambda item: (-item[1], item[0]))
            ),
            "dominant": dominant_vermagic,
            "deviants": vermagic_deviants,
        },
        "partitions": dict(sorted(partitions.items())),
        "modules": rows,
        "uncovered_gpl": [row["name"] for row in uncovered_gpl],
        "uncovered_non_gpl": [row["name"] for row in uncovered_non_gpl],
        "scan_errors": errors,
        "published_trees": [
            {
                "tree": document.get("tree"),
                "branch": document.get("branch"),
                "label": document.get("label"),
                "module_targets": len(document.get("modules", [])),
            }
            for document in published_documents
        ],
    }


def _cell(value) -> str:
    return str(value).replace("|", "\\|")


def _markdown_table(headers, rows) -> list:
    lines = ["| " + " | ".join(headers) + " |"]
    lines.append("|" + "|".join("---" for _ in headers) + "|")
    for row in rows:
        lines.append("| " + " | ".join(_cell(cell) for cell in row) + " |")
    return lines


def render_report(audit: dict) -> str:
    summary = audit["summary"]
    metadata = audit["metadata"]
    lines = [
        "# GPLv2 module compliance audit",
        "",
        f"_{audit['scope_note']}_",
        "",
        "## Device",
        "",
    ]
    if metadata:
        lines.extend(
            _markdown_table(
                ["Field", "Value"],
                [[key.replace("_", " ").capitalize(), value] for key, value in metadata.items()],
            )
        )
    else:
        lines.append("No device metadata supplied (see `--device`, `--firmware`, `--kernel-commit`).")
    lines.extend(
        [
            "",
            "## Summary",
            "",
        ]
    )
    lines.extend(
        _markdown_table(
            ["Metric", "Value"],
            [
                ["Modules scanned", summary["modules_scanned"]],
                ["GPL-declared modules", summary["gpl_declared"]],
                ["Non-GPL modules", summary["non_gpl"]],
                ["Modules without license field", summary["unknown_license"]],
                ["GPL-declared with matching published target", summary["gpl_covered"]],
                ["GPL-declared without matching published target", summary["gpl_uncovered"]],
                ["Non-GPL without matching published target", summary["non_gpl_uncovered"]],
                ["Published module targets loaded", summary["published_module_targets"]],
                ["Unreadable module files", summary["scan_errors"]],
            ],
        )
    )
    lines.extend(["", "## Declared licenses", ""])
    lines.extend(
        _markdown_table(
            ["License string", "Modules"],
            [[f"`{license_text}`", count] for license_text, count in audit["license_breakdown"].items()],
        )
    )

    lines.extend(
        [
            "",
            "## Published source trees",
            "",
        ]
    )
    if audit["published_trees"]:
        lines.extend(
            _markdown_table(
                ["Tree", "Branch", "Label", "Module targets"],
                [
                    [
                        tree["tree"] or "",
                        tree["branch"] or "",
                        tree["label"] or "",
                        tree["module_targets"],
                    ]
                    for tree in audit["published_trees"]
                ],
            )
        )
    else:
        lines.append("No published trees were supplied.")

    lines.extend(
        [
            "",
            "## Coverage",
            "",
            f"{summary['gpl_covered']} of {summary['gpl_declared']} GPL-declared modules "
            "have a matching module target in the supplied published trees.",
            "",
            "### GPL-declared modules without a published source target",
            "",
        ]
    )
    uncovered = [
        row
        for row in audit["modules"]
        if row["license_class"] == "gpl" and not row["published"]
    ]
    if uncovered:
        lines.extend(
            _markdown_table(
                ["Module", "License", "Partition", "File"],
                [
                    [
                        f"`{row['name']}`",
                        row["license"] or "<absent>",
                        row["partition"],
                        f"`{row['file']}`",
                    ]
                    for row in uncovered
                ],
            )
        )
    else:
        lines.append("None.")

    lines.extend(
        [
            "",
            "## Modules without a GPL declaration and without a published target",
            "",
            "Informational only; these are not counted as GPL compliance gaps.",
            "",
        ]
    )
    uncovered_non_gpl = [
        row
        for row in audit["modules"]
        if row["license_class"] != "gpl" and not row["published"]
    ]
    if uncovered_non_gpl:
        for row in uncovered_non_gpl:
            lines.append(
                f"- `{row['name']}` — license: {row['license'] or '<absent>'} "
                f"({row['license_class']}), partition: {row['partition']}"
            )
    else:
        lines.append("None.")

    lines.extend(["", "## Vermagic", ""])
    lines.extend(
        _markdown_table(
            ["Vermagic", "Modules"],
            [[f"`{value}`", count] for value, count in audit["vermagic"]["counts"].items()],
        )
    )
    deviants = audit["vermagic"]["deviants"]
    if deviants:
        lines.extend(["", "Modules that differ from the dominant vermagic:", ""])
        for name in deviants:
            lines.append(f"- `{name}`")

    lines.extend(["", "## Reproduction", "", "```bash"])
    roots = " ".join(shlex.quote(root) for root in audit["roots"])
    lines.append(
        f"python3 tools/gpl-audit/rothko_gpl_audit.py scan {roots} --out inventory.json"
    )
    for tree in audit["published_trees"]:
        command = "python3 tools/gpl-audit/rothko_gpl_audit.py inventory " + shlex.quote(
            tree["tree"] or "<tree>"
        )
        if tree["branch"]:
            command += " --branch " + shlex.quote(tree["branch"])
        command += " --out published.json"
        lines.append(command)
    lines.append(
        "python3 tools/gpl-audit/rothko_gpl_audit.py audit "
        "--scan inventory.json --published published.json --outdir reports"
    )
    lines.extend(["```", "", "## Inputs", ""])
    lines.extend(
        _markdown_table(
            ["Input", "Schema", "SHA-256"],
            [
                [
                    f"`{entry['path']}`",
                    f"`{entry['schema']}`",
                    f"`{entry['sha256']}`",
                ]
                for entry in audit["inputs"]
            ],
        )
    )
    lines.extend(
        [
            "",
            f"Generated by `rothko_gpl_audit.py` {audit['tool_version']} at {audit['generated_at']}.",
            "",
        ]
    )
    return "\n".join(lines)


def write_csv(path: Path, audit: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    buffer = io.StringIO()
    writer = csv.writer(buffer, lineterminator="\n")
    writer.writerow(
        [
            "module",
            "coverage",
            "license",
            "license_class",
            "partition",
            "vermagic",
            "file",
            "sha256",
            "publication_conditional",
        ]
    )
    for row in audit["modules"]:
        writer.writerow(
            [
                row["name"],
                "covered" if row["published"] else "missing",
                row["license"] or "",
                row["license_class"],
                row["partition"],
                row["vermagic"] or "",
                row["file"],
                row["sha256"],
                "" if row["publication_conditional"] is None else str(row["publication_conditional"]).lower(),
            ]
        )
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write(buffer.getvalue())


def cmd_scan(args) -> int:
    modules, errors = scan_roots(args.roots)
    document = {
        "schema": SCHEMA_SCAN,
        "tool_version": TOOL_VERSION,
        "generated_at": utc_now(),
        "roots": [Path(root).as_posix() for root in args.roots],
        "modules": modules,
        "errors": errors,
    }
    write_json(Path(args.out), document)
    licenses = Counter(module["license"] if module["license"] else "<absent>" for module in modules)
    breakdown = ", ".join(
        f"{license_text}: {count}"
        for license_text, count in sorted(licenses.items(), key=lambda item: (-item[1], item[0]))
    )
    print(f"scanned {len(modules)} modules ({breakdown or 'none'}) -> {args.out}")
    if errors:
        print(f"warning: {len(errors)} unreadable module file(s) recorded", file=sys.stderr)
    return EXIT_OK


def cmd_inventory(args) -> int:
    document = build_published_inventory(
        args.tree, branch=args.branch, label=args.label, excludes=args.exclude
    )
    write_json(Path(args.out), document)
    conditional = sum(1 for module in document["modules"] if module["conditional"])
    print(
        f"found {len(document['modules'])} module targets "
        f"({conditional} conditional) in {args.tree} -> {args.out}"
    )
    return EXIT_OK


def cmd_audit(args) -> int:
    timestamp = args.now or utc_now()
    try:
        datetime.datetime.fromisoformat(timestamp.replace("Z", "+00:00"))
    except ValueError as exc:
        raise AuditError(f"invalid --now timestamp {timestamp!r}: {exc}") from exc

    scan_paths = args.scan
    published_paths = args.published
    scan_documents = [load_json(path, SCHEMA_SCAN) for path in scan_paths]
    published_documents = [load_json(path, SCHEMA_PUBLISHED) for path in published_paths]

    metadata = {}
    if args.device:
        metadata["device"] = args.device
    if args.firmware:
        metadata["firmware"] = args.firmware
    if args.kernel_commit:
        metadata["kernel_commit"] = args.kernel_commit

    audit = build_audit(scan_documents, published_documents, metadata, timestamp)
    audit["inputs"] = [
        {"path": Path(path).as_posix(), "schema": document["schema"], "sha256": sha256_file(Path(path))}
        for path, document in zip(scan_paths + published_paths, scan_documents + published_documents)
    ]

    outdir = Path(args.outdir)
    write_json(outdir / "audit.json", audit)
    write_csv(outdir / "audit.csv", audit)
    outdir.mkdir(parents=True, exist_ok=True)
    with open(outdir / "report.md", "w", encoding="utf-8", newline="\n") as handle:
        handle.write(render_report(audit))

    summary = audit["summary"]
    print(
        f"audit: {summary['modules_scanned']} modules, "
        f"{summary['gpl_declared']} GPL-declared, "
        f"{summary['gpl_covered']} covered, "
        f"{summary['gpl_uncovered']} uncovered -> {args.outdir}"
    )
    if summary["scan_errors"]:
        print(
            f"warning: {summary['scan_errors']} unreadable module file(s); "
            "the inventory is incomplete",
            file=sys.stderr,
        )

    if summary["scan_errors"] and not args.allow_scan_errors:
        print("error: refusing to certify coverage from an incomplete scan", file=sys.stderr)
        return EXIT_ERROR
    return EXIT_UNCOVERED if summary["gpl_uncovered"] else EXIT_OK


class _ArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        self.print_usage(sys.stderr)
        self.exit(EXIT_ERROR, f"{self.prog}: error: {message}\n")


def build_parser() -> argparse.ArgumentParser:
    parser = _ArgumentParser(
        prog="rothko_gpl_audit.py",
        description="Audit GPL-declared vendor kernel modules against published source trees.",
    )
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    subparsers = parser.add_subparsers(dest="command", required=True)

    scan = subparsers.add_parser("scan", help="read .ko/.ko.xz/.ko.gz/.ko.zst files")
    scan.add_argument("roots", nargs="+", help="directories containing kernel modules")
    scan.add_argument("--out", default="inventory.json", help="scan JSON output path")
    scan.set_defaults(func=cmd_scan)

    inventory = subparsers.add_parser(
        "inventory", help="collect module targets from a published source tree"
    )
    inventory.add_argument("tree", help="path to a kernel source checkout")
    inventory.add_argument("--branch", help="branch name recorded in the output")
    inventory.add_argument("--label", help="free-form label recorded in the output")
    inventory.add_argument("--out", default="published.json", help="inventory JSON output path")
    inventory.add_argument(
        "--exclude",
        action="append",
        default=None,
        help="directory name to skip (repeatable; replaces the defaults)",
    )
    inventory.set_defaults(func=cmd_inventory)

    audit = subparsers.add_parser(
        "audit", help="diff a scan against published inventories and render reports"
    )
    audit.add_argument("--scan", action="append", required=True, help="scan JSON (repeatable)")
    audit.add_argument(
        "--published", action="append", required=True, help="published JSON (repeatable)"
    )
    audit.add_argument("--outdir", default="reports", help="report output directory")
    audit.add_argument("--device", help="device name recorded in the report")
    audit.add_argument("--firmware", help="firmware build recorded in the report")
    audit.add_argument("--kernel-commit", help="kernel git hash recorded in the report")
    audit.add_argument(
        "--now", help="ISO-8601 timestamp used as generated_at (for reproducible reports)"
    )
    audit.add_argument(
        "--allow-scan-errors",
        action="store_true",
        help="report coverage even if some module files could not be read",
    )
    audit.set_defaults(func=cmd_audit)
    return parser


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except AuditError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return EXIT_ERROR


if __name__ == "__main__":
    sys.exit(main())
