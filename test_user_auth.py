#!/usr/bin/env python3
"""
Test user-based authentication in MCP Gateway.

This script tests the new userId-based credential lookup:
1. Creates a session with userId (instead of credentials)
2. Gateway should call get_token.py to retrieve credentials
3. Session should be created successfully
"""

import socket
import json
import sys

def send_jsonrpc(sock, method, params, msg_id=1):
    """Send JSON-RPC request and receive response."""
    request = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": msg_id
    }

    message = json.dumps(request) + "\n"
    print(f"→ Sending: {message.strip()}")
    sock.sendall(message.encode('utf-8'))

    # Receive response
    response_data = sock.recv(4096).decode('utf-8')
    print(f"← Received: {response_data.strip()}")

    return json.loads(response_data.strip())


def test_user_based_auth():
    """Test user-based authentication."""
    print("=" * 70)
    print("Testing User-Based Authentication")
    print("=" * 70)
    print()

    # Connect to MCP Gateway
    print("1. Connecting to MCP Gateway on localhost:8700...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(('localhost', 8700))
        print("   ✅ Connected!")
    except Exception as e:
        print(f"   ❌ Failed to connect: {e}")
        print("   Make sure MCP Gateway is running!")
        return 1

    print()

    # Test 1: Create session with userId (NEW METHOD)
    print("2. Creating session with userId='test@ns.nl'...")
    try:
        response = send_jsonrpc(sock, "mcp-manager/create-session", {
            "serverType": "azure",
            "userId": "test@ns.nl"
        })

        if "error" in response:
            print(f"   ❌ Error: {response['error']}")
            return 1

        session_id = response["result"]["sessionId"]
        print(f"   ✅ Session created: {session_id}")
    except Exception as e:
        print(f"   ❌ Exception: {e}")
        import traceback
        traceback.print_exc()
        return 1

    print()

    # Test 2: List sessions
    print("3. Listing active sessions...")
    try:
        response = send_jsonrpc(sock, "mcp-manager/list-sessions", {})
        sessions = response["result"]["sessions"]
        print(f"   ✅ Active sessions: {len(sessions)}")
        for sess in sessions:
            print(f"      - {sess['sessionId']}: {sess['serverType']}")
    except Exception as e:
        print(f"   ❌ Exception: {e}")
        return 1

    print()

    # Test 3: Destroy session
    print("4. Destroying session...")
    try:
        response = send_jsonrpc(sock, "mcp-manager/destroy-session", {
            "sessionId": session_id
        })
        print(f"   ✅ Session destroyed")
    except Exception as e:
        print(f"   ❌ Exception: {e}")
        return 1

    print()
    print("=" * 70)
    print("✅ All tests passed!")
    print("=" * 70)

    sock.close()
    return 0


def test_legacy_auth():
    """Test legacy credentials-based authentication (backward compatibility)."""
    print("=" * 70)
    print("Testing Legacy Credentials-Based Authentication")
    print("=" * 70)
    print()

    # Connect to MCP Gateway
    print("1. Connecting to MCP Gateway on localhost:8700...")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(('localhost', 8700))
        print("   ✅ Connected!")
    except Exception as e:
        print(f"   ❌ Failed to connect: {e}")
        return 1

    print()

    # Test: Create session with credentials (OLD METHOD)
    print("2. Creating session with credentials object (legacy)...")
    try:
        response = send_jsonrpc(sock, "mcp-manager/create-session", {
            "serverType": "azure",
            "credentials": {
                "pat": "test_legacy_token_123"
            }
        })

        if "error" in response:
            print(f"   ❌ Error: {response['error']}")
            return 1

        session_id = response["result"]["sessionId"]
        print(f"   ✅ Session created: {session_id}")
        print(f"   ⚠️  Legacy authentication still works (backward compatible)")
    except Exception as e:
        print(f"   ❌ Exception: {e}")
        return 1

    print()

    # Cleanup
    print("3. Destroying session...")
    try:
        response = send_jsonrpc(sock, "mcp-manager/destroy-session", {
            "sessionId": session_id
        })
        print(f"   ✅ Session destroyed")
    except Exception as e:
        print(f"   ❌ Exception: {e}")
        return 1

    print()
    print("=" * 70)
    print("✅ Legacy authentication test passed!")
    print("=" * 70)

    sock.close()
    return 0


if __name__ == "__main__":
    print()
    print("MCP Gateway User-Based Authentication Test")
    print()

    # Run both tests
    result1 = test_user_based_auth()
    print()
    result2 = test_legacy_auth()

    sys.exit(max(result1, result2))
