# Copyright 2026 the openage authors. See copying.md for legal info.

"""Regression tests for object-reference members in generated nyan data."""

from openage.testing.testing import assert_value

from .nyan_structs import MemberOperator, MemberType, NyanMember, NyanMemberType, NyanObject


def object_reference_members() -> None:
    """Object references retain their identity and serialize inside collections."""
    target = NyanObject("Target")
    target.set_fqon("test.Target")
    reference_type = NyanMemberType(target)
    optional_type = NyanMemberType(MemberType.OPTIONAL, [reference_type])

    for member_type in (reference_type, optional_type):
        member = NyanMember("target", member_type, target, MemberOperator.ASSIGN)
        assert_value(member.get_value() is target, True)
        assert_value(str(member), "test.Target")

    collection_type = NyanMemberType(MemberType.SET, [reference_type])
    collection = NyanMember("targets", collection_type, [target], MemberOperator.ASSIGN)
    assert_value(list(collection.get_value()), [target])
    assert_value("test.Target" in str(collection), True)

    dictionary_type = NyanMemberType(MemberType.DICT, [reference_type, reference_type])
    dictionary = NyanMember("targets", dictionary_type, {target: target}, MemberOperator.ASSIGN)
    assert_value(dictionary.get_value(), {target: target})
    assert_value("test.Target: test.Target" in str(dictionary), True)
