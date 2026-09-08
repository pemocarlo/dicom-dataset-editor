#!/usr/bin/env -S uv run --script
# ruff: noqa: C901, COM812, CPY001, D101, D102, D103, D107, EM101, PERF401, PLR0912, PLR0915, PLR2004, S311, T201, TC003, TRY003
# /// script
# requires-python = ">=3.13,<3.14"
# dependencies = ["highdicom==0.28.1", "pydicom==3.0.2"]
# ///
"""Generate synthetic CT Radiation Dose SR files (DICOM PS3.16 2026c).

Run: uv run --python 3.13 scripts/generate_ct_rdsr.py --output /tmp/rdsr
All dependencies are declared above; no project install or input images needed.
The default includes all applicable optional rows. Alternatives are selected
with --effective-method, --reference-authority, --water-method, --acquisition,
and --quality-format. --optional random samples optional rows; none omits them.
--ssde-method all includes every SSDE method as a separate measurement.

Templates: 10011-10016, 1002-1004, 1015, 1020, 1021, 1204.
https://dicom.nema.org/medical/dicom/current/output/chtml/part16/sect_CTRadiationDoseSRIODTemplates.html
https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_A.35.8.3.html

These are synthetic software fixtures, not patient dose calculations. Internal
checks cover construction invariants, not independent DICOM certification.
UIDs identify synthetic acquisitions; no referenced images are produced.
Seed reproduces content and UIDs: use distinct seeds for distinct fixture sets.
Private 99SYNTH codes used by CODE image-quality parameters are explicitly
synthetic values (the parameter names themselves are from CID 10037).
"""

from __future__ import annotations

import argparse
import random
from collections import defaultdict
from collections.abc import Callable, Iterator
from collections.abc import Sequence as Items
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from decimal import Decimal
from pathlib import Path
from typing import Literal, cast

import pydicom
import pydicom.config
from highdicom import sr
from pydicom import Dataset, FileDataset, FileMetaDataset
from pydicom.sequence import Sequence as DicomSequence
from pydicom.sr.codedict import codes
from pydicom.sr.coding import Code
from pydicom.uid import UID, ExplicitVRLittleEndian, XRayRadiationDoseSRStorage

type OptionalMode = Literal["all", "random", "none"]
type Name = str | Code
type Acquisition = Literal[
    "spiral",
    "sequenced",
    "stationary",
    "free",
    "constant-angle",
    "cone-beam",
    "random",
]
type EffectiveMethod = Literal["dlp-mc", "air-mc", "dlp-measured", "air-measured"]
type Scope = Literal["study", "series", "pps", "pps-to-date", "event"]

MOD = sr.RelationshipTypeValues.HAS_CONCEPT_MOD
PROP = sr.RelationshipTypeValues.HAS_PROPERTIES
OBS = sr.RelationshipTypeValues.HAS_OBS_CONTEXT
INF = sr.RelationshipTypeValues.INFERRED_FROM
METHOD = Code("370129005", "SCT", "Measurement Method")
YES = Code("373066001", "SCT", "Yes")
NO = Code("373067005", "SCT", "No")
EXTRA = {
    "DateTimeStarted": Code("111526", "DCM", "DateTime Started"),
    "ImageQualityReferenceParameters": Code(
        "131550",
        "DCM",
        "Image Quality Reference Parameters",
    ),
    "NoiseIndex": Code("131551", "DCM", "Noise Index"),
    "ReferenceMAs": Code("131552", "DCM", "Reference mAs"),
    "LongitudinalModulation": Code("114102", "DCM", "Longitudinal modulation"),
    "LongitudinalPositionZ": Code("113994", "DCM", "Longitudinal Position Z"),
}
ACQUISITIONS = {
    "spiral": Code("116152004", "SCT", "Spiral Acquisition"),
    "sequenced": Code("113804", "DCM", "Sequenced Acquisition"),
    "stationary": Code("113806", "DCM", "Stationary Acquisition"),
    "free": Code("113807", "DCM", "Free Acquisition"),
    "constant-angle": Code("113805", "DCM", "Constant Angle Acquisition"),
    "cone-beam": Code("702569007", "SCT", "Cone Beam Acquisition"),
}
EFFECTIVE_METHODS = {
    "dlp-mc": "DLPToEConversionViaMCComputation",
    "air-mc": "CtdifreeairToEConversionViaMCComputation",
    "dlp-measured": "DLPToEConversionViaMeasurement",
    "air-measured": "CtdifreeairToEConversionViaMeasurement",
}
SSDE_METHODS = {
    "lateral": "AAPM204LateralDimension",
    "ap": "AAPM204APDimension",
    "sum": "AAPM204SumOfLateralAndAPDimension",
    "age": "AAPM204EffectiveDiameterEstimatedFromPatientAge",
    "water": "EstimatedFromWaterEquivalentDiameter",
    "water-profile": "ArithmeticAverageOfSSDEZ",
}
WATER_METHODS = {
    "representative": "WaterEquivalentDiameterRepresentativeValue",
    "integrated": "WaterEquivalentDiameterIntegratedAcrossScanRange",
    "localizer": "WaterEquivalentDiameterFromLocalizer",
    "raw": "WaterEquivalentDiameterFromRawData",
}


def concept(name: Name) -> Code:
    if isinstance(name, Code):
        return name
    return EXTRA[name] if name in EXTRA else getattr(codes.DCM, name)


def with_children[T: sr.ContentItem](result: T, children: Items[Dataset] | None) -> T:
    if children:
        result.ContentSequence = sr.ContentSequence(
            cast("Items[sr.ContentItem]", children)
        )
    return result


def container(
    name: Name,
    *,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
    children: Items[Dataset],
) -> sr.ContainerContentItem:
    return with_children(
        sr.ContainerContentItem(
            concept(name),
            is_content_continuous=False,
            relationship_type=rel,
        ),
        children,
    )


def text_item(
    name: Name,
    value: str,
    *,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
    children: Items[Dataset] | None = None,
) -> sr.TextContentItem:
    return with_children(
        sr.TextContentItem(concept(name), value, relationship_type=rel),
        children,
    )


def uid_item(
    name: Name,
    value: str,
    *,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
) -> sr.UIDRefContentItem:
    return sr.UIDRefContentItem(concept(name), value, relationship_type=rel)


