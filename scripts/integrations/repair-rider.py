"""Apply the owner's verified public Rider export without private credentials."""
from pathlib import Path
import datetime
import json
import re
import shutil
import subprocess
import tomllib

root = Path(__file__).resolve().parents[2]
home = Path.home()
stamp = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
url = "http://127.0.0.1:64342/stream"

def save(p, text):
    if p.exists() and p.read_text() == text:
        return
    if p.exists():
        shutil.copy2(p, p.with_name(p.name + ".backup.rider-" + stamp))
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)
    print("Saved:", p)

for p, entry in [
    (root / ".mcp.json", {"type": "http", "url": url}),
    (root / ".kimi-code/mcp.json", {"url": url, "enabled": True, "startupTimeoutMs": 8000, "toolTimeoutMs": 60000}),
    (root / ".agents/mcp_config.json", {"serverUrl": url, "disabled": False}),
]:
    d = json.loads(p.read_text())
    d["mcpServers"]["jetbrains-rider"] = entry
    save(p, json.dumps(d, indent=2) + "\n")

p = home / ".codex/config.toml"
text = p.read_text()
pattern = r'(?m)^\[mcp_servers\.jetbrains-rider\]\n[^\[]*'
m = re.search(pattern, text)
assert m, "Expected existing Rider server configuration"
block = m.group()
for key, value in {"url": json.dumps(url), "enabled": "true", "startup_timeout_sec": "8", "tool_timeout_sec": "60"}.items():
    if re.search(rf'(?m)^{key}\s*=', block):
        block = re.sub(rf'(?m)^{key}\s*=.*$', f'{key} = {value}', block)
    else:
        block = block.replace("\n", f'\n{key} = {value}\n', 1)
text = text[:m.start()] + block + text[m.end():]
tomllib.loads(text)
save(p, text)

for p in [home / ".claude.json", home / ".gemini/config/mcp_config.json"]:
    if p.exists():
        shutil.copy2(p, p.with_name(p.name + ".backup.rider-native-" + stamp))
subprocess.run(["claude", "mcp", "add", "--scope", "user", "--transport", "http", "jetbrains-rider", url], check=True, timeout=15)
subprocess.run(["agy", "mcp", "add", "--type", "http", "jetbrains-rider", url], check=True, timeout=15)
print("Public Rider connection configured. No private token or brave-mode setting changed.")
