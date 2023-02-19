#include "i2c_handler.h"

#include "aptina.h"
#include "aptina_i2c.h"

#include <nds.h>

void i2cFifoHandler(u32 value32, void *userdata) {
	switch(value32) {
		case CAM_INIT:
			init(I2C_CAM0);
			init(I2C_CAM1);
			fifoSendValue32(FIFO_CAMERA, aptReadRegister(I2C_CAM0, 0));
			break;
		case CAM0_ACTIVATE:
			activate(I2C_CAM0);
			fifoSendValue32(FIFO_CAMERA, CAM0_ACTIVATE);
			break;
		case CAM0_DEACTIVATE:
			deactivate(I2C_CAM0);
			fifoSendValue32(FIFO_CAMERA, CAM0_DEACTIVATE);
			break;
		case CAM1_ACTIVATE:
			activate(I2C_CAM1);
			fifoSendValue32(FIFO_CAMERA, CAM1_ACTIVATE);
			break;
		case CAM1_DEACTIVATE:
			deactivate(I2C_CAM1);
			fifoSendValue32(FIFO_CAMERA, CAM1_DEACTIVATE);
			break;
		case CAM_SET_MODE_PREVIEW:
			setMode(CAPTURE_MODE_PREVIEW);
			fifoSendValue32(FIFO_CAMERA, CAPTURE_MODE_PREVIEW);
			break;
		case CAM_SET_MODE_CAPTURE:
			setMode(CAPTURE_MODE_CAPTURE);
			fifoSendValue32(FIFO_CAMERA, CAPTURE_MODE_CAPTURE);
			break;
		default:
			break;
	}
}
