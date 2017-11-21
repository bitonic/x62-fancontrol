#!/usr/bin/env python3

# Script that automatically restarts x62-fancontrol if it fails
# with exit code 2 after a reasonable time. Useful to survive
# suspends.

import os
import sys
import subprocess
import datetime

x62_fancontrol = os.path.dirname(os.path.realpath(__file__)) + "/x62-fancontrol"

fast_crashes = 0

while True:
  started_at = datetime.datetime.now()
  exit = subprocess.call([x62_fancontrol, "manager"])
  if exit == 2:
    now = datetime.datetime.now()
    if (now - started_at).microseconds < 100 * 1000:
      fast_crashes += 1
      if fast_crashes >= 5:
        raise RuntimeError(
          "x62-fancontrol exited with code 2, which usually happens "
          "when resuming from a suspend, but it failed 5 times within 100 "
          "milliseconds, thus probably something else is wrong. Exiting.")
      else:
        print(
          ("x62-fancontrol exited with code 2, which usually happens "
           "when resuming from a suspend, but it failed {} times within 100 "
           "milliseconds. Will sleep for a second and retry.").format(fast_crashes),
          file=sys.stderr)
        time.sleep(1)
    else:
      fast_crashes = 0
      print(
        "Got exit code 2 from x62-fancontrol, assuming that\n"
        "it's after a suspend, will restart.",
        file=sys.stderr)
  else:
    raise RuntimeError(
      "x62-fancontrol manager unexpectedly terminated with exit code {}".format(exit))
