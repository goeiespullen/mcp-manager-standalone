# Azure DevOps REST API Quick Reference

## Base URLs
```
https://dev.azure.com/{organization}/_apis/
https://dev.azure.com/{organization}/{project}/_apis/
```

## Authentication
```bash
# Using PAT (Personal Access Token)
curl -u ":{PAT}" "https://dev.azure.com/{org}/_apis/..."

# Or with header
curl -H "Authorization: Basic {base64(:{PAT})}" "https://dev.azure.com/{org}/_apis/..."
```

## Core APIs

### Projects
```bash
# List all projects
GET https://dev.azure.com/{org}/_apis/projects?api-version=7.1

# Get project details
GET https://dev.azure.com/{org}/_apis/projects/{projectId}?api-version=7.1
```

### Teams
```bash
# List teams in project
GET https://dev.azure.com/{org}/_apis/projects/{projectId}/teams?api-version=7.1

# Get team details
GET https://dev.azure.com/{org}/_apis/projects/{projectId}/teams/{teamId}?api-version=7.1
```

### Iterations (Sprints)
```bash
# List team iterations
GET https://dev.azure.com/{org}/{project}/{team}/_apis/work/teamsettings/iterations?api-version=7.1

# Get current iteration
GET https://dev.azure.com/{org}/{project}/{team}/_apis/work/teamsettings/iterations?$timeframe=current&api-version=7.1
```

## Work Items

### Query Work Items
```bash
# WIQL Query
POST https://dev.azure.com/{org}/{project}/_apis/wit/wiql?api-version=7.1
Content-Type: application/json

{
  "query": "SELECT [System.Id], [System.Title], [System.State] FROM WorkItems WHERE [System.IterationPath] = 'ProjectName\\Sprint 1'"
}
```

### Get Work Item Details
```bash
# Get single work item
GET https://dev.azure.com/{org}/{project}/_apis/wit/workitems/{id}?api-version=7.1

# Get multiple work items
GET https://dev.azure.com/{org}/{project}/_apis/wit/workitems?ids={id1},{id2},{id3}&api-version=7.1
```

### Work Item Fields
```bash
# List all fields
GET https://dev.azure.com/{org}/{project}/_apis/wit/fields?api-version=7.1
```

## Git APIs

### Repositories
```bash
# List repositories in project
GET https://dev.azure.com/{org}/{project}/_apis/git/repositories?api-version=7.1

# Get repository details
GET https://dev.azure.com/{org}/{project}/_apis/git/repositories/{repositoryId}?api-version=7.1
```

### Items (Files/Folders)
```bash
# Get items in repository
GET https://dev.azure.com/{org}/{project}/_apis/git/repositories/{repositoryId}/items?scopePath=/&recursionLevel=Full&api-version=7.1

# Get file content
GET https://dev.azure.com/{org}/{project}/_apis/git/repositories/{repositoryId}/items?path={filePath}&api-version=7.1
```

### Code Search
```bash
# Search code
POST https://almsearch.dev.azure.com/{org}/_apis/search/codesearchresults?api-version=7.1-preview.1
Content-Type: application/json

{
  "$top": 100,
  "searchText": "function calculateTotal",
  "filters": {
    "Project": ["{projectName}"]
  }
}
```

## Build & Release

### Builds
```bash
# List build definitions
GET https://dev.azure.com/{org}/{project}/_apis/build/definitions?api-version=7.1

# Queue a build
POST https://dev.azure.com/{org}/{project}/_apis/build/builds?api-version=7.1
```

### Releases
```bash
# List release definitions
GET https://vsrm.dev.azure.com/{org}/{project}/_apis/release/definitions?api-version=7.1
```

## Common Query Parameters

- `api-version`: Required for all requests (e.g., `7.1`, `7.1-preview.1`)
- `$top`: Limit number of results (e.g., `$top=100`)
- `$skip`: Skip number of results for pagination
- `$expand`: Include related data (e.g., `$expand=fields`)

## Example: Complete Workflow

```bash
# 1. Get organization
ORG="ns-topaas"
PROJECT="DNANSI"
TEAM="DNANSI Team"
PAT="your_personal_access_token"

# 2. List projects
curl -u ":$PAT" "https://dev.azure.com/$ORG/_apis/projects?api-version=7.1"

# 3. Get current sprint
curl -u ":$PAT" \
  "https://dev.azure.com/$ORG/$PROJECT/$TEAM/_apis/work/teamsettings/iterations?\$timeframe=current&api-version=7.1"

# 4. Query work items in current sprint
curl -u ":$PAT" \
  -X POST \
  -H "Content-Type: application/json" \
  -d '{"query": "SELECT [System.Id] FROM WorkItems WHERE [System.IterationPath] = \"DNANSI\\\\Sprint 1\""}' \
  "https://dev.azure.com/$ORG/$PROJECT/_apis/wit/wiql?api-version=7.1"

# 5. Get work item details
curl -u ":$PAT" \
  "https://dev.azure.com/$ORG/$PROJECT/_apis/wit/workitems/12345?api-version=7.1"
```

## Official Documentation

- **Main REST API Reference**: https://learn.microsoft.com/en-us/rest/api/azure/devops/
- **Core API**: https://learn.microsoft.com/en-us/rest/api/azure/devops/core/
- **Work Item Tracking**: https://learn.microsoft.com/en-us/rest/api/azure/devops/wit/
- **Git**: https://learn.microsoft.com/en-us/rest/api/azure/devops/git/
- **Build**: https://learn.microsoft.com/en-us/rest/api/azure/devops/build/
- **Release**: https://learn.microsoft.com/en-us/rest/api/azure/devops/release/

## Rate Limits

Azure DevOps has rate limits:
- **Global limit**: 200 requests per user per minute
- **Per resource**: Varies by endpoint

## Error Codes

- `200 OK`: Success
- `401 Unauthorized`: Invalid or missing PAT
- `403 Forbidden`: No permission
- `404 Not Found`: Resource doesn't exist
- `429 Too Many Requests`: Rate limit exceeded
- `500 Internal Server Error`: Server error
