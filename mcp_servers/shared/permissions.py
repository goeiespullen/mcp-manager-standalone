"""
Tool Permission Categories voor MCP Servers.

Definieert de verschillende permission levels voor MCP tools.
"""

from enum import Enum
from typing import List, Set


class PermissionCategory(str, Enum):
    """Permission categorieën voor MCP tools."""

    READ_REMOTE = "READ_REMOTE"      # Leest data van remote APIs
    WRITE_REMOTE = "WRITE_REMOTE"    # Schrijft naar remote systemen
    WRITE_LOCAL = "WRITE_LOCAL"      # Schrijft naar lokale filesystem
    EXECUTE_AI = "EXECUTE_AI"        # Voert AI model calls uit (kost tokens)
    EXECUTE_CODE = "EXECUTE_CODE"    # Voert code/scripts uit


class PermissionDefaults:
    """Default permission settings (secure by default)."""

    GLOBAL_DEFAULTS = {
        PermissionCategory.READ_REMOTE: True,    # Veilig, standaard aan
        PermissionCategory.WRITE_REMOTE: False,  # Gevaarlijk, standaard uit
        PermissionCategory.WRITE_LOCAL: False,   # Disk usage, standaard uit
        PermissionCategory.EXECUTE_AI: False,    # Kost geld, standaard uit
        PermissionCategory.EXECUTE_CODE: False,  # Security risk, standaard uit
    }


def categorize_tool(tool_name: str, server_type: str) -> Set[PermissionCategory]:
    """
    Auto-categoriseer een tool op basis van naam en server type.

    Returns:
        Set van PermissionCategory's die deze tool nodig heeft
    """
    categories = set()

    # Lowercase voor matching
    name_lower = tool_name.lower()

    # READ_REMOTE detectie
    read_keywords = ['list', 'get', 'search', 'health', 'find', 'show', 'view']
    if any(keyword in name_lower for keyword in read_keywords):
        categories.add(PermissionCategory.READ_REMOTE)

    # WRITE_REMOTE detectie
    write_remote_keywords = ['create', 'update', 'delete', 'modify', 'set', 'post', 'put', 'patch']
    if any(keyword in name_lower for keyword in write_remote_keywords):
        categories.add(PermissionCategory.WRITE_REMOTE)

    # WRITE_LOCAL detectie
    write_local_keywords = ['dump', 'export', 'save', 'download', 'build', 'index']
    if any(keyword in name_lower for keyword in write_local_keywords):
        categories.add(PermissionCategory.WRITE_LOCAL)

    # EXECUTE_AI detectie
    ai_keywords = ['chat', 'completion', 'generate', 'ai', 'gpt', 'model']
    if any(keyword in name_lower for keyword in ai_keywords):
        categories.add(PermissionCategory.EXECUTE_AI)

    # EXECUTE_CODE detectie
    code_keywords = ['refresh', 'run', 'execute', 'script', 'subprocess']
    if any(keyword in name_lower for keyword in code_keywords):
        categories.add(PermissionCategory.EXECUTE_CODE)

    # Fallback: als niets matcht, is het READ_REMOTE
    if not categories:
        categories.add(PermissionCategory.READ_REMOTE)

    return categories


def get_tool_permission_metadata(tool_name: str, server_type: str,
                                  manual_categories: List[str] = None) -> dict:
    """
    Genereer permission metadata voor een tool.

    Args:
        tool_name: Naam van de tool
        server_type: Type server (teamcentraal, confluence, etc)
        manual_categories: Optioneel handmatig opgegeven categorieën

    Returns:
        Dictionary met permission metadata
    """
    if manual_categories:
        categories = {PermissionCategory(cat) for cat in manual_categories}
    else:
        categories = categorize_tool(tool_name, server_type)

    return {
        "tool_name": tool_name,
        "server_type": server_type,
        "categories": [cat.value for cat in categories],
        "requires_approval": any(cat in categories for cat in [
            PermissionCategory.WRITE_REMOTE,
            PermissionCategory.WRITE_LOCAL,
            PermissionCategory.EXECUTE_AI,
            PermissionCategory.EXECUTE_CODE
        ])
    }
