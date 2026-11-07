# Design references

- [GNOME keyboard guidance](https://developer.gnome.org/hig/guidelines/keyboard.html): consistent focus, Tab, Enter, Escape, and keyboard access to primary views.
- [GNOME typography](https://developer.gnome.org/hig/guidelines/typography.html): clear hierarchy, restrained type sizes, readable text on solid backgrounds.
- [GNOME UI styling](https://developer.gnome.org/hig/guidelines/ui-styling.html): status has text labels as well as color; long-use developer workspace uses a dark palette.
- [Linux cfg80211](https://docs.kernel.org/driver-api/80211/cfg80211.html): standard nl80211 wireless control through iw and wpa_supplicant.
- [Linux wireless drivers](https://wireless.docs.kernel.org/en/latest/en/users/drivers.html): ath9k PCI/PCIe and RTL8187 USB driver selection.
- [Linux framebuffer API](https://www.kernel.org/doc/Documentation/fb/): framebuffer format discovery, stride-aware rendering, and Linux input events.

The desktop displays metrics read from training output, network addresses read
from the system, and free capacity read from the mounted filesystem.

The updated UI uses warm neutral surfaces, a restrained accent, a model picker
beside chat, and separate project configuration and system controls. References:
- https://www.assistant-ui.com/examples/claude
- https://aiuxplayground.com/teardowns/claude/composer
