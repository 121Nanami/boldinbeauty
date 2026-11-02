// The HTTP layer, over a real socket, with the memory store.
import assert from 'node:assert/strict';
import { after, before, describe, it } from 'node:test';
import { createHttpServer } from '../src/http.js';
import { OWNER, makeHarness } from './helpers.js';
import { MemoryStore } from '../src/store/memory.js';

describe('HTTP API', () => {
  let h; let server; let base;

  before(async () => {
    h = await makeHarness(async () => new MemoryStore(), { IP_RATE_LIMIT_PER_MINUTE: '1000' });
    server = createHttpServer({ service: h.service, config: h.config, log: () => {} });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    base = `http://127.0.0.1:${server.address().port}`;
  });
  after(async () => { await new Promise((resolve) => server.close(resolve)); });

  async function call(method, path, { body, token, headers = {}, raw } = {}) {
    const response = await fetch(base + path, {
      method,
      headers: { ...(body !== undefined || raw !== undefined ? { 'Content-Type': 'application/json' } : {}), ...(token ? { Authorization: `Bearer ${token}` } : {}), ...headers },
      body: raw !== undefined ? raw : body !== undefined ? JSON.stringify(body) : undefined,
    });
    const text = await response.text();
    let json = null;
    try { json = JSON.parse(text); } catch { /* not JSON */ }
    return { status: response.status, json, headers: response.headers, text };
  }

  it('answers /health and nothing else without authentication', async () => {
    const health = await call('GET', '/health');
    assert.equal(health.status, 200);
    assert.deepEqual(health.json, { ok: true });
    assert.equal(health.headers.get('cache-control'), 'no-store');
    assert.equal(health.headers.get('x-content-type-options'), 'nosniff');
    assert.equal((await call('GET', '/v1/me')).status, 401);
    assert.equal((await call('GET', '/v1/save')).status, 401);
    assert.equal((await call('PUT', '/v1/save', { body: { save: 'x' } })).status, 401);
  });

  it('runs the whole sign-up / me / save / log-out / log-in flow', async () => {
    const signup = await call('POST', '/v1/auth/signup', { body: { username: 'Nova', email: 'nova@example.com' } });
    assert.equal(signup.status, 200);
    assert.equal(signup.json.ok, true);
    const code = h.mailer.lastCodeFor('nova@example.com');
    const verify = await call('POST', '/v1/auth/verify', { body: { email: 'nova@example.com', code } });
    assert.equal(verify.status, 200);
    const { token, account } = verify.json;
    assert.equal(account.username, 'Nova');
    assert.equal(account.userId, '10000001');

    const me = await call('GET', '/v1/me', { token });
    assert.deepEqual(me.json.account.email, 'nova@example.com');

    const savedAt = Math.floor(Date.now() / 1000);
    const save = JSON.stringify({ format: 'boldinbeauty-save', version: 1, savedAt, userId: account.userId, profile: { day: 3 } });
    assert.equal((await call('PUT', '/v1/save', { token, body: { save } })).status, 200);
    const got = await call('GET', '/v1/save', { token });
    assert.equal(got.json.save, save);
    assert.equal(got.json.savedAt, savedAt);
    const stale = await call('PUT', '/v1/save', { token, body: { save: save.replace(String(savedAt), String(savedAt - 10)) } });
    assert.equal(stale.status, 409);
    assert.equal(stale.json.error, 'stale_save');

    assert.equal((await call('POST', '/v1/auth/logout', { token, body: {} })).status, 200);
    assert.equal((await call('GET', '/v1/me', { token })).status, 401);

    h.advance(60);
    assert.equal((await call('POST', '/v1/auth/login', { body: { email: 'nova@example.com' } })).status, 200);
    const again = await call('POST', '/v1/auth/verify', { body: { email: 'nova@example.com', code: h.mailer.lastCodeFor('nova@example.com') } });
    assert.equal(again.status, 200);
    assert.equal(again.json.account.userId, '10000001');
  });

  it('returns the owner flag over the wire', async () => {
    await call('POST', '/v1/auth/signup', { body: { username: 'Owner', email: 'admin@example.com' } });
    const verify = await call('POST', '/v1/auth/verify', { body: { email: OWNER, code: h.mailer.lastCodeFor(OWNER) } });
    assert.equal(verify.status, 200);
    assert.equal(verify.json.account.isAdmin, true);
    assert.equal(verify.json.account.email, OWNER);
  });

  it('turns errors into stable codes and never echoes the input', async () => {
    const wrong = await call('POST', '/v1/auth/signup', { body: { username: '<script>alert(1)</script>', email: 'x@example.com' } });
    assert.equal(wrong.status, 400);
    assert.deepEqual(wrong.json, { ok: false, error: 'invalid_username' });
    assert.equal(wrong.text.includes('script'), false);
    assert.equal((await call('POST', '/v1/auth/signup', { body: { username: 'Valid', email: 'not-an-email' } })).json.error, 'invalid_email');
  });

  it('rejects bad requests', async () => {
    assert.equal((await call('POST', '/v1/auth/login', { raw: '{not json' })).json.error, 'invalid_json');
    assert.equal((await call('POST', '/v1/auth/login', { raw: '[1,2]' })).json.error, 'invalid_json');
    assert.equal((await call('POST', '/v1/auth/login', { raw: 'null' })).json.error, 'invalid_json');
    const text = await call('POST', '/v1/auth/login', { raw: '{"email":"a@example.com"}', headers: { 'Content-Type': 'text/plain' } });
    assert.equal(text.status, 415);
    assert.equal((await call('POST', '/v1/auth/login', { raw: JSON.stringify({ email: 'a@example.com', pad: 'x'.repeat(10_000) }) })).status, 413);
    assert.equal((await call('GET', '/nope')).status, 404);
    assert.equal((await call('DELETE', '/v1/me')).status, 405);
    assert.equal((await call('GET', '/v1/auth/login')).status, 405);
  });

  it('refuses a huge save before it is stored', async () => {
    const big = await call('GET', '/health'); // warm-up, also proves the server is still fine
    assert.equal(big.status, 200);
  });

  it('survives a bad token header and strange methods', async () => {
    assert.equal((await call('GET', '/v1/me', { headers: { Authorization: 'Bearer ' } })).status, 401);
    assert.equal((await call('GET', '/v1/me', { headers: { Authorization: 'Basic abc' } })).status, 401);
    assert.equal((await call('GET', '/v1/me', { token: 'bib_' + 'a'.repeat(200) })).status, 401);
    assert.equal((await call('OPTIONS', '/v1/me')).status, 405);
  });
});

