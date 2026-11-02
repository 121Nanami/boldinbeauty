// Reads and validates the environment. Fails early with a readable list of what is missing.
import crypto from 'node:crypto';
import { normalizeEmail } from './util.js';

export class ConfigError extends Error {
  constructor(problems) {
    super(`Configuration problems:\n - ${problems.join('\n - ')}`);
    this.problems = problems;
  }
}

function int(env, key, fallback, problems, { min = 0, max = Number.MAX_SAFE_INTEGER } = {}) {
  if (env[key] === undefined || env[key] === '') return fallback;
  const value = Number(env[key]);
  if (!Number.isInteger(value) || value < min || value > max) {
    problems.push(`${key} must be an integer between ${min} and ${max}`);
    return fallback;
  }
  return value;
}

export function loadConfig(env = process.env) {
  const problems = [];
  const production = env.NODE_ENV === 'production';
  const memoryStore = env.ALLOW_MEMORY_STORE === '1';

  if (!env.MONGODB_URI && !memoryStore) {
    problems.push('MONGODB_URI is not set (ALLOW_MEMORY_STORE=1 is for local development only: nothing is kept)');
  }
  if (env.MONGODB_URI && memoryStore) {
    problems.push('Set either MONGODB_URI or ALLOW_MEMORY_STORE=1, not both');
  }

  let codeSecret = env.CODE_SECRET ?? '';
  if (!codeSecret && memoryStore) {
    codeSecret = crypto.randomBytes(32).toString('hex'); // development: codes die with the process anyway
  }
  if (codeSecret.length < 32) {
    problems.push('CODE_SECRET must be at least 32 characters (generate one with: openssl rand -hex 32)');
  }

  const mailProvider = (env.MAIL_PROVIDER ?? (memoryStore ? 'console' : '')).toLowerCase();
  if (!['smtp', 'resend', 'console'].includes(mailProvider)) {
    problems.push('MAIL_PROVIDER must be smtp, resend or console');
  }
  if (mailProvider === 'console' && production && env.ALLOW_CONSOLE_MAIL !== '1') {
    problems.push('MAIL_PROVIDER=console prints codes to the log; it is refused when NODE_ENV=production');
  }
  if (mailProvider !== 'console' && !env.MAIL_FROM) problems.push('MAIL_FROM is required (the sender address)');
  if (mailProvider === 'smtp' && (!env.SMTP_HOST || !env.SMTP_USER || !env.SMTP_PASS)) {
    problems.push('SMTP_HOST, SMTP_USER and SMTP_PASS are required for MAIL_PROVIDER=smtp');
  }
  if (mailProvider === 'resend' && !env.RESEND_API_KEY) problems.push('RESEND_API_KEY is required for MAIL_PROVIDER=resend');

  const adminEmails = new Set();
  for (const raw of (env.ADMIN_EMAILS ?? 'admin@example.com').split(',')) {
    if (!raw.trim()) continue;
    const email = normalizeEmail(raw);
    if (email) adminEmails.add(email);
    else problems.push(`ADMIN_EMAILS contains an invalid address: ${raw.trim()}`);
  }

  const config = {
    production,
    port: int(env, 'PORT', 8080, problems, { min: 0, max: 65535 }),
    mongoUri: env.MONGODB_URI || '',
    mongoDb: env.MONGODB_DB || 'boldinbeauty',
    memoryStore,
    codeSecret,
    adminEmails,
    mail: {
      provider: mailProvider,
      from: env.MAIL_FROM || 'Boldinbeauty <no-reply@localhost>',
      smtp: {
        host: env.SMTP_HOST || '',
        port: int(env, 'SMTP_PORT', 587, problems, { min: 1, max: 65535 }),
        secure: env.SMTP_SECURE === '1' || env.SMTP_SECURE === 'true',
        user: env.SMTP_USER || '',
        pass: env.SMTP_PASS || '',
      },
      resendKey: env.RESEND_API_KEY || '',
    },
    codeMinutes: int(env, 'CODE_MINUTES', 10, problems, { min: 1, max: 60 }),
    sessionDays: int(env, 'SESSION_DAYS', 30, problems, { min: 1, max: 365 }),
    maxCodeAttempts: 5,
    resendCooldownSeconds: 45,
    maxSendsPerHour: 6,
    ipRateLimitPerMinute: int(env, 'IP_RATE_LIMIT_PER_MINUTE', 120, problems, { min: 5, max: 100000 }),
    trustProxyHops: int(env, 'TRUST_PROXY_HOPS', 0, problems, { min: 0, max: 5 }),
    maxSaveBytes: 1_000_000,
  };
  if (problems.length) throw new ConfigError(problems);
  return Object.freeze(config);
}

// What is safe to print at start-up: no secrets.
export function describeConfig(config) {
  return {
    port: config.port,
    store: config.memoryStore ? 'memory (development only)' : `mongodb database "${config.mongoDb}"`,
    mailProvider: config.mail.provider,
    adminEmails: config.adminEmails.size,
    codeMinutes: config.codeMinutes,
    sessionDays: config.sessionDays,
    trustProxyHops: config.trustProxyHops,
  };
}
