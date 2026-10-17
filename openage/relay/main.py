# Copyright 2026 the openage authors. See copying.md for legal info.

"""Command-line entry point for an FVD one-match relay service."""

from __future__ import annotations

import typing

from ..log import info

if typing.TYPE_CHECKING:
    from argparse import ArgumentParser, Namespace
    from collections.abc import Callable


def init_subparser(cli: ArgumentParser) -> None:
    """Initialize the relay service subparser."""
    cli.set_defaults(entrypoint=main)
    cli.add_argument(
        "--listen",
        default="0.0.0.0",
        help="IPv4, IPv6 or DNS address on which the relay accepts connections",
    )
    cli.add_argument(
        "--port",
        type=int,
        default=32456,
        help="TCP port on which the relay accepts connections",
    )
    cli.add_argument(
        "--pair",
        action="append",
        nargs=4,
        required=True,
        metavar=("MATCH_ID", "PLAYER_ID", "HOST_TOKEN", "GUEST_TOKEN"),
        help="provision one guest pair with two 64-character hexadecimal tokens",
    )


def _parse_pair(values: list[str], error: Callable[[str], typing.NoReturn]) -> tuple[int, int, bytes, bytes]:
    """Parse and validate one command-line relay pair."""
    try:
        match_id = int(values[0])
        player_id = int(values[1])
    except ValueError:
        error("relay match and player IDs must be integers")
    if match_id <= 0 or match_id > 0xFFFFFFFFFFFFFFFF:
        error("relay match IDs must be nonzero unsigned 64-bit integers")
    if player_id <= 0 or player_id > 0xFFFFFFFFFFFFFFFF:
        error("relay player IDs must be nonzero unsigned 64-bit integers")

    try:
        host_token = bytes.fromhex(values[2])
        guest_token = bytes.fromhex(values[3])
    except ValueError:
        error("relay tokens must be hexadecimal")
    if len(host_token) != 32 or len(guest_token) != 32:
        error("relay tokens must contain exactly 32 bytes")
    if not any(host_token) or not any(guest_token):
        error("relay tokens must not be all zero")
    if host_token == guest_token:
        error("relay host and guest tokens must be distinct")
    return match_id, player_id, host_token, guest_token


def main(args: Namespace, error: Callable[[str], typing.NoReturn]) -> int:
    """Run a relay for one set of lobby-provisioned guest pairs."""
    if args.port <= 0 or args.port > 65535:
        error("relay port must be between 1 and 65535")

    pairs = [_parse_pair(values, error) for values in args.pair]

    from ..cppinterface.setup import setup as cpp_interface_setup
    from .main_cpp import run_relay

    cpp_interface_setup(args)
    info("starting FVD relay for %d guest pair(s)", len(pairs))
    return run_relay(args.listen, args.port, pairs)
