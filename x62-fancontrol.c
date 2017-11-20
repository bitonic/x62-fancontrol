#include <sys/io.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pci/pci.h>

void get_perms(void) {
  ioperm(0x4E, 2, 1); // 78 and 79
  ioperm(0x68, 1, 1); // 104
  ioperm(0x6C, 1, 1); // 108
}

void initialize_pci(void) {
  // initialize pci_access
  struct pci_access *pacc;
  pacc = pci_alloc();
  pci_init(pacc);
  pci_scan_bus(pacc);

  // initialize filter
  struct pci_filter filter;
  pci_filter_init(pacc, &filter);
  char *err = pci_filter_parse_id(&filter, "8086:9cc3");
  if (err) {
    printf("Could not parse filter: %s\n", err);
    exit(1);
  }

  // match the device
  struct pci_dev *device = NULL;
  struct pci_dev *current_device = pacc->devices;
  while (current_device) {
    if (pci_filter_match(&filter, current_device)) {
      if (device) {
        printf("Matched multiple devices!\n");
        exit(1);
      }
      device = current_device;
    }
    current_device = current_device->next;
  }
  if (!device) {
    printf("Could not match any device!\n");
    exit(1);
  }

  // set the required config
  pci_write_long(device, 0x84, 0x40069);

  // cleanup
  pci_cleanup(pacc);
}

void initialize(void) {
  // get the permissions
  get_perms();
  // PCI
  initialize_pci();
  // ports
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
  int unset = (inb(0x6C) & 2) == 2;
  while (counter <= 1000 && !unset) {
    counter++;
    usleep(1000); // 1ms
    unset = (inb(0x6C) & 2) == 2;
  }
  if (!unset) {
    printf("The second bit of 0x6C didn't reset!\n");
    exit(1);
  }
}

void wait_0x6C_first_bit_set(void) {
  int counter = 0;
  int set = !(inb(0x6C) & 1);
  while (counter <= 1000 && !set) {
    counter++;
    usleep(1000); // 1ms
    set = !(inb(0x6C) & 1);
  }
  if (!set) {
    printf("The first bit of 0x6C didn't get set!\n");
    exit(1);
  }
}

void set_0x6C(unsigned char val) {
  wait_0x6C_second_bit_unset();
  outb(val, 0x6C);
  wait_0x6C_second_bit_unset();
}

void unknown_communication(void) {
  set_0x6C(0x33);
  outb(0x06, 0x68);
}

char read_temperature(void) {
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
  printf("Initializing\n");
  initialize();
  printf("unknown_communication\n");
  unknown_communication();
  printf("Reading temperature\n");
  char temp = read_temperature();
  printf("Current temperature: %d\n", temp);
  return 0;
}
