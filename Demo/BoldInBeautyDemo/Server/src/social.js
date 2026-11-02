// The online features next to the account: public profile, friends, gangs, admin mail, notices and the arena.
// The contract is docs/online-api.md; every number there is a PLACEHOLDER unless the owner gave it.
//
//   profile : the game pushes a small public summary; friends, gangs and the arena read it
//   friends : requests both ways, max 50 friends and 50 pending each way
//   gangs   : max 30 members, join requirements, daily contribution caps, monthly season with coin payout
//   mail    : admin mail to one player or to every player (broadcasts reach accounts created before them only)
//   notices : admin announcements, pinned first, newest first
//   arena   : defence lineups, matchmaking by strength then trophies, zero-sum trophies with floors, monthly seasons
//
// Seasons are the UTC calendar month ("YYYY-MM") of now(); daily caps use the UTC day. Season ends are applied lazily, by the
// first request that touches the gang or the arena record in a new month, and each step is a conditional (atomic) store update
// so two simultaneous requests cannot pay out twice. The save is client-authoritative in this demo: reported level, power and
// fight results are trusted (server-side battle verification is OPEN).
import crypto from 'node:crypto';
import { StoreConflict } from './store/errors.js';
import { ApiError } from './util.js';

export const MAX_FRIENDS = 50;
export const MAX_PENDING = 50;
export const MAX_GANG_MEMBERS = 30;
export const GANG_MILESTONES = [1000, 3000, 6000, 10000, 15000];
const GANG_TIERS = [[15000, 900], [10000, 650], [6000, 450], [3000, 300], [1000, 150], [0, 50]];
const CHECKIN_POINTS = 50;
const DONATION_POINTS = 10;
const MAX_DONATIONS_PER_DAY = 5;
const MAX_ACTIVITY_PER_DAY = 200;
const MAX_ACTIVITY_PER_CALL = 50;
const REWARD_KEYS = ['gold', 'bytes', 'hearts', 'diamonds', 'regular', 'special', 'arcade', 'gangCoins'];
const MAX_REWARD = 1_000_000;
const NOTICE_CATEGORIES = ['notice', 'event', 'update', 'maintenance'];
const MAX_POWER = 2_000_000_000;
const MATCH_MINUTES = 10;
const RECENT_OPPONENTS = 3;
const POWER_BANDS = [0.15, 0.30, 0.60];
const WRITES_PER_MINUTE = 60;
const READS_PER_MINUTE = 120;

// ------------------------------------------------------------------ pure rules
export function seasonKey(date) {
  return date.toISOString().slice(0, 7);
}

export function dayKey(date) {
  return date.toISOString().slice(0, 10);
}

export function gangLevel(xp) {
  return Math.min(30, 1 + Math.floor(Math.sqrt(Math.max(0, xp) / 500)));
}

export function gangTierCoins(seasonPoints) {
  for (const [points, coins] of GANG_TIERS) if (seasonPoints >= points) return coins;
  return 50;
}

export function gangSeasonCoins(seasonPoints, seasonContribution) {
  return gangTierCoins(seasonPoints) + Math.min(300, Math.floor(Math.max(0, seasonContribution) / 10));
}

export function arenaRank(trophies) {
  return Math.min(15, Math.floor(Math.max(0, trophies) / 100));
}

export function trophyDelta(winnerTrophies, loserTrophies) {
  return Math.min(50, Math.max(10, Math.round(30 + (loserTrophies - winnerTrophies) / 25)));
}

// The arena record after the season end, if its season is not the current one (pure; the store applies it atomically).
export function arenaSeasonEnd(record, season) {
  if (record.season === season) return null;
  const oldRank = arenaRank(record.trophies);
  const newRank = Math.max(0, oldRank - (oldRank >= 9 ? 4 : 3));
  return { season, trophies: newRank * 100, champion: false, pendingReward: { season: record.season, rank: oldRank } };
}

// ------------------------------------------------------------------ validation
function bad() {
  return new ApiError(400, 'invalid_input');
}

function int(value, min, max) {
  if (!Number.isInteger(value) || value < min || value > max) throw bad();
  return value;
}

function optInt(value, min, max, fallback) {
  return value === undefined ? fallback : int(value, min, max);
}

