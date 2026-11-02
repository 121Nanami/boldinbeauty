// Shared test setup: a service wired to a store, a recording mailer and a movable clock.
import { loadConfig } from '../src/config.js';
import { MemoryMailer } from '../src/mailer.js';
import { AccountService } from '../src/service.js';
import { SocialService } from '../src/social.js';
import { MemoryStore } from '../src/store/memory.js';
import { MongoStore } from '../src/store/mongo.js';
import crypto from 'node:crypto';

export const OWNER = 'admin@example.com';

// Stores under test: memory always, MongoDB when TEST_MONGODB_URI points at a throw-away server.
export function storeFactories() {
  const factories = [{ label: 'memory store', make: async () => { const store = new MemoryStore(); await store.init(); return store; } }];
  if (process.env.TEST_MONGODB_URI) {
    factories.push({
      label: 'mongodb store',
      make: async () => {
        const store = new MongoStore({ uri: process.env.TEST_MONGODB_URI, dbName: `bib_test_${crypto.randomBytes(6).toString('hex')}` });
        await store.init();
        const close = store.close.bind(store);
        store.close = async () => { await store.client.db(store.dbName).dropDatabase(); await close(); };
        return store;
      },
    });
  }
  return factories;
}

export async function makeHarness(makeStore, env = {}) {
  const store = await makeStore();
  const mailer = new MemoryMailer();
  const config = loadConfig({ ALLOW_MEMORY_STORE: '1', MAIL_PROVIDER: 'console', ...env });
  const clock = { time: new Date() };
  const service = new AccountService({ store, mailer, config, now: () => new Date(clock.time), log: () => {} });
  const social = new SocialService({ store, config, now: () => new Date(clock.time), log: () => {} });
  return {
    store, mailer, config, service, social, clock,
    advance(seconds) { clock.time = new Date(clock.time.getTime() + seconds * 1000); },
    close: () => store.close(),
  };
}

// Signs an account up end to end and returns the session reply.
export async function signUp(h, username, email) {
  await h.service.requestSignup({ username, email });
  const code = h.mailer.lastCodeFor(email.trim().toLowerCase());
  const reply = await h.service.verify({ email, code });
  h.advance(60); // clear the per-address cool-down so the next request in a test is not "too soon"
  return reply;
}

export async function expectApiError(promise, status, code) {
  try {
    await promise;
  } catch (error) {
    if (error.status === status && error.code === code) return error;
    throw new Error(`expected ${status} ${code}, got ${error.status} ${error.code} (${error.message})`);
  }
  throw new Error(`expected ${status} ${code}, but the call succeeded`);
}
