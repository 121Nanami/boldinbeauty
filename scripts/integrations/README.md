# Integration maintenance scripts

These four Python scripts record the dated 2026-09-30 setup/repair procedures. They are not startup checks or automatically safe repeatable maintenance. Running or importing them immediately mutates project or user-level native configuration; some also call provider CLIs. Do not run them as part of workspace validation.

Read `reports/integration-setup.md`, MAIN section 12.12 and `agents/integration-automation-agent/skills.md` first. Inspect current client configuration and service status through supported tools without printing credentials. Compare the exact intended changes with current state and preserve unrelated registrations, settings and native approvals. Device paths, service health and endpoints must be checked on that device; dated values do not prove current availability.

- `setup-clients.py` contains the original offline-service set and can disable services subsequently repaired. It also assigns the Claude enabled-server list rather than merging it. Review these operations against current health and preserve unrelated enabled servers before any authorized reuse.
- `disable-dead-mcp.py` contains a dated offline list; recheck each target before disabling it.
- `repair-rider.py` uses a dated public Rider endpoint. Obtain the current public export and bounded health evidence before applying it; keep Rider running. Never substitute a private endpoint/token.
- `register-native-clients.py` calls native registration commands. Authentication/consent and new-session discovery must be verified separately; registration alone does not establish connected tools.

After an authorized repair, verify native configuration syntax and bounded connection health, write a report and device handoff with before/after hashes, and return a MAIN proposal to the lead. Do not include tokens, connection storage or user settings in transfer packages. Root task review added this run guidance without rerunning the scripts or changing connections.