function num(value, min, max) {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < min || value > max) throw bad();
  return value;
}

function bool(value, fallback) {
  if (value === undefined) return fallback;
  if (typeof value !== 'boolean') throw bad();
  return value;
}

function text(value, min, max) {
  if (typeof value !== 'string') throw bad();
  const trimmed = value.trim();
  if (trimmed.length < min || trimmed.length > max) throw bad();
  return trimmed;
}

// A user id from the body: "10000002" or 10000002.
function uidOf(value) {
  const raw = typeof value === 'number' ? String(value) : value;
  if (typeof raw !== 'string' || !/^\d{1,12}$/.test(raw.trim())) throw bad();
  return Number(raw.trim());
}

// An opaque id we generated (gang, mail, notice, match).
function idOf(value) {
  if (typeof value !== 'string' || !/^[a-f0-9]{8,40}$/.test(value)) throw bad();
  return value;
}

function newId() {
  return crypto.randomBytes(12).toString('hex');
}

// Gang names: trimmed, inner spaces collapsed, 3-20 letters, digits or spaces.
export function normalizeGangName(raw) {
  if (typeof raw !== 'string') return null;
  const name = raw.trim().replace(/\s+/g, ' ');
  return /^[A-Za-z0-9 ]{3,20}$/.test(name) ? name : null;
}

// Unknown keys dropped, numbers floored and clamped to 0..1,000,000, anything else dropped.
export function cleanReward(raw) {
  if (raw === undefined || raw === null) return {};
  if (typeof raw !== 'object' || Array.isArray(raw)) throw bad();
  const reward = {};
  for (const key of REWARD_KEYS) {
    const value = raw[key];
    if (typeof value !== 'number' || !Number.isFinite(value)) continue;
    const amount = Math.min(MAX_REWARD, Math.max(0, Math.floor(value)));
    if (amount > 0) reward[key] = amount;
  }
  return reward;
}

function cleanTeamMember(raw) {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) throw bad();
  return { op: int(raw.op, 0, 19), level: int(raw.level, 1, 9999), stars: int(raw.stars, 0, 999), power: int(raw.power, 0, MAX_POWER) };
}

function cleanFighter(raw) {
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) throw bad();
  return {
    op: int(raw.op, 0, 19),
    level: int(raw.level, 1, 9999),
    stars: int(raw.stars, 0, 999),
    hp: num(raw.hp, 0, 1e12),
    attack: num(raw.attack, 0, 1e12),
    defence: num(raw.defence, 0, 75),
    power: int(raw.power, 0, MAX_POWER),
  };
}

function iso(value) {
  if (!value) return '';
  return value instanceof Date ? value.toISOString() : String(value);
}

function body(raw) {
  return raw && typeof raw === 'object' && !Array.isArray(raw) ? raw : {};
}

// ---------------------------------------------------------------------- service
export class SocialService {
  constructor({ store, config, now = () => new Date(), log = () => {} }) {
    this.store = store;
    this.config = config;
    this.now = now;
    this.log = log;
  }

  async limit(account, kind) {
    const max = kind === 'write' ? WRITES_PER_MINUTE : READS_PER_MINUTE;
    const count = await this.store.hit(`${kind}:${account.uid}`, 60, this.now());
    if (count > max) throw new ApiError(429, 'rate_limited', { retryAfterSeconds: 60 });
  }

  requireAdmin(account) {
    if (account.role !== 'admin') throw new ApiError(403, 'forbidden');
  }

  // ------------------------------------------------------------------- profile
  async putProfile(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    if (b.team !== undefined && (!Array.isArray(b.team) || b.team.length > 3)) throw bad();
    const profile = {
      uid: account.uid,
      name: account.username,
      level: int(b.level, 1, 999),
      power: int(b.power, 0, MAX_POWER),
      lobbyOp: optInt(b.lobbyOp, 0, 19, 0),
      team: (b.team ?? []).map(cleanTeamMember),
      updatedAt: this.now(),
    };
    await this.store.putProfile(account.uid, profile);
    return { ok: true };
  }