def person_name(
    name: Name,
    value: str,
    *,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
    children: Items[Dataset] | None = None,
) -> sr.PnameContentItem:
    return with_children(
        sr.PnameContentItem(concept(name), value, relationship_type=rel),
        children,
    )


def date_time(
    name: Name,
    value: str,
    *,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
) -> sr.DateTimeContentItem:
    return sr.DateTimeContentItem(concept(name), value, relationship_type=rel)


def num(
    name: Name,
    value: float,
    unit: str,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
    children: Items[Dataset] | None = None,
) -> sr.NumContentItem:
    meaning = {
        "{events}": "events",
        "{ratio}": "ratio",
        "{X-Ray sources}": "X-Ray sources",
        "a": "Year",
    }.get(unit, unit)
    return with_children(
        sr.NumContentItem(
            concept(name),
            float(value),
            Code(unit, "UCUM", meaning),
            relationship_type=rel,
        ),
        children,
    )


def code_item(
    name: Name,
    value: Name,
    rel: str | sr.RelationshipTypeValues = sr.RelationshipTypeValues.CONTAINS,
    children: Items[Dataset] | None = None,
) -> sr.CodeContentItem:
    return with_children(
        sr.CodeContentItem(concept(name), concept(value), relationship_type=rel),
        children,
    )


@dataclass(frozen=True)
class Options:
    optional: OptionalMode = "all"
    events: int = 3
    acquisition: Acquisition = "spiral"
    effective_method: EffectiveMethod = "dlp-mc"
    reference_authority: str = "code"
    ssde_method: str = "all"
    water_method: str = "representative"
    quality_format: str = "num"
    scope: Scope = "study"
    sources: int = 2
    dose_checks: str = "exceeded"


@dataclass(frozen=True)
class ReportIdentifiers:
    """UIDs shared by the report-level TID 10011 rows."""

    study: str
    series: str
    sop: str


@dataclass(frozen=True)
class AccumulationScope:
    """TID 10011 scope row, kept consistent with report identity UIDs."""

    name: str
    uid_name: str
    uid: str

    @classmethod
    def for_report(
        cls,
        scope: Scope,
        identifiers: ReportIdentifiers,
        event_uids: list[str],
        new_uid: Callable[[], str],
    ) -> AccumulationScope:
        if scope == "study":
            return cls("Study", "StudyInstanceUID", identifiers.study)
        if scope == "series":
            return cls("Series", "SeriesInstanceUID", identifiers.series)
        if scope == "event":
            return cls("IrradiationEvent", "IrradiationEventUID", event_uids[0])
        return cls(
            "PerformedProcedureStep" if scope == "pps" else "ProcedureStepToThisPoint",
            "PerformedProcedureStepSOPInstanceUID",
            new_uid(),
        )


# ---------------------------------------------------------------------------
# Blueprint layer
#
# These classes contain template identity and shape only.  They never call
# pydicom, consume RNG state, or create ContentItems.  The ContentBuilder
# classes below are the executable layer that turns these blueprints into a
# DICOM content tree.
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class TemplateBlueprint:
    """Immutable identity for one DICOM SR template."""

    template_identifier: str
    root_concept: Name
    is_root: bool = False
    description: str = ""


@dataclass(frozen=True)
class TID10011CTRadiationDoseBlueprint(TemplateBlueprint):
    template_identifier: str = "10011"
    root_concept: Name = "XRayRadiationDoseReport"
    is_root: bool = True
    description: str = "CT Radiation Dose"


@dataclass(frozen=True)
class TID10012CTAccumulatedDoseDataBlueprint(TemplateBlueprint):
    template_identifier: str = "10012"
    root_concept: Name = "CTAccumulatedDoseData"
    description: str = "CT Accumulated Dose Data"


@dataclass(frozen=True)
class TID10013CTIrradiationEventDataBlueprint(TemplateBlueprint):
    template_identifier: str = "10013"
    root_concept: Name = "CTAcquisition"
    description: str = "CT Irradiation Event Data"


@dataclass(frozen=True)
class TID10014ScanningLengthBlueprint(TemplateBlueprint):
    template_identifier: str = "10014"
    root_concept: Name = "ScanningLength"
    description: str = "Scanning Length"


@dataclass(frozen=True)
class TID10015CTDoseCheckDetailsBlueprint(TemplateBlueprint):
    template_identifier: str = "10015"
    root_concept: Name = "DoseCheckAlertDetails"
    description: str = "CT Dose Check Details"


@dataclass(frozen=True)
class TID10016ImageQualityReferenceParametersBlueprint(TemplateBlueprint):
    template_identifier: str = "10016"
    root_concept: Name = "ImageQualityReferenceParameters"
    description: str = "Image Quality Reference Parameters"


@dataclass(frozen=True)
class TID1002ObserverContextBlueprint(TemplateBlueprint):
    template_identifier: str = "1002"
    root_concept: Name = "ObserverType"
    description: str = "Observer Context"


@dataclass(frozen=True)
class TID1003PersonObserverIdentifyingAttributesBlueprint(TemplateBlueprint):
    template_identifier: str = "1003"
    root_concept: Name = "PersonObserverName"
    description: str = "Person Observer Identifying Attributes"


@dataclass(frozen=True)
class TID1004DeviceObserverIdentifyingAttributesBlueprint(TemplateBlueprint):
    template_identifier: str = "1004"
    root_concept: Name = "DeviceObserverUID"
    description: str = "Device Observer Identifying Attributes"


@dataclass(frozen=True)
class TID1015PersonObserverRoleBlueprint(TemplateBlueprint):
    template_identifier: str = "1015"
    root_concept: Name = "PersonObserverRoleInThisProcedure"
    description: str = "Person Observer Role"


@dataclass(frozen=True)
class TID1020PersonParticipantBlueprint(TemplateBlueprint):
    template_identifier: str = "1020"
    root_concept: Name = "PersonName"
    description: str = "Person Participant"


@dataclass(frozen=True)
class TID1021DeviceParticipantBlueprint(TemplateBlueprint):
    template_identifier: str = "1021"
    root_concept: Name = "DeviceRoleInProcedure"
    description: str = "Device Participant"


@dataclass(frozen=True)
class TID1204LanguageOfContentItemAndDescendantsBlueprint(TemplateBlueprint):
    template_identifier: str = "1204"
    root_concept: Name = "LanguageOfContentItemAndDescendants"
    description: str = "Language of Content Item and Descendants"


# ---------------------------------------------------------------------------
# Content-generation layer
# ---------------------------------------------------------------------------


class TID1204LanguageOfContentItemAndDescendantsContentBuilder:
    """Generate pydicom content for the TID 1204 blueprint."""

    blueprint = TID1204LanguageOfContentItemAndDescendantsBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> Dataset | None:
        if not self.report.optional():
            return None
        language: list[Dataset] = []
        if self.report.optional():
            language.append(
                code_item(
                    "CountryOfLanguage",
                    Code("US", "ISO3166_1", "United States"),
                    MOD,
                ),
            )
        return code_item(
            "LanguageOfContentItemAndDescendants",
            Code("en", "RFC5646", "English"),
            MOD,
            language,
        )


class TID1003PersonObserverIdentifyingAttributesContentBuilder:
    """Generate pydicom content for the TID 1003 blueprint."""

    blueprint = TID1003PersonObserverIdentifyingAttributesBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> sr.PersonObserverIdentifyingAttributes:
        return sr.PersonObserverIdentifyingAttributes(
            name="SYNTHETIC^Reader",
            login_name=("synthetic-reader" if self.report.optional() else None),
            organization_name=(
                "Synthetic Imaging Lab" if self.report.optional() else None
            ),
            role_in_organization=(
                codes.cid7452.MedicalPractitioner if self.report.optional() else None
            ),
        )


class TID1004DeviceObserverIdentifyingAttributesContentBuilder:
    """Generate pydicom content for the TID 1004 blueprint."""

    blueprint = TID1004DeviceObserverIdentifyingAttributesBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> sr.DeviceObserverIdentifyingAttributes:
        return sr.DeviceObserverIdentifyingAttributes(
            uid=self.report.recorder_uid,
            name="SYNTH_RDSR" if self.report.optional() else None,
            manufacturer_name=(
                "Synthetic Instruments" if self.report.optional() else None
            ),
            model_name=("RDSR Fixture Generator" if self.report.optional() else None),
            serial_number=("SYNTH-RDSR-001" if self.report.optional() else None),
            physical_location=("Test Lab" if self.report.optional() else None),
            role_in_procedure=(codes.DCM.Recording if self.report.optional() else None),
        )


class TID1020PersonParticipantContentBuilder:
    """Generate pydicom content for the TID 1020 blueprint."""

    blueprint = TID1020PersonParticipantBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(
        self,
        role: str,
        rel: str = "CONTAINS",
    ) -> Dataset:
        children: list[Dataset] = [code_item("PersonRoleInProcedure", role, PROP)]
        for name, value in [
            ("PersonID", "SYNTH-OP-1"),
            ("PersonIDIssuer", "SYNTHETIC"),
            ("OrganizationName", "Synthetic Imaging Lab"),
        ]:
            if self.report.optional():
                children.append(text_item(name, value, rel=PROP))
        if self.report.optional():
            children.append(
                code_item(
                    "PersonRoleInOrganization",
                    codes.cid7452.MedicalPractitioner,
                    PROP,
                ),
            )
        return person_name(
            "PersonName",
            "SYNTHETIC^Operator",
            rel=rel,
            children=children,
        )


class TID1021DeviceParticipantContentBuilder:
    """Generate pydicom content for the TID 1021 blueprint."""

    blueprint = TID1021DeviceParticipantBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> Dataset:
        children: list[Dataset] = []
        if self.report.optional():
            children.append(text_item("DeviceName", "SYNTH_CT", rel=PROP))
        for name, value in [
            ("DeviceManufacturer", "Synthetic Instruments"),
            ("DeviceModelName", "CT Fixture"),
            ("DeviceSerialNumber", "SYNTH-CT-001"),
        ]:
            children.append(text_item(name, value, rel=PROP))
        children.append(
            uid_item("DeviceObserverUID", self.report.scanner_uid, rel=PROP)
        )
        return code_item(
            "DeviceRoleInProcedure",
            "IrradiatingDevice",
            children=children,
        )


class TID1002ObserverContextContentBuilder:
    """Generate pydicom content for TID 1002 and its includes."""

    blueprint = TID1002ObserverContextBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> list[Dataset]:
        result: list[Dataset] = []
        if self.report.optional():
            person = TID1003PersonObserverIdentifyingAttributesContentBuilder(
                self.report
            ).build()
            result.extend(sr.ObserverContext(codes.DCM.Person, person))
            if self.report.optional():
                props = (
                    [
                        text_item(
                            "IdentifierWithinPersonObserverRole",
                            "READER-1",
                            rel=MOD,
                        ),
                    ]
                    if self.report.optional()
                    else []
                )
                result.append(
                    code_item(
                        "PersonObserverRoleInThisProcedure",
                        "Recording",
                        OBS,
                        props,
                    ),
                )
            if self.report.optional():
                result.append(
                    code_item(
                        "ReaderSpecialty",
                        "ThoracicImagingSpecialty",
                        OBS,
                        [num(Code("C54627", "NCIt", "Experience"), 10, "a", PROP)],
                    ),
                )
        device = TID1004DeviceObserverIdentifyingAttributesContentBuilder(
            self.report
        ).build()
        result.extend(sr.ObserverContext(codes.DCM.Device, device))
        if self.report.optional():
            result.append(text_item("StationAETitle", "SYNTH_RDSR", rel=OBS))
        if self.report.optional():
            result.append(
                uid_item(
                    "DeviceObserverManufacturerClassUID",
                    self.report.uid(),
                    rel=OBS,
                ),
            )
        if self.report.optional():
            children = [
                text_item(
                    Code("74711-3", "LN", "Unique Device Identifier"),
                    "SYNTHETIC-NOT-REGISTERED",
                ),
            ]
            if self.report.optional():
                children.append(
                    text_item("DeviceDescription", "Synthetic recording workstation"),
                )
            result.append(
                container("UniqueDeviceIdentifiers", rel=OBS, children=children),
            )
        return result


