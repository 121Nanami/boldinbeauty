#!/usr/bin/env python3
"""Check or explicitly regenerate project agent registrations from portable sources.

Only *-agent.md sources are agents. Native writes must run with actual host
permission; --check is read-only and does not establish runtime discovery.
"""
import argparse
import json
import re
from pathlib import Path


def read_agent(path):
    text = path.read_text()
    parts = text.split('---', 2)
    if len(parts) != 3 or parts[0].strip():
        raise ValueError(f'Missing frontmatter: {path}')
    fields = {}
    key = None
    for line in parts[1].splitlines():
        match = re.match(r'^([a-z_]+):\s*(.*)$', line)
        if match:
            key, value = match.groups()
            fields[key] = value
        elif line.startswith(' ') and key:
            fields[key] += ' ' + line.strip()
    name = fields.get('name', '').strip('"\'')
    description = fields.get('description', '')
    if description.startswith(('>-', '>', '|')):
        description = description.split(' ', 1)[1]
    description = description.strip('"\'')
    if name != path.stem or not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', name) or not description:
        raise ValueError(f'Invalid name/description: {path}')
    return name, description, parts[2].strip()


def expected_registrations(root):
    expected = {}
    for source in sorted((root / 'agents').glob('*-agent.md')):
        name, description, body = read_agent(source)
        native_md = ('---\nname: ' + name + '\ndescription: ' + json.dumps(description)
                     + '\n---\n\n' + body + '\n')
        # Inherit actual host model/tool settings. Cross-provider requested IDs
        # remain body guidance; portable metadata is not universal native config.
        native_toml = ('# Generated from agents/' + source.name + '; edit portable source first.\n'
                       + 'name = ' + json.dumps(name) + '\ndescription = ' + json.dumps(description)
                       + '\ndeveloper_instructions = ' + json.dumps(body + '\n') + '\n')
        expected['.codex/agents/' + name + '.toml'] = native_toml
        expected['.claude/agents/' + name + '.md'] = native_md
        expected['.cursor/agents/' + name + '.md'] = native_md
    if not expected:
        raise ValueError('No portable agent definitions found')
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true')
    mode.add_argument('--write', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    expected = expected_registrations(root)
    mismatches = [relative for relative, content in expected.items()
                  if not (root / relative).is_file() or (root / relative).read_text() != content]
    false_agents = [relative for relative in ('.codex/agents/README.toml', '.claude/agents/README.md',
                                              '.cursor/agents/README.md') if (root / relative).exists()]
    if args.write:
        for relative in mismatches:
            target = root / relative
            if target.is_symlink():
                raise ValueError('Refusing symlink registration: ' + relative)
        for relative in false_agents:
            if (root / relative).is_symlink():
                raise ValueError('Refusing symlink README registration: ' + relative)
        for relative in mismatches:
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(expected[relative])
        for relative in false_agents:
            (root / relative).unlink()
    print(json.dumps({'mode': 'write' if args.write else 'check', 'portable_agents': len(expected)//3,
                      'expected_native_registrations': len(expected), 'changed_or_mismatched': mismatches,
                      'removed_or_false_README_agents': false_agents,
                      'runtime_discovery': 'OPEN; reload/check each actual host separately'}, indent=2))
    return 0 if args.write or not (mismatches or false_agents) else 1


if __name__ == '__main__':
    raise SystemExit(main())
