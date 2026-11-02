# Version control for Boldinbeauty

Two separate repositories keep project knowledge and the game source independent:

- Documents: `BoldInBeauty-Project` (this folder).
- Game: `Unreal Projects/BoldInBeauty` inside this folder.

Both use the `main` branch and Git LFS. The downloaded `UE_5.8` engine, local connections, credentials and generated build/cache files are excluded. Game source, Config, Build resources and plugin source are retained. Binary assets such as `.uasset`, `.umap`, textures, audio and authoring files use LFS via committed `.gitattributes`. Never force-push or copy one device over the other before reviewing differences. Windows reconciliation remains OPEN; `Narrator:Diamon.md` needs an owner-approved filename mapping before a Windows checkout.

GitHub copies: [project documents](https://github.com/121Nanami/BoldInBeauty-Project) and [Unreal game](https://github.com/121Nanami/BoldInBeauty-Game), both private.

## Everyday steps

In GitHub Desktop, use File → Add Local Repository and select each folder above. Select the repository you intend to work on. Review changed files, write a short description, click Commit to main, then Push origin after GitHub has been connected. Fetch origin checks for remote changes; pull before editing on another device. Commits save versions locally; push backs those commits up to GitHub.

In Terminal, run these commands in the selected repository:

```sh
git status
git add path/to/changed-file
git commit -m "Describe the change"
git push
```

Only add files you reviewed. Before the first remote publication, authenticate with `gh auth login --hostname github.com --git-protocol https --web`; private repositories are the default for this project. A private repository restricts viewing, but still exclude credentials. Never place tokens or passwords in project files. Do not enable paid LFS overages.

On a new computer, install Git and Git LFS, run `git lfs install`, clone each repository separately, then run `git lfs pull` in each clone. Recheck sync/README.md before reconciling the Windows implementation; the current Mac game is not an approved replacement for it. Device profiles and writer reservations remain local.

## Checks

```sh
git status
git lfs ls-files
git lfs fsck
git remote -v
```

The existing RiderLink plugin icon uses LFS (7.2 KB); future game assets will appear as they are added. `.gitattributes` makes future matching files use it automatically.

Configuration follows [GitHub LFS documentation](https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage) and [ignore-file documentation](https://docs.github.com/en/get-started/git-basics/ignoring-files).
