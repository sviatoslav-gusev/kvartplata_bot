Scripts for kvartlplata_bot service management.

install.sh
    To be able run install.sh:
        place kvartlplata_bot binary to this directory
        or make symlink to binary located in build dir

uninstall.sh [--purge-data]
    Add --purge-data flag to remove "/etc/..." and "/var/lib/..."

retoken.sh
    Allows to reinput bot token

check.sh
    Check installed files, minimalism at permissions etc.
    Temporary secutity breach of configs location for 1.0 is allowed. TODO: fix that later

kvartplata_bot.service
    Service to be runned.
        Start/Stop/Restart:     systemctl start|stop|restart kvartlplata_bot.service
        Status:                 systemctl status kvartlplata_bot.service
        Logs (journal):         journalctl -u kvartlplata_bot.service -f