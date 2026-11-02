// The HTTP layer: JSON in, JSON out. No cookies, no CORS (the game is a native client), small body limits, rate limits per
// client address, nothing user-supplied echoed back in an error.
//
//   GET  /health
//   POST /v1/auth/signup   { username, email }
//   POST /v1/auth/login    { email }
//   POST /v1/auth/verify   { email, code }          -> { token, account }
//   POST /v1/auth/logout                            (Bearer token)
//   GET  /v1/me                                     (Bearer token)
//   GET  /v1/save                                   (Bearer token)
//   PUT  /v1/save          { save }                 (Bearer token)
//
// Online routes (all Bearer token; contract in docs/online-api.md):
//   PUT  /v1/profile                       GET  /v1/friends    POST /v1/friends/{request,accept,decline,remove}
//   GET  /v1/gangs?q=   GET /v1/gangs/mine   POST /v1/gangs    POST /v1/gangs/{join,leave,kick,settings,contribute,claim,milestone}
//   GET  /v1/mail       POST /v1/mail/ack    POST /v1/admin/mail
//   GET  /v1/notices    POST /v1/admin/notices                 POST /v1/admin/notices/delete
//   PUT  /v1/arena/defence  GET /v1/arena/me  POST /v1/arena/{match,result,claim}
import http from 'node:http';
import { SocialService } from './social.js';
import { ApiError } from './util.js';

const SMALL_BODY = 4 * 1024;
const TEAM_BODY = 16 * 1024;

// Online routes: method + path -> [handler name, body limit or 0 for no body].
const SOCIAL_ROUTES = {
  'PUT /v1/profile': ['putProfile', TEAM_BODY],
  'GET /v1/friends': ['listFriends', 0],
  'POST /v1/friends/request': ['requestFriend', SMALL_BODY],
  'POST /v1/friends/accept': ['acceptFriend', SMALL_BODY],
  'POST /v1/friends/decline': ['declineFriend', SMALL_BODY],
  'POST /v1/friends/remove': ['removeFriend', SMALL_BODY],
  'GET /v1/gangs/mine': ['myGang', 0],
  'POST /v1/gangs': ['createGang', SMALL_BODY],
  'POST /v1/gangs/join': ['joinGang', SMALL_BODY],
  'POST /v1/gangs/leave': ['leaveGang', SMALL_BODY],
  'POST /v1/gangs/kick': ['kickMember', SMALL_BODY],
  'POST /v1/gangs/settings': ['gangSettings', SMALL_BODY],
  'POST /v1/gangs/contribute': ['contribute', SMALL_BODY],
  'POST /v1/gangs/claim': ['claimGangCoins', SMALL_BODY],
  'POST /v1/gangs/milestone': ['claimMilestone', SMALL_BODY],
  'GET /v1/mail': ['listMail', 0],
  'POST /v1/mail/ack': ['ackMail', SMALL_BODY],
  'POST /v1/admin/mail': ['sendMail', SMALL_BODY],
  'GET /v1/notices': ['listNotices', 0],
  'POST /v1/admin/notices': ['createNotice', SMALL_BODY],
  'POST /v1/admin/notices/delete': ['deleteNotice', SMALL_BODY],
  'PUT /v1/arena/defence': ['putDefence', TEAM_BODY],
  'GET /v1/arena/me': ['arenaMe', 0],
  'POST /v1/arena/match': ['arenaMatch', SMALL_BODY],
  'POST /v1/arena/result': ['arenaResult', SMALL_BODY],
  'POST /v1/arena/claim': ['arenaClaim', SMALL_BODY],
};
const KNOWN_PATHS = new Set([
  '/health', '/v1/auth/signup', '/v1/auth/login', '/v1/auth/verify', '/v1/auth/logout', '/v1/me', '/v1/save',
  '/v1/gangs', ...Object.keys(SOCIAL_ROUTES).map((route) => route.split(' ')[1]),
]);

function clientIp(req, hops) {
  const direct = req.socket.remoteAddress || 'unknown';
  if (hops <= 0) return direct;
  const forwarded = String(req.headers['x-forwarded-for'] || '').split(',').map((part) => part.trim()).filter(Boolean);
  // The proxies we trust append the address they saw; the entry `hops` from the right is the real client.
  return forwarded.length >= hops ? forwarded[forwarded.length - hops] : direct;
}

