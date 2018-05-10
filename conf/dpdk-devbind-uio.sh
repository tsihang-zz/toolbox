#!/bin/bash

dpdk-devbind.py -b vfio-pci  05:00.2
dpdk-devbind.py -b vfio-pci  05:00.3
