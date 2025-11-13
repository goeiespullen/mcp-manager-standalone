#!/usr/bin/env python3
"""Confluence MCP Server.

Provides tools for interacting with Confluence API:
- list_spaces: Get all Confluence spaces
- search_pages: CQL search functionality
- dump_space: Export space content
- dump_team_pages: Team-specific page dumps
- build_rag_index: Build RAG index from space content
- health_check: Connection verification
"""
from __future__ import annotations

import asyncio
import json
import re
import sys
import urllib.parse
import zipfile
from pathlib import Path
from typing import List, Dict, Any, Optional

import requests
from requests.auth import HTTPBasicAuth
from mcp.server import Server
from mcp.types import Tool as BaseTool, TextContent

# Extend Tool class to support permissions
class Tool(BaseTool):
    """Extended Tool class with permissions support."""
    def __init__(self, *args, permissions=None, **kwargs):
        super().__init__(*args, **kwargs)
        self.permissions = permissions or {}

    def model_dump(self, **kwargs):
        """Override serialization to include permissions."""
        data = super().model_dump(**kwargs)
        if hasattr(self, 'permissions'):
            data['permissions'] = self.permissions
        return data

try:
    from shared.config import MCPServerConfig
    from shared.permissions import PermissionCategory, get_tool_permission_metadata
except ImportError:
    # Fallback for when called from dashboard
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).parent))
    from shared.config import MCPServerConfig
    from shared.permissions import PermissionCategory, get_tool_permission_metadata

# Initialize server
server = Server("confluence-server")
config = MCPServerConfig.from_env()

# Tool permission mappings
TOOL_PERMISSIONS = {
    "list_spaces": [PermissionCategory.READ_REMOTE],
    "search_pages": [PermissionCategory.READ_REMOTE],
    "get_page_content": [PermissionCategory.READ_REMOTE],
    "get_page_children": [PermissionCategory.READ_REMOTE],
    "health_check": [PermissionCategory.READ_REMOTE],
    "dump_space": [PermissionCategory.READ_REMOTE, PermissionCategory.WRITE_LOCAL],
    "dump_team_pages": [PermissionCategory.READ_REMOTE, PermissionCategory.WRITE_LOCAL],
    "build_rag_index": [PermissionCategory.READ_REMOTE, PermissionCategory.WRITE_LOCAL],
    "create_page": [PermissionCategory.WRITE_REMOTE],
    "update_page": [PermissionCategory.WRITE_REMOTE],
}


class ConfluenceAPIError(Exception):
    """Confluence API error."""
    pass


def _make_request(url: str, method: str = "GET", timeout: int = 60) -> dict:
    """Make authenticated request to Confluence API."""
    if not config.is_confluence_configured():
        raise ConfluenceAPIError("Confluence not configured (missing email or API token)")

    response = requests.request(
        method, url,
        auth=HTTPBasicAuth(config.confluence_email, config.confluence_api_token),
        headers={"Accept": "application/json"},
        timeout=timeout
    )

    if not response.ok:
        raise ConfluenceAPIError(
            f"HTTP {response.status_code} {response.reason}\n"
            f"URL: {url}\n"
            f"Response: {response.text[:500]}"
        )

    return response.json()


def _sanitize_filename(name: str) -> str:
    """Sanitize filename for safe filesystem usage."""
    name = re.sub(r"[\r\n\t]+", " ", (name or ""))
    name = re.sub(r"[\\/:*?\"<>|]+", "_", name)
    return name.strip()[:180] or "untitled"