  // Cards for a list of uids, in that order (accounts that no longer exist are skipped).
  async cards(uids) {
    if (uids.length === 0) return [];
    const [accounts, profiles, members] = await Promise.all([
      this.store.findAccountsByUids(uids), this.store.getProfiles(uids), this.store.getGangMembersByUids(uids),
    ]);
    const gangIds = [...new Set(members.map((m) => m.gangId))];
    const gangs = gangIds.length ? await this.store.getGangsByIds(gangIds) : [];
    const accountBy = new Map(accounts.map((a) => [a.uid, a]));
    const profileBy = new Map(profiles.map((p) => [p.uid, p]));
    const gangNameBy = new Map(gangs.map((g) => [g.id, g.name]));
    const gangOf = new Map(members.map((m) => [m.uid, gangNameBy.get(m.gangId) ?? '']));
    const cards = [];
    for (const uid of uids) {
      const acc = accountBy.get(uid);
      if (!acc) continue;
      const profile = profileBy.get(uid);
      cards.push({
        uid: String(uid),
        name: acc.username,
        level: profile ? profile.level : 1,
        power: profile ? profile.power : 0,
        lastSeen: iso(profile ? profile.updatedAt : acc.lastLoginAt),
        gang: gangOf.get(uid) ?? '',
      });
    }
    return cards;
  }

  // ------------------------------------------------------------------- friends
  async friendState(uid) {
    const links = await this.store.listFriendLinks(uid);
    const friends = []; const incoming = []; const outgoing = [];
    for (const link of links) {
      const other = link.a === uid ? link.b : link.a;
      if (link.status === 'friends') friends.push(other);
      else if (link.to === uid) incoming.push(other);
      else outgoing.push(other);
    }
    return { friends, incoming, outgoing };
  }

  async listFriends(account) {
    await this.limit(account, 'read');
    const state = await this.friendState(account.uid);
    const [friends, incoming, outgoing] = await Promise.all([this.cards(state.friends), this.cards(state.incoming), this.cards(state.outgoing)]);
    return { ok: true, friends, incoming, outgoing };
  }

  async findPlayer(raw) {
    if (typeof raw === 'number' || (typeof raw === 'string' && /^\s*\d+\s*$/.test(raw))) {
      return this.store.findAccountByUid(uidOf(raw));
    }
    if (typeof raw !== 'string' || raw.trim().length === 0 || raw.length > 40) throw bad();
    return this.store.findAccountByUsername(raw.trim().toLowerCase());
  }

  async requestFriend(account, raw) {
    await this.limit(account, 'write');
    const target = await this.findPlayer(body(raw).to);
    if (!target) throw new ApiError(404, 'not_found');
    if (target.uid === account.uid) throw new ApiError(400, 'self');
    const link = await this.store.getFriendLink(account.uid, target.uid);
    if (link && link.status === 'friends') throw new ApiError(409, 'already_friends');
    if (link && link.from === account.uid) throw new ApiError(409, 'already_requested');
    if (link && link.from === target.uid) {
      await this.acceptFrom(account.uid, target.uid);
      return { ok: true, status: 'accepted' };
    }
    const [mine, theirs] = await Promise.all([this.friendState(account.uid), this.friendState(target.uid)]);
    if (mine.friends.length >= MAX_FRIENDS || mine.outgoing.length >= MAX_PENDING || theirs.incoming.length >= MAX_PENDING) {
      throw new ApiError(409, 'friends_full');
    }
    const created = await this.store.addFriendRequest({ from: account.uid, to: target.uid, createdAt: this.now() });
    if (!created) throw new ApiError(409, 'already_requested');
    return { ok: true, status: 'sent' };
  }

  async acceptFrom(uid, fromUid) {
    const [mine, theirs] = await Promise.all([this.friendState(uid), this.friendState(fromUid)]);
    if (mine.friends.length >= MAX_FRIENDS || theirs.friends.length >= MAX_FRIENDS) throw new ApiError(409, 'friends_full');
    if (!(await this.store.acceptFriendRequest(fromUid, uid, this.now()))) throw new ApiError(404, 'not_found');
  }

  async acceptFriend(account, raw) {
    await this.limit(account, 'write');
    const from = uidOf(body(raw).uid);
    if (from === account.uid) throw new ApiError(400, 'self');
    await this.acceptFrom(account.uid, from);
    return { ok: true };
  }

