This document is for internal use inside Amazon only.

### Overview
UFBL provides collection of Amazon bootloader features
#### IDME
Amazon global variables such as mac address and unlock code etc
#### Secure boot
Unified architecture to perform secure boot on kernel image
#### Unlock
Unlock the production device
#### BCB
Boot control block to perform ping pong OTA

UFBL code is integrated with bootloader code to take advantage of unified features.

### Architecture
`ufbl_features` contains subdirectories in `ufbl_features/features`
which have UFBL feature implementation and Makefiles to assist in integration of individual UFBL components.
`ufbl_features/project` contains the makefile which is used to integrate UFBL features selected by feature selection flags.
Linkage of supporting external libraries is provided by placing a symlink to external code in the `ufbl_features/features`
which is also chosen with feature selection flags in `ufbl_features/project`
`ufbl_features/platform` contains the supporting code resulting out of missing standard lib
function definitions in bootloader code which are needed by supporting libraries (toupper etc.).
`ufbl_features/tests` contains the test tools which are instrumental in testing the UFBL features on device or host.
### Sub Components
#### IDME
IDME code contains the functions needed to be called in bootloader after a persistent memory store (such as eMMC) is intialized.
IDME data is read from a default product specific header file which provides initial values to the device global variables.
IDME also provides on the fly table update in eMMC with newly added IDME variables which creates new field in the IDME table on eMMC.

#### Secure Boot
Secure boot feature in UFBL provides secure boot design of second stage boot loader authentication of kernel image. Kenel Image is signed with
private key and bootloader authenticates the same with public key. Public key is stored as part of the code (hardcoded value). If the image is not
authenticated the system hangs at bootloader stage. Secure boot uses external libraries OpenSSL or Libtomcrypt+Libtommath currently to provide
cryptographic functions (this list is not complete and may change in future if more suitable library is found to replace any one or all of these).

#### Unlock
Unlock contains common (reusable) code of unlock infrastructure provided on Amazon devices. It basically contains bootloader functions to read
the unlock code from devices (IDME get var for e.g.), functions to verify the authenticity of unlock code etc.

#### BCB
Bootloader control block contains the functions needed by ping pong OTA to choose the appropriate partition and also to provide bootloader
commandline commands or fastboot command response to test and facilitate the partition choosing process.

#### USB Update Signature Verification
Perform signature verification on usb update package: update.zip.  Provides wrapper API's for the bootloader to hash the update.zip, and
verify the update package using their public key.  This feature depends on libtomcrypt+libtommath, so the target project must include those
libraries as well.
