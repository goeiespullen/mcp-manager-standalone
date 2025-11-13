#!/usr/bin/env python3
"""Azure DevOps MCP Server.

Provides tools for interacting with Azure DevOps API:
- list_projects: Get all projects
- list_teams: Get teams for a project
- get_team_iterations: Get sprints/iterations for a team
- refresh_data: Refresh sprint data
"""
from __future__ import annotations

import asyncio
import re
import subprocess
import sys
from pathlib import Path
from typing import List, Dict, Any
from urllib.parse import quote

import requests
from requests.auth import HTTPBasicAuth
from mcp.server import Server
from mcp.types import Tool as BaseTool, TextContent
from typing import Any as ToolAny

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
server = Server("devops-server")
config = MCPServerConfig.from_env()

# Tool permission mappings
TOOL_PERMISSIONS = {
    "list_projects": [PermissionCategory.READ_REMOTE],
    "list_teams": [PermissionCategory.READ_REMOTE],
    "get_team_iterations": [PermissionCategory.READ_REMOTE],
    "get_current_iteration": [PermissionCategory.READ_REMOTE],
    "health_check": [PermissionCategory.READ_REMOTE],
    "list_repositories": [PermissionCategory.READ_REMOTE],
    "get_repository_files": [PermissionCategory.READ_REMOTE],
    "get_file_content": [PermissionCategory.READ_REMOTE],
    "search_code": [PermissionCategory.READ_REMOTE],
    "get_work_items": [PermissionCategory.READ_REMOTE],
    "get_work_item_details": [PermissionCategory.READ_REMOTE],
    "refresh_data": [PermissionCategory.EXECUTE_CODE, PermissionCategory.WRITE_LOCAL],
}


class DevOpsAPIError(Exception):
    """Azure DevOps API error."""
    pass


def _get_json(url: str, timeout: int = 40) -> dict:
    """Make authenticated request to Azure DevOps API."""
    if not config.azdo_pat:
        raise DevOpsAPIError("Azure DevOps PAT not configured")

    response = requests.get(
        url,
        auth=HTTPBasicAuth("", config.azdo_pat),
        headers={
            "Accept": "application/json",
            "Content-Type": "application/json",
            "X-TFS-FedAuthRedirect": "Suppress",
        },
        timeout=timeout
    )

    if not response.ok:
        raise DevOpsAPIError(
            f"HTTP {response.status_code} {response.reason}\n"
            f"URL: {url}\n"
            f"Response: {response.text[:500]}"
        )

    return response.json()