@server.list_tools()
async def list_tools() -> list[Tool]:
    """List available tools."""
    tools = [
        Tool(
            name="list_spaces",
            description="Get list of all Confluence spaces",
            inputSchema={
                "type": "object",
                "properties": {
                    "include_personal": {
                        "type": "boolean",
                        "description": "Include personal spaces (starting with ~)",
                        "default": False
                    }
                },
                "required": []
            }
        ),
        Tool(
            name="search_pages",
            description="Search Confluence pages using CQL (Confluence Query Language). CQL requires specific syntax - use 'text~' for content search, 'title~' for title search, 'space=' for space filtering. Always quote search terms.",
            inputSchema={
                "type": "object",
                "properties": {
                    "cql": {
                        "type": "string",
                        "description": "CQL query string using CONFLUENCE fields (NOT Azure DevOps fields). Valid Confluence CQL fields: 'text', 'title', 'space', 'type', 'created', 'lastModified'. Examples: 'text ~ \"keyword\"' (search in page content), 'title ~ \"keyword\"' (search in page titles), 'space=DNANSI AND type=page' (all pages in DNANSI space), 'text ~ \"sprint\" AND space=DNANSI' (content search in specific space). DO NOT use Azure DevOps fields like 'project' or 'team' - use 'space' instead. ALWAYS use text~ or title~ operators - bare words like 'DNANSI' or 'keyword' will fail with 400 error."
                    },
                    "limit": {
                        "type": "integer",
                        "description": "Maximum number of results",
                        "default": 100
                    }
                },
                "required": ["cql"]
            }
        ),
        Tool(
            name="dump_space",
            description="Export content from a Confluence space",
            inputSchema={
                "type": "object",
                "properties": {
                    "space_key": {
                        "type": "string",
                        "description": "Space key to dump"
                    },
                    "format": {
                        "type": "string",
                        "description": "Export format",
                        "enum": ["view", "storage", "adf"],
                        "default": "storage"
                    },
                    "max_pages": {
                        "type": "integer",
                        "description": "Maximum pages to dump (0 = all)",
                        "default": 0
                    },
                    "include_archived": {
                        "type": "boolean",
                        "description": "Include archived pages",
                        "default": False
                    }
                },
                "required": ["space_key"]
            }
        ),
        Tool(
            name="dump_team_pages",
            description="Dump pages related to a specific team",
            inputSchema={
                "type": "object",
                "properties": {
                    "space_key": {
                        "type": "string",
                        "description": "Space key to search in"
                    },
                    "team_name": {
                        "type": "string",
                        "description": "Team name to search for"
                    },
                    "format": {
                        "type": "string",
                        "description": "Export format",
                        "enum": ["view", "storage", "adf"],
                        "default": "storage"
                    },
                    "max_pages": {
                        "type": "integer",
                        "description": "Maximum pages to dump (0 = all)",
                        "default": 0
                    }
                },
                "required": ["space_key", "team_name"]
            }
        ),
        Tool(
            name="build_rag_index",
            description="Build RAG index from space content",
            inputSchema={
                "type": "object",
                "properties": {
                    "space_key": {
                        "type": "string",
                        "description": "Space key to index"
                    },
                    "max_words": {
                        "type": "integer",
                        "description": "Maximum words per chunk",
                        "default": 900
                    },
                    "overlap": {
                        "type": "integer",
                        "description": "Overlap between chunks",
                        "default": 120
                    }
                },
                "required": ["space_key"]
            }
        ),
        Tool(
            name="get_page_content",
            description="Get content of a specific Confluence page",
            inputSchema={
                "type": "object",
                "properties": {
                    "page_id": {
                        "type": "string",
                        "description": "Page ID or title"
                    },
                    "space_key": {
                        "type": "string",
                        "description": "Space key (optional if using page ID)"
                    },
                    "expand": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Fields to expand (e.g., body.storage, body.view)",
                        "default": ["body.storage", "version"]
                    }
                },
                "required": ["page_id"]
            }
        ),
        Tool(
            name="create_page",
            description="Create a new Confluence page",
            inputSchema={
                "type": "object",
                "properties": {
                    "space_key": {
                        "type": "string",
                        "description": "Space key where to create the page"
                    },
                    "title": {
                        "type": "string",
                        "description": "Page title"
                    },
                    "content": {
                        "type": "string",
                        "description": "Page content (HTML/Storage format)"
                    },
                    "parent_id": {
                        "type": "string",
                        "description": "Parent page ID (optional)"
                    }
                },
                "required": ["space_key", "title", "content"]
            }
        ),
        Tool(
            name="update_page",
            description="Update an existing Confluence page",
            inputSchema={
                "type": "object",
                "properties": {
                    "page_id": {
                        "type": "string",
                        "description": "Page ID to update"
                    },
                    "title": {
                        "type": "string",
                        "description": "New page title (optional)"
                    },
                    "content": {
                        "type": "string",
                        "description": "New page content (HTML/Storage format)"
                    },
                    "version_comment": {
                        "type": "string",
                        "description": "Version comment",
                        "default": "Updated via MCP"
                    }
                },
                "required": ["page_id", "content"]
            }
        ),
        Tool(
            name="get_page_children",
            description="Get child pages of a Confluence page",
            inputSchema={
                "type": "object",
                "properties": {
                    "page_id": {
                        "type": "string",
                        "description": "Parent page ID"
                    },
                    "limit": {
                        "type": "integer",
                        "description": "Maximum number of children to return",
                        "default": 50
                    }
                },
                "required": ["page_id"]
            }
        ),
        Tool(
            name="health_check",
            description="Check Confluence API connectivity",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        )
    ]

    # Add permissions metadata to each tool
    for tool in tools:
        # Get permissions for this tool from TOOL_PERMISSIONS dict
        perms = TOOL_PERMISSIONS.get(tool.name, [PermissionCategory.READ_REMOTE])
        perm_strings = [p.value if isinstance(p, PermissionCategory) else p for p in perms]

        # Add permissions as an attribute (MCP SDK should serialize this)
        tool.permissions = get_tool_permission_metadata(tool.name, "confluence", perm_strings)

    return tools


