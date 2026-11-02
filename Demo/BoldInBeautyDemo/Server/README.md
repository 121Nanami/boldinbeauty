# Boldinbeauty account service

Sign-up and log-in for the Boldinbeauty demo, by e-mailed code, plus a cloud copy of each player's save. Small Node service
(no web framework), MongoDB for storage. The game talks to it through `UBIBAccountSubsystem`
(`Source/BoldInBeautyDemo/BIBGame/BIBAccount.cpp`); nothing secret ever lives in the game.

**Status (2026-10-06).** The online routes (friends, gangs, mail, notices, arena; see below) were added and tested on both stores:
118 tests with the in-memory store, 190 against a local `mongod` 7.0.14. **Status (2026-10-05).** Logic, HTTP layer and both stores are tested here: 84 tests with the in-memory store, 127 when
`TEST_MONGODB_URI` points at a real MongoDB (run against `mongod` 7.0.14). The SMTP sender is tested against a local SMTP
server. The guided `npm run setup` was tested with a scripted fake of the Atlas CLI (its commands and flags were checked against
the real CLI 1.58.3's `--help`) and, in a real pseudo-terminal, against a local MongoDB and a local SMTP server (hidden password input,
`.env` mode 600, live database check, SMTP login, test e-mail, then the server started from the generated `.env` and the owner signed up
as admin). **Not yet done:** the same run against *your* Atlas account and Google mailbox, because only you can approve the Atlas login
code and create the App password; and a public host (optional, later).

## Quick start: one command on your Mac (about 5 minutes)

```sh
cd Demo/BoldInBeautyDemo/Server
npm install
npm run setup
```

(Needs Node 20.19 or newer: `brew install node` if you do not have it.) Or just **double-click `setup.command`** in this folder: it opens
Terminal and does the same. The setup walks you through it and does the rest itself:

1. **Database (MongoDB Atlas).** It uses the official Atlas CLI (it offers to install it with Homebrew). A browser tab opens with a
   **one-time code: you type or paste it there and confirm. That is the only Atlas step you do.** The script then picks your
   project, creates a free cluster if you have none (and waits for it), creates a database user with a random password that can
   read and write **only** the game database, allows this computer to connect, builds the connection string, and checks that it works.
   (Prefer to do Atlas yourself? Choose "I already have a connection string" and paste it.)
2. **E-mail.** Choose Google Workspace / Gmail and **paste a 16-letter App password** (the script shows the link and the steps:
   Google Account > Security > App passwords). It checks the login and can send you a test e-mail. Resend, any other SMTP service,
   or "skip" (codes appear in the server window) are also offered.
3. **Saves** a private `.env` (permissions `600`, git-ignored; any existing one is kept as `.env.bak`) and offers to **start the
   server**. Leave that window open while you play; the game already looks for it at `http://127.0.0.1:8080`.

Then start the game and choose **Create account** with `admin@example.com`: the e-mailed code signs you in as the
owner/admin. Nothing you type is echoed, nothing secret is printed, and nothing is sent anywhere except to Atlas and your mailbox.
Run `npm run setup` again at any time to renew the database password or change the sender.

If something fails, the script says what and where to look (for example: a VPN or a new public address needs adding in Atlas >
Network Access) and writes nothing.

## How the player sees it

| Step | What happens |
| --- | --- |
| **Create account** | Player enters a username and an e-mail. The server e-mails a 6-digit code. Entering it creates the account and signs in. |
| **Log in** | Player enters an e-mail. The server e-mails a code. Entering it signs in. (No passwords.) |
| **In the game** | The Profile page shows the username, e-mail and user id (an 8-digit number from 10000001). |
| **Next launch** | The game signs in again from a saved session token; no code needed until it expires (30 days) or the player signs out. |
| **Owner / admin** | The e-mail `admin@example.com` (any letter case) is flagged admin by the **server** (`ADMIN_EMAILS`) and recorded with its user id in the `admins` collection. The game gives that account every operator, unlimited currencies and every level. |

## Run it locally (no database, no e-mail account)

```sh
cd Demo/BoldInBeautyDemo/Server
npm install
npm run dev          # in-memory store, e-mails printed to the log, port 8080
```

The game's default address is `http://127.0.0.1:8080` (`[BoldInBeauty] ApiBaseUrl` in `Config/DefaultGame.ini`, or the launch
argument `-BIBApi=...`). The verification code appears in the server log: look for `verification code is 123456`.

## Production setup

You need three things. Nothing is committed to the repository: they are environment variables on the host.