@server.list_tools()
async def list_tools() -> list[Tool]:
    """List available tools."""
    tools = [
        Tool(
            name="list_projects",
            description="Get list of all Azure DevOps projects",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="list_teams",
            description="Get list of teams for a specific project",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    }
                },
                "required": ["project"]
            }
        ),
        Tool(
            name="get_team_iterations",
            description="Get list of available sprints/iterations for a team with their exact iteration paths. ALWAYS call this FIRST before querying sprint work items to get the correct iteration path (e.g., 'DNANSI\\Sprint 1 (juli 219)'). Returns sprint names, paths, start/finish dates.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "team": {
                        "type": "string",
                        "description": "Team name"
                    }
                },
                "required": ["project", "team"]
            }
        ),
        Tool(
            name="get_current_iteration",
            description="REQUIRED FIRST STEP when user asks for CURRENT/HUIDIGE/ACTIVE sprint! Gets TODAY's active sprint based on current date. Returns the exact iteration path needed for get_work_items. DO NOT use get_work_items for current sprint without calling this first!",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "team": {
                        "type": "string",
                        "description": "Team name"
                    }
                },
                "required": ["project", "team"]
            }
        ),
        Tool(
            name="refresh_data",
            description="Refresh sprint data for projects and teams",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name (optional, if not provided refreshes all)"
                    },
                    "teams": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "List of team names (optional)"
                    },
                    "require_effort": {
                        "type": "boolean",
                        "description": "Only include teams with effort data",
                        "default": False
                    },
                    "snapshot": {
                        "type": "string",
                        "description": "Snapshot type",
                        "enum": ["start", "end", "current"],
                        "default": "end"
                    }
                },
                "required": []
            }
        ),
        Tool(
            name="health_check",
            description="Check if Azure DevOps API is accessible",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="list_repositories",
            description="Get list of Git repositories in a project",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    }
                },
                "required": ["project"]
            }
        ),
        Tool(
            name="get_repository_files",
            description="Browse files in a repository",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "repository": {
                        "type": "string",
                        "description": "Repository name"
                    },
                    "path": {
                        "type": "string",
                        "description": "Path to browse (default: root)",
                        "default": "/"
                    }
                },
                "required": ["project", "repository"]
            }
        ),
        Tool(
            name="get_file_content",
            description="Get content of a specific file",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "repository": {
                        "type": "string",
                        "description": "Repository name"
                    },
                    "file_path": {
                        "type": "string",
                        "description": "Full path to the file"
                    },
                    "max_size": {
                        "type": "integer",
                        "description": "Maximum file size in KB (default: 100KB)",
                        "default": 100
                    }
                },
                "required": ["project", "repository", "file_path"]
            }
        ),
        Tool(
            name="search_code",
            description="Search for code across repositories",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "repository": {
                        "type": "string",
                        "description": "Repository name (optional, searches all if not specified)"
                    },
                    "search_text": {
                        "type": "string",
                        "description": "Text to search for"
                    },
                    "file_type": {
                        "type": "string",
                        "description": "File extension filter (e.g., '.py', '.js')"
                    }
                },
                "required": ["project", "search_text"]
            }
        ),
        Tool(
            name="get_work_items",
            description="Get work items (User Stories, Bugs, Tasks) using WIQL query. IMPORTANT: For CURRENT/ACTIVE sprint, FIRST call get_current_iteration. For other sprints, FIRST call get_team_iterations. NEVER guess iteration paths! Then use the exact path: SELECT [System.Id] FROM WorkItems WHERE [System.IterationPath] = 'ProjectName\\SprintName'. Returns IDs, titles, states, story points.",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "wiql_query": {
                        "type": "string",
                        "description": "WIQL query string. Example for sprint: SELECT [System.Id] FROM WorkItems WHERE [System.IterationPath] = 'DNANSI\\Sprint 1 (juli 219)'. Use exact iteration path from get_team_iterations.",
                        "default": "SELECT [System.Id], [System.Title], [System.State] FROM WorkItems WHERE [System.TeamProject] = @project AND [System.WorkItemType] = 'User Story' AND [System.State] <> 'Removed'"
                    },
                    "limit": {
                        "type": "integer",
                        "description": "Maximum number of work items to return (default: 50, max: 200)",
                        "default": 50
                    }
                },
                "required": ["project"]
            }
        ),
        Tool(
            name="get_work_item_details",
            description="Get detailed information for specific work item(s) including description and acceptance criteria",
            inputSchema={
                "type": "object",
                "properties": {
                    "project": {
                        "type": "string",
                        "description": "Project name"
                    },
                    "work_item_ids": {
                        "type": "array",
                        "items": {"type": "integer"},
                        "description": "List of work item IDs to get details for"
                    },
                    "fields": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Specific fields to retrieve (optional, gets all fields if not specified)",
                        "default": ["System.Id", "System.Title", "System.Description", "System.WorkItemType", "System.State", "System.AssignedTo", "Microsoft.VSTS.Common.AcceptanceCriteria", "System.CreatedDate", "System.ChangedDate"]
                    }
                },
                "required": ["project", "work_item_ids"]
            }
        )
    ]

    # Add permissions metadata to each tool
    for tool in tools:
        # Get permissions for this tool from TOOL_PERMISSIONS dict
        perms = TOOL_PERMISSIONS.get(tool.name, [PermissionCategory.READ_REMOTE])
        perm_strings = [p.value if isinstance(p, PermissionCategory) else p for p in perms]

        # Set permissions via our custom Tool class
        tool.permissions = get_tool_permission_metadata(tool.name, "devops", perm_strings)

    return tools


