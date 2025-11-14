#!/usr/bin/env python3
"""
Register token for user in encrypted keystore.

Interactive CLI tool for registering user credentials in the multi-user tokenstore.
Each user has their own encrypted keystore file.

Usage:
    python3 register_token.py
    python3 register_token.py --user user@ns.nl --system azure --token ghp_abc123
    python3 register_token.py --list user@ns.nl

Features:
- Interactive mode (no arguments) prompts for all inputs
- Non-interactive mode (with arguments) for scripting
- List mode to view stored credentials (keys only, not values)
- Encrypted storage with Fernet (AES-128)
- Per-user keystores in keystores/ directory

Examples:
    # Interactive mode
    $ python3 register_token.py
    User email: maarten.ahlrichs@ns.nl
    System (azure/confluence/chatns/teamcentraal): azure
    Token/PAT: [hidden input]
    ✅ Token registered!

    # Non-interactive mode
    $ python3 register_token.py --user user@ns.nl --system azure --token ghp_abc123

    # List credentials
    $ python3 register_token.py --list user@ns.nl
    Credentials for user@ns.nl:
      azure: pat, token
      confluence: token, email
"""
import sys
import getpass
import argparse
from pathlib import Path
from typing import Optional

# Import keystore from same directory
from keystore import Keystore


def get_keystores_dir() -> Path:
    """Get keystores directory path."""
    script_dir = Path(__file__).parent
    keystores_dir = script_dir.parent / "keystores"
    keystores_dir.mkdir(exist_ok=True, mode=0o700)  # Owner only
    return keystores_dir


def get_user_keystore(user_id: str) -> Keystore:
    """Get or create keystore for user."""
    keystores_dir = get_keystores_dir()
    keystore_path = keystores_dir / f"{user_id}.keystore"
    keystore_key_path = keystores_dir / f"{user_id}.key"

    return Keystore(
        keystore_path=str(keystore_path),
        master_key_path=str(keystore_key_path)
    )


def register_token_interactive():
    """Interactive token registration."""
    print("=" * 70)
    print("🔐 MCP Token Registration")
    print("=" * 70)
    print()

    # Get user email
    while True:
        user_id = input("User email: ").strip()
        if not user_id:
            print("❌ User email is required")
            continue
        if "@" not in user_id:
            print("⚠️  Warning: doesn't look like an email address")
            confirm = input("Continue anyway? [y/N]: ").strip().lower()
            if confirm not in ('y', 'yes'):
                continue
        break

    # Get system name
    valid_systems = ["azure", "confluence", "chatns", "teamcentraal"]
    print(f"\nSupported systems: {', '.join(valid_systems)}")
    while True:
        system = input("System: ").strip().lower()
        if not system:
            print("❌ System is required")
            continue
        if system not in valid_systems:
            print(f"⚠️  Warning: '{system}' is not a standard system")
            confirm = input("Continue anyway? [y/N]: ").strip().lower()
            if confirm not in ('y', 'yes'):
                continue
        break

    # Get token (hidden input)
    while True:
        token = getpass.getpass("\nToken/PAT (hidden): ").strip()
        if not token:
            print("❌ Token is required")
            continue
        # Confirm token
        token_confirm = getpass.getpass("Confirm token: ").strip()
        if token != token_confirm:
            print("❌ Tokens don't match, try again")
            continue
        break

    # Store token
    try:
        keystore = get_user_keystore(user_id)

        # Determine key name based on system
        if system == "azure":
            key_name = "pat"  # Azure uses 'pat' as primary key
        else:
            key_name = "token"  # Others use 'token'

        keystore.set_credential(system, key_name, token)

        print()
        print("✅ Token registered successfully!")
        print(f"   User:    {user_id}")
        print(f"   System:  {system}")
        print(f"   Key:     {key_name}")
        print(f"   Keystore: {get_keystores_dir() / f'{user_id}.keystore'}")
        print()

        # Show all stored credentials (keys only)
        print("📋 All stored credentials for this user:")
        for service in keystore.list_services():
            keys = keystore.list_credentials(service)
            print(f"   {service}: {', '.join(keys)}")

        return 0

    except Exception as e:
        print(f"\n❌ Failed to store token: {e}")
        import traceback
        traceback.print_exc()
        return 1


