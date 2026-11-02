// The account rules, independent of HTTP: sign-up and log-in by e-mailed code, sessions, the owner/admin flag, cloud save.
//
//   sign up : username + e-mail  -> code e-mailed -> verify -> account created (admin flag if the e-mail is an admin e-mail)
//   log in  : e-mail             -> code e-mailed -> verify -> session
//
// Nothing is stored in plain: codes are kept as HMACs, session tokens as SHA-256 hashes. A code works once, for a few
// minutes, with a handful of tries. Log-in answers the same whether or not the e-mail has an account.
import { StoreConflict } from './store/errors.js';
import { codeEmail } from './mailer.js';
import {
  ApiError, accountView, hmacHex, maskEmail, normalizeEmail, randomCode, randomToken, safeEqualHex, sha256Hex, validateUsername,
} from './util.js';

const SAVE_FORMAT = 'boldinbeauty-save';

export class AccountService {
  constructor({ store, mailer, config, log = () => {}, now = () => new Date() }) {
    this.store = store;
    this.mailer = mailer;
    this.config = config;
    this.log = log;
    this.now = now;
  }

  // ------------------------------------------------------------------ helpers
  codeHash(type, emailLower, code) {
    return hmacHex(this.config.codeSecret, `${type}:${emailLower}:${code}`);
  }

  isAdminEmail(emailLower) {
    return this.config.adminEmails.has(emailLower);
  }

  async limit(key, windowSeconds, max) {
    const count = await this.store.hit(key, windowSeconds, this.now());
    if (count > max) throw new ApiError(429, 'rate_limited', { retryAfterSeconds: windowSeconds });
  }

  // One cool-down and one hourly cap per e-mail address, whether or not the address has an account.
  async limitSends(emailLower) {
    const cool = await this.store.hit(`cool:${emailLower}`, this.config.resendCooldownSeconds, this.now());
    if (cool > 1) throw new ApiError(429, 'too_soon', { retryAfterSeconds: this.config.resendCooldownSeconds });
    await this.limit(`hour:${emailLower}`, 3600, this.config.maxSendsPerHour);
  }

  async issueChallenge({ type, emailLower, username }) {
    const code = randomCode();
    const now = this.now();
    await this.store.putChallenge({
      emailLower,
      type,
      username: username ?? null,
      codeHash: this.codeHash(type, emailLower, code),
      createdAt: now,
      expiresAt: new Date(now.getTime() + this.config.codeMinutes * 60_000),
    });
    return code;
  }

  async sendCode({ type, emailLower, code }) {
    const message = codeEmail({ code, minutes: this.config.codeMinutes, purpose: type });
    await this.mailer.send({ to: emailLower, ...message });
  }

  sentReply() {
    return {
      ok: true,
      expiresInSeconds: this.config.codeMinutes * 60,
      resendAfterSeconds: this.config.resendCooldownSeconds,
    };
  }

  // ------------------------------------------------------------------- sign up
  async requestSignup({ username, email }) {
    const emailLower = normalizeEmail(email);
    if (!emailLower) throw new ApiError(400, 'invalid_email');
    const name = validateUsername(username, { allowReserved: this.isAdminEmail(emailLower) });
    if (!name.ok) throw new ApiError(400, name.code);

    if (await this.store.findAccountByEmail(emailLower)) throw new ApiError(409, 'email_taken');
    if (await this.store.findAccountByUsername(name.username.toLowerCase())) throw new ApiError(409, 'username_taken');

    await this.limitSends(emailLower);
    const code = await this.issueChallenge({ type: 'signup', emailLower, username: name.username });
    try {
      await this.sendCode({ type: 'signup', emailLower, code });
    } catch (error) {
      await this.store.deleteChallenge(emailLower);
      this.log('error', 'sign-up e-mail failed', { to: maskEmail(emailLower), error: String(error.message) });
      throw new ApiError(502, 'mail_failed');
    }
    this.log('info', 'sign-up code sent', { to: maskEmail(emailLower) });
    return this.sentReply();
  }

  // -------------------------------------------------------------------- log in
  async requestLogin({ email }) {
    const emailLower = normalizeEmail(email);
    if (!emailLower) throw new ApiError(400, 'invalid_email');
    await this.limitSends(emailLower);
    const account = await this.store.findAccountByEmail(emailLower);
    if (account) {
      const code = await this.issueChallenge({ type: 'login', emailLower });
      try {
        await this.sendCode({ type: 'login', emailLower, code });
        this.log('info', 'log-in code sent', { to: maskEmail(emailLower) });
      } catch (error) {
        // The reply stays the same either way so that it does not reveal whether the e-mail has an account.
        await this.store.deleteChallenge(emailLower);
        this.log('error', 'log-in e-mail failed', { to: maskEmail(emailLower), error: String(error.message) });
      }
    }
    return this.sentReply();
  }

