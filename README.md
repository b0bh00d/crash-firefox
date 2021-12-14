# Crash Firefox!
This project puts a simple GUI frontend on top of Benjamin Smedberg's [crashfirefox-intentionally](https://github.com/bsmedberg/crashfirefox-intentionally) project.

![CrashFirefox_2021-12-14_11-42-32](https://user-images.githubusercontent.com/4536448/146060126-9fff8abc-1403-42bc-b719-cc38f753e9d8.png)

Since it utilizes the same code he used to crash the Firefox process, this is necessarily a Windows-only project.

Please refer to that orignial project for the intent of this tool ( I'm not just being mean-spirited, ya know :smirk: ).

## How about some fire, scarecrow!
It also sports an additional feature for crashing multiple instances.  The original project was a command-line utility that took a process id to be crashed.  ``Crash Firefox!`` allows you to "Punish similar processes" to the one you select (presumably the Firefox "firefox.exe" process).  In this way, you can bring down all Firefox instances that are currently running on your machine.

## Nuts 'n bolts
``Crash Firefox!`` uses Qt as its GUI framework.  I wrote it against Qt 5.14.1, and any 5.x version should compile without issue.  If you want to compile against Qt 6.x, you may need to make some changes.  I have not tested with that version.

The release available from this page was statically linked against the aforementioned Qt version, so you should need nothing more than the executable itself to run the program.

## Go forth and hurl, dude!
I hope you find this useful, and enjoy hurling thunderbolts at the Firefox browser.  :grin: