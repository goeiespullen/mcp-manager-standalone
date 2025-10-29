# Azure DevOps REST API - Praktische Handleiding

Deze handleiding bevat concrete voorbeelden voor het gebruik van de Azure DevOps REST API via de **API Tester** tab in MCP Server Manager.

## 📋 Inhoudsopgave

1. [Configuratie](#configuratie)
2. [Basis API Calls](#basis-api-calls)
3. [Work Items & Story Points](#work-items--story-points)
4. [Sprint/Iteration Management](#sprintiteration-management)
5. [Git Repositories](#git-repositories)
6. [Build & Release](#build--release)
7. [Veelgebruikte Workflows](#veelgebruikte-workflows)
8. [Troubleshooting](#troubleshooting)

---

## Configuratie

### API Tester Instellingen

**Vaste configuratie:**
- **Organization:** `ns-topaas`
- **PAT Token:** Stel in via het PAT Token veld
- **Base URL:** `https://dev.azure.com/ns-topaas/` (wordt automatisch toegevoegd)

**Let op:**
- Vul alleen het **endpoint** deel in, NIET de volledige URL
- Gebruik de juiste HTTP method (GET, POST, etc.)
- Voeg `api-version` altijd toe aan de query parameters

---

## Basis API Calls

### 1. Lijst van alle projecten

**Method:** `GET`
**Endpoint:** `_apis/projects?api-version=7.1`

**Response:**
```json
{
  "count": 32,
  "value": [
    {
      "id": "229b1a37-3573-4cc4-ac1a-424a96b3e557",
      "name": "DNABI",
      "description": "Voor DNABI",
      "state": "wellFormed"
    }
  ]
}
```

### 2. Teams binnen een project

**Method:** `GET`
**Endpoint:** `_apis/projects/{project}/teams?api-version=7.1`

**Voorbeeld:**
- Endpoint: `_apis/projects/DNANSI/teams?api-version=7.1`

### 3. Beschikbare Resource Areas (API categorieën)

**Method:** `GET`
**Endpoint:** `_apis/resourceareas?api-version=7.0-preview`

**Gebruik:** Ontdek welke API functionaliteiten beschikbaar zijn

---

## Work Items & Story Points

### 4. Work Items ophalen met WIQL Query

**Method:** `POST`
**Endpoint:** `{project}/_apis/wit/wiql?api-version=7.1`

**Request Body - Huidige Sprint:**
```json
{
  "query": "SELECT [System.Id], [System.Title], [System.State], [Microsoft.VSTS.Scheduling.StoryPoints], [Microsoft.VSTS.Scheduling.Effort] FROM WorkItems WHERE [System.TeamProject] = 'DNANSI' AND [System.IterationPath] = @CurrentIteration ORDER BY [System.ChangedDate] DESC"
}
```

**Request Body - Specifieke Sprint:**
```json
{
  "query": "SELECT [System.Id], [System.Title], [System.State] FROM WorkItems WHERE [System.TeamProject] = 'DNANSI' AND [System.IterationPath] = 'DNANSI\\Sprint 17' ORDER BY [System.ChangedDate] DESC"
}
```

**Response:**
```json
{
  "workItems": [
    {"id": 1013726},
    {"id": 1012816}
  ]
}
```

**Let op:** Dit geeft alleen IDs terug, geen details!

### 5. Work Item Details ophalen (met Story Points)

#### Optie A: Batch API (Aanbevolen - max 200 items)

**Method:** `POST`
**Endpoint:** `_apis/wit/workitemsbatch?api-version=7.1-preview.1`

**Request Body:**
```json
{
  "ids": [1013726, 1012816, 1012000, 1011966],
  "fields": [
    "System.Id",
    "System.Title",
    "System.WorkItemType",
    "System.State",
    "System.IterationPath",
    "Microsoft.VSTS.Scheduling.StoryPoints",
    "Microsoft.VSTS.Scheduling.Effort",
    "Microsoft.VSTS.Scheduling.RemainingWork",
    "Microsoft.VSTS.Scheduling.OriginalEstimate"
  ]
}
```

**Response:**
```json
{
  "count": 4,
  "value": [
    {
      "id": 1013726,
      "fields": {
        "System.Id": 1013726,
        "System.Title": "Implement user authentication",
        "System.WorkItemType": "User Story",
        "System.State": "Active",
        "Microsoft.VSTS.Scheduling.StoryPoints": 5,
        "Microsoft.VSTS.Scheduling.Effort": 8
      }
    }
  ]
}
```

#### Optie B: Individuele GET (langzamer)

**Method:** `GET`
**Endpoint:** `_apis/wit/workitems?ids={id1},{id2},{id3}&fields={fields}&api-version=7.1`

**Voorbeeld:**
```
_apis/wit/workitems?ids=1013726,1012816&fields=System.Id,System.Title,Microsoft.VSTS.Scheduling.StoryPoints&api-version=7.1
```

### 6. Alle beschikbare Work Item Fields

**Method:** `GET`
**Endpoint:** `{project}/_apis/wit/fields?api-version=7.1`

**Gebruik:** Ontdek welke custom fields beschikbaar zijn

---

## Sprint/Iteration Management

### 7. Team Iterations (Sprints) ophalen

**Method:** `GET`
**Endpoint:** `{project}/{team}/_apis/work/teamsettings/iterations?api-version=7.1-preview.1`

**Voorbeeld:**
```
DNANSI/DNANSI Team/_apis/work/teamsettings/iterations?api-version=7.1-preview.1
```

**Response:**
```json
{
  "value": [
    {
      "id": "abc123",
      "name": "Sprint 17",
      "path": "DNANSI\\Sprint 17",
      "attributes": {
        "startDate": "2025-10-01T00:00:00Z",
        "finishDate": "2025-10-14T00:00:00Z"
      }
    }
  ]
}
```

### 8. Work Items in specifieke Iteration

**Method:** `GET`
**Endpoint:** `{project}/{team}/_apis/work/teamsettings/iterations/{iterationId}/workitems?api-version=7.1-preview.1`

**Response:**
```json
{
  "workItemRelations": [
    {
      "target": {
        "id": 1013726,
        "url": "..."
      }
    }
  ]
}
```

**Let op:**
- Dit geeft alleen IDs terug via `workItemRelations[].target.id`
- Gebruik daarna de batch API (stap 5) voor details + story points

### 9. Huidige Sprint ophalen

**Method:** `GET`
**Endpoint:** `{project}/{team}/_apis/work/teamsettings/iterations?$timeframe=current&api-version=7.1-preview.1`

**Voorbeeld:**
```
DNANSI/DNANSI Team/_apis/work/teamsettings/iterations?$timeframe=current&api-version=7.1-preview.1
```

### 10. Team Capacity voor Sprint

**Method:** `GET`
**Endpoint:** `{project}/{team}/_apis/work/teamsettings/iterations/{iterationId}/capacities?api-version=7.1-preview.1`

**Response:**
```json
{
  "value": [
    {
      "teamMember": {
        "displayName": "John Doe"
      },
      "activities": [
        {
          "name": "Development",
          "capacityPerDay": 6
        }
      ],
      "daysOff": []
    }
  ]
}
```

---

## Git Repositories

### 11. Repositories in Project

**Method:** `GET`
**Endpoint:** `_apis/git/repositories?project={project}&api-version=7.0`

**Voorbeeld:**
```
_apis/git/repositories?project=DNANSI&api-version=7.0
```

### 12. Repository Items (bestanden/mappen)

**Method:** `GET`
**Endpoint:** `{project}/_apis/git/repositories/{repository}/items?path={path}&recursionLevel=OneLevel&api-version=7.0`

**Voorbeeld - Root directory:**
```
DNANSI/_apis/git/repositories/my-repo/items?path=/&recursionLevel=OneLevel&api-version=7.0
```

**Voorbeeld - Specifiek pad:**
```
DNANSI/_apis/git/repositories/my-repo/items?path=/src/components&recursionLevel=OneLevel&api-version=7.0
```

### 13. Bestand content ophalen

**Method:** `GET`
**Endpoint:** `{project}/_apis/git/repositories/{repository}/items?path={filePath}&includeContent=true&api-version=7.0`

**Voorbeeld:**
```
DNANSI/_apis/git/repositories/my-repo/items?path=/README.md&includeContent=true&api-version=7.0
```

### 14. Pull Requests

**Method:** `GET`
**Endpoint:** `{project}/_apis/git/repositories/{repositoryId}/pullrequests?api-version=7.1`

**Query parameters:**
- `searchCriteria.status=active` - Alleen actieve PRs
- `searchCriteria.creatorId={userId}` - Filter op creator

---

## Build & Release

### 15. Build Definitions (Pipelines)

**Method:** `GET`
**Endpoint:** `{project}/_apis/build/definitions?api-version=7.1`

### 16. Recent Builds

**Method:** `GET`
**Endpoint:** `{project}/_apis/build/builds?api-version=7.1`

**Query parameters:**
- `$top=10` - Laatste 10 builds
- `definitions={definitionId}` - Filter op pipeline
- `statusFilter=completed` - Alleen completed builds

### 17. Build Details

**Method:** `GET`
**Endpoint:** `{project}/_apis/build/builds/{buildId}?api-version=7.1`

### 18. Build Logs

**Method:** `GET`
**Endpoint:** `{project}/_apis/build/builds/{buildId}/logs?api-version=7.1`

### 19. Queue een Build

**Method:** `POST`
**Endpoint:** `{project}/_apis/build/builds?api-version=7.1`

**Request Body:**
```json
{
  "definition": {
    "id": 123
  },
  "sourceBranch": "refs/heads/main"
}
```

---

## Veelgebruikte Workflows

### Workflow 1: Story Points van huidige sprint ophalen

**Stap 1:** Haal huidige sprint op
```
GET {project}/{team}/_apis/work/teamsettings/iterations?$timeframe=current&api-version=7.1-preview.1
```

**Stap 2:** Haal werk items IDs op met WIQL
```
POST {project}/_apis/wit/wiql?api-version=7.1
Body: {"query": "SELECT [System.Id] FROM WorkItems WHERE [System.IterationPath] = @CurrentIteration"}
```

**Stap 3:** Haal story points op met batch API
```
POST _apis/wit/workitemsbatch?api-version=7.1-preview.1
Body: {"ids": [1,2,3], "fields": ["System.Id", "Microsoft.VSTS.Scheduling.StoryPoints"]}
```

### Workflow 2: Sprint burndown data

**Stap 1:** Haal iteration ID
```
GET {project}/{team}/_apis/work/teamsettings/iterations?api-version=7.1-preview.1
```

**Stap 2:** Haal work items in iteration
```
GET {project}/{team}/_apis/work/teamsettings/iterations/{iterationId}/workitems?api-version=7.1-preview.1
```

**Stap 3:** Haal details + remaining work
```
POST _apis/wit/workitemsbatch?api-version=7.1-preview.1
Body: {"ids": [...], "fields": ["Microsoft.VSTS.Scheduling.RemainingWork", "System.State"]}
```

### Workflow 3: Code wijzigingen in sprint

**Stap 1:** Haal sprint start/eind datum
```
GET {project}/{team}/_apis/work/teamsettings/iterations/{iterationId}?api-version=7.1-preview.1
```

**Stap 2:** Haal commits in periode
```
GET {project}/_apis/git/repositories/{repoId}/commits?searchCriteria.fromDate={startDate}&searchCriteria.toDate={endDate}&api-version=7.1
```

---

## Troubleshooting

### ❌ Error: "The requested version \"X\" is under preview"

**Oplossing:** Voeg `-preview` toe aan de API versie
```
api-version=7.1        →  api-version=7.1-preview.1
api-version=7.0        →  api-version=7.0-preview
```

### ❌ Error: "A potentially dangerous Request.Path value was detected"

**Probleem:** Je vult de volledige URL in inclusief base URL

**Oplossing:** Vul alleen het endpoint in:
```
❌ https://dev.azure.com/ns-topaas/DNANSI/_apis/...
✅ DNANSI/_apis/...
```

### ❌ Error: "HTTP 405 Method Not Allowed"

**Probleem:** Je gebruikt GET voor een POST endpoint (of andersom)

**Oplossing:** Controleer de HTTP method:
- WIQL queries: altijd **POST**
- Batch work items: altijd **POST**
- Ophalen data: meestal **GET**

### ❌ Error: "TF51011: Iteration path does not exist"

**Probleem:** De iteration path bestaat niet of is verkeerd gespeld

**Oplossing:**
1. Haal eerst de exacte iteration paths op met stap 7
2. Kopieer de `path` waarde exact (inclusief backslashes)
3. Gebruik: `DNANSI\\Sprint 17` (let op dubbele backslash in JSON)

### ❌ Lege response of geen story points

**Mogelijke oorzaken:**
1. Story Points field heet anders in jouw configuratie
2. Gebruik `Microsoft.VSTS.Scheduling.Effort` in plaats van `.StoryPoints`
3. Haal alle fields op om te zien welke beschikbaar zijn (stap 6)

### ❌ Error: "401 Unauthorized"

**Probleem:** PAT token is verlopen of incorrect

**Oplossing:**
1. Genereer nieuwe PAT in Azure DevOps
2. Zorg voor juiste scopes: Work Items (Read), Code (Read)
3. Update PAT Token veld in API Tester

---

## Nuttige Tips

### 🔍 Ontdek beschikbare fields

Gebruik de Fields API om te zien welke velden beschikbaar zijn:
```
GET {project}/_apis/wit/fields?api-version=7.1
```

### 📊 Story Points vs Effort

Azure DevOps heeft meerdere fields voor effort tracking:
- `Microsoft.VSTS.Scheduling.StoryPoints` - Agile Story Points
- `Microsoft.VSTS.Scheduling.Effort` - Effort (uren)
- `Microsoft.VSTS.Scheduling.RemainingWork` - Remaining Work
- `Microsoft.VSTS.Scheduling.OriginalEstimate` - Original Estimate

Controleer welke jouw organisatie gebruikt!

### ⚡ Performance

- Gebruik batch API voor meerdere work items (max 200)
- Selecteer alleen benodigde fields
- Gebruik `$top` parameter om responses te beperken
- WIQL queries: voeg `TOP {n}` toe aan SELECT

### 🔐 API Rate Limits

Azure DevOps rate limits:
- **200 requests per user per minuut**
- Bij overschrijding: HTTP 429 error
- Gebruik batch APIs om calls te reduceren

---

## Referenties

### Officiële Documentatie

- [Azure DevOps REST API](https://learn.microsoft.com/en-us/rest/api/azure/devops/)
- [Work Item Tracking API](https://learn.microsoft.com/en-us/rest/api/azure/devops/wit/)
- [Git API](https://learn.microsoft.com/en-us/rest/api/azure/devops/git/)
- [Build API](https://learn.microsoft.com/en-us/rest/api/azure/devops/build/)
- [WIQL Syntax](https://learn.microsoft.com/en-us/azure/devops/boards/queries/wiql-syntax)

### Quick Links

- [Resource Areas lijst](https://dev.azure.com/ns-topaas/_apis/resourceareas?api-version=7.0-preview)
- [Azure DevOps Status](https://status.dev.azure.com/)
- [PAT Token Beheer](https://dev.azure.com/ns-topaas/_usersSettings/tokens)

---

**Laatst bijgewerkt:** 2025-10-28
**Versie:** 1.0
