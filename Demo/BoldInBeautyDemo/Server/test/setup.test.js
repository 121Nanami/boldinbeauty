// The guided setup (scripts/lib/setup-core.mjs) driven with a scripted fake terminal, a fake Atlas CLI, a fake file system and fake
// connection/mail checks. The real terminal and process wiring is exercised separately with a pseudo-terminal (see README).
import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { parseJson } from '../scripts/lib/atlas.mjs';
import { DEFAULTS, buildMongoUri, nodeVersionOk, randomPassword, renderEnv, runSetup } from '../scripts/lib/setup-core.mjs';

const WARNING = 'Warning: Secure storage is not available, falling back to insecure storage\nTo disable this alert, run "atlas config set silence_storage_warning true"\n';

function fakeIo(answers) {
  const queue = [...answers];
  const said = [];
  const prompts = [];
  const next = (kind, question) => {
    prompts.push(`${kind}: ${question}`);
    if (queue.length === 0) throw new Error(`no scripted answer left for ${kind}: ${question}`);
    const entry = queue.shift();
    if (entry && typeof entry === 'object' && 'match' in entry) {
      assert.ok(question.includes(entry.match), `expected the prompt "${question}" to contain "${entry.match}"`);
      return entry.value;
    }
    return entry;
  };
  return {
    said, prompts, remaining: () => queue.length,
    io: {
      say: (text) => said.push(text),
      ask: async (question, options = {}) => {
        const value = next('ask', question);
        const verdict = options.validate ? options.validate(value) : true;
        assert.equal(verdict, true, `scripted answer for "${question}" was rejected: ${verdict}`);
        return value;
      },
      choose: async (question, options) => {
        const value = next('choose', question);
        assert.ok(options.some((option) => option.value === value), `"${value}" is not an option of "${question}"`);
        return value;
      },
      confirm: async (question) => next('confirm', question),
    },
    transcript: () => said.join('\n'),
  };
}

function fakeFs(initial = {}) {
  const files = new Map(Object.entries(initial).map(([name, text]) => [name, { text, mode: 0o644 }]));
  return {
    files,
    exists: async (name) => files.has(name),
    write: async (name, text, mode) => { files.set(name, { text, mode }); },
    copy: async (from, to) => { files.set(to, { text: files.get(from).text, mode: 0o600 }); },
  };
}

