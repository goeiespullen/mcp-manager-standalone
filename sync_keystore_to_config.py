#!/usr/bin/env python3
"""
Sync credentials from keystore to MCP server configs.

This script reads credentials from the chatns_summerschool keystore
and updates the configs/servers.json file with the decrypted values.
"""

import sys
import json
from pathlib import Path

# Add chatns_summerschool to path to import keystore
sys.path.insert(0, str(Path(__file__).parent.parent / "chatns_summerschool"))

from dashapp.keystore import get_keystore


def sync_credentials():
    """
    Export keystore credentials to environment variables.

    SECURITY NOTE: This script NO LONGER writes credentials to servers.json.
    Credentials are exported to environment variables only, which are inherited
    by MCP Manager and passed to child processes.
    """

    # Get keystore from chatns_summerschool directory
    keystore_path = Path(__file__).parent.parent / "chatns_summerschool" / ".keystore"
    keystore_key_path = Path(__file__).parent.parent / "chatns_summerschool" / ".keystore.key"

    if not keystore_path.exists():
        print(f"❌ Keystore not found: {keystore_path}")
        return False

    print(f"📖 Loading keystore from {keystore_path}")
    ks = get_keystore(str(keystore_path), str(keystore_key_path))

    # Verify servers.json exists (but we don't modify it)
    config_path = Path(__file__).parent / "configs" / "servers.json"
    if not config_path.exists():
        print(f"❌ Config not found: {config_path}")
        return False

    print(f"📖 Loading config from {config_path}")

    # Mapping of server names to keystore services
    service_mappings = {
        'Azure DevOps': 'azure',
        'Confluence': 'confluence',
        'TeamCentraal': 'teamcentraal',
        'ChatNS': 'chatns',
    }

    # Export credentials to environment variables (inherited by child processes)
    import os
    exported = False

    for service_name, service_key in service_mappings.items():
        # Get environment variable mapping
        env_vars = ks.export_to_env(service_key)

        if not env_vars:
            print(f"ℹ️  No env vars to export for {service_name}")
            continue

        for env_key, env_value in env_vars.items():
            # Skip if already set (don't override existing env vars)
            if os.environ.get(env_key):
                print(f"ℹ️  Skipped {service_name}: {env_key} (already set)")
            else:
                os.environ[env_key] = env_value
                print(f"✅ Exported {service_name}: {env_key}")
                exported = True

    if not exported:
        print("\nℹ️  No updates needed")
        return False
    else:
        print("\n✅ Credentials exported to environment variables")
        print("   MCP servers will inherit these credentials at runtime")
        return True


if __name__ == '__main__':
    success = sync_credentials()
    sys.exit(0 if success else 1)
