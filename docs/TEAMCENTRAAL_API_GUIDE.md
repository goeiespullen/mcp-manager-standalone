# TeamCentraal OData REST API - Praktische Handleiding

Deze handleiding bevat concrete voorbeelden voor het gebruik van de TeamCentraal OData V4 REST API.

## 📋 Inhoudsopgave

1. [Overzicht](#overzicht)
2. [Authenticatie](#authenticatie)
3. [Endpoints](#endpoints)
4. [Basis API Calls](#basis-api-calls)
5. [Geavanceerde Queries](#geavanceerde-queries)
6. [OData Query Opties](#odata-query-opties)
7. [Troubleshooting](#troubleshooting)

---

## Overzicht

TeamCentraal is de centrale teamregistratie van NS. Via de OData V4 REST API kun je:
- Team informatie ophalen (teamleden, beschrijving, enz.)
- Organisatiestructuur raadplegen (Departments, Clusters, Resultaatgebieden)
- Koppelingen met Azure DevOps en Jira ophalen
- DORA metingen en effectiviteit scores raadplegen

**⚠️ Belangrijke updates:**
- **OData V3 is uitgefaseerd** per 18 juni 2025
- **DevOps Playbook** is niet meer actief per 18 juni 2025

---

## Authenticatie

### Credentials

TeamCentraal gebruikt **HTTP Basic Authentication**.

**Credentials aanvragen:**
- Email: TeamCentraal@ns.nl
- Vermeld: Jouw naam, team, gebruik

**DLO (Data Leveringsovereenkomst):**
- Vereist voor gebruik van de API
- Regelt afspraken over datagebru
ik
- Bij niet naleven kan toegang worden geblokkeerd

### API Tester Configuratie

**Organization:** `teamcentraal-a.ns.nl` (Acceptatie) of `teamcentraal.ns.nl` (Productie)
**Username:** (ontvangen van TeamCentraal)
**Password:** (ontvangen van TeamCentraal)

---

## Endpoints

### Base URLs

| Omgeving | Base URL |
|----------|----------|
| **Acceptatie** | `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/` |
| **Productie** | `https://teamcentraal.ns.nl/odata/POS_Odata_v4/` |

### Beschikbare Endpoints

| Endpoint | Beschrijving | Status |
|----------|--------------|--------|
| `/$metadata` | Schema informatie | ✅ Primair |
| `/Teams` | Team informatie | ✅ Primair |
| `/TeamMembers` | Teamleden | ✅ Primair |
| `/Departments` | RG's, Clusters, Domeinen | ✅ Primair |
| `/Accounts` | Account informatie | ✅ Primair |
| `/FunctieRols` | Functierollen | ✅ Primair |
| `/Rols` | COM rollen | ✅ Primair |
| `/Betrokkenens` | Betrokkenen (handmatig) | ✅ Primair |
| `/Gildes` | Gildes | ✅ Primair |
| `/GildeMembers` | Gildeleden | ✅ Primair |
| `/ResponsibleApplications` | App verantwoordelijkheid | ✅ Primair |
| `/WorkingOnApplications` | Teams die aan app werken | ✅ Primair |
| `/Effectivities` | Effectiviteitsmetingen | ✅ Primair |
| `/DoraMetings` | DORA metingen | ✅ Primair |
| `/Assessments` | Assessments | ⚠️ Test nodig |
| `/MaturityScores` | Maturity scores | ⚠️ Test nodig |

---

## Basis API Calls

### 1. Schema Informatie Ophalen

**Doel:** Ontdek alle beschikbare velden en relaties

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/$metadata`

**Gebruik:**
- Zie welke velden beschikbaar zijn
- Ontdek navigatie properties (relaties)
- Begrijp datatypen

---

### 2. Alle Teams Ophalen

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Teams`

**Response bevat:**
- Team ID
- Team naam
- Team soort (via `TeamCategory`, NIET `TeamType`)
- Azure DevOps key
- Jira key
- Department referentie

**Voorbeeld response:**
```json
{
  "value": [
    {
      "ID": "79375943432407120",
      "Name": "Platform Engineering",
      "TeamCategory": "Development",
      "AzureDevOpsKey": "PLATFORM",
      "JiraKey": "PLAT",
      "Team_Department": {
        "ID": "12345",
        "Name": "Cluster Digital"
      }
    }
  ]
}
```

---

### 3. Specifiek Team Ophalen

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Teams(79375943432407120)`

**Let op:** ID tussen haakjes, geen quotes!

---

### 4. Teamleden Ophalen

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/TeamMembers`

**Met expand voor meer info:**
```
https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/TeamMembers?$expand=TeamMember_Team,Account,FunctieRols
```

**Response bevat:**
- Team referentie
- Account informatie (naam, email)
- Functierol (rol binnen team)

---

### 5. Departments (Organisatie Structuur)

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Departments`

**Bevat:**
- Resultaatgebieden
- Clusters
- Domeinen

**Specifiek department:**
```
https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Departments(79375943432407120)
```

---

### 6. Rollen van een Department

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Departments(79375943432407120)/Rols`

**Gebruik:**
- Zie wie welke rol heeft in een Cluster/RG
- Bijvoorbeeld: PO, SM, Engineering Manager

---

### 7. Betrokkenen bij Department

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Betrokkenens?$expand=ResultaatGebied,DepartmentFinal,Rol`

**Verschil met TeamMembers:**
- **Betrokkenen:** Handmatig ingevoerd (via Azure Graph API)
- **TeamMembers:** Afkomstig uit ServiceNow

---

### 8. Azure DevOps / Jira Koppelingen

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/Teams?$filter=AzureDevOpsKey ne null`

**Filter op Jira:**
```
/Teams?$filter=JiraKey ne null
```

---

### 9. Verantwoordelijke Applicaties

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/ResponsibleApplications`

**Koppeling:**
- SYSID uit ServiceNow
- Functional ID uit ServiceNow
- Team nummer uit TeamCentraal

---

### 10. DORA Metingen

**Method:** GET
**URL:** `https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/DoraMetings`

**Met expand voor details:**
```
/DoraMetings?$expand=DoraAnswers
```

---

## Geavanceerde Queries

### Filter Queries

**Teams van een specifiek type:**
```
/Teams?$filter=TeamCategory eq 'Development'
```

**Teams in een specifiek department:**
```
/Teams?$filter=Team_Department/ID eq '12345'
```

**Teams met Azure DevOps koppeling:**
```
/Teams?$filter=AzureDevOpsKey ne null
```

### Expand (Gerelateerde Data)

**Team met department info:**
```
/Teams?$expand=Team_Department
```

**Teamleden met alle details:**
```
/TeamMembers?$expand=TeamMember_Team,Account,FunctieRols
```

**Multi-level expand:**
```
/Teams?$expand=Team_Department($expand=Rols)
```

### Select (Specifieke Velden)

**Alleen naam en ID:**
```
/Teams?$select=ID,Name
```

**Gecombineerd met filter:**
```
/Teams?$select=ID,Name,TeamCategory&$filter=TeamCategory eq 'Development'
```

### OrderBy (Sortering)

**Alfabetisch op naam:**
```
/Teams?$orderby=Name
```

**Omgekeerd:**
```
/Teams?$orderby=Name desc
```

### Top & Skip (Paginering)

**Eerste 10 teams:**
```
/Teams?$top=10
```

**Skip eerste 10, haal volgende 10:**
```
/Teams?$skip=10&$top=10
```

### Count

**Aantal teams:**
```
/Teams/$count
```

**Met filter:**
```
/Teams/$count?$filter=TeamCategory eq 'Development'
```

---

## OData Query Opties

### Overzicht

| Optie | Syntax | Beschrijving | Voorbeeld |
|-------|--------|--------------|-----------|
| **$filter** | `?$filter=...` | Filter resultaten | `$filter=Name eq 'Platform'` |
| **$expand** | `?$expand=...` | Gerelateerde data | `$expand=Team_Department` |
| **$select** | `?$select=...` | Specifieke velden | `$select=ID,Name` |
| **$orderby** | `?$orderby=...` | Sorteer resultaten | `$orderby=Name desc` |
| **$top** | `?$top=N` | Eerste N resultaten | `$top=10` |
| **$skip** | `?$skip=N` | Skip eerste N | `$skip=20` |
| **$count** | `/$count` | Tel resultaten | `/Teams/$count` |

### Filter Operators

| Operator | Betekenis | Voorbeeld |
|----------|-----------|-----------|
| `eq` | Equals | `Name eq 'Platform'` |
| `ne` | Not equals | `TeamCategory ne null` |
| `gt` | Greater than | `ID gt 1000` |
| `lt` | Less than | `ID lt 9999` |
| `ge` | Greater or equal | `ID ge 1000` |
| `le` | Less or equal | `ID le 9999` |
| `and` | Logical AND | `Name eq 'Platform' and TeamCategory eq 'Dev'` |
| `or` | Logical OR | `TeamCategory eq 'Dev' or TeamCategory eq 'Ops'` |
| `not` | Logical NOT | `not (TeamCategory eq 'Dev')` |

### String Functies

| Functie | Beschrijving | Voorbeeld |
|---------|--------------|-----------|
| `contains` | Bevat tekst | `$filter=contains(Name, 'Platform')` |
| `startswith` | Begint met | `$filter=startswith(Name, 'Team')` |
| `endswith` | Eindigt op | `$filter=endswith(Name, 'Dev')` |
| `tolower` | Naar lowercase | `$filter=tolower(Name) eq 'platform'` |
| `toupper` | Naar uppercase | `$filter=toupper(Name) eq 'PLATFORM'` |

---

## Troubleshooting

### ❌ Error: "Invalid ID format"

**Probleem:** ID's worden afgekapt in sommige tools (zoals BRUNO)

**Bug:** BRUNO kapt ID's af na positie 16 en vervangt rest met nullen

**Oplossing:**
- Gebruik andere API tool (Postman, Insomnia, curl)
- Of gebruik de MCP Manager API Tester

**Voorbeeld:**
```
Correct ID:  79375943432407120
BRUNO kapt:  793759434324071000  ← Laatste 2 cijfers = 00
```

### ❌ Error: "DevOps Playbook not found"

**Probleem:** Query bevat referentie naar DevOps Playbook

**Oorzaak:** Per 18 juni 2025 is DevOps Playbook uitgefaseerd

**Oplossing:**
- Verwijder alle Playbook gerelateerde velden uit query
- Gebruik nieuwe endpoints (Assessments, MaturityScores)

### ❌ Error: "401 Unauthorized"

**Probleem:** Authenticatie mislukt

**Oplossing:**
1. Check username/password
2. Zorg dat credentials aangevraagd zijn
3. Check of DLO is getekend
4. Test met Acceptatie eerst

### ❌ Error: "404 Not Found"

**Probleem:** Endpoint bestaat niet

**Mogelijke oorzaken:**
1. Oude V3 endpoint gebruikt (gebruik V4!)
2. Typo in endpoint naam
3. Entity is verwijderd (zie vervallen OData gegevens)

**Oplossing:**
```
❌ /odata/POS_Odata/Teams        (V3 - uitgefaseerd)
✅ /odata/POS_Odata_v4/Teams     (V4 - correct)
```

### ❌ Error: "Query too complex"

**Probleem:** Te veel $expand levels

**Oplossing:**
- Beperk $expand diepte
- Maak meerdere queries
- Gebruik $select om velden te beperken

### ⚠️ Lege resultaten bij expand

**Probleem:** $expand geeft null terug

**Oorzaak:** Relatie bestaat niet voor dit record

**Check:**
- Gebruik `$expand=RelationName($select=ID)` om te testen
- Controleer in $metadata of relatie bestaat

---

## Praktische Workflows

### Workflow 1: Team Informatie Ophalen

**Stap 1:** Zoek team op naam
```
GET /Teams?$filter=contains(Name, 'Platform')&$select=ID,Name
```

**Stap 2:** Haal team details op
```
GET /Teams({ID})?$expand=Team_Department
```

**Stap 3:** Haal teamleden op
```
GET /TeamMembers?$filter=TeamMember_Team/ID eq {ID}&$expand=Account,FunctieRols
```

### Workflow 2: Organisatie Structuur

**Stap 1:** Haal alle departments op
```
GET /Departments?$orderby=Name
```

**Stap 2:** Haal rollen van een cluster
```
GET /Departments({ID})/Rols?$expand=Betrokkenens
```

**Stap 3:** Haal teams binnen een cluster
```
GET /Teams?$filter=Team_Department/ID eq {ID}
```

### Workflow 3: Azure DevOps Koppeling

**Stap 1:** Vind teams met Azure DevOps
```
GET /Teams?$filter=AzureDevOpsKey ne null&$select=ID,Name,AzureDevOpsKey
```

**Stap 2:** Match met Azure DevOps project
- Gebruik `AzureDevOpsKey` om te zoeken in Azure DevOps
- Bijvoorbeeld: `PLATFORM` → Azure DevOps project "PLATFORM"

---

## Best Practices

### 1. ✅ Gebruik Altijd $select

**Waarom:** Vermindert response size

**Voorbeeld:**
```
❌ /Teams
✅ /Teams?$select=ID,Name,TeamCategory
```

### 2. ✅ Test Eerst in Acceptatie

**Waarom:** Voorkom productie issues

```
Acceptatie: https://teamcentraal-a.ns.nl/odata/POS_Odata_v4/
Productie:  https://teamcentraal.ns.nl/odata/POS_Odata_v4/
```

### 3. ✅ Gebruik TeamCategory (niet TeamType)

**Waarom:** TeamType is verouderd

```
❌ /Teams?$filter=TeamType eq 'Dev'
✅ /Teams?$filter=TeamCategory eq 'Development'
```

### 4. ✅ Limiteer Resultaten

**Waarom:** Performance

```
/Teams?$top=100  ← Limiteer tot 100
```

### 5. ✅ Cache Metadata

**Waarom:** Schema wijzigt zelden

- Download `$metadata` eenmalig
- Gebruik voor referentie
- Update maandelijks

### 6. ✅ Error Handling

**Bij API calls:**
- Check 401 → Credentials vernieuwen
- Check 404 → Endpoint/ID bestaat niet
- Check 500 → Server issue, retry later

---

## Rate Limits

**Geen officiële rate limits gedocumenteerd.**

**Aanbevolen:**
- Max 100 requests per minuut
- Gebruik $top en $skip voor paginering
- Cache responses waar mogelijk

---

## Referenties

### Officiële Documentatie

- Interne TeamCentraal confluence pagina
- $metadata endpoint voor schema

### Support

- **Email:** TeamCentraal@ns.nl
- **DLO aanvragen:** TeamCentraal@ns.nl

### Tools

- **API Tester:** MCP Manager (deze applicatie)
- **Alternatives:** Postman, Insomnia (NIET BRUNO)
- **CLI:** curl met basic auth

### Gerelateerde Systemen

- **ServiceNow:** Voor applicatie SYSIDs
- **Azure DevOps:** Via AzureDevOpsKey koppeling
- **Jira:** Via JiraKey koppeling

---

**Laatst bijgewerkt:** 2025-10-29
**API Versie:** OData V4
**Document Versie:** 1.0