1. **MongoDB (Atlas).** In Atlas: *Database Access* -> add a database user with the built-in role **readWrite on the database
   `boldinbeauty`** only (not "Atlas admin"). *Network Access* -> allow the address(es) your server runs from (prefer that over
   `0.0.0.0/0`). *Database* -> *Connect* -> *Drivers* -> copy the `mongodb+srv://...` string into `MONGODB_URI`. The service creates
   its collections and indexes on first start.
2. **E-mail.** Either any SMTP service (`MAIL_PROVIDER=smtp`, `SMTP_HOST`, `SMTP_PORT`, `SMTP_USER`, `SMTP_PASS`) or Resend
   (`MAIL_PROVIDER=resend`, `RESEND_API_KEY`), plus `MAIL_FROM`. Most providers require you to verify the sending address or
   domain first. Sign-up codes only reach the player if this works: the service logs `mail server check failed` at start-up when
   SMTP is misconfigured.
3. **A host** that runs a container or Node 20.19+ and gives you an `https://` address (Render, Fly.io, Railway, Cloud Run, a VPS...).
   Build with the `Dockerfile` here, or run `npm ci --omit=dev && node src/index.js`. Set `TRUST_PROXY_HOPS=1` when a load balancer
   sits in front, so rate limits see the player's address and not the balancer's. Health check: `GET /health`.

Required variables are listed in `.env.example`. `CODE_SECRET` is any random string of 32+ characters (`openssl rand -hex 32`);
changing it invalidates codes that are in flight (harmless). The service refuses to start, and says what is missing, if anything
required is absent. It also refuses `MAIL_PROVIDER=console` when `NODE_ENV=production`.

Then set the game's address: `[BoldInBeauty] ApiBaseUrl=https://your-host` in `Config/DefaultGame.ini` (or `-BIBApi=`). The game
warns in its log if the address is plain `http://` and not local.

### Try the real thing on your own Mac first (no hosting needed)

The game and the server can run on the same computer. Put your real settings in a git-ignored `.env`, start the server, and play:

```sh
cd Demo/BoldInBeautyDemo/Server
cp .env.example .env      # then edit: MONGODB_URI, CODE_SECRET, MAIL_*  (the file is git-ignored)
npm install
npm run start:env         # node --env-file=.env src/index.js  (Node 20.6 or newer)
```

Start the game normally (its default address is `http://127.0.0.1:8080`). For a mailbox you already own, any SMTP login works; for
example with Google Workspace/Gmail, turn on 2-step verification, create an *App password*, and use `SMTP_HOST=smtp.gmail.com`,
`SMTP_PORT=587`, `SMTP_SECURE=0`, `SMTP_USER=<the mailbox>`, `SMTP_PASS=<the app password>`, `MAIL_FROM=<the mailbox>`.
The service insists on TLS for the SMTP login (STARTTLS on port 587, or `SMTP_SECURE=1` on 465) unless the SMTP host is localhost.

### What only you can do

