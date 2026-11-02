// MongoDB store (Atlas or any MongoDB 6+). Same contract as MemoryStore; test/store.contract.js runs against both.
//
// Collections
//   accounts    { _id: uid(Number), uid, username, usernameLower*, email, emailLower*, role, createdAt, emailVerifiedAt, lastLoginAt }   (* unique)
//   admins      { _id: uid, uid, email, flaggedAt, lastSeenAt }            who is flagged as owner/admin
//   challenges  { _id: emailLower, type, username?, codeHash, attempts, createdAt, expiresAt(TTL) }   one pending code per e-mail
//   sessions    { _id: tokenHash, uid, createdAt, expiresAt(TTL) }         the token itself is never stored
//   saves       { _id: uid, data, savedAt, updatedAt }
//   counters    { _id: 'uid', seq }                                         sequential user ids from 10000001
//   rate        { _id: key, count, expiresAt(TTL) }                         fixed-window rate limit counters
//   profiles    { _id: uid, uid, name, level, power, lobbyOp, team, updatedAt }            public profile (docs/online-api.md)
//   friends     { _id: "lowUid:highUid", a, b, from, to, status: pending|friends, createdAt, acceptedAt }   one link per pair
//   gangs       { _id: id, id, name, nameLower*, leaderUid, leaderName, minLevel, minPower, open, xp, season, seasonPoints,
//                 lastSeason, lastSeasonPoints, memberCount, createdAt, lastActiveAt }     (* unique)
//   gang_members { _id: uid, uid, gangId, role, season, contribution, seasonContribution, milestones, day, dayCheckins,
//                 dayDonations, dayActivity, lastGrant, joinedAt }                         one gang per player
//   gang_pending { _id: uid, coins, season }                                end-of-season coins waiting to be claimed
//   mail        { _id: id, id, to: uid|'all', title, body, reward, from, createdAt }
//   mail_acks   { _id: "uid:mailId", uid, mailId, at }                      mail the game has imported
//   notices     { _id: id, id, category, title, body, pinned, createdAt }
//   arena       { _id: uid, uid, name, trophies, season, champion, wins, losses, defenceWins, defenceLosses, log, recent,
//                 pendingReward, team, power, hasDefence, lastDelta }
//   arena_matches { _id: id, id, attacker, defender, createdAt, expiresAt, done, purgeAt(TTL) }
import { MongoClient } from 'mongodb';
import { StoreConflict } from './errors.js';

const FIRST_UID = 10_000_000; // the first account gets 10000001, like the game's default profile id
const CHAMPION_TROPHIES = 1500;

const pairKey = (x, y) => (x < y ? `${x}:${y}` : `${y}:${x}`);
const plain = (doc) => {
  if (!doc) return null;
  const { _id, ...rest } = doc;
  return rest;
};
const escapeRegex = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

function duplicateField(error) {
  if (!error || error.code !== 11000) return null;
  const pattern = error.keyPattern ?? {};
  if (pattern.emailLower) return 'email';
  if (pattern.usernameLower) return 'username';
  if (pattern._id) return 'uid';
  return 'unknown';
}

export class MongoStore {
  constructor({ uri, dbName }) {
    this.client = new MongoClient(uri, { serverSelectionTimeoutMS: 10_000, appName: 'boldinbeauty-accounts' });
    this.dbName = dbName;
  }

  async init() {
    await this.client.connect();
    const db = this.client.db(this.dbName);
    this.accounts = db.collection('accounts');
    this.admins = db.collection('admins');
    this.challenges = db.collection('challenges');
    this.sessions = db.collection('sessions');
    this.saves = db.collection('saves');
    this.counters = db.collection('counters');
    this.rate = db.collection('rate');
    this.profiles = db.collection('profiles');
    this.friends = db.collection('friends');
    this.gangs = db.collection('gangs');
    this.gangMembers = db.collection('gang_members');
    this.gangPending = db.collection('gang_pending');
    this.mail = db.collection('mail');
    this.mailAcks = db.collection('mail_acks');
    this.notices = db.collection('notices');
    this.arena = db.collection('arena');
    this.arenaMatches = db.collection('arena_matches');
    await Promise.all([
      this.accounts.createIndex({ emailLower: 1 }, { unique: true }),
      this.accounts.createIndex({ usernameLower: 1 }, { unique: true }),
      this.challenges.createIndex({ expiresAt: 1 }, { expireAfterSeconds: 0 }),
      this.sessions.createIndex({ expiresAt: 1 }, { expireAfterSeconds: 0 }),
      this.sessions.createIndex({ uid: 1 }),
      this.rate.createIndex({ expiresAt: 1 }, { expireAfterSeconds: 0 }),
      this.friends.createIndex({ a: 1 }),
      this.friends.createIndex({ b: 1 }),
      this.gangs.createIndex({ nameLower: 1 }, { unique: true }),
      this.gangs.createIndex({ season: 1, seasonPoints: -1 }),
      this.gangMembers.createIndex({ gangId: 1 }),
      this.mail.createIndex({ to: 1, createdAt: 1 }),
      this.mailAcks.createIndex({ uid: 1 }),
      this.notices.createIndex({ pinned: -1, createdAt: -1 }),
      this.arena.createIndex({ hasDefence: 1, power: 1 }),
      this.arenaMatches.createIndex({ purgeAt: 1 }, { expireAfterSeconds: 0 }),
    ]);
  }

  async close() {
    await this.client.close();
  }

  // ------------------------------------------------------------------ accounts
  async findAccountByEmail(emailLower) {
    return this.accounts.findOne({ emailLower });
  }

  async findAccountByUsername(usernameLower) {
    return this.accounts.findOne({ usernameLower });
  }

  async findAccountByUid(uid) {
    return this.accounts.findOne({ _id: uid });
  }

  async findAccountsByUids(uids) {
    return this.accounts.find({ _id: { $in: uids } }).toArray();
  }

  async nextUid() {
    const doc = await this.counters.findOneAndUpdate(
      { _id: 'uid' },
      { $inc: { seq: 1 }, $setOnInsert: { _id: 'uid' } },
      { upsert: true, returnDocument: 'after' },
    );
    // The first upsert creates { seq: 1 }; offset so the first id is FIRST_UID + 1.
    return FIRST_UID + doc.seq;
  }

  async createAccount(account) {
    try {
      await this.accounts.insertOne({ ...account, _id: account.uid });
    } catch (error) {
      const field = duplicateField(error);
      if (field) throw new StoreConflict(field);
      throw error;
    }
  }

  async updateAccount(uid, patch) {
    await this.accounts.updateOne({ _id: uid }, { $set: patch });
  }

  async upsertAdmin(uid, email, now) {
    await this.admins.updateOne(
      { _id: uid },
      { $set: { uid, email, lastSeenAt: now }, $setOnInsert: { flaggedAt: now } },
      { upsert: true },
    );
  }

  async removeAdmin(uid) {
    await this.admins.deleteOne({ _id: uid });
  }

  // ---------------------------------------------------------------- challenges
  async putChallenge(challenge) {
    const { emailLower, ...rest } = challenge;
    await this.challenges.replaceOne({ _id: emailLower }, { ...rest, emailLower, attempts: 0 }, { upsert: true });
  }

  async getChallenge(emailLower) {
    return this.challenges.findOne({ _id: emailLower });
  }

  async bumpAttempts(emailLower) {
    const doc = await this.challenges.findOneAndUpdate({ _id: emailLower }, { $inc: { attempts: 1 } }, { returnDocument: 'after' });
    return doc ? doc.attempts : null;
  }

  async takeChallenge(emailLower) {
    return this.challenges.findOneAndDelete({ _id: emailLower });
  }

  async deleteChallenge(emailLower) {
    await this.challenges.deleteOne({ _id: emailLower });
  }

  // ------------------------------------------------------------------ sessions
  async createSession(session) {
    const { tokenHash, ...rest } = session;
    await this.sessions.insertOne({ _id: tokenHash, ...rest });
  }

  async getSession(tokenHash) {
    const doc = await this.sessions.findOne({ _id: tokenHash });
    return doc ? { tokenHash: doc._id, uid: doc.uid, createdAt: doc.createdAt, expiresAt: doc.expiresAt } : null;
  }

  async deleteSession(tokenHash) {
    const result = await this.sessions.deleteOne({ _id: tokenHash });
    return result.deletedCount > 0;
  }

  async deleteSessionsForUid(uid) {
    const result = await this.sessions.deleteMany({ uid });
    return result.deletedCount;
  }

  // --------------------------------------------------------------------- saves
  async getSave(uid) {
    const doc = await this.saves.findOne({ _id: uid });
    return doc ? { data: doc.data, savedAt: doc.savedAt, updatedAt: doc.updatedAt } : null;
  }

