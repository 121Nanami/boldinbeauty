# Boldinbeauty website

Static site with no build step. Open `index.html` in a browser, or upload the whole `website/` folder to any static host (GitHub Pages, Netlify, Cloudflare Pages).

## Layout
- `index.html`: home view (hero, news, world, operators, gameplay/rates, FAQ) and support view (`#support`, `#socials`, `#merch`).
- `css/style.css`: all styling. Colour and font tokens sit at the top in `:root`.
- `js/site.js`: content you edit (links, news posts, operator roster) plus rendering.
- `assets/operators/*.jpg`: 900px-tall JPEGs made with `sips` from `art/whole-character-views/2026-10-04/<name>/front.png` (draft designs).

## Common edits (all in `js/site.js`)
- **Patreon / Buy Me a Coffee / merch store:** paste the full `https://` URL into `SITE.support.patreon`, `SITE.support.buyMeACoffee` or `SITE.merchStore`. The button switches from "Coming soon" to a live link automatically.
- **Socials:** fill `url` for each entry in `SITE.socials`. Add or remove entries as needed.
- **News:** add an object to the top of `NEWS` with `type` (`news`, `dev` or `art`), `date` (`YYYY-MM-DD`), `title` and `body`.
- **Operator art:** replace the JPEG in `assets/operators/` with the same filename.

Game facts on the page come from `BoldInBeauty_MAIN` sections 1, 2, 5, 6, 7 and 8 (as of 2026-10-06). Summoning rates are labelled as planned and subject to change.
