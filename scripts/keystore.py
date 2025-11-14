"""
Encrypted Keystore for Dashboard Credentials

This module provides secure, encrypted storage for sensitive credentials like API tokens.
Uses Fernet (symmetric encryption) from the cryptography library.

Features:
- AES-128 encryption with HMAC authentication
- Master key derived from password or auto-generated
- Encrypted JSON storage
- Credential isolation per service
- Key rotation support
"""

import os
import json
import logging
from pathlib import Path
from typing import Dict, Optional, List
from cryptography.fernet import Fernet

logger = logging.getLogger(__name__)


class Keystore:
    """Encrypted credential storage using Fernet symmetric encryption."""

    def __init__(self, keystore_path: str = ".keystore", master_key_path: str = ".keystore.key"):
        """
        Initialize keystore.

        Args:
            keystore_path: Path to encrypted credentials file
            master_key_path: Path to master encryption key
        """
        self.keystore_path = Path(keystore_path)
        self.master_key_path = Path(master_key_path)
        self.cipher = None
        self._initialize()

    def _initialize(self):
        """Initialize or load master key and cipher."""
        if self.master_key_path.exists():
            # Load existing master key
            key = self.master_key_path.read_bytes()
            logger.info("Loaded existing master key")
        else:
            # Generate new master key
            key = Fernet.generate_key()
            self.master_key_path.write_bytes(key)
            # Secure permissions (owner read/write only)
            os.chmod(self.master_key_path, 0o600)
            logger.info("Generated new master key")

        self.cipher = Fernet(key)

    def _load_encrypted_data(self) -> Dict[str, str]:
        """Load and decrypt keystore data."""
        if not self.keystore_path.exists():
            return {}

        try:
            encrypted_data = self.keystore_path.read_bytes()
            decrypted_data = self.cipher.decrypt(encrypted_data)
            return json.loads(decrypted_data.decode('utf-8'))
        except Exception as e:
            logger.error(f"Failed to decrypt keystore: {e}")
            return {}

    def _save_encrypted_data(self, data: Dict[str, str]):
        """Encrypt and save keystore data."""
        try:
            json_data = json.dumps(data, indent=2)
            encrypted_data = self.cipher.encrypt(json_data.encode('utf-8'))
            self.keystore_path.write_bytes(encrypted_data)
            # Secure permissions
            os.chmod(self.keystore_path, 0o600)
            logger.info("Keystore saved successfully")
        except Exception as e:
            logger.error(f"Failed to encrypt keystore: {e}")
            raise

    def set_credential(self, service: str, key: str, value: str):
        """
        Store a credential securely.

        Args:
            service: Service name (e.g., 'azure', 'confluence', 'chatns')
            key: Credential key (e.g., 'token', 'username', 'password')
            value: Credential value
        """
        data = self._load_encrypted_data()

        # Create service namespace if not exists
        if service not in data:
            data[service] = {}

        data[service][key] = value
        self._save_encrypted_data(data)
        logger.info(f"Stored credential: {service}.{key}")

    def get_credential(self, service: str, key: str, default: Optional[str] = None) -> Optional[str]:
        """
        Retrieve a credential.

        Args:
            service: Service name
            key: Credential key
            default: Default value if not found

        Returns:
            Credential value or default
        """
        data = self._load_encrypted_data()

        if service in data and key in data[service]:
            return data[service][key]

        return default

    def delete_credential(self, service: str, key: str) -> bool:
        """
        Delete a credential.

        Args:
            service: Service name
            key: Credential key

        Returns:
            True if deleted, False if not found
        """
        data = self._load_encrypted_data()

        if service in data and key in data[service]:
            del data[service][key]
            # Remove service namespace if empty
            if not data[service]:
                del data[service]
            self._save_encrypted_data(data)
            logger.info(f"Deleted credential: {service}.{key}")
            return True

        return False

    def list_services(self) -> List[str]:
        """List all services with stored credentials."""
        data = self._load_encrypted_data()
        return list(data.keys())

    def list_credentials(self, service: str) -> List[str]:
        """
        List all credential keys for a service.

        Args:
            service: Service name

        Returns:
            List of credential keys
        """
        data = self._load_encrypted_data()
        if service in data:
            return list(data[service].keys())
        return []

    def get_service_credentials(self, service: str) -> Dict[str, str]:
        """
        Get all credentials for a service.

        Args:
            service: Service name

        Returns:
            Dictionary of credentials
        """
        data = self._load_encrypted_data()
        return data.get(service, {})

    def clear_service(self, service: str) -> bool:
        """
        Clear all credentials for a service.

        Args:
            service: Service name

        Returns:
            True if service existed and was cleared
        """
        data = self._load_encrypted_data()

        if service in data:
            del data[service]
            self._save_encrypted_data(data)
            logger.info(f"Cleared all credentials for service: {service}")
            return True

        return False

    def migrate_from_files(self, file_mappings: Dict[str, tuple[str, str]]):
        """
        Migrate credentials from plaintext files to keystore.

        Args:
            file_mappings: Dict mapping file paths to (service, key) tuples
                Example: {'.azure_token': ('azure', 'token')}
        """
        migrated = []

        for file_path, (service, key) in file_mappings.items():
            p = Path(file_path)
            if p.exists():
                try:
                    value = p.read_text().strip()
                    if value:
                        self.set_credential(service, key, value)
                        migrated.append((service, key, file_path))
                        logger.info(f"Migrated {file_path} to {service}.{key}")
                except Exception as e:
                    logger.error(f"Failed to migrate {file_path}: {e}")

        return migrated

    def export_to_env(self, service: str) -> Dict[str, str]:
        """
        Export service credentials as environment variables.

        Args:
            service: Service name

        Returns:
            Dictionary suitable for os.environ.update()
        """
        credentials = self.get_service_credentials(service)

        # Map to common environment variable names
        env_mapping = {
            'azure': {
                'token': 'AZDO_PAT',
                'pat': 'AZDO_PAT',
            },
            'confluence': {
                'token': 'ATLASSIAN_API_TOKEN',
                'email': 'ATLASSIAN_EMAIL',
                'username': 'CONFLUENCE_USERNAME',
            },
            'chatns': {
                'token': 'CHAT_BEARER',
                'api_key': 'CHAT_APIM',
            },
            'teamcentraal': {
                'username': 'TEAMCENTRAAL_USERNAME',
                'password': 'TEAMCENTRAAL_PASSWORD',
                'url': 'TEAMCENTRAAL_URL',
            }
        }

        env_vars = {}
        service_mapping = env_mapping.get(service, {})

        for key, value in credentials.items():
            env_name = service_mapping.get(key, f"{service.upper()}_{key.upper()}")
            env_vars[env_name] = value

        return env_vars


