/*
 * Copyright (c) 2024 Titouan Christophe
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_USB_CLASS_USBD_MIDI_H_
#define ZEPHYR_INCLUDE_USB_CLASS_USBD_MIDI_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB MIDI 2.0 class device API
 * @defgroup usbd_midi2 USB MIDI 2.0 Class device API
 * @ingroup usb
 * @since 4.1
 * @version 0.2.0
 * @see midi20: "Universal Serial Bus Device Class Definition for MIDI Devices"
 *              Document Release 2.0 (May 5, 2020)
 * @{
 */

#include <zephyr/device.h>
#include <zephyr/audio/midi.h>

/**
 * @brief USB-MIDI interface status
 *
 * By default the class driver exposes both the legacy USB-MIDI 1.0 alternate
 * and the USB-MIDI 2.0 alternate during enumeration. Hosts that do not
 * understand MIDI 2.0 stay on the MIDI 1.0 alternate; MIDI 2.0 aware hosts
 * switch to the MIDI 2.0 alternate. The driver translates between USB-MIDI 1.0
 * event packets and Universal MIDI Packets transparently, so applications
 * always exchange UMPs through @ref usbd_midi_send and
 * @ref usbd_midi_ops.rx_packet_cb.
 *
 * Which alternates are exposed can be restricted at compile time via
 * :kconfig:option:`CONFIG_USBD_MIDI2_ALTSETTING_MIDI1` and
 * :kconfig:option:`CONFIG_USBD_MIDI2_ALTSETTING_MIDI2`, and at run time via
 * @ref usbd_midi_set_mode. When only a single alternate is exposed it is always
 * presented to the host as alternate setting 0.
 *
 * Applications can observe the interface status through
 * @ref usbd_midi_ops.status_changed_cb to know whether the interface is
 * available and which alternate the host has selected, in order to gate
 * features that have no MIDI 1.0 representation (for example MIDI 2.0 channel
 * voice messages with per-note attributes).
 */
enum usbd_midi_status {
	/** The interface is not available to exchange MIDI with the host. */
	USBD_MIDI_STATUS_DISABLED = 0,
	/** The interface is enabled on the legacy USB-MIDI 1.0 alternate. */
	USBD_MIDI_STATUS_MIDI1 = 1,
	/** The interface is enabled on the USB-MIDI 2.0 alternate. */
	USBD_MIDI_STATUS_MIDI2 = 2,
};

/**
 * @brief      MIDI2 application event handlers
 */
struct usbd_midi_ops {
	/**
	 * @brief Callback type for incoming Universal MIDI Packets from host
	 * @param[in]  dev   The MIDI2 device receiving the packet
	 * @param[in]  ump   The received packet in Universal MIDI Packet format
	 *
	 * Invoked for every UMP received from the host, regardless of which USB
	 * alternate the host selected. When the host is on the USB-MIDI 1.0
	 * alternate, the driver translates each USB-MIDI event packet into the
	 * equivalent UMP before invoking this callback.
	 */
	void (*rx_packet_cb)(const struct device *dev, const struct midi_ump ump);

	/**
	 * @brief Callback type for USB-MIDI interface status change
	 * @param[in]  dev     The MIDI2 device
	 * @param[in]  status  The new interface status
	 *
	 * Invoked whenever the interface becomes available or unavailable, or
	 * the host selects a different USB-MIDI alternate setting (including the
	 * initial selection at enumeration). A single callback reports both
	 * readiness and the active alternate: the interface is ready whenever
	 * @p status is not @ref USBD_MIDI_STATUS_DISABLED. Applications can use
	 * this to gate behaviour that has no MIDI 1.0 representation (such as
	 * MIDI 2.0 channel voice messages).
	 */
	void (*status_changed_cb)(const struct device *dev, enum usbd_midi_status status);
};

/**
 * @brief      Send a Universal MIDI Packet to the host
 * @param[in]  dev   The MIDI2 device
 * @param[in]  ump   The packet to send, in Universal MIDI Packet format
 *
 * When the host has selected the USB-MIDI 1.0 alternate setting, the driver
 * translates the supplied UMP into one or more USB-MIDI 1.0 event packets.
 * UMPs that have no MIDI 1.0 representation (for example
 * @ref UMP_MT_MIDI2_CHANNEL_VOICE) are rejected with `-ENOTSUP` while the
 * MIDI 1.0 alternate is active. Applications that need to know whether such
 * messages can be sent should track the interface status via
 * @ref usbd_midi_ops.status_changed_cb.
 *
 * @return     0 on success, all other values should be treated as error
 *             -EIO if the interface is not ready
 *             -ENOBUFS if there is no space in the TX buffer
 *             -ENOTSUP if the UMP cannot be represented on the active
 *             alternate setting
 *             -EINVAL if the UMP is malformed
 */
int usbd_midi_send(const struct device *dev, const struct midi_ump ump);

/**
 * @brief Set the application event handlers on a USB MIDI device
 * @param[in]  dev   The MIDI2 device
 * @param[in]  ops   The event handlers. Pass NULL to reset all callbacks
 */
void usbd_midi_set_ops(const struct device *dev, const struct usbd_midi_ops *ops);

/**
 * @brief Get the current status of the USB-MIDI interface
 * @param[in]  dev   The MIDI2 device
 * @return     The current interface status, reporting both whether the
 *             interface is available and which alternate the host has selected
 */
enum usbd_midi_status usbd_midi_get_status(const struct device *dev);

/**
 * @brief Configure which USB-MIDI alternate settings are exposed to the host
 *
 * Allows an application to expose only the USB-MIDI 1.0 alternate, only the
 * USB-MIDI 2.0 alternate, or both, at run time. When only a single alternate is
 * exposed it is presented to the host as alternate setting 0. An alternate can
 * only be enabled if it was also built in (see
 * :kconfig:option:`CONFIG_USBD_MIDI2_ALTSETTING_MIDI1` and
 * :kconfig:option:`CONFIG_USBD_MIDI2_ALTSETTING_MIDI2`).
 *
 * This API changes the reported descriptors, so it must be called while the USB
 * device stack is disabled (typically before the first @c usbd_enable), so the
 * host enumerates the chosen configuration.
 *
 * @param[in]  dev           The MIDI2 device
 * @param[in]  enable_midi1  True to expose the USB-MIDI 1.0 alternate setting
 * @param[in]  enable_midi2  True to expose the USB-MIDI 2.0 alternate setting
 *
 * @return 0 on success
 * @retval -ENOTSUP if an alternate is requested that was not built in
 * @retval -EINVAL if both alternates are disabled
 * @retval -EBUSY if the USB device is currently enabled
 */
int usbd_midi_set_mode(const struct device *dev, bool enable_midi1, bool enable_midi2);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