class TID10013SizeSpecificDoseEstimateContentBuilder:
    """Generate pydicom content for TID 10013 SSDE rows."""

    blueprint = TID10013CTIrradiationEventDataBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self, ctdivol: float, series_uid: str) -> list[Dataset]:
        result: list[Dataset] = []
        methods = (
            list(SSDE_METHODS)
            if self.report.options.ssde_method == "all"
            else [self.report.options.ssde_method]
        )
        for method in methods:
            children: list[Dataset] = [code_item(METHOD, SSDE_METHODS[method], MOD)]
            if method in ("lateral", "sum"):
                children.append(num("MeasuredLateralDimension", 320, "mm", INF))
            if method in ("ap", "sum"):
                children.append(num("MeasuredAPDimension", 240, "mm", INF))
            if method in ("lateral", "ap", "sum", "age"):
                children.append(num("DerivedEffectiveDiameter", 277.128129, "mm", INF))
            if method in ("water", "water-profile") and self.report.optional():
                children.extend(
                    [
                        num("DwConversionFactorCoefficients", 1.0, "mm", INF),
                        num("DwConversionFactorCoefficients", 0.0, "mm", INF),
                    ],
                )
            if self.report.optional():
                children.append(
                    uid_item(
                        "SeriesOrInstanceUsedForWaterEquivalentDiameterEstimation",
                        series_uid,
                        rel=INF,
                    ),
                )
            if method in ("water", "water-profile"):
                water: list[Dataset] = [
                    code_item(
                        METHOD,
                        WATER_METHODS[self.report.options.water_method],
                        MOD,
                    ),
                ]
                if self.report.options.water_method == "representative":
                    water.append(num("LongitudinalPositionZ", 0, "mm", INF))
                children.append(num("WaterEquivalentDiameter", 280, "mm", INF, water))
            if method == "water-profile":
                for z in (-10, 0, 10):
                    children.append(
                        num(
                            "SizeSpecificDoseEstimateAtLongitudinalPositionZ",
                            ctdivol,
                            "mGy",
                            INF,
                            [
                                num("LongitudinalPositionZ", z, "mm", INF),
                                num(
                                    "WaterEquivalentDiameterAtLongitudinalPositionZ",
                                    280,
                                    "mm",
                                    INF,
                                ),
                            ],
                        ),
                    )
            result.append(
                num("SizeSpecificDoseEstimate", ctdivol, "mGy", children=children)
            )
        return result


class TID10015CTDoseCheckDetailsContentBuilder:
    """Generate pydicom content for the TID 10015 blueprint."""

    blueprint = TID10015CTDoseCheckDetailsBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self, dlp: float, ctdivol: float, accumulated: float) -> list[Dataset]:
        if self.report.options.dose_checks == "absent":
            return []
        configured = self.report.options.dose_checks != "unconfigured"
        exceeded = self.report.options.dose_checks == "exceeded"
        result: list[Dataset] = []
        for alert in (True, False):
            title = "Alert" if alert else "Notification"
            children: list[Dataset] = [
                code_item(f"DLP{title}ValueConfigured", YES if configured else NO),
                code_item(f"Ctdivol{title}ValueConfigured", YES if configured else NO),
            ]
            estimate = accumulated if alert else dlp
            if configured:
                factor = 0.8 if exceeded else 1.2
                children.extend(
                    [
                        num(f"DLP{title}Value", round(estimate * factor, 6), "mGy.cm"),
                        num(f"Ctdivol{title}Value", round(ctdivol * factor, 6), "mGy"),
                    ],
                )
            if exceeded:
                children.extend(
                    [
                        num(
                            "AccumulatedDLPForwardEstimate"
                            if alert
                            else "DLPForwardEstimate",
                            estimate,
                            "mGy.cm",
                        ),
                        num(
                            "AccumulatedCtdivolForwardEstimate"
                            if alert
                            else "CtdivolForwardEstimate",
                            ctdivol,
                            "mGy",
                        ),
                    ],
                )
                if self.report.optional():
                    children.append(
                        text_item(
                            "ReasonForProceeding",
                            "Synthetic threshold-exceedance test",
                        ),
                    )
                if alert or self.report.optional():
                    children.append(
                        TID1020PersonParticipantContentBuilder(self.report).build(
                            "IrradiationAuthorizing"
                        )
                    )
            if alert and self.report.optional():
                children.append(code_item("AlternativeDoseAlertBehaviorActive", NO))
            result.append(container(f"DoseCheck{title}Details", children=children))
        return result


class TID10014ScanningLengthContentBuilder:
    """Generate pydicom content for the TID 10014 blueprint."""

    blueprint = TID10014ScanningLengthBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(
        self,
        length: float,
        duration: float,
        acquisition: str,
        *,
        has_dose: bool,
        currents: list[int],
    ) -> tuple[list[Dataset], bool]:
        options = self.report.options
        parameters: list[Dataset] = [
            num("ExposureTime", duration, "s"),
            num("ScanningLength", length, "mm"),
        ]
        if self.report.optional():
            parameters.append(num("LengthOfReconstructableVolume", length, "mm"))
        if acquisition == "spiral" and self.report.optional():
            parameters.append(num("ExposedRange", length + 40, "mm"))
        positions = False
        for name, value in [
            ("TopZLocationOfReconstructableVolume", length / 2),
            ("BottomZLocationOfReconstructableVolume", -length / 2),
            ("TopZLocationOfScanningLength", length / 2),
            ("BottomZLocationOfScanningLength", -length / 2),
        ]:
            if self.report.optional():
                parameters.append(num(name, value, "mm"))
                positions = True
        include_ssde = has_dose and self.report.optional()
        if positions or (
            include_ssde
            and options.water_method == "representative"
            and options.ssde_method in ("all", "water", "water-profile")
        ):
            parameters.append(uid_item("FrameOfReferenceUID", self.report.uid()))
        parameters.extend(
            [
                num("NominalSingleCollimationWidth", 0.625, "mm"),
                num("NominalTotalCollimationWidth", 40, "mm"),
            ],
        )
        if acquisition in ("spiral", "sequenced"):
            parameters.append(num("PitchFactor", 1, "{ratio}"))
        parameters.append(
            num("NumberOfXRaySources", options.sources, "{X-Ray sources}")
        )
        for source, current in enumerate(currents):
            source_parameters: list[Dataset] = [
                text_item("IdentificationOfTheXRaySource", str(source + 1)),
                num("KVP", self.report.rng.choice([80, 100, 120, 140]), "kV"),
                num("MaximumXRayTubeCurrent", current + 50, "mA"),
                num("XRayTubeCurrent", current, "mA"),
            ]
            if acquisition != "constant-angle":
                source_parameters.append(num("ExposureTimePerRotation", 0.5, "s"))
            if self.report.optional():
                source_parameters.append(num("XRayFilterAluminumEquivalent", 3, "mm"))
            parameters.append(
                container("CTXRaySourceParameters", children=source_parameters)
            )
        return parameters, include_ssde


