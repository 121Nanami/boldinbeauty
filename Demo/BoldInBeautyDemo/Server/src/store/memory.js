// In-memory store: used by the tests and by `npm run dev`. Nothing survives a restart. It follows the same contract as the
// MongoDB store (test/store.contract.js runs against both).
import { StoreConflict } from './errors.js';

const clone = (value) => (value == null ? null : structuredClone(value));
const pairKey = (x, y) => (x < y ? `${x}:${y}` : `${y}:${x}`);
const CHAMPION_TROPHIES = 1500;

export class MemoryStore {
  constructor() {
    this.accounts = new Map();      // uid -> account
    this.challenges = new Map();    // emailLower -> challenge
    this.sessions = new Map();      // tokenHash -> session
    this.saves = new Map();         // uid -> { data, savedAt, updatedAt }
    this.admins = new Map();        // uid -> { uid, email, flaggedAt }
    this.rate = new Map();          // key -> { count, expiresAt }
    this.profiles = new Map();      // uid -> public profile
    this.friends = new Map();       // "lowUid:highUid" -> { a, b, from, to, status, createdAt, acceptedAt }
    this.gangs = new Map();         // id -> gang
    this.gangMembers = new Map();   // uid -> membership (one gang per player)
    this.gangPending = new Map();   // uid -> { coins, season }
    this.mail = new Map();          // id -> mail
    this.mailAcks = new Map();      // "uid:mailId" -> acknowledged at
    this.notices = new Map();       // id -> notice
    this.arena = new Map();         // uid -> arena record
    this.arenaMatches = new Map();  // id -> match
    this.uidSeq = 10_000_000;
  }

  async init() {}
  async close() {}

  // ------------------------------------------------------------------ accounts
  async findAccountByEmail(emailLower) {
    for (const account of this.accounts.values()) if (account.emailLower === emailLower) return { ...account };
    return null;
  }

  async findAccountByUsername(usernameLower) {
    for (const account of this.accounts.values()) if (account.usernameLower === usernameLower) return { ...account };
    return null;
  }

  async findAccountByUid(uid) {
    const account = this.accounts.get(uid);
    return account ? { ...account } : null;
  }

  async findAccountsByUids(uids) {
    return uids.map((uid) => this.accounts.get(uid)).filter(Boolean).map((account) => ({ ...account }));
  }

  async nextUid() {
    this.uidSeq += 1;
    return this.uidSeq;
  }

  async createAccount(account) {
    for (const other of this.accounts.values()) {
      if (other.emailLower === account.emailLower) throw new StoreConflict('email');
      if (other.usernameLower === account.usernameLower) throw new StoreConflict('username');
    }
    if (this.accounts.has(account.uid)) throw new StoreConflict('uid');
    this.accounts.set(account.uid, { ...account });
  }

  async updateAccount(uid, patch) {
    const account = this.accounts.get(uid);
    if (account) Object.assign(account, patch);
  }

  async upsertAdmin(uid, email, now) {
    const existing = this.admins.get(uid);
    this.admins.set(uid, { uid, email, flaggedAt: existing ? existing.flaggedAt : now, lastSeenAt: now });
  }

  async removeAdmin(uid) {
    this.admins.delete(uid);
  }

  // ---------------------------------------------------------------- challenges
  async putChallenge(challenge) {
    this.challenges.set(challenge.emailLower, { ...challenge, attempts: 0 });
  }

  async getChallenge(emailLower) {
    const challenge = this.challenges.get(emailLower);
    return challenge ? { ...challenge } : null;
  }

  async bumpAttempts(emailLower) {
    const challenge = this.challenges.get(emailLower);
    if (!challenge) return null;
    challenge.attempts += 1;
    return challenge.attempts;
  }

  async takeChallenge(emailLower) {
    const challenge = this.challenges.get(emailLower);
    if (!challenge) return null;
    this.challenges.delete(emailLower);
    return { ...challenge };
  }

  async deleteChallenge(emailLower) {
    this.challenges.delete(emailLower);
  }

  // ------------------------------------------------------------------ sessions
  async createSession(session) {
    this.sessions.set(session.tokenHash, { ...session });
  }

  async getSession(tokenHash) {
    const session = this.sessions.get(tokenHash);
    return session ? { ...session } : null;
  }

