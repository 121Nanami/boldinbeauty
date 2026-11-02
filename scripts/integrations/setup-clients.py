"""Install native, credential-free MCP configuration for Boldinbeauty clients.

Global writes require host approval. Existing settings and secrets are preserved.
"""
from pathlib import Path
import datetime
import json
import re
import shutil
import tomllib

root = Path(__file__).resolve().parents[2]
home = Path.home()
stamp = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
urls = {
    "openaiDeveloperDocs": "https://developers.openai.com/mcp",
    "notion": "https://mcp.notion.com/mcp",
    "make": "https://mcp.make.com",
    "clickup": "https://mcp.clickup.com/mcp",
    "unreal-mcp": "http://127.0.0.1:8000/mcp",
    "jetbrains-rider": "http://127.0.0.1:64342/stream",
    "n8n": "https://myla-h-reincarnated.app.n8n.cloud/mcp-server/http",
}
offline = {"unreal-mcp", "n8n"}

def save(path, content):
    if path.exists() and path.read_text() == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        shutil.copy2(path, path.with_name(path.name + ".backup.integration-" + stamp))
    path.write_text(content)
    print("Saved:", path)

def merge_json(path, servers):
    old = json.loads(path.read_text()) if path.exists() and path.read_text().strip() else {}
    old.setdefault("mcpServers", {}).update(servers)
    save(path, json.dumps(old, indent=2) + "\n")

# Claude has no disabled field in its standard MCP definition: omit offline
# targets altogether. These remain available in Kimi/Antigravity as disabled.
merge_json(root / ".mcp.json", {n: {"type": "http", "url": u} for n, u in urls.items() if n not in offline})
merge_json(root / ".kimi-code/mcp.json", {
    n: {"url": u, "enabled": n not in offline, "startupTimeoutMs": 8000, "toolTimeoutMs": 60000}
    for n, u in urls.items()
})
merge_json(root / ".agents/mcp_config.json", {
    n: {"serverUrl": u, "disabled": n in offline} for n, u in urls.items()
})
# Repair the existing empty Antigravity global config without adding global
# project services; workspace config supplies the actual project connections.
ag = home / ".gemini/config/mcp_config.json"
if ag.exists() and not ag.read_text().strip():
    save(ag, '{"mcpServers": {}}\n')

path = home / ".codex/config.toml"
original = path.read_text()
parsed = tomllib.loads(original)
updated = original
for name, url in urls.items():
    pattern = rf'(?m)^\[mcp_servers\.{re.escape(name)}\]\n[^\[]*'
    match = re.search(pattern, updated)
    if match:
        block = match.group()
        for key, value in {"enabled": str(name not in offline).lower(), "startup_timeout_sec": "8", "tool_timeout_sec": "60"}.items():
            if re.search(rf'(?m)^{key}\s*=', block):
                block = re.sub(rf'(?m)^{key}\s*=.*$', f'{key} = {value}', block)
            else:
                block = block.replace("\n", f'\n{key} = {value}\n', 1)
        updated = updated[:match.start()] + block + updated[match.end():]
    else:
        updated += f'\n[mcp_servers.{name}]\nurl = {json.dumps(url)}\nenabled = {str(name not in offline).lower()}\nstartup_timeout_sec = 8\ntool_timeout_sec = 60\n'
tomllib.loads(updated)
save(path, updated)

# Explicit owner authorization covers these reviewed project definitions;
# keep unrelated existing Claude permissions intact.
settings = root / ".claude/settings.local.json"
d = json.loads(settings.read_text()) if settings.exists() else {}
d["enabledMcpjsonServers"] = [n for n in urls if n not in offline]
d.setdefault("env", {})["MCP_TIMEOUT"] = "8000"
save(settings, json.dumps(d, indent=2) + "\n")
print("Registered official endpoints. OAuth-required services are not yet authenticated.")
