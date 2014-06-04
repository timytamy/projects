This add-on is built to control any DMX device, including the Enttec DMX USB PRO
and 'simpler' devices. Eventually it should work on all systems supported by
openFrameworks itself. For now OSX and GNU/Linux are supported, although
preliminary support for Windows can be found in the w32-port branch on GitHub.

USAGE
-----
This add-on uses libftdi (1.0, unreleased) and libusbx (1.0.12), which have been included for OSX and GNU/Linux. Depending on your programming environment, you might need to add the relevant library and include paths to your project configuration. As reference please take a look at the example supplied with the add-on.

The included libftdi has been compiled from their git repository and is versioned as '1.0'. The reason for not using an actual release is that libftdi-1.0 and libusbx together are sufficient for this add-on whereas the latest official release of libftdi requires and older libusb which on OSX can only be emulated by yet another wrapper library.

CREDITS
-------
This add-on is based on the basic structure of the DMX512 add-on by Chris O'Shea and also uses code from that add-on for the USB PRO protocol. As second reference, the Enttec USB PRO example was used (see below in the links section).

OS SPECIFIC NOTES
-----------------
 * (OSX) If the add-on compiles, appears to open a device but does not send any data, it is likely you have the Ftdi VCP driver installed. This can be determined by checking if /dev/ttyUsbSerial exists. Note that (older) Arduino software also installs this driver.
   A temporary solution is to uninstall the driver (see Ftdi's installation guide: <http://www.ftdichip.com/Support/Documents/InstallGuides.htm>). If things still do not work, try rebooting your computer.
 * (Linux) By default, DMX controllers require root permissions; this can be fixed by using udev (see here: <https://github.com/libusbx/libusbx/wiki/FAQ#wiki-Can_I_run_Linux_libusbx_applications_without_root_privilege>). An example rules file for the Enttec DMX USB PRO has been included (scripts/scripts/75-permissions-enttec.rules), copy this file to /etc/udev/rules.d to use it.
 * (Windows/Linux) While setting up a Code::Blocks project, the libraries will have to be added to link against. Make sure they end up in the right order: first libftdi, then libusb-compat and finally libusbx.


MISCELLANEOUS NOTES
-------------------
 * DmxRawDevice is untested.
 * By lack of an RDM-capable device to test with, such features have not been added.
 * To recompile the libraries this add-on depends on, please see scripts/building-libs-howto.txt.
 * libftdi has a C++ wrapper; it depends on Boost however, so it isn't used in this add-on to avoid additional dependencies.

REFERENCES
----------
  * <http://forum.openframeworks.cc/index.php?&topic=3938.0>
  * <http://forum.openframeworks.cc/index.php?&topic=928.0>
  * Enttec USB PRO protocol specification: <http://www.enttec.com/docs/dmx_usb_pro_api_spec.pdf>
  * Enttec USB PRO / ftdi example: <http://www.enttec.com/download/pro_example_v2.zip>

TODO
----
see TODO file