function readJson(req, maxBytes) {
  return new Promise((resolve, reject) => {
    const type = String(req.headers['content-type'] || '');
    if (!type.toLowerCase().startsWith('application/json')) {
      reject(new ApiError(415, 'unsupported_media_type'));
      return;
    }
    const chunks = [];
    let size = 0;
    let finished = false;
    req.on('data', (chunk) => {
      if (finished) return;
      size += chunk.length;
      if (size > maxBytes) {
        finished = true;
        reject(new ApiError(413, 'body_too_large'));
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => {
      if (finished) return;
      finished = true;
      try {
        const value = JSON.parse(Buffer.concat(chunks).toString('utf8'));
        if (value === null || typeof value !== 'object' || Array.isArray(value)) throw new Error('not an object');
        resolve(value);
      } catch {
        reject(new ApiError(400, 'invalid_json'));
      }
    });
    req.on('error', () => {
      if (!finished) {
        finished = true;
        reject(new ApiError(400, 'invalid_request'));
      }
    });
  });
}

function bearer(req) {
  const header = String(req.headers.authorization || '');
  return header.startsWith('Bearer ') ? header.slice(7).trim() : '';
}

export function createHttpServer({ service, social, config, log }) {
  // Older callers pass only the account service: build the online service on the same store and clock.
  const online = social ?? new SocialService({ store: service.store, config, now: service.now, log });
  const server = http.createServer(async (req, res) => {
    const send = (status, body) => {
      const payload = JSON.stringify(body);
      res.writeHead(status, {
        'Content-Type': 'application/json; charset=utf-8',
        'Content-Length': Buffer.byteLength(payload),
        'Cache-Control': 'no-store',
        'X-Content-Type-Options': 'nosniff',
        ...(body.error === 'rate_limited' || body.error === 'too_soon' ? { 'Retry-After': String(body.retryAfterSeconds ?? 60) } : {}),
        ...(status === 413 || status === 415 ? { Connection: 'close' } : {}),
      });
      res.end(payload);
    };

    try {
      const url = new URL(req.url || '/', 'http://localhost');
      const route = `${req.method} ${url.pathname}`;
      if (route === 'GET /health') {
        send(200, { ok: true });
        return;
      }
      await service.limit(`ip:${clientIp(req, config.trustProxyHops)}`, 60, config.ipRateLimitPerMinute);

      switch (route) {
        case 'POST /v1/auth/signup':
          send(200, await service.requestSignup(await readJson(req, SMALL_BODY)));
          return;
        case 'POST /v1/auth/login':
          send(200, await service.requestLogin(await readJson(req, SMALL_BODY)));
          return;
        case 'POST /v1/auth/verify':
          send(200, await service.verify(await readJson(req, SMALL_BODY)));
          return;
        case 'POST /v1/auth/logout':
          send(200, await service.logout(bearer(req)));
          return;
        case 'GET /v1/me':
          send(200, await service.me(await service.authenticate(bearer(req))));
          return;
        case 'GET /v1/save':
          send(200, await service.getSave(await service.authenticate(bearer(req))));
          return;
        case 'PUT /v1/save': {
          const account = await service.authenticate(bearer(req));
          send(200, await service.putSave(account, await readJson(req, config.maxSaveBytes + 4096)));
          return;
        }
        case 'GET /v1/gangs':
          send(200, await online.searchGangs(await service.authenticate(bearer(req)), url.searchParams.get('q') ?? ''));
          return;
        default: {
          const social = SOCIAL_ROUTES[route];
          if (social) {
            const [handler, maxBytes] = social;
            const account = await service.authenticate(bearer(req));
            // A POST with no body (e.g. leave / claim) is accepted as {}.
            const hasBody = maxBytes > 0 && (Number(req.headers['content-length'] || 0) > 0 || req.headers['transfer-encoding']);
            const body = hasBody ? await readJson(req, maxBytes) : {};
            send(200, await online[handler](account, body));
            return;
          }
          const known = KNOWN_PATHS.has(url.pathname);
          send(known ? 405 : 404, { ok: false, error: known ? 'method_not_allowed' : 'not_found' });
        }
      }
    } catch (error) {
      if (error instanceof ApiError) {
        send(error.status, { ok: false, error: error.code, ...error.extra });
      } else {
        log('error', 'unhandled error', { error: String(error && error.stack ? error.stack : error) });
        send(500, { ok: false, error: 'internal_error' });
      }
    }
  });
  server.requestTimeout = 20_000;
  server.headersTimeout = 10_000;
  server.keepAliveTimeout = 5_000;
  return server;
}
