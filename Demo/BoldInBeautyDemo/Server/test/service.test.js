// The account rules end to end (service level), on every store.
import assert from 'node:assert/strict';
import { afterEach, beforeEach, describe, it } from 'node:test';
import { sha256Hex } from '../src/util.js';
import { OWNER, expectApiError, makeHarness, signUp, storeFactories } from './helpers.js';

function saveText(uid, savedAt, extra = {}) {
  return JSON.stringify({ format: 'boldinbeauty-save', version: 1, savedAt, userId: String(uid), profile: { day: 1, ...extra } });
}

for (const { label, make } of storeFactories()) {
  describe(`accounts: ${label}`, () => {
    let h;
    beforeEach(async () => { h = await makeHarness(make); });
    afterEach(async () => { await h.close(); });

    describe('sign up', () => {
      it('e-mails a 6-digit code, then verifying creates the account and logs in', async () => {
        const reply = await h.service.requestSignup({ username: 'Nova', email: 'Nova@Example.com' });
        assert.equal(reply.ok, true);
        assert.equal(h.mailer.sent.length, 1);
        assert.equal(h.mailer.last().to, 'nova@example.com');
        assert.match(h.mailer.last().subject, /\b\d{6}\b/);
        assert.match(h.mailer.last().text, /expires in 10 minutes/);
        assert.equal(await h.store.findAccountByEmail('nova@example.com'), null, 'no account before the e-mail is verified');

        const code = h.mailer.lastCodeFor('nova@example.com');
        const session = await h.service.verify({ email: 'nova@example.com', code });
        assert.equal(session.ok, true);
        assert.match(session.token, /^bib_[A-Za-z0-9_-]{43}$/);
        assert.deepEqual({ ...session.account, createdAt: undefined }, {
          userId: '10000001', username: 'Nova', email: 'nova@example.com', isAdmin: false, createdAt: undefined,
        });
        const me = await h.service.me(await h.service.authenticate(session.token));
        assert.equal(me.account.username, 'Nova');
        assert.equal(me.account.email, 'nova@example.com');
        assert.equal(me.account.userId, '10000001');
      });

      it('gives each new account the next user id', async () => {
        const a = await signUp(h, 'First', 'a@example.com');
        const b = await signUp(h, 'Second', 'b@example.com');
        assert.equal(a.account.userId, '10000001');
        assert.equal(b.account.userId, '10000002');
      });

      it('validates the e-mail and the username', async () => {
        for (const email of ['', 'nope', 'a@b', 'a@@b.com', 'a b@example.com', '.a@example.com', 'a..b@example.com', 'a@exa mple.com', 5, null, undefined, `${'a'.repeat(250)}@example.com`]) {
          await expectApiError(h.service.requestSignup({ username: 'Valid_Name', email }), 400, 'invalid_email');
        }
        for (const username of ['', 'ab', '1abc', 'a'.repeat(21), 'has space', 'semi;colon', 'ünicode', '<b>x</b>', null, 7, undefined]) {
          await expectApiError(h.service.requestSignup({ username, email: 'x@example.com' }), 400, 'invalid_username');
        }
        for (const username of ['Admin', 'owner', 'BOLDINBEAUTY', 'Support']) {
          await expectApiError(h.service.requestSignup({ username, email: 'x@example.com' }), 400, 'reserved_username');
        }
        assert.equal(h.mailer.sent.length, 0, 'nothing is sent for invalid input');
      });

      it('refuses a username or e-mail that is taken, ignoring letter case', async () => {
        await signUp(h, 'Nova', 'nova@example.com');
        await expectApiError(h.service.requestSignup({ username: 'nova', email: 'other@example.com' }), 409, 'username_taken');
        await expectApiError(h.service.requestSignup({ username: 'Other', email: 'NOVA@example.com' }), 409, 'email_taken');
      });

      it('does not reserve the username until the e-mail is verified, and re-checks at verification', async () => {
        await h.service.requestSignup({ username: 'Dupe', email: 'one@example.com' });
        await h.service.requestSignup({ username: 'Dupe', email: 'two@example.com' });
        const first = await h.service.verify({ email: 'one@example.com', code: h.mailer.lastCodeFor('one@example.com') });
        assert.equal(first.account.username, 'Dupe');
        await expectApiError(h.service.verify({ email: 'two@example.com', code: h.mailer.lastCodeFor('two@example.com') }), 409, 'username_taken');
        assert.equal(await h.store.findAccountByEmail('two@example.com'), null);
      });

      it('reports a mail failure and lets the user try again', async () => {
        h.mailer.failNext = true;
        await expectApiError(h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' }), 502, 'mail_failed');
        assert.equal(await h.store.getChallenge('nova@example.com'), null, 'the unsent code is discarded');
        h.advance(60);
        assert.equal((await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' })).ok, true);
      });
    });

    describe('the owner / admin account', () => {
      it('flags the owner e-mail as admin, whatever the letter case, and records the user id', async () => {
        const session = await signUp(h, 'Owner', 'admin@example.com');
        assert.equal(session.account.isAdmin, true);
        assert.equal(session.account.email, OWNER);
        assert.equal(session.account.username, 'Owner', 'the owner may use a reserved name');
        const stored = await h.store.findAccountByEmail(OWNER);
        assert.equal(stored.role, 'admin');
        if (h.store.admins instanceof Map) assert.equal(h.store.admins.get(stored.uid).email, OWNER);
        else assert.equal((await h.store.admins.findOne({ _id: stored.uid })).email, OWNER);
      });

      it('never flags anyone else', async () => {
        const session = await signUp(h, 'Nova', 'nova@example.com');
        assert.equal(session.account.isAdmin, false);
        const lookalikes = ['admin@example.co', 'xadmin@example.com', 'admin+1@example.com', 'admin@example.com.evil.io'];
        for (const email of lookalikes) {
          const s = await signUp(h, `User${lookalikes.indexOf(email)}`, email);
          assert.equal(s.account.isAdmin, false, email);
        }
      });

      it('re-reads the admin list at every log-in (adding or removing an admin takes effect)', async () => {
        await signUp(h, 'Nova', 'nova@example.com');
        assert.equal((await loginAs(h, 'nova@example.com')).account.isAdmin, false);
        const promoted = await makeHarnessSharing(h, { ADMIN_EMAILS: `${OWNER},nova@example.com` });
        assert.equal((await loginAs(promoted, 'nova@example.com')).account.isAdmin, true);
        const demoted = await makeHarnessSharing(h, { ADMIN_EMAILS: 'someone.else@example.com' });
        assert.equal((await loginAs(demoted, 'nova@example.com')).account.isAdmin, false);
      });
    });

    describe('log in', () => {
      it('e-mails a code to a known address and a verified code logs in', async () => {
        const created = await signUp(h, 'Nova', 'nova@example.com');
        const session = await loginAs(h, 'NOVA@example.com');
        assert.equal(session.account.userId, created.account.userId);
        assert.equal(session.account.username, 'Nova');
        assert.notEqual(session.token, created.token, 'every log-in gets its own session');
      });

      it('answers the same for an unknown address, and sends nothing', async () => {
        await signUp(h, 'Nova', 'nova@example.com');
        const known = await h.service.requestLogin({ email: 'nova@example.com' });
        const unknown = await h.service.requestLogin({ email: 'ghost@example.com' });
        assert.deepEqual(unknown, known);
        assert.equal(h.mailer.sent.filter((m) => m.to === 'ghost@example.com').length, 0);
        await expectApiError(h.service.verify({ email: 'ghost@example.com', code: '123456' }), 400, 'invalid_code');
      });

      it('still answers the same when the mail provider fails', async () => {
        await signUp(h, 'Nova', 'nova@example.com');
        h.mailer.failNext = true;
        const reply = await h.service.requestLogin({ email: 'nova@example.com' });
        assert.equal(reply.ok, true);
        assert.equal(await h.store.getChallenge('nova@example.com'), null);
      });

      it('a sign-up code cannot be used to log in as someone else, and vice versa', async () => {
        await signUp(h, 'Nova', 'nova@example.com');
        h.advance(60);
        await h.service.requestLogin({ email: 'nova@example.com' });
        const loginCode = h.mailer.lastCodeFor('nova@example.com');
        await expectApiError(h.service.verify({ email: 'other@example.com', code: loginCode }), 400, 'invalid_code');
        const session = await h.service.verify({ email: 'nova@example.com', code: loginCode });
        assert.equal(session.account.username, 'Nova');
      });
    });

    describe('codes', () => {
      it('work once', async () => {
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const code = h.mailer.lastCodeFor('nova@example.com');
        await h.service.verify({ email: 'nova@example.com', code });
        await expectApiError(h.service.verify({ email: 'nova@example.com', code }), 400, 'invalid_code');
      });

      it('work once even when two requests race', async () => {
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const code = h.mailer.lastCodeFor('nova@example.com');
        const results = await Promise.allSettled([1, 2, 3, 4].map(() => h.service.verify({ email: 'nova@example.com', code })));
        assert.equal(results.filter((r) => r.status === 'fulfilled').length, 1);
        assert.equal((await h.store.findAccountByEmail('nova@example.com')).username, 'Nova');
      });

      it('expire after the configured minutes', async () => {
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const code = h.mailer.lastCodeFor('nova@example.com');
        h.advance(10 * 60 + 1);
        await expectApiError(h.service.verify({ email: 'nova@example.com', code }), 400, 'invalid_code');
      });

      it('allow five wrong tries and then burn the code, even for the right digits', async () => {
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const code = h.mailer.lastCodeFor('nova@example.com');
        const wrong = code === '000000' ? '111111' : '000000';
        for (let tries = 1; tries <= 5; tries += 1) {
          const error = await expectApiError(h.service.verify({ email: 'nova@example.com', code: wrong }), 400, 'invalid_code');
          assert.equal(error.extra.attemptsLeft, 5 - tries);
        }
        await expectApiError(h.service.verify({ email: 'nova@example.com', code }), 400, 'invalid_code');
        assert.equal(await h.store.findAccountByEmail('nova@example.com'), null);
      });

      it('are stored hashed, never in plain', async () => {
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const code = h.mailer.lastCodeFor('nova@example.com');
        const challenge = await h.store.getChallenge('nova@example.com');
        assert.notEqual(challenge.codeHash, code);
        assert.match(challenge.codeHash, /^[0-9a-f]{64}$/);
        assert.equal(JSON.stringify(challenge).includes(code), false);
      });

      it('a new code replaces the old one', async () => {
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const first = h.mailer.lastCodeFor('nova@example.com');
        h.advance(60);
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const second = h.mailer.lastCodeFor('nova@example.com');
        if (first !== second) await expectApiError(h.service.verify({ email: 'nova@example.com', code: first }), 400, 'invalid_code');
        h.advance(60);
        // wrong tries above may have used some attempts, but a fresh challenge needs a fresh request
        await h.service.requestSignup({ username: 'Nova', email: 'nova@example.com' });
        const third = h.mailer.lastCodeFor('nova@example.com');
        assert.equal((await h.service.verify({ email: 'nova@example.com', code: third })).ok, true);
      });

      it('must be exactly six digits', async () => {
        for (const code of ['', '12345', '1234567', 'abcdef', '12 345', null, undefined, {}, ['123456']]) {
          await expectApiError(h.service.verify({ email: 'nova@example.com', code }), 400, 'invalid_code');
        }
        await expectApiError(h.service.verify({ email: 'bad', code: '123456' }), 400, 'invalid_email');
      });
    });

    describe('rate limits', () => {
      it('make the user wait between codes, the same for known and unknown addresses', async () => {
        await signUp(h, 'Nova', 'nova@example.com');
        h.advance(60);
        await h.service.requestLogin({ email: 'nova@example.com' });
        const known = await expectApiError(h.service.requestLogin({ email: 'nova@example.com' }), 429, 'too_soon');
        await h.service.requestLogin({ email: 'ghost@example.com' });
        const unknown = await expectApiError(h.service.requestLogin({ email: 'ghost@example.com' }), 429, 'too_soon');
        assert.deepEqual(known.extra, unknown.extra);
        h.advance(46);
        assert.equal((await h.service.requestLogin({ email: 'nova@example.com' })).ok, true);
      });

      it('cap the codes per address per hour', async () => {
        let sent = 0;
        let blocked = false;
        for (let attempt = 0; attempt < 9; attempt += 1) {
          h.advance(50);
          try { await h.service.requestLogin({ email: 'ghost@example.com' }); sent += 1; } catch (error) { assert.equal(error.code, 'rate_limited'); blocked = true; break; }
        }
        assert.equal(sent, 6);
        assert.equal(blocked, true);
        h.advance(3600);
        assert.equal((await h.service.requestLogin({ email: 'ghost@example.com' })).ok, true);
      });

      it('limit how often one address can be guessed at', async () => {
        let limited = false;
        for (let attempt = 0; attempt < 25; attempt += 1) {
          try { await h.service.verify({ email: 'nova@example.com', code: '123456' }); } catch (error) { if (error.code === 'rate_limited') { limited = true; break; } }
        }
        assert.equal(limited, true);
      });
    });

    describe('sessions', () => {
      it('store only a hash of the token', async () => {
        const session = await signUp(h, 'Nova', 'nova@example.com');
        assert.ok(await h.store.getSession(sha256Hex(session.token)));
        assert.equal(await h.store.getSession(session.token), null);
      });

      it('expire after the configured days', async () => {
        const session = await signUp(h, 'Nova', 'nova@example.com');
        h.advance(29 * 86_400);
        assert.ok(await h.service.authenticate(session.token));
        h.advance(2 * 86_400);
        await expectApiError(h.service.authenticate(session.token), 401, 'unauthorized');
      });

      it('end at log-out, and only that session', async () => {
        const first = await signUp(h, 'Nova', 'nova@example.com');
        const second = await loginAs(h, 'nova@example.com');
        await h.service.logout(first.token);
        await expectApiError(h.service.authenticate(first.token), 401, 'unauthorized');
        assert.ok(await h.service.authenticate(second.token));
        assert.deepEqual(await h.service.logout('garbage'), { ok: true });
      });

      it('reject missing, malformed and forged tokens', async () => {
        for (const token of ['', undefined, null, 'bib_', 'bib_nope', 'x'.repeat(500), 'Bearer bib_x', 42]) {
          await expectApiError(h.service.authenticate(token), 401, 'unauthorized');
        }
      });
    });

    describe('cloud save', () => {
      it('stores and returns a save', async () => {
        const session = await signUp(h, 'Nova', 'nova@example.com');
        const account = await h.service.authenticate(session.token);
        assert.deepEqual(await h.service.getSave(account), { ok: true, save: null, savedAt: 0 });
        const now = Math.floor(h.clock.time.getTime() / 1000);
        const text = saveText(account.uid, now);
        assert.deepEqual(await h.service.putSave(account, { save: text }), { ok: true, savedAt: now });
        const back = await h.service.getSave(account);
        assert.equal(back.save, text);
        assert.equal(back.savedAt, now);
      });

      it('refuses an older save than the one stored, and says how new the stored one is', async () => {
        const account = await h.service.authenticate((await signUp(h, 'Nova', 'nova@example.com')).token);
        const now = Math.floor(h.clock.time.getTime() / 1000);
        await h.service.putSave(account, { save: saveText(account.uid, now) });
        const error = await expectApiError(h.service.putSave(account, { save: saveText(account.uid, now - 100) }), 409, 'stale_save');
        assert.equal(error.extra.savedAt, now);
        assert.deepEqual(await h.service.putSave(account, { save: saveText(account.uid, now + 5) }), { ok: true, savedAt: now + 5 });
      });

      it('only accepts a save written for that account', async () => {
        const nova = await h.service.authenticate((await signUp(h, 'Nova', 'nova@example.com')).token);
        const other = await h.service.authenticate((await signUp(h, 'Other', 'other@example.com')).token);
        const now = Math.floor(h.clock.time.getTime() / 1000);
        await expectApiError(h.service.putSave(nova, { save: saveText(other.uid, now) }), 400, 'wrong_account');
        await expectApiError(h.service.putSave(nova, { save: saveText('', now) }), 400, 'wrong_account');
        assert.equal((await h.service.getSave(other)).save, null);
      });

      it('refuses malformed, oversized and far-future saves', async () => {
        const account = await h.service.authenticate((await signUp(h, 'Nova', 'nova@example.com')).token);
        const now = Math.floor(h.clock.time.getTime() / 1000);
        for (const save of [undefined, null, '', 5, 'not json', '[]', '{}', JSON.stringify({ format: 'other', savedAt: now, userId: String(account.uid), profile: {} }),
          JSON.stringify({ format: 'boldinbeauty-save', savedAt: now, userId: String(account.uid) }),
          saveText(account.uid, 0), saveText(account.uid, -5), saveText(account.uid, 1.5), saveText(account.uid, now + 3 * 86_400)]) {
          await expectApiError(h.service.putSave(account, { save }), 400, 'invalid_save');
        }
        await expectApiError(h.service.putSave(account, { save: saveText(account.uid, now, { pad: 'x'.repeat(1_100_000) }) }), 413, 'save_too_large');
        await expectApiError(h.service.putSave(account, null), 400, 'invalid_save');
        assert.equal((await h.service.getSave(account)).save, null);
      });

      it('keeps each account\'s save separate', async () => {
        const nova = await h.service.authenticate((await signUp(h, 'Nova', 'nova@example.com')).token);
        const other = await h.service.authenticate((await signUp(h, 'Other', 'other@example.com')).token);
        const now = Math.floor(h.clock.time.getTime() / 1000);
        await h.service.putSave(nova, { save: saveText(nova.uid, now, { who: 'nova' }) });
        await h.service.putSave(other, { save: saveText(other.uid, now, { who: 'other' }) });
        assert.match((await h.service.getSave(nova)).save, /nova/);
        assert.match((await h.service.getSave(other)).save, /other/);
      });
    });
  });
}

async function loginAs(h, email) {
  h.advance(60);
  await h.service.requestLogin({ email });
  return h.service.verify({ email, code: h.mailer.lastCodeFor(email.trim().toLowerCase()) });
}

// A second service over the SAME store with a different configuration (for the admin-list tests).
async function makeHarnessSharing(h, env) {
  const { loadConfig } = await import('../src/config.js');
  const { AccountService } = await import('../src/service.js');
  const config = loadConfig({ ALLOW_MEMORY_STORE: '1', MAIL_PROVIDER: 'console', CODE_SECRET: h.config.codeSecret, ...env });
  const service = new AccountService({ store: h.store, mailer: h.mailer, config, now: () => new Date(h.clock.time), log: () => {} });
  return { ...h, config, service };
}
