import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { ConfigError, describeConfig, loadConfig } from '../src/config.js';
import { codeEmail } from '../src/mailer.js';
import { maskEmail, normalizeEmail, safeEqualHex, validateUsername } from '../src/util.js';

const GOOD = {
  MONGODB_URI: 'mongodb://127.0.0.1:27017', CODE_SECRET: 'x'.repeat(40), MAIL_PROVIDER: 'smtp', MAIL_FROM: 'a@b.co',
  SMTP_HOST: 'smtp.example.com', SMTP_USER: 'u', SMTP_PASS: 'p',
};

describe('configuration', () => {
  it('accepts a complete production setup and defaults the admin to the owner', () => {
    const config = loadConfig({ ...GOOD, NODE_ENV: 'production' });
    assert.deepEqual([...config.adminEmails], ['admin@example.com']);
    assert.equal(config.memoryStore, false);
    assert.equal(config.codeMinutes, 10);
  });

  it('lists every problem at once', () => {
    assert.throws(() => loadConfig({}), (error) => error instanceof ConfigError && error.problems.length >= 3);
    assert.throws(() => loadConfig({ ...GOOD, CODE_SECRET: 'short' }), /CODE_SECRET/);
    assert.throws(() => loadConfig({ ...GOOD, MAIL_PROVIDER: 'pigeon' }), /MAIL_PROVIDER/);
    assert.throws(() => loadConfig({ ...GOOD, SMTP_PASS: '' }), /SMTP_HOST, SMTP_USER and SMTP_PASS/);
    assert.throws(() => loadConfig({ ...GOOD, MAIL_PROVIDER: 'resend' }), /RESEND_API_KEY/);
    assert.throws(() => loadConfig({ ...GOOD, ADMIN_EMAILS: 'not-an-email' }), /ADMIN_EMAILS/);
    assert.throws(() => loadConfig({ ...GOOD, PORT: 'abc' }), /PORT/);
    assert.throws(() => loadConfig({ ...GOOD, ALLOW_MEMORY_STORE: '1' }), /not both/);
  });

  it('refuses console mail in production, and the memory store without opt-in', () => {
    assert.throws(() => loadConfig({ ...GOOD, MAIL_PROVIDER: 'console', NODE_ENV: 'production' }), /console/);
    assert.throws(() => loadConfig({ CODE_SECRET: 'x'.repeat(40), MAIL_PROVIDER: 'console' }), /MONGODB_URI/);
    const dev = loadConfig({ ALLOW_MEMORY_STORE: '1' });
    assert.equal(dev.mail.provider, 'console');
    assert.ok(dev.codeSecret.length >= 32, 'development gets a random secret');
  });

  it('takes several admin e-mails, normalised', () => {
    const config = loadConfig({ ...GOOD, ADMIN_EMAILS: ' One@Example.com, two@example.com ,' });
    assert.deepEqual([...config.adminEmails].sort(), ['one@example.com', 'two@example.com']);
  });

  it('describes itself without secrets', () => {
    const text = JSON.stringify(describeConfig(loadConfig({ ...GOOD, SMTP_PASS: 'distinctive-smtp-password', MONGODB_URI: 'mongodb://user:distinctive-db-password@host/db' })));
    assert.equal(text.includes(GOOD.CODE_SECRET), false);
    assert.equal(text.includes('distinctive-smtp-password'), false);
    assert.equal(text.includes('distinctive-db-password'), false);
  });
});

describe('validation helpers', () => {
  it('normalises e-mail addresses', () => {
    assert.equal(normalizeEmail('  admin@example.com '), 'admin@example.com');
    assert.equal(normalizeEmail('first.last+tag@sub.example.co.uk'), 'first.last+tag@sub.example.co.uk');
    for (const bad of ['', 'a', 'a@b', 'a@b.', '@b.com', 'a@.com', 'a@b..com', 'a@b.c', 'a b@c.com', 'a@b.com,c@d.com', 'a@-b.com', 'a@b-.com', 'a@[1.2.3.4]']) {
      assert.equal(normalizeEmail(bad), null, JSON.stringify(bad));
    }
  });

  it('validates usernames', () => {
    assert.deepEqual(validateUsername(' Nova_7 '), { ok: true, username: 'Nova_7' });
    assert.equal(validateUsername('admin').code, 'reserved_username');
    assert.equal(validateUsername('admin', { allowReserved: true }).ok, true);
    for (const bad of ['', 'ab', '_abc', '9abc', 'a-b-c', 'a'.repeat(21)]) assert.equal(validateUsername(bad).ok, false, bad);
  });

  it('compares hashes safely', () => {
    assert.equal(safeEqualHex('ab12', 'ab12'), true);
    assert.equal(safeEqualHex('ab12', 'ab13'), false);
    assert.equal(safeEqualHex('ab12', 'ab1'), false);
    assert.equal(safeEqualHex('zz', 'zz'), false);
    assert.equal(safeEqualHex(undefined, 'ab'), false);
  });

  it('masks e-mails for the logs', () => {
    assert.equal(maskEmail('nova@example.com'), 'n***@example.com');
    assert.equal(maskEmail(undefined), '***');
  });

  it('writes a plain, link-free code e-mail', () => {
    const message = codeEmail({ code: '042517', minutes: 10, purpose: 'signup' });
    assert.match(message.subject, /042517/);
    assert.match(message.text, /042517/);
    assert.equal(/https?:\/\//.test(message.text + message.html), false, 'no links to phish with');
  });
});