@server.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[TextContent]:
    """Handle tool calls."""
    try:
        if name == "list_projects":
            return await _list_projects()
        elif name == "list_teams":
            return await _list_teams(arguments["project"])
        elif name == "get_team_iterations":
            return await _get_team_iterations(arguments["project"], arguments["team"])
        elif name == "get_current_iteration":
            return await _get_current_iteration(arguments["project"], arguments["team"])
        elif name == "refresh_data":
            return await _refresh_data(**arguments)
        elif name == "health_check":
            return await _health_check()
        elif name == "list_repositories":
            return await _list_repositories(arguments["project"])
        elif name == "get_repository_files":
            return await _get_repository_files(arguments["project"], arguments["repository"], arguments.get("path", "/"))
        elif name == "get_file_content":
            return await _get_file_content(arguments["project"], arguments["repository"], arguments["file_path"], arguments.get("max_size", 100))
        elif name == "search_code":
            return await _search_code(arguments["project"], arguments.get("repository"), arguments["search_text"], arguments.get("file_type"))
        elif name == "get_work_items":
            return await _get_work_items(arguments["project"], arguments.get("wiql_query"), arguments.get("limit", 50))
        elif name == "get_work_item_details":
            return await _get_work_item_details(arguments["project"], arguments["work_item_ids"], arguments.get("fields"))
        else:
            return [TextContent(type="text", text=f"Unknown tool: {name}")]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {str(e)}")]


async def _list_projects() -> list[TextContent]:
    """Get list of all projects."""
    base_url = f"https://dev.azure.com/{config.azdo_org}"
    data = _get_json(f"{base_url}/_apis/projects?$top=10000&api-version=7.0")
    projects = [p["name"] for p in data.get("value", [])]

    return [TextContent(
        type="text",
        text=f"Found {len(projects)} projects: {', '.join(projects)}"
    )]


async def _list_teams(project: str) -> list[TextContent]:
    """Get list of teams for a project."""
    base_url = f"https://dev.azure.com/{config.azdo_org}"
    data = _get_json(f"{base_url}/_apis/projects/{quote(project)}/teams?$top=10000&api-version=7.0")
    teams = [t["name"] for t in data.get("value", [])]

    return [TextContent(
        type="text",
        text=f"Found {len(teams)} teams in {project}: {', '.join(teams)}"
    )]


