#include <sys/io.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pci/pci.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void die(char *msg, ...) {
  va_list args;
  va_start(args, msg);
  fprintf(stderr, "x62-fancontrol: ");
  vfprintf(stderr, msg, args);
  fputc('\n', stderr);
  exit(1);
}

// fan control / temperature
// --------------------------------------------------------------------

void get_perm(unsigned long from, unsigned long num) {
  int err = ioperm(from, num, 1);
  if (err) {
    die("Could not set permission from 0x%x to 0x%x: %s", from, from+num, strerror(errno));
  }
}

void get_perms(void) {
  printf("Getting IO ports permissions\n");
  get_perm(0x4E, 2); // 78 and 79
  get_perm(0x68, 1); // 104
  get_perm(0x6C, 1); // 108
}

void initialize_pci(void) {
  // initialize pci_access
  struct pci_access *pacc;
  pacc = pci_alloc();
  pacc->error = die;
  printf("  Initializing PCI\n");
  pci_init(pacc);
  pci_scan_bus(pacc);

  // match the device
  printf("  Finding PCI device\n");
  struct pci_dev *device = NULL;
  struct pci_dev *current_device = pacc->devices;
  while (current_device) {
    printf("    Testing device %x:%x\n", current_device->vendor_id, current_device->device_id);
    if (current_device->vendor_id == 0x8086 && current_device->device_id == 0x9cc3) {
      printf("      Match!\n");
      if (device) {
        die("Matched multiple devices!");
      }
      device = current_device;
    }
    current_device = current_device->next;
  }
  if (!device) {
    die("Could not match any device!");
  }

  // set the required config
  printf("  Setting PCI config\n");
  pci_write_long(device, 0x84, 0x40069);

  // cleanup
  pci_cleanup(pacc);
}

void wait_0x6C_second_bit_unset(void) {
  int counter = 0;
  int set = (inb(0x6C) & 2) == 2;
  while (counter <= 1000 && set) {
    counter++;
    usleep(1000); // 1ms
    set = (inb(0x6C) & 2) == 2;
  }
  if (set) {
    die("The second bit of 0x6C didn't reset!");
  }
}

void wait_0x6C_first_bit_set(void) {
  int counter = 0;
  int unset = !(inb(0x6C) & 1);
  while (counter <= 1000 && unset) {
    counter++;
    usleep(1000); // 1ms
    unset = !(inb(0x6C) & 1);
  }
  if (unset) {
    die("The first bit of 0x6C didn't get set!");
  }
}

void set_0x6C(unsigned char val) {
  wait_0x6C_second_bit_unset();
  outb(val, 0x6C);
  wait_0x6C_second_bit_unset();
}

void unknown_communication(void) {
  printf("Sending unknown commands\n");
  set_0x6C(0x33);
  outb(0x06, 0x68);
}

unsigned char read_temperature(void) {
  set_0x6C(0x44);
  outb(0x00, 0x68);
  wait_0x6C_first_bit_set();
  return inb(0x68);
}

void set_fan_speed(unsigned char fan_speed) {
  set_0x6C(0x55);
  outb(fan_speed, 0x68);
}

void initialize(void) {
  printf("Initializing\n");
  // PCI
  initialize_pci();
  // get the permissions for io ports
  get_perms();
  // ports
  printf("Send init IO ports commands\n");
  outb(0x07, 0x4E);
  outb(0x12, 0x4F);
  outb(0x30, 0x4E);
  outb(0x00, 0x4F);
  outb(0x61, 0x4E);
  outb(0x68, 0x4F);
  outb(0x63, 0x4E);
  outb(0x6C, 0x4F);
  outb(0x30, 0x4E);
  outb(0x01, 0x4F);
  // not sure what this does
  unknown_communication();
}

// daemon
// --------------------------------------------------------------------

// enter must be geq than leave
struct temp_level {
  unsigned char enter; // the temperature at which this level is entered
  unsigned char leave; // the temperature at which this level is left
  unsigned char fan_speed; // the fan speed for this level
};

struct temp_level default_levels[] = {
  { 0,  0,  0  },
  { 40, 30, 50 },
  { 55, 45, 20 },
  { 65, 55, 10 },
  { 75, 65, 110 }
};

int num_default_levels = 5;

void fan_manager(useconds_t poll_interval, int num_levels, struct temp_level levels[]) {
  int level = 0;
  while (1) {
    unsigned char temp = read_temperature();
    printf("Current temperature: %d\n", temp);
    if (temp < levels[level].leave && level > 0) {
      printf("  Leaving level %d since the temperature is below %d\n", level, levels[level].leave);
      level--;
      printf("  New fan speed: %d\n", levels[level].fan_speed);
    } else if (level < num_levels-1 && temp > levels[level+1].enter) {
      printf("  Leaving level %d since the temperature is above %d\n", level, levels[level+1].enter);
      level++;
      printf("  New fan speed: %d\n", levels[level].fan_speed);
    } else {
      if (level > 0) {
        printf("  Lower bound: %d\n", levels[level].leave);
      }
      if (level < num_levels-1) {
        printf("  Upper bound: %d\n", levels[level+1].enter);
      }
    }
    // we always set the level since sometimes the EC or something
    // else seems to kick back in without our control
    set_fan_speed(levels[level].fan_speed);
    usleep(poll_interval);
  }
}

// main
// --------------------------------------------------------------------

void usage(void) {
  fprintf(stderr,
"x62-fancontrol temp\n"
"\tDisplays the current temperature.\n"
"\n"
"x62-fancontrol set-fan-speed <fan-speed>\n"
"\tSets the current fan speed. The EC will kick back in after\n"
"\ta few seconds.\n"
"\n"
"x62-fancontrol manager\n"
"\tManages the fan speed for you.\n");
  exit(0);
}

int main(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "temp")) {
    initialize();
    unsigned char temp = read_temperature();
    printf("Current temperature: %d\n", temp);
  }
  else if (argc == 3 && !strcmp(argv[1], "set-fan-speed")) {
    initialize();
    long int fan_speed_l = strtol(argv[2], NULL, 10);
    if (fan_speed_l < 0 || fan_speed_l > 255) {
      die("Invalid fan speed %s", argv[2]);
    } else {
      unsigned char fan_speed = (unsigned char)fan_speed_l;
      printf("Setting fan speed to %d\n", fan_speed);
      set_fan_speed(fan_speed);
    }
  }
  else if (argc == 2 && !strcmp(argv[1], "manager")) {
    initialize();
    fan_manager(1 * 1000 * 1000, num_default_levels, default_levels);
  }
  else {
    usage();
  }
}