class TID10016ImageQualityReferenceParametersContentBuilder:
    """Generate pydicom content for the TID 10016 blueprint."""

    blueprint = TID10016ImageQualityReferenceParametersBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> Dataset | None:
        if not self.report.optional():
            return None
        quality: list[Dataset] = []
        for name, value, unit in [
            ("NoiseIndex", 12, "1"),
            ("ReferenceMAs", 200, "mA.s"),
        ]:
            if self.report.options.quality_format == "num":
                quality.append(num(name, value, unit))
            elif self.report.options.quality_format == "text":
                quality.append(text_item(name, f"{value} {unit}"))
            else:
                quality.append(
                    code_item(
                        name,
                        Code(
                            f"{name}-{value}",
                            "99SYNTH",
                            f"Synthetic {name} {value} {unit}",
                        ),
                    ),
                )
        return container("ImageQualityReferenceParameters", children=quality)


class TID10013CTIrradiationEventDataContentBuilder:
    """Generate pydicom content for a TID 10013 event."""

    blueprint = TID10013CTIrradiationEventDataBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(
        self,
        index: int,
        event_uid: str,
        previous_uid: str | None,
        started: datetime,
        accumulated: float,
    ) -> tuple[Dataset, float, float, str, float]:
        return self.report.build_event(
            index,
            event_uid,
            previous_uid,
            started,
            accumulated,
        )


class TID10012CTAccumulatedDoseDataContentBuilder:
    """Generate pydicom content for the TID 10012 blueprint."""

    blueprint = TID10012CTAccumulatedDoseDataBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(
        self,
        options: Options,
        total: float,
        effective_total: float,
        subtotals: dict[str, float],
    ) -> Dataset:
        return self.report.build_accumulated_dose_data(
            options,
            total,
            effective_total,
            subtotals,
        )


class TID10011CTRadiationDoseContentBuilder:
    """Generate the TID 10011 root and DICOM SR document."""

    blueprint = TID10011CTRadiationDoseBlueprint()

    def __init__(self, report: CTRadiationDoseContentGenerator) -> None:
        self.report = report

    def build(self) -> FileDataset:
        return self.report.build_report()


class CTRadiationDoseContentGenerator:
    """Generate synthetic CT Radiation Dose SR content and DICOM metadata.

    This deliberately does not use ``highdicom.sr.MeasurementReport``: that
    class models TID 1500, while this document is a TID 10011 RDSR. The
    report-level classes here provide the same compositional boundary without
    changing the dose-specific template rows.
    """

    def __init__(self, seed: int | None, options: Options) -> None:
        self.rng = random.Random(seed)
        self.options = options
        self.scanner_uid = self.uid()
        self.recorder_uid = self.uid()

    def uid(self) -> str:
        return f"2.25.{self.rng.getrandbits(128)}"

    def optional(self) -> bool:
        return self.options.optional == "all" or (
            self.options.optional == "random" and self.rng.random() < 0.5
        )

    def person(self, role: str, rel: str = "CONTAINS") -> Dataset:
        return TID1020PersonParticipantContentBuilder(self).build(role, rel)

    def device(self) -> Dataset:
        return TID1021DeviceParticipantContentBuilder(self).build()

    def observers(self) -> list[Dataset]:
        return TID1002ObserverContextContentBuilder(self).build()

    def ssde(self, ctdivol: float, series_uid: str) -> list[Dataset]:
        return TID10013SizeSpecificDoseEstimateContentBuilder(self).build(
            ctdivol, series_uid
        )

    def dose_checks(
        self,
        dlp: float,
        ctdivol: float,
        accumulated: float,
    ) -> list[Dataset]:
        return TID10015CTDoseCheckDetailsContentBuilder(self).build(
            dlp, ctdivol, accumulated
        )

    def event(
        self,
        index: int,
        event_uid: str,
        previous_uid: str | None,
        started: datetime,
        accumulated: float,
    ) -> tuple[Dataset, float, float, str, float]:
        return TID10013CTIrradiationEventDataContentBuilder(self).build(
            index,
            event_uid,
            previous_uid,
            started,
            accumulated,
        )

    def build_event(
        self,
        index: int,
        event_uid: str,
        previous_uid: str | None,
        started: datetime,
        accumulated: float,
    ) -> tuple[Dataset, float, float, str, float]:
        options = self.options
        acquisition = (
            self.rng.choice(list(ACQUISITIONS))
            if options.acquisition == "random"
            else options.acquisition
        )
        length = float(self.rng.randrange(200, 401, 10))
        if acquisition in ("stationary", "free", "cone-beam"):
            length = 40.0
        duration = round(self.rng.uniform(2, 10), 3)
        currents = [self.rng.randrange(80, 301) for _ in range(options.sources)]
        free_air = round(sum(currents) * duration * 0.1, 8)
        ctdivol = round(self.rng.uniform(5, 25), 3)
        # Constant-angle localizers have no CTDIvol/DLP in this model.
        has_dose = acquisition != "constant-angle"
        dlp = round(ctdivol * length / 10, 6) if has_dose else 0.0
        phantom = (
            "IECHeadDosimetryPhantom" if index % 2 == 0 else "IECBodyDosimetryPhantom"
        )
        head = phantom == "IECHeadDosimetryPhantom"
        effective = round(dlp * (0.002 if head else 0.014), 8)
        if options.effective_method.startswith("air"):
            effective = (
                round(free_air * (0.001 if head else 0.005), 8) if has_dose else 0.0
            )
        children: list[Dataset] = []
        if self.optional():
            children.append(
                text_item(
                    "AcquisitionProtocol",
                    "Synthetic Head" if head else "Synthetic Chest",
                ),
            )
        children.append(
            code_item(
                "TargetRegion",
                Code("69536005", "SCT", "Head")
                if head
                else Code("51185008", "SCT", "Thorax"),
            ),
        )
        reconstruction = (
            [code_item("ReconstructionAlgorithm", "IterativeReconstruction", PROP)]
            if self.optional()
            else []
        )
        children.append(
            code_item(
                "CTAcquisitionType",
                ACQUISITIONS[acquisition],
                children=reconstruction,
            ),
        )
        if self.optional():
            children.append(
                code_item(
                    Code("408730004", "SCT", "Procedure Context"),
                    codes.cid10014.CTWithoutContrast,
                ),
            )
        children.append(uid_item("IrradiationEventUID", event_uid))
        if self.optional():
            children.append(
                text_item(
                    "IrradiationEventLabel",
                    str(index + 1),
                    children=[code_item("LabelType", "AcquisitionNumber", MOD)],
                ),
            )
        if self.optional():
            repeated = previous_uid is not None
            props: list[Dataset] = (
                [code_item("ReasonForRepeatingAcquisition", "PatientMotion", MOD)]
                if repeated
                else []
            )
            if repeated and self.optional():
                props.append(
                    uid_item("IrradiationEventUID", previous_uid or "", rel=PROP),
                )
            children.append(
                code_item(
                    "IsRepeatedAcquisition",
                    YES if repeated else NO,
                    children=props,
                ),
            )
        if self.optional():
            children.append(
                code_item(
                    "IsRejectedAcquisition",
                    YES,
                    children=[
                        code_item(
                            "ReasonForRejectingAcquisition", "PatientMotion", MOD
                        ),
                    ],
                ),
            )
        if self.optional():
            children.append(
                date_time("DateTimeStarted", started.strftime("%Y%m%d%H%M%S.%f")),
            )
        parameters, include_ssde = TID10014ScanningLengthContentBuilder(self).build(
            length,
            duration,
            acquisition,
            has_dose=has_dose,
            currents=currents,
        )
        children.append(container("CTAcquisitionParameters", children=parameters))
        if has_dose:
            dose: list[Dataset] = [
                num("MeanCtdivol", ctdivol, "mGy"),
                code_item("CtdiwPhantomType", phantom),
            ]
            if self.optional():
                dose.append(num("CtdifreeairCalculationFactor", 0.1, "mGy/mA.s"))
            if self.optional() or options.effective_method.startswith("air"):
                dose.append(num("MeanCtdifreeair", free_air, "mGy"))
            dose.append(num("DLP", dlp, "mGy.cm"))
            if self.optional():
                modifiers: list[Dataset] = []
                if options.effective_method.startswith("dlp"):
                    modifiers.append(
                        num(
                            "EffectiveDoseConversionFactor",
                            0.002 if head else 0.014,
                            "mSv/mGy.cm",
                            PROP,
                        ),
                    )
                dose.append(
                    num(
                        "EffectiveDose",
                        effective,
                        "mSv",
                        children=[
                            code_item(
                                METHOD,
                                EFFECTIVE_METHODS[options.effective_method],
                                MOD,
                                modifiers,
                            ),
                        ],
                    ),
                )
            if include_ssde:
                dose.extend(self.ssde(ctdivol, self.uid()))
            dose.extend(self.dose_checks(dlp, ctdivol, round(accumulated + dlp, 6)))
            children.append(container("CTDose", children=dose))
        if self.optional():
            children.append(text_item("XRayModulationType", "Longitudinal modulation"))
        if self.optional():
            children.append(code_item("XRayModulationType", "LongitudinalModulation"))
        quality = TID10016ImageQualityReferenceParametersContentBuilder(self).build()
        if quality is not None:
            children.append(quality)
        if self.optional():
            children.append(
                text_item("Comment", "Synthetic irradiation event; not clinical data"),
            )
        if self.optional():
            children.append(self.person("IrradiationAdministering"))
        children.append(self.device())
        return (
            container("CTAcquisition", children=children),
            dlp,
            effective,
            phantom,
            duration,
        )

    def generate(self) -> FileDataset:
        return TID10011CTRadiationDoseContentBuilder(self).build()

    def build_accumulated_dose_data(
        self,
        options: Options,
        total: float,
        effective_total: float,
        subtotals: dict[str, float],
    ) -> Dataset:
        accumulated: list[Dataset] = [
            num("TotalNumberOfIrradiationEvents", options.events, "{events}"),
            num("CTDoseLengthProductTotal", total, "mGy.cm"),
        ]
        if len(subtotals) > 1 and self.optional():
            for phantom, subtotal in subtotals.items():
                accumulated.append(
                    num(
                        "CTDoseLengthProductSubTotal",
                        subtotal,
                        "mGy.cm",
                        children=[code_item("CtdiwPhantomType", phantom, PROP)],
                    ),
                )
        if self.optional():
            authority = (
                code_item("ReferenceAuthority", "ICRPPub103", PROP)
                if options.reference_authority == "code"
                else text_item(
                    "ReferenceAuthority",
                    "Synthetic reference definition, fixture edition 1",
                    rel=PROP,
                )
            )
            properties: list[Dataset] = [
                authority,
                code_item(METHOD, EFFECTIVE_METHODS[options.effective_method], MOD),
            ]
            if options.effective_method.endswith("mc"):
                properties.append(
                    text_item(
                        "PatientModel",
                        "Synthetic adult reference model",
                        rel=PROP,
                    ),
                )
            else:
                properties.append(
                    container(
                        "ConditionEffectiveDoseMeasured",
                        rel=PROP,
                        children=[
                            text_item(
                                "EffectiveDosePhantomType",
                                "Synthetic anthropomorphic phantom",
                            ),
                            text_item("DosimeterType", "Synthetic TLD"),
                        ],
                    ),
                )
            accumulated.append(
                num(
                    "CTEffectiveDoseTotal", effective_total, "mSv", children=properties
                ),
            )
        if self.optional():
            accumulated.append(text_item("Comment", "Synthetic dose accumulation"))
        accumulated.append(self.device())
        return container("CTAccumulatedDoseData", children=accumulated)

    def build_report(self) -> FileDataset:
        options = self.options
        blueprint = TID10011CTRadiationDoseContentBuilder.blueprint
        if options.events < 1 or options.sources < 1:
            raise ValueError("events and sources must be positive")
        if options.scope == "event" and options.events != 1:
            raise ValueError("event scope requires --events 1")
        identifiers = ReportIdentifiers(self.uid(), self.uid(), self.uid())
        study_uid, series_uid, sop_uid = (
            identifiers.study,
            identifiers.series,
            identifiers.sop,
        )
        event_uids = [self.uid() for _ in range(options.events)]
        start = datetime(2025, 1, 1, 8, tzinfo=UTC) + timedelta(
            seconds=self.rng.randrange(365 * 86400),
        )
        events = []
        total = 0.0
        effective_total = 0.0
        subtotals: dict[str, float] = defaultdict(float)
        end = start
        for index, event_uid in enumerate(event_uids):
            event, dlp, effective, phantom, duration = self.event(
                index,
                event_uid,
                event_uids[index - 1] if index else None,
                end,
                total,
            )
            events.append(event)
            total = round(total + dlp, 6)
            effective_total = round(effective_total + effective, 8)
            if dlp:
                subtotals[phantom] = round(subtotals[phantom] + dlp, 6)
            end += timedelta(seconds=duration)
            if index < options.events - 1:
                end += timedelta(seconds=20)
        root: list[Dataset] = []
        language = TID1204LanguageOfContentItemAndDescendantsContentBuilder(
            self
        ).build()
        if language is not None:
            root.append(language)
        root.append(
            code_item(
                "ProcedureReported",
                Code("77477000", "SCT", "Computed Tomography X-Ray"),
                MOD,
                [
                    code_item(
                        Code("363703001", "SCT", "Has Intent"),
                        codes.cid3629.DiagnosticIntent,
                        MOD,
                    ),
                ],
            ),
        )
        root.extend(self.observers())
        root.extend(
            [
                date_time(
                    "StartOfXRayIrradiation",
                    start.strftime("%Y%m%d%H%M%S.%f"),
                    rel=OBS,
                ),
                date_time(
                    "EndOfXRayIrradiation",
                    end.strftime("%Y%m%d%H%M%S.%f"),
                    rel=OBS,
                ),
            ],
        )
        accumulation_scope = AccumulationScope.for_report(
            options.scope,
            identifiers,
            event_uids,
            self.uid,
        )
        root.append(
            code_item(
                "ScopeOfAccumulation",
                accumulation_scope.name,
                OBS,
                [
                    uid_item(
                        accumulation_scope.uid_name,
                        accumulation_scope.uid,
                        rel=PROP,
                    ),
                ],
            ),
        )
        root.append(
            TID10012CTAccumulatedDoseDataContentBuilder(self).build(
                options,
                total,
                effective_total,
                subtotals,
            )
        )
        root.extend(events)
        if self.optional():
            root.append(
                text_item("Comment", "SYNTHETIC DATA - generated software fixture"),
            )
        root.append(code_item("SourceOfDoseInformation", "AutomatedDataCollection"))
        if self.optional():
            root.append(self.person("IrradiationAuthorizing"))
        meta = FileMetaDataset()
        meta.MediaStorageSOPClassUID = XRayRadiationDoseSRStorage
        meta.MediaStorageSOPInstanceUID = UID(sop_uid)
        meta.TransferSyntaxUID = ExplicitVRLittleEndian
        meta.ImplementationClassUID = UID(
            "2.25.317396310426862753541126799083634639842",
        )
        meta.ImplementationVersionName = "SYNTH_RDSR_1"
        ds = FileDataset("", {}, file_meta=meta, preamble=b"\0" * 128)
        ds.TimezoneOffsetFromUTC = "+0000"
        ds.SOPClassUID = meta.MediaStorageSOPClassUID
        ds.SOPInstanceUID = sop_uid
        ds.StudyInstanceUID = study_uid
        ds.SeriesInstanceUID = series_uid
        ds.PatientName = (
            f"SYNTHETIC^{self.rng.choice(['Alex', 'Morgan', 'Taylor', 'Robin'])}"
        )
        ds.PatientID = f"SYNTH-{self.rng.randrange(10000000):07d}"
        ds.PatientBirthDate = f"{self.rng.randrange(1940, 2001)}0101"
        ds.PatientSex = self.rng.choice(["M", "F", "O"])
        ds.PatientIdentityRemoved = "YES"
        ds.DeidentificationMethod = "Wholly synthetic; no source patient"
        ds.StudyDate = start.strftime("%Y%m%d")
        ds.StudyTime = start.strftime("%H%M%S")
        ds.StudyID = "SYNTHETIC"
        ds.AccessionNumber = ""
        ds.ReferringPhysicianName = ""
        ds.StudyDescription = "Synthetic CT dose test"
        ds.SeriesNumber = 999
        ds.SeriesDescription = "Synthetic CT Radiation Dose SR"
        ds.Modality = "SR"
        ds.Manufacturer = "Synthetic Instruments"
        ds.ManufacturerModelName = "RDSR Fixture Generator"
        ds.DeviceSerialNumber = "SYNTH-RDSR-001"
        ds.SoftwareVersions = "1.0"
        if options.quality_format == "code":
            scheme = Dataset()
            scheme.CodingSchemeDesignator = "99SYNTH"
            scheme.CodingSchemeUID = "2.25.317396310426862753541126799083634639843"
            scheme.CodingSchemeName = "Synthetic CT fixture quality parameter values"
            scheme.CodingSchemeResponsibleOrganization = "Synthetic Imaging Lab"
            ds.CodingSchemeIdentificationSequence = [scheme]
        ds.DeviceUID = self.recorder_uid
        ds.StationName = "SYNTH_RDSR"
        ds.InstitutionName = "Synthetic Imaging Lab"
        ds.InstanceNumber = 1
        ds.ContentDate = end.strftime("%Y%m%d")
        ds.ContentTime = end.strftime("%H%M%S.%f")
        ds.CompletionFlag = "COMPLETE"
        ds.VerificationFlag = "UNVERIFIED"
        ds.PreliminaryFlag = "FINAL"
        ds.ReferencedPerformedProcedureStepSequence = DicomSequence([])
        ds.PerformedProcedureCodeSequence = DicomSequence([])
        content = sr.ContainerContentItem(
            concept(blueprint.root_concept),
            is_content_continuous=False,
            template_id=blueprint.template_identifier,
        )
        content.ContentSequence = sr.ContentSequence(
            cast("Items[sr.ContentItem]", root)
        )
        ds.update(content)
        validate(ds)
        return ds