  async declineFriend(account, raw) {
    await this.limit(account, 'write');
    const other = uidOf(body(raw).uid);
    if (other === account.uid) throw new ApiError(400, 'self');
    if (!(await this.store.removeFriendLink(account.uid, other, 'pending'))) throw new ApiError(404, 'not_found');
    return { ok: true };
  }

  async removeFriend(account, raw) {
    await this.limit(account, 'write');
    const other = uidOf(body(raw).uid);
    if (other === account.uid) throw new ApiError(400, 'self');
    if (!(await this.store.removeFriendLink(account.uid, other, 'friends'))) throw new ApiError(404, 'not_found');
    return { ok: true };
  }

  // --------------------------------------------------------------------- gangs
  // Brings a gang to the current season: the first caller in a new month closes the old season (atomic), then every member
  // still on an older season is settled (atomic per member: paid once, then reset). Returns the up-to-date gang or null.
  async freshGang(gangId) {
    let gang = await this.store.getGang(gangId);
    if (!gang) return null;
    const season = seasonKey(this.now());
    if (gang.season !== season) {
      await this.store.rollGangSeason(gang.id, gang.season, season);
      gang = await this.store.getGang(gangId);
      if (!gang) return null;
    }
    const members = await this.store.listGangMembers(gang.id);
    for (const member of members) {
      if (member.season === gang.season) continue;
      const before = await this.store.settleGangMember(member.uid, gang.id, gang.season);
      if (!before) continue;
      // Only the season that just finished pays (an empty month in between, with no request at all, pays nothing).
      if (before.season === gang.lastSeason) {
        const coins = gangSeasonCoins(gang.lastSeasonPoints ?? 0, before.seasonContribution ?? 0);
        await this.store.addPendingCoins(member.uid, coins, gang.lastSeason);
      }
    }
    return gang;
  }

  async myMembership(account) {
    const member = await this.store.getGangMember(account.uid);
    if (!member) return { member: null, gang: null };
    const gang = await this.freshGang(member.gangId);
    if (!gang) return { member: null, gang: null };
    return { member: await this.store.getGangMember(account.uid), gang };
  }

  async requireMembership(account) {
    const state = await this.myMembership(account);
    if (!state.member) throw new ApiError(409, 'not_in_gang');
    return state;
  }

  gangSummary(gang) {
    return {
      id: gang.id,
      name: gang.name,
      level: gangLevel(gang.xp),
      members: gang.memberCount,
      maxMembers: MAX_GANG_MEMBERS,
      minLevel: gang.minLevel,
      minPower: gang.minPower,
      open: gang.open,
      leaderName: gang.leaderName,
    };
  }

  async gangDetail(gang, uid) {
    const members = await this.store.listGangMembers(gang.id);
    const uids = members.map((m) => m.uid);
    const [accounts, profiles] = await Promise.all([this.store.findAccountsByUids(uids), this.store.getProfiles(uids)]);
    const accountBy = new Map(accounts.map((a) => [a.uid, a]));
    const profileBy = new Map(profiles.map((p) => [p.uid, p]));
    const me = members.find((m) => m.uid === uid);
    const list = members.map((m) => {
      const profile = profileBy.get(m.uid);
      const acc = accountBy.get(m.uid);
      return {
        uid: String(m.uid),
        name: acc ? acc.username : '',
        role: m.role,
        level: profile ? profile.level : 1,
        power: profile ? profile.power : 0,
        contribution: m.contribution,
        seasonContribution: m.season === gang.season ? m.seasonContribution : 0,
        lastSeen: iso(profile ? profile.updatedAt : acc && acc.lastLoginAt),
      };
    });
    list.sort((x, y) => (x.role === 'leader' ? -1 : y.role === 'leader' ? 1 : y.seasonContribution - x.seasonContribution || y.contribution - x.contribution));
    return {
      ...this.gangSummary({ ...gang, memberCount: members.length }),
      xp: gang.xp,
      season: gang.season,
      seasonPoints: gang.seasonPoints,
      milestonesClaimed: me && me.season === gang.season ? [...me.milestones].sort((a, b) => a - b) : [],
      myRole: me ? me.role : 'member',
      members: list,
    };
  }

