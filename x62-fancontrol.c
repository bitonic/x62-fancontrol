#include <sys/io.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pci/pci.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

void die(char *msg, ...) {
  va_list args;
  va_start(args, msg);
  fprintf(stderr, "x62-fancontrol: ");
  vfprintf(stderr, msg, args);
  fputc('\n', stderr);
  exit(1);
}

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

char read_temperature(void) {
  printf("Reading temperature\n");
  set_0x6C(0x44);
  outb(0x00, 0x68);
  wait_0x6C_first_bit_set();
  return inb(0x68);
}

void set_fan_speed(unsigned char fan_speed) {
  set_0x6C(0x55);
  outb(fan_speed, 0x68);
}

int main(void) {
  initialize();
  unknown_communication();
  char temp = read_temperature();
  printf("Current temperature: %d\n", temp);
  return 0;
}
