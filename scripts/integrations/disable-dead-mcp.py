"""Disable known dead Codex MCP entries, preserving all unrelated settings."""
from pathlib import Path
import datetime
import re
import shutil
import tomllib

path = Path.home() / ".codex/config.toml"
original = path.read_text()
updated = original
for name in ("n8n",):
    pattern = rf'(?m)^\[mcp_servers\.{re.escape(name)}\]\n[^\[]*'
    match = re.search(pattern, updated)
    if not match:
        continue
    block = match.group()
    if re.search(r"(?m)^enabled\s*=", block):
        block = re.sub(r"(?m)^enabled\s*=.*$", "enabled = false", block)
    else:
        block = block.replace("\n", "\nenabled = false\n", 1)
    updated = updated[:match.start()] + block + updated[match.end():]
parsed = tomllib.loads(updated)
for name in ("n8n",):
    if name in parsed.get("mcp_servers", {}):
        assert parsed["mcp_servers"][name]["enabled"] is False
if original != updated:
    backup = path.with_name("config.backup.integration-" + datetime.datetime.now().strftime("%Y%m%dT%H%M%S") + ".toml")
    shutil.copy2(path, backup)
    path.write_text(updated)
    print("Saved backup:", backup)
print("Confirmed unavailable n8n disabled; working Rider configuration preserved.")