  async searchGangs(account, query) {
    await this.limit(account, 'read');
    const q = typeof query === 'string' ? query.trim().replace(/\s+/g, ' ').slice(0, 20).toLowerCase() : '';
    const gangs = await this.store.searchGangs({ q, season: seasonKey(this.now()), limit: 20 });
    return { ok: true, gangs: gangs.map((g) => this.gangSummary(g)) };
  }

  async myGang(account) {
    await this.limit(account, 'read');
    const { gang } = await this.myMembership(account);
    const pending = await this.store.getPendingCoins(account.uid);
    return {
      ok: true,
      gang: gang ? await this.gangDetail(gang, account.uid) : null,
      pendingCoins: pending ? pending.coins : 0,
      pendingSeason: pending ? pending.season : '',
    };
  }

  newMember(uid, gangId, role, season) {
    const now = this.now();
    return {
      uid, gangId, role, season, contribution: 0, seasonContribution: 0, milestones: [],
      day: dayKey(now), dayCheckins: 0, dayDonations: 0, dayActivity: 0, lastGrant: 0, joinedAt: now,
    };
  }

  async createGang(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    const name = normalizeGangName(b.name);
    if (!name) throw new ApiError(400, 'invalid_name');
    const minLevel = optInt(b.minLevel, 0, 999, 0);
    const minPower = optInt(b.minPower, 0, MAX_POWER, 0);
    const open = bool(b.open, true);
    if (await this.store.getGangMember(account.uid)) throw new ApiError(409, 'in_gang');
    const now = this.now();
    const season = seasonKey(now);
    const gang = {
      id: newId(), name, nameLower: name.toLowerCase(), leaderUid: account.uid, leaderName: account.username,
      minLevel, minPower, open, xp: 0, season, seasonPoints: 0, lastSeason: '', lastSeasonPoints: 0,
      memberCount: 0, createdAt: now, lastActiveAt: now,
    };
    try {
      await this.store.createGang(gang);
    } catch (error) {
      if (error instanceof StoreConflict) throw new ApiError(409, 'name_taken');
      throw error;
    }
    const joined = await this.store.addGangMember(gang.id, this.newMember(account.uid, gang.id, 'leader', season), { max: MAX_GANG_MEMBERS, requireOpen: false });
    if (joined !== 'ok') {
      await this.store.deleteGang(gang.id);
      throw new ApiError(409, 'in_gang');
    }
    this.log('info', 'gang created', { uid: account.uid, gang: gang.id });
    return { ok: true, gang: await this.gangDetail(await this.store.getGang(gang.id), account.uid) };
  }

  async joinGang(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    const id = idOf(b.id);
    const level = int(b.level, 0, 999);
    const power = int(b.power, 0, MAX_POWER);
    if (await this.store.getGangMember(account.uid)) throw new ApiError(409, 'in_gang');
    const gang = await this.freshGang(id);
    if (!gang) throw new ApiError(404, 'not_found');
    if (!gang.open) throw new ApiError(403, 'gang_closed');
    if (level < gang.minLevel || power < gang.minPower) throw new ApiError(403, 'requirements');
    const result = await this.store.addGangMember(gang.id, this.newMember(account.uid, gang.id, 'member', gang.season), { max: MAX_GANG_MEMBERS, requireOpen: true });
    if (result === 'full') throw new ApiError(409, 'gang_full');
    if (result === 'closed') throw new ApiError(403, 'gang_closed');
    if (result === 'in_gang') throw new ApiError(409, 'in_gang');
    if (result !== 'ok') throw new ApiError(404, 'not_found');
    return { ok: true, gang: await this.gangDetail(await this.store.getGang(gang.id), account.uid) };
  }

  // Removes a member; a leaving leader hands over to the member with the most contribution; the last one out deletes the gang.
  async removeMember(gang, uid) {
    const result = await this.store.removeGangMember(gang.id, uid);
    if (!result.removed || result.remaining === 0) return;
    const fresh = await this.store.getGang(gang.id);
    if (!fresh || fresh.leaderUid !== uid) return;
    const members = await this.store.listGangMembers(gang.id);
    if (members.length === 0) return;
    members.sort((x, y) => y.contribution - x.contribution || new Date(x.joinedAt) - new Date(y.joinedAt) || x.uid - y.uid);
    const heir = members[0];
    const acc = await this.store.findAccountByUid(heir.uid);
    await this.store.updateGangMember(heir.uid, { role: 'leader' });
    await this.store.updateGang(gang.id, { leaderUid: heir.uid, leaderName: acc ? acc.username : '' });
  }