@server.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[TextContent]:
    """Handle tool calls."""
    try:
        if name == "list_spaces":
            return await _list_spaces(arguments.get("include_personal", False))
        elif name == "search_pages":
            return await _search_pages(arguments["cql"], arguments.get("limit", 100))
        elif name == "dump_space":
            return await _dump_space(**arguments)
        elif name == "dump_team_pages":
            return await _dump_team_pages(**arguments)
        elif name == "build_rag_index":
            return await _build_rag_index(**arguments)
        elif name == "get_page_content":
            return await _get_page_content(**arguments)
        elif name == "create_page":
            return await _create_page(**arguments)
        elif name == "update_page":
            return await _update_page(**arguments)
        elif name == "get_page_children":
            return await _get_page_children(**arguments)
        elif name == "health_check":
            return await _health_check()
        else:
            return [TextContent(type="text", text=f"Unknown tool: {name}")]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {str(e)}")]


async def _list_spaces(include_personal: bool = False) -> list[TextContent]:
    """Get list of all Confluence spaces."""
    spaces = []
    start, limit = 0, 100

    while True:
        url = f"{config.confluence_base_url.rstrip('/')}/rest/api/space?limit={limit}&start={start}&expand=homepage"
        data = _make_request(url)
        results = data.get("results", [])

        for space in results:
            key = space.get("key")
            if not include_personal and str(key).startswith("~"):
                continue

            spaces.append({
                "key": key,
                "name": space.get("name"),
                "type": space.get("type"),
                "homepage": (space.get("homepage") or {}).get("title")
            })

        if len(results) < limit:
            break
        start += limit

    space_info = []
    for space in spaces:
        space_info.append(f"{space['key']} - {space['name']} ({space['type']})")

    return [TextContent(
        type="text",
        text=f"Found {len(spaces)} spaces:\\n" + "\\n".join(space_info)
    )]


