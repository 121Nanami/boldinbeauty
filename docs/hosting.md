# Putting the demo in other people's hands: hosted account server + standalone macOS build

*Status 2026-10-05, prepared by the lead session. Nothing here is deployed: every step that needs a login, a card or a decision is the owner's.
The Mac helper can do the build steps; no AI has a Render, Atlas or mail login. Nothing on this page needs a secret in the repository.*

## Where things stand

- The account server (`Demo/BoldInBeautyDemo/Server`) works on the owner's Mac (HANDOFF.md, 2026-10-05): sign-up/log-in by e-mailed code, the admin
  flag for `admin@example.com`, cloud saves. It only runs while a Terminal window on that Mac is open.
- A slim, LFS-free branch `render-deploy` already holds just the server (made 2026-10-04). `render.yaml` at the repository root is a Render
  blueprint for it (free plan, Docker, `/health`). Whether a Render service already exists is **OPEN**: no AI could see the owner's Render account.
- The standalone macOS app has never been packaged from this demo branch. The full cook passed earlier (637 packages, 0 errors).

## 1. Host the server (free tiers)

You need, once: a Render login (free plan needs no card at the time of writing, but check the page), and the three values you already have from
`npm run setup`: the Atlas connection string, the sender mailbox + App password.

1. Render > **New > Blueprint** > connect `121Nanami/BoldInBeauty-Project` and pick a branch that contains `render.yaml` (this work branch, or `main`
   once it is merged). The blueprint deploys the slim branch `render-deploy`, folder `Demo/BoldInBeautyDemo/Server`. Refresh that branch from the current server first if you changed it:
   `git checkout render-deploy && git checkout <work branch> -- Demo/BoldInBeautyDemo/Server && git commit && git push`.
2. Render asks for the five values the blueprint leaves blank: `MONGODB_URI`, `MAIL_FROM`, `SMTP_USER`, `SMTP_PASS` (and confirms `CODE_SECRET`,
   which it generates). Paste them in the dashboard only; never into a file.
3. Atlas > **Network Access**: Render's free instances have no fixed address, so the only way in is `0.0.0.0/0`. That is a trade-off for you to
   accept: it is less risky here because the database user can touch only the `boldinbeauty` database (role `readWrite`) and has a random password.
4. When the deploy is live open `https://<service>.onrender.com/health` (it answers `{"ok":true}`; the first request after a quiet spell takes
   about a minute on the free plan).
5. **Mail check** (only matters for a hosted server; e-mailed codes work on the owner's Mac per the owner). Some hosts block outbound SMTP on free plans (Render announced this for its free web services; confirm on its current docs).
   If sign-up codes never arrive, the service logs `mail server check failed`: switch to HTTPS e-mail with `MAIL_PROVIDER=resend` and
   `RESEND_API_KEY` (resend.com; it needs a verified sending domain, so the DNS records of `example.com` are involved), or use a host that
   allows SMTP (Fly.io, Railway, Cloud Run, a small VPS).
6. Tell the game: `[BoldInBeauty] ApiBaseUrl=https://<service>.onrender.com` in `Demo/BoldInBeautyDemo/Config/DefaultGame.ini`, or start the game with
   `-BIBApi=https://<service>.onrender.com`.

`TRUST_PROXY_HOPS=1` is already in the blueprint so the per-address rate limits see the player and not Render's proxy.

## 2. Standalone macOS build (Mac helper job, UE 5.8.3)

The helper runs this on the owner's Mac after the owner has chosen the address (or leave the default and pass `-BIBApi=` at launch):

```sh
cd <worktree>/Demo/BoldInBeautyDemo
git lfs pull
<UE_5.8>/Engine/Build/BatchFiles/RunUAT.sh BuildCookRun -project=$PWD/BoldInBeautyDemo.uproject -platform=Mac \
  -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=$PWD/Build/Mac -NoHotReloadFromIDE
```

Mac job 7 (2026-10-05) packaged it successfully, but `-archive` copied a content-less `Binaries/Mac` app; the complete app is `Saved/StagedBuilds/Mac/BoldInBeautyDemo.app` (copy it over the archived one with `ditto`). The packaged app logs to `~/Library/Containers/com.YourCompany.BoldInBeautyDemo/Data/Library/Logs/`.

The result is `Build/Mac/.../BoldInBeautyDemo.app` (untracked; the folder is a build leftover, never commit it). Unsigned apps need a
right-click > Open the first time; signing/notarising is only needed to hand the app to other people (Apple Developer account: owner decision).
A scripted PIE run cannot drive the packaged app, so the helper only checks that it starts and stays up with `-BIBGuest` and writes no error to its log;
the sign-up / tutorial / save play-through below is a person's job.

## 3. The owner's 30-minute play-through (no cost; only a person can feel it)

1. Sign up with the admin e-mail: check username, e-mail, user id and the admin extras (all operators, unlimited items, every level open).
2. Sign out and back in (code by e-mail). Make a second account with another e-mail and play the nine-step tutorial; quit half-way and see it resume.
3. Guest account (a local base profile, no login): summon, level up, play Chapter 1 level 1 (Plack arrives), close and reopen to check the save.
4. New: open the Campaign tab, check the 13 chapters and their locks, and play a few stages of Chapters 2-4 as guest and as admin. Note what feels
   wrong (too easy, too hard, too long, a type of enemy that is unfair) in a short list; every number is a placeholder in `docs/campaign.md`.

## 4. Not done, on purpose

Windows (no machine), real payment (always simulated), code signing, and the hosting itself (needs the owner's accounts).