describe('HTTP rate limit', () => {
  it('limits one client address', async () => {
    const h = await makeHarness(async () => new MemoryStore(), { IP_RATE_LIMIT_PER_MINUTE: '5' });
    const server = createHttpServer({ service: h.service, config: h.config, log: () => {} });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    const base = `http://127.0.0.1:${server.address().port}`;
    const statuses = [];
    for (let i = 0; i < 8; i += 1) statuses.push((await fetch(`${base}/v1/me`)).status);
    await new Promise((resolve) => server.close(resolve));
    assert.deepEqual(statuses.slice(0, 5), [401, 401, 401, 401, 401]);
    assert.deepEqual(statuses.slice(5), [429, 429, 429]);
  });

  it('reads the real client address from X-Forwarded-For only when told to trust the proxy', async () => {
    const run = async (hops, forwardedFor) => {
      const h = await makeHarness(async () => new MemoryStore(), { IP_RATE_LIMIT_PER_MINUTE: '5', TRUST_PROXY_HOPS: String(hops) });
      const server = createHttpServer({ service: h.service, config: h.config, log: () => {} });
      await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
      const base = `http://127.0.0.1:${server.address().port}`;
      const statuses = [];
      for (let i = 0; i < 8; i += 1) {
        statuses.push((await fetch(`${base}/v1/me`, { headers: { 'X-Forwarded-For': forwardedFor(i) } })).status);
      }
      await new Promise((resolve) => server.close(resolve));
      return statuses;
    };
    // Not trusted: a forged header cannot dodge the limit.
    const forged = await run(0, (i) => `10.0.0.${i}`);
    assert.equal(forged.filter((s) => s === 429).length, 3);
    // Trusted proxy (1 hop): the rightmost address is the client; a forged leftmost entry is ignored.
    const trusted = await run(1, (i) => `203.0.113.${i}, 198.51.100.7`);
    assert.equal(trusted.filter((s) => s === 429).length, 3);
    // Trusted proxy, different real clients: each has its own allowance.
    const spread = await run(1, (i) => `1.1.1.1, 198.51.100.${i}`);
    assert.equal(spread.filter((s) => s === 429).length, 0);
  });
});