- Run `npm run setup` and, when it asks: approve the Atlas login code in your browser, and paste a Google App password (or another
  sender's credentials). Everything else is done by the script.
- Optional, later: put the server on a public host (the setup only prepares this computer; for a host, set the same variables from
  `.env.example` in its dashboard, add the host's address in Atlas > Network Access, and set `ApiBaseUrl` in the game).
- Decide the demo-tools question below.

## API

JSON in, JSON out, `Authorization: Bearer <token>` where marked. Errors are `{ "ok": false, "error": "<code>" }`.

| Method and path | Body | Answer |
| --- | --- | --- |
| `GET /health` | | `{ ok: true }` |
| `POST /v1/auth/signup` | `{ username, email }` | `200` code e-mailed. `400 invalid_username / reserved_username / invalid_email`, `409 username_taken / email_taken`, `429 too_soon / rate_limited`, `502 mail_failed` |
| `POST /v1/auth/login` | `{ email }` | `200` always the same answer, whether or not the address has an account |
| `POST /v1/auth/verify` | `{ email, code }` | `{ token, expiresAt, account: { userId, username, email, isAdmin } }`; `400 invalid_code` (with `attemptsLeft`), `429 too_many_attempts` |
| `POST /v1/auth/logout` | | ends this session |
| `GET /v1/me` (Bearer) | | `{ account }` |
| `GET /v1/save` (Bearer) | | `{ save: "<json text>" or null, savedAt }` |
| `PUT /v1/save` (Bearer) | `{ save }` | `{ savedAt }`; `409 stale_save` if the stored save is newer, `400 wrong_account / invalid_save`, `413` |

### Online routes (friends, gangs, mail, notices, arena)

All need `Authorization: Bearer <token>`; the full contract (bodies, replies, error codes, limits, season rules) is
[`docs/online-api.md`](../docs/online-api.md). Code: `src/social.js`.

| Area | Routes |
| --- | --- |
| Public profile | `PUT /v1/profile` |
| Friends (50 friends, 50 pending each way) | `GET /v1/friends`, `POST /v1/friends/request` / `accept` / `decline` / `remove` |
| Gangs (30 members, monthly season) | `GET /v1/gangs?q=`, `GET /v1/gangs/mine`, `POST /v1/gangs`, `POST /v1/gangs/join` / `leave` / `kick` / `settings` / `contribute` / `claim` / `milestone` |
| Mail | `GET /v1/mail`, `POST /v1/mail/ack`, `POST /v1/admin/mail` (admin) |
| Notices | `GET /v1/notices`, `POST /v1/admin/notices`, `POST /v1/admin/notices/delete` (admin) |
| Arena (monthly season) | `PUT /v1/arena/defence`, `GET /v1/arena/me`, `POST /v1/arena/match` / `result` / `claim` |

Errors are `400 invalid_input` for any malformed field, plus the codes in the contract. Codes the contract leaves open:
`409 not_in_gang`, `409 already_reported` (a second result for one match), `409 match_expired`, `404 not_found` (unknown
gang / notice / match), `403 forbidden` (admin routes). Writes are limited to 60 per minute per player, reads to 120.
Season ends are applied by the first request in a new UTC month (gang: any member's gang request; arena: that player's
request, or when they are matched or scored as a defender), with conditional updates so nothing is paid twice.

## Rules it enforces

- A code is 6 digits, valid 10 minutes, works once, allows 5 wrong tries, and is stored only as an HMAC. One pending code per e-mail.
- One code request per address per 45 s and 6 per hour, **the same for addresses with and without an account**, so the answer never
  reveals which e-mails are registered. 120 requests per minute per client address.
- Session tokens are 256-bit random values, stored only as SHA-256 hashes, valid 30 days, revoked at log-out.
- Usernames: 3-20 characters, letters/digits/underscore, start with a letter, unique ignoring case; names like `admin`, `owner`,
  `support` are reserved (the admin e-mail may use them).
- The admin flag comes only from `ADMIN_EMAILS` and is re-read at every log-in. Nothing a client or a save file says can set it.
- A save is accepted only for the account it was written for (`userId` inside the save must match), at most 1 MB, never dated more
  than a day ahead. The newest `savedAt` wins.
- E-mail addresses are masked in the log; codes are never logged (except by the development `console` mailer).

## Honest limits

- A save is a plain JSON file. The service checks that it is well-formed and belongs to the account, **not that the numbers are
  honest**: this is a single-player demo, not an anti-cheat system. Rewards and currency are placeholders anyway.
- E-mail-only log-in means whoever controls the mailbox controls the account (the same as "forgot password" everywhere).
- Rate limit counters live in the database, so they hold across several server instances.
- The "demo tools" in the game's Settings page (grant currency, next day...) are still open to every account. They were the owner's
  existing stand-ins for real play; restricting them to the admin is a one-line change in `BIBPages.cpp`. **Owner decision.**

## Tests

```sh
npm test                                              # memory store (118 tests, about 5 s)
TEST_MONGODB_URI=mongodb://127.0.0.1:27017 npm test   # also runs everything against that MongoDB (190 tests)
```

`TEST_MONGODB_URI` must point at a **throw-away** server: each run creates and drops its own `bib_test_<random>` databases, but do
not aim it at production. To get a local `mongod` without installing anything system-wide, download the tarball from
fastdl.mongodb.org and run it with `--dbpath` in a temp folder.

## Data model

`accounts` (`uid`, `username`, `email`, `role`, timestamps; unique on lower-cased e-mail and username), `admins` (`uid`, `email`,
`flaggedAt`), `challenges` (pending codes, TTL), `sessions` (token hashes, TTL), `saves` (`uid` -> save text, `savedAt`),
`counters` (user id sequence), `rate` (rate-limit counters, TTL).

Online features: `profiles` (public summary per uid), `friends` (one document per pair: pending request or friendship),
`gangs` (unique lower-cased name, `memberCount` kept atomically at most 30, season fields), `gang_members` (one per player:
role, contribution, season contribution, milestone claims, today's caps), `gang_pending` (end-of-season coins waiting to be
claimed, kept after leaving), `mail` (direct or `to: "all"`), `mail_acks` (mail the game has imported), `notices`, `arena`
(trophies, season, Champions floor, counters, last 10 defences, last 3 opponents, defence team, pending season reward) and
`arena_matches` (one result each, 10-minute expiry, purged after a day by TTL).
