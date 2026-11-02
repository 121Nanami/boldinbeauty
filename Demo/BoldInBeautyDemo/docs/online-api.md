# Online API (friends, gangs, mail, notices, arena) — contract, 2026-10-06

Owner request 2026-10-06: a friends menu, gangs (clans) of up to 30 with join requirements, gang season rewards and a gang shop,
admin mail to one user or every user, Nikke-style notices, and a semi-online arena (attack / defence lineups, trophies, ranks,
monthly seasons, matchmaking by strength then rank). Everything numeric is PLACEHOLDER unless the owner gave it.

The account service (`Server/`) serves these routes next to `/v1/auth/*`, `/v1/me` and `/v1/save`. Every route below needs a
session (`Authorization: Bearer <token>`) and answers JSON `{ ok: true, ... }` or `{ ok: false, error: "<code>" }` with the usual
status codes (400 bad input, 401 no session, 403 not allowed, 404 unknown, 409 conflict, 429 rate limited). Bodies are small
(4 KB, 16 KB for `/v1/arena/defence` and `/v1/profile`). Nothing user-supplied is echoed in an error. All times are ISO strings.

**Season key:** the UTC calendar month, `"YYYY-MM"` (gang season, arena season). The game derives the same key from the clock.

## Public profile
The game pushes a small public summary after load and when it changes; friends, gangs and the arena read it.

| Route | Body | Reply |
| --- | --- | --- |
| `PUT /v1/profile` | `{ level:int 1..999, power:int 0..2e9, lobbyOp:int 0..19, team:[{op:int 0..19, level:int 1..9999, stars:int 0..999, power:int}] (0..3) }` | `{ ok }` |

Stored per uid with `name` (the account username, never from the body), `updatedAt` (= "last seen").

## Friends (max 50 friends, max 50 pending each way)

| Route | Body | Reply |
| --- | --- | --- |
| `GET /v1/friends` | | `{ friends:[Card], incoming:[Card], outgoing:[Card] }` |
| `POST /v1/friends/request` | `{ to:"<user id or username>" }` | `{ ok, status:"sent"|"accepted" }` (a request to someone who already asked you accepts it) |
| `POST /v1/friends/accept` | `{ uid:"<id>" }` | `{ ok }` |
| `POST /v1/friends/decline` | `{ uid }` | `{ ok }` (also cancels your own outgoing request) |
| `POST /v1/friends/remove` | `{ uid }` | `{ ok }` |

`Card = { uid:"10000002", name, level, power, lastSeen, gang:"<gang name or ''>" }`. Errors: `not_found` (no such player),
`self`, `already_friends`, `already_requested`, `friends_full`.

## Gangs
A gang has `id` (string), `name` (3-20 chars, letters/digits/spaces, unique case-insensitively), `leaderUid`, `members` (max 30),
`minLevel` (0..999), `minPower` (0..2e9), `open` (bool: when false nobody can join), `xp` (lifetime), `season` ("YYYY-MM"),
`seasonPoints`. A player is in at most one gang.

| Route | Body | Reply |
| --- | --- | --- |
| `GET /v1/gangs?q=<text>` | | `{ gangs:[GangSummary] }` (up to 20; name contains q, case-insensitive; empty q = most active) |
| `GET /v1/gangs/mine` | | `{ gang: GangDetail | null, pendingCoins:int, pendingSeason:"YYYY-MM" | "" }` |
| `POST /v1/gangs` | `{ name, minLevel, minPower, open }` | `{ gang: GangDetail }` (the creator leads; `in_gang`, `name_taken`, `invalid_name`) |
| `POST /v1/gangs/join` | `{ id, level, power }` | `{ gang: GangDetail }` (`in_gang`, `gang_full`, `gang_closed`, `requirements`) |
| `POST /v1/gangs/leave` | `{}` | `{ ok }` (a leaving leader hands over to the member with the most contribution; the last member leaving deletes the gang) |
| `POST /v1/gangs/kick` | `{ uid }` | `{ gang }` (leader only: `not_leader`) |
| `POST /v1/gangs/settings` | `{ minLevel, minPower, open }` | `{ gang }` (leader only) |
| `POST /v1/gangs/contribute` | `{ kind:"checkin"|"donate"|"activity", amount:int }` | `{ gang, added:int }` |
| `POST /v1/gangs/claim` | `{}` | `{ coins:int, season }` (the pending end-of-season coins; `nothing_to_claim`) |
| `POST /v1/gangs/milestone` | `{ index:int 0..4 }` | `{ ok }` (records this member's claim for the current season; `not_reached`, `already_claimed`) |

Contribution points (per member, per UTC day, server-enforced): `checkin` = 50 once a day; `donate` = `amount` points, 10 per
donation, at most 5 donations a day (the game takes 1,000 Gold per donation); `activity` = `amount` points (1..50 per call), at
most 200 a day (the game sends 5 per claimed daily quest and 2 per cleared stage). Each point adds to the gang's `xp`,
`seasonPoints` and the member's `contribution` and `seasonContribution`.

Gang level = `1 + floor(sqrt(xp / 500))`, max 30 (shown only). Season milestones on `seasonPoints`: `[1000, 3000, 6000, 10000,
15000]` (the game knows each milestone's reward). **Season end** (the first gang request in a new month for that gang, or any
member's request): for the finished season every current member gets `pendingCoins += tierCoins + min(300, floor(seasonContribution
/ 10))` where `tierCoins` by seasonPoints is `0→50, 1000→150, 3000→300, 6000→450, 10000→650, 15000→900`; then `seasonPoints`,
every `seasonContribution` and the milestone claims reset and `season` becomes the current key. Pending coins wait for the player
even after they leave the gang.

`GangSummary = { id, name, level, members:int, maxMembers:30, minLevel, minPower, open, leaderName }`.
`GangDetail = GangSummary + { xp, season, seasonPoints, milestonesClaimed:[int] (this player's), myRole:"leader"|"member",
members:[{ uid, name, role, level, power, contribution, seasonContribution, lastSeen }] }`.

## Mail
Admin mail to one user or to every user. Rewards are a small object of non-negative ints:
`{ gold, bytes, hearts, diamonds, regular, special, arcade, gangCoins }` (each 0..1,000,000; unknown keys dropped).

| Route | Body | Reply |
| --- | --- | --- |
| `GET /v1/mail` | | `{ mail:[{ id, title, body, reward, from:"Admin", createdAt }] }` — mail for this player not yet acknowledged: direct mail, and broadcasts created after the account was created. Max 50, oldest first. |
| `POST /v1/mail/ack` | `{ ids:[string] }` | `{ ok }` — the game imported them into the save (they are not offered again) |
| `POST /v1/admin/mail` | `{ to:"all" | "<user id>", title (1..60), body (0..600), reward }` | `{ ok, id, recipients:int|"all" }` — admin only (`forbidden`), `not_found` for an unknown user |

## Notices (Nikke-style announcements)

| Route | Body | Reply |
| --- | --- | --- |
| `GET /v1/notices` | | `{ notices:[{ id, category:"notice"|"event"|"update"|"maintenance", title, body, createdAt, pinned }] }` newest first, pinned first, max 30 |
| `POST /v1/admin/notices` | `{ category, title (1..60), body (1..2000), pinned:bool }` | `{ ok, id }` admin only |
| `POST /v1/admin/notices/delete` | `{ id }` | `{ ok }` admin only |

## Arena
Ranks by trophies: rank index `min(15, floor(trophies / 100))`; 0-2 Bronze 1-3, 3-5 Silver 1-3, 6-8 Gold 1-3, 9-11 Diamond 1-3,
12-14 Platinum 1-3, 15 Champions (1,500+). New players start at 0 trophies (Bronze 1).

| Route | Body | Reply |
| --- | --- | --- |
| `PUT /v1/arena/defence` | `{ team:[Fighter] (1..3), power:int }` | `{ ok }` |
| `GET /v1/arena/me` | | `{ trophies, rank, season, champion:bool, wins, losses, defenceWins, defenceLosses, log:[{ at, attacker, won, delta }] (last 10 defences), pendingReward:{ season, rank } | null }` |
| `POST /v1/arena/match` | `{ power:int }` | `{ matchId, opponent:{ uid, name, trophies, rank, power, team:[Fighter] } }` or `{ matchId:"", opponent:null }` when no other player has a defence |
| `POST /v1/arena/result` | `{ matchId, won:bool }` | `{ trophies, rank, delta }` (one result per match; a match expires after 10 minutes: `match_expired`) |
| `POST /v1/arena/claim` | `{}` | `{ season, rank }` (the pending season reward, once) |

`Fighter = { op:int 0..19, level:int, stars:int, hp:number, attack:number, defence:number (0..75), power:int }`.

**Matchmaking:** candidates are other players with a defence team, excluding the last 3 opponents of this player. First by
strength: power within ±15% of the attacker's, widening to ±30% and ±60%, then anyone; inside that band, the closest trophies
win (ties random). **Trophies (zero-sum):** `delta = clamp(round(30 + (loserTrophies - winnerTrophies) / 25), 10, 50)`; the winner
gains it and the loser loses it (attacker or defender). Floors: never below 0, and a player who reached Champions (1,500) this
season never falls below 1,500 until the season ends. **Season end** (first arena request in a new month for that player): rank
drops by 3, or by 4 from Diamond 1 upward (index ≥ 9), trophies become `newRank * 100`, the Champions floor clears, and
`pendingReward = { season: old, rank: oldRank }`.

## Trust
The save is client-authoritative in this demo, so the server trusts the reported level, power and fight result. Server-side
battle verification is OPEN.