  async deleteSession(tokenHash) {
    return this.sessions.delete(tokenHash);
  }

  async deleteSessionsForUid(uid) {
    let count = 0;
    for (const [hash, session] of this.sessions) {
      if (session.uid === uid) {
        this.sessions.delete(hash);
        count += 1;
      }
    }
    return count;
  }

  // --------------------------------------------------------------------- saves
  async getSave(uid) {
    const save = this.saves.get(uid);
    return save ? { ...save } : null;
  }

  // Keeps the newer save. Returns { ok: true } or { stale: true, savedAt } when the stored one is newer.
  async putSave(uid, data, savedAt, now) {
    const current = this.saves.get(uid);
    if (current && current.savedAt > savedAt) return { stale: true, savedAt: current.savedAt };
    this.saves.set(uid, { data, savedAt, updatedAt: now });
    return { ok: true };
  }

  // ---------------------------------------------------------------- rate limits
  // Fixed window counter: returns how many hits the key has had in the current window (including this one).
  async hit(key, windowSeconds, now) {
    const current = this.rate.get(key);
    if (!current || current.expiresAt <= now) {
      this.rate.set(key, { count: 1, expiresAt: new Date(now.getTime() + windowSeconds * 1000) });
      return 1;
    }
    current.count += 1;
    return current.count;
  }

  // ------------------------------------------------------------------ profiles
  async putProfile(uid, profile) {
    this.profiles.set(uid, clone(profile));
  }

  async getProfiles(uids) {
    return uids.map((uid) => this.profiles.get(uid)).filter(Boolean).map(clone);
  }

  // ------------------------------------------------------------------- friends
  // One link per pair: { a: lower uid, b: higher uid, from, to, status: 'pending' | 'friends' }.
  async getFriendLink(x, y) {
    return clone(this.friends.get(pairKey(x, y)));
  }

  async listFriendLinks(uid) {
    return [...this.friends.values()].filter((link) => link.a === uid || link.b === uid).map(clone);
  }

  // false when the pair already has a link (request or friendship).
  async addFriendRequest({ from, to, createdAt }) {
    const key = pairKey(from, to);
    if (this.friends.has(key)) return false;
    this.friends.set(key, { a: Math.min(from, to), b: Math.max(from, to), from, to, status: 'pending', createdAt, acceptedAt: null });
    return true;
  }

  // Turns the pending request from -> to into a friendship; false when there is no such request.
  async acceptFriendRequest(from, to, now) {
    const link = this.friends.get(pairKey(from, to));
    if (!link || link.status !== 'pending' || link.from !== from || link.to !== to) return false;
    link.status = 'friends';
    link.acceptedAt = now;
    return true;
  }

  async removeFriendLink(x, y, status) {
    const key = pairKey(x, y);
    const link = this.friends.get(key);
    if (!link || link.status !== status) return false;
    this.friends.delete(key);
    return true;
  }

  // --------------------------------------------------------------------- gangs
  async createGang(gang) {
    for (const other of this.gangs.values()) if (other.nameLower === gang.nameLower) throw new StoreConflict('name');
    this.gangs.set(gang.id, clone(gang));
  }

  async deleteGang(id) {
    this.gangs.delete(id);
  }

  async getGang(id) {
    return clone(this.gangs.get(id));
  }

  async getGangsByIds(ids) {
    return ids.map((id) => this.gangs.get(id)).filter(Boolean).map(clone);
  }

  async updateGang(id, patch) {
    const gang = this.gangs.get(id);
    if (gang) Object.assign(gang, clone(patch));
  }

  // q = lower-case text the name must contain; empty q = most active (this season's points, then members, then xp).
  async searchGangs({ q, season, limit }) {
    const all = [...this.gangs.values()].filter((g) => g.memberCount > 0 && (!q || g.nameLower.includes(q)));
    // Gangs on the current season first (by its points), then the others; same order as the MongoDB store.
    const current = (g) => (g.season === season ? 1 : 0);
    all.sort(q
      ? (x, y) => y.memberCount - x.memberCount || x.nameLower.localeCompare(y.nameLower)
      : (x, y) => current(y) - current(x) || (current(x) ? y.seasonPoints - x.seasonPoints : 0) || y.memberCount - x.memberCount || y.xp - x.xp);
    return all.slice(0, limit).map(clone);
  }

