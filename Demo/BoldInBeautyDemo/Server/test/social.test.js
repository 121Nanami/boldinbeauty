// The online features (docs/online-api.md) at service level, on every store: profile, friends, gangs, mail, notices, arena.
import assert from 'node:assert/strict';
import { afterEach, beforeEach, describe, it } from 'node:test';
import { arenaRank, cleanReward, gangLevel, gangSeasonCoins, gangTierCoins, normalizeGangName, trophyDelta } from '../src/social.js';
import { OWNER, expectApiError, makeHarness, signUp, storeFactories } from './helpers.js';

const fighter = (power = 100) => ({ op: 1, level: 10, stars: 1, hp: 500, attack: 50, defence: 10, power });

describe('online rules (pure)', () => {
  it('computes gang levels, season coins, ranks and trophy deltas as the contract says', () => {
    assert.equal(gangLevel(0), 1);
    assert.equal(gangLevel(499), 1);
    assert.equal(gangLevel(500), 2);
    assert.equal(gangLevel(2000), 3);
    assert.equal(gangLevel(1e12), 30);
    assert.deepEqual([0, 999, 1000, 3000, 6000, 10000, 15000, 99999].map(gangTierCoins), [50, 50, 150, 300, 450, 650, 900, 900]);
    assert.equal(gangSeasonCoins(3000, 305), 330);
    assert.equal(gangSeasonCoins(15000, 50000), 1200, 'the contribution bonus is capped at 300');
    assert.deepEqual([0, 99, 100, 899, 900, 1499, 1500, 9999].map(arenaRank), [0, 0, 1, 8, 9, 14, 15, 15]);
    assert.equal(trophyDelta(0, 500), 50);
    assert.equal(trophyDelta(500, 0), 10);
    assert.equal(trophyDelta(200, 100), 26);
    assert.equal(trophyDelta(100, 100), 30);
  });

  it('normalizes gang names and cleans rewards', () => {
    assert.equal(normalizeGangName('  Alpha   Wolves '), 'Alpha Wolves');
    for (const bad of ['ab', 'a'.repeat(21), 'bad!name', 'ünicode', '', 5, null]) assert.equal(normalizeGangName(bad), null);
    assert.deepEqual(cleanReward({ gold: 5.7, diamonds: -3, hacks: 9, gangCoins: 2e9, bytes: '7', hearts: Infinity }), { gold: 5, gangCoins: 1_000_000 });
    assert.deepEqual(cleanReward(undefined), {});
  });
});