def register_token_noninteractive(user_id: str, system: str, token: str) -> int:
    """Non-interactive token registration."""
    try:
        keystore = get_user_keystore(user_id)

        # Determine key name based on system
        if system == "azure":
            key_name = "pat"
        else:
            key_name = "token"

        keystore.set_credential(system, key_name, token)

        print(f"✅ Token registered: {user_id} / {system} / {key_name}")
        return 0

    except Exception as e:
        print(f"❌ Failed to store token: {e}", file=sys.stderr)
        return 1


def list_credentials(user_id: str) -> int:
    """List stored credentials for user (keys only, not values)."""
    keystores_dir = get_keystores_dir()
    keystore_path = keystores_dir / f"{user_id}.keystore"

    if not keystore_path.exists():
        print(f"❌ No keystore found for user: {user_id}", file=sys.stderr)
        print(f"   Expected location: {keystore_path}", file=sys.stderr)
        return 1

    try:
        keystore = get_user_keystore(user_id)

        print(f"📋 Credentials for {user_id}:")
        print(f"   Keystore: {keystore_path}")
        print()

        services = keystore.list_services()
        if not services:
            print("   (no credentials stored)")
            return 0

        for service in services:
            keys = keystore.list_credentials(service)
            print(f"   {service}:")
            for key in keys:
                value = keystore.get_credential(service, key)
                # Show first/last 4 chars only for security
                if value and len(value) > 8:
                    masked = f"{value[:4]}...{value[-4:]}"
                else:
                    masked = "****"
                print(f"      {key}: {masked}")

        return 0

    except Exception as e:
        print(f"❌ Failed to read keystore: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


def delete_credential(user_id: str, system: str, key: Optional[str] = None) -> int:
    """Delete credential(s) for user."""
    keystores_dir = get_keystores_dir()
    keystore_path = keystores_dir / f"{user_id}.keystore"

    if not keystore_path.exists():
        print(f"❌ No keystore found for user: {user_id}", file=sys.stderr)
        return 1

    try:
        keystore = get_user_keystore(user_id)

        if key:
            # Delete specific key
            if keystore.delete_credential(system, key):
                print(f"✅ Deleted: {user_id} / {system} / {key}")
                return 0
            else:
                print(f"❌ Not found: {user_id} / {system} / {key}", file=sys.stderr)
                return 1
        else:
            # Delete entire service
            if keystore.clear_service(system):
                print(f"✅ Deleted all credentials for: {user_id} / {system}")
                return 0
            else:
                print(f"❌ Service not found: {user_id} / {system}", file=sys.stderr)
                return 1

    except Exception as e:
        print(f"❌ Failed to delete: {e}", file=sys.stderr)
        return 1


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Register tokens in multi-user encrypted keystore",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode
  python3 register_token.py

  # Non-interactive registration
  python3 register_token.py --user user@ns.nl --system azure --token ghp_abc123

  # List credentials
  python3 register_token.py --list user@ns.nl

  # Delete credential
  python3 register_token.py --delete user@ns.nl --system azure --key pat
  python3 register_token.py --delete user@ns.nl --system azure  # Delete all azure creds
        """
    )

    parser.add_argument("--user", help="User email address")
    parser.add_argument("--system", help="System name (azure, confluence, chatns, etc.)")
    parser.add_argument("--token", help="Token/PAT value")
    parser.add_argument("--list", metavar="USER", help="List credentials for user")
    parser.add_argument("--delete", metavar="USER", help="Delete credential(s) for user")
    parser.add_argument("--key", help="Specific key to delete (use with --delete)")

    args = parser.parse_args()

    # List mode
    if args.list:
        return list_credentials(args.list)

    # Delete mode
    if args.delete:
        if not args.system:
            print("❌ --system is required with --delete", file=sys.stderr)
            return 1
        return delete_credential(args.delete, args.system, args.key)

    # Non-interactive mode
    if args.user and args.system and args.token:
        return register_token_noninteractive(args.user, args.system, args.token)

    # Partial arguments - show error
    if args.user or args.system or args.token:
        print("❌ For non-interactive mode, provide --user, --system, AND --token", file=sys.stderr)
        print("   Or run without arguments for interactive mode", file=sys.stderr)
        return 1

    # Interactive mode (default)
    return register_token_interactive()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\n\n❌ Cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)
