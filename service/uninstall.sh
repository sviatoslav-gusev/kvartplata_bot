#!/usr/bin/env bash
set -euo pipefail

# Ensure the script runs as root
if [[ "$EUID" -ne 0 ]]; then
  echo "❌ This script must be run as root (use: sudo $0)"
  exit 1
fi

APP_NAME="kvartplata_bot"
APP_USER="kvartplata"

BIN_DST="/usr/local/bin/${APP_NAME}"
SERVICE_FILE="/etc/systemd/system/${APP_NAME}.service"
KEY_DIR="/etc/${APP_NAME}"
CFG_DIR="/var/lib/${APP_NAME}"

PURGE_DATA="${1:-}"   # --purge-data (remove /etc/... and /var/lib/...)

read -p "Remove service ${APP_NAME}? (yes/no) " ans
[[ "$ans" == "yes" ]] || { echo "Aborted."; exit 0; }

echo "Stopping and disabling service..."
sudo systemctl stop "${APP_NAME}.service" || true
sudo systemctl disable "${APP_NAME}.service" || true

if [[ -f "$SERVICE_FILE" ]]; then
  sudo rm -f "$SERVICE_FILE"
  echo "Removed unit: ${SERVICE_FILE}"
fi

sudo systemctl daemon-reload || true

if [[ -f "$BIN_DST" ]]; then
  sudo rm -f "$BIN_DST"
  echo "Removed binary: ${BIN_DST}"
fi

if [[ "$PURGE_DATA" == "--purge-data" ]]; then
  # Remove data/config dirs entirely (this deletes key/enc/configs)
  sudo rm -rf "$CFG_DIR" "$KEY_DIR"
  echo "Removed data/config: ${CFG_DIR} ${KEY_DIR}"
else
  echo "Kept data/config: ${CFG_DIR} ${KEY_DIR}"
fi

# Optionally remove system user (usually not required):
# sudo userdel -r "${APP_USER}" 2>/dev/null || true

echo "Done."