async def _get_team_iterations(project: str, team: str) -> list[TextContent]:
    """Get iterations for a team."""
    try:
        base_url = f"https://dev.azure.com/{config.azdo_org}"
        data = _get_json(
            f"{base_url}/{quote(project)}/{quote(team)}/_apis/work/teamsettings/iterations?api-version=7.1-preview.1"
        )
        iters = data.get("value", []) or []
    except Exception as e:
        # If API fails, fallback to local CSV data
        iters = []

    # If API returned no iterations, fallback to local CSV data
    if not iters:
        csv_path = config.data_dir / project / team / "sprint_totals_all.csv"
        if csv_path.exists():
            import csv
            iter_info = []
            with open(csv_path, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                rows = list(reader)
                # Get last 10 sprints
                for row in rows[-10:]:
                    name = row.get('sprint_name', 'Unknown')
                    path = row.get('iteration_path', 'N/A')
                    start = row.get('start', 'N/A')
                    finish = row.get('finish', 'N/A')
                    iter_info.append(f"Sprint: {name} | Path: {path} | Dates: {start} to {finish}")

            result_text = f"Found {len(iter_info)} iterations for {project}/{team} (from local data):\n\n"
            result_text += "\n".join(iter_info)
            result_text += "\n\nIMPORTANT: Use the 'Path' value exactly as shown in your WIQL query's [System.IterationPath] field."

            return [TextContent(
                type="text",
                text=result_text
            )]
        else:
            return [TextContent(
                type="text",
                text=f"No iterations found for {project}/{team} (API returned empty and no local data available)"
            )]

    # Sort safely on startDate
    sorted_iters = sorted(iters, key=lambda x: (x.get("attributes", {}).get("startDate") or "9999-12-31"))

    iter_info = []
    for iteration in sorted_iters:
        name = iteration.get("name", "Unknown")
        path = iteration.get("path", "N/A")
        attrs = iteration.get("attributes", {})
        start = attrs.get("startDate", "N/A")
        end = attrs.get("finishDate", "N/A")
        # Include the iteration path in the output - this is what's needed for WIQL queries!
        iter_info.append(f"Sprint: {name} | Path: {path} | Dates: {start} to {end}")

    result_text = f"Found {len(iter_info)} iterations for {project}/{team}:\n\n"
    result_text += "\n".join(iter_info)
    result_text += "\n\nIMPORTANT: Use the 'Path' value exactly as shown in your WIQL query's [System.IterationPath] field."

    return [TextContent(
        type="text",
        text=result_text
    )]


async def _get_current_iteration(project: str, team: str) -> list[TextContent]:
    """Get the current/active iteration for a team using @CurrentIteration macro."""
    print(f"\n🔍 DEBUG get_current_iteration called for: {project}/{team}")
    try:
        # Use WIQL query with @CurrentIteration macro to get the current sprint
        base_url = f"https://dev.azure.com/{config.azdo_org}"

        # WIQL query to get work items from current iteration
        # Use team-specific endpoint so @CurrentIteration automatically uses team context
        wiql_query = f"""
        SELECT [System.Id]
        FROM WorkItems
        WHERE [System.TeamProject] = '{project}'
        AND [System.IterationPath] = @CurrentIteration('[{project}]\\{team}')
        """

        wiql_request = {"query": wiql_query}
        print(f"   Trying @CurrentIteration macro with Azure DevOps API...")

        # Use team-specific WIQL endpoint to provide team context
        response = requests.post(
            f"{base_url}/{quote(project)}/{quote(team)}/_apis/wit/wiql?api-version=7.1",
            auth=HTTPBasicAuth("", config.azdo_pat),
            headers={
                "Accept": "application/json",
                "Content-Type": "application/json",
                "X-TFS-FedAuthRedirect": "Suppress",
            },
            json=wiql_request,
            timeout=40
        )

        if not response.ok:
            print(f"   ❌ @CurrentIteration API failed: {response.status_code}, falling back to CSV...")
            return [TextContent(
                type="text",
                text=f"Failed to get current iteration: HTTP {response.status_code} - {response.text[:500]}"
            )]

        data = response.json()
        work_items = data.get("workItems", [])

        # If we found work items, we can determine the current iteration
        # Now get the actual iteration details from team settings
        iter_response = requests.get(
            f"{base_url}/{quote(project)}/{quote(team)}/_apis/work/teamsettings/iterations?api-version=7.1-preview.1",
            auth=HTTPBasicAuth("", config.azdo_pat),
            headers={
                "Accept": "application/json",
                "X-TFS-FedAuthRedirect": "Suppress",
            },
            timeout=40
        )

        if iter_response.ok:
            iter_data = iter_response.json()
            iterations = iter_data.get("value", [])

            # Find current iteration based on date
            from datetime import datetime
            today = datetime.now().date()

            for iteration in iterations:
                attrs = iteration.get("attributes", {})
                start_str = attrs.get("startDate")
                finish_str = attrs.get("finishDate")

                if start_str and finish_str:
                    try:
                        start_date = datetime.fromisoformat(start_str.replace('Z', '+00:00')).date()
                        finish_date = datetime.fromisoformat(finish_str.replace('Z', '+00:00')).date()

                        if start_date <= today <= finish_date:
                            name = iteration.get("name", "Unknown")
                            path = iteration.get("path", "N/A")

                            result_text = f"Current sprint for {project}/{team}:\n\n"
                            result_text += f"Sprint: {name}\n"
                            result_text += f"Path: {path}\n"
                            result_text += f"Dates: {start_str} to {finish_str}\n"
                            result_text += f"Work items in sprint: {len(work_items)}\n\n"
                            result_text += f"⚠️ IMPORTANT: Use this EXACT path in WIQL queries: {path}"

                            return [TextContent(
                                type="text",
                                text=result_text
                            )]
                    except Exception as e:
                        continue

        # Fallback: use CSV data
        print(f"   Falling back to CSV data...")
        csv_path = config.data_dir / project / team / "sprint_totals_all.csv"
        if csv_path.exists():
            import csv
            from datetime import datetime

            today = datetime.now().date()
            print(f"   Today's date: {today}")

            with open(csv_path, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    start_str = row.get('start', '')
                    finish_str = row.get('finish', '')

                    if start_str and finish_str:
                        try:
                            start_date = datetime.fromisoformat(start_str.replace('Z', '+00:00')).date()
                            finish_date = datetime.fromisoformat(finish_str.replace('Z', '+00:00')).date()

                            if start_date <= today <= finish_date:
                                name = row.get('sprint_name', 'Unknown')
                                path = row.get('iteration_path', 'N/A')

                                print(f"   ✅ Found current sprint in CSV: {name}")
                                print(f"   ✅ Iteration path: {path}")

                                result_text = f"Current sprint for {project}/{team} (from local data):\n\n"
                                result_text += f"Sprint: {name}\n"
                                result_text += f"Path: {path}\n"
                                result_text += f"Dates: {start_str} to {finish_str}\n\n"
                                result_text += f"⚠️ IMPORTANT: Use this EXACT path in WIQL queries: {path}"

                                return [TextContent(
                                    type="text",
                                    text=result_text
                                )]
                        except Exception as e:
                            print(f"   ⚠️ Error parsing sprint row: {e}")
                            continue

            print(f"   ❌ No active sprint found for {today}")

        return [TextContent(
            type="text",
            text=f"Could not determine current sprint for {project}/{team}. No active sprint found for today's date."
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error getting current iteration: {str(e)}"
        )]


async def _refresh_data(project: str = None, teams: List[str] = None,
                       require_effort: bool = False, snapshot: str = "end") -> list[TextContent]:
    """Refresh sprint data using the existing script."""
    try:
        cmd = ["python3", "devops_sprint_totals.py", "--data-dir", str(config.data_dir), "--snapshot", snapshot]

        if require_effort:
            cmd.append("--require-effort-used")

        if project and teams:
            cmd.extend(["--project", project])
            for team in teams:
                cmd.extend(["--team", team])
        else:
            cmd.append("--scan-all")

        result = subprocess.run(cmd, capture_output=True, text=True, check=False, timeout=300)
        success = (result.returncode == 0)

        if success:
            return [TextContent(
                type="text",
                text=f"Data refresh successful. Output: {result.stdout[:500]}"
            )]
        else:
            return [TextContent(
                type="text",
                text=f"Data refresh failed. Error: {result.stderr or result.stdout}"
            )]

    except subprocess.TimeoutExpired:
        return [TextContent(
            type="text",
            text="Data refresh timed out after 5 minutes"
        )]
    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Data refresh exception: {str(e)}"
        )]


