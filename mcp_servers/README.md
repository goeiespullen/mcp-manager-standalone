# TeamCentraal MCP Server

MCP Server voor toegang tot de NS TeamCentraal OData V4 API.

## Functionaliteit

Deze MCP server biedt tools voor:

- **Teams**: Ophalen, zoeken en filteren van teams
- **Teamleden**: Teamleden met rollen en account informatie
- **Organisatie**: Departments, Clusters en Resultaatgebieden
- **Koppelingen**: Azure DevOps en Jira project keys
- **Metingen**: DORA metingen en effectiviteit scores
- **Applicaties**: Verantwoordelijke applicaties (ServiceNow SYSID)

## Installatie

### 1. Vereisten

```bash
# Python 3.8 of hoger
python3 --version

# Installeer dependencies
pip install -r requirements.txt
```

### 2. Credentials Aanvragen

Credentials moeten aangevraagd worden bij het TeamCentraal team:

- **Email**: TeamCentraal@ns.nl
- **Vermeld**: Jouw naam, team en gebruiksdoel
- **DLO**: Data Leveringsovereenkomst moet getekend worden

### 3. Environment Variables

Stel de volgende environment variables in:

```bash
export TEAMCENTRAAL_USERNAME="jouw_username"
export TEAMCENTRAAL_PASSWORD="jouw_password"

# Optioneel: wijzig base URL (default = acceptatie)
export TEAMCENTRAAL_URL="https://teamcentraal-a.ns.nl/odata/POS_Odata_v4"

# Voor productie:
# export TEAMCENTRAAL_URL="https://teamcentraal.ns.nl/odata/POS_Odata_v4"
```

### 4. MCP Manager Configuratie

Voeg de server toe aan `configs/servers.json`:

```json
{
  "name": "TeamCentraal",
  "type": "python",
  "command": "/usr/bin/python3",
  "arguments": ["mcp_servers/teamcentraal_server.py"],
  "port": 8770,
  "workingDir": "/home/laptop/MEGA/development/chatns/mcp-manager-standalone",
  "autostart": false,
  "healthCheckInterval": 30000,
  "env": {
    "TEAMCENTRAAL_USERNAME": "jouw_username",
    "TEAMCENTRAAL_PASSWORD": "jouw_password",
    "TEAMCENTRAAL_URL": "https://teamcentraal-a.ns.nl/odata/POS_Odata_v4"
  }
}
```

**⚠️ BELANGRIJK**:
- Plaats NOOIT credentials in Git repositories
- Gebruik environment variables of een secrets manager
- Voor productie gebruik: externe credential injection

### 5. Server Starten

Via MCP Manager GUI:
1. Herlaad configuratie (File → Reload Config)
2. Klik op "Start" bij TeamCentraal server
3. Status wordt "Running" met groene indicator

Via command line (testen):
```bash
export TEAMCENTRAAL_USERNAME="username"
export TEAMCENTRAAL_PASSWORD="password"
python3 mcp_servers/teamcentraal_server.py
```

## Beschikbare Tools

### 1. `list_teams`
Haal alle teams op met optionele filtering en paginering.

**Parameters:**
- `filter` (optioneel): OData filter query (bijv. `"TeamCategory eq 'Development'"`)
- `select` (optioneel): Komma-gescheiden veldnamen (bijv. `"ID,Name,TeamCategory"`)
- `expand` (optioneel): Gerelateerde data (bijv. `"Team_Department"`)
- `top` (optioneel): Maximaal aantal resultaten
- `skip` (optioneel): Aantal resultaten overslaan (paginering)

**Voorbeeld:**
```json
{
  "filter": "TeamCategory eq 'Development'",
  "select": "ID,Name,TeamCategory,AzureDevOpsKey",
  "top": 20
}
```

### 2. `get_team`
Haal een specifiek team op via ID.

**Parameters:**
- `team_id` (verplicht): Team ID uit TeamCentraal
- `expand` (optioneel): Gerelateerde data (bijv. `"Team_Department"`)

### 3. `search_teams`
Zoek teams op naam (case-insensitive contains match).

**Parameters:**
- `name` (verplicht): Zoekterm voor teamnaam

**Voorbeeld:**
```json
{
  "name": "Platform"
}
```

### 4. `list_team_members`
Haal teamleden op, optioneel gefilterd op team.

**Parameters:**
- `team_id` (optioneel): Filter op specifiek team ID
- `expand` (optioneel): Gerelateerde data (default: `"TeamMember_Team,Account,FunctieRols"`)

