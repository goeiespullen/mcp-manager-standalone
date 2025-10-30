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
            'Content-Type': 'application/json'
        })

    def _make_request(self, endpoint: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Maak een API request."""
        url = f"{self.base_url}/{endpoint.lstrip('/')}"

        try:
            logger.info(f"API Request: {endpoint}")
            response = self.session.get(url, params=params, timeout=30)
            response.raise_for_status()
            return response.json()
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
        filter_query = f"contains(Name, '{name_query}')"
        return self.get_teams(
            filter_query=filter_query,
            select='ID,Name,TeamCategory,AzureDevOpsKey,JiraKey'
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

        # Define available tools
        self.tools = {
            "list_teams": {
                "description": "Haal alle teams op uit TeamCentraal. Ondersteunt filtering, selectie en paginering.",
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
                "description": "Haal teamleden op, optioneel gefilterd op team",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "team_id": {
                            "type": "string",
                            "description": "Optioneel: filter op specifiek team ID"
                        },
                        "expand": {
                            "type": "string",
                            "description": "Gerelateerde data (bijv. \"TeamMember_Team,Account,FunctieRols\")"
                        }
                    }
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
        """List available tools."""
        logger.info("Listing tools")
        return {
            "tools": [
                {
                    "name": name,
                    "description": tool["description"],
                    "inputSchema": tool["inputSchema"]
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
                filter_query = None
                if "team_id" in arguments:
                    filter_query = f"TeamMember_Team/ID eq {arguments['team_id']}"
                result = self.api.get_team_members(
                    filter_query=filter_query,
                    expand=arguments.get("expand", "TeamMember_Team,Account,FunctieRols")
                )
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
