## Scripts for kvartplata_bot service management

To build binary refer to documentation at `release_1` branch

---

### `install.sh`
To be able to run `install.sh`:
- Place `kvartplata_bot` binary in this directory,  
  **or**
- Make a symlink to the binary located in the build directory.

---

### `uninstall.sh [--purge-data]`
Add the `--purge-data` flag to remove:
```
/etc/...
/var/lib/...
```

---

### `retoken.sh`
Allows you to re-enter the bot token.

---

### `check.sh`
Checks installed files, verifies minimal permissions, etc.  
A temporary security breach of configs location for version **1.0** is allowed.  
**TODO:** fix that later.

---

### `kvartplata_bot.service`
Service to be run by **systemd**.

Commands:
```bash
# Start / Stop / Restart service
systemctl start|stop|restart kvartplata_bot.service

# Check status
systemctl status kvartplata_bot.service

# View logs (journal)
journalctl -u kvartplata_bot.service -f
```
