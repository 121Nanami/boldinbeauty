#!/bin/bash
# Double-click this file (or run it) to set up the Boldinbeauty account server: it opens Terminal and runs the guided setup.
# You only approve one Atlas login code in your browser and paste one Google App password; see README.md in this folder.
cd "$(dirname "$0")" || exit 1
echo "Boldinbeauty account server setup"
echo
if ! command -v node >/dev/null 2>&1; then
  echo "Node.js is not installed. Install it first (in Terminal:  brew install node  , or https://nodejs.org ) and double-click this again."
  read -r -p "Press Enter to close."
  exit 1
fi
if [ ! -d node_modules ]; then
  echo "Installing the server's packages (one time)..."
  npm install --no-audit --no-fund || { echo "npm install failed."; read -r -p "Press Enter to close."; exit 1; }
fi
npm run setup
status=$?
echo
if [ $status -ne 0 ]; then echo "Setup stopped (see the message above). You can run it again: double-click setup.command."; fi
read -r -p "Press Enter to close this window."
exit $status