### 5. `list_departments`
Haal departments op (RG's, Clusters, Domeinen).

**Parameters:**
- `filter` (optioneel): OData filter query
- `expand` (optioneel): Gerelateerde data (bijv. `"Rols"`)

### 6. `get_azure_devops_teams`
Haal alle teams op die gekoppeld zijn aan Azure DevOps.

**Parameters:** Geen

### 7. `get_jira_teams`
Haal alle teams op die gekoppeld zijn aan Jira.

**Parameters:** Geen

### 8. `get_team_by_azdo_key`
Zoek team op Azure DevOps project key.

**Parameters:**
- `azdo_key` (verplicht): Azure DevOps project key (bijv. `"PLATFORM"`)

### 9. `list_responsible_applications`
Haal verantwoordelijke applicaties op (ServiceNow SYSID koppeling).

**Parameters:**
- `team_id` (optioneel): Filter op specifiek team ID

### 10. `list_dora_metings`
Haal DORA metingen op.

**Parameters:**
- `expand` (optioneel): Gerelateerde data (bijv. `"DoraAnswers"`)

## OData Query Voorbeelden

### Filter Operators

```bash
# Equals
TeamCategory eq 'Development'

# Not equals
AzureDevOpsKey ne null

# Contains (case-sensitive!)
contains(Name, 'Platform')

# Logical AND
TeamCategory eq 'Development' and AzureDevOpsKey ne null

# Logical OR
TeamCategory eq 'Development' or TeamCategory eq 'Operations'
```

### Expand (Gerelateerde Data)

```bash
# Enkel niveau
Team_Department

# Meerdere relaties
TeamMember_Team,Account,FunctieRols

# Multi-level
Team_Department($expand=Rols)
```

### Select (Specifieke Velden)

```bash
# Minimale velden
ID,Name

# Met categorie en keys
ID,Name,TeamCategory,AzureDevOpsKey,JiraKey
```

## Troubleshooting

### ❌ Credentials niet geconfigureerd

**Error:** "TeamCentraal credentials niet geconfigureerd"

**Oplossing:**
1. Check environment variables: `echo $TEAMCENTRAAL_USERNAME`
2. Herstart MCP Manager na het instellen van env vars
3. Check `configs/servers.json` voor juiste env configuratie

### ❌ 401 Unauthorized

**Oorzaken:**
- Verkeerde username/password
- Credentials verlopen
- DLO niet getekend

**Oplossing:**
1. Verifieer credentials met API tester
2. Vraag nieuwe credentials aan bij TeamCentraal@ns.nl
3. Check DLO status

### ❌ 404 Not Found

**Oorzaken:**
- Verkeerde endpoint (V3 vs V4)
- Team ID bestaat niet
- Typo in endpoint naam

**Oplossing:**
- Gebruik altijd V4 API: `/odata/POS_Odata_v4/`
- Verifieer team ID via `list_teams` eerst

### ⚠️ Lege resultaten

**Mogelijke oorzaken:**
- Filter query matcht geen resultaten
- Expand relatie bestaat niet voor dit record
- Team heeft geen leden/koppelingen

**Check:**
- Test query zonder filter eerst
- Check `$metadata` endpoint voor beschikbare relaties
- Gebruik `$expand=RelationName($select=ID)` om te testen

## API Referentie

### Base URLs

| Omgeving | URL |
|----------|-----|
| **Acceptatie** | `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/` |
| **Productie** | `https://teamcentraal.ns.nl/odata/POS_Odata_v4/` |

### Rate Limits

Geen officiële rate limits gedocumenteerd.

**Aanbevolen:**
- Max 100 requests per minuut
- Gebruik paginering met `$top` en `$skip`
- Cache responses waar mogelijk

## Support

- **TeamCentraal**: TeamCentraal@ns.nl
- **API Documentatie**: Zie `docs/TEAMCENTRAAL_API_GUIDE.md`
- **MCP Manager Issues**: GitHub issues in mcp-manager repository

## Best Practices

1. **Test eerst in Acceptatie**: Gebruik acceptatie omgeving voor development
2. **Gebruik $select**: Beperk response size door alleen benodigde velden op te halen
3. **Limiteer resultaten**: Gebruik `$top` om grote datasets te beperken
4. **Cache data**: TeamCentraal data wijzigt niet continu, cache waar mogelijk
5. **Error handling**: Implementeer retry logic voor tijdelijke netwerk issues

## Licentie

Internal NS use only - Conform Data Leveringsovereenkomst (DLO)