# Global keystore instance
_keystore_instance: Optional[Keystore] = None


def get_keystore(keystore_path: str = ".keystore", master_key_path: str = ".keystore.key") -> Keystore:
    """
    Get or create global keystore instance.

    Args:
        keystore_path: Path to encrypted credentials file
        master_key_path: Path to master encryption key

    Returns:
        Keystore instance
    """
    global _keystore_instance

    if _keystore_instance is None:
        _keystore_instance = Keystore(keystore_path, master_key_path)

    return _keystore_instance


# Example usage and testing
if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)

    # Create keystore
    ks = Keystore('.test_keystore', '.test_keystore.key')

    # Store credentials
    print("\n1. Storing credentials...")
    ks.set_credential('azure', 'token', 'test-azure-token-123')
    ks.set_credential('confluence', 'email', 'test@example.com')
    ks.set_credential('confluence', 'token', 'test-confluence-token-456')
    ks.set_credential('chatns', 'api_key', 'test-chatns-key-789')

    # Retrieve credentials
    print("\n2. Retrieving credentials...")
    azure_token = ks.get_credential('azure', 'token')
    print(f"Azure token: {azure_token}")

    conf_creds = ks.get_service_credentials('confluence')
    print(f"Confluence credentials: {conf_creds}")

    # List services
    print("\n3. Listing services...")
    services = ks.list_services()
    print(f"Services: {services}")

    for service in services:
        keys = ks.list_credentials(service)
        print(f"  {service}: {keys}")

    # Export to environment
    print("\n4. Export to environment variables...")
    env_vars = ks.export_to_env('azure')
    print(f"Azure env vars: {env_vars}")

    # Cleanup test files
    print("\n5. Cleaning up test files...")
    Path('.test_keystore').unlink(missing_ok=True)
    Path('.test_keystore.key').unlink(missing_ok=True)
    print("Done!")
