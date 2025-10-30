#!/usr/bin/env python3
"""
Demo MCP Server - A minimal but CORRECT implementation of the MCP protocol.

This demonstrates that ANY MCP server following the protocol will work with our gateway.
This is the same protocol used by ALL MCP servers from GitHub!
"""

import sys
import json
import logging
from typing import Any, Dict

# Setup logging to stderr (stdout is used for MCP protocol)
logging.basicConfig(
    level=logging.DEBUG,
    stream=sys.stderr,
    format='[Demo MCP] %(message)s'
)
logger = logging.getLogger(__name__)


class DemoMCPServer:
    """
    Minimal MCP Protocol Server

    This implements the EXACT same protocol as:
    - mcp-atlassian (Confluence/Jira)
    - @modelcontextprotocol/server-github
    - @modelcontextprotocol/server-filesystem
    - And ANY other MCP server from GitHub!
    """

    def __init__(self):
        self.tools = {
            "echo": {
                "description": "Echo back a message",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "message": {"type": "string", "description": "Message to echo"}
                    },
                    "required": ["message"]
                }
            },
            "add": {
                "description": "Add two numbers",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "a": {"type": "number"},
                        "b": {"type": "number"}
                    },
                    "required": ["a", "b"]
                }
            },
            "get_env": {
                "description": "Get an environment variable (demonstrates credential injection)",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "name": {"type": "string", "description": "Environment variable name"}
                    },
                    "required": ["name"]
                }
            }
        }

    def handle_initialize(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle initialization request"""
        logger.info("Received initialize request")
        return {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "tools": {}
            },
            "serverInfo": {
                "name": "demo-mcp-server",
                "version": "1.0.0"
            }
        }

    def handle_tools_list(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """List available tools"""
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
        """Call a tool"""
        tool_name = params.get("name")
        arguments = params.get("arguments", {})

        logger.info(f"Calling tool: {tool_name} with args: {arguments}")

        if tool_name == "echo":
            message = arguments.get("message", "")
            return {
                "content": [
                    {
                        "type": "text",
                        "text": f"Echo: {message}"
                    }
                ]
            }

        elif tool_name == "add":
            a = arguments.get("a", 0)
            b = arguments.get("b", 0)
            result = a + b
            return {
                "content": [
                    {
                        "type": "text",
                        "text": f"{a} + {b} = {result}"
                    }
                ]
            }

        elif tool_name == "get_env":
            import os
            var_name = arguments.get("name", "")
            value = os.environ.get(var_name, "(not set)")
            return {
                "content": [
                    {
                        "type": "text",
                        "text": f"{var_name} = {value}"
                    }
                ]
            }

        else:
            raise ValueError(f"Unknown tool: {tool_name}")

    def handle_request(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Handle a JSON-RPC request"""
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
        """Main server loop - read from stdin, write to stdout"""
        logger.info("Demo MCP Server starting...")
        logger.info("This server implements the STANDARD MCP protocol")
        logger.info("It will work with ANY MCP client, including our gateway!")

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
                    logger.debug(f"Sent response: {response}")

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
    server = DemoMCPServer()
    server.run()
