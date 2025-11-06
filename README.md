## Telegram bot with monthly reminder

There is just standalone executable.  
If you want to run it as Linux systemd service, go to `release_1_linux_service` branch — you will find installation and another scripts.

---
**Download binary**

You may find ready binaries [here](https://github.com/sviatoslav-gusev/kvartplata_bot/tree/release_1/release).

---
**Build by yourself**

1. **Install dependence:** [https://github.com/reo7sp/tgbot-cpp](https://github.com/reo7sp/tgbot-cpp).

2. **Project valid for Linux and Windows** and was built in my setup with cmake/clang/ninja at Fleet IDE.  
   Edit for your needs.  
   Build binary your server.

---
**How to use**

It is recommended to use this as Linux systemd service. Detailed instructions are [here](https://github.com/sviatoslav-gusev/kvartplata_bot/blob/release_1_linux_service/README.md).  
But also you can use it as standalone executable (both Windows or Linux) instead.

1. **At first run** `kvartplata_bot` will request token from BotFather — input it and follow hints. 
   It running at your own TG-bot :)

2. **To setup DD, HH, MM of alarm**, you will need to stop program and edit `kvartplata_bot.cfg`.  
   After that start again.
   TODO: change this approach in future versions

3. **Binary arguments:** (standalone usage specific)

   ```bash
   kvartplata_bot [options] [args...]
   ```

   **Options:**
   ```
   -h, --help
       Show this help and exit
   -f, --enable_logs_to_file
       Enable writing logs to file
   -c, --cfg_dir <value>
       Config directory
   -k, --key_dir <value>
       Key directory
   -l, --log_dir <value>
       Log directory
   ```

   You can leave flags empty — all files will be created at the same directory as executable.