async def _health_check() -> list[TextContent]:
    """Check Azure DevOps API health."""
    try:
        if not config.is_devops_configured():
            return [TextContent(
                type="text",
                text="❌ Azure DevOps not configured (missing PAT token)"
            )]

        # Try a simple API call
        base_url = f"https://dev.azure.com/{config.azdo_org}"
        _get_json(f"{base_url}/_apis/projects?$top=1&api-version=7.0")

        return [TextContent(
            type="text",
            text="✅ Azure DevOps API is accessible"
        )]
    except Exception as e:
        return [TextContent(
            type="text",
            text=f"❌ Azure DevOps API error: {str(e)}"
        )]


async def _list_repositories(project: str) -> list[TextContent]:
    """Get list of Git repositories in a project."""
    base_url = f"https://dev.azure.com/{config.azdo_org}"
    data = _get_json(f"{base_url}/_apis/git/repositories?project={quote(project)}&api-version=7.0")
    repos = []

    for repo in data.get("value", []):
        repos.append({
            "name": repo.get("name"),
            "id": repo.get("id"),
            "url": repo.get("webUrl"),
            "defaultBranch": repo.get("defaultBranch", "refs/heads/main"),
            "size": repo.get("size", 0)
        })

    repo_info = []
    for repo in repos:
        size_mb = round(repo["size"] / (1024 * 1024), 2) if repo["size"] else 0
        repo_info.append(f"{repo['name']} ({size_mb} MB) - {repo['url']}")

    return [TextContent(
        type="text",
        text=f"Found {len(repos)} repositories in {project}:\\n" + "\\n".join(repo_info)
    )]


async def _get_repository_files(project: str, repository: str, path: str = "/") -> list[TextContent]:
    """Browse files in a repository."""
    try:
        # Clean path
        path = path.strip("/") if path != "/" else ""
        base_url = f"https://dev.azure.com/{config.azdo_org}"
        url = f"{base_url}/{quote(project)}/_apis/git/repositories/{quote(repository)}/items"

        params = {
            "path": f"/{path}" if path else "/",
            "recursionLevel": "OneLevel",
            "includeContentMetadata": "true",
            "api-version": "7.0"
        }

        # Build URL with params
        param_str = "&".join([f"{k}={quote(str(v))}" for k, v in params.items()])
        full_url = f"{url}?{param_str}"

        data = _get_json(full_url)
        items = data.get("value", [])

        files = []
        folders = []

        for item in items:
            if item.get("gitObjectType") == "tree":
                folders.append(f"📁 {item.get('path', '')}")
            elif item.get("gitObjectType") == "blob":
                size = item.get("size", 0)
                size_kb = round(size / 1024, 1) if size else 0
                files.append(f"📄 {item.get('path', '')} ({size_kb} KB)")

        result = []
        if folders:
            result.extend(sorted(folders))
        if files:
            result.extend(sorted(files))

        return [TextContent(
            type="text",
            text=f"Contents of {repository}:{path}\\n" + "\\n".join(result[:50])  # Limit output
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error browsing repository: {str(e)}"
        )]


