#!/usr/bin/env python3
"""ChatNS MCP Server.

Provides tools for interacting with ChatNS API:
- chat_completion: Chat with AI models
- semantic_search: Search knowledge buckets
- list_buckets: Get available data buckets
- health_check: Check service availability
"""
from __future__ import annotations

import asyncio
import json
import os
import sys
from pathlib import Path
from typing import List, Dict, Any, Optional

import requests
from mcp.server import Server
from mcp.types import Tool, TextContent

try:
    from shared.config import MCPServerConfig
except ImportError:
    # Fallback for when called from dashboard
    import sys
    from pathlib import Path
    sys.path.insert(0, str(Path(__file__).parent))
    from shared.config import MCPServerConfig

# Initialize server
server = Server("chatns-server")
config = MCPServerConfig.from_env()


class ChatNSAPIError(Exception):
    """ChatNS API error."""
    pass


def _get_headers() -> Dict[str, str]:
    """Get headers for ChatNS API requests."""
    headers = {
        "Content-Type": "application/json",
        "User-Agent": "Mozilla/5.0",  # Required by NS API Portal
    }

    # Add Bearer token if available
    if config.chatns_bearer:
        headers["Authorization"] = f"Bearer {config.chatns_bearer}"

    # Add APIM key (required)
    if config.chatns_apim:
        headers["Ocp-Apim-Subscription-Key"] = config.chatns_apim
    else:
        raise ChatNSAPIError("ChatNS APIM key not configured")

    return headers


def _post_json(url: str, data: dict, timeout: int = 60) -> dict:
    """Make authenticated POST request to ChatNS API."""
    headers = _get_headers()

    response = requests.post(
        url,
        headers=headers,
        json=data,
        timeout=timeout
    )

    if not response.ok:
        raise ChatNSAPIError(f"API request failed: {response.status_code} {response.text}")

    return response.json()


def _get_json(url: str, timeout: int = 30) -> dict:
    """Make authenticated GET request to ChatNS API."""
    headers = _get_headers()

    response = requests.get(
        url,
        headers=headers,
        timeout=timeout
    )

    if not response.ok:
        raise ChatNSAPIError(f"API request failed: {response.status_code} {response.text}")

    return response.json()


@server.list_tools()
async def list_tools() -> List[Tool]:
    """List available ChatNS tools."""
    return [
        Tool(
            name="chat_completion",
            description="Send a chat completion request to ChatNS AI models",
            inputSchema={
                "type": "object",
                "properties": {
                    "messages": {
                        "type": "array",
                        "description": "List of message objects with role and content",
                        "items": {
                            "type": "object",
                            "properties": {
                                "role": {"type": "string", "enum": ["system", "user", "assistant"]},
                                "content": {"type": "string"}
                            },
                            "required": ["role", "content"]
                        }
                    },
                    "model": {
                        "type": "string",
                        "description": "Model to use for completion",
                        "default": "gpt-4o"
                    },
                    "temperature": {
                        "type": "number",
                        "description": "Sampling temperature (0.0 to 1.0)",
                        "default": 0.7,
                        "minimum": 0.0,
                        "maximum": 1.0
                    },
                    "max_tokens": {
                        "type": "integer",
                        "description": "Maximum tokens to generate",
                        "default": 1000
                    }
                },
                "required": ["messages"]
            }
        ),
        Tool(
            name="semantic_search",
            description="Search knowledge buckets using semantic similarity",
            inputSchema={
                "type": "object",
                "properties": {
                    "prompt": {
                        "type": "string",
                        "description": "Search query text"
                    },
                    "bucket_id": {
                        "type": ["string", "integer"],
                        "description": "Bucket ID to search in"
                    },
                    "top_n": {
                        "type": "integer",
                        "description": "Number of results to return",
                        "default": 5,
                        "minimum": 1,
                        "maximum": 20
                    },
                    "min_cosine_similarity": {
                        "type": "number",
                        "description": "Minimum similarity score (0.0 to 1.0)",
                        "default": 0.75,
                        "minimum": 0.0,
                        "maximum": 1.0
                    }
                },
                "required": ["prompt", "bucket_id"]
            }
        ),
        Tool(
            name="list_buckets",
            description="List available knowledge buckets",
            inputSchema={
                "type": "object",
                "properties": {},
                "additionalProperties": False
            }
        ),
        Tool(
            name="health_check",
            description="Check ChatNS service health",
            inputSchema={
                "type": "object",
                "properties": {},
                "additionalProperties": False
            }
        )
    ]


