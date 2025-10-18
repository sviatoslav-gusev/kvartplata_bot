#!/usr/bin/env bash
set -euo pipefail

# Ensure the script runs as root
if [[ "$EUID" -ne 0 ]]; then
  echo "❌ This script must be run as root (use: sudo $0)"
  exit 1
fi

APP_NAME="kvartplata_bot"
APP_USER="kvartplata"

BIN_SRC="./${APP_NAME}"                     # Binary located in the same dir as this install.sh
BIN_DST="/usr/local/bin/${APP_NAME}"

KEY_DIR="/etc/${APP_NAME}"
KEY_PATH="${KEY_DIR}/${APP_NAME}.key"

CFG_DIR="/var/lib/${APP_NAME}"
ENC_PATH="${CFG_DIR}/${APP_NAME}.enc"
CFG_PATH="${CFG_DIR}/${APP_NAME}.cfg"

SERVICE_FILE="/etc/systemd/system/${APP_NAME}.service"

# 0) Check bin
[[ -f "$BIN_SRC" ]] || { echo "❌ Cannot find binary: $BIN_SRC"; exit 1; }

# 1) Check depencencies (ldd)
echo "🔎 Checking deps..."
if ! command -v ldd >/dev/null 2>&1; then
  echo "⚠️  ldd not found. Skip deps check."
else
  MISSING=$(ldd "$BIN_SRC" | awk '/not found/ {print $1}')
  if [[ -n "${MISSING}" ]]; then
    echo "❌ Libs not found:"
    echo "$MISSING"
    echo "Install missing libs and run install.sh again."
    exit 2
  else
    echo "✅ All deps are found."
  fi
fi

# 2) Service user
if ! id -u "$APP_USER" >/dev/null 2>&1; then
  useradd -r -s /usr/sbin/nologin "$APP_USER"
  echo "✅ User $APP_USER is created"
fi

# 3) Dirs / rights
mkdir -p "$KEY_DIR" "$CFG_DIR"
chown -R root:"$APP_USER" "$KEY_DIR"
chown -R "$APP_USER:$APP_USER" "$CFG_DIR"
chmod 750 "$KEY_DIR"
chmod 700 "$CFG_DIR"
echo "✅ Ready catalogs: $KEY_DIR $CFG_DIR"

systemctl stop "${APP_NAME}.service" || true

# 4) Install bin
install -m 0755 "$BIN_SRC" "$BIN_DST"
echo "✅ Binary installed: $BIN_DST"

# 5) Unit-file
cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=Kvartplata Telegram Bot
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${APP_USER}
Group=${APP_USER}
WorkingDirectory=${CFG_DIR}
ExecStart=${BIN_DST} --key_dir ${KEY_DIR} --cfg_dir ${CFG_DIR}

Restart=always
RestartSec=5

NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=${CFG_DIR} ${KEY_DIR}

StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

echo "✅ Unit written: $SERVICE_FILE"

# 6) First run (with token input)
echo
echo "ℹ️ First run with user ${APP_USER} for token input."
read -p "Enter -> input token -> Enter -> few secs -> Stop (Ctrl+C)"

# ВРЕМЕННО: дать группе запись на каталог с ключом (для атомарной записи/rename)
chgrp "$APP_USER" "$KEY_DIR"
chgrp "$APP_USER" "$CFG_DIR"
chmod 770 "$KEY_DIR"
chmod 770 "$CFG_DIR"

# Подготовим пустые файлы/права, чтобы процесс мог писать куда нужно
rm -f "$KEY_PATH" || true
rm -f "$ENC_PATH" || true

# Запуск (CLI укажет директории, а внутри твой код соберёт полные имена файлов)
sudo -u "$APP_USER" bash -lc "cd '${CFG_DIR}' && '${BIN_DST}' --key_dir '${KEY_DIR}' --cfg_dir '${CFG_DIR}'" || true

# Narrowing rights
chown root:"$APP_USER" "$KEY_PATH"
chmod 640 "$KEY_PATH"
chmod 750 "$KEY_DIR"

chown "$APP_USER:$APP_USER" "$ENC_PATH"
chmod 600 "$ENC_PATH"
chmod 666 "$CFG_PATH" # TODO 600/640
chmod 755 "$CFG_DIR"  # TODO 700

# ВОЗВРАТ прав на каталог (без права записи группе)
sudo chmod 750 "$KEY_DIR"


# 7) Enable and run service
systemctl daemon-reload
systemctl enable --now "${APP_NAME}.service"
systemctl --no-pager --full status "${APP_NAME}.service" || true

echo
echo "🎉 Ready!"
echo "▶️ Start/Stop/Restart:     systemctl start|stop|restart ${APP_NAME}.service"
echo "ℹ️ Status:                 systemctl status ${APP_NAME}.service"
echo "📜 Logs (journal):         journalctl -u ${APP_NAME}.service -f"