def walk(ds: Dataset) -> Iterator[Dataset]:
    yield ds
    for child in ds.get("ContentSequence", []):
        yield from walk(child)


def children_named(ds: Dataset, name: Name) -> list[Dataset]:
    expected = concept(name)
    return [
        child
        for child in ds.get("ContentSequence", [])
        if child.ConceptNameCodeSequence[0].CodeValue == expected.value
        and child.ConceptNameCodeSequence[0].CodingSchemeDesignator
        == expected.scheme_designator
    ]


def numeric(ds: Dataset) -> Decimal:
    return Decimal(str(ds.MeasuredValueSequence[0].NumericValue))


def validate(ds: Dataset) -> None:
    """Fail on broken generated-tree invariants, also after disk round-trip."""
    if ds.SOPClassUID != XRayRadiationDoseSRStorage or ds.CompletionFlag != "COMPLETE":
        raise ValueError("Expected a complete X-Ray Radiation Dose SR")
    if ds.ContentTemplateSequence[0].TemplateIdentifier != "10011":
        raise ValueError("Expected TID 10011")
    events = children_named(ds, "CTAcquisition")
    (accumulated,) = children_named(ds, "CTAccumulatedDoseData")
    (count,) = children_named(accumulated, "TotalNumberOfIrradiationEvents")
    if numeric(count) != len(events):
        raise ValueError("Event count does not match accumulation")
    total = Decimal(0)
    uids = set()
    subtotals: dict[str, Decimal] = defaultdict(Decimal)
    for event in events:
        (uid,) = children_named(event, "IrradiationEventUID")
        if uid.UID in uids:
            raise ValueError("Duplicate event UID")
        uids.add(uid.UID)
        (parameters,) = children_named(event, "CTAcquisitionParameters")
        (sources,) = children_named(parameters, "NumberOfXRaySources")
        if numeric(sources) != len(
            children_named(parameters, "CTXRaySourceParameters"),
        ):
            raise ValueError("X-Ray source count mismatch")
        for dose in children_named(event, "CTDose"):
            (dlp,) = children_named(dose, "DLP")
            (phantom,) = children_named(dose, "CtdiwPhantomType")
            total += numeric(dlp)
            subtotals[phantom.ConceptCodeSequence[0].CodeValue] += numeric(dlp)
    (declared,) = children_named(accumulated, "CTDoseLengthProductTotal")
    if numeric(declared) != total:
        raise ValueError("Total DLP does not equal the event sum")
    for subtotal in children_named(accumulated, "CTDoseLengthProductSubTotal"):
        (phantom,) = children_named(subtotal, "CtdiwPhantomType")
        if numeric(subtotal) != subtotals[phantom.ConceptCodeSequence[0].CodeValue]:
            raise ValueError("Phantom DLP subtotal mismatch")
    for node in walk(ds):
        if node is not ds and "RelationshipType" not in node:
            raise ValueError("Missing relationship type")
        if node.ValueType == "NUM" and not numeric(node).is_finite():
            raise ValueError("Non-finite measurement")
        if node.ValueType == "CONTAINER" and not node.get("ContentSequence"):
            raise ValueError("Empty container")