@server.call_tool()
async def call_tool(name: str, arguments: dict) -> List[TextContent]:
    """Handle tool calls."""
    try:
        if name == "chat_completion":
            return await _chat_completion(arguments)
        elif name == "semantic_search":
            return await _semantic_search(arguments)
        elif name == "list_buckets":
            return await _list_buckets(arguments)
        elif name == "health_check":
            return await _health_check(arguments)
        else:
            raise ValueError(f"Unknown tool: {name}")
    except Exception as e:
        return [TextContent(type="text", text=f"Error calling ChatNS tool: {e}")]


async def _chat_completion(args: dict) -> List[TextContent]:
    """Handle chat completion requests."""
    messages = args["messages"]
    model = args.get("model", "gpt-4o")
    temperature = args.get("temperature", 0.7)
    max_tokens = args.get("max_tokens", 1000)

    payload = {
        "model": model,
        "messages": messages,
        "temperature": float(temperature),
        "max_tokens": int(max_tokens)
    }

    try:
        response = _post_json(config.chatns_api_url, payload)

        # Extract the response content
        content = ""
        if "choices" in response and response["choices"]:
            content = response["choices"][0].get("message", {}).get("content", "")

        result = {
            "status": "success",
            "response": content,
            "model": model,
            "usage": response.get("usage", {})
        }

        return [TextContent(type="text", text=json.dumps(result, indent=2))]

    except Exception as e:
        error_result = {
            "status": "error",
            "error": str(e),
            "model": model
        }
        return [TextContent(type="text", text=json.dumps(error_result, indent=2))]


async def _semantic_search(args: dict) -> List[TextContent]:
    """Handle semantic search requests."""
    prompt = args["prompt"]
    bucket_id = args["bucket_id"]
    top_n = args.get("top_n", 5)
    min_sim = args.get("min_cosine_similarity", 0.75)

    # Construct semantic search URL
    base_url = config.chatns_api_url.replace("/chat/completions", "")
    semantic_url = f"{base_url}/semantic_search"

    payload = {
        "prompt": prompt,
        "top_n": int(top_n),
        "bucket_id": int(bucket_id) if str(bucket_id).isdigit() else bucket_id,
        "min_cosine_similarity": float(min_sim)
    }

    try:
        response = _post_json(semantic_url, payload)

        # Handle different response formats
        results = response if isinstance(response, list) else response.get("results", [])

        result = {
            "status": "success",
            "bucket_id": bucket_id,
            "query": prompt,
            "results_count": len(results),
            "results": results
        }

        return [TextContent(type="text", text=json.dumps(result, indent=2))]

    except Exception as e:
        error_result = {
            "status": "error",
            "error": str(e),
            "bucket_id": bucket_id,
            "query": prompt
        }
        return [TextContent(type="text", text=json.dumps(error_result, indent=2))]


async def _list_buckets(args: dict) -> List[TextContent]:
    """List available knowledge buckets."""
    try:
        # This would need to be implemented based on ChatNS API
        # For now, return a placeholder
        result = {
            "status": "success",
            "buckets": [
                {"id": 1, "name": "General Knowledge", "description": "General purpose knowledge base"},
                {"id": 2, "name": "Technical Docs", "description": "Technical documentation and guides"}
            ],
            "note": "Bucket listing may need ChatNS API extension"
        }

        return [TextContent(type="text", text=json.dumps(result, indent=2))]

    except Exception as e:
        error_result = {
            "status": "error",
            "error": str(e)
        }
        return [TextContent(type="text", text=json.dumps(error_result, indent=2))]


async def _health_check(args: dict) -> List[TextContent]:
    """Check ChatNS service health."""
    try:
        # Simple health check - try to make a minimal request
        test_messages = [{"role": "user", "content": "Hello"}]
        payload = {
            "model": "gpt-4o",
            "messages": test_messages,
            "max_tokens": 5
        }

        response = _post_json(config.chatns_api_url, payload)

        result = {
            "status": "healthy",
            "service": "ChatNS",
            "api_url": config.chatns_api_url,
            "auth_configured": bool(config.chatns_apim),
            "bearer_configured": bool(config.chatns_bearer),
            "test_response": "OK"
        }

        return [TextContent(type="text", text=f"✅ ChatNS: {json.dumps(result, indent=2)}")]

    except Exception as e:
        result = {
            "status": "unhealthy",
            "service": "ChatNS",
            "error": str(e),
            "api_url": config.chatns_api_url,
            "auth_configured": bool(config.chatns_apim)
        }

        return [TextContent(type="text", text=f"❌ ChatNS: {json.dumps(result, indent=2)}")]


async def main():
    """Run the ChatNS MCP server."""
    from mcp.server.stdio import stdio_server

    async with stdio_server() as (read_stream, write_stream):
        await server.run(
            read_stream,
            write_stream,
            server.create_initialization_options()
        )


if __name__ == "__main__":
    asyncio.run(main())