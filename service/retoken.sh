#!/usr/bin/env bash
set -euo pipefail

# Ensure the script runs as root
if [[ "$EUID" -ne 0 ]]; then
  echo "❌ This script must be run as root (use: sudo $0)"
  exit 1
fi

APP_NAME="kvartplata_bot"
APP_USER="kvartplata"

KEY_DIR="/etc/${APP_NAME}"
CFG_DIR="/var/lib/${APP_NAME}"
KEY_PATH="${KEY_DIR}/kvartplata_bot.key"
ENC_PATH="${CFG_DIR}/kvartplata_bot.enc"
BIN_DST="/usr/local/bin/${APP_NAME}"

echo "Stopping service..."
sudo systemctl stop "${APP_NAME}.service" || true

# Prepare backups and ensure dirs are writable (needed for atomic create/rename)
STAMP="$(date +%Y%m%d-%H%M%S)"
echo "Temporarily making dirs writable for ${APP_USER}..."
sudo chmod 770 "${KEY_DIR}"
sudo chmod 770 "${CFG_DIR}"

# Backup existing files if present, then REMOVE them so the program must recreate them
if [[ -f "${KEY_PATH}" ]]; then
  echo "Backing up key -> ${KEY_PATH}.${STAMP}.bak"
  sudo cp -a "${KEY_PATH}" "${KEY_PATH}.${STAMP}.bak"
  echo "Removing old key: ${KEY_PATH}"
  sudo rm -f "${KEY_PATH}"
else
  echo "Key does not exist (ok): ${KEY_PATH}"
fi

if [[ -f "${ENC_PATH}" ]]; then
  echo "Backing up enc -> ${ENC_PATH}.${STAMP}.bak"
  sudo cp -a "${ENC_PATH}" "${ENC_PATH}.${STAMP}.bak"
  echo "Removing old enc: ${ENC_PATH}"
  sudo rm -f "${ENC_PATH}"
else
  echo "Enc does not exist (ok): ${ENC_PATH}"
fi

echo "Starting interactive token entry as ${APP_USER}..."
echo "When the program finishes initialization successfully, press Ctrl+C to exit."
read -p "Press Enter to start... "

sudo -u "${APP_USER}" bash -lc "cd '${CFG_DIR}' && '${BIN_DST}' --key_dir '${KEY_DIR}' --cfg_dir '${CFG_DIR}'" || true

# Sanity check: ensure files were created
missing=false
if [[ ! -f "${KEY_PATH}" ]]; then echo "ERROR: key was not created: ${KEY_PATH}"; missing=true; fi
if [[ ! -f "${ENC_PATH}" ]]; then echo "ERROR: enc was not created: ${ENC_PATH}"; missing=true; fi
if $missing; then
  echo "Retoken failed: files missing. Check program output above."
  exit 1
fi

# Restore secure permissions
echo "Restoring secure permissions..."
sudo chown root:"${APP_USER}" "${KEY_PATH}"
sudo chmod 640 "${KEY_PATH}"

sudo chown "${APP_USER}:${APP_USER}" "${ENC_PATH}"
sudo chmod 600 "${ENC_PATH}"

sudo chmod 750 "${KEY_DIR}"
sudo chmod 700 "${CFG_DIR}"

echo "Starting service..."
sudo systemctl start "${APP_NAME}.service"
sudo systemctl --no-pager --full status "${APP_NAME}.service" || true

echo "Done. Tail logs with:"
echo "  sudo journalctl -u ${APP_NAME}.service -f"
