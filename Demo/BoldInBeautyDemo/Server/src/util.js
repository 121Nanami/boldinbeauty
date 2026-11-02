// Small pure helpers: validation, hashing, random codes. No I/O.
import crypto from 'node:crypto';

export class ApiError extends Error {
  constructor(status, code, extra = {}) {
    super(code);
    this.status = status;
    this.code = code;
    this.extra = extra;
  }
}

// Names that only the owner/admin account may use, so nobody can pose as staff.
const RESERVED_USERNAMES = new Set([
  'admin', 'administrator', 'root', 'system', 'support', 'staff', 'moderator', 'mod', 'owner', 'official',
  'boldinbeauty', 'greyhat', 'commander', 'server', 'null', 'undefined',
]);

export function normalizeEmail(raw) {
  if (typeof raw !== 'string') return null;
  const email = raw.trim().toLowerCase();
  if (email.length < 6 || email.length > 254) return null;
  const match = /^([a-z0-9.!#$%&'*+/=?^_`{|}~-]{1,64})@((?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?)$/.exec(email);
  if (!match) return null;
  const local = match[1];
  if (local.startsWith('.') || local.endsWith('.') || local.includes('..')) return null;
  if (!/\.[a-z0-9-]{2,}$/.test(match[2])) return null;
  return email;
}

// 3-20 characters, starts with a letter, then letters, digits or underscore.
export function validateUsername(raw, { allowReserved = false } = {}) {
  if (typeof raw !== 'string') return { ok: false, code: 'invalid_username' };
  const username = raw.trim();
  if (!/^[A-Za-z][A-Za-z0-9_]{2,19}$/.test(username)) return { ok: false, code: 'invalid_username' };
  if (!allowReserved && RESERVED_USERNAMES.has(username.toLowerCase())) return { ok: false, code: 'reserved_username' };
  return { ok: true, username };
}

export function sha256Hex(text) {
  return crypto.createHash('sha256').update(text).digest('hex');
}

export function hmacHex(secret, text) {
  return crypto.createHmac('sha256', secret).update(text).digest('hex');
}

export function safeEqualHex(a, b) {
  if (typeof a !== 'string' || typeof b !== 'string' || a.length !== b.length || a.length % 2 !== 0) return false;
  if (!/^[0-9a-f]+$/.test(a) || !/^[0-9a-f]+$/.test(b)) return false;
  return crypto.timingSafeEqual(Buffer.from(a, 'hex'), Buffer.from(b, 'hex'));
}

export function randomCode() {
  return String(crypto.randomInt(0, 1_000_000)).padStart(6, '0');
}

export function randomToken() {
  return `bib_${crypto.randomBytes(32).toString('base64url')}`;
}

export function maskEmail(email) {
  if (typeof email !== 'string' || !email.includes('@')) return '***';
  const [local, domain] = email.split('@');
  return `${local.slice(0, 1)}***@${domain}`;
}

export function accountView(account) {
  return {
    userId: String(account.uid),
    username: account.username,
    email: account.email,
    isAdmin: account.role === 'admin',
    createdAt: account.createdAt instanceof Date ? account.createdAt.toISOString() : account.createdAt,
  };
}
