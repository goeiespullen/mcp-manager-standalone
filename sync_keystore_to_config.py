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
    """Sync keystore credentials to servers.json."""

    # Get keystore from chatns_summerschool directory
    keystore_path = Path(__file__).parent.parent / "chatns_summerschool" / ".keystore"
    keystore_key_path = Path(__file__).parent.parent / "chatns_summerschool" / ".keystore.key"

    if not keystore_path.exists():
        print(f"❌ Keystore not found: {keystore_path}")
        return False

    print(f"📖 Loading keystore from {keystore_path}")
    ks = get_keystore(str(keystore_path), str(keystore_key_path))

    # Load servers.json
    config_path = Path(__file__).parent / "configs" / "servers.json"
    if not config_path.exists():
        print(f"❌ Config not found: {config_path}")
        return False

    print(f"📖 Loading config from {config_path}")
    with open(config_path, 'r') as f:
        config = json.load(f)

    # Mapping of server names to keystore services
    service_mappings = {
        'Azure DevOps': 'azure',
        'Confluence': 'confluence',
        'TeamCentraal': 'teamcentraal',
        'ChatNS': 'chatns',
    }

    updated = False

    # Update each server's env with keystore credentials
    for server in config.get('servers', []):
        server_name = server.get('name')

        if server_name not in service_mappings:
            continue

        service = service_mappings[server_name]

        # Get credentials from keystore
        creds = ks.get_service_credentials(service)

        if not creds:
            print(f"ℹ️  No credentials in keystore for {server_name}")
            continue

        # Get environment variable mapping
        env_vars = ks.export_to_env(service)

        if not env_vars:
            print(f"ℹ️  No env vars to export for {server_name}")
            continue

        # Update server env
        if 'env' not in server:
            server['env'] = {}

        for env_key, env_value in env_vars.items():
            # Only update if the current value is empty or placeholder
            current_value = server['env'].get(env_key, '')
            if not current_value or current_value == '':
                server['env'][env_key] = env_value
                print(f"✅ Updated {server_name}: {env_key}")
                updated = True
            else:
                print(f"ℹ️  Skipped {server_name}: {env_key} (already set)")

    if updated:
        # Save updated config
        print(f"\n💾 Saving updated config to {config_path}")
        with open(config_path, 'w') as f:
            json.dump(config, f, indent=4)
        print("✅ Config updated successfully!")
        return True
    else:
        print("\nℹ️  No updates needed")
        return False


if __name__ == '__main__':
    success = sync_credentials()
    sys.exit(0 if success else 1)
