#ifndef LIBCAER_SRC_DVS128_H_
#define LIBCAER_SRC_DVS128_H_

#include "libcaer/devices/device_discover.h"
#include "libcaer/devices/dvs128.h"

#include "container_generation.h"
#include "data_exchange.h"
#include "usb_utils.h"

#define DVS_DEVICE_NAME "DVS128"

#define DVS_DEVICE_PID 0x8400

// #define DVS_REQUIRED_LOGIC_REVISION 1
#define DVS_REQUIRED_FIRMWARE_VERSION 14

#define DVS_ARRAY_SIZE_X 128
#define DVS_ARRAY_SIZE_Y 128

#define DVS_EVENT_TYPES 2

#define DVS_POLARITY_DEFAULT_SIZE 4096
#define DVS_SPECIAL_DEFAULT_SIZE 128

#define DVS_DATA_ENDPOINT 0x86

#define VENDOR_REQUEST_START_TRANSFER 0xB3
#define VENDOR_REQUEST_STOP_TRANSFER 0xB4
#define VENDOR_REQUEST_SEND_BIASES 0xB8
#define VENDOR_REQUEST_RESET_TS 0xBB
#define VENDOR_REQUEST_RESET_ARRAY 0xBD
#define VENDOR_REQUEST_TS_MASTER 0xBE

#define BIAS_NUMBER 12
#define BIAS_LENGTH 3

struct dvs128_state {
	// Per-device log-level
	atomic_uint_fast8_t deviceLogLevel;
	// Data Acquisition Thread -> Mainloop Exchange
	struct data_exchange dataExchange;
	// USB Device State
	struct usb_state usbState;
	// Timestamp fields
	struct {
		int32_t wrapOverflow;
		int32_t wrapAdd;
		int32_t last;
		int32_t current;
	} timestamps;
	// Packet Container state
	struct container_generation container;
	struct {
		// Polarity Packet State
		caerPolarityEventPacket polarity;
		int32_t polarityPosition;
		// Special Packet state
		caerSpecialEventPacket special;
		int32_t specialPosition;
	} currentPackets;
	struct {
		// Camera bias and settings memory (for getter operations)
		uint8_t biases[BIAS_NUMBER][BIAS_LENGTH];
		atomic_bool running;
		atomic_bool isMaster;
	} dvs;
};

typedef struct dvs128_state *dvs128State;

struct dvs128_handle {
	uint16_t deviceType;
	// Information fields
	struct caer_dvs128_info info;
	// State for data management.
	struct dvs128_state state;
};

typedef struct dvs128_handle *dvs128Handle;

ssize_t dvs128Find(caerDeviceDiscoveryResult *discoveredDevices);

caerDeviceHandle dvs128Open(
	uint16_t deviceID, uint8_t busNumberRestrict, uint8_t devAddressRestrict, const char *serialNumberRestrict);
bool dvs128Close(caerDeviceHandle handle);

bool dvs128SendDefaultConfig(caerDeviceHandle handle);
// Negative addresses are used for host-side configuration.
// Positive addresses (including zero) are used for device-side configuration.
bool dvs128ConfigSet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t param);
bool dvs128ConfigGet(caerDeviceHandle handle, int8_t modAddr, uint8_t paramAddr, uint32_t *param);

bool dvs128DataStart(caerDeviceHandle handle, void (*dataNotifyIncrease)(void *ptr),
	void (*dataNotifyDecrease)(void *ptr), void *dataNotifyUserPtr, void (*dataShutdownNotify)(void *ptr),
	void *dataShutdownUserPtr);
bool dvs128DataStop(caerDeviceHandle handle);
caerEventPacketContainer dvs128DataGet(caerDeviceHandle handle);

#endif /* LIBCAER_SRC_DVS128_H_ */