  async putSave(uid, data, savedAt, now) {
    try {
      // Matches only when nothing is stored yet or the stored save is not newer. If a newer one exists the filter misses
      // and the upsert collides on _id: that is the "stale" answer.
      await this.saves.updateOne({ _id: uid, savedAt: { $lte: savedAt } }, { $set: { data, savedAt, updatedAt: now } }, { upsert: true });
      return { ok: true };
    } catch (error) {
      if (error && error.code === 11000) {
        const current = await this.saves.findOne({ _id: uid });
        return { stale: true, savedAt: current ? current.savedAt : 0 };
      }
      throw error;
    }
  }

  // ---------------------------------------------------------------- rate limits
  async hit(key, windowSeconds, now) {
    const expiresAt = new Date(now.getTime() + windowSeconds * 1000);
    // Count inside the current window...
    const inWindow = await this.rate.findOneAndUpdate({ _id: key, expiresAt: { $gt: now } }, { $inc: { count: 1 } }, { returnDocument: 'after' });
    if (inWindow) return inWindow.count;
    // ...or start a new window (also replaces a document the TTL monitor has not removed yet).
    const fresh = await this.rate.findOneAndUpdate({ _id: key }, { $set: { count: 1, expiresAt } }, { upsert: true, returnDocument: 'after' });
    return fresh.count;
  }

  // ------------------------------------------------------------------ profiles
  async putProfile(uid, profile) {
    await this.profiles.replaceOne({ _id: uid }, { ...profile }, { upsert: true });
  }

  async getProfiles(uids) {
    return (await this.profiles.find({ _id: { $in: uids } }).toArray()).map(plain);
  }

  // ------------------------------------------------------------------- friends
  async getFriendLink(x, y) {
    return plain(await this.friends.findOne({ _id: pairKey(x, y) }));
  }

  async listFriendLinks(uid) {
    return (await this.friends.find({ $or: [{ a: uid }, { b: uid }] }).toArray()).map(plain);
  }

  async addFriendRequest({ from, to, createdAt }) {
    try {
      await this.friends.insertOne({
        _id: pairKey(from, to), a: Math.min(from, to), b: Math.max(from, to), from, to, status: 'pending', createdAt, acceptedAt: null,
      });
      return true;
    } catch (error) {
      if (error && error.code === 11000) return false;
      throw error;
    }
  }

  async acceptFriendRequest(from, to, now) {
    const result = await this.friends.updateOne({ _id: pairKey(from, to), status: 'pending', from, to }, { $set: { status: 'friends', acceptedAt: now } });
    return result.modifiedCount === 1;
  }

  async removeFriendLink(x, y, status) {
    const result = await this.friends.deleteOne({ _id: pairKey(x, y), status });
    return result.deletedCount === 1;
  }

  // --------------------------------------------------------------------- gangs
  async createGang(gang) {
    try {
      await this.gangs.insertOne({ ...gang, _id: gang.id });
    } catch (error) {
      if (error && error.code === 11000) throw new StoreConflict('name');
      throw error;
    }
  }

  async deleteGang(id) {
    await this.gangs.deleteOne({ _id: id });
  }

  async getGang(id) {
    return plain(await this.gangs.findOne({ _id: id }));
  }

  async getGangsByIds(ids) {
    return (await this.gangs.find({ _id: { $in: ids } }).toArray()).map(plain);
  }

  async updateGang(id, patch) {
    await this.gangs.updateOne({ _id: id }, { $set: patch });
  }

  async searchGangs({ q, season, limit }) {
    const base = { memberCount: { $gt: 0 } };
    if (q) {
      return (await this.gangs.find({ ...base, nameLower: { $regex: escapeRegex(q) } }).sort({ memberCount: -1, nameLower: 1 }).limit(limit).toArray()).map(plain);
    }
    const current = await this.gangs.find({ ...base, season }).sort({ seasonPoints: -1, memberCount: -1, xp: -1 }).limit(limit).toArray();
    const rest = current.length < limit
      ? await this.gangs.find({ ...base, season: { $ne: season } }).sort({ memberCount: -1, xp: -1 }).limit(limit - current.length).toArray()
      : [];
    return [...current, ...rest].map(plain);
  }

  async addGangPoints(id, points, now) {
    await this.gangs.updateOne({ _id: id }, { $inc: { xp: points, seasonPoints: points }, $set: { lastActiveAt: now } });
  }

  async rollGangSeason(id, from, to) {
    // One pipeline stage: every expression sees the document before the update, so lastSeasonPoints gets the old points.
    const result = await this.gangs.updateOne({ _id: id, season: from }, [
      { $set: { lastSeason: { $literal: from }, lastSeasonPoints: '$seasonPoints', season: { $literal: to }, seasonPoints: 0 } },
    ]);
    return result.modifiedCount === 1;
  }

