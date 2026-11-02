"""Register reviewed public service URLs through native CLI controls."""
from pathlib import Path
import datetime
import shutil
import subprocess

home = Path.home()
stamp = datetime.datetime.now().strftime("%Y%m%dT%H%M%S")
for p in (home / ".claude.json", home / ".gemini/config/mcp_config.json"):
    if p.exists():
        shutil.copy2(p, p.with_name(p.name + ".backup.native-" + stamp))
urls = {
    "openaiDeveloperDocs": "https://developers.openai.com/mcp",
    "notion": "https://mcp.notion.com/mcp",
    "make": "https://mcp.make.com",
    "clickup": "https://mcp.clickup.com/mcp",
}
for name, url in urls.items():
    # User-scope registration follows the same supported workflow as the
    # existing kimi-cu integration and avoids modifying workspace trust.
    subprocess.run(["claude", "mcp", "add", "--scope", "user", "--transport", "http", name, url], check=True, timeout=15)
    subprocess.run(["agy", "mcp", "add", "--type", "http", name, url], check=True, timeout=15)