async def _get_file_content(project: str, repository: str, file_path: str, max_size: int = 100) -> list[TextContent]:
    """Get content of a specific file."""
    try:
        max_bytes = max_size * 1024  # Convert KB to bytes
        base_url = f"https://dev.azure.com/{config.azdo_org}"
        url = f"{base_url}/{quote(project)}/_apis/git/repositories/{quote(repository)}/items"

        params = {
            "path": file_path,
            "includeContent": "true",
            "api-version": "7.0"
        }

        param_str = "&".join([f"{k}={quote(str(v))}" for k, v in params.items()])
        full_url = f"{url}?{param_str}"

        data = _get_json(full_url)

        if data.get("gitObjectType") != "blob":
            return [TextContent(
                type="text",
                text=f"Error: {file_path} is not a file"
            )]

        content = data.get("content", "")
        file_size = data.get("size", 0)

        if file_size > max_bytes:
            return [TextContent(
                type="text",
                text=f"File too large: {file_size} bytes (max: {max_bytes} bytes). Use a larger max_size parameter."
            )]

        # Try to decode content (it might be base64 encoded)
        try:
            import base64
            decoded_content = base64.b64decode(content).decode('utf-8')
        except:
            decoded_content = content

        return [TextContent(
            type="text",
            text=f"File: {file_path} ({file_size} bytes)\\n\\n{decoded_content}"
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error reading file: {str(e)}"
        )]


async def _search_code(project: str, repository: str = None, search_text: str = "", file_type: str = None) -> list[TextContent]:
    """Search for code across repositories."""
    try:
        # For now, return a placeholder as Azure DevOps code search API requires special setup
        repo_filter = f" in repository {repository}" if repository else ""
        type_filter = f" in {file_type} files" if file_type else ""

        return [TextContent(
            type="text",
            text=f"Code search for '{search_text}'{repo_filter}{type_filter} in project {project}\\n\\n"
                  f"Note: Full code search implementation requires Azure DevOps search service configuration.\\n"
                  f"Consider using get_repository_files and get_file_content for specific file searches."
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error searching code: {str(e)}"
        )]


async def _get_work_items(project: str, wiql_query: str = None, limit: int = 50) -> list[TextContent]:
    """Get work items using WIQL query."""
    try:
        base_url = f"https://dev.azure.com/{config.azdo_org}"

        # Default WIQL query for active user stories
        if not wiql_query:
            wiql_query = f"SELECT [System.Id], [System.Title], [System.State] FROM WorkItems WHERE [System.TeamProject] = '{project}' AND [System.WorkItemType] = 'User Story' AND [System.State] <> 'Removed'"

        # Ensure limit doesn't exceed 200 (Azure DevOps max)
        limit = min(limit, 200)

        # Add TOP clause if not present
        if "TOP" not in wiql_query.upper():
            wiql_query = wiql_query.replace("SELECT", f"SELECT TOP {limit}")

        # Prepare WIQL request
        wiql_request = {
            "query": wiql_query
        }

        # Make WIQL query request
        response = requests.post(
            f"{base_url}/{quote(project)}/_apis/wit/wiql?api-version=7.1",
            auth=HTTPBasicAuth("", config.azdo_pat),
            headers={
                "Accept": "application/json",
                "Content-Type": "application/json",
                "X-TFS-FedAuthRedirect": "Suppress",
            },
            json=wiql_request,
            timeout=40
        )

        if not response.ok:
            error_text = response.text
            # Check if error is about non-existent iteration path
            if "TF51011" in error_text or "iteration path does not exist" in error_text.lower():
                error_msg = f"❌ ITERATION PATH ERROR: The specified iteration path does not exist in Azure DevOps.\n\n"
                error_msg += f"WIQL Query used: {wiql_query}\n\n"
                error_msg += "💡 SOLUTION: You MUST call get_current_iteration (for current sprint) or get_team_iterations (for other sprints) FIRST to get the exact, valid iteration path.\n"
                error_msg += "DO NOT guess or construct iteration paths yourself!\n\n"
                error_msg += f"Original error: {error_text[:300]}"
                raise DevOpsAPIError(error_msg)
            else:
                raise DevOpsAPIError(
                    f"WIQL query failed: HTTP {response.status_code} {response.reason}\\n"
                    f"Response: {error_text[:500]}"
                )

        data = response.json()
        work_items = data.get("workItems", [])

        if not work_items:
            return [TextContent(
                type="text",
                text=f"No work items found for query in project {project}"
            )]

        # Format results
        result_lines = [f"Found {len(work_items)} work items in {project}:"]
        result_lines.append("")

        for item in work_items:
            work_item_id = item.get("id", "N/A")
            result_lines.append(f"• Work Item #{work_item_id}")

        result_lines.append("")
        result_lines.append(f"Use get_work_item_details with IDs: {[item.get('id') for item in work_items]}")

        return [TextContent(
            type="text",
            text="\\n".join(result_lines)
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error getting work items: {str(e)}"
        )]


