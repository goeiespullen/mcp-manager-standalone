# MCP Manager - Logging System

De MCP Manager heeft een uitgebreid logging systeem dat **alle traffic en behaviour** vastlegt in log bestanden.

## 📁 Log Bestanden Locatie

**Standaard locatie:**
```
~/.local/share/mcp-manager/logs/
```

**Openen vanaf de applicatie:**
- **File** → **📄 Open Logs Folder...**

## 📝 Log Bestanden

Het systeem maakt automatisch de volgende log bestanden aan:

| Bestand | Inhoud | Gebruik |
|---------|--------|---------|
| **general.log** | Algemene applicatie events | Opstarten, configuratie, UI events |
| **traffic.log** | MCP protocol communicatie | Alle IN/OUT messages tussen clients en servers |
| **server.log** | Server lifecycle events | Server start/stop, errors, status wijzigingen |
| **gateway.log** | Gateway operaties | Client connections, session management |
| **errors.log** | Alle warnings en errors | Centrale foutafhandeling (duplicaat van errors uit andere logs) |

## 🔍 Log Formaat

### Algemene Logs
```
[timestamp] [level] [category] message
```

**Voorbeeld:**
```
[2025-10-29 08:45:12.345] [INFO ] [GENERAL] MCP Server Manager started successfully
[2025-10-29 08:45:13.123] [DEBUG] [GATEWAY] MCPGateway listening on port 8700
[2025-10-29 08:45:15.789] [WARN ] [SERVER ] Failed to start server: Azure DevOps
```

### Traffic Logs
```
[timestamp] [direction] [client_id] message
```

**Voorbeeld:**
```
[2025-10-29 08:46:01.234] [IN ] [client_001] {"jsonrpc":"2.0","method":"tools/list","id":"1"}
[2025-10-29 08:46:01.567] [OUT] [client_001] {"jsonrpc":"2.0","result":{"tools":[...]},"id":"1"}
```

## 📊 Log Levels

| Level | Gebruik | Kleur in console |
|-------|---------|------------------|
| **DEBUG** | Gedetailleerde debug informatie | Grijs |
| **INFO** | Normale operationele events | Wit |
| **WARN** | Waarschuwingen (niet fataal) | Geel |
| **ERROR** | Foutmeldingen | Rood |
| **CRIT** | Kritieke fouten (fataal) | Rood (bold) |

## 🔄 Log Rotatie

**Automatische rotatie:**
- Max bestandsgrootte: **10 MB** per log bestand
- Bij het bereiken van de limiet worden logs geroteerd
- Oude logs worden hernoemd: `.log` → `.log.1` → `.log.2` etc.
- Maximaal **5 backups** worden bewaard

**Voorbeeld:**
```
general.log         ← Actieve log
general.log.1       ← Backup 1 (meest recent)
general.log.2       ← Backup 2
general.log.3       ← Backup 3
general.log.4       ← Backup 4
general.log.5       ← Backup 5 (oudste)
```

## 🎯 Gebruik van Logs

### Debuggen van MCP Communicatie

**Traffic logs bekijken:**
```bash
# Real-time monitoring
tail -f ~/.local/share/mcp-manager/logs/traffic.log

# Filter op client
grep "client_001" ~/.local/share/mcp-manager/logs/traffic.log

# Filter op direction
grep "\[IN \]" ~/.local/share/mcp-manager/logs/traffic.log
grep "\[OUT\]" ~/.local/share/mcp-manager/logs/traffic.log
```

### Troubleshooting Server Problems

**Error logs bekijken:**
```bash
# Alle errors
cat ~/.local/share/mcp-manager/logs/errors.log

# Laatste 50 errors
tail -50 ~/.local/share/mcp-manager/logs/errors.log

# Errors van specifieke server
grep "Azure DevOps" ~/.local/share/mcp-manager/logs/server.log
```

### Gateway Activity Monitoring

**Gateway logs bekijken:**
```bash
# Client connections
grep "connected" ~/.local/share/mcp-manager/logs/gateway.log

# Session creation
grep "Session created" ~/.local/share/mcp-manager/logs/gateway.log

# Session destruction
grep "Session.*destroyed" ~/.local/share/mcp-manager/logs/gateway.log
```

## 🔧 Configuratie (Code Level)

### Log Directory wijzigen

```cpp
#include "Logger.h"

// Wijzig log directory
Logger::instance()->setLogDirectory("/custom/path/logs");
```

### Categories aan/uitzetten

```cpp
// Disable traffic logging
Logger::instance()->enableCategory(Logger::Traffic, false);

// Enable server logging
Logger::instance()->enableCategory(Logger::Server, true);
```

### Max file size aanpassen

```cpp
// Set to 50 MB
Logger::instance()->setMaxFileSize(50 * 1024 * 1024);
```

### Logging gebruiken in code