  async leaveGang(account) {
    await this.limit(account, 'write');
    const { gang } = await this.requireMembership(account);
    await this.removeMember(gang, account.uid);
    return { ok: true };
  }

  async kickMember(account, raw) {
    await this.limit(account, 'write');
    const target = uidOf(body(raw).uid);
    const { member, gang } = await this.requireMembership(account);
    if (member.role !== 'leader') throw new ApiError(403, 'not_leader');
    if (target === account.uid) throw new ApiError(400, 'self');
    const other = await this.store.getGangMember(target);
    if (!other || other.gangId !== gang.id) throw new ApiError(404, 'not_found');
    await this.removeMember(gang, target);
    return { ok: true, gang: await this.gangDetail(await this.store.getGang(gang.id), account.uid) };
  }

  async gangSettings(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    const { member, gang } = await this.requireMembership(account);
    if (member.role !== 'leader') throw new ApiError(403, 'not_leader');
    const patch = {
      minLevel: optInt(b.minLevel, 0, 999, gang.minLevel),
      minPower: optInt(b.minPower, 0, MAX_POWER, gang.minPower),
      open: bool(b.open, gang.open),
    };
    await this.store.updateGang(gang.id, patch);
    return { ok: true, gang: await this.gangDetail(await this.store.getGang(gang.id), account.uid) };
  }

  // checkin: 50 once a UTC day. donate: amount = 10 per donation (10..50), at most 5 donations a day. activity: amount 1..50,
  // at most 200 a day. A call over the cap is not an error: it adds what is left (possibly 0).
  async contribute(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    let spec;
    if (b.kind === 'checkin') spec = { field: 'dayCheckins', cap: 1, units: 1, points: CHECKIN_POINTS };
    else if (b.kind === 'donate') {
      const amount = optInt(b.amount, DONATION_POINTS, DONATION_POINTS * MAX_DONATIONS_PER_DAY, DONATION_POINTS);
      if (amount % DONATION_POINTS !== 0) throw bad();
      spec = { field: 'dayDonations', cap: MAX_DONATIONS_PER_DAY, units: amount / DONATION_POINTS, points: DONATION_POINTS };
    } else if (b.kind === 'activity') {
      spec = { field: 'dayActivity', cap: MAX_ACTIVITY_PER_DAY, units: int(b.amount, 1, MAX_ACTIVITY_PER_CALL), points: 1 };
    } else throw bad();
    const { gang } = await this.requireMembership(account);
    const now = this.now();
    const granted = await this.store.contributeGangMember(account.uid, gang.id, { day: dayKey(now), ...spec });
    if (granted === null) throw new ApiError(409, 'not_in_gang');
    const added = granted * spec.points;
    if (added > 0) await this.store.addGangPoints(gang.id, added, now);
    return { ok: true, gang: await this.gangDetail(await this.store.getGang(gang.id), account.uid), added };
  }

  async claimGangCoins(account) {
    await this.limit(account, 'write');
    await this.myMembership(account); // a season end that is due pays out first
    const taken = await this.store.takePendingCoins(account.uid);
    if (!taken || !(taken.coins > 0)) throw new ApiError(409, 'nothing_to_claim');
    return { ok: true, coins: taken.coins, season: taken.season };
  }

  async claimMilestone(account, raw) {
    await this.limit(account, 'write');
    const index = int(body(raw).index, 0, GANG_MILESTONES.length - 1);
    const { gang } = await this.requireMembership(account);
    if (gang.seasonPoints < GANG_MILESTONES[index]) throw new ApiError(409, 'not_reached');
    if (!(await this.store.claimGangMilestone(account.uid, gang.id, gang.season, index))) throw new ApiError(409, 'already_claimed');
    return { ok: true };
  }

  // ---------------------------------------------------------------------- mail
  mailView(mail) {
    return { id: mail.id, title: mail.title, body: mail.body, reward: { ...mail.reward }, from: 'Admin', createdAt: iso(mail.createdAt) };
  }

  async listMail(account) {
    await this.limit(account, 'read');
    const mail = await this.store.listMailFor(account.uid, new Date(account.createdAt), 50);
    return { ok: true, mail: mail.map((m) => this.mailView(m)) };
  }

  async ackMail(account, raw) {
    await this.limit(account, 'write');
    const ids = body(raw).ids;
    if (!Array.isArray(ids) || ids.length > 50) throw bad();
    const clean = [...new Set(ids.map(idOf))];
    if (clean.length) await this.store.ackMail(account.uid, new Date(account.createdAt), clean, this.now());
    return { ok: true };
  }

  async sendMail(account, raw) {
    this.requireAdmin(account);
    await this.limit(account, 'write');
    const b = body(raw);
    const title = text(b.title, 1, 60);
    const bodyText = b.body === undefined ? '' : text(b.body, 0, 600);
    const reward = cleanReward(b.reward);
    let to;
    if (b.to === 'all') to = 'all';
    else {
      const uid = uidOf(b.to);
      if (!(await this.store.findAccountByUid(uid))) throw new ApiError(404, 'not_found');
      to = uid;
    }
    const mail = { id: newId(), to, title, body: bodyText, reward, from: account.uid, createdAt: this.now() };
    await this.store.createMail(mail);
    this.log('info', 'admin mail', { by: account.uid, to: to === 'all' ? 'all' : to, id: mail.id });
    return { ok: true, id: mail.id, recipients: to === 'all' ? 'all' : 1 };
  }

  // ------------------------------------------------------------------- notices
  async listNotices(account) {
    await this.limit(account, 'read');
    const notices = await this.store.listNotices(30);
    return {
      ok: true,
      notices: notices.map((n) => ({ id: n.id, category: n.category, title: n.title, body: n.body, createdAt: iso(n.createdAt), pinned: n.pinned })),
    };
  }

  async createNotice(account, raw) {
    this.requireAdmin(account);
    await this.limit(account, 'write');
    const b = body(raw);
    if (!NOTICE_CATEGORIES.includes(b.category)) throw bad();
    const notice = {
      id: newId(), category: b.category, title: text(b.title, 1, 60), body: text(b.body, 1, 2000), pinned: bool(b.pinned, false),
      createdAt: this.now(),
    };
    await this.store.createNotice(notice);
    return { ok: true, id: notice.id };
  }

  async deleteNotice(account, raw) {
    this.requireAdmin(account);
    await this.limit(account, 'write');
    if (!(await this.store.deleteNotice(idOf(body(raw).id)))) throw new ApiError(404, 'not_found');
    return { ok: true };
  }

  // --------------------------------------------------------------------- arena
  // The player's arena record, created on first use and brought to the current season (atomic: one season end per month).
  async arenaRecord(uid, name) {
    const season = seasonKey(this.now());
    let record = await this.store.getArena(uid);
    if (!record) {
      record = await this.store.createArena({
        uid, name, trophies: 0, season, champion: false, wins: 0, losses: 0, defenceWins: 0, defenceLosses: 0,
        log: [], recent: [], pendingReward: null, team: [], power: 0, hasDefence: false, lastDelta: 0,
      });
    }
    const end = arenaSeasonEnd(record, season);
    if (end) {
      await this.store.rollArenaSeason(uid, record.season, end);
      record = await this.store.getArena(uid);
    }
    return record;
  }

  arenaView(record) {
    return {
      ok: true,
      trophies: record.trophies,
      rank: arenaRank(record.trophies),
      season: record.season,
      champion: Boolean(record.champion),
      wins: record.wins,
      losses: record.losses,
      defenceWins: record.defenceWins,
      defenceLosses: record.defenceLosses,
      log: [...record.log].reverse().map((e) => ({ at: iso(e.at), attacker: e.attacker, won: e.won, delta: e.delta })),
      pendingReward: record.pendingReward ? { season: record.pendingReward.season, rank: record.pendingReward.rank } : null,
    };
  }

  async putDefence(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    if (!Array.isArray(b.team) || b.team.length < 1 || b.team.length > 3) throw bad();
    const team = b.team.map(cleanFighter);
    const power = int(b.power, 0, MAX_POWER);
    await this.arenaRecord(account.uid, account.username);
    await this.store.setArenaDefence(account.uid, { team, power, name: account.username, hasDefence: true, defenceAt: this.now() });
    return { ok: true };
  }

  async arenaMe(account) {
    await this.limit(account, 'read');
    return this.arenaView(await this.arenaRecord(account.uid, account.username));
  }

  // Strength first (power within ±15%, ±30%, ±60%, then anyone), then the closest trophies (ties random).
  async findOpponent(me, power) {
    const season = seasonKey(this.now());
    const exclude = [me.uid, ...(me.recent ?? [])];
    const bands = [...POWER_BANDS.map((f) => ({ minPower: Math.floor(power * (1 - f)), maxPower: Math.ceil(power * (1 + f)) })), {}];
    for (const band of bands) {
      const candidates = await this.store.findArenaCandidates({ exclude, ...band, limit: 200 });
      if (candidates.length === 0) continue;
      let best = []; let bestGap = Infinity;
      for (const c of candidates) {
        const end = arenaSeasonEnd(c, season);
        const gap = Math.abs((end ? end.trophies : c.trophies) - me.trophies);
        if (gap < bestGap) { best = [c]; bestGap = gap; } else if (gap === bestGap) best.push(c);
      }
      return best[crypto.randomInt(0, best.length)];
    }
    return null;
  }

  async arenaMatch(account, raw) {
    await this.limit(account, 'write');
    const power = int(body(raw).power, 0, MAX_POWER);
    const me = await this.arenaRecord(account.uid, account.username);
    const found = await this.findOpponent(me, power);
    if (!found) return { ok: true, matchId: '', opponent: null };
    const opponent = await this.arenaRecord(found.uid, found.name);
    const now = this.now();
    const match = {
      id: newId(), attacker: account.uid, defender: opponent.uid, createdAt: now,
      expiresAt: new Date(now.getTime() + MATCH_MINUTES * 60_000), purgeAt: new Date(now.getTime() + 86_400_000), done: false,
    };
    await this.store.createArenaMatch(match);
    await this.store.setArenaRecent(account.uid, [...(me.recent ?? []), opponent.uid].slice(-RECENT_OPPONENTS));
    return {
      ok: true,
      matchId: match.id,
      opponent: {
        uid: String(opponent.uid), name: opponent.name, trophies: opponent.trophies, rank: arenaRank(opponent.trophies),
        power: opponent.power, team: opponent.team.map((f) => ({ ...f })),
      },
    };
  }

  async arenaResult(account, raw) {
    await this.limit(account, 'write');
    const b = body(raw);
    const matchId = idOf(b.matchId);
    const won = bool(b.won, undefined);
    if (won === undefined) throw bad();
    const match = await this.store.getArenaMatch(matchId);
    if (!match || match.attacker !== account.uid) throw new ApiError(404, 'not_found');
    if (match.done) throw new ApiError(409, 'already_reported');
    if (new Date(match.expiresAt) <= this.now()) throw new ApiError(409, 'match_expired');
    if (!(await this.store.takeArenaMatch(matchId, account.uid))) throw new ApiError(409, 'already_reported');

    const me = await this.arenaRecord(account.uid, account.username);
    const defenderAccount = await this.store.findAccountByUid(match.defender);
    const them = await this.arenaRecord(match.defender, defenderAccount ? defenderAccount.username : '');
    const delta = won ? trophyDelta(me.trophies, them.trophies) : trophyDelta(them.trophies, me.trophies);
    const now = this.now();
    const after = await this.store.applyArenaResult(account.uid, { delta: won ? delta : -delta, win: won, defence: false });
    await this.store.applyArenaResult(match.defender, {
      delta: won ? -delta : delta, win: !won, defence: true, logEntry: { at: now, attacker: account.username, won: !won },
    });
    return { ok: true, trophies: after.trophies, rank: arenaRank(after.trophies), delta: after.lastDelta };
  }

  async arenaClaim(account) {
    await this.limit(account, 'write');
    await this.arenaRecord(account.uid, account.username);
    const taken = await this.store.takeArenaReward(account.uid);
    if (!taken) throw new ApiError(409, 'nothing_to_claim');
    return { ok: true, season: taken.season, rank: taken.rank };
  }
}
