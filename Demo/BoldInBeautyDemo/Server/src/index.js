// Starts the account service. Configuration comes from environment variables only (see .env.example).
import { ConfigError, describeConfig, loadConfig } from './config.js';
import { createHttpServer } from './http.js';
import { createMailer } from './mailer.js';
import { AccountService } from './service.js';
import { SocialService } from './social.js';
import { MemoryStore } from './store/memory.js';
import { MongoStore } from './store/mongo.js';

function log(level, message, fields = {}) {
  process.stdout.write(`${JSON.stringify({ t: new Date().toISOString(), level, message, ...fields })}\n`);
}

let config;
try {
  config = loadConfig();
} catch (error) {
  if (error instanceof ConfigError) {
    process.stderr.write(`${error.message}\n`);
    process.exit(1);
  }
  throw error;
}

const store = config.memoryStore ? new MemoryStore() : new MongoStore({ uri: config.mongoUri, dbName: config.mongoDb });
await store.init();
const mailer = createMailer(config.mail, log);
if (typeof mailer.verify === 'function') {
  try {
    await mailer.verify();
    log('info', 'mail server reachable');
  } catch (error) {
    log('error', 'mail server check failed: codes will not be delivered until this is fixed', { error: String(error.message) });
  }
}
const service = new AccountService({ store, mailer, config, log });
const social = new SocialService({ store, config, log });
const server = createHttpServer({ service, social, config, log });
server.listen(config.port, () => log('info', 'account service listening', describeConfig(config)));

async function shutdown(signal) {
  log('info', 'shutting down', { signal });
  server.close(async () => {
    await store.close();
    process.exit(0);
  });
  setTimeout(() => process.exit(1), 10_000).unref();
}
process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));
