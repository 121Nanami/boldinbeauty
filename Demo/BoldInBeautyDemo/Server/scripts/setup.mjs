#!/usr/bin/env node
// Guided setup for the account service. Run from the Server folder:   npm install && npm run setup
// The logic is in scripts/lib/setup-core.mjs (unit-tested with fakes); this file wires the real terminal, processes and files.
import { spawn } from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import readline from 'node:readline';
import { Writable } from 'node:stream';
import { fileURLToPath } from 'node:url';
import { SmtpMailer } from '../src/mailer.js';
import { MongoStore } from '../src/store/mongo.js';
import { runSetup } from './lib/setup-core.mjs';

const serverDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const tty = Boolean(process.stdin.isTTY);

// ------------------------------------------------------------------ terminal
let sharedLines = null;
const nextLine = async () => {
  if (!sharedLines) {
    sharedLines = readline.createInterface({ input: process.stdin, terminal: false })[Symbol.asyncIterator]();
  }
  const { value, done } = await sharedLines.next();
  if (done) throw new Error('The input ended before the setup was finished.');
  return value;
};

// A typed-in secret is not echoed. (In a terminal each prompt gets its own readline whose output is muted after the question.)
function readLine(query, secret) {
  if (!tty) {
    process.stdout.write(query);
    return nextLine().then((line) => {
      process.stdout.write('\n');
      return line;
    });
  }
  let muted = false;
  const out = new Writable({
    write(chunk, encoding, callback) {
      if (!muted) process.stdout.write(chunk, encoding);
      callback();
    },
  });
  const rl = readline.createInterface({ input: process.stdin, output: out, terminal: true });
  return new Promise((resolve) => {
    rl.question(query, (answer) => {
      rl.close();
      process.stdout.write('\n');
      resolve(answer);
    });
    muted = Boolean(secret);
  });
}

const io = {
  say: (text) => console.log(text),
  async ask(question, { default: fallback, secret = false, validate } = {}) {
    for (;;) {
      const suffix = fallback ? ` [${fallback}]` : '';
      const raw = await readLine(`${question}${suffix}: `, secret);
      const value = raw.trim() === '' && fallback !== undefined ? fallback : raw;
      const verdict = validate ? validate(value) : true;
      if (verdict === true) return value;
      console.log(typeof verdict === 'string' ? verdict : 'That does not look right. Try again.');
    }
  },
  async choose(question, options) {
    console.log(`\n${question}`);
    options.forEach((option, index) => console.log(`  ${index + 1}) ${option.label}`));
    for (;;) {
      const raw = (await readLine(`Enter a number [1]: `, false)).trim();
      const index = raw === '' ? 0 : Number(raw) - 1;
      if (Number.isInteger(index) && index >= 0 && index < options.length) return options[index].value;
      console.log(`Please type a number from 1 to ${options.length}.`);
    }
  },
  async confirm(question, fallback = true) {
    for (;;) {
      const raw = (await readLine(`${question} ${fallback ? '[Y/n]' : '[y/N]'}: `, false)).trim().toLowerCase();
      if (raw === '') return fallback;
      if (['y', 'yes'].includes(raw)) return true;
      if (['n', 'no'].includes(raw)) return false;
    }
  },
};

// ------------------------------------------------------------------ processes and files
function exec(command, args, { inherit = false } = {}) {
  return new Promise((resolve) => {
    const child = spawn(command, args, { stdio: inherit ? 'inherit' : ['ignore', 'pipe', 'pipe'], cwd: serverDir });
    let stdout = '';
    let stderr = '';
    child.stdout?.on('data', (chunk) => { stdout += chunk; });
    child.stderr?.on('data', (chunk) => { stderr += chunk; });
    child.on('error', (error) => resolve({ code: 127, stdout, stderr: `${stderr}${error.message}` }));
    child.on('close', (code) => resolve({ code: code ?? 1, stdout, stderr }));
  });
}

const files = {
  exists: async (name) => fs.access(path.join(serverDir, name)).then(() => true, () => false),
  write: async (name, text, mode) => {
    await fs.writeFile(path.join(serverDir, name), text, { mode });
    await fs.chmod(path.join(serverDir, name), mode); // writeFile keeps the mode of an existing file
  },
  copy: async (from, to) => {
    await fs.copyFile(path.join(serverDir, from), path.join(serverDir, to));
    await fs.chmod(path.join(serverDir, to), 0o600);
  },
};

// ------------------------------------------------------------------ checks
async function checkMongo(uri, database) {
  const store = new MongoStore({ uri, dbName: database });
  try {
    await store.init(); // connects, authenticates, creates the indexes
    await store.client.db(database).command({ ping: 1 });
    return { ok: true };
  } catch (error) {
    return { ok: false, error: String(error.message || error) };
  } finally {
    await store.close().catch(() => {});
  }
}

const mailer = (smtp) => new SmtpMailer({ from: smtp.from, smtp: { host: smtp.host, port: smtp.port, secure: smtp.secure, user: smtp.user, pass: smtp.pass } }, () => {});

async function checkMail(smtp) {
  try {
    await mailer(smtp).verify();
    return { ok: true };
  } catch (error) {
    return { ok: false, error: String(error.message || error) };
  }
}

async function sendTestMail(smtp, to) {
  try {
    await mailer(smtp).send({
      to,
      subject: 'Boldinbeauty account service: test e-mail',
      text: 'This is the test e-mail from the Boldinbeauty account service setup. If you can read it, verification codes will reach players.',
      html: '<p>This is the test e-mail from the Boldinbeauty account service setup. If you can read it, verification codes will reach players.</p>',
    });
    return { ok: true };
  } catch (error) {
    return { ok: false, error: String(error.message || error) };
  }
}

const result = await runSetup({
  io, exec, fs: files, checkMongo, checkMail, sendTestMail,
  platformHasBrew: async () => (await exec('brew', ['--version'])).code === 0,
});
if (!result.ok) process.exit(1);

// Finished (or an existing .env was kept): offer to run the server right here, so the owner needs just this one command.
if (await io.confirm('Start the account server now? (keep this window open while you play; Ctrl-C stops it)', true)) {
  const server = spawn(process.execPath, ['--env-file=.env', 'src/index.js'], { cwd: serverDir, stdio: 'inherit' });
  server.on('exit', (code) => process.exit(code ?? 0));
} else {
  process.exit(0);
}
