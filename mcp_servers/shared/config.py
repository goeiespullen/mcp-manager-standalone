"""Shared configuration for MCP servers."""
from __future__ import annotations

import os
from pathlib import Path
from dataclasses import dataclass
from typing import Optional


@dataclass
class MCPServerConfig:
    """Configuration for MCP servers."""

    # Azure DevOps
    azdo_org: str = "ns-topaas"
    azdo_pat: Optional[str] = None

    # Confluence
    confluence_base_url: str = "https://ns-topaas.atlassian.net/wiki"
    confluence_email: Optional[str] = None
    confluence_api_token: Optional[str] = None

    # ChatNS
    chatns_api_url: str = "https://gateway.apiportal.ns.nl/genai/v1/chat/completions"
    chatns_bearer: Optional[str] = None
    chatns_apim: Optional[str] = None

    # Data directories
    data_dir: Path = Path("data")

    @classmethod
    def from_env(cls) -> 'MCPServerConfig':
        """Load configuration from environment variables."""
        return cls(
            azdo_pat=cls._get_azdo_token(),
            confluence_email=os.environ.get("ATLASSIAN_EMAIL"),
            confluence_api_token=os.environ.get("ATLASSIAN_API_TOKEN"),
            chatns_bearer=os.environ.get("CHATNS_API_KEY") or os.environ.get("CHAT_BEARER"),
            chatns_apim=os.environ.get("CHAT_APIM") or os.environ.get("OCP_APIM_SUBSCRIPTION_KEY"),
        )

    @staticmethod
    def _get_azdo_token() -> Optional[str]:
        """Get Azure DevOps token from various sources."""
        # Check local file first
        p_local = Path(".azure_token")
        if p_local.exists():
            return p_local.read_text().strip() or None

        # Check environment variable
        env = os.environ.get("AZDO_PAT", "").strip()
        if env:
            return env

        # Check home directory
        p_home = Path.home() / ".azdo_pat"
        if p_home.exists():
            return p_home.read_text().strip() or None

        return None

    def is_devops_configured(self) -> bool:
        """Check if Azure DevOps is properly configured."""
        return bool(self.azdo_pat)

    def is_confluence_configured(self) -> bool:
        """Check if Confluence is properly configured."""
        return bool(self.confluence_email and self.confluence_api_token)

    def is_chatns_configured(self) -> bool:
        """Check if ChatNS is properly configured."""
        return bool(self.chatns_apim)  # APIM is required, bearer is optional