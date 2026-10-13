# Copyright 2026 the openage authors. See copying.md for legal info.

"""Regression tests for DE diplomatic technology effects."""

import struct

from openage.nyan.nyan_structs import MemberOperator
from openage.testing.testing import assert_raises, assert_value

from ....entity_object.conversion.aoc.genie_civ import GenieCivilizationGroup, GenieCivilizationObject
from ....entity_object.conversion.aoc.genie_effect import GenieEffectBundle, GenieEffectObject
from ....entity_object.conversion.aoc.genie_object_container import GenieObjectContainer
from ....entity_object.conversion.aoc.genie_tech import (
    CivBonus,
    CivTeamBonus,
    GenieTechEffectBundleGroup,
    GenieTechObject,
)
from ....entity_object.conversion.aoc.genie_unit import (
    GenieBuildingLineGroup,
    GenieUnitLineGroup,
    GenieUnitObject,
)
from ....entity_object.conversion.converter_object import ConverterObject, ConverterObjectGroup, RawAPIObject
from ....entity_object.conversion.stringresource import StringResource
from ....service.conversion import internal_name_lookups
from ....service.read.nyan_api_loader import load_api
from ....value_object.conversion.forward_ref import ForwardRef
from ....value_object.init.game_version import GameEdition, GameVersion
from ....value_object.read.media.datfile.tech import Effect
from ....value_object.read.value_members import FloatMember, IDMember, NoDiffMember
from ..aoc.pregen_processor import AoCPregenSubprocessor
from ..aoc.upgrade_ability_subprocessor import AoCUpgradeAbilitySubprocessor
from .processor import DE2Processor
from .tech_subprocessor import DE2TechSubprocessor


def diplomatic_effects() -> None:
    """Keep separate operations and recipients when a tech changes the same resource."""
    version = GameVersion(GameEdition("DE", "AOE2DE", "yes", [], [], {}, {}, []))
    dataset = GenieObjectContainer(version, load_api(), StringResource())
    pregen = ConverterObjectGroup(-1)
    AoCPregenSubprocessor.generate_diplomatic_stances(dataset, pregen)
    AoCPregenSubprocessor.generate_diplomatic_patch_properties(dataset, pregen)
    population = RawAPIObject(
        "util.resource.types.PopulationSpace", "PopulationSpace", dataset.nyan_api_objects
    )
    population.add_raw_parent("engine.util.resource.ResourceContingent")
    population.create_nyan_object()
    dataset.pregen_nyan_objects[population.get_id()] = population

    pregen.create_nyan_objects()
    pregen.create_nyan_members()

    effects = {}
    # A single bundle exercises unscoped and all diplomatic recipients without overwriting patches.
    cases = (
        (1, 0, 10.0, None, MemberOperator.ASSIGN),
        (11, 1, 2.0, "Team", MemberOperator.ADD),
        (21, 0, 20.0, "Enemy", MemberOperator.ASSIGN),
        (31, 1, 3.0, "Neutral", MemberOperator.ADD),
        (36, 0, 1.5, "Neutral", MemberOperator.MULTIPLY),
        (41, 0, 30.0, "Gaia", MemberOperator.ASSIGN),
    )
    for effect_id, (effect_type, mode, value, _, _) in enumerate(cases):
        raw = struct.pack("<Bhhhf", effect_type, 32, mode, -1, value)
        offset, members = Effect().read(raw, 0, version)
        assert_value(offset, len(raw))
        effects[effect_id] = GenieEffectObject(
            effect_id, 0, dataset, {member.name: member for member in members}
        )

    dataset.genie_effect_bundles[0] = GenieEffectBundle(0, effects, dataset)
    dataset.genie_techs[201] = GenieTechObject(
        201, dataset, {"tech_effect_id": IDMember("tech_effect_id", 0)}
    )
    group = GenieTechEffectBundleGroup(201, dataset)
    patches = DE2TechSubprocessor.get_patches(group)
    assert_value(len(patches), len(cases))
    assert_value(len({patch.raw_api_object_ref for patch in patches}), len(cases))
    group.create_nyan_objects()
    group.create_nyan_members()

    diplomatic_type = dataset.nyan_api_objects["engine.util.patch.property.type.Diplomatic"]
    patch_type = dataset.nyan_api_objects["engine.util.patch.Patch"]
    for reference, (effect_type, _, value, recipient, operator) in zip(patches, cases, strict=True):
        wrapper = reference.resolve()
        properties = wrapper.get_member_by_name("properties", patch_type).get_value()
        if recipient is None:
            assert_value(properties, None)
        else:
            expected = dataset.pregen_nyan_objects[f"util.patch.property.types.{recipient}"].get_nyan_object()
            assert_value(properties[diplomatic_type] is expected, True)

        patch = wrapper.get_member_by_name("patch", patch_type).get_value()
        amount = patch.get_member_by_name("max_amount")
        assert_value(amount.get_value(), value)
        assert_value(amount.get_operator(), operator)
        assert_value(effects[patches.index(reference)].get_type(), effect_type)

    with assert_raises(KeyError):
        Effect().read(struct.pack("<Bhhhf", 77, 32, 0, -1, 1.0), 0, version)

    # Different civ bonuses can share the same Genie effect bundle.
    dataset.genie_civs[1] = GenieCivilizationObject(1, dataset, {"tech_tree_id": IDMember("tech_tree_id", 0)})
    civ_group = GenieCivilizationGroup(1, dataset)
    dataset.civ_groups[1] = civ_group
    assert_value(internal_name_lookups.get_group_lookups(civ_group)[1][0], "Britons")
    assert_value(internal_name_lookups.get_group_lookups(group)[201][0], "Bracer")
    bonus_references = []
    for tech_id in (201, 202):
        dataset.genie_techs[tech_id] = GenieTechObject(
            tech_id, dataset, {"tech_effect_id": IDMember("tech_effect_id", 0)}
        )
        bonus_references.extend(DE2TechSubprocessor.get_patches(CivBonus(tech_id, 1, dataset)))
    assert_value(len({ref.raw_api_object_ref for ref in bonus_references}), 2 * len(cases))

    team_references = DE2TechSubprocessor.get_patches(CivTeamBonus(10000, 1, 0, dataset))
    civ_group.create_nyan_objects()
    civ_group.create_nyan_members()
    team_properties = team_references[0].resolve().get_member_by_name("properties", patch_type).get_value()
    expected_team = dataset.pregen_nyan_objects["util.patch.property.types.Team"].get_nyan_object()
    assert_value(team_properties[diplomatic_type] is expected_team, True)

    mill = GenieBuildingLineGroup(68, dataset)
    mill.add_unit(GenieUnitObject(68, dataset, {"id0": IDMember("id0", 68)}))
    selection_diff = ConverterObject(
        "selection",
        {
            "selection_sound_id": NoDiffMember("selection_sound_id", IDMember("selection_sound_id", -1)),
            "selection_shape_x": FloatMember("selection_shape_x", 1.0),
            "selection_shape_y": FloatMember("selection_shape_y", 1.0),
        },
    )
    # Buildings match their sprite and have no rectangular selection-box object to patch.
    assert_value(
        AoCUpgradeAbilitySubprocessor.selectable_ability(civ_group, mill, "Britons", selection_diff), []
    )

    external_group = ConverterObjectGroup(-2)
    parent = RawAPIObject("Parent", "Parent", dataset.nyan_api_objects)
    parent.add_raw_parent("engine.root.Object")
    external_group.add_raw_api_object(parent)
    external_reference = ForwardRef(external_group, "Parent")
    child = RawAPIObject("Child", "Child", dataset.nyan_api_objects, external_reference)
    group.add_raw_api_object(child)
    external_group.rename_raw_api_objects({"Parent": "RenamedParent"})
    assert_value(external_reference.resolve_raw() is parent, True)
    child_location = child.get_location()
    assert isinstance(child_location, ForwardRef)
    assert_value(child_location.resolve_raw() is parent, True)