  // ------------------------------------------------------------------- verify
  async verify({ email, code }) {
    const emailLower = normalizeEmail(email);
    if (!emailLower) throw new ApiError(400, 'invalid_email');
    const digits = typeof code === 'string' || typeof code === 'number' ? String(code).trim() : '';
    if (!/^\d{6}$/.test(digits)) throw new ApiError(400, 'invalid_code');

    await this.limit(`verify:${emailLower}`, 600, 20);
    const challenge = await this.store.getChallenge(emailLower);
    if (!challenge || new Date(challenge.expiresAt) <= this.now()) throw new ApiError(400, 'invalid_code');

    const attempts = await this.store.bumpAttempts(emailLower);
    if (attempts === null) throw new ApiError(400, 'invalid_code');
    if (attempts > this.config.maxCodeAttempts) {
      await this.store.deleteChallenge(emailLower);
      throw new ApiError(429, 'too_many_attempts');
    }
    if (!safeEqualHex(this.codeHash(challenge.type, emailLower, digits), challenge.codeHash)) {
      const attemptsLeft = Math.max(0, this.config.maxCodeAttempts - attempts);
      if (attemptsLeft === 0) await this.store.deleteChallenge(emailLower);
      this.log('warn', 'wrong code', { to: maskEmail(emailLower), attemptsLeft });
      throw new ApiError(400, 'invalid_code', { attemptsLeft });
    }
    // Correct. Taking the challenge is atomic: of two simultaneous correct requests only one gets it.
    const taken = await this.store.takeChallenge(emailLower);
    if (!taken) throw new ApiError(400, 'invalid_code');

    const account = taken.type === 'signup' ? await this.createAccount(taken, emailLower) : await this.loginAccount(emailLower);
    return this.startSession(account);
  }

  async createAccount(challenge, emailLower) {
    const now = this.now();
    const usernameLower = challenge.username.toLowerCase();
    if (await this.store.findAccountByEmail(emailLower)) throw new ApiError(409, 'email_taken');
    if (await this.store.findAccountByUsername(usernameLower)) throw new ApiError(409, 'username_taken');
    const admin = this.isAdminEmail(emailLower);
    const account = {
      uid: await this.store.nextUid(),
      username: challenge.username,
      usernameLower,
      email: emailLower,
      emailLower,
      role: admin ? 'admin' : 'player',
      createdAt: now,
      emailVerifiedAt: now,
      lastLoginAt: now,
    };
    try {
      await this.store.createAccount(account);
    } catch (error) {
      if (error instanceof StoreConflict) throw new ApiError(409, error.field === 'username' ? 'username_taken' : 'email_taken');
      throw error;
    }
    if (admin) await this.store.upsertAdmin(account.uid, emailLower, now);
    this.log('info', 'account created', { uid: account.uid, admin });
    return account;
  }

  async loginAccount(emailLower) {
    const account = await this.store.findAccountByEmail(emailLower);
    if (!account) throw new ApiError(400, 'invalid_code');
    const now = this.now();
    // The server config decides who is an admin, every time: adding or removing an e-mail in ADMIN_EMAILS takes effect at the next log-in.
    const role = this.isAdminEmail(emailLower) ? 'admin' : 'player';
    await this.store.updateAccount(account.uid, { lastLoginAt: now, role });
    if (role === 'admin') await this.store.upsertAdmin(account.uid, emailLower, now);
    else if (account.role === 'admin') await this.store.removeAdmin(account.uid);
    this.log('info', 'log-in', { uid: account.uid, admin: role === 'admin' });
    return { ...account, role, lastLoginAt: now };
  }

  async startSession(account) {
    const token = randomToken();
    const now = this.now();
    const expiresAt = new Date(now.getTime() + this.config.sessionDays * 86_400_000);
    await this.store.createSession({ tokenHash: sha256Hex(token), uid: account.uid, createdAt: now, expiresAt });
    return { ok: true, token, expiresAt: expiresAt.toISOString(), account: accountView(account) };
  }

  // ------------------------------------------------------------------ sessions
  async authenticate(token) {
    if (typeof token !== 'string' || !token.startsWith('bib_') || token.length > 100) throw new ApiError(401, 'unauthorized');
    const session = await this.store.getSession(sha256Hex(token));
    if (!session || new Date(session.expiresAt) <= this.now()) throw new ApiError(401, 'unauthorized');
    const account = await this.store.findAccountByUid(session.uid);
    if (!account) throw new ApiError(401, 'unauthorized');
    return account;
  }

  async me(account) {
    return { ok: true, account: accountView(account) };
  }

  async logout(token) {
    if (typeof token === 'string' && token.startsWith('bib_') && token.length <= 100) await this.store.deleteSession(sha256Hex(token));
    return { ok: true };
  }

  // ---------------------------------------------------------------- cloud save
  async getSave(account) {
    const save = await this.store.getSave(account.uid);
    return save ? { ok: true, save: save.data, savedAt: save.savedAt } : { ok: true, save: null, savedAt: 0 };
  }

  async putSave(account, body) {
    const text = body && body.save;
    if (typeof text !== 'string' || text.length === 0) throw new ApiError(400, 'invalid_save');
    if (Buffer.byteLength(text, 'utf8') > this.config.maxSaveBytes) throw new ApiError(413, 'save_too_large');
    let parsed;
    try {
      parsed = JSON.parse(text);
    } catch {
      throw new ApiError(400, 'invalid_save');
    }
    if (!parsed || typeof parsed !== 'object' || parsed.format !== SAVE_FORMAT || !parsed.profile || typeof parsed.profile !== 'object') {
      throw new ApiError(400, 'invalid_save');
    }
    const savedAt = parsed.savedAt;
    const now = this.now();
    if (!Number.isInteger(savedAt) || savedAt <= 0 || savedAt > Math.floor(now.getTime() / 1000) + 86_400) throw new ApiError(400, 'invalid_save');
    // A save only ever goes into the account it was written for.
    if (String(parsed.userId) !== String(account.uid)) throw new ApiError(400, 'wrong_account');

    const result = await this.store.putSave(account.uid, text, savedAt, now);
    if (result.stale) throw new ApiError(409, 'stale_save', { savedAt: result.savedAt });
    return { ok: true, savedAt };
  }
}