for (const { label, make } of storeFactories()) {
  describe(`online: ${label}`, () => {
    let h;
    let n = 0;
    beforeEach(async () => {
      h = await makeHarness(make);
      h.clock.time = new Date('2026-10-15T12:00:00Z');
    });
    afterEach(async () => { await h.close(); });

    // A signed-up player's account record (what authenticate() hands to the routes).
    async function player(name, email = `${name.toLowerCase()}${(n += 1)}@example.com`) {
      const session = await signUp(h, name, email);
      return h.service.authenticate(session.token);
    }

    // --------------------------------------------------------------- profile
    describe('profile', () => {
      it('stores the public profile under the account name and validates it', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const at = h.clock.time.toISOString();
        assert.deepEqual(await h.social.putProfile(a, { level: 12, power: 3456, lobbyOp: 3, name: 'Hacker', team: [{ op: 2, level: 5, stars: 1, power: 900 }] }), { ok: true });
        const [profile] = await h.store.getProfiles([a.uid]);
        assert.equal(profile.name, 'Nova', 'the name comes from the account, never from the body');
        assert.equal(profile.level, 12);
        assert.equal(profile.team.length, 1);
        await h.social.requestFriend(b, { to: 'Nova' });
        const { outgoing } = await h.social.listFriends(b);
        assert.deepEqual(outgoing[0], { uid: String(a.uid), name: 'Nova', level: 12, power: 3456, lastSeen: at, gang: '' });
        for (const body of [{ level: 0, power: 1 }, { level: 1000, power: 1 }, { level: 1, power: -1 }, { level: 1, power: 1, lobbyOp: 20 },
          { level: 1, power: 1, team: [{}, {}, {}, {}] }, { level: '5', power: 1 }, { level: 1, power: 1, team: [{ op: 0, level: 0, stars: 0, power: 0 }] }]) {
          await expectApiError(h.social.putProfile(a, body), 400, 'invalid_input');
        }
      });

      it('rate-limits writes per player', async () => {
        const a = await player('Nova');
        for (let i = 0; i < 60; i += 1) await h.social.putProfile(a, { level: 1, power: 1 });
        await expectApiError(h.social.putProfile(a, { level: 1, power: 1 }), 429, 'rate_limited');
        h.advance(61);
        assert.deepEqual(await h.social.putProfile(a, { level: 1, power: 1 }), { ok: true });
      });
    });

    // --------------------------------------------------------------- friends
    describe('friends', () => {
      it('requests by username or id, accepts, declines, cancels and removes', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const c = await player('Juno');
        assert.deepEqual(await h.social.requestFriend(a, { to: 'rhea' }), { ok: true, status: 'sent' });
        assert.deepEqual(await h.social.requestFriend(a, { to: String(c.uid) }), { ok: true, status: 'sent' });
        let state = await h.social.listFriends(a);
        assert.deepEqual(state.outgoing.map((x) => x.name).sort(), ['Juno', 'Rhea']);
        assert.deepEqual((await h.social.listFriends(b)).incoming.map((x) => x.uid), [String(a.uid)]);

        assert.deepEqual(await h.social.acceptFriend(b, { uid: String(a.uid) }), { ok: true });
        await expectApiError(h.social.acceptFriend(b, { uid: String(a.uid) }), 404, 'not_found');
        assert.deepEqual(await h.social.declineFriend(c, { uid: a.uid }), { ok: true });
        state = await h.social.listFriends(a);
        assert.deepEqual(state.friends.map((x) => x.name), ['Rhea']);
        assert.deepEqual(state.outgoing, []);
        assert.deepEqual((await h.social.listFriends(b)).friends.map((x) => x.name), ['Nova']);

        // Cancelling one's own outgoing request is a decline too.
        await h.social.requestFriend(a, { to: 'Juno' });
        assert.deepEqual(await h.social.declineFriend(a, { uid: c.uid }), { ok: true });
        assert.deepEqual((await h.social.listFriends(c)).incoming, []);

        assert.deepEqual(await h.social.removeFriend(b, { uid: a.uid }), { ok: true });
        assert.deepEqual((await h.social.listFriends(a)).friends, []);
        await expectApiError(h.social.removeFriend(b, { uid: a.uid }), 404, 'not_found');
      });

      it('accepts automatically when the other player already asked you', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        await h.social.requestFriend(a, { to: 'Rhea' });
        assert.deepEqual(await h.social.requestFriend(b, { to: 'Nova' }), { ok: true, status: 'accepted' });
        assert.deepEqual((await h.social.listFriends(a)).friends.map((x) => x.name), ['Rhea']);
      });

      it('refuses self, unknown players, duplicates and bad input', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        await expectApiError(h.social.requestFriend(a, { to: 'Nova' }), 400, 'self');
        await expectApiError(h.social.requestFriend(a, { to: String(a.uid) }), 400, 'self');
        await expectApiError(h.social.requestFriend(a, { to: 'Nobody' }), 404, 'not_found');
        await expectApiError(h.social.requestFriend(a, { to: '99999999' }), 404, 'not_found');
        await expectApiError(h.social.requestFriend(a, {}), 400, 'invalid_input');
        await h.social.requestFriend(a, { to: 'Rhea' });
        await expectApiError(h.social.requestFriend(a, { to: 'Rhea' }), 409, 'already_requested');
        await h.social.acceptFriend(b, { uid: a.uid });
        await expectApiError(h.social.requestFriend(a, { to: 'Rhea' }), 409, 'already_friends');
        await expectApiError(h.social.requestFriend(b, { to: 'Nova' }), 409, 'already_friends');
        await expectApiError(h.social.acceptFriend(a, { uid: 'abc' }), 400, 'invalid_input');
      });

      it('caps friends at 50 and pending requests at 50 each way', async () => {
        const a = await player('Nova');
        const others = [];
        for (let i = 0; i < 51; i += 1) others.push(await player(`P${i}x`));
        for (let i = 0; i < 50; i += 1) await h.social.requestFriend(a, { to: String(others[i].uid) });
        await expectApiError(h.social.requestFriend(a, { to: String(others[50].uid) }), 409, 'friends_full');
        h.advance(61); // the rate limit window
        for (let i = 0; i < 50; i += 1) await h.social.acceptFriend(others[i], { uid: a.uid });
        assert.equal((await h.social.listFriends(a)).friends.length, 50);
        await expectApiError(h.social.requestFriend(a, { to: String(others[50].uid) }), 409, 'friends_full');
        await h.social.requestFriend(others[50], { to: 'Nova' });
        await expectApiError(h.social.acceptFriend(a, { uid: others[50].uid }), 409, 'friends_full');
      });
    });

    // ----------------------------------------------------------------- gangs
    describe('gangs', () => {
      it('creates, searches and joins with the requirements', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const c = await player('Juno');
        const created = await h.social.createGang(a, { name: '  Alpha   Wolves ', minLevel: 10, minPower: 1000, open: true });
        assert.equal(created.gang.name, 'Alpha Wolves');
        assert.equal(created.gang.myRole, 'leader');
        assert.equal(created.gang.members.length, 1);
        assert.equal(created.gang.maxMembers, 30);
        assert.equal(created.gang.season, '2026-10');
        assert.equal(created.gang.leaderName, 'Nova');
        await expectApiError(h.social.createGang(b, { name: 'alpha wolves' }), 409, 'name_taken');
        await expectApiError(h.social.createGang(a, { name: 'Second Gang' }), 409, 'in_gang');
        for (const name of ['ab', 'bad!name', '', null]) await expectApiError(h.social.createGang(b, { name }), 400, 'invalid_name');
        await expectApiError(h.social.createGang(b, { name: 'Ok Name', minLevel: 1000 }), 400, 'invalid_input');
        await h.social.createGang(b, { name: 'Beta Crew' });

        assert.deepEqual((await h.social.searchGangs(c, 'WOLV')).gangs.map((g) => g.name), ['Alpha Wolves']);
        assert.deepEqual((await h.social.searchGangs(c, '')).gangs.map((g) => g.name).sort(), ['Alpha Wolves', 'Beta Crew']);
        assert.deepEqual((await h.social.searchGangs(c, '.*')).gangs, [], 'the query is text, not a pattern');

        const id = created.gang.id;
        await expectApiError(h.social.joinGang(c, { id, level: 9, power: 5000 }), 403, 'requirements');
        await expectApiError(h.social.joinGang(c, { id, level: 10, power: 999 }), 403, 'requirements');
        await expectApiError(h.social.joinGang(c, { id: 'ffffffffffffffffffffffff', level: 10, power: 1000 }), 404, 'not_found');
        const joined = await h.social.joinGang(c, { id, level: 10, power: 1000 });
        assert.equal(joined.gang.myRole, 'member');
        assert.equal(joined.gang.members.length, 2);
        await expectApiError(h.social.joinGang(c, { id, level: 10, power: 1000 }), 409, 'in_gang');
        await expectApiError(h.social.joinGang(b, { id, level: 10, power: 1000 }), 409, 'in_gang');

        const mine = await h.social.myGang(c);
        assert.equal(mine.gang.id, id);
        assert.equal(mine.pendingCoins, 0);
        assert.equal(mine.pendingSeason, '');
        assert.equal((await h.social.myGang(await player('Lone'))).gang, null);
        // Friend cards show the gang name.
        await h.social.requestFriend(c, { to: 'Nova' });
        assert.equal((await h.social.listFriends(a)).incoming[0].gang, 'Alpha Wolves');
      });

      it('refuses closed gangs and stops at 30 members', async () => {
        const leader = await player('Lead');
        const { gang } = await h.social.createGang(leader, { name: 'Full House' });
        const players = [];
        for (let i = 0; i < 30; i += 1) players.push(await player(`M${i}x`));
        for (let i = 0; i < 29; i += 1) await h.social.joinGang(players[i], { id: gang.id, level: 1, power: 0 });
        assert.equal((await h.social.myGang(leader)).gang.members.length, 30);
        await expectApiError(h.social.joinGang(players[29], { id: gang.id, level: 1, power: 0 }), 409, 'gang_full');
        assert.equal((await h.store.getGang(gang.id)).memberCount, 30);

        await h.social.gangSettings(leader, { open: false });
        await h.social.leaveGang(players[0]);
        await expectApiError(h.social.joinGang(players[29], { id: gang.id, level: 1, power: 0 }), 403, 'gang_closed');
        await h.social.gangSettings(leader, { open: true });
        assert.equal((await h.social.joinGang(players[29], { id: gang.id, level: 1, power: 0 })).gang.members.length, 30);
      });

      it('never passes 30 members when many players join at once', async () => {
        const leader = await player('Lead');
        const { gang } = await h.social.createGang(leader, { name: 'Rush' });
        const players = [];
        for (let i = 0; i < 34; i += 1) players.push(await player(`R${i}x`));
        const results = await Promise.allSettled(players.map((p) => h.social.joinGang(p, { id: gang.id, level: 1, power: 0 })));
        assert.equal(results.filter((r) => r.status === 'fulfilled').length, 29);
        assert.ok(results.filter((r) => r.status === 'rejected').every((r) => r.reason.code === 'gang_full'));
        assert.equal((await h.store.getGang(gang.id)).memberCount, 30);
        assert.equal((await h.store.listGangMembers(gang.id)).length, 30);
      });

      it('hands the lead to the top contributor and deletes the gang when the last member leaves', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const c = await player('Juno');
        const { gang } = await h.social.createGang(a, { name: 'Handover' });
        await h.social.joinGang(b, { id: gang.id, level: 1, power: 0 });
        await h.social.joinGang(c, { id: gang.id, level: 1, power: 0 });
        await h.social.contribute(b, { kind: 'checkin' });
        await h.social.contribute(c, { kind: 'checkin' });
        await h.social.contribute(c, { kind: 'activity', amount: 30 });
        await h.social.leaveGang(a);
        let detail = (await h.social.myGang(c)).gang;
        assert.equal(detail.myRole, 'leader');
        assert.equal(detail.leaderName, 'Juno');
        assert.equal(detail.members.length, 2);
        await expectApiError(h.social.leaveGang(a), 409, 'not_in_gang');
        await h.social.leaveGang(c);
        detail = (await h.social.myGang(b)).gang;
        assert.equal(detail.myRole, 'leader');
        await h.social.leaveGang(b);
        assert.equal(await h.store.getGang(gang.id), null);
        assert.deepEqual((await h.social.searchGangs(a, '')).gangs, []);
        // The name is free again.
        assert.equal((await h.social.createGang(a, { name: 'Handover' })).gang.name, 'Handover');
      });

      it('lets only the leader kick and change settings', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const c = await player('Juno');
        const { gang } = await h.social.createGang(a, { name: 'Strict' });
        await h.social.joinGang(b, { id: gang.id, level: 1, power: 0 });
        await expectApiError(h.social.kickMember(b, { uid: a.uid }), 403, 'not_leader');
        await expectApiError(h.social.gangSettings(b, { open: false }), 403, 'not_leader');
        await expectApiError(h.social.kickMember(a, { uid: a.uid }), 400, 'self');
        await expectApiError(h.social.kickMember(a, { uid: c.uid }), 404, 'not_found');
        const kicked = await h.social.kickMember(a, { uid: String(b.uid) });
        assert.equal(kicked.gang.members.length, 1);
        assert.equal((await h.social.myGang(b)).gang, null);
        const changed = await h.social.gangSettings(a, { minLevel: 50, minPower: 7, open: false });
        assert.deepEqual([changed.gang.minLevel, changed.gang.minPower, changed.gang.open], [50, 7, false]);
        await expectApiError(h.social.gangSettings(a, { minLevel: -1 }), 400, 'invalid_input');
        await expectApiError(h.social.kickMember(c, { uid: a.uid }), 409, 'not_in_gang');
      });

      it('caps contributions per UTC day and resets them the next day', async () => {
        const a = await player('Nova');
        const { gang } = await h.social.createGang(a, { name: 'Workers' });
        assert.equal((await h.social.contribute(a, { kind: 'checkin' })).added, 50);
        assert.equal((await h.social.contribute(a, { kind: 'checkin' })).added, 0);
        for (let i = 0; i < 4; i += 1) assert.equal((await h.social.contribute(a, { kind: 'donate', amount: 10 })).added, 10);
        assert.equal((await h.social.contribute(a, { kind: 'donate', amount: 20 })).added, 10, 'only one donation was left today');
        assert.equal((await h.social.contribute(a, { kind: 'donate', amount: 10 })).added, 0);
        for (let i = 0; i < 4; i += 1) assert.equal((await h.social.contribute(a, { kind: 'activity', amount: 50 })).added, 50);
        assert.equal((await h.social.contribute(a, { kind: 'activity', amount: 1 })).added, 0);
        let result = await h.social.contribute(a, { kind: 'checkin' });
        assert.equal(result.gang.xp, 300);
        assert.equal(result.gang.seasonPoints, 300);
        assert.equal(result.gang.members[0].contribution, 300);
        assert.equal(result.gang.members[0].seasonContribution, 300);
        for (const body of [{ kind: 'donate', amount: 15 }, { kind: 'donate', amount: 60 }, { kind: 'activity', amount: 51 }, { kind: 'activity', amount: 0 }, { kind: 'cheat' }, {}]) {
          await expectApiError(h.social.contribute(a, body), 400, 'invalid_input');
        }

        h.clock.time = new Date('2026-10-16T00:00:01Z');
        assert.equal((await h.social.contribute(a, { kind: 'checkin' })).added, 50);
        assert.equal((await h.social.contribute(a, { kind: 'donate', amount: 50 })).added, 50);
        result = await h.social.contribute(a, { kind: 'activity', amount: 50 });
        assert.equal(result.added, 50);
        assert.equal(result.gang.xp, 450);
        assert.equal((await h.store.getGang(gang.id)).seasonPoints, 450);
        await expectApiError(h.social.contribute(await player('Lone'), { kind: 'checkin' }), 409, 'not_in_gang');
      });

      it('records each milestone once per season', async () => {
        const a = await player('Nova');
        const { gang } = await h.social.createGang(a, { name: 'Climbers' });
        await expectApiError(h.social.claimMilestone(a, { index: 0 }), 409, 'not_reached');
        await h.store.addGangPoints(gang.id, 3000, h.clock.time);
        assert.deepEqual(await h.social.claimMilestone(a, { index: 0 }), { ok: true });
        assert.deepEqual(await h.social.claimMilestone(a, { index: 1 }), { ok: true });
        await expectApiError(h.social.claimMilestone(a, { index: 0 }), 409, 'already_claimed');
        await expectApiError(h.social.claimMilestone(a, { index: 2 }), 409, 'not_reached');
        await expectApiError(h.social.claimMilestone(a, { index: 5 }), 400, 'invalid_input');
        assert.deepEqual((await h.social.myGang(a)).gang.milestonesClaimed, [0, 1]);

        h.clock.time = new Date('2026-11-01T00:00:00Z');
        assert.deepEqual((await h.social.myGang(a)).gang.milestonesClaimed, []);
        await expectApiError(h.social.claimMilestone(a, { index: 0 }), 409, 'not_reached');
        await h.store.addGangPoints(gang.id, 1000, h.clock.time);
        assert.deepEqual(await h.social.claimMilestone(a, { index: 0 }), { ok: true });
      });

      it('pays every member at season end (tier + contribution bonus), resets the season, and pays out once', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const late = await player('Late');
        const { gang } = await h.social.createGang(a, { name: 'Season Gang' });
        await h.social.joinGang(b, { id: gang.id, level: 1, power: 0 });
        await h.social.contribute(a, { kind: 'checkin' });
        for (let i = 0; i < 4; i += 1) await h.social.contribute(a, { kind: 'activity', amount: 50 });
        await h.social.contribute(a, { kind: 'donate', amount: 50 }); // Nova: 300 this season
        await h.social.contribute(b, { kind: 'checkin' });             // Rhea: 50
        await h.store.addGangPoints(gang.id, 3000, h.clock.time);      // 3,350 season points: tier 300
        await expectApiError(h.social.claimGangCoins(a), 409, 'nothing_to_claim');

        h.clock.time = new Date('2026-11-02T08:00:00Z');
        // The first gang request in the new month (any member) closes October for everyone.
        const rhea = await h.social.myGang(b);
        assert.equal(rhea.pendingCoins, 300 + 5);
        assert.equal(rhea.pendingSeason, '2026-10');
        assert.equal(rhea.gang.season, '2026-11');
        assert.equal(rhea.gang.seasonPoints, 0);
        assert.equal(rhea.gang.xp, 3350);
        assert.ok(rhea.gang.members.every((m) => m.seasonContribution === 0));
        assert.equal(rhea.gang.members.find((m) => m.name === 'Nova').contribution, 300);
        // Joining now starts in the new season and earns nothing for October.
        await h.social.joinGang(late, { id: gang.id, level: 1, power: 0 });
        assert.equal((await h.social.myGang(late)).pendingCoins, 0);
        // Repeated requests do not pay twice.
        await h.social.myGang(a);
        await h.social.myGang(b);
        const nova = await h.social.myGang(a);
        assert.equal(nova.pendingCoins, 300 + 30);

        assert.deepEqual(await h.social.claimGangCoins(a), { ok: true, coins: 330, season: '2026-10' });
        await expectApiError(h.social.claimGangCoins(a), 409, 'nothing_to_claim');
        // Pending coins wait even after leaving the gang.
        await h.social.leaveGang(b);
        assert.deepEqual(await h.social.claimGangCoins(b), { ok: true, coins: 305, season: '2026-10' });
        assert.equal((await h.social.myGang(b)).pendingCoins, 0);
      });

      it('pays each member once when the first requests of a new month arrive together', async () => {
        const a = await player('Nova');
        const b = await player('Rhea');
        const { gang } = await h.social.createGang(a, { name: 'Together' });
        await h.social.joinGang(b, { id: gang.id, level: 1, power: 0 });
        await h.social.contribute(a, { kind: 'checkin' });
        h.clock.time = new Date('2026-11-01T00:00:00Z');
        await Promise.all([h.social.myGang(a), h.social.myGang(b), h.social.myGang(a), h.social.myGang(b)]);
        assert.equal((await h.social.myGang(a)).pendingCoins, 55);
        assert.equal((await h.social.myGang(b)).pendingCoins, 50);
        const claims = await Promise.allSettled([h.social.claimGangCoins(a), h.social.claimGangCoins(a), h.social.claimGangCoins(a)]);
        assert.equal(claims.filter((r) => r.status === 'fulfilled').length, 1);
      });

      it('pays only the finished season when a gang sat idle for months', async () => {
        const a = await player('Nova');
        const { gang } = await h.social.createGang(a, { name: 'Sleepy' });
        await h.social.contribute(a, { kind: 'checkin' });
        h.clock.time = new Date('2027-01-10T00:00:00Z');
        const mine = await h.social.myGang(a);
        assert.equal(mine.pendingCoins, 50 + 5);
        assert.equal(mine.pendingSeason, '2026-10');
        assert.equal(mine.gang.season, '2027-01');
        assert.equal((await h.store.getGang(gang.id)).lastSeason, '2026-10');
      });
    });

    // ------------------------------------------------------------------ mail
    describe('mail', () => {
      it('delivers direct mail and broadcasts (only to accounts that existed), hides acknowledged mail', async () => {
        const owner = await player('Owner', OWNER);
        const early = await player('Early');
        await expectApiError(h.social.sendMail(early, { to: 'all', title: 'x' }), 403, 'forbidden');
        h.advance(5);
        const broadcast = await h.social.sendMail(owner, { to: 'all', title: 'Welcome', body: 'Hello', reward: { gold: 100, diamonds: 5.9, junk: 1 } });
        assert.deepEqual({ ...broadcast, id: undefined }, { ok: true, id: undefined, recipients: 'all' });
        h.advance(5);
        const late = await player('Later');
        const direct = await h.social.sendMail(owner, { to: String(late.uid), title: 'Just you', reward: { gangCoins: 50 } });
        assert.equal(direct.recipients, 1);
        await expectApiError(h.social.sendMail(owner, { to: '99999999', title: 'x' }), 404, 'not_found');
        await expectApiError(h.social.sendMail(owner, { to: 'all', title: '' }), 400, 'invalid_input');
        await expectApiError(h.social.sendMail(owner, { to: 'all', title: 'x'.repeat(61) }), 400, 'invalid_input');
        await expectApiError(h.social.sendMail(owner, { to: 'all', title: 'x', body: 'y'.repeat(601) }), 400, 'invalid_input');

        const earlyMail = (await h.social.listMail(early)).mail;
        assert.equal(earlyMail.length, 1);
        assert.deepEqual({ ...earlyMail[0], createdAt: undefined }, {
          id: broadcast.id, title: 'Welcome', body: 'Hello', reward: { gold: 100, diamonds: 5 }, from: 'Admin', createdAt: undefined,
        });
        assert.match(earlyMail[0].createdAt, /^2026-10-15T/);
        const lateMail = (await h.social.listMail(late)).mail;
        assert.deepEqual(lateMail.map((m) => m.title), ['Just you'], 'a broadcast from before the account existed is not delivered');

        assert.deepEqual(await h.social.ackMail(early, { ids: [broadcast.id, 'abcdef0123456789'] }), { ok: true });
        assert.deepEqual((await h.social.listMail(early)).mail, []);
        // Acknowledging someone else's mail has no effect.
        await h.social.ackMail(early, { ids: [direct.id] });
        assert.equal((await h.social.listMail(late)).mail.length, 1);
        await expectApiError(h.social.ackMail(early, { ids: 'nope' }), 400, 'invalid_input');
        await expectApiError(h.social.ackMail(early, { ids: ['../x'] }), 400, 'invalid_input');
      });

      it('lists at most 50, oldest first', async () => {
        const owner = await player('Owner', OWNER);
        const p = await player('Reader');
        for (let i = 0; i < 55; i += 1) {
          h.advance(1);
          await h.social.sendMail(owner, { to: String(p.uid), title: `m${i}` });
        }
        const mail = (await h.social.listMail(p)).mail;
        assert.equal(mail.length, 50);
        assert.equal(mail[0].title, 'm0');
        assert.equal(mail[49].title, 'm49');
        await h.social.ackMail(p, { ids: mail.slice(0, 10).map((m) => m.id) });
        assert.equal((await h.social.listMail(p)).mail[0].title, 'm10');
      });
    });

    // --------------------------------------------------------------- notices
    describe('notices', () => {
      it('lists pinned first, then newest first; only the admin creates and deletes', async () => {
        const owner = await player('Owner', OWNER);
        const p = await player('Reader');
        const first = await h.social.createNotice(owner, { category: 'notice', title: 'First', body: 'one' });
        h.advance(10);
        const pinned = await h.social.createNotice(owner, { category: 'maintenance', title: 'Pinned', body: 'two', pinned: true });
        h.advance(10);
        const third = await h.social.createNotice(owner, { category: 'event', title: 'Third', body: 'three' });
        let list = (await h.social.listNotices(p)).notices;
        assert.deepEqual(list.map((x) => x.id), [pinned.id, third.id, first.id]);
        assert.deepEqual(Object.keys(list[0]).sort(), ['body', 'category', 'createdAt', 'id', 'pinned', 'title']);
        assert.equal(list[0].pinned, true);

        await expectApiError(h.social.createNotice(p, { category: 'notice', title: 'x', body: 'y' }), 403, 'forbidden');
        await expectApiError(h.social.deleteNotice(p, { id: first.id }), 403, 'forbidden');
        for (const body of [{ category: 'news', title: 'x', body: 'y' }, { category: 'notice', title: '', body: 'y' }, { category: 'notice', title: 'x', body: '' },
          { category: 'notice', title: 'x', body: 'y'.repeat(2001) }, { category: 'notice', title: 'x', body: 'y', pinned: 'yes' }]) {
          await expectApiError(h.social.createNotice(owner, body), 400, 'invalid_input');
        }
        assert.deepEqual(await h.social.deleteNotice(owner, { id: third.id }), { ok: true });
        await expectApiError(h.social.deleteNotice(owner, { id: third.id }), 404, 'not_found');
        list = (await h.social.listNotices(p)).notices;
        assert.deepEqual(list.map((x) => x.id), [pinned.id, first.id]);
      });

      it('returns at most 30', async () => {
        const owner = await player('Owner', OWNER);
        for (let i = 0; i < 32; i += 1) { h.advance(1); await h.social.createNotice(owner, { category: 'update', title: `n${i}`, body: 'b' }); }
        const list = (await h.social.listNotices(owner)).notices;
        assert.equal(list.length, 30);
        assert.equal(list[0].title, 'n31');
      });
    });

    // ----------------------------------------------------------------- arena
    describe('arena', () => {
      // Sets fields of a player's arena record directly (creating the record first).
      async function setArena(p, patch) {
        await h.store.createArena({
          uid: p.uid, name: p.username, trophies: 0, season: '2026-10', champion: false, wins: 0, losses: 0, defenceWins: 0, defenceLosses: 0,
          log: [], recent: [], pendingReward: null, team: [], power: 0, hasDefence: false, lastDelta: 0,
        });
        await h.store.setArenaDefence(p.uid, patch);
      }

      // A player with a defence team; trophies (and the Champions flag) set directly when given.
      async function defender(name, power, trophies, champion) {
        const p = await player(name);
        await h.social.putDefence(p, { team: [fighter(power)], power });
        if (trophies !== undefined) await setArena(p, { trophies, ...(champion ? { champion: true } : {}) });
        return p;
      }

      it('starts at Bronze 1 and validates the defence', async () => {
        const a = await player('Nova');
        const me = await h.social.arenaMe(a);
        assert.deepEqual(me, {
          ok: true, trophies: 0, rank: 0, season: '2026-10', champion: false, wins: 0, losses: 0, defenceWins: 0, defenceLosses: 0, log: [], pendingReward: null,
        });
        for (const body of [{ team: [], power: 1 }, { team: [fighter(), fighter(), fighter(), fighter()], power: 1 }, { team: [{ ...fighter(), defence: 76 }], power: 1 },
          { team: [{ ...fighter(), op: 20 }], power: 1 }, { team: [fighter()], power: -1 }, { team: 'x', power: 1 }]) {
          await expectApiError(h.social.putDefence(a, body), 400, 'invalid_input');
        }
        assert.deepEqual(await h.social.putDefence(a, { team: [fighter(), fighter()], power: 200 }), { ok: true });
      });

      it('finds nobody when no other player has a defence', async () => {
        const a = await defender('Nova', 1000);
        await player('NoTeam');
        assert.deepEqual(await h.social.arenaMatch(a, { power: 1000 }), { ok: true, matchId: '', opponent: null });
      });

      it('matches by strength band first, then by the closest trophies', async () => {
        const attacker = await player('Nova');
        await defender('Far', 2000, 0);       // closest trophies but outside every band up to ±60%
        await defender('Band', 1100, 500);    // within ±15%
        let match = await h.social.arenaMatch(attacker, { power: 1000 });
        assert.equal(match.opponent.name, 'Band');
        assert.equal(match.opponent.trophies, 500);
        assert.equal(match.opponent.rank, 5);
        assert.equal(match.opponent.power, 1100);
        assert.deepEqual(match.opponent.team, [fighter(1100)]);
        assert.match(match.matchId, /^[a-f0-9]{24}$/);

        const fresh = await player('Fresh');
        await defender('Near', 950, 100);     // same band, closer trophies
        match = await h.social.arenaMatch(fresh, { power: 1000 });
        assert.equal(match.opponent.name, 'Near');
        // Nobody within ±15% or ±30% of 3,000, but "Far" (2,000) is within ±60%: it wins over the closer trophies of "anyone".
        const third = await player('Third');
        await setArena(third, { trophies: 500 });
        assert.equal((await h.social.arenaMatch(third, { power: 3000 })).opponent.name, 'Far');
        // Nobody within ±60% of 10,000: anyone, closest trophies (Band has 500, like the attacker).
        const fourth = await player('Fourth');
        await setArena(fourth, { trophies: 500 });
        assert.equal((await h.social.arenaMatch(fourth, { power: 10000 })).opponent.name, 'Band');
      });

      it('never offers yourself or your last 3 opponents', async () => {
        const a = await defender('Nova', 1000);
        const names = [];
        for (let i = 0; i < 4; i += 1) await defender(`D${i}x`, 1000);
        for (let i = 0; i < 5; i += 1) names.push((await h.social.arenaMatch(a, { power: 1000 })).opponent.name);
        assert.equal(new Set(names.slice(0, 4)).size, 4, 'four different opponents in a row');
        assert.ok(!names.includes('Nova'));
        assert.equal(names[4], names[0], 'the fifth is the only one not among the last three');
      });

      it('moves trophies zero-sum with the contract formula, logs the defence, and takes one result per match', async () => {
        const a = await player('Nova');
        const d = await defender('Wall', 1000, 500);
        let match = await h.social.arenaMatch(a, { power: 1000 });
        const result = await h.social.arenaResult(a, { matchId: match.matchId, won: true });
        assert.deepEqual(result, { ok: true, trophies: 50, rank: 0, delta: 50 });
        await expectApiError(h.social.arenaResult(a, { matchId: match.matchId, won: true }), 409, 'already_reported');
        const wall = await h.social.arenaMe(d);
        assert.equal(wall.trophies, 450);
        assert.equal(wall.defenceLosses, 1);
        assert.equal(wall.log.length, 1);
        assert.deepEqual({ ...wall.log[0], at: undefined }, { at: undefined, attacker: 'Nova', won: false, delta: -50 });
        assert.equal((await h.social.arenaMe(a)).wins, 1);

        // A loss: attacker 100 vs defender 200, defender wins: 30 + (100 - 200) / 25 = 26.
        await setArena(a, { trophies: 100, recent: [] });
        await setArena(d, { trophies: 200 });
        match = await h.social.arenaMatch(a, { power: 1000 });
        assert.deepEqual(await h.social.arenaResult(a, { matchId: match.matchId, won: false }), { ok: true, trophies: 74, rank: 0, delta: -26 });
        assert.equal((await h.social.arenaMe(d)).trophies, 226);
        assert.equal((await h.social.arenaMe(d)).defenceWins, 1);
        assert.equal((await h.social.arenaMe(a)).losses, 1);

        // Someone else's match, unknown ids and bad bodies.
        const other = await player('Other');
        await setArena(a, { recent: [] });
        match = await h.social.arenaMatch(a, { power: 1000 });
        await expectApiError(h.social.arenaResult(other, { matchId: match.matchId, won: true }), 404, 'not_found');
        await expectApiError(h.social.arenaResult(a, { matchId: 'ffffffffffffffffffffffff', won: true }), 404, 'not_found');
        await expectApiError(h.social.arenaResult(a, { matchId: match.matchId }), 400, 'invalid_input');
        // Expiry after 10 minutes.
        h.advance(601);
        await expectApiError(h.social.arenaResult(a, { matchId: match.matchId, won: true }), 409, 'match_expired');
      });

      it('takes exactly one result when the same match is reported at once, and pays a reward once', async () => {
        const a = await player('Nova');
        const d = await defender('Wall', 1000, 0);
        const match = await h.social.arenaMatch(a, { power: 1000 });
        const results = await Promise.allSettled(Array.from({ length: 5 }, () => h.social.arenaResult(a, { matchId: match.matchId, won: true })));
        assert.equal(results.filter((r) => r.status === 'fulfilled').length, 1);
        assert.ok(results.filter((r) => r.status === 'rejected').every((r) => r.reason.code === 'already_reported'));
        assert.equal((await h.social.arenaMe(a)).trophies, 30);
        assert.equal((await h.social.arenaMe(d)).defenceLosses, 1);
        h.clock.time = new Date('2026-11-01T00:00:00Z');
        const claims = await Promise.allSettled(Array.from({ length: 4 }, () => h.social.arenaClaim(a)));
        assert.equal(claims.filter((r) => r.status === 'fulfilled').length, 1);
      });

      it('keeps trophies at 0 or above, and at 1,500 or above once Champions was reached this season', async () => {
        const a = await player('Nova');
        const d = await defender('Wall', 1000, 5);
        await setArena(a, { trophies: 5 });
        let match = await h.social.arenaMatch(a, { power: 1000 });
        assert.deepEqual(await h.social.arenaResult(a, { matchId: match.matchId, won: false }), { ok: true, trophies: 0, rank: 0, delta: -5 });
        assert.equal((await h.social.arenaMe(d)).trophies, 35);

        // Reaching 1,500 by winning sets the Champions floor.
        await setArena(a, { trophies: 1480, recent: [] });
        await setArena(d, { trophies: 1480 });
        match = await h.social.arenaMatch(a, { power: 1000 });
        assert.deepEqual(await h.social.arenaResult(a, { matchId: match.matchId, won: true }), { ok: true, trophies: 1510, rank: 15, delta: 30 });
        assert.equal((await h.social.arenaMe(a)).champion, true);
        await setArena(a, { recent: [] });
        await setArena(d, { trophies: 1700 });
        match = await h.social.arenaMatch(a, { power: 1000 });
        assert.deepEqual(await h.social.arenaResult(a, { matchId: match.matchId, won: false }), { ok: true, trophies: 1500, rank: 15, delta: -10 });
        // The same applies when defending.
        const c = await defender('Champ', 1000, 1600, true);
        const e = await player('Raider');
        await setArena(d, { hasDefence: false });
        await setArena(a, { hasDefence: false });
        match = await h.social.arenaMatch(e, { power: 1000 });
        assert.equal(match.opponent.name, 'Champ');
        await h.social.arenaResult(e, { matchId: match.matchId, won: true });
        assert.equal((await h.social.arenaMe(c)).trophies, 1550, 'lost 50, still above the floor');
        await setArena(c, { trophies: 1510 });
        await setArena(e, { recent: [] });
        match = await h.social.arenaMatch(e, { power: 1000 });
        await h.social.arenaResult(e, { matchId: match.matchId, won: true });
        assert.equal((await h.social.arenaMe(c)).trophies, 1500);
      });

      it('ends the season: rank drops by 3, by 4 from Diamond 1 up, the floor clears, and the reward is claimed once', async () => {
        const gold = await defender('Gold', 1000, 820);      // rank 8 -> 5
        const diamond = await defender('Diamond', 1000, 950); // rank 9 -> 5
        const bronze = await defender('Bronze', 1000, 250);   // rank 2 -> 0
        const champ = await defender('Champ', 1000, 1600, true); // rank 15 -> 11
        await expectApiError(h.social.arenaClaim(gold), 409, 'nothing_to_claim');

        h.clock.time = new Date('2026-11-01T00:00:05Z');
        const g = await h.social.arenaMe(gold);
        assert.deepEqual([g.trophies, g.rank, g.season], [500, 5, '2026-11']);
        assert.deepEqual(g.pendingReward, { season: '2026-10', rank: 8 });
        assert.equal((await h.social.arenaMe(diamond)).trophies, 500);
        assert.equal((await h.social.arenaMe(bronze)).trophies, 0);
        const c = await h.social.arenaMe(champ);
        assert.deepEqual([c.trophies, c.rank, c.champion], [1100, 11, false]);
        assert.deepEqual(c.pendingReward, { season: '2026-10', rank: 15 });
        // A second request in the same month changes nothing.
        assert.equal((await h.social.arenaMe(champ)).trophies, 1100);

        assert.deepEqual(await h.social.arenaClaim(champ), { ok: true, season: '2026-10', rank: 15 });
        await expectApiError(h.social.arenaClaim(champ), 409, 'nothing_to_claim');
        assert.equal((await h.social.arenaMe(champ)).pendingReward, null);
        // The floor is gone: losing goes below 1,500 now (it is already below; losing more keeps going down).
        const raider = await player('Raider');
        for (const p of [gold, diamond, bronze]) await setArena(p, { hasDefence: false });
        const match = await h.social.arenaMatch(raider, { power: 1000 });
        await h.social.arenaResult(raider, { matchId: match.matchId, won: true });
        assert.ok((await h.social.arenaMe(champ)).trophies < 1100);
      });

      it('rolls a defender that has not played this month before matching and scoring', async () => {
        const d = await defender('Old', 1000, 950);
        h.clock.time = new Date('2026-11-03T00:00:00Z');
        const a = await player('Nova');
        const match = await h.social.arenaMatch(a, { power: 1000 });
        assert.equal(match.opponent.trophies, 500, 'the opponent was brought to the new season first');
        await h.social.arenaResult(a, { matchId: match.matchId, won: true });
        const old = await h.social.arenaMe(d);
        assert.equal(old.trophies, 500 - 50);
        assert.deepEqual(old.pendingReward, { season: '2026-10', rank: 9 });
      });
    });
  });
}
