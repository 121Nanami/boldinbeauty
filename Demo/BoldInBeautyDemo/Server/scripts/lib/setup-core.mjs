// The guided setup for the account service: gets a MongoDB Atlas database (through the Atlas CLI, which only asks the owner to
// approve a login code in the browser), a sender mailbox, and writes a git-ignored .env. All side effects are injected (io, exec,
// fs, checks), so the whole flow is unit-tested with fakes; scripts/setup.mjs wires the real ones.
import crypto from 'node:crypto';
import { createAtlas } from './atlas.mjs';

export const DEFAULTS = {
  database: 'boldinbeauty',
  dbUser: 'boldinbeauty_app',
  project: 'BoldInBeauty',
  cluster: 'boldinbeauty',
  region: 'US_EAST_1',
  ownerEmail: 'admin@example.com',
};

export const randomPassword = (length = 32) => {
  const alphabet = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789';
  let out = '';
  while (out.length < length) {
    const byte = crypto.randomBytes(1)[0];
    if (byte < alphabet.length * Math.floor(256 / alphabet.length)) out += alphabet[byte % alphabet.length]; // unbiased
  }
  return out;
};

export function buildMongoUri(srvHost, username, password, database) {
  const host = srvHost.replace(/^mongodb\+srv:\/\//, '').replace(/\/.*$/, '');
  return `mongodb+srv://${encodeURIComponent(username)}:${encodeURIComponent(password)}@${host}/${database}?retryWrites=true&w=majority&appName=${database}`;
}

export function renderEnv(values) {
  const quote = (value) => (/^[A-Za-z0-9_.:/@+,=?&%~-]*$/.test(value) ? value : `"${value.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`);
  const lines = [
    '# Written by `npm run setup`. Secrets: keep this file private (it is git-ignored). Do not paste it anywhere.',
    ...Object.entries(values).filter(([, value]) => value !== undefined && value !== '').map(([key, value]) => `${key}=${quote(String(value))}`),
  ];
  return `${lines.join('\n')}\n`;
}

export function nodeVersionOk(version) {
  const [major, minor] = version.replace(/^v/, '').split('.').map(Number);
  return major > 20 || (major === 20 && minor >= 19);
}

class Abort extends Error {}

export async function runSetup({ io, exec, fs, checkMongo, checkMail, sendTestMail, sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms)), nodeVersion = process.version, platformHasBrew = async () => false }) {
  const secrets = new Set();
  const redact = (text) => [...secrets].reduce((out, secret) => (secret ? out.split(secret).join('***') : out), text);
  const say = (text = '') => io.say(redact(text));
  const atlas = createAtlas(exec, redact);

  const step = (number, title) => say(`\n=== ${number}. ${title} ===`);

  say('Boldinbeauty account service setup');
  say('This gets a MongoDB Atlas database and a sender mailbox ready, then writes Server/.env (private, git-ignored).');
  say('You will only need to: approve one login code in your browser, and paste one e-mail app password. Nothing is sent anywhere else.\n');

  if (!nodeVersionOk(nodeVersion)) {
    say(`Node ${nodeVersion} is too old: this needs Node 20.19 or newer (https://nodejs.org). Install it, then run this again.`);
    return { ok: false, reason: 'node' };
  }

  // ------------------------------------------------------------------ existing .env
  if (await fs.exists('.env')) {
    say('A .env already exists here.');
    if (!(await io.confirm('Replace it? (the old one is kept as .env.bak; the database user password will be renewed)', false))) {
      say('Left everything as it was. Start the server with:  npm run start:env');
      return { ok: true, reason: 'kept' };
    }
  }

  try {
    // ---------------------------------------------------------------- 1. database
    step(1, 'MongoDB Atlas database');
    const database = DEFAULTS.database;
    let mongoUri = '';

    const mode = await io.choose('How do you want to connect the database?', [
      { value: 'auto', label: 'Automatic (recommended): the Atlas CLI sets it up; you approve a login code in the browser' },
      { value: 'paste', label: 'I already have a connection string and will paste it' },
    ]);

    let useAuto = mode === 'auto';
    if (useAuto && !(await atlas.installed())) {
      say('The Atlas CLI is not installed.');
      if (await platformHasBrew()) {
        if (await io.confirm('Install it now with Homebrew (brew install mongodb-atlas-cli)?', true)) {
          const result = await exec('brew', ['install', 'mongodb-atlas-cli'], { inherit: true });
          if (result.code !== 0 || !(await atlas.installed())) {
            say('The install did not work. Switching to the paste-a-connection-string route.');
            useAuto = false;
          }
        } else {
          useAuto = false;
        }
      } else {
        say('Install it from https://www.mongodb.com/docs/atlas/cli/current/install-atlas-cli/ and run this again, or paste a connection string instead.');
        useAuto = false;
      }
    }

    if (useAuto) {
      const who = await atlas.whoami();
      if (who.ok) {
        say(`Already logged in to Atlas. ${who.text.split('\n')[0] || ''}`.trim());
      } else {
        say('Logging in to Atlas. A browser tab opens and shows a one-time code: type or paste the code there and confirm.');
        await atlas.login();
        if (!(await atlas.whoami()).ok) throw new Abort('Atlas login did not complete. Run the setup again.');
      }

      // project
      let projects = await atlas.projects();
      let project;
      if (projects.length === 0) {
        say(`No Atlas project yet: creating "${DEFAULTS.project}".`);
        project = await atlas.createProject(DEFAULTS.project);
      } else if (projects.length === 1) {
        project = projects[0];
        say(`Using your Atlas project "${project.name}".`);
      } else {
        const picked = await io.choose('Which Atlas project should hold the game database?', projects.map((p) => ({ value: p.id, label: p.name })));
        project = projects.find((p) => p.id === picked);
      }

      // cluster
      let clusters = await atlas.clusters(project.id);
      let cluster;
      if (clusters.length === 0) {
        say('There is no cluster in that project yet.');
        if (!(await io.confirm(`Create a free cluster "${DEFAULTS.cluster}" (M0, AWS ${DEFAULTS.region}; no charge)?`, true))) {
          throw new Abort('A cluster is needed. Create one in Atlas (or choose "paste a connection string") and run this again.');
        }
        try {
          await atlas.createCluster(project.id, DEFAULTS.cluster, { provider: 'AWS', region: DEFAULTS.region, tier: 'M0' });
        } catch (error) {
          say(`${error.message}`);
          if (!(await io.confirm('Try a Flex cluster instead? (a small monthly charge taken from your Atlas credit; check the price in Atlas)', false))) {
            throw new Abort('No cluster created. Fix the problem above or create a cluster in Atlas, then run this again.');
          }
          await atlas.createCluster(project.id, DEFAULTS.cluster, { provider: 'AWS', region: DEFAULTS.region, tier: 'FLEX' });
        }
        say('Cluster requested. Waiting until it is ready (usually 3-10 minutes; progress is shown).');
        await atlas.watchCluster(project.id, DEFAULTS.cluster);
        clusters = await atlas.clusters(project.id);
        cluster = clusters.find((c) => c.name === DEFAULTS.cluster) || { name: DEFAULTS.cluster, srv: '' };
      } else if (clusters.length === 1) {
        cluster = clusters[0];
        say(`Using your cluster "${cluster.name}".`);
      } else {
        const picked = await io.choose('Which cluster should hold the game database?', clusters.map((c) => ({ value: c.name, label: c.name })));
        cluster = clusters.find((c) => c.name === picked);
      }
      if (cluster.state && cluster.state !== 'IDLE') {
        say(`The cluster is ${cluster.state}; waiting until it is ready.`);
        await atlas.watchCluster(project.id, cluster.name);
      }

      // database user (least privilege: readWrite on the game database only)
      const password = randomPassword();
      secrets.add(password);
      try {
        await atlas.createDbUser(project.id, { username: DEFAULTS.dbUser, password, database, cluster: cluster.name });
      } catch (error) {
        if (!/exist|already|duplicate/i.test(error.message)) throw error;
        say(`The database user "${DEFAULTS.dbUser}" already exists: renewing its password.`);
        await atlas.deleteDbUser(project.id, DEFAULTS.dbUser);
        await atlas.createDbUser(project.id, { username: DEFAULTS.dbUser, password, database, cluster: cluster.name });
      }
      say(`Database user "${DEFAULTS.dbUser}" created (can read and write only the "${database}" database).`);

      // network access for this computer
      await atlas.allowCurrentIp(project.id, 'BoldInBeauty account service (this computer)');
      say('This computer is allowed to connect. (If you later run the server somewhere else, add that address in Atlas > Network Access.)');

      // connection string
      const srv = cluster.srv || (await atlas.srvHost(project.id, cluster.name));
      if (!srv) throw new Abort('Could not read the cluster address from Atlas. Open the cluster in Atlas > Connect > Drivers, copy the connection string, and run this again choosing "paste".');
      mongoUri = buildMongoUri(srv, DEFAULTS.dbUser, password, database);
      secrets.add(mongoUri);
    } else {
      say('Atlas: open your cluster > Connect > Drivers > copy the connection string (it starts with mongodb+srv://).');
      say('Replace <password> in it with the password of a database user that can read and write (Database Access > Add New Database User).');
      for (let attempt = 0; attempt < 3 && !mongoUri; attempt += 1) {
        const pasted = (await io.ask('Paste the connection string', { secret: true })).trim();
        if (!/^mongodb(\+srv)?:\/\//.test(pasted)) say('That does not start with mongodb:// or mongodb+srv://. Try again.');
        else if (/<password>|<db_password>/i.test(pasted)) say('It still contains the <password> placeholder: replace it with the real password.');
        else mongoUri = pasted;
      }
      if (!mongoUri) throw new Abort('No usable connection string.');
      secrets.add(mongoUri);
    }

    // prove it works (a new database user can take up to a minute to become active)
    say('Checking the database connection...');
    let mongoOk = false;
    let lastError = '';
    for (let attempt = 1; attempt <= 12 && !mongoOk; attempt += 1) {
      const result = await checkMongo(mongoUri, database);
      mongoOk = result.ok;
      if (!mongoOk) {
        lastError = result.error || 'unknown error';
        if (attempt < 12) {
          if (attempt === 1) say('Not ready yet (new users and network rules take a short while to apply); retrying for up to a minute...');
          await sleep(5000);
        }
      }
    }
    if (!mongoOk) {
      throw new Abort(`Could not connect to the database: ${redact(lastError)}\nCommon causes: a VPN or a changed public address (add it in Atlas > Network Access), or a wrong password. Nothing was written; run this again.`);
    }
    say('Database connection OK (collections and indexes are created on first start).');

    // ------------------------------------------------------------------ 2. e-mail
    step(2, 'E-mail sender (for the verification codes)');
    const method = await io.choose('Which mailbox sends the codes?', [
      { value: 'gmail', label: 'Google Workspace / Gmail mailbox with an App password (recommended; you have Workspace)' },
      { value: 'resend', label: 'Resend (API key; needs a verified sending domain)' },
      { value: 'smtp', label: 'Another SMTP service' },
      { value: 'none', label: 'Skip for now (codes are printed in the server window; for testing on this computer only)' },
    ]);

    const mail = { MAIL_PROVIDER: 'console' };
    let smtp = null;
    if (method === 'gmail') {
      const mailbox = (await io.ask('Mailbox that will send the codes', { default: DEFAULTS.ownerEmail })).trim();
      say('Make an App password: Google Account > Security > 2-Step Verification (must be on) > App passwords.');
      say('Direct link: https://myaccount.google.com/apppasswords  - name it "Boldinbeauty", copy the 16-letter password Google shows.');
      const appPassword = (await io.ask('Paste the 16-letter App password', { secret: true, validate: (v) => /^[a-z]{16}$/i.test(v.replace(/\s+/g, '')) || 'It should be 16 letters (spaces are ignored).' })).replace(/\s+/g, '');
      secrets.add(appPassword);
      smtp = { host: 'smtp.gmail.com', port: 587, secure: false, user: mailbox, pass: appPassword, from: `Boldinbeauty <${mailbox}>` };
    } else if (method === 'resend') {
      const key = (await io.ask('Paste your Resend API key (starts with re_)', { secret: true, validate: (v) => /^re_/.test(v.trim()) || 'It should start with re_' })).trim();
      secrets.add(key);
      const from = (await io.ask('Sender address on your verified domain, e.g. Boldinbeauty <no-reply@yourdomain.com>')).trim();
      Object.assign(mail, { MAIL_PROVIDER: 'resend', RESEND_API_KEY: key, MAIL_FROM: from });
    } else if (method === 'smtp') {
      const host = (await io.ask('SMTP host')).trim();
      const secure = await io.confirm('Does it use SSL from the start (usually port 465)? No means STARTTLS on port 587', false);
      const port = Number((await io.ask('SMTP port', { default: secure ? '465' : '587' })).trim());
      const user = (await io.ask('SMTP username')).trim();
      const pass = await io.ask('SMTP password', { secret: true });
      secrets.add(pass);
      const from = (await io.ask('Sender address, e.g. Boldinbeauty <no-reply@yourdomain.com>')).trim();
      smtp = { host, port, secure, user, pass, from };
    }

    if (smtp) {
      let mailOk = false;
      while (!mailOk) {
        say('Checking the mailbox login...');
        const result = await checkMail(smtp);
        if (result.ok) {
          mailOk = true;
          say('Mailbox login OK.');
          break;
        }
        say(`The mail server said no: ${redact(result.error || 'unknown error')}`);
        const next = await io.choose('What now?', [
          { value: 'retry', label: 'Try again (re-enter the password)' },
          { value: 'continue', label: 'Continue anyway (fix it later in .env)' },
          { value: 'skip', label: 'Skip e-mail for now (codes printed in the server window)' },
        ]);
        if (next === 'continue') break;
        if (next === 'skip') {
          smtp = null;
          break;
        }
        const fresh = await io.ask('Paste the password again', { secret: true });
        secrets.add(fresh);
        smtp = { ...smtp, pass: fresh.replace(/\s+/g, '') };
      }
      if (smtp) {
        Object.assign(mail, { MAIL_PROVIDER: 'smtp', MAIL_FROM: smtp.from, SMTP_HOST: smtp.host, SMTP_PORT: smtp.port, SMTP_SECURE: smtp.secure ? '1' : '0', SMTP_USER: smtp.user, SMTP_PASS: smtp.pass });
        if (await io.confirm(`Send a test e-mail to ${DEFAULTS.ownerEmail} now?`, true)) {
          const sent = await sendTestMail(smtp, DEFAULTS.ownerEmail);
          say(sent.ok ? `Test e-mail sent to ${DEFAULTS.ownerEmail}: check that inbox (and spam).` : `The test e-mail failed: ${redact(sent.error || 'unknown error')}`);
        }
      }
    }
    if (mail.MAIL_PROVIDER === 'console') {
      say('Codes will be printed in the server window instead of e-mailed. Run this setup again later to add a real sender.');
    }

    // ---------------------------------------------------------------- 3. write .env
    step(3, 'Saving');
    const values = {
      PORT: 8080,
      MONGODB_URI: mongoUri,
      MONGODB_DB: database,
      CODE_SECRET: crypto.randomBytes(32).toString('hex'),
      ADMIN_EMAILS: DEFAULTS.ownerEmail,
      ...mail,
      TRUST_PROXY_HOPS: 0,
    };
    if (await fs.exists('.env')) await fs.copy('.env', '.env.bak');
    await fs.write('.env', renderEnv(values), 0o600);
    say('Saved Server/.env (readable only by you; git ignores it).');

    say('\nAll set. To run the account server:   npm run start:env');
    say(`Then start the game as usual and choose Create account with ${DEFAULTS.ownerEmail}: that account is the owner/admin.`);
    say('(The game talks to http://127.0.0.1:8080 by default, which is this computer.)');
    return { ok: true, reason: 'done', mailProvider: mail.MAIL_PROVIDER };
  } catch (error) {
    if (error instanceof Abort) {
      say(`\nStopped: ${error.message}`);
      return { ok: false, reason: 'abort' };
    }
    say(`\nStopped: ${redact(error.message || String(error))}`);
    return { ok: false, reason: 'error' };
  }
}
