## Telegram bot with monthly reminder

There is just standalone executable.  
If you want to run it as Linux systemd service, go to `release_1_linux_service` branch — you will find installation and another scripts.

---
**Built binaries**

Find them [here](https://github.com/sviatoslav-gusev/kvartplata_bot/tree/release_1/release).

---
**To build by yourself**

1. **Install dependence:** [https://github.com/reo7sp/tgbot-cpp](https://github.com/reo7sp/tgbot-cpp)

2. **Project valid for Linux and Windows** and was built in my setup with cmake/clang/ninja at Fleet IDE.  
   Edit for your needs.

---
**Usage**

It is highly recommended to use this bot as Linux systemd service. All you need is [here](https://github.com/sviatoslav-gusev/kvartplata_bot/tree/release_1_linux_service).
But you can also use this as standalone binary (valid for both Windows and Linux versions).

1. **At first run** `kvartplata_bot` will request token from BotFather — input it. Now you own working bot.

2. **To setup DD, HH, MM of alarm**, you will need to stop program and edit `kvartplata_bot.cfg`.  
   After that start again. TODO: make such behavior more friendly

3. **Arguments:** (helpful for standalone usage)

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