// A scripted stand-in for the `atlas` command.
function fakeAtlas(options = {}) {
  const state = {
    installed: options.installed ?? true,
    loggedIn: options.loggedIn ?? false,
    projects: options.projects ?? [{ id: 'p1', name: 'Project 0' }],
    clusters: options.clusters ?? [],
    dbUsers: new Set(options.dbUsers ?? []),
    failM0: options.failM0 ?? false,
    brew: options.brew ?? false,
    calls: [],
  };
  const json = (value) => ({ code: 0, stdout: WARNING + JSON.stringify(value), stderr: '' });
  const exec = async (command, args) => {
    state.calls.push([command, ...args]);
    if (command === 'brew') {
      if (args[0] === '--version') return { code: state.brew ? 0 : 127, stdout: '', stderr: '' };
      state.installed = true;
      return { code: 0, stdout: '', stderr: '' };
    }
    assert.equal(command, 'atlas');
    const [a, b, c] = args;
    if (a === '--version') return { code: state.installed ? 0 : 127, stdout: 'atlascli version: 1.58.3', stderr: '' };
    if (a === 'auth' && b === 'whoami') return { code: state.loggedIn ? 0 : 1, stdout: WARNING + 'Logged in as owner@example.com', stderr: '' };
    if (a === 'auth' && b === 'login') { state.loggedIn = true; return { code: 0, stdout: '', stderr: '' }; }
    if (a === 'projects' && b === 'list') return json({ results: state.projects, totalCount: state.projects.length });
    if (a === 'projects' && b === 'create') { const project = { id: 'pNew', name: b === 'create' ? c : '' }; state.projects.push(project); return json(project); }
    if (a === 'clusters' && b === 'list') {
      const flexOnly = args.includes('--tier') && args[args.indexOf('--tier') + 1] === 'FLEX';
      return json({ results: state.clusters.filter((cluster) => (cluster.flex ? flexOnly : !flexOnly)), totalCount: 1 });
    }
    if (a === 'clusters' && b === 'create') {
      const tier = args[args.indexOf('--tier') + 1];
      if (tier === 'M0' && state.failM0) return { code: 1, stdout: '', stderr: 'Error: CANNOT_CREATE_FREE_CLUSTER_VIA_PUBLIC_API' };
      state.clusters.push({ name: b === 'create' ? c : '', stateName: 'CREATING', flex: tier === 'FLEX', connectionStrings: { standardSrv: 'mongodb+srv://boldinbeauty.abc12.mongodb.net' } });
      return { code: 0, stdout: '', stderr: '' };
    }
    if (a === 'clusters' && b === 'watch') { state.clusters.forEach((cluster) => { cluster.stateName = 'IDLE'; }); return { code: 0, stdout: '', stderr: '' }; }
    if (a === 'dbusers' && b === 'create') {
      const username = args[args.indexOf('--username') + 1];
      if (state.dbUsers.has(username)) return { code: 1, stdout: '', stderr: 'Error: USER_ALREADY_EXISTS: user already exists' };
      state.dbUsers.add(username);
      return json({ username });
    }
    if (a === 'dbusers' && b === 'delete') { state.dbUsers.delete(c); return { code: 0, stdout: '', stderr: '' }; }
    if (a === 'accessList' && b === 'create') return json({ results: [] });
    if (a === 'clusters' && b === 'connectionStrings') return json({ standard: 'mongodb://x', standardSrv: 'mongodb+srv://boldinbeauty.abc12.mongodb.net' });
    throw new Error(`fake atlas: unhandled ${args.join(' ')}`);
  };
  return { exec, state };
}

const okMongo = (record) => async (uri, database) => { record.push({ uri, database }); return { ok: true }; };
const okMail = async () => ({ ok: true });
const sentMail = (record) => async (smtp, to) => { record.push({ smtp, to }); return { ok: true }; };

function setupArgs(overrides = {}) {
  const atlas = overrides.atlas ?? fakeAtlas();
  const terminal = fakeIo(overrides.answers ?? []);
  const fs = overrides.fs ?? fakeFs();
  const mongoChecks = [];
  const mails = [];
  return {
    atlas, terminal, fs, mongoChecks, mails,
    args: {
      io: terminal.io, exec: atlas.exec, fs,
      checkMongo: overrides.checkMongo ?? okMongo(mongoChecks),
      checkMail: overrides.checkMail ?? okMail,
      sendTestMail: overrides.sendTestMail ?? sentMail(mails),
      sleep: async () => {},
      nodeVersion: overrides.nodeVersion ?? 'v22.22.0',
      platformHasBrew: async () => atlas.state.brew,
    },
  };
}

const APP_PASSWORD = 'abcd efgh ijkl mnop';