def branching_upgrade_techs() -> None:
    """Retain researchable branch upgrades absent from connection-table research IDs."""
    version = GameVersion(GameEdition("DE", "AOE2DE", "yes", [], [], {}, {}, []))
    dataset = GenieObjectContainer(version, load_api(), StringResource())
    source_line = GenieUnitLineGroup(74, dataset)
    target_line = GenieUnitLineGroup(1793, dataset)
    for unit_id in (74, 75, 77, 264, 567, 1793):
        unit = GenieUnitObject(
            unit_id, dataset, {"id0": IDMember("id0", unit_id), "unit_class": IDMember("unit_class", 6)}
        )
        line = target_line if unit_id == 1793 else source_line
        line.add_unit(unit)
        dataset.genie_units[unit_id] = unit
        dataset.unit_ref[unit_id] = line

    effects = {}
    for effect_id, (source_id, effect_type) in enumerate(((74, 3), (75, 3), (77, 3), (567, 43))):
        raw = struct.pack("<Bhhhf", effect_type, source_id, 1793, -1, 0.0)
        _, members = Effect().read(raw, 0, version)
        effects[effect_id] = GenieEffectObject(
            effect_id, 896, dataset, {member.name: member for member in members}
        )
    dataset.genie_effect_bundles[896] = GenieEffectBundle(896, effects, dataset)
    dataset.genie_techs[885] = GenieTechObject(
        885,
        dataset,
        {
            "tech_effect_id": IDMember("tech_effect_id", 896),
            "research_time": IDMember("research_time", 100),
            "research_location_id": IDMember("research_location_id", 12),
        },
    )
    DE2Processor.create_researchable_tech_groups(dataset)
    assert_value(dataset.tech_groups[885].is_researchable(), True)
    tech_group = dataset.tech_groups[885]
    DE2Processor.create_researchable_tech_groups(dataset)
    assert_value(dataset.tech_groups[885] is tech_group, True)

    assert_value(DE2TechSubprocessor.get_branch_upgrade_source_position(dataset, effects[0]), None)
    assert_value(DE2TechSubprocessor.get_branch_upgrade_source_position(dataset, effects[1]), None)
    assert_value(DE2TechSubprocessor.get_branch_upgrade_source_position(dataset, effects[2]), 2)
    assert_value(DE2TechSubprocessor.get_branch_upgrade_source_position(dataset, effects[3]), 4)
