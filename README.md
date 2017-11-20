# x62-fancontrol

Small tool to set the fan speed of x62 laptops from 51nb.
This was written analyzing the windows utility available
at <https://forum.51nb.com/forum.php?mod=attachment&aid=MjI3ODAzNHwzYjcyMmUxMnwxNTA2NDU1NTQyfDE4MjEwMDh8MTY4OTMzNg%3D%3D>.

Use at your own risk, obviously.

Usage:

```
$ sudo apt-get install libpci3 libpci-dev # or equivalent on your system
$ make
$ sudo ./x62-fancontrol    # gets the temperature
$ sudo ./x62-fancontrol n  # set the fan speed to n, where 0 <= n <= 255
```

I'm not sure yet what the fan speed numbers represent.