  async addGangPoints(id, points, now) {
    const gang = this.gangs.get(id);
    if (!gang) return;
    gang.xp += points;
    gang.seasonPoints += points;
    gang.lastActiveAt = now;
  }

  // Closes the season `from` (only if the gang is still on it): remembers its points, resets them, moves to `to`.
  async rollGangSeason(id, from, to) {
    const gang = this.gangs.get(id);
    if (!gang || gang.season !== from) return false;
    Object.assign(gang, { lastSeason: from, lastSeasonPoints: gang.seasonPoints, season: to, seasonPoints: 0 });
    return true;
  }

  // Returns 'ok' | 'missing' | 'closed' | 'full' | 'in_gang'.
  async addGangMember(gangId, member, { max, requireOpen }) {
    const gang = this.gangs.get(gangId);
    if (!gang) return 'missing';
    if (requireOpen && !gang.open) return 'closed';
    if (gang.memberCount >= max) return 'full';
    if (this.gangMembers.has(member.uid)) return 'in_gang';
    this.gangMembers.set(member.uid, clone(member));
    gang.memberCount += 1;
    return 'ok';
  }

  // Removes the membership; deletes the gang when nobody is left. Returns { removed, remaining }.
  async removeGangMember(gangId, uid) {
    const member = this.gangMembers.get(uid);
    const gang = this.gangs.get(gangId);
    if (!member || member.gangId !== gangId || !gang) return { removed: false, remaining: gang ? gang.memberCount : 0 };
    this.gangMembers.delete(uid);
    gang.memberCount -= 1;
    if (gang.memberCount <= 0) this.gangs.delete(gangId);
    return { removed: true, remaining: Math.max(0, gang.memberCount) };
  }

  async getGangMember(uid) {
    return clone(this.gangMembers.get(uid));
  }

  async getGangMembersByUids(uids) {
    return uids.map((uid) => this.gangMembers.get(uid)).filter(Boolean).map(clone);
  }

  async listGangMembers(gangId) {
    return [...this.gangMembers.values()].filter((m) => m.gangId === gangId).map(clone);
  }

  async updateGangMember(uid, patch) {
    const member = this.gangMembers.get(uid);
    if (member) Object.assign(member, clone(patch));
  }

  // Moves a member to `season` once: returns the membership as it was (with the finished season's numbers), or null.
  async settleGangMember(uid, gangId, season) {
    const member = this.gangMembers.get(uid);
    if (!member || member.gangId !== gangId || member.season === season) return null;
    const before = clone(member);
    Object.assign(member, { season, seasonContribution: 0, milestones: [] });
    return before;
  }

  // Adds up to `units` of today's allowance (`cap` per UTC day in `field`), `points` each. Returns the units granted, or null
  // when the player is not (or no longer) in that gang.
  async contributeGangMember(uid, gangId, { day, field, cap, units, points }) {
    const member = this.gangMembers.get(uid);
    if (!member || member.gangId !== gangId) return null;
    if (member.day !== day) Object.assign(member, { day, dayCheckins: 0, dayDonations: 0, dayActivity: 0 });
    const granted = Math.max(0, Math.min(units, cap - member[field]));
    member[field] += granted;
    member.contribution += granted * points;
    member.seasonContribution += granted * points;
    member.lastGrant = granted;
    return granted;
  }

  async claimGangMilestone(uid, gangId, season, index) {
    const member = this.gangMembers.get(uid);
    if (!member || member.gangId !== gangId || member.season !== season || member.milestones.includes(index)) return false;
    member.milestones.push(index);
    return true;
  }

  async addPendingCoins(uid, coins, season) {
    const current = this.gangPending.get(uid) ?? { coins: 0, season: '' };
    this.gangPending.set(uid, { coins: current.coins + coins, season });
  }

  async getPendingCoins(uid) {
    return clone(this.gangPending.get(uid));
  }

  async takePendingCoins(uid) {
    const pending = this.gangPending.get(uid);
    if (!pending) return null;
    this.gangPending.delete(uid);
    return clone(pending);
  }

  // ---------------------------------------------------------------------- mail
  async createMail(mail) {
    this.mail.set(mail.id, clone(mail));
  }

