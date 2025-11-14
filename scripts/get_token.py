#!/usr/bin/env python3
"""
Get token for user from encrypted keystore.

This script is called by MCP Gateway (C++) to retrieve user credentials.
Outputs token to stdout for capture by calling process.

Usage:
    python3 get_token.py <user_id> <system>

Arguments:
    user_id: User email address (e.g., user@ns.nl)
    system: System name (azure, confluence, chatns, teamcentraal)

Exit codes:
    0: Success - token found and printed to stdout
    1: Error - no keystore, no token, or other error

Example:
    $ python3 get_token.py user@ns.nl azure
    ghp_abc123xyz...
"""
import sys
import logging
from pathlib import Path

# Import keystore from same directory
from keystore import Keystore

# Disable logging to avoid polluting stdout (only token should go to stdout)
logging.basicConfig(level=logging.CRITICAL)


def get_token(user_id: str, system: str) -> str:
    """
    Get token for user from encrypted keystore.

    Priority order:
    1. Shared dashboard keystore (for backwards compatibility)
    2. Per-user keystore in keystores/

    Args:
        user_id: User email address
        system: System name (azure, confluence, chatns, etc.)

    Returns:
        Token string

    Raises:
        SystemExit: On any error (exit code 1)
    """
    script_dir = Path(__file__).parent

    # Option 1: Try shared dashboard keystore first (PRIORITY)
    dashboard_keystore = script_dir.parent.parent / "chatns_summerschool" / "dashapp" / ".keystore"
    dashboard_key = script_dir.parent.parent / "chatns_summerschool" / "dashapp" / ".keystore.key"

    # Option 2: Per-user keystore
    keystores_dir = script_dir.parent / "keystores"
    user_keystore = keystores_dir / f"{user_id}.keystore"
    user_key = keystores_dir / f"{user_id}.key"

    # Try shared keystore first (backwards compatible)
    if dashboard_keystore.exists() and dashboard_key.exists():
        keystore_path = dashboard_keystore
        keystore_key_path = dashboard_key
    # Fallback to per-user keystore
    elif user_keystore.exists():
        keystore_path = user_keystore
        keystore_key_path = user_key
    else:
        print(f"ERROR: No keystore found for user: {user_id}", file=sys.stderr)
        print(f"Tried locations:", file=sys.stderr)
        print(f"  1. Shared: {dashboard_keystore}", file=sys.stderr)
        print(f"  2. Per-user: {user_keystore}", file=sys.stderr)
        print(f"Hint: Credentials should be in the dashboard keystore", file=sys.stderr)
        sys.exit(1)

    # Load keystore
    try:
        keystore = Keystore(
            keystore_path=str(keystore_path),
            master_key_path=str(keystore_key_path)
        )
    except Exception as e:
        print(f"ERROR: Failed to load keystore: {e}", file=sys.stderr)
        sys.exit(1)

    # Try different key names depending on system
    token = None

    if system == "azure":
        # Azure DevOps uses 'pat' as primary key
        token = keystore.get_credential("azure", "pat")
        if not token:
            token = keystore.get_credential("azure", "token")

    elif system == "confluence":
        token = keystore.get_credential("confluence", "token")

    elif system == "chatns":
        # ChatNS might use 'token' or 'api_key'
        token = keystore.get_credential("chatns", "token")
        if not token:
            token = keystore.get_credential("chatns", "api_key")

    elif system == "teamcentraal":
        # TeamCentraal uses password
        token = keystore.get_credential("teamcentraal", "password")

    else:
        # Generic fallback - try 'token' key
        token = keystore.get_credential(system, "token")
        if not token:
            token = keystore.get_credential(system, "pat")

    if not token:
        print(f"ERROR: No token found for system '{system}' in user keystore", file=sys.stderr)
        print(f"Available services: {keystore.list_services()}", file=sys.stderr)
        sys.exit(1)

    # Success - output token to stdout (ONLY token, nothing else!)
    print(token)
    sys.exit(0)


if __name__ == "__main__":
    # Parse arguments
    if len(sys.argv) != 3:
        print("Usage: get_token.py <user_id> <system>", file=sys.stderr)
        print("", file=sys.stderr)
        print("Arguments:", file=sys.stderr)
        print("  user_id: User email address (e.g., user@ns.nl)", file=sys.stderr)
        print("  system:  System name (azure, confluence, chatns, teamcentraal)", file=sys.stderr)
        print("", file=sys.stderr)
        print("Example:", file=sys.stderr)
        print("  python3 get_token.py user@ns.nl azure", file=sys.stderr)
        sys.exit(1)

    user_id = sys.argv[1]
    system = sys.argv[2]

    # Validate user_id format (simple check)
    if "@" not in user_id:
        print(f"WARNING: user_id '{user_id}' doesn't look like an email", file=sys.stderr)

    # Get and output token
    get_token(user_id, system)