async def _get_work_item_details(project: str, work_item_ids: List[int], fields: List[str] = None) -> list[TextContent]:
    """Get detailed information for specific work items."""
    try:
        base_url = f"https://dev.azure.com/{config.azdo_org}"

        # Default fields if none specified
        if not fields:
            fields = [
                "System.Id", "System.Title", "System.Description",
                "System.WorkItemType", "System.State", "System.AssignedTo",
                "Microsoft.VSTS.Common.AcceptanceCriteria",
                "System.CreatedDate", "System.ChangedDate"
            ]

        # Limit to max 200 work items
        work_item_ids = work_item_ids[:200]

        # Get work items in batch
        ids_param = ",".join(map(str, work_item_ids))
        fields_param = ",".join(fields)

        url = f"{base_url}/_apis/wit/workitems"
        params = {
            "ids": ids_param,
            "fields": fields_param,
            "api-version": "7.1",
            "$expand": "fields"
        }

        param_str = "&".join([f"{k}={quote(str(v))}" for k, v in params.items()])
        full_url = f"{url}?{param_str}"

        data = _get_json(full_url)
        work_items = data.get("value", [])

        if not work_items:
            return [TextContent(
                type="text",
                text=f"No work items found with IDs: {work_item_ids}"
            )]

        # Format detailed results
        result_lines = [f"Work Item Details ({len(work_items)} items):"]
        result_lines.append("=" * 50)

        for item in work_items:
            item_fields = item.get("fields", {})

            work_item_id = item_fields.get("System.Id", "N/A")
            title = item_fields.get("System.Title", "No Title")
            work_type = item_fields.get("System.WorkItemType", "Unknown")
            state = item_fields.get("System.State", "Unknown")
            assigned_to = item_fields.get("System.AssignedTo", {}).get("displayName", "Unassigned") if isinstance(item_fields.get("System.AssignedTo"), dict) else str(item_fields.get("System.AssignedTo", "Unassigned"))

            result_lines.append(f"\\n🎯 #{work_item_id}: {title}")
            result_lines.append(f"   Type: {work_type} | State: {state} | Assigned: {assigned_to}")

            # Description
            description = item_fields.get("System.Description", "")
            if description:
                # Clean HTML tags from description
                import re
                clean_desc = re.sub(r'<[^>]+>', '', description).strip()
                if len(clean_desc) > 200:
                    clean_desc = clean_desc[:200] + "..."
                result_lines.append(f"   📝 Description: {clean_desc}")

            # Acceptance Criteria
            acceptance_criteria = item_fields.get("Microsoft.VSTS.Common.AcceptanceCriteria", "")
            if acceptance_criteria:
                # Clean HTML tags from acceptance criteria
                clean_ac = re.sub(r'<[^>]+>', '', acceptance_criteria).strip()
                if len(clean_ac) > 200:
                    clean_ac = clean_ac[:200] + "..."
                result_lines.append(f"   ✅ Acceptance Criteria: {clean_ac}")

            # Dates
            created = item_fields.get("System.CreatedDate", "")
            changed = item_fields.get("System.ChangedDate", "")
            if created:
                result_lines.append(f"   📅 Created: {created[:10]}")
            if changed:
                result_lines.append(f"   🔄 Last Changed: {changed[:10]}")

            result_lines.append("")

        return [TextContent(
            type="text",
            text="\\n".join(result_lines)
        )]

    except Exception as e:
        return [TextContent(
            type="text",
            text=f"Error getting work item details: {str(e)}"
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
    if not config.is_devops_configured():
        print("Warning: Azure DevOps PAT not configured", file=sys.stderr)

    asyncio.run(main())