# Mac and Windows project reconciliation

Owner request: 2026-09-30. This is a documentation synchronization structure. No remote transfer, repository write or reconciliation of the two implementations has been performed. Windows root, version and transfer method remain OPEN. Each device holds a replica of the conceptual global database `BoldInBeauty_MAIN`; neither replica wins merely because it is on Mac or Windows.

## File ownership and scope

| Scope | Treatment |
| --- | --- |
| Shared project knowledge | Reconcile `BoldInBeauty_MAIN`, portable `agents/*.md`, reusable `agents/*/skills.md`, root `AGENTS.md`/`CLAUDE.md`, `.cursor/rules/project-context.mdc` and this protocol against both replicas and the last mutually acknowledged base. |
| Reports and research | Transfer only explicitly listed, reviewed project-relative files; report verified findings with provenance. Reports do not automatically establish canon. |
| Implementation | Keep the two Unreal project versions separate until relative paths, requirements and intended differences have been mapped. Identify each change as shared, macOS-only, Windows-only or OPEN. |
| Native agent registrations | Adapt/regenerate `.codex/agents` and `.claude/agents` from reconciled portable sources using the established client process. Verify discovery on each host separately. Do not blindly copy device-specific host settings. |
| Device overlays | `sync/devices/macos.json` and `windows.json` describe each local environment. Absolute roots and local settings belong to that device and are not canonical shared paths. |

Exclude credentials, OAuth storage, authorization codes, tokens, MCP connection files/URLs, `.mcp`, `.agents/mcp_config.json`, `.kimi-code/mcp.json`, `.mcp.json`, user settings, `.claude/settings.local.json`, `.git`, `.DS_Store`, virtual environments, downloaded engines/vendor source, IDE state, `Binaries`, `Intermediate`, `Saved` and `DerivedDataCache` from transfer packages. Do not broadly copy whole project folders. Unreal `Config`, `Build`, `Plugins`, `Source` and `Content` need explicit shared/device classification; some are source material, others contain platform differences. Do not exclude source solely because of its directory name.

## Task startup checklist

1. Read `BoldInBeauty_MAIN` and the matching local device profile. Verify the actual root; historical paths are reference data.
2. State device, local project version, exact affected root and task scope. Identify the last mutually acknowledged base manifest, including SHA-256 hashes, or record base as OPEN. A Git commit is optional evidence, not a substitute for file hashes when no committed baseline exists.
3. Compare affected local file hashes with that base and the latest counterpart handoff. Record dirty/untracked changes separately. Modification time alone does not establish freshness.
4. If counterpart state is unavailable, label cross-device freshness OPEN. Continue isolated authorized work; defer conflicting shared-file integration until the other version is available. Never silently replace a newer or divergent file.
5. For concurrent shared updates, the accountable lead records a single writer reservation in `sync/handoff-state.json` and obtains acknowledgement from both active device sessions before integration. An unacknowledged reservation is only a proposal; the file is coordination state, not a cross-device filesystem lock. Proactively collaborate with available AI agents when useful under the owner's current authorization; this protocol does not itself launch agents.

## Task close checklist

1. Return device, affected root/version, base reference, paths, before/after SHA-256 hashes, shared/device scope, evidence, checks and unresolved issues to the lead/root. Use `change-manifest.example.json` as the record shape; do not label the example an actual completed transfer.
2. Propose concise MAIN updates for durable verified findings, reports, skills/procedures and completed work. Keep assumptions and OPEN issues distinct from confirmed facts. One accountable writer rereads and integrates MAIN; an independent agent may do so directly within authorized scope.
3. Record deliberate platform differences in the manifest. Keep local overlays local. Release the writer reservation after integration and identify resulting hashes; claim counterpart acknowledgement only after receiving it.
4. Report whether changes were prepared locally, transferred, applied on the counterpart or verified there. Local checks cannot prove Windows or macOS counterpart behavior.

## First reconciliation — no existing base assumed

1. Confirm the Windows project root, actual Unreal version, intended Mac/Windows differences and an owner-approved transfer method. Inspect both versions before selecting any canonical implementation.
2. Preserve original replicas in authorized local snapshots. Produce a file allowlist and SHA-256 inventory on both devices. For a specific file, Python `hashlib.sha256(Path(path).read_bytes()).hexdigest()` works on either platform; use actual bytes, not modification dates.
3. Exchange only the allowlisted nonsecret records/files through the approved transport. Use project-relative paths in the handoff. Review filename case, separators and platform-incompatible names before applying a mapping; do not rename files automatically.
4. Compare each shared file. Identical hashes establish equality. Without a known common base, treat unequal hashes as a conflict requiring content review; do not infer which file is newer. With a base, distinguish unchanged, one-sided and two-sided edits; deletion requires explicit review.
5. For each conflict record path, scope, base/Mac/Windows hashes, evidence, competing edits, intended platform differences, proposed resolution, accountable writer, owner decision if needed, status and final hashes. Resolve knowledge using newest explicit owner decisions, then corrected memory and original sources. Preserve unresolved differences as OPEN; do not invent creative decisions.
6. The acknowledged writer applies reviewed shared changes only within authorized scope. Adapt native registrations per device. Preserve device-specific files and unrelated local work. Git/GitHub/Git LFS actions must remain within the owner-assigned goal and actual runtime permissions; do not add repeated conversational confirmations to already-authorized work.
7. Verify both copies independently, compare resulting shared-file hashes and obtain acknowledgement from both devices. Only then record a mutually acknowledged base manifest for subsequent tasks. Reconcile implementation separately with task-relevant checks on each affected platform.

## Current evidence

Earlier Mac inspection SNAPSHOT, 2026-09-30: documentation root is `<local-home>/Documents/BoldInBeauty-Project`; it has no `.git`. Nested `Unreal Projects/BoldInBeauty` has Git metadata but no valid `HEAD`, and its project files are untracked. Its `.uproject` declares `EngineAssociation` `5.8`; this does not prove runtime compatibility or a successful build. Historical `<local-home>/Documents/BoldInBeauty_AI` and external `<local-home>/Documents/Unreal Projects/BoldInBeauty` were absent at inspection. Do not conflate these roots or silently redirect a task to the nested project. Windows details and cross-device equality remain OPEN.

Owner-requested version-control setup later on 2026-09-30 initialized separate documentation and game repositories on `main`, with local Git LFS, initial commits and verified private GitHub remotes under `121Nanami`. Current evidence: `reports/version-control/setup-2026-09-30.md`. This does not reconcile Windows or establish a mutually acknowledged base.