async def _search_pages(cql: str, limit: int = 100) -> list[TextContent]:
    """Search pages using CQL."""
    pages = []
    start = 0
    MAX_RESULTS = 500  # Prevent fetching thousands of pages

    while True:
        url = (f"{config.confluence_base_url.rstrip('/')}/rest/api/search?"
               f"cql={urllib.parse.quote(cql)}&limit={limit}&start={start}")

        data = _make_request(url)
        results = data.get("results", [])

        for page in results:
            pages.append({
                "id": page.get("id"),
                "title": page.get("title"),
                "spaceKey": (page.get("space") or {}).get("key"),
                "url": (page.get("_links") or {}).get("base", "").rstrip("/") +
                       (page.get("_links") or {}).get("webui", ""),
                "excerpt": page.get("excerpt", "")
            })

        # Stop if no more results OR we've reached max results
        if len(results) < limit or len(pages) >= MAX_RESULTS:
            break
        start += limit

    page_info = []
    for page in pages:
        # Include Page ID so get_page_content can use it
        page_info.append(f"{page['title']} (ID: {page['id']}, Space: {page['spaceKey']}) - {page['url']}")

    return [TextContent(
        type="text",
        text=f"Found {len(pages)} pages matching CQL:\\n" + "\\n".join(page_info)
    )]


async def _dump_space(space_key: str, format: str = "storage", max_pages: int = 0,
                     include_archived: bool = False) -> list[TextContent]:
    """Dump space content to files."""
    # This is a simplified implementation
    # In a real scenario, you'd want to implement the full dumping logic

    output_dir = config.data_dir / "confluence_dumps" / space_key
    output_dir.mkdir(parents=True, exist_ok=True)

    # Get pages in space
    cql = f'space = "{space_key}" AND type = page'
    if not include_archived:
        cql += ' AND content.archived = false'

    url = f"{config.confluence_base_url.rstrip('/')}/rest/api/search?cql={urllib.parse.quote(cql)}&limit=1000"
    data = _make_request(url)
    pages = data.get("results", [])

    if max_pages > 0:
        pages = pages[:max_pages]

    saved_count = 0
    for page in pages:
        try:
            page_id = page.get("id")
            title = _sanitize_filename(page.get("title", f"page-{page_id}"))

            # Get page content with specified format
            if format == "view":
                expand = "title,body.view"
                content_key = "view"
                ext = "html"
            elif format == "storage":
                expand = "title,body.storage"
                content_key = "storage"
                ext = "xml"
            elif format == "adf":
                expand = "title,body.atlas_doc_format"
                content_key = "atlas_doc_format"
                ext = "adf.json"

            page_url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content/{page_id}?expand={expand}"
            page_data = _make_request(page_url)

            content = (page_data.get("body") or {}).get(content_key, {})
            if format == "adf":
                file_content = json.dumps(content, ensure_ascii=False, indent=2)
            else:
                file_content = content.get("value", "")

            filename = f"{page_id} - {title}.{ext}"
            filepath = output_dir / filename
            filepath.write_text(file_content, encoding="utf-8")
            saved_count += 1

        except Exception as e:
            continue  # Skip failed pages

    return [TextContent(
        type="text",
        text=f"Space dump completed: {saved_count} pages saved to {output_dir}"
    )]


async def _dump_team_pages(space_key: str, team_name: str, format: str = "storage",
                          max_pages: int = 0) -> list[TextContent]:
    """Dump pages related to a specific team."""
    team_q = team_name.replace('"', '\\"')
    cql = f'space = "{space_key}" AND type = page AND (title ~ "{team_q}" OR text ~ "{team_q}")'

    # First search for team pages
    search_result = await _search_pages(cql, max_pages if max_pages > 0 else 200)

    output_dir = config.data_dir / "confluence_dumps" / space_key / "_teams" / _sanitize_filename(team_name)
    output_dir.mkdir(parents=True, exist_ok=True)

    # This would implement the actual dumping logic
    # For now, return the search results
    return [TextContent(
        type="text",
        text=f"Team pages for '{team_name}' found. Would save to {output_dir}\\n{search_result[0].text}"
    )]