describe('setup helpers', () => {
  it('reads JSON after the CLI warning lines', () => {
    assert.deepEqual(parseJson(`${WARNING}{"a":[1,2]}\n`), { a: [1, 2] });
    assert.deepEqual(parseJson(`${WARNING}[{"id":"x"}]`), [{ id: 'x' }]);
    assert.throws(() => parseJson('nothing here'));
  });

  it('builds the connection string with an encoded password and the game database', () => {
    assert.equal(buildMongoUri('mongodb+srv://c.abc.mongodb.net', 'u', 'p@ss/w:rd', 'boldinbeauty'),
      'mongodb+srv://u:p%40ss%2Fw%3Ard@c.abc.mongodb.net/boldinbeauty?retryWrites=true&w=majority&appName=boldinbeauty');
    assert.match(buildMongoUri('mongodb+srv://c.abc.mongodb.net/?x=1', 'u', 'p', 'd'), /@c\.abc\.mongodb\.net\/d\?/);
  });

  it('makes long random passwords from unambiguous characters', () => {
    const passwords = new Set(Array.from({ length: 200 }, () => randomPassword()));
    assert.equal(passwords.size, 200);
    for (const password of passwords) assert.match(password, /^[A-HJ-NP-Za-km-z2-9]{32}$/);
  });

  it('quotes env values only when needed and skips empty ones', () => {
    const text = renderEnv({ A: 'plain-1.2', B: 'has space', C: 'quote"inside', D: '', E: undefined, F: 8080 });
    assert.match(text, /^A=plain-1\.2$/m);
    assert.match(text, /^B="has space"$/m);
    assert.match(text, /^C="quote\\"inside"$/m);
    assert.match(text, /^F=8080$/m);
    assert.doesNotMatch(text, /^D=/m);
    assert.doesNotMatch(text, /^E=/m);
  });

  it('checks the Node version', () => {
    assert.equal(nodeVersionOk('v22.1.0'), true);
    assert.equal(nodeVersionOk('v20.19.0'), true);
    assert.equal(nodeVersionOk('v20.18.9'), false);
    assert.equal(nodeVersionOk('v18.20.0'), false);
  });
});

describe('guided setup', () => {
  it('sets everything up from a fresh Atlas account: login code, free cluster, user, access, mail, .env', async () => {
    const t = setupArgs({
      answers: [
        'auto',                         // how to connect the database
        true,                           // create the free cluster
        'gmail',                        // mail method
        DEFAULTS.ownerEmail,            // mailbox
        APP_PASSWORD,                   // app password (secret)
        true,                           // send the test e-mail
      ],
    });
    const result = await runSetup(t.args);
    assert.equal(result.ok, true, t.terminal.transcript());
    assert.equal(t.terminal.remaining(), 0);

    // the Atlas steps, in order, with the safe options
    const calls = t.atlas.state.calls.filter((call) => call[0] === 'atlas').map((call) => call.join(' '));
    assert.ok(calls.some((c) => c === 'atlas auth login'), 'asked the user to log in');
    assert.ok(calls.some((c) => c.startsWith('atlas clusters create boldinbeauty --projectId p1 --provider AWS --region US_EAST_1 --tier M0')), 'created the free cluster');
    assert.ok(calls.some((c) => c.startsWith('atlas clusters watch boldinbeauty')), 'waited for the cluster');
    const create = t.atlas.state.calls.find((call) => call[1] === 'dbusers' && call[2] === 'create');
    assert.ok(create.includes('readWrite@boldinbeauty'), 'least-privilege role on the game database only');
    assert.ok(create.includes('--scope') && create.includes('boldinbeauty'), 'user limited to this cluster');
    assert.ok(calls.some((c) => c.startsWith('atlas accessList create --currentIp --projectId p1')), 'this computer allowed');

    // the .env
    const env = t.fs.files.get('.env');
    assert.equal(env.mode, 0o600, 'private file');
    const password = create[create.indexOf('--password') + 1];
    assert.match(env.text, new RegExp(`^MONGODB_URI=mongodb\\+srv://boldinbeauty_app:${password}@boldinbeauty\\.abc12\\.mongodb\\.net/boldinbeauty\\?retryWrites=true&w=majority&appName=boldinbeauty$`, 'm'));
    assert.match(env.text, /^CODE_SECRET=[0-9a-f]{64}$/m);
    assert.match(env.text, /^ADMIN_EMAILS=admin@example.com$/m);
    assert.match(env.text, /^MAIL_PROVIDER=smtp$/m);
    assert.match(env.text, /^SMTP_HOST=smtp\.gmail\.com$/m);
    assert.match(env.text, /^SMTP_PORT=587$/m);
    assert.match(env.text, /^SMTP_SECURE=0$/m);
    assert.match(env.text, /^SMTP_PASS=abcdefghijklmnop$/m, 'spaces removed from the app password');
    assert.match(env.text, /^MAIL_FROM="Boldinbeauty <admin@example.com>"$/m);

    // checks ran against the real URI; a test mail went to the owner
    assert.equal(t.mongoChecks.length, 1);
    assert.equal(t.mongoChecks[0].uri, env.text.match(/^MONGODB_URI=(.*)$/m)[1]);
    assert.equal(t.mails.length, 1);
    assert.equal(t.mails[0].to, DEFAULTS.ownerEmail);

    // nothing secret was printed
    const said = t.terminal.transcript();
    assert.equal(said.includes(password), false, 'database password not printed');
    assert.equal(said.includes('abcdefghijklmnop'), false, 'app password not printed');
    assert.equal(said.includes('mongodb+srv://boldinbeauty_app'), false, 'connection string not printed');
    assert.match(said, /npm run start:env/);
  });

  it('uses what already exists: logged in, one project, one running cluster', async () => {
    const atlas = fakeAtlas({ loggedIn: true, clusters: [{ name: 'Cluster0', stateName: 'IDLE', connectionStrings: { standardSrv: 'mongodb+srv://cluster0.xyz.mongodb.net' } }] });
    const t = setupArgs({ atlas, answers: ['auto', 'none'] });
    const result = await runSetup(t.args);
    assert.equal(result.ok, true, t.terminal.transcript());
    const calls = atlas.state.calls.map((call) => call.join(' '));
    assert.equal(calls.some((c) => c === 'atlas auth login'), false, 'no second login');
    assert.equal(calls.some((c) => c.startsWith('atlas clusters create')), false, 'no new cluster');
    assert.match(t.fs.files.get('.env').text, /cluster0\.xyz\.mongodb\.net/);
    assert.match(t.fs.files.get('.env').text, /^MAIL_PROVIDER=console$/m);
    assert.match(t.terminal.transcript(), /printed in the server window/);
  });

  it('finds a Flex cluster that the standard listing does not show', async () => {
    const atlas = fakeAtlas({ loggedIn: true, clusters: [{ name: 'FlexOne', flex: true, stateName: 'IDLE', connectionStrings: { standardSrv: 'mongodb+srv://flexone.q1.mongodb.net' } }] });
    const t = setupArgs({ atlas, answers: ['auto', 'none'] });
    assert.equal((await runSetup(t.args)).ok, true, t.terminal.transcript());
    assert.match(t.fs.files.get('.env').text, /flexone\.q1\.mongodb\.net/);
  });

  it('asks which project and cluster when there are several', async () => {
    const atlas = fakeAtlas({
      loggedIn: true,
      projects: [{ id: 'pA', name: 'Other' }, { id: 'pB', name: 'Games' }],
      clusters: [{ name: 'One', stateName: 'IDLE', connectionStrings: { standardSrv: 'mongodb+srv://one.a.mongodb.net' } }, { name: 'Two', stateName: 'IDLE', connectionStrings: { standardSrv: 'mongodb+srv://two.a.mongodb.net' } }],
    });
    const t = setupArgs({ atlas, answers: ['auto', 'pB', 'Two', 'none'] });
    assert.equal((await runSetup(t.args)).ok, true, t.terminal.transcript());
    assert.ok(atlas.state.calls.some((call) => call.includes('--projectId') && call.includes('pB')));
    assert.match(t.fs.files.get('.env').text, /two\.a\.mongodb\.net/);
  });

  it('renews the password when the database user already exists', async () => {
    const atlas = fakeAtlas({ loggedIn: true, dbUsers: ['boldinbeauty_app'], clusters: [{ name: 'C', stateName: 'IDLE', connectionStrings: { standardSrv: 'mongodb+srv://c.a.mongodb.net' } }] });
    const t = setupArgs({ atlas, answers: ['auto', 'none'] });
    assert.equal((await runSetup(t.args)).ok, true, t.terminal.transcript());
    const sequence = atlas.state.calls.filter((call) => call[1] === 'dbusers').map((call) => call[2]);
    assert.deepEqual(sequence, ['create', 'delete', 'create']);
  });

  it('offers a Flex cluster when the free one cannot be created, and stops if declined', async () => {
    const declined = setupArgs({ atlas: fakeAtlas({ loggedIn: true, failM0: true }), answers: ['auto', true, false] });
    const stopped = await runSetup(declined.args);
    assert.equal(stopped.ok, false);
    assert.equal(declined.fs.files.has('.env'), false, 'nothing written');

    const accepted = setupArgs({ atlas: fakeAtlas({ loggedIn: true, failM0: true }), answers: ['auto', true, true, 'none'] });
    const done = await runSetup(accepted.args);
    assert.equal(done.ok, true, accepted.terminal.transcript());
    assert.ok(accepted.atlas.state.calls.some((call) => call[1] === 'clusters' && call[2] === 'create' && call.includes('FLEX')));
  });

  it('installs the Atlas CLI with Homebrew when asked, and falls back to pasting when there is no Homebrew', async () => {
    const withBrew = setupArgs({ atlas: fakeAtlas({ installed: false, brew: true, loggedIn: true, clusters: [{ name: 'C', stateName: 'IDLE', connectionStrings: { standardSrv: 'mongodb+srv://c.a.mongodb.net' } }] }), answers: ['auto', true, 'none'] });
    assert.equal((await runSetup(withBrew.args)).ok, true, withBrew.terminal.transcript());
    assert.ok(withBrew.atlas.state.calls.some((call) => call[0] === 'brew' && call.includes('mongodb-atlas-cli')));

    const noBrew = setupArgs({ atlas: fakeAtlas({ installed: false }), answers: ['auto', 'mongodb+srv://u:pw@c.a.mongodb.net/?retryWrites=true', 'none'] });
    assert.equal((await runSetup(noBrew.args)).ok, true, noBrew.terminal.transcript());
    assert.match(noBrew.terminal.transcript(), /install-atlas-cli/);
    assert.match(noBrew.fs.files.get('.env').text, /^MONGODB_URI=mongodb\+srv:\/\/u:pw@c\.a\.mongodb\.net/m);
  });

  it('accepts a pasted connection string, rejecting a placeholder or junk first', async () => {
    const t = setupArgs({ answers: ['paste', 'hello', 'mongodb+srv://user:<password>@c.a.mongodb.net/', 'mongodb+srv://user:realpw@c.a.mongodb.net/', 'none'] });
    assert.equal((await runSetup(t.args)).ok, true, t.terminal.transcript());
    assert.match(t.terminal.transcript(), /does not start with mongodb/);
    assert.match(t.terminal.transcript(), /placeholder/);
    assert.equal(t.terminal.transcript().includes('realpw'), false);
    assert.equal(t.atlas.state.calls.length, 0, 'the Atlas CLI was never touched');
  });

  it('waits for a new database user to become active, then gives up with a clear message and writes nothing', async () => {
    let attempts = 0;
    const slow = setupArgs({ answers: ['paste', 'mongodb+srv://u:pw@c.a.mongodb.net/', 'none'], checkMongo: async () => (++attempts < 4 ? { ok: false, error: 'bad auth' } : { ok: true }) });
    assert.equal((await runSetup(slow.args)).ok, true, slow.terminal.transcript());
    assert.equal(attempts, 4);

    const never = setupArgs({ answers: ['paste', 'mongodb+srv://u:secretpw@c.a.mongodb.net/'], checkMongo: async () => ({ ok: false, error: 'connect failed for mongodb+srv://u:secretpw@c.a.mongodb.net/' }) });
    const result = await runSetup(never.args);
    assert.equal(result.ok, false);
    assert.equal(never.fs.files.has('.env'), false, 'nothing written');
    assert.match(never.terminal.transcript(), /Network Access/);
    assert.equal(never.terminal.transcript().includes('secretpw'), false, 'an error message never leaks the password');
  });

  it('lets the owner retry a wrong mail password, then continues', async () => {
    let calls = 0;
    const t = setupArgs({
      answers: ['paste', 'mongodb+srv://u:pw@c.a.mongodb.net/', 'gmail', DEFAULTS.ownerEmail, 'aaaabbbbccccdddd', 'retry', 'eeeeffffgggghhhh', false],
      checkMail: async () => (++calls < 2 ? { ok: false, error: 'Invalid login' } : { ok: true }),
    });
    assert.equal((await runSetup(t.args)).ok, true, t.terminal.transcript());
    assert.match(t.fs.files.get('.env').text, /^SMTP_PASS=eeeeffffgggghhhh$/m);
    assert.equal(t.mails.length, 0, 'the test e-mail was declined');
  });

  it('can switch to console mail after a mail failure, and supports Resend and plain SMTP', async () => {
    const skip = setupArgs({ answers: ['paste', 'mongodb+srv://u:pw@c.a.mongodb.net/', 'smtp', 'smtp.example.com', false, '587', 'me', 'pass', 'Me <me@example.com>', 'skip'], checkMail: async () => ({ ok: false, error: 'nope' }) });
    assert.equal((await runSetup(skip.args)).ok, true, skip.terminal.transcript());
    assert.match(skip.fs.files.get('.env').text, /^MAIL_PROVIDER=console$/m);
    assert.doesNotMatch(skip.fs.files.get('.env').text, /SMTP_PASS/);

    const resend = setupArgs({ answers: ['paste', 'mongodb+srv://u:pw@c.a.mongodb.net/', 'resend', 're_abc123', 'Boldinbeauty <no-reply@example.com>'] });
    assert.equal((await runSetup(resend.args)).ok, true, resend.terminal.transcript());
    assert.match(resend.fs.files.get('.env').text, /^MAIL_PROVIDER=resend$/m);
    assert.match(resend.fs.files.get('.env').text, /^RESEND_API_KEY=re_abc123$/m);
  });

  it('does not touch an existing .env unless told to, and keeps a backup when replacing it', async () => {
    const kept = setupArgs({ fs: fakeFs({ '.env': 'OLD=1\n' }), answers: [false] });
    const stay = await runSetup(kept.args);
    assert.equal(stay.ok, true);
    assert.equal(kept.fs.files.get('.env').text, 'OLD=1\n');
    assert.equal(kept.atlas.state.calls.length, 0, 'nothing was contacted');

    const replaced = setupArgs({ fs: fakeFs({ '.env': 'OLD=1\n' }), answers: [true, 'paste', 'mongodb+srv://u:pw@c.a.mongodb.net/', 'none'] });
    assert.equal((await runSetup(replaced.args)).ok, true, replaced.terminal.transcript());
    assert.equal(replaced.fs.files.get('.env.bak').text, 'OLD=1\n');
    assert.match(replaced.fs.files.get('.env').text, /MONGODB_URI=/);
  });

  it('refuses an old Node before doing anything', async () => {
    const t = setupArgs({ nodeVersion: 'v18.19.0' });
    const result = await runSetup(t.args);
    assert.equal(result.ok, false);
    assert.match(t.terminal.transcript(), /Node 20\.19/);
    assert.equal(t.atlas.state.calls.length, 0);
  });

  it('stops cleanly if the Atlas login did not complete', async () => {
    const atlas = fakeAtlas();
    const original = atlas.exec;
    atlas.exec = async (command, args) => (command === 'atlas' && args[0] === 'auth' && args[1] === 'login' ? { code: 0, stdout: '', stderr: '' } : original(command, args));
    const t = setupArgs({ atlas, answers: ['auto'] });
    const result = await runSetup(t.args);
    assert.equal(result.ok, false);
    assert.match(t.terminal.transcript(), /login did not complete/);
    assert.equal(t.fs.files.has('.env'), false);
  });
});
