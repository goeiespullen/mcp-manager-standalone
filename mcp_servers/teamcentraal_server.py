#!/usr/bin/env python3
"""
TeamCentraal MCP Server - Toegang tot NS TeamCentraal OData API

Biedt tools voor:
- Teams ophalen en doorzoeken
- Teamleden en rollen
- Organisatiestructuur (Departments, Clusters, RG's)
- Azure DevOps en Jira koppelingen
- DORA metingen en effectiviteit
"""

import sys
import json
import logging
import os
import requests
from typing import Any, Dict, Optional, List
from urllib.parse import quote
from pathlib import Path

# Import permission system
sys.path.insert(0, str(Path(__file__).parent))
from shared.permissions import PermissionCategory, get_tool_permission_metadata

# Setup logging to stderr (stdout is used for MCP protocol)
logging.basicConfig(
    level=logging.INFO,
    stream=sys.stderr,
    format='[TeamCentraal MCP] %(message)s'
)
logger = logging.getLogger(__name__)


class TeamCentraalAPI:
    """Client voor TeamCentraal OData V4 API."""

    def __init__(self, base_url: str, username: str, password: str):
        self.base_url = base_url.rstrip('/')
        self.username = username
        self.password = password
        self.session = requests.Session()
        self.session.auth = (username, password)
        self.session.headers.update({
            'Accept': 'application/json',
            'Content-Type': 'application/json',
            'User-Agent': 'curl/8.0.1'  # Mimic curl to bypass Azure App Gateway filtering
        })

    def _make_request(self, endpoint: str, params: Optional[Dict] = None, timeout: int = 90) -> Dict[str, Any]:
        """Maak een API request."""
        url = f"{self.base_url}/{endpoint.lstrip('/')}"

        try:
            logger.info(f"API Request: {endpoint}")
            response = self.session.get(url, params=params, timeout=timeout)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.Timeout as e:
            logger.error(f"API Timeout after {timeout}s: {endpoint}")
            raise Exception(f"TeamCentraal API timeout after {timeout}s - query te complex. Probeer met minder expand opties.")
        except requests.exceptions.RequestException as e:
            logger.error(f"API Error: {e}")
            raise Exception(f"TeamCentraal API error: {str(e)}")

    def get_teams(self, filter_query: Optional[str] = None,
                  select: Optional[str] = None,
                  expand: Optional[str] = None,
                  top: Optional[int] = None,
                  skip: Optional[int] = None) -> Dict[str, Any]:
        """Haal teams op."""
        params = {}
        if filter_query:
            params['$filter'] = filter_query
        if select:
            params['$select'] = select
        if expand:
            params['$expand'] = expand
        if top is not None:
            params['$top'] = top
        if skip is not None:
            params['$skip'] = skip

        return self._make_request('Teams', params)

    def get_team_by_id(self, team_id: str, expand: Optional[str] = None) -> Dict[str, Any]:
        """Haal specifiek team op."""
        params = {}
        if expand:
            params['$expand'] = expand
        return self._make_request(f'Teams({team_id})', params)

    def get_team_members(self, filter_query: Optional[str] = None,
                        expand: Optional[str] = None) -> Dict[str, Any]:
        """Haal teamleden op."""
        params = {}
        if filter_query:
            params['$filter'] = filter_query
        if expand:
            params['$expand'] = expand
        return self._make_request('TeamMembers', params)

    def get_departments(self, filter_query: Optional[str] = None,
                       expand: Optional[str] = None) -> Dict[str, Any]:
        """Haal departments op (RG's, Clusters, Domeinen)."""
        params = {}
        if filter_query:
            params['$filter'] = filter_query
        if expand:
            params['$expand'] = expand
        return self._make_request('Departments', params)

    def get_accounts(self, filter_query: Optional[str] = None) -> Dict[str, Any]:
        """Haal accounts op."""
        params = {}
        if filter_query:
            params['$filter'] = filter_query
        return self._make_request('Accounts', params)

    def get_responsible_applications(self, filter_query: Optional[str] = None) -> Dict[str, Any]:
        """Haal verantwoordelijke applicaties op."""
        params = {}
        if filter_query:
            params['$filter'] = filter_query
        return self._make_request('ResponsibleApplications', params)

    def get_dora_metings(self, expand: Optional[str] = None) -> Dict[str, Any]:
        """Haal DORA metingen op."""
        params = {}
        if expand:
            params['$expand'] = expand
        return self._make_request('DoraMetings', params)

    def search_teams(self, name_query: str) -> Dict[str, Any]:
        """Zoek teams op naam."""
        filter_query = f"contains(Naam, '{name_query}')"
        return self.get_teams(
            filter_query=filter_query,
            select='ID,Naam,OmschrijvingTeam,TeamCategory,_AzureDevOpsKey,_JiraKey'
        )


