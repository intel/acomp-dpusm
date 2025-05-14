# Installation Guide for Acompress Provider

This document provides step-by-step instructions for installing and configuring the Acompress Provider for DPUSM and ZFS.

## Prerequisites

Before you begin, ensure that you have the following:

- A system with Linux Kernel support for the Crypto API's acomp interface and zlib-deflate (versions 6.3 to 6.6).
- Administrative access to the system for installation and configuration tasks.
- Setup top level source directory.

   ```bash
   export HOME_PATH=/devel/zfs
   sudo mkdir -p $HOME_PATH
   sudo chown -R $USER:$USER $HOME_PATH
   ```

## Component 1: DPUSM

### Download DPUSM

1. Clone the DPUSM repository:

   ```bash
   cd $HOME_PATH
   git clone https://github.com/hpc/dpusm
   cd dpusm
   ```

### Build and Install DPUSM

2. Build DPUSM

   ```bash
   make
   ```

3. Load the module:

   ```bash
   sudo insmod dpusm.ko
   ```

4. Verify module is loaded:

   ```bash
   lsmod | grep dpusm
   ```
   
   Example output:

   ```bash
   lsmod | grep dpusm
   dpusm                  36864  0
   ```
 
## Component 2: ZFS with Z.I.A.

### Obtain ZFS with Z.I.A.

To use ZFS with the ZFS Interface for Accelerators (Z.I.A.), you need to obtain the specific branch that includes Z.I.A. support. Follow these steps:

1. Clone the repository with the Z.I.A.:

   ```bash
   cd $HOME_PATH
   git clone https://github.com/hpc/zfs.git
   cd zfs
   ```
   
### Build and Install ZFS

1. Build ZFS

   Notes:  If DPUSM is installed to another directory, update the following command accordingly.

   ```bash
   ./autogen.sh
   ./configure --with-zia=$HOME_PATH/dpusm
   make -j$(nproc)
   ```

3. Install ZFS:

   ```bash
   sudo make install
   ```

4. Load ZFS modules:

   ```bash
   sudo modprobe zfs
   ```

5. Verify ZFS with Z.I.A was loaded:

   ```bash
   sudo dmesg | grep -E "Z\.I\.A|ZFS"
   ```
   
   The output will look like:

      ```bash
      user@server:~/zfs$ sudo dmesg | grep -E "Z\.I\.A|ZFS"
      [59203.606953] Z.I.A. initialized (0000000029342ebc)
      [59204.818383] ZFS: Loaded module v2.3.99-313_g325a5e241, ZFS pool version 5000, ZFS filesystem version 5
      ```

## Component 3: Acompress Provider

### Clone the Acompress Provider Repository

1. Clone the Acompress Provider repository:

   ```bash
   cd $HOME_PATH
   git clone https://github.com/intel/acomp-dpusm.git
   cd acomp-dpusm
   ```

### Build and Install Provider:

1. Install the Acompress Provider:

    ```bash
    make
    ```

3. Install the Provider:

    ```bash
    sudo insmod acomp-dpusm.ko
    ```

4. Verify provided was loaded:

   ```bash
   sudo dmesg | grep acomp_dpusm
   ```

   Example output:

      ```bash
      user@server:~/acomp-dpusm$ sudo dmesg | grep acomp_dpusm
      [62503.851101] Provider acomp_dpusm supports GZIP Level 1 Compress
      [62503.851112] Provider acomp_dpusm supports GZIP Level 2 Compress
      [62503.851114] Provider acomp_dpusm supports GZIP Level 3 Compress
      [62503.851116] Provider acomp_dpusm supports GZIP Level 4 Compress
      [62503.851117] Provider acomp_dpusm supports GZIP Level 5 Compress
      [62503.851118] Provider acomp_dpusm supports GZIP Level 6 Compress
      [62503.851119] Provider acomp_dpusm supports GZIP Level 7 Compress
      [62503.851120] Provider acomp_dpusm supports GZIP Level 8 Compress
      [62503.851121] Provider acomp_dpusm supports GZIP Level 9 Compress
      [62503.851122] Provider acomp_dpusm supports GZIP Level 1 Decompress
      [62503.851123] Provider acomp_dpusm supports GZIP Level 2 Decompress
      [62503.851124] Provider acomp_dpusm supports GZIP Level 3 Decompress
      [62503.851125] Provider acomp_dpusm supports GZIP Level 4 Decompress
      [62503.851126] Provider acomp_dpusm supports GZIP Level 5 Decompress
      [62503.851127] Provider acomp_dpusm supports GZIP Level 6 Decompress
      [62503.851128] Provider acomp_dpusm supports GZIP Level 7 Decompress
      [62503.851129] Provider acomp_dpusm supports GZIP Level 8 Decompress
      [62503.851130] Provider acomp_dpusm supports GZIP Level 9 Decompress
      [62503.851131] dpusm_provider_register: DPUSM Provider "acomp_dpusm" (00000000863c55ce) added. Now 1 providers registered.
      ```

## Runtime Configuration

Ensure the zpools are configured to use the provider and enable compression/decompression.  The operations would look like:

   ```bash
   sudo $HOME_PATH/zfs/zpool create -f test_zpool /dev/sda
   sudo $HOME_PATH/zfs/zpool set zia_provider=acomp_dpusm test_zpool
   sudo $HOME_PATH/zfs/zpool zia_compress=on test_zpool
   sudo $HOME_PATH/zfs/zpool zia_decompress=on test_zpool
   sudo $HOME_PATH/zfs/zfs set compression=gzip test_zpool
   ```