```cpp
#include "Logger.h"

// Verschillende log levels
LOG_DEBUG(Logger::General, "Debug information");
LOG_INFO(Logger::Server, "Server started");
LOG_WARNING(Logger::Gateway, "Connection timeout");
LOG_ERROR(Logger::Server, "Failed to load config");
LOG_CRITICAL(Logger::General, "Fatal error occurred");

// Traffic logging
LOG_TRAFFIC("IN", "client_123", jsonMessage);
LOG_TRAFFIC("OUT", "client_123", responseMessage);
```

## 📈 Log Analyse

### Statistieken genereren

**Aantal requests per client:**
```bash
grep "\[IN \]" traffic.log | cut -d'[' -f3 | cut -d']' -f1 | sort | uniq -c | sort -rn
```

**Error rate per uur:**
```bash
grep "ERROR\|CRIT" errors.log | cut -d' ' -f1-2 | cut -d':' -f1 | uniq -c
```

**Meest voorkomende errors:**
```bash
grep "\[ERROR\]" errors.log | cut -d']' -f4 | sort | uniq -c | sort -rn | head -10
```

### Log aggregatie

**Alle logs van een sessie:**
```bash
SESSION_ID="session_abc123"
grep "$SESSION_ID" *.log
```

**Timeline reconstructie:**
```bash
# Combineer alle logs gesorteerd op timestamp
cat general.log server.log gateway.log traffic.log | sort
```

## 🚨 Alerts & Monitoring

### Kritieke errors detecteren

```bash
# Check voor kritieke errors in laatste 5 minuten
find ~/.local/share/mcp-manager/logs/ -name "*.log" -mmin -5 -exec grep -H "CRIT" {} \;
```

### Log size monitoring

```bash
# Check log sizes
du -h ~/.local/share/mcp-manager/logs/*.log

# Check totale log directory size
du -sh ~/.local/share/mcp-manager/logs/
```

## 🔐 Privacy & Security

**Wat wordt NIET gelogd:**
- PAT tokens (worden gefilterd in traffic logs)
- Wachtwoorden
- API keys (indien herkend)

**Wat wordt WEL gelogd:**
- Alle MCP JSON-RPC messages
- Foutmeldingen (kunnen data bevatten)
- Debug informatie

**⚠️ Belangrijk:**
- Log bestanden kunnen gevoelige informatie bevatten
- Deel logs niet zonder ze eerst te controleren
- Verwijder logs met gevoelige data handmatig indien nodig

## 📦 Log Backup & Archivering

### Handmatig backup maken

```bash
# Compress huidige logs
cd ~/.local/share/mcp-manager/logs/
tar -czf ~/mcp-logs-backup-$(date +%Y%m%d).tar.gz *.log

# Verplaats oude logs
mkdir -p ~/mcp-logs-archive/
mv *.log.* ~/mcp-logs-archive/
```

### Automatisch opschonen

```bash
# Verwijder logs ouder dan 30 dagen
find ~/.local/share/mcp-manager/logs/ -name "*.log.*" -mtime +30 -delete
```

## 🐛 Debugging Tips

### Volledige trace van een request

1. **Identificeer request ID** in traffic.log
2. **Zoek alle logs** met dat ID:
   ```bash
   grep "request_id_123" *.log | sort
   ```

### Performance analyse

```bash
# Gemiddelde response tijd meten
grep "\[OUT\]" traffic.log | \
  awk '{print $1,$2}' | \
  while read ts; do date -d "$ts" +%s.%N; done | \
  # Calculate average...
```

### Memory leaks detecteren

```bash
# Monitor log file growth rate
while true; do
  ls -lh general.log;
  sleep 60;
done
```

## 📞 Support

Bij problemen met logging:

1. **Check of logs worden aangemaakt:**
   ```bash
   ls -la ~/.local/share/mcp-manager/logs/
   ```

2. **Check permissions:**
   ```bash
   ls -ld ~/.local/share/mcp-manager/logs/
   # Should show: drwxr-xr-x
   ```

3. **Check disk space:**
   ```bash
   df -h ~/.local/share/
   ```

4. **Test logging handmatig:**
   Start de applicatie en check of er nieuwe entries verschijnen:
   ```bash
   tail -f ~/.local/share/mcp-manager/logs/general.log
   ```

## 🎓 Best Practices

1. ✅ **Monitor errors.log dagelijks** voor kritieke problemen
2. ✅ **Archiveer logs periodiek** (maandelijks/wekelijks)
3. ✅ **Gebruik traffic.log** voor API debugging
4. ✅ **Check log rotation** werkt correct
5. ✅ **Filter gevoelige data** voor delen van logs
6. ✅ **Bewaar logs** van productie servers minimaal 90 dagen

---

**Laatst bijgewerkt:** 2025-10-29
**Versie:** 1.0