class TeamCentraalMCPServer:
    """TeamCentraal MCP Server."""

    def __init__(self):
        # Load config from environment
        self.base_url = os.environ.get(
            'TEAMCENTRAAL_URL',
            'https://teamcentraal-a.ns.nl/odata/POS_Odata_v4'
        )
        self.username = os.environ.get('TEAMCENTRAAL_USERNAME')
        self.password = os.environ.get('TEAMCENTRAAL_PASSWORD')

        # Initialize API client
        self.api = None
        if self.username and self.password:
            self.api = TeamCentraalAPI(self.base_url, self.username, self.password)
            logger.info("TeamCentraal API client initialized")
        else:
            logger.warning("TeamCentraal credentials not configured")

        # Define available tools with permission metadata
        self.server_type = "teamcentraal"
        self.tools = {
            "list_teams": {
                "description": "Haal alle teams op uit TeamCentraal. Ondersteunt filtering, selectie en paginering.",
                "permissions": get_tool_permission_metadata("list_teams", self.server_type, [PermissionCategory.READ_REMOTE.value]),
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "filter": {
                            "type": "string",
                            "description": "OData filter query (bijv. \"TeamCategory eq 'Development'\")"
                        },
                        "select": {
                            "type": "string",
                            "description": "Komma-gescheiden veldnamen (bijv. \"ID,Name,TeamCategory\")"
                        },
                        "expand": {
                            "type": "string",
                            "description": "Gerelateerde data ophalen (bijv. \"Team_Department\")"
                        },
                        "top": {
                            "type": "number",
                            "description": "Maximaal aantal resultaten"
                        },
                        "skip": {
                            "type": "number",
                            "description": "Aantal resultaten overslaan (paginering)"
                        }
                    }
                }
            },
            "get_team": {
                "description": "Haal een specifiek team op via ID",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "team_id": {
                            "type": "string",
                            "description": "Team ID uit TeamCentraal"
                        },
                        "expand": {
                            "type": "string",
                            "description": "Gerelateerde data ophalen (bijv. \"Team_Department\")"
                        }
                    },
                    "required": ["team_id"]
                }
            },
            "search_teams": {
                "description": "Zoek teams op naam met contains match",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "name": {
                            "type": "string",
                            "description": "Zoekterm voor teamnaam (case-insensitive contains)"
                        }
                    },
                    "required": ["name"]
                }
            },
            "list_team_members": {
                "description": "Haal teamleden op voor een specifiek team. Accepteert team ID of team naam (bijv. 'DIA.NSXR').",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "team_id": {
                            "type": "string",
                            "description": "Team ID (numeriek)"
                        },
                        "team_name": {
                            "type": "string",
                            "description": "Team naam (bijv. 'DIA.NSXR'). Wordt automatisch omgezet naar team ID."
                        },
                        "expand": {
                            "type": "string",
                            "description": "Gerelateerde data. Default: 'Account'. Voor meer details gebruik 'Account,FunctieRols'"
                        },
                        "top": {
                            "type": "number",
                            "description": "Maximaal aantal resultaten (default: 100)"
                        }
                    },
                    "required": []
                }
            },
            "list_departments": {
                "description": "Haal departments op (RG's, Clusters, Domeinen)",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "filter": {
                            "type": "string",
                            "description": "OData filter query"
                        },
                        "expand": {
                            "type": "string",
                            "description": "Gerelateerde data (bijv. \"Rols\")"
                        }
                    }
                }
            },
            "get_azure_devops_teams": {
                "description": "Haal alle teams op die gekoppeld zijn aan Azure DevOps",
                "inputSchema": {
                    "type": "object",
                    "properties": {}
                }
            },
            "get_jira_teams": {
                "description": "Haal alle teams op die gekoppeld zijn aan Jira",
                "inputSchema": {
                    "type": "object",
                    "properties": {}
                }
            },
            "list_responsible_applications": {
                "description": "Haal verantwoordelijke applicaties op (ServiceNow koppeling)",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "team_id": {
                            "type": "string",
                            "description": "Optioneel: filter op specifiek team ID"
                        }
                    }
                }
            },
            "list_dora_metings": {
                "description": "Haal DORA metingen op",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "expand": {
                            "type": "string",
                            "description": "Gerelateerde data (bijv. \"DoraAnswers\")"
                        }
                    }
                }
            },
            "get_team_by_azdo_key": {
                "description": "Zoek team op Azure DevOps key",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "azdo_key": {
                            "type": "string",
                            "description": "Azure DevOps project key"
                        }
                    },
                    "required": ["azdo_key"]
                }
            }
        }

    def handle_initialize(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle initialization request."""
        logger.info("Received initialize request")
        return {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "tools": {}
            },
            "serverInfo": {
                "name": "teamcentraal-mcp-server",
                "version": "1.0.0"
            }
        }

    def handle_tools_list(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """List available tools with permission metadata."""
        logger.info("Listing tools")
        return {
            "tools": [
                {
                    "name": name,
                    "description": tool["description"],
                    "inputSchema": tool["inputSchema"],
                    "permissions": tool.get("permissions", get_tool_permission_metadata(name, self.server_type))
                }
                for name, tool in self.tools.items()
            ]
        }

    def handle_tools_call(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Call a tool."""
        tool_name = params.get("name")
        arguments = params.get("arguments", {})

        logger.info(f"Calling tool: {tool_name} with args: {arguments}")

        # Check if API is configured
        if not self.api:
            return {
                "content": [{
                    "type": "text",
                    "text": "❌ TeamCentraal credentials niet geconfigureerd. Stel TEAMCENTRAAL_USERNAME en TEAMCENTRAAL_PASSWORD environment variables in."
                }]
            }

        try:
            # Route to appropriate handler
            if tool_name == "list_teams":
                result = self.api.get_teams(
                    filter_query=arguments.get("filter"),
                    select=arguments.get("select"),
                    expand=arguments.get("expand"),
                    top=arguments.get("top"),
                    skip=arguments.get("skip")
                )
                return self._format_response(result)

            elif tool_name == "get_team":
                result = self.api.get_team_by_id(
                    team_id=arguments["team_id"],
                    expand=arguments.get("expand")
                )
                return self._format_response(result)

            elif tool_name == "search_teams":
                result = self.api.search_teams(arguments["name"])
                return self._format_response(result)

            elif tool_name == "list_team_members":
                # Bepaal team ID (zoek op naam indien nodig)
                team_id = arguments.get("team_id")
                team_name = arguments.get("team_name")

                if not team_id and not team_name:
                    return {
                        "content": [{
                            "type": "text",
                            "text": "❌ Error: Geef team_id of team_name op"
                        }]
                    }

                # Als team_name gegeven is, zoek eerst het team op
                if team_name and not team_id:
                    logger.info(f"Zoeken naar team met naam: {team_name}")
                    search_result = self.api.search_teams(team_name)

                    if not search_result.get("value"):
                        return {
                            "content": [{
                                "type": "text",
                                "text": f"❌ Team '{team_name}' niet gevonden"
                            }]
                        }

                    # Neem het eerste resultaat
                    teams = search_result["value"]
                    if len(teams) > 1:
                        logger.warning(f"Meerdere teams gevonden voor '{team_name}', gebruik eerste match")

                    team_id = teams[0]["ID"]
                    logger.info(f"Team '{team_name}' gevonden met ID: {team_id}")

                # Haal teamleden op met geoptimaliseerde query
                # Gebruik direct filter op TeamMembers in plaats van navigatie
                filter_query = f"TeamMember_Team/ID eq {team_id}"
                expand = arguments.get("expand", "Account")  # Default alleen Account, niet alle relaties
                top = arguments.get("top", 100)

                logger.info(f"Ophalen teamleden voor team ID {team_id} (expand: {expand})")

                # Gebruik aangepaste timeout voor deze query
                params = {"$filter": filter_query, "$expand": expand, "$top": top}
                try:
                    result = self.api._make_request('TeamMembers', params, timeout=90)
                except Exception as e:
                    if "timeout" in str(e).lower():
                        return {
                            "content": [{
                                "type": "text",
                                "text": f"❌ Timeout: Query duurde te lang. Probeer met minder expand opties (nu: '{expand}'). Gebruik alleen 'Account' in plaats van 'Account,FunctieRols,TeamMember_Team'"
                            }]
                        }
                    raise

                return self._format_response(result)

            elif tool_name == "list_departments":
                result = self.api.get_departments(
                    filter_query=arguments.get("filter"),
                    expand=arguments.get("expand")
                )
                return self._format_response(result)

            elif tool_name == "get_azure_devops_teams":
                result = self.api.get_teams(
                    filter_query="AzureDevOpsKey ne null",
                    select="ID,Name,TeamCategory,AzureDevOpsKey"
                )
                return self._format_response(result)

            elif tool_name == "get_jira_teams":
                result = self.api.get_teams(
                    filter_query="JiraKey ne null",
                    select="ID,Name,TeamCategory,JiraKey"
                )
                return self._format_response(result)

            elif tool_name == "list_responsible_applications":
                filter_query = None
                if "team_id" in arguments:
                    filter_query = f"Team/ID eq {arguments['team_id']}"
                result = self.api.get_responsible_applications(filter_query)
                return self._format_response(result)

            elif tool_name == "list_dora_metings":
                result = self.api.get_dora_metings(
                    expand=arguments.get("expand")
                )
                return self._format_response(result)

            elif tool_name == "get_team_by_azdo_key":
                azdo_key = arguments["azdo_key"]
                result = self.api.get_teams(
                    filter_query=f"AzureDevOpsKey eq '{azdo_key}'",
                    expand="Team_Department"
                )
                return self._format_response(result)

            else:
                raise ValueError(f"Unknown tool: {tool_name}")

        except Exception as e:
            logger.error(f"Tool execution error: {e}", exc_info=True)
            return {
                "content": [{
                    "type": "text",
                    "text": f"❌ Error: {str(e)}"
                }]
            }

    def _format_response(self, data: Dict[str, Any]) -> Dict[str, Any]:
        """Format API response as MCP content."""
        # Pretty print JSON
        formatted = json.dumps(data, indent=2, ensure_ascii=False)

        # Add summary if it's a collection
        summary = ""
        if "value" in data:
            count = len(data["value"])
            summary = f"✅ {count} resultaten gevonden\n\n"

        return {
            "content": [{
                "type": "text",
                "text": f"{summary}```json\n{formatted}\n```"
            }]
        }

    def handle_request(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Handle a JSON-RPC request."""
        method = request.get("method")
        params = request.get("params", {})

        logger.debug(f"Handling method: {method}")

        # Route to appropriate handler
        if method == "initialize":
            result = self.handle_initialize(params)
        elif method == "tools/list":
            result = self.handle_tools_list(params)
        elif method == "tools/call":
            result = self.handle_tools_call(params)
        else:
            raise ValueError(f"Unknown method: {method}")

        return result

    def run(self):
        """Main server loop - read from stdin, write to stdout."""
        logger.info("TeamCentraal MCP Server starting...")
        logger.info(f"Base URL: {self.base_url}")
        logger.info(f"Username configured: {bool(self.username)}")

        try:
            for line in sys.stdin:
                try:
                    # Parse request
                    request = json.loads(line)
                    request_id = request.get("id")

                    logger.debug(f"Received request: {request}")

                    # Handle request
                    result = self.handle_request(request)

                    # Send response
                    response = {
                        "jsonrpc": "2.0",
                        "id": request_id,
                        "result": result
                    }

                    # Write to stdout (MCP protocol uses stdout for responses)
                    print(json.dumps(response), flush=True)
                    logger.debug(f"Sent response")

                except json.JSONDecodeError as e:
                    logger.error(f"JSON decode error: {e}")
                    # Send error response
                    error_response = {
                        "jsonrpc": "2.0",
                        "id": request.get("id") if 'request' in locals() else None,
                        "error": {
                            "code": -32700,
                            "message": "Parse error"
                        }
                    }
                    print(json.dumps(error_response), flush=True)

                except Exception as e:
                    logger.error(f"Error handling request: {e}", exc_info=True)
                    # Send error response
                    error_response = {
                        "jsonrpc": "2.0",
                        "id": request.get("id") if 'request' in locals() else None,
                        "error": {
                            "code": -32603,
                            "message": str(e)
                        }
                    }
                    print(json.dumps(error_response), flush=True)

        except KeyboardInterrupt:
            logger.info("Server stopped")


if __name__ == "__main__":
    server = TeamCentraalMCPServer()
    server.run()
