#include <libcaer/libcaer.h>
#include <libcaer/devices/dvs128.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>

static atomic_bool globalShutdown = ATOMIC_VAR_INIT(false);

static void globalShutdownSignalHandler(int signal) {
	// Simply set the running flag to false on SIGTERM and SIGINT (CTRL+C) for global shutdown.
	if (signal == SIGTERM || signal == SIGINT) {
		atomic_store(&globalShutdown, true);
	}
}

static void usbShutdownHandler(void *ptr) {
	(void) (ptr); // UNUSED.

	atomic_store(&globalShutdown, true);
}

int main(void) {
// Install signal handler for global shutdown.
#if defined(_WIN32)
	if (signal(SIGTERM, &globalShutdownSignalHandler) == SIG_ERR) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		return (EXIT_FAILURE);
	}

	if (signal(SIGINT, &globalShutdownSignalHandler) == SIG_ERR) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGINT. Error: %d.", errno);
		return (EXIT_FAILURE);
	}
#else
	struct sigaction shutdownAction;

	shutdownAction.sa_handler = &globalShutdownSignalHandler;
	shutdownAction.sa_flags   = 0;
	sigemptyset(&shutdownAction.sa_mask);
	sigaddset(&shutdownAction.sa_mask, SIGTERM);
	sigaddset(&shutdownAction.sa_mask, SIGINT);

	if (sigaction(SIGTERM, &shutdownAction, NULL) == -1) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGTERM. Error: %d.", errno);
		return (EXIT_FAILURE);
	}

	if (sigaction(SIGINT, &shutdownAction, NULL) == -1) {
		caerLog(CAER_LOG_CRITICAL, "ShutdownAction", "Failed to set signal handler for SIGINT. Error: %d.", errno);
		return (EXIT_FAILURE);
	}
#endif

	// Open a DVS128, give it a device ID of 1, and don't care about USB bus or SN restrictions.
	caerDeviceHandle dvs128_handle = caerDeviceOpen(1, CAER_DEVICE_DVS128, 0, 0, NULL);
	if (dvs128_handle == NULL) {
		return (EXIT_FAILURE);
	}

	// Let's take a look at the information we have on the device.
	struct caer_dvs128_info dvs128_info = caerDVS128InfoGet(dvs128_handle);

	printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Firmware: %d.\n", dvs128_info.deviceString,
		dvs128_info.deviceID, dvs128_info.deviceIsMaster, dvs128_info.dvsSizeX, dvs128_info.dvsSizeY,
		dvs128_info.firmwareVersion);

	// Send the default configuration before using the device.
	// No configuration is sent automatically!
	caerDeviceSendDefaultConfig(dvs128_handle);

	// Tweak some biases, to increase bandwidth in this case.
	caerDeviceConfigSet(dvs128_handle, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PR, 695);
	caerDeviceConfigSet(dvs128_handle, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_FOLL, 867);

	// Let's verify they really changed!
	uint32_t prBias, follBias;
	caerDeviceConfigGet(dvs128_handle, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_PR, &prBias);
	caerDeviceConfigGet(dvs128_handle, DVS128_CONFIG_BIAS, DVS128_CONFIG_BIAS_FOLL, &follBias);

	printf("New bias values --- PR: %d, FOLL: %d.\n", prBias, follBias);

	// Now let's get start getting some data from the device. We just loop in blocking mode,
	// no notification needed regarding new events. The shutdown notification, for example if
	// the device is disconnected, should be listened to.
	caerDeviceDataStart(dvs128_handle, NULL, NULL, NULL, &usbShutdownHandler, NULL);

	// Let's turn on blocking data-get mode to avoid wasting resources.
	caerDeviceConfigSet(dvs128_handle, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);

	while (!atomic_load_explicit(&globalShutdown, memory_order_relaxed)) {
		caerEventPacketContainer packetContainer = caerDeviceDataGet(dvs128_handle);
		if (packetContainer == NULL) {
			continue; // Skip if nothing there.
		}

		int32_t packetNum = caerEventPacketContainerGetEventPacketsNumber(packetContainer);

		printf("\nGot event container with %d packets (allocated).\n", packetNum);

		for (int32_t i = 0; i < packetNum; i++) {
			caerEventPacketHeader packetHeader = caerEventPacketContainerGetEventPacket(packetContainer, i);
			if (packetHeader == NULL) {
				printf("Packet %d is empty (not present).\n", i);
				continue; // Skip if nothing there.
			}

			printf("Packet %d of type %d -> size is %d.\n", i, caerEventPacketHeaderGetEventType(packetHeader),
				caerEventPacketHeaderGetEventNumber(packetHeader));

			// Packet 0 is always the special events packet for DVS128, while packet is the polarity events packet.
			if (i == POLARITY_EVENT) {
				caerPolarityEventPacket polarity = (caerPolarityEventPacket) packetHeader;

				// Get full timestamp and addresses of first event.
				caerPolarityEvent firstEvent = caerPolarityEventPacketGetEvent(polarity, 0);

				int32_t ts = caerPolarityEventGetTimestamp(firstEvent);
				uint16_t x = caerPolarityEventGetX(firstEvent);
				uint16_t y = caerPolarityEventGetY(firstEvent);
				bool pol   = caerPolarityEventGetPolarity(firstEvent);

				printf("First polarity event - ts: %d, x: %d, y: %d, pol: %d.\n", ts, x, y, pol);
			}
		}

		caerEventPacketContainerFree(packetContainer);
	}

	caerDeviceDataStop(dvs128_handle);

	caerDeviceClose(&dvs128_handle);

	printf("Shutdown successful.\n");

	return (EXIT_SUCCESS);
}
