# NS MCP Gateway Launcher

## 🚀 Opstart Opties

De NS MCP Gateway kan nu op 3 manieren gestart worden:

### 1. Via Desktop Application Menu

Het **NS MCP Gateway** icoon is nu beschikbaar in je applicatie menu!

- Open je applicatie launcher (bijv. Activities/Dash in GNOME)
- Zoek naar "NS MCP Gateway"
- Klik op het icoon om te starten

**Extra functies:**
- Rechtermuisklik op het icoon voor extra opties:
  - **View Logs** - Open terminal met live logs
  - **Stop Gateway** - Stop de gateway

### 2. Via Browser Launcher (Voor Site Navigatie)

Open de HTML launcher in je browser:

```
file:///home/laptop/MEGA/development/chatns/mcp-manager/launcher.html
```

Of:
```bash
xdg-open /home/laptop/MEGA/development/chatns/mcp-manager/launcher.html
```

**Features:**
- ✅ Real-time status indicator (checks elke 5 seconden)
- 🟢 Groene status = Gateway actief
- 🔴 Rode status = Gateway offline
- Overzicht van alle beschikbare MCP servers
- Direct links naar:
  - Gateway endpoint (localhost:8700)
  - Logs bekijken
  - Start/Stop gateway

**Bookmark deze pagina** in je browser voor snelle toegang!

### 3. Via Command Line (Traditioneel)

```bash
cd /home/laptop/MEGA/development/chatns/mcp-manager
./run.sh
```

## 📋 Bestanden

| Bestand | Beschrijving | Locatie |
|---------|--------------|---------|
| `mcp-gateway.svg` | Icoon (128x128 SVG) | `/home/laptop/MEGA/development/chatns/mcp-manager/` |
| `mcp-gateway.desktop` | Desktop entry | `~/.local/share/applications/` |
| `launcher.html` | Browser launcher | `/home/laptop/MEGA/development/chatns/mcp-manager/` |

## 🎨 Icoon Details

Het icoon toont:
- Blauwe cirkel = Gateway
- Witte nodes = Connected servers
- Gele hub = Central protocol handler
- "8700" = Gateway poort nummer

## 🔧 Gateway Informatie

- **Naam:** NS MCP Gateway
- **Poort:** 8700
- **Protocol:** HTTP JSON-RPC
- **Endpoint:** http://localhost:8700
- **Logs:** `/home/laptop/MEGA/development/chatns/mcp-manager/mcp-manager.log`

## 🖥️ Connected MCP Servers

| Server | Poort | Beschrijving |
|--------|-------|--------------|
| Azure DevOps | 8765 | Work items, sprints, teams |
| ChatNS | 8767 | AI chat en semantic search |
| Confluence | 8766 | Confluence wiki access |
| Demo MCP | 8768 | Test server |
| TeamCentraal | 8770 | NS Team registry |

## 📍 Browser Navigatie Integratie

Voor integratie in je site navigatie:

**HTML Link:**
```html
<a href="file:///home/laptop/MEGA/development/chatns/mcp-manager/launcher.html"
   target="_blank">
  <img src="file:///home/laptop/MEGA/development/chatns/mcp-manager/mcp-gateway.svg"
       alt="NS MCP Gateway" width="48">
  NS MCP Gateway
</a>
```

**Of als bookmark:**
1. Open `launcher.html` in je browser
2. Voeg toe aan bookmarks (Ctrl+D)
3. Sleep naar je bookmarks bar

## 🔄 Status Checken

```bash
# Check of gateway draait
curl http://localhost:8700/list-servers

# Check process
ps aux | grep mcp-manager

# Check port
netstat -tlnp | grep 8700
```

## 🛑 Gateway Stoppen

```bash
pkill -9 mcp-manager
```

Of gebruik de desktop entry rechtermuis menu.

## 📝 Logs Bekijken

```bash
tail -f /home/laptop/MEGA/development/chatns/mcp-manager/mcp-manager.log
```

## ⚙️ Configuratie

Server configuratie: `/home/laptop/MEGA/development/chatns/mcp-manager/configs/servers.json`

Credentials (encrypted): `/home/laptop/MEGA/development/chatns/chatns_summerschool/.keystore`

## 🐛 Troubleshooting

**Gateway start niet:**
```bash
# Check logs
tail -100 mcp-manager.log

# Poort al in gebruik?
netstat -tlnp | grep 8700
```

**Servers starten niet:**
- Check credentials in `.keystore`
- Verify server configuratie in `configs/servers.json`
- Check individuele server logs

**Desktop icoon verschijnt niet:**
```bash
# Update desktop database
update-desktop-database ~/.local/share/applications/
```
