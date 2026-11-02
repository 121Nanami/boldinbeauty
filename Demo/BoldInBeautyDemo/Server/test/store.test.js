// The store contract: every store (memory, MongoDB) must behave the same.
import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';
import { StoreConflict } from '../src/store/errors.js';
import { storeFactories } from './helpers.js';

function account(uid, name, email) {
  const now = new Date();
  return { uid, username: name, usernameLower: name.toLowerCase(), email, emailLower: email, role: 'player', createdAt: now, emailVerifiedAt: now, lastLoginAt: now };
}

for (const { label, make } of storeFactories()) {
  describe(`store contract: ${label}`, () => {
    let store;
    before(async () => { store = await make(); });
    after(async () => { await store.close(); });

    it('hands out unique, sequential user ids starting at 10000001, even in parallel', async () => {
      const ids = await Promise.all(Array.from({ length: 25 }, () => store.nextUid()));
      assert.equal(new Set(ids).size, 25);
      assert.equal(Math.min(...ids), 10_000_001);
      assert.equal(Math.max(...ids), 10_000_025);
    });

    it('creates and finds accounts by e-mail, username and id', async () => {
      await store.createAccount(account(500, 'Nova', 'nova@example.com'));
      assert.equal((await store.findAccountByEmail('nova@example.com')).username, 'Nova');
      assert.equal((await store.findAccountByUsername('nova')).uid, 500);
      assert.equal((await store.findAccountByUid(500)).email, 'nova@example.com');
      assert.equal(await store.findAccountByEmail('nobody@example.com'), null);
      assert.equal(await store.findAccountByUid(501), null);
    });

    it('refuses a second account with the same e-mail or username', async () => {
      await assert.rejects(store.createAccount(account(501, 'Other', 'nova@example.com')), (e) => e instanceof StoreConflict && e.field === 'email');
      await assert.rejects(store.createAccount(account(502, 'NOVA', 'other@example.com')), (e) => e instanceof StoreConflict && e.field === 'username');
      assert.equal(await store.findAccountByUid(501), null);
      assert.equal(await store.findAccountByUid(502), null);
    });

    it('lets only one of two parallel sign-ups for the same e-mail win', async () => {
      const results = await Promise.allSettled([
        store.createAccount(account(510, 'RaceOne', 'race@example.com')),
        store.createAccount(account(511, 'RaceTwo', 'race@example.com')),
      ]);
      assert.equal(results.filter((r) => r.status === 'fulfilled').length, 1);
    });

    it('updates an account and flags / unflags admins', async () => {
      await store.updateAccount(500, { role: 'admin' });
      assert.equal((await store.findAccountByUid(500)).role, 'admin');
      const t1 = new Date('2026-10-04T10:00:00Z');
      await store.upsertAdmin(500, 'nova@example.com', t1);
      await store.upsertAdmin(500, 'nova@example.com', new Date('2026-10-05T10:00:00Z'));
      await store.removeAdmin(500);
      await store.removeAdmin(500); // idempotent
    });

    it('keeps one pending code per e-mail, resets tries when a new one is issued', async () => {
      const base = { emailLower: 'c@example.com', type: 'login', username: null, codeHash: 'aa', createdAt: new Date(), expiresAt: new Date(Date.now() + 600_000) };
      await store.putChallenge(base);
      assert.equal(await store.bumpAttempts('c@example.com'), 1);
      assert.equal(await store.bumpAttempts('c@example.com'), 2);
      await store.putChallenge({ ...base, codeHash: 'bb' });
      const fresh = await store.getChallenge('c@example.com');
      assert.equal(fresh.codeHash, 'bb');
      assert.equal(fresh.attempts, 0);
      assert.equal(await store.bumpAttempts('missing@example.com'), null);
    });

    it('counts tries atomically', async () => {
      await store.putChallenge({ emailLower: 'd@example.com', type: 'login', username: null, codeHash: 'aa', createdAt: new Date(), expiresAt: new Date(Date.now() + 600_000) });
      const counts = await Promise.all(Array.from({ length: 12 }, () => store.bumpAttempts('d@example.com')));
      assert.deepEqual([...counts].sort((a, b) => a - b), Array.from({ length: 12 }, (_, i) => i + 1));
    });

    it('lets exactly one caller take a code', async () => {
      await store.putChallenge({ emailLower: 'e@example.com', type: 'signup', username: 'Eve', codeHash: 'aa', createdAt: new Date(), expiresAt: new Date(Date.now() + 600_000) });
      const taken = await Promise.all(Array.from({ length: 8 }, () => store.takeChallenge('e@example.com')));
      assert.equal(taken.filter(Boolean).length, 1);
      assert.equal(taken.find(Boolean).username, 'Eve');
      assert.equal(await store.getChallenge('e@example.com'), null);
    });

    it('stores sessions by hash and deletes them one by one or per account', async () => {
      const later = new Date(Date.now() + 86_400_000);
      await store.createSession({ tokenHash: 'h1', uid: 700, createdAt: new Date(), expiresAt: later });
      await store.createSession({ tokenHash: 'h2', uid: 700, createdAt: new Date(), expiresAt: later });
      await store.createSession({ tokenHash: 'h3', uid: 701, createdAt: new Date(), expiresAt: later });
      assert.equal((await store.getSession('h1')).uid, 700);
      assert.equal(await store.getSession('nope'), null);
      assert.equal(await store.deleteSession('h1'), true);
      assert.equal(await store.deleteSession('h1'), false);
      assert.equal(await store.deleteSessionsForUid(700), 1);
      assert.equal(await store.getSession('h2'), null);
      assert.ok(await store.getSession('h3'));
    });

    it('keeps the newest save and reports a stale one', async () => {
      const now = new Date();
      assert.equal(await store.getSave(800), null);
      assert.deepEqual(await store.putSave(800, 'first', 100, now), { ok: true });
      assert.deepEqual(await store.putSave(800, 'newer', 200, now), { ok: true });
      assert.deepEqual(await store.putSave(800, 'same time', 200, now), { ok: true });
      assert.deepEqual(await store.putSave(800, 'older', 150, now), { stale: true, savedAt: 200 });
      const save = await store.getSave(800);
      assert.equal(save.data, 'same time');
      assert.equal(save.savedAt, 200);
    });

    it('counts hits in a fixed window and starts a new window after it', async () => {
      const t0 = new Date('2026-10-04T10:00:00Z');
      assert.equal(await store.hit('k', 60, t0), 1);
      assert.equal(await store.hit('k', 60, new Date(t0.getTime() + 30_000)), 2);
      assert.equal(await store.hit('k', 60, new Date(t0.getTime() + 59_000)), 3);
      assert.equal(await store.hit('k', 60, new Date(t0.getTime() + 61_000)), 1);
      assert.equal(await store.hit('other', 60, t0), 1);
    });
  });
}
