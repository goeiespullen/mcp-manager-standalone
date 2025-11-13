# TeamCentraal MCP Server - Quick Start Guide

Snelle handleiding om te beginnen met de TeamCentraal MCP Server in MCP Manager.

## Inhoudsopgave

1. [Installatie](#installatie)
2. [Configuratie](#configuratie)
3. [Gebruik met Client](#gebruik-met-client)
4. [Veelgebruikte Tools](#veelgebruikte-tools)
5. [Voorbeelden](#voorbeelden)
6. [Troubleshooting](#troubleshooting)

---

## Installatie

### Stap 1: Dependencies Installeren

```bash
cd /path/to/mcp-manager-standalone/mcp_servers
pip install -r requirements.txt
```

### Stap 2: Credentials Aanvragen

**Email:** TeamCentraal@ns.nl

**Vermeld in je email:**
- Je naam
- Je team
- Waarvoor je de API wilt gebruiken
- Vraag om een DLO (Data Leveringsovereenkomst)

Je ontvangt:
- Username
- Password
- Toegang tot acceptatie omgeving (en eventueel productie)

### Stap 3: Environment Variables Instellen

**Linux/macOS:**
```bash
export TEAMCENTRAAL_USERNAME="jouw_username"
export TEAMCENTRAAL_PASSWORD="jouw_password"
export TEAMCENTRAAL_URL="https://teamcentraal-a.ns.nl/odata/POS_Odata_v4"
```

**Windows:**
```cmd
set TEAMCENTRAAL_USERNAME=jouw_username
set TEAMCENTRAAL_PASSWORD=jouw_password
set TEAMCENTRAAL_URL=https://teamcentraal-a.ns.nl/odata/POS_Odata_v4
```

**Of maak een .env bestand** (voeg toe aan .gitignore!):
```bash
TEAMCENTRAAL_USERNAME=jouw_username
TEAMCENTRAAL_PASSWORD=jouw_password
TEAMCENTRAAL_URL=https://teamcentraal-a.ns.nl/odata/POS_Odata_v4
```

---

## Configuratie

### Server is al Geconfigureerd!

De TeamCentraal server is al toegevoegd aan `configs/servers.json`:

```json
{
  "name": "TeamCentraal",
  "type": "python",
  "command": "/usr/bin/python3",
  "arguments": ["mcp_servers/teamcentraal_server.py"],
  "port": 8770,
  "workingDir": "/home/laptop/MEGA/development/chatns/mcp-manager-standalone",
  "env": {
    "TEAMCENTRAAL_URL": "https://teamcentraal-a.ns.nl/odata/POS_Odata_v4",
    "TEAMCENTRAAL_USERNAME": "",
    "TEAMCENTRAAL_PASSWORD": ""
  }
}
```

**⚠️ Credentials worden via environment variables ingesteld, NIET in servers.json!**

### Server Starten

1. **Open MCP Manager GUI**
   ```bash
   ./run.sh
   ```

2. **Herlaad configuratie** (als je wijzigingen hebt gemaakt)
   - File → Reload Config

3. **Start TeamCentraal server**
   - Vind "TeamCentraal" in de server lijst
   - Klik op "Start" knop
   - Status wordt groen: "Running"

4. **Verifieer in logs**
   - Ga naar "Logs" tab
   - Filter op "TeamCentraal"
   - Je zou moeten zien: "TeamCentraal MCP Server starting..."

---

## Gebruik met Client

### Python Client Voorbeeld

```python
from mcp_client.mcp_manager_client import MCPManagerClient

# Connect to MCP Manager Gateway
with MCPManagerClient(host='localhost', port=8700) as client:
    # Create session with TeamCentraal
    session = client.create_session('TeamCentraal', {
        'TEAMCENTRAAL_USERNAME': 'jouw_username',
        'TEAMCENTRAAL_PASSWORD': 'jouw_password',
        'TEAMCENTRAAL_URL': 'https://teamcentraal-a.ns.nl/odata/POS_Odata_v4'
    })

    print(f"Session created: {session.session_id}")

    # List all tools
    tools = client.list_tools(session.session_id)
    print(f"Available tools: {[t['name'] for t in tools]}")

    # Search for a team
    result = client.call_tool(
        session.session_id,
        'search_teams',
        {'name': 'Platform'}
    )
    print(f"Found teams: {result}")

    # Cleanup
    client.destroy_session(session.session_id)
```

### JSON-RPC Direct Voorbeeld

```python
import socket
import json

# Connect to gateway
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('localhost', 8700))

# Create session
request = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "mcp-manager/create-session",
    "params": {
        "serverType": "TeamCentraal",
        "credentials": {
            "TEAMCENTRAAL_USERNAME": "jouw_username",
            "TEAMCENTRAAL_PASSWORD": "jouw_password"
        }
    }
}

sock.sendall(json.dumps(request).encode() + b'\n')
response = json.loads(sock.recv(4096).decode())
session_id = response['result']['sessionId']

print(f"Session: {session_id}")

# Call search_teams tool
request = {
    "jsonrpc": "2.0",
    "id": 2,
    "method": "tools/call",
    "params": {
        "sessionId": session_id,
        "name": "search_teams",
        "arguments": {"name": "Platform"}
    }
}

sock.sendall(json.dumps(request).encode() + b'\n')
response = json.loads(sock.recv(8192).decode())
print(response['result'])

sock.close()
```

---

## Veelgebruikte Tools

### 1. `search_teams` - Zoek Teams op Naam

**Use case:** Vind teams die aan een bepaald project werken

```python
result = client.call_tool(
    session_id,
    'search_teams',
    {'name': 'Platform'}
)
```

**Output:**
```json
{
  "value": [
    {
      "ID": "79375943432407120",
      "Name": "Platform Engineering",
      "TeamCategory": "Development",
      "AzureDevOpsKey": "PLATFORM",
      "JiraKey": "PLAT"
    }
  ]
}
```

---

### 2. `list_teams` - Alle Teams met Filters

**Use case:** Haal alle development teams op

```python
result = client.call_tool(
    session_id,
    'list_teams',
    {
        'filter': "TeamCategory eq 'Development'",
        'select': 'ID,Name,TeamCategory,AzureDevOpsKey',
        'top': 20
    }
)
```

---

### 3. `get_team` - Specifiek Team met Details

**Use case:** Haal volledige team informatie op inclusief department

```python
result = client.call_tool(
    session_id,
    'get_team',
    {
        'team_id': '79375943432407120',
        'expand': 'Team_Department'
    }
)
```

---

### 4. `list_team_members` - Teamleden met Rollen

**Use case:** Zie wie er in een team zit en wat hun rol is

```python
result = client.call_tool(
    session_id,
    'list_team_members',
    {
        'team_id': '79375943432407120',
        'expand': 'TeamMember_Team,Account,FunctieRols'
    }
)
```

**Output bevat:**
- Account informatie (naam, email)
- Functierol (Product Owner, Scrum Master, Developer, etc.)
- Team referentie

---

### 5. `get_azure_devops_teams` - Teams met Azure DevOps

**Use case:** Vind alle teams die Azure DevOps gebruiken

```python
result = client.call_tool(
    session_id,
    'get_azure_devops_teams',
    {}
)
```

---

### 6. `get_team_by_azdo_key` - Zoek via Azure DevOps Key

**Use case:** Je hebt een Azure DevOps project en wilt het bijbehorende team vinden

```python
result = client.call_tool(
    session_id,
    'get_team_by_azdo_key',
    {'azdo_key': 'PLATFORM'}
)
```

---

### 7. `list_departments` - Organisatiestructuur

**Use case:** Zie alle Resultaatgebieden, Clusters en Domeinen

```python
result = client.call_tool(
    session_id,
    'list_departments',
    {'expand': 'Rols'}
)
```

---

### 8. `list_responsible_applications` - Applicatie Verantwoordelijkheden

**Use case:** Zie welke applicaties een team beheert (ServiceNow SYSID)

```python
result = client.call_tool(
    session_id,
    'list_responsible_applications',
    {'team_id': '79375943432407120'}
)
```

---

### 9. `list_dora_metings` - DORA Metingen

**Use case:** Haal DORA metingen op voor team effectiviteit

```python
result = client.call_tool(
    session_id,
    'list_dora_metings',
    {'expand': 'DoraAnswers'}
)
```

---

## Voorbeelden

### Voorbeeld 1: Vind Team en Teamleden

```python
from mcp_client.mcp_manager_client import MCPManagerClient

with MCPManagerClient() as client:
    session = client.create_session('TeamCentraal', {
        'TEAMCENTRAAL_USERNAME': 'user',
        'TEAMCENTRAAL_PASSWORD': 'pass'
    })

    # Zoek team
    teams = client.call_tool(
        session.session_id,
        'search_teams',
        {'name': 'Platform'}
    )

    if teams['value']:
        team = teams['value'][0]
        team_id = team['ID']
        print(f"Found team: {team['Name']}")

        # Haal teamleden op
        members = client.call_tool(
            session.session_id,
            'list_team_members',
            {
                'team_id': team_id,
                'expand': 'Account,FunctieRols'
            }
        )

        print(f"\nTeam members ({len(members['value'])}):")
        for member in members['value']:
            account = member.get('Account', {})
            role = member.get('FunctieRols', {})
            print(f"  - {account.get('Name')} ({role.get('Name')})")

    client.destroy_session(session.session_id)
```

---

### Voorbeeld 2: Azure DevOps Koppeling Vinden

```python
from mcp_client.mcp_manager_client import MCPManagerClient

with MCPManagerClient() as client:
    session = client.create_session('TeamCentraal', {...})

    # Vind team via Azure DevOps key
    result = client.call_tool(
        session.session_id,
        'get_team_by_azdo_key',
        {'azdo_key': 'PLATFORM'}
    )

    if result['value']:
        team = result['value'][0]
        print(f"Team: {team['Name']}")
        print(f"Azure DevOps: {team.get('AzureDevOpsKey')}")
        print(f"Jira: {team.get('JiraKey')}")

        # Department info
        dept = team.get('Team_Department', {})
        print(f"Department: {dept.get('Name')}")

    client.destroy_session(session.session_id)
```

---

### Voorbeeld 3: Organisatiestructuur Ophalen

```python
from mcp_client.mcp_manager_client import MCPManagerClient

with MCPManagerClient() as client:
    session = client.create_session('TeamCentraal', {...})

    # Haal alle departments op
    depts = client.call_tool(
        session.session_id,
        'list_departments',
        {'expand': 'Rols'}
    )

    print("Organizational Structure:")
    for dept in depts['value']:
        print(f"\n{dept['Name']}")

        # Rollen in dit department
        rols = dept.get('Rols', [])
        if rols:
            print("  Roles:")
            for rol in rols:
                print(f"    - {rol.get('Name')}")

        # Teams in dit department
        teams = client.call_tool(
            session.session_id,
            'list_teams',
            {
                'filter': f"Team_Department/ID eq '{dept['ID']}'",
                'select': 'ID,Name'
            }
        )

        if teams['value']:
            print("  Teams:")
            for team in teams['value']:
                print(f"    - {team['Name']}")

    client.destroy_session(session.session_id)
```

---

## Troubleshooting

### ❌ Error: "TeamCentraal credentials niet geconfigureerd"

**Oorzaak:** Environment variables zijn niet ingesteld

**Oplossing:**
```bash
# Check of variables zijn ingesteld
echo $TEAMCENTRAAL_USERNAME
echo $TEAMCENTRAAL_PASSWORD

# Als leeg, stel opnieuw in
export TEAMCENTRAAL_USERNAME="jouw_username"
export TEAMCENTRAAL_PASSWORD="jouw_password"

# Herstart MCP Manager
```

---

### ❌ Error: "401 Unauthorized"

**Oorzaak:** Verkeerde credentials of DLO niet getekend

**Oplossing:**
1. Verifieer credentials met API tester tool
2. Check of DLO is getekend
3. Vraag nieuwe credentials aan bij TeamCentraal@ns.nl

---

### ❌ Error: "Session creation failed"

**Oorzaak:** Server is niet gestart of configuratie klopt niet

**Oplossing:**
1. Check of TeamCentraal server running is in MCP Manager GUI
2. Kijk in logs tab voor error messages
3. Verifieer `configs/servers.json` configuratie
4. Test server handmatig:
   ```bash
   export TEAMCENTRAAL_USERNAME="user"
   export TEAMCENTRAAL_PASSWORD="pass"
   python3 mcp_servers/teamcentraal_server.py
   ```

---

### ❌ Lege resultaten

**Mogelijke oorzaken:**
1. Filter query matcht niets
2. Team bestaat niet
3. Verkeerde ID gebruikt

**Oplossing:**
1. Test zonder filter eerst: `list_teams` zonder parameters
2. Gebruik `search_teams` om team ID te vinden
3. Verifieer team ID format (lange numerieke string)

---

### ⚠️ Acceptatie vs Productie

**Standaard:** Acceptatie omgeving wordt gebruikt

**Voor Productie:**
```bash
export TEAMCENTRAAL_URL="https://teamcentraal.ns.nl/odata/POS_Odata_v4"
```

**Let op:**
- Test ALTIJD eerst in acceptatie
- Productie credentials kunnen anders zijn
- Data kan verschillen tussen omgevingen

---

## Meer Informatie

- **Complete API Reference:** `docs/TEAMCENTRAAL_API_GUIDE.md`
- **Server Documentatie:** `mcp_servers/README.md`
- **MCP Manager INSTALL:** `INSTALL.md`
- **Support:** TeamCentraal@ns.nl

---

## Tips & Best Practices

### 1. Gebruik `$select` om responses klein te houden

```python
# ❌ Haalt alle velden op (groot!)
result = client.call_tool(session_id, 'list_teams', {})

# ✅ Haalt alleen benodigde velden op (klein!)
result = client.call_tool(
    session_id,
    'list_teams',
    {'select': 'ID,Name,TeamCategory'}
)
```

### 2. Gebruik `$top` voor paginering

```python
# Haal eerste 50 teams
result = client.call_tool(
    session_id,
    'list_teams',
    {'top': 50}
)

# Haal volgende 50 teams
result = client.call_tool(
    session_id,
    'list_teams',
    {'top': 50, 'skip': 50}
)
```

### 3. Cache data waar mogelijk

TeamCentraal data wijzigt niet elk uur, dus:
- Cache team lists
- Cache organizational structure
- Refresh periodiek (bijv. dagelijks)

### 4. Gebruik expand voor related data

```python
# ❌ 2 API calls
team = client.call_tool(session_id, 'get_team', {'team_id': id})
dept = client.call_tool(session_id, 'get_department', {'id': team['dept_id']})

# ✅ 1 API call met expand
team = client.call_tool(
    session_id,
    'get_team',
    {'team_id': id, 'expand': 'Team_Department'}
)
# team['Team_Department'] bevat nu department info
```

### 5. Error handling implementeren

```python
try:
    result = client.call_tool(session_id, 'search_teams', {'name': 'X'})
    if result['value']:
        # Process results
        pass
    else:
        print("No teams found")
except Exception as e:
    print(f"Error: {e}")
    # Implement retry logic
```

---

**Veel success met de TeamCentraal MCP Server!** 🚀