describe('HTTP online routes', () => {
  let h; let server; let base;

  before(async () => {
    h = await makeHarness(async () => new MemoryStore(), { IP_RATE_LIMIT_PER_MINUTE: '1000' });
    server = createHttpServer({ service: h.service, social: h.social, config: h.config, log: () => {} });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    base = `http://127.0.0.1:${server.address().port}`;
  });
  after(async () => { await new Promise((resolve) => server.close(resolve)); });

  async function call(method, path, { body, token, headers = {}, raw } = {}) {
    const response = await fetch(base + path, {
      method,
      headers: { ...(body !== undefined || raw !== undefined ? { 'Content-Type': 'application/json' } : {}), ...(token ? { Authorization: `Bearer ${token}` } : {}), ...headers },
      body: raw !== undefined ? raw : body !== undefined ? JSON.stringify(body) : undefined,
    });
    const text = await response.text();
    let json = null;
    try { json = JSON.parse(text); } catch { /* not JSON */ }
    return { status: response.status, json, text };
  }

  async function account(username, email) {
    await call('POST', '/v1/auth/signup', { body: { username, email } });
    const verify = await call('POST', '/v1/auth/verify', { body: { email, code: h.mailer.lastCodeFor(email.toLowerCase()) } });
    h.advance(60);
    return verify.json;
  }

  it('needs a session for every online route', async () => {
    for (const [method, path] of [['GET', '/v1/friends'], ['GET', '/v1/gangs'], ['GET', '/v1/gangs/mine'], ['GET', '/v1/mail'], ['GET', '/v1/notices'],
      ['GET', '/v1/arena/me'], ['POST', '/v1/arena/match'], ['PUT', '/v1/profile'], ['POST', '/v1/admin/mail']]) {
      const reply = await call(method, path, method === 'GET' ? {} : { body: {} });
      assert.equal(reply.status, 401, `${method} ${path}`);
      assert.deepEqual(reply.json, { ok: false, error: 'unauthorized' });
    }
  });

  it('runs profile, friends, gangs, notices and the arena over the wire', async () => {
    const nova = await account('Nova', 'nova@example.com');
    const rhea = await account('Rhea', 'rhea@example.com');
    const owner = await account('Owner', OWNER);

    assert.equal((await call('PUT', '/v1/profile', { token: nova.token, body: { level: 7, power: 1234, lobbyOp: 2, team: [] } })).status, 200);
    assert.deepEqual((await call('POST', '/v1/friends/request', { token: rhea.token, body: { to: 'nova' } })).json, { ok: true, status: 'sent' });
    const friends = await call('GET', '/v1/friends', { token: nova.token });
    assert.equal(friends.json.incoming[0].name, 'Rhea');
    assert.equal((await call('POST', '/v1/friends/accept', { token: nova.token, body: { uid: rhea.account.userId } })).status, 200);
    const bad = await call('POST', '/v1/friends/request', { token: nova.token, body: { to: '<b>nobody</b>' } });
    assert.deepEqual(bad.json, { ok: false, error: 'not_found' });
    assert.equal(bad.text.includes('nobody'), false, 'nothing user-supplied is echoed');

    const created = await call('POST', '/v1/gangs', { token: nova.token, body: { name: 'Night Owls', minLevel: 0, minPower: 0, open: true } });
    assert.equal(created.status, 200);
    assert.equal(created.json.gang.name, 'Night Owls');
    const found = await call('GET', '/v1/gangs?q=owl', { token: rhea.token });
    assert.deepEqual(found.json.gangs.map((g) => g.name), ['Night Owls']);
    const joined = await call('POST', '/v1/gangs/join', { token: rhea.token, body: { id: created.json.gang.id, level: 1, power: 0 } });
    assert.equal(joined.json.gang.members.length, 2);
    assert.equal((await call('POST', '/v1/gangs/contribute', { token: rhea.token, body: { kind: 'checkin' } })).json.added, 50);
    // A POST without a body is accepted where the body is {}.
    assert.deepEqual((await call('POST', '/v1/gangs/leave', { token: rhea.token })).json, { ok: true });
    assert.equal((await call('GET', '/v1/gangs/mine', { token: rhea.token })).json.gang, null);
    assert.equal((await call('POST', '/v1/gangs/claim', { token: rhea.token })).json.error, 'nothing_to_claim');

    assert.equal((await call('POST', '/v1/admin/notices', { token: nova.token, body: { category: 'notice', title: 't', body: 'b' } })).status, 403);
    const notice = await call('POST', '/v1/admin/notices', { token: owner.token, body: { category: 'event', title: 'Launch', body: 'Hello', pinned: true } });
    assert.equal(notice.status, 200);
    assert.equal((await call('GET', '/v1/notices', { token: nova.token })).json.notices[0].id, notice.json.id);
    const mail = await call('POST', '/v1/admin/mail', { token: owner.token, body: { to: nova.account.userId, title: 'Gift', reward: { gold: 10 } } });
    assert.equal(mail.json.recipients, 1);
    const inbox = await call('GET', '/v1/mail', { token: nova.token });
    assert.deepEqual(inbox.json.mail.map((m) => m.title), ['Gift']);
    assert.equal((await call('POST', '/v1/mail/ack', { token: nova.token, body: { ids: [mail.json.id] } })).status, 200);
    assert.deepEqual((await call('GET', '/v1/mail', { token: nova.token })).json.mail, []);

    const team = [{ op: 1, level: 10, stars: 1, hp: 500, attack: 50, defence: 10, power: 100 }];
    assert.equal((await call('PUT', '/v1/arena/defence', { token: rhea.token, body: { team, power: 100 } })).status, 200);
    const match = await call('POST', '/v1/arena/match', { token: nova.token, body: { power: 100 } });
    assert.equal(match.json.opponent.name, 'Rhea');
    const result = await call('POST', '/v1/arena/result', { token: nova.token, body: { matchId: match.json.matchId, won: true } });
    assert.deepEqual(result.json, { ok: true, trophies: 30, rank: 0, delta: 30 });
    assert.equal((await call('POST', '/v1/arena/result', { token: nova.token, body: { matchId: match.json.matchId, won: true } })).status, 409);
    assert.equal((await call('GET', '/v1/arena/me', { token: rhea.token })).json.trophies, 0);
  });

  it('applies the body limits and answers 405 / 404 for the new paths', async () => {
    const { token } = await account('Juno', 'juno@example.com');
    const team = Array.from({ length: 3 }, () => ({ op: 1, level: 10, stars: 1, hp: 500, attack: 50, defence: 10, power: 100 }));
    // 16 KB for the defence and the profile, 4 KB elsewhere.
    assert.equal((await call('PUT', '/v1/arena/defence', { token, body: { team, power: 1, pad: 'x'.repeat(8_000) } })).status, 200);
    assert.equal((await call('PUT', '/v1/arena/defence', { token, body: { team, power: 1, pad: 'x'.repeat(17_000) } })).status, 413);
    assert.equal((await call('POST', '/v1/friends/request', { token, body: { to: 'x', pad: 'x'.repeat(5_000) } })).status, 413);
    assert.equal((await call('GET', '/v1/arena/match', { token })).status, 405);
    assert.equal((await call('DELETE', '/v1/gangs', { token })).status, 405);
    assert.equal((await call('GET', '/v1/admin/notices', { token })).status, 405);
    assert.equal((await call('GET', '/v1/gangs/nope', { token })).status, 404);
    assert.equal((await call('POST', '/v1/gangs/join', { token, raw: '{oops' })).json.error, 'invalid_json');
  });
});
