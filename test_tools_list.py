#!/usr/bin/env python3
"""
Test script to demonstrate tools/list functionality via MCP Manager Gateway.
Shows how the dashboard can dynamically discover available tools.
"""
import socket
import json

def send_request(sock, request):
    """Send JSON-RPC request and receive response."""
    message = json.dumps(request) + "\n"
    sock.sendall(message.encode())

    # Read response
    response = ""
    while True:
        chunk = sock.recv(4096).decode()
        response += chunk
        if "\n" in response:
            break

    return json.loads(response.strip())

def main():
    # Connect to gateway
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("localhost", 8700))

    print("✅ Connected to MCP Manager Gateway on port 8700\n")

    try:
        # Step 1: List available servers
        print("📋 Step 1: List available servers")
        response = send_request(sock, {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "mcp-manager/list-servers",
            "params": {}
        })

        servers = response.get("result", {}).get("servers", [])
        print(f"Found {len(servers)} servers:")
        for server in servers:
            print(f"  - {server['name']} ({server['type']}) - Status: {server['status']}")
        print()

        # Step 2: Create a session for Azure DevOps
        print("🔧 Step 2: Create session for Azure DevOps")
        response = send_request(sock, {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "mcp-manager/create-session",
            "params": {
                "serverType": "Azure DevOps",
                "credentials": {
                    "AZDO_PAT": "your-pat-token-here"  # Will be mapped to ADO_MCP_AUTH_TOKEN
                }
            }
        })

        session_id = response.get("result", {}).get("sessionId")
        print(f"✅ Session created: {session_id}\n")

        # Step 3: Get tools list dynamically!
        print("🔍 Step 3: Discover available tools dynamically")
        response = send_request(sock, {
            "jsonrpc": "2.0",
            "id": 3,
            "method": "tools/list",
            "params": {
                "sessionId": session_id
            }
        })

        if "result" in response:
            tools = response["result"].get("tools", [])
            print(f"📊 Found {len(tools)} tools!\n")

            # Group tools by domain (prefix)
            domains = {}
            for tool in tools:
                name = tool.get("name", "")
                domain = name.split("_")[0] if "_" in name else "other"
                if domain not in domains:
                    domains[domain] = []
                domains[domain].append(tool)

            # Display tools grouped by domain
            for domain in sorted(domains.keys()):
                print(f"\n🏷️  {domain.upper()} Domain ({len(domains[domain])} tools):")
                for tool in domains[domain][:5]:  # Show first 5 of each domain
                    print(f"   • {tool['name']}")
                    print(f"     {tool.get('description', 'No description')[:80]}...")
                if len(domains[domain]) > 5:
                    print(f"   ... and {len(domains[domain]) - 5} more")

            # Show iteration-related tools specifically
            print("\n🔄 Sprint/Iteration Tools:")
            iteration_tools = [t for t in tools if "iteration" in t.get("name", "").lower()]
            for tool in iteration_tools:
                print(f"   • {tool['name']}")
                print(f"     {tool.get('description', '')}")

        else:
            print(f"❌ Error: {response.get('error', {}).get('message', 'Unknown error')}")

        print("\n✨ Dashboard kan nu:")
        print("   1. Dynamisch alle tools ontdekken via tools/list")
        print("   2. Tool namen, descriptions en schemas ophalen")
        print("   3. Automatisch UI genereren voor beschikbare tools")
        print("   4. Geen hardcoded tool namen meer nodig!")

    finally:
        sock.close()

if __name__ == "__main__":
    main()
