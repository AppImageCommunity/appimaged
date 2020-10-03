# appimaged

## :warning: This project has been deprecated in favor of the [new codebase](https://github.com/probonopd/go-appimage).

`appimaged` is an optional daemon that watches locations like `~/bin` and `~/Downloads` for AppImages and if it detects some, registers them with the system, so that they show up in the menu, have their icons show up, MIME types associated, etc. It also unregisters AppImages again from the system if they are deleted. Optionally you can use a sandbox if you like: If the [firejail](https://github.com/netblue30/firejail) sandbox is installed, it runs the AppImages with it.
