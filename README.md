# x62-fancontrol

Small tool to set the fan speed of x62 laptops from 51nb.
This was written analyzing the windows utility available at
<https://forum.51nb.com/forum.php?mod=attachment&aid=MjI3ODAzNHwzYjcyMmUxMnwxNTA2NDU1NTQyfDE4MjEwMDh8MTY4OTMzNg%3D%3D>.

Use at your own risk, obviously.

Usage:

```
$ sudo apt-get install libpci3 libpci-dev # or equivalent on your system
$ make
$ sudo ./x62-fancontrol
In every command below we'll fail with error code 2 in
the case of unexpected data from an IO port, which is
useful since this seems to happen after a resume.

x62-fancontrol temp
  Displays the current temperature.

x62-fancontrol set-fan-speed <fan-speed>
  Sets the current fan speed. The EC will kick back in after
  a few seconds.

x62-fancontrol manager
  Manages the fan speed for you.
```

I'm not sure yet what the fan speed numbers represent.

See also discussion at
<https://forum.thinkpads.com/viewtopic.php?f=80&t=125041>

A wrapper script that automatically restarts on expected failures after
resumes is provided, `x62-fancontrol-manager.py`.

I currently run it using a simple `systemd` service file:

```
[Service]
ExecStart=/home/bitonic/src/x62-fancontrol/x62-fancontrol-manager.py

[Install]
WantedBy=default.target
```