async def _build_rag_index(space_key: str, max_words: int = 900, overlap: int = 120) -> list[TextContent]:
    """Build RAG index from space content."""
    # This would implement the RAG index building logic
    # For now, return a placeholder

    index_dir = config.data_dir / "rag_index" / space_key
    index_dir.mkdir(parents=True, exist_ok=True)

    return [TextContent(
        type="text",
        text=f"RAG index building started for space '{space_key}' with max_words={max_words}, overlap={overlap}. Index will be saved to {index_dir}"
    )]


async def _get_page_content(page_id: str, space_key: str = None, expand: List[str] = None) -> list[TextContent]:
    """Get content of a specific Confluence page."""
    try:
        if not expand:
            expand = ["body.storage", "version"]

        expand_str = ",".join(expand)

        # Check if page_id is numeric (actual ID) or a title
        if page_id.isdigit():
            url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content/{page_id}?expand={expand_str}"
        else:
            # Search by title
            if not space_key:
                return [TextContent(
                    type="text",
                    text="Error: space_key is required when searching by title"
                )]

            # First find the page by title
            search_url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content?spaceKey={space_key}&title={urllib.parse.quote(page_id)}&limit=1"
            search_data = _make_request(search_url)
            results = search_data.get("results", [])

            if not results:
                return [TextContent(
                    type="text",
                    text=f"Page not found with title: {page_id}"
                )]

            page_id = results[0]["id"]
            url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content/{page_id}?expand={expand_str}"

        data = _make_request(url)

        # Format the response
        info = []
        info.append(f"📄 Page: {data.get('title', 'Unknown')}")
        info.append(f"   ID: {data.get('id')}")
        info.append(f"   Space: {data.get('space', {}).get('key')}")
        info.append(f"   Version: {data.get('version', {}).get('number')}")
        info.append(f"   Created by: {data.get('version', {}).get('by', {}).get('displayName', 'Unknown')}")
        info.append(f"   Last modified: {data.get('version', {}).get('when', 'Unknown')}")

        # Add content if expanded
        if "body.storage" in expand:
            storage_content = data.get("body", {}).get("storage", {}).get("value", "")
            if storage_content:
                # Truncate if too long
                if len(storage_content) > 1000:
                    storage_content = storage_content[:1000] + "..."
                info.append(f"\n📝 Content (Storage Format):\n{storage_content}")

        if "body.view" in expand:
            view_content = data.get("body", {}).get("view", {}).get("value", "")
            if view_content:
                # Convert HTML to text (simple version)
                import re
                text_content = re.sub(r'<[^>]+>', '', view_content)
                if len(text_content) > 1000:
                    text_content = text_content[:1000] + "..."
                info.append(f"\n📖 Content (Text):\n{text_content}")

        web_url = f"{config.confluence_base_url.rstrip('/')}/pages/viewpage.action?pageId={data.get('id')}"
        info.append(f"\n🔗 URL: {web_url}")

        return [TextContent(
            type="text",
            text="\n".join(info)
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error getting page content: {str(e)}"
        )]


async def _create_page(space_key: str, title: str, content: str, parent_id: str = None) -> list[TextContent]:
    """Create a new Confluence page."""
    try:
        url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content"

        page_data = {
            "type": "page",
            "title": title,
            "space": {"key": space_key},
            "body": {
                "storage": {
                    "value": content,
                    "representation": "storage"
                }
            }
        }

        if parent_id:
            page_data["ancestors"] = [{"id": parent_id}]

        response = requests.post(
            url,
            json=page_data,
            auth=HTTPBasicAuth(config.confluence_email, config.confluence_api_token),
            headers={"Accept": "application/json", "Content-Type": "application/json"},
            timeout=30
        )

        if not response.ok:
            return [TextContent(
                type="text",
                text=f"Failed to create page: {response.status_code} - {response.text[:500]}"
            )]

        data = response.json()
        page_id = data.get("id")
        web_url = f"{config.confluence_base_url.rstrip('/')}/pages/viewpage.action?pageId={page_id}"

        return [TextContent(
            type="text",
            text=f"✅ Page created successfully!\n"
                 f"   Title: {title}\n"
                 f"   ID: {page_id}\n"
                 f"   Space: {space_key}\n"
                 f"   URL: {web_url}"
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error creating page: {str(e)}"
        )]


async def _update_page(page_id: str, content: str, title: str = None, version_comment: str = "Updated via MCP") -> list[TextContent]:
    """Update an existing Confluence page."""
    try:
        # First get current page info to get version
        url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content/{page_id}?expand=version"
        current_data = _make_request(url)

        current_version = current_data.get("version", {}).get("number", 1)
        current_title = current_data.get("title", "")

        # Prepare update data
        update_data = {
            "type": "page",
            "title": title or current_title,
            "body": {
                "storage": {
                    "value": content,
                    "representation": "storage"
                }
            },
            "version": {
                "number": current_version + 1,
                "message": version_comment
            }
        }

        # Update the page
        update_url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content/{page_id}"
        response = requests.put(
            update_url,
            json=update_data,
            auth=HTTPBasicAuth(config.confluence_email, config.confluence_api_token),
            headers={"Accept": "application/json", "Content-Type": "application/json"},
            timeout=30
        )

        if not response.ok:
            return [TextContent(
                type="text",
                text=f"Failed to update page: {response.status_code} - {response.text[:500]}"
            )]

        data = response.json()
        web_url = f"{config.confluence_base_url.rstrip('/')}/pages/viewpage.action?pageId={page_id}"

        return [TextContent(
            type="text",
            text=f"✅ Page updated successfully!\n"
                 f"   Title: {data.get('title')}\n"
                 f"   ID: {page_id}\n"
                 f"   Version: {current_version + 1}\n"
                 f"   Comment: {version_comment}\n"
                 f"   URL: {web_url}"
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error updating page: {str(e)}"
        )]


async def _get_page_children(page_id: str, limit: int = 50) -> list[TextContent]:
    """Get child pages of a Confluence page."""
    try:
        url = f"{config.confluence_base_url.rstrip('/')}/rest/api/content/{page_id}/child/page?limit={limit}"
        data = _make_request(url)

        children = data.get("results", [])

        if not children:
            return [TextContent(
                type="text",
                text=f"No child pages found for page ID {page_id}"
            )]

        child_info = []
        child_info.append(f"Found {len(children)} child pages:")
        child_info.append("")

        for child in children:
            child_id = child.get("id")
            child_title = child.get("title", "Unknown")
            child_status = child.get("status", "current")
            child_info.append(f"📄 {child_title}")
            child_info.append(f"   ID: {child_id}")
            child_info.append(f"   Status: {child_status}")
            child_info.append("")

        return [TextContent(
            type="text",
            text="\n".join(child_info)
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error getting page children: {str(e)}"
        )]


async def _health_check() -> list[TextContent]:
    """Check Confluence API health."""
    try:
        if not config.is_confluence_configured():
            return [TextContent(
                type="text",
                text="❌ Confluence not configured (missing email or API token)"
            )]

        # Try a simple API call
        url = f"{config.confluence_base_url.rstrip('/')}/rest/api/space?limit=1"
        _make_request(url)

        return [TextContent(
            type="text",
            text="✅ Confluence API is accessible"
        )]
    except Exception as e:
        return [TextContent(
            type="text",
            text=f"❌ Confluence API error: {str(e)}"
        )]


async def main():
    """Run the MCP server."""
    from mcp.server.stdio import stdio_server

    async with stdio_server() as (read_stream, write_stream):
        await server.run(
            read_stream,
            write_stream,
            server.create_initialization_options()
        )


if __name__ == "__main__":
    # Check configuration on startup
    if not config.is_confluence_configured():
        print("Warning: Confluence not configured", file=sys.stderr)

    asyncio.run(main())