  // Unacknowledged mail for uid: direct mail, and broadcasts created after `since` (the account's creation). Oldest first.
  async listMailFor(uid, since, limit) {
    return [...this.mail.values()]
      .filter((m) => (m.to === uid || (m.to === 'all' && m.createdAt > since)) && !this.mailAcks.has(`${uid}:${m.id}`))
      .sort((x, y) => x.createdAt - y.createdAt || x.id.localeCompare(y.id))
      .slice(0, limit)
      .map(clone);
  }

  // Records acknowledgements only for mail this player can see.
  async ackMail(uid, since, ids, now) {
    for (const id of ids) {
      const m = this.mail.get(id);
      const key = `${uid}:${id}`;
      if (m && (m.to === uid || (m.to === 'all' && m.createdAt > since)) && !this.mailAcks.has(key)) this.mailAcks.set(key, now);
    }
  }

  // ------------------------------------------------------------------- notices
  async createNotice(notice) {
    this.notices.set(notice.id, clone(notice));
  }

  async listNotices(limit) {
    return [...this.notices.values()]
      .sort((x, y) => Number(y.pinned) - Number(x.pinned) || y.createdAt - x.createdAt || y.id.localeCompare(x.id))
      .slice(0, limit)
      .map(clone);
  }

  async deleteNotice(id) {
    return this.notices.delete(id);
  }

  // --------------------------------------------------------------------- arena
  async getArena(uid) {
    return clone(this.arena.get(uid));
  }

  // Inserts the record unless one exists; returns the stored record either way.
  async createArena(record) {
    if (!this.arena.has(record.uid)) this.arena.set(record.uid, clone(record));
    return clone(this.arena.get(record.uid));
  }

  async rollArenaSeason(uid, from, patch) {
    const record = this.arena.get(uid);
    if (!record || record.season !== from) return false;
    Object.assign(record, clone(patch));
    return true;
  }

  async setArenaDefence(uid, patch) {
    const record = this.arena.get(uid);
    if (record) Object.assign(record, clone(patch));
  }

  async setArenaRecent(uid, recent) {
    const record = this.arena.get(uid);
    if (record) record.recent = [...recent];
  }

  // Players with a defence team, not in `exclude`, with power in [minPower, maxPower] when given.
  async findArenaCandidates({ exclude, minPower, maxPower, limit }) {
    const out = [];
    for (const record of this.arena.values()) {
      if (!record.hasDefence || exclude.includes(record.uid)) continue;
      if (minPower !== undefined && (record.power < minPower || record.power > maxPower)) continue;
      out.push(clone(record));
      if (out.length >= limit) break;
    }
    return out;
  }

  async createArenaMatch(match) {
    this.arenaMatches.set(match.id, clone(match));
  }

  async getArenaMatch(id) {
    return clone(this.arenaMatches.get(id));
  }

  // Marks the match reported; true only for the first caller.
  async takeArenaMatch(id, attacker) {
    const match = this.arenaMatches.get(id);
    if (!match || match.attacker !== attacker || match.done) return false;
    match.done = true;
    return true;
  }

  // Applies a signed trophy change with the floors (0, or 1,500 once Champions was reached this season), the win/loss count
  // and, for a defence, the log entry (last 10). Returns the record after, with `lastDelta` = the change actually applied.
  async applyArenaResult(uid, { delta, win, defence, logEntry }) {
    const record = this.arena.get(uid);
    if (!record) return null;
    const floor = record.champion ? CHAMPION_TROPHIES : 0;
    const trophies = delta >= 0 ? record.trophies + delta : Math.max(Math.min(floor, record.trophies), record.trophies + delta);
    record.lastDelta = trophies - record.trophies;
    record.trophies = trophies;
    if (trophies >= CHAMPION_TROPHIES) record.champion = true;
    const counter = defence ? (win ? 'defenceWins' : 'defenceLosses') : (win ? 'wins' : 'losses');
    record[counter] += 1;
    if (logEntry) record.log = [...record.log, { ...logEntry, delta: record.lastDelta }].slice(-10);
    return clone(record);
  }

  async takeArenaReward(uid) {
    const record = this.arena.get(uid);
    if (!record || !record.pendingReward) return null;
    const reward = record.pendingReward;
    record.pendingReward = null;
    return clone(reward);
  }
}