  async addGangMember(gangId, member, { max, requireOpen }) {
    // Reserve a seat first (atomic: memberCount never passes max), then insert the membership (unique per player).
    const seat = await this.gangs.findOneAndUpdate(
      { _id: gangId, memberCount: { $lt: max }, ...(requireOpen ? { open: true } : {}) },
      { $inc: { memberCount: 1 } },
    );
    if (!seat) {
      const gang = await this.gangs.findOne({ _id: gangId });
      if (!gang) return 'missing';
      if (requireOpen && !gang.open) return 'closed';
      return 'full';
    }
    try {
      await this.gangMembers.insertOne({ ...member, _id: member.uid });
      return 'ok';
    } catch (error) {
      await this.gangs.updateOne({ _id: gangId }, { $inc: { memberCount: -1 } });
      if (error && error.code === 11000) return 'in_gang';
      throw error;
    }
  }

  async removeGangMember(gangId, uid) {
    const deleted = await this.gangMembers.deleteOne({ _id: uid, gangId });
    if (deleted.deletedCount !== 1) {
      const gang = await this.gangs.findOne({ _id: gangId });
      return { removed: false, remaining: gang ? gang.memberCount : 0 };
    }
    const gang = await this.gangs.findOneAndUpdate({ _id: gangId }, { $inc: { memberCount: -1 } }, { returnDocument: 'after' });
    const remaining = gang ? Math.max(0, gang.memberCount) : 0;
    if (gang && remaining === 0) await this.gangs.deleteOne({ _id: gangId, memberCount: { $lte: 0 } });
    return { removed: true, remaining };
  }

  async getGangMember(uid) {
    return plain(await this.gangMembers.findOne({ _id: uid }));
  }

  async getGangMembersByUids(uids) {
    return (await this.gangMembers.find({ _id: { $in: uids } }).toArray()).map(plain);
  }

  async listGangMembers(gangId) {
    return (await this.gangMembers.find({ gangId }).sort({ joinedAt: 1, _id: 1 }).toArray()).map(plain);
  }

  async updateGangMember(uid, patch) {
    await this.gangMembers.updateOne({ _id: uid }, { $set: patch });
  }

  async settleGangMember(uid, gangId, season) {
    return plain(await this.gangMembers.findOneAndUpdate(
      { _id: uid, gangId, season: { $ne: season } },
      { $set: { season, seasonContribution: 0, milestones: [] } },
      { returnDocument: 'before' },
    ));
  }

  async contributeGangMember(uid, gangId, { day, field, cap, units, points }) {
    const today = (name) => ({ $cond: [{ $eq: ['$day', day] }, `$${name}`, 0] });
    // Stage 1 starts a new day if needed and works out the grant from the values before the update; stage 2 applies it.
    const doc = await this.gangMembers.findOneAndUpdate({ _id: uid, gangId }, [
      {
        $set: {
          day: { $literal: day },
          dayCheckins: today('dayCheckins'),
          dayDonations: today('dayDonations'),
          dayActivity: today('dayActivity'),
          lastGrant: { $max: [0, { $min: [units, { $subtract: [cap, today(field)] }] }] },
        },
      },
      {
        $set: {
          [field]: { $add: [`$${field}`, '$lastGrant'] },
          contribution: { $add: ['$contribution', { $multiply: ['$lastGrant', points] }] },
          seasonContribution: { $add: ['$seasonContribution', { $multiply: ['$lastGrant', points] }] },
        },
      },
    ], { returnDocument: 'after' });
    return doc ? doc.lastGrant : null;
  }

  async claimGangMilestone(uid, gangId, season, index) {
    const result = await this.gangMembers.updateOne({ _id: uid, gangId, season, milestones: { $ne: index } }, { $push: { milestones: index } });
    return result.modifiedCount === 1;
  }

  async addPendingCoins(uid, coins, season) {
    await this.gangPending.updateOne({ _id: uid }, { $inc: { coins }, $set: { season } }, { upsert: true });
  }

  async getPendingCoins(uid) {
    return plain(await this.gangPending.findOne({ _id: uid }));
  }

  async takePendingCoins(uid) {
    return plain(await this.gangPending.findOneAndDelete({ _id: uid }));
  }

  // ---------------------------------------------------------------------- mail
  async createMail(mail) {
    await this.mail.insertOne({ ...mail, _id: mail.id });
  }

