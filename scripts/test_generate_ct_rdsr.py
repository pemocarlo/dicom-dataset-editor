# /// script
# requires-python = ">=3.13,<3.14"
# dependencies = ["highdicom==0.28.1", "pydicom==3.0.2"]
# ///
# ruff: noqa: C901, COM812, CPY001, D101, D102, D103, PLC0415, PLR0912, PLR0915, PT009, PT027, S603, TC006
"""Run with uv run --python 3.13 scripts/test_generate_ct_rdsr.py.

Focused template regression checks use standard code values independently of
the generator's keyword lookup. Set DCIODVFY to optionally validate every case
with dicom3tools; DCMTK dsr2xml is exercised when installed.
"""

from __future__ import annotations

import io
import itertools
import os
import shutil
import subprocess
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from typing import cast

import pydicom
from generate_ct_rdsr import (
    ACQUISITIONS,
    EFFECTIVE_METHODS,
    SSDE_METHODS,
    WATER_METHODS,
    Acquisition,
    EffectiveMethod,
    Generator,
    Options,
    numeric,
    validate,
    walk,
)
from highdicom import sr
from pydicom import Dataset


def find(parent: Dataset, code: str) -> list[Dataset]:
    return [
        child
        for child in parent.get("ContentSequence", [])
        if child.ConceptNameCodeSequence[0].CodeValue == code
    ]


def one(parent: Dataset, code: str) -> Dataset:
    (result,) = find(parent, code)
    return result


def value(node: Dataset) -> str:
    return node.ConceptCodeSequence[0].CodeValue


