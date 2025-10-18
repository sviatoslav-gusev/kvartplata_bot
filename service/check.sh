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

KEY_PATH="${KEY_DIR}/kvartplata_bot.key"
CFG_PATH="${CFG_DIR}/kvartplata_bot.cfg"
ENC_PATH="${CFG_DIR}/kvartplata_bot.enc"

RED=$'\e[31m'; YEL=$'\e[33m'; GRN=$'\e[32m'; RST=$'\e[0m'

echo "=== ${APP_NAME} quick health check ==="

check_path() {
  local p="$1"
  if [[ -e "$p" ]]; then
    echo "[$GRN OK $RST] exists: $p"
  else
    echo "[$RED !! $RST] missing: $p"
  fi
}

check_owner_mode() {
  local p="$1" want_user="$2" want_group="$3" want_mode="$4"
  if [[ ! -e "$p" ]]; then
    echo "[$RED !! $RST] missing: $p"
    return
  fi
  local st; st=$(stat -c "%U %G %a" "$p")
  local u g m; read -r u g m <<<"$st"
  local ok=true
  if [[ "$u" != "$want_user" ]]; then
    echo "[$YEL !! $RST] $p owner: $u (expected $want_user)"; ok=false
  fi
  if [[ "$g" != "$want_group" ]]; then
    echo "[$YEL !! $RST] $p group: $g (expected $want_group)"; ok=false
  fi
  if [[ "$m" != "$want_mode" ]]; then
    echo "[$YEL !! $RST] $p mode:  $m (expected $want_mode)"; ok=false
  fi
  $ok && echo "[$GRN OK $RST] $p => $u:$g $m"
}

echo "-- presence --"
check_path "$BIN_DST"
check_path "$SERVICE_FILE"
check_path "$KEY_DIR"
check_path "$CFG_DIR"
check_path "$KEY_PATH"
check_path "$ENC_PATH"
check_path "$CFG_PATH"

echo "-- permissions --"
# каталоги
check_owner_mode "$KEY_DIR" root "$APP_USER" 750
check_owner_mode "$CFG_DIR" "$APP_USER" "$APP_USER" 755
echo "$RED       TODO: 755 to 700 for ${CFG_DIR}  $RST"
# файлы
check_owner_mode "$KEY_PATH" root "$APP_USER" 640
check_owner_mode "$ENC_PATH" "$APP_USER" "$APP_USER" 600

echo "-- systemd --"
if systemctl is-enabled "${APP_NAME}.service" >/dev/null 2>&1; then
  echo "[$GRN OK $RST] enabled"
else
  echo "[$YEL .. $RST] not enabled (run: sudo systemctl enable ${APP_NAME}.service)"
fi

state="$(systemctl is-active "${APP_NAME}.service" 2>/dev/null || true)"
case "$state" in
  active)   echo "[$GRN OK $RST] service active";;
  inactive) echo "[$YEL .. $RST] service inactive";;
  failed)   echo "[$RED !! $RST] service failed";;
  *)        echo "[$YEL .. $RST] service state: $state";;
esac

echo
echo "▶️ Start/Stop/Restart:     systemctl start|stop|restart ${APP_NAME}.service"
echo "ℹ️ Status:                 systemctl status ${APP_NAME}.service"
echo "📜 Logs (journal):         journalctl -u ${APP_NAME}.service -f"