  visibleMail(uid, since) {
    return { $or: [{ to: uid }, { to: 'all', createdAt: { $gt: since } }] };
  }

  async listMailFor(uid, since, limit) {
    const acked = (await this.mailAcks.find({ uid }).project({ mailId: 1 }).toArray()).map((a) => a.mailId);
    const docs = await this.mail.find({ ...this.visibleMail(uid, since), _id: { $nin: acked } }).sort({ createdAt: 1, _id: 1 }).limit(limit).toArray();
    return docs.map(plain);
  }

  async ackMail(uid, since, ids, now) {
    const visible = await this.mail.find({ _id: { $in: ids }, ...this.visibleMail(uid, since) }).project({ _id: 1 }).toArray();
    if (visible.length === 0) return;
    await this.mailAcks.bulkWrite(visible.map(({ _id }) => ({
      updateOne: { filter: { _id: `${uid}:${_id}` }, update: { $setOnInsert: { uid, mailId: _id, at: now } }, upsert: true },
    })), { ordered: false });
  }

  // ------------------------------------------------------------------- notices
  async createNotice(notice) {
    await this.notices.insertOne({ ...notice, _id: notice.id });
  }

  async listNotices(limit) {
    return (await this.notices.find({}).sort({ pinned: -1, createdAt: -1, _id: -1 }).limit(limit).toArray()).map(plain);
  }

  async deleteNotice(id) {
    return (await this.notices.deleteOne({ _id: id })).deletedCount === 1;
  }

  // --------------------------------------------------------------------- arena
  async getArena(uid) {
    return plain(await this.arena.findOne({ _id: uid }));
  }

  async createArena(record) {
    await this.arena.updateOne({ _id: record.uid }, { $setOnInsert: { ...record } }, { upsert: true });
    return this.getArena(record.uid);
  }

  async rollArenaSeason(uid, from, patch) {
    return (await this.arena.updateOne({ _id: uid, season: from }, { $set: patch })).modifiedCount === 1;
  }

  async setArenaDefence(uid, patch) {
    await this.arena.updateOne({ _id: uid }, { $set: patch });
  }

  async setArenaRecent(uid, recent) {
    await this.arena.updateOne({ _id: uid }, { $set: { recent } });
  }

  async findArenaCandidates({ exclude, minPower, maxPower, limit }) {
    const filter = { hasDefence: true, _id: { $nin: exclude } };
    if (minPower !== undefined) filter.power = { $gte: minPower, $lte: maxPower };
    return (await this.arena.find(filter).limit(limit).toArray()).map(plain);
  }

  async createArenaMatch(match) {
    await this.arenaMatches.insertOne({ ...match, _id: match.id });
  }

  async getArenaMatch(id) {
    return plain(await this.arenaMatches.findOne({ _id: id }));
  }

  async takeArenaMatch(id, attacker) {
    return (await this.arenaMatches.updateOne({ _id: id, attacker, done: false }, { $set: { done: true } })).modifiedCount === 1;
  }

  async applyArenaResult(uid, { delta, win, defence, logEntry }) {
    const floor = { $cond: ['$champion', CHAMPION_TROPHIES, 0] };
    const next = delta >= 0
      ? { $add: ['$trophies', delta] }
      : { $max: [{ $min: [floor, '$trophies'] }, { $add: ['$trophies', delta] }] };
    const counter = defence ? (win ? 'defenceWins' : 'defenceLosses') : (win ? 'wins' : 'losses');
    const second = { champion: { $or: ['$champion', { $gte: ['$trophies', CHAMPION_TROPHIES] }] } };
    if (logEntry) {
      const entry = { at: { $literal: logEntry.at }, attacker: { $literal: logEntry.attacker }, won: { $literal: logEntry.won }, delta: '$lastDelta' };
      second.log = { $slice: [{ $concatArrays: [{ $ifNull: ['$log', []] }, [entry]] }, -10] };
    }
    return plain(await this.arena.findOneAndUpdate({ _id: uid }, [
      { $set: { lastDelta: { $subtract: [next, '$trophies'] }, trophies: next, [counter]: { $add: [`$${counter}`, 1] } } },
      { $set: second },
    ], { returnDocument: 'after' }));
  }

  async takeArenaReward(uid) {
    const before = await this.arena.findOneAndUpdate({ _id: uid, pendingReward: { $ne: null } }, { $set: { pendingReward: null } }, { returnDocument: 'before' });
    return before ? before.pendingReward : null;
  }
}