def positive(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return number


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("ct_rdsr"),
        help="output directory (files are never overwritten)",
    )
    parser.add_argument("--count", type=positive, default=1)
    parser.add_argument(
        "--seed",
        type=int,
        help="reproducible fixture seed, including UIDs",
    )
    parser.add_argument("--events", type=positive, default=3)
    parser.add_argument("--optional", choices=["all", "random", "none"], default="all")
    parser.add_argument(
        "--acquisition",
        choices=[*ACQUISITIONS, "random"],
        default="spiral",
    )
    parser.add_argument(
        "--effective-method",
        choices=EFFECTIVE_METHODS,
        default="dlp-mc",
    )
    parser.add_argument(
        "--reference-authority",
        choices=["code", "text"],
        default="code",
    )
    parser.add_argument("--ssde-method", choices=["all", *SSDE_METHODS], default="all")
    parser.add_argument(
        "--water-method",
        choices=WATER_METHODS,
        default="representative",
    )
    parser.add_argument(
        "--quality-format",
        choices=["num", "text", "code"],
        default="num",
    )
    parser.add_argument(
        "--scope",
        choices=["study", "series", "pps", "pps-to-date", "event"],
        default="study",
    )
    parser.add_argument("--sources", type=positive, default=2)
    parser.add_argument(
        "--dose-checks",
        choices=["absent", "unconfigured", "normal", "exceeded"],
        default="exceeded",
    )
    args = parser.parse_args()
    options = Options(
        **{name: getattr(args, name) for name in Options.__dataclass_fields__},
    )
    if options.scope == "event" and options.events != 1:
        parser.error("--scope event requires --events 1")
    pydicom.config.settings.reading_validation_mode = pydicom.config.RAISE
    pydicom.config.settings.writing_validation_mode = pydicom.config.RAISE
    generator = Generator(args.seed, options)
    args.output.mkdir(parents=True, exist_ok=True)
    for _ in range(args.count):
        ds = generator.generate()
        destination = args.output / f"ct_rdsr_{ds.SOPInstanceUID}.dcm"
        try:
            with destination.open("xb") as stream:
                pydicom.dcmwrite(stream, ds, enforce_file_format=True)
            validate(pydicom.dcmread(destination))
        except FileExistsError:
            parser.exit(1, f"Refusing to overwrite {destination}\n")
        print(destination)


class CTRadiationDoseReport:
    """Public facade that keeps report orchestration separate from generation.

    ``CTRadiationDoseContentGenerator`` owns the RNG and pydicom construction.
    This facade is intentionally small so callers do not need to know how the
    blueprint and content-builder layers are composed.
    """

    def __init__(self, seed: int | None, options: Options) -> None:
        self.content_generator = CTRadiationDoseContentGenerator(seed, options)

    def generate(self) -> FileDataset:
        """Generate one complete CT Radiation Dose SR document."""
        return self.content_generator.generate()


# Backwards-compatible name used by the focused fixture tests and by callers
# that imported the original script before the report facade was introduced.
Generator = CTRadiationDoseReport


if __name__ == "__main__":
    main()