class RadiationDoseTests(unittest.TestCase):
    def check_conditions(self, ds: Dataset) -> None:
        procedure = one(ds, "121058")
        self.assertEqual(value(procedure), "77477000")
        self.assertEqual(
            one(procedure, "363703001").RelationshipType, "HAS CONCEPT MOD"
        )
        accumulated = one(ds, "113811")
        self.assertEqual(numeric(one(accumulated, "113812")), len(find(ds, "113819")))
        for effective in find(accumulated, "113814"):
            self.assertEqual(len(find(effective, "121406")), 1)
            method = value(one(effective, "370129005"))
            self.assertEqual(
                bool(find(effective, "113815")), method in ("113800", "113801")
            )
            self.assertEqual(
                bool(find(effective, "113816")), method in ("113802", "113803")
            )
        prior_uids = set()
        for event in find(ds, "113819"):
            event_uid = one(event, "113769").UID
            acquisition = value(one(event, "113820"))
            for repeated in find(event, "128551"):
                self.assertEqual(
                    bool(find(repeated, "128552")), value(repeated) == "373066001"
                )
                for previous in find(repeated, "113769"):
                    self.assertIn(previous.UID, prior_uids)
            prior_uids.add(event_uid)
            for rejected in find(event, "130503"):
                self.assertEqual(
                    bool(find(rejected, "130504")), value(rejected) == "373066001"
                )
            parameters = one(event, "113822")
            self.assertFalse(find(one(parameters, "113825"), "113893"))
            self.assertEqual(
                bool(find(parameters, "113828")), acquisition in ("116152004", "113804")
            )
            if acquisition != "116152004":
                self.assertFalse(find(parameters, "113899"))
            for source in find(parameters, "113831"):
                self.assertGreaterEqual(
                    numeric(one(source, "113833")), numeric(one(source, "113734"))
                )
                self.assertEqual(bool(find(source, "113834")), acquisition != "113805")
            self.assertEqual(bool(find(event, "113829")), acquisition != "113805")
            for dose in find(event, "113829"):
                self.assertEqual(
                    numeric(one(dose, "113838")),
                    numeric(one(dose, "113830"))
                    * numeric(one(parameters, "113825"))
                    / 10,
                )
                for effective in find(dose, "113839"):
                    method_node = one(effective, "370129005")
                    dlp_method = value(method_node) in ("113800", "113802")
                    self.assertEqual(bool(find(method_node, "113840")), dlp_method)
                    if dlp_method:
                        self.assertEqual(
                            numeric(effective),
                            numeric(one(dose, "113838"))
                            * numeric(one(method_node, "113840")),
                        )
                for ssde in find(dose, "113930"):
                    method = value(one(ssde, "370129005"))
                    self.assertEqual(
                        bool(find(ssde, "113931")), method in ("113934", "113936")
                    )
                    self.assertEqual(
                        bool(find(ssde, "113932")), method in ("113935", "113936")
                    )
                    self.assertEqual(
                        bool(find(ssde, "113933")),
                        method in ("113934", "113935", "113936", "113937"),
                    )
                    water = method in ("113988", "113989")
                    self.assertEqual(bool(find(ssde, "113980")), water)
                    self.assertIn(len(find(ssde, "113991")), (0, 2) if water else (0,))
                    for diameter in find(ssde, "113980"):
                        if value(one(diameter, "370129005")) == "113981":
                            one(diameter, "113994")
                            one(parameters, "112227")
                    profile = find(ssde, "113993")
                    self.assertEqual(bool(profile), method == "113989")
                    if profile:
                        self.assertEqual(
                            numeric(ssde),
                            sum(numeric(point) for point in profile) / len(profile),
                        )
                        self.assertEqual(
                            numeric(one(ssde, "113980")),
                            sum(numeric(one(point, "113995")) for point in profile)
                            / len(profile),
                        )
                        for point in profile:
                            one(point, "113994")
                for check_code, configured_codes, threshold_codes, estimate_codes in [
                    (
                        "113900",
                        ("113901", "113902"),
                        ("113903", "113904"),
                        ("113905", "113906"),
                    ),
                    (
                        "113908",
                        ("113909", "113910"),
                        ("113911", "113912"),
                        ("113913", "113914"),
                    ),
                ]:
                    for check in find(dose, check_code):
                        for flag, threshold in zip(
                            configured_codes, threshold_codes, strict=True
                        ):
                            self.assertEqual(
                                bool(find(check, threshold)),
                                value(one(check, flag)) == "373066001",
                            )
                        exceeded = any(
                            numeric(estimate) > numeric(one(check, threshold))
                            for threshold, estimate_code in zip(
                                threshold_codes, estimate_codes, strict=True
                            )
                            for estimate in find(check, estimate_code)
                        )
                        if not exceeded:
                            self.assertFalse(find(check, "113907"))
                            self.assertFalse(find(check, "113870"))
                        elif check_code == "113900":
                            one(check, "113870")
        for node in walk(ds):
            if node.ValueType == "NUM":
                measured = node.MeasuredValueSequence[0]
                self.assertLessEqual(len(str(measured.NumericValue)), 16)
                self.assertEqual(
                    measured.MeasurementUnitsCodeSequence[0].CodingSchemeDesignator,
                    "UCUM",
                )
        validate(ds)

    def check_roundtrip(self, options: Options, seed: int) -> Dataset:
        ds = Generator(seed, options).generate()
        self.check_conditions(ds)
        stream = io.BytesIO()
        pydicom.dcmwrite(stream, ds, enforce_file_format=True)
        self.assertEqual(stream.getvalue()[128:132], b"DICM")
        stream.seek(0)
        read = pydicom.dcmread(stream)
        self.check_conditions(read)
        self.assertEqual(read.file_meta.MediaStorageSOPInstanceUID, read.SOPInstanceUID)
        # Independently reconstruct through highdicom's content parser.
        content = sr.ContentSequence.from_sequence([read], is_root=True)
        self.assertEqual(len(content), 1)
        validators = []
        if executable := os.environ.get("DCIODVFY"):
            validators.append([executable])
        if executable := shutil.which("dsr2xml"):
            validators.append([executable])
        if validators:
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "report.dcm"
                path.write_bytes(stream.getvalue())
                for command in validators:
                    result = subprocess.run(
                        [*command, str(path)],
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertNotIn("Warning", result.stderr)
                    self.assertNotIn("Error", result.stderr)
                    self.assertNotIn("W:", result.stderr)
        return ds

    def test_acquisition_optional_and_effective_method_matrix(self) -> None:
        for acquisition, optional, method in itertools.product(
            ACQUISITIONS, ("all", "random", "none"), EFFECTIVE_METHODS
        ):
            with self.subTest(
                acquisition=acquisition, optional=optional, method=method
            ):
                self.check_roundtrip(
                    Options(
                        acquisition=cast(Acquisition, acquisition),
                        optional=optional,
                        effective_method=cast(EffectiveMethod, method),
                    ),
                    42,
                )

    def test_alternative_branches(self) -> None:
        for field, values in {
            "water_method": WATER_METHODS,
            "ssde_method": SSDE_METHODS,
            "reference_authority": ("text", "code"),
            "quality_format": ("num", "text", "code"),
            "dose_checks": ("absent", "unconfigured", "normal", "exceeded"),
            "scope": ("study", "series", "pps", "pps-to-date", "event"),
        }.items():
            for value_ in values:
                with self.subTest(field=field, value=value_):
                    self.check_roundtrip(
                        replace(Options(events=1), **{field: value_}), 91
                    )

    def test_random_optional_dependencies(self) -> None:
        for seed in range(30):
            with self.subTest(seed=seed):
                self.check_roundtrip(
                    Options(optional="random", acquisition="random"), seed
                )

    def test_optional_row_coverage(self) -> None:
        ds = self.check_roundtrip(Options(), 1)
        present = {node.ConceptNameCodeSequence[0].CodeValue for node in walk(ds)}
        # All U/UC row concept names across the included templates, excluding
        # mutually exclusive TEXT reference authority / image-quality forms.
        required = {
            "121049",
            "121046",
            "121106",
            "113870",
            "113871",
            "113872",
            "113873",
            "113874",
            "128774",
            "121009",
            "121010",
            "121011",
            "128775",
            "128003",
            "C54627",
            "121013",
            "121014",
            "121015",
            "121016",
            "121017",
            "113876",
            "110119",
            "121061",
            "121000",
            "74711-3",
            "120999",
            "113877",
            "130745",
            "113814",
            "125203",
            "113961",
            "408730004",
            "113605",
            "128551",
            "113769",
            "130503",
            "111526",
            "113821",
            "113836",
            "113837",
            "113839",
            "113930",
            "113991",
            "113985",
            "113842",
            "131550",
            "113893",
            "113899",
            "113895",
            "113896",
            "113897",
            "113898",
            "113907",
            "113915",
        }
        self.assertFalse(required - present, required - present)
        for node in walk(ds):
            if node.ConceptNameCodeSequence[0].CodeValue == "113930":
                self.assertIsInstance(node, sr.NumContentItem)

    def test_reproducibility_and_validation_failure(self) -> None:
        first = Generator(7, Options()).generate()
        second = Generator(7, Options()).generate()
        self.assertEqual(first, second)
        self.assertNotEqual(
            first.SOPInstanceUID, Generator(8, Options()).generate().SOPInstanceUID
        )
        total = one(one(first, "113811"), "113813")
        total.MeasuredValueSequence[0].NumericValue = "999999"
        with self.assertRaisesRegex(ValueError, "Total DLP"):
            validate(first)

    def test_accumulation_scope_uses_report_identity(self) -> None:
        for scope, uid_code in (
            ("study", "110180"),
            ("series", "112002"),
        ):
            with self.subTest(scope=scope):
                report = Generator(
                    12, replace(Options(events=1), scope=scope)
                ).generate()
                scope_item = one(report, "113705")
                uid_item = one(scope_item, uid_code)
                expected_uid = (
                    report.StudyInstanceUID
                    if scope == "study"
                    else report.SeriesInstanceUID
                )
                self.assertEqual(uid_item.UID, expected_uid)

    def test_cli_and_no_overwrite(self) -> None:
        import sys

        script = Path(__file__).with_name("generate_ct_rdsr.py")
        with tempfile.TemporaryDirectory() as directory:
            command = [
                sys.executable,
                str(script),
                "--output",
                directory,
                "--seed",
                "8",
                "--count",
                "2",
            ]
            result = subprocess.run(
                command, capture_output=True, text=True, check=False
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            before = {
                path.name: path.read_bytes() for path in Path(directory).glob("*.dcm")
            }
            self.assertEqual(len(before), 2)
            result = subprocess.run(
                command, capture_output=True, text=True, check=False
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Refusing to overwrite", result.stderr)
            self.assertEqual(
                before,
                {
                    path.name: path.read_bytes()
                    for path in Path(directory).glob("*.dcm")
                },
            )
            for arguments in (
                ["--events", "0"],
                ["--sources", "0"],
                ["--scope", "event"],
                ["--count", "-1"],
            ):
                result = subprocess.run(
                    [sys.executable, str(script), *arguments],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 2)


if __name__ == "__main__":
    unittest.main()
