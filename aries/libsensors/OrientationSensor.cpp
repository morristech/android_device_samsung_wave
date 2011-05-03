/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>


#include "OrientationSensor.h"


/*****************************************************************************/
OrientationSensor::OrientationSensor()
    : SensorBase(NULL, "orientation_sensor"),
      mEnabled(0),
      mInputReader(4),
      mHasPendingEvent(false)
{
    LOGD("OrientationSensor::OrientationSensor()");
    mPendingEvent.version = sizeof(sensors_event_t);
    mPendingEvent.sensor = ID_O;
    mPendingEvent.type =  SENSOR_TYPE_ORIENTATION;
    memset(mPendingEvent.data, 0, sizeof(mPendingEvent.data));
    
    LOGD("OrientationSensor::OrientationSensor() open data_fd");
	
    if (data_fd) {
        strcpy(input_sysfs_path, "/sys/class/input/");
        strcat(input_sysfs_path, input_name);
        strcat(input_sysfs_path, "/device/");
        input_sysfs_path_len = strlen(input_sysfs_path);

        enable(0, 1);
    }
}

OrientationSensor::~OrientationSensor() {

    LOGD("OrientationSensor::~OrientationSensor()");
    if (mEnabled) {
        enable(0, 0);
    }
}


int OrientationSensor::enable(int32_t, int en) {

	   
    LOGD("OrientationSensor::~enable(0, %d)", en);
    int flags = en ? 1 : 0;
    if (flags != mEnabled) {
        int fd;
        strcpy(&input_sysfs_path[input_sysfs_path_len], "enable");
        LOGD("OrientationSensor::~enable(0, %d) open %s",en,  input_sysfs_path);
        fd = open(input_sysfs_path, O_RDWR);
        if (fd >= 0) {
             LOGD("OrientationSensor::~enable(0, %d) opened %s",en,  input_sysfs_path);
            char buf[2];
            int err;
            buf[1] = 0;
            if (flags) {
                buf[0] = '1';
            } else {
                buf[0] = '0';
            }
            err = write(fd, buf, sizeof(buf));
            close(fd);
            mEnabled = flags;
            //setInitialState();
            return 0;
        }
        return -1;        
    }
    return 0;
}


bool OrientationSensor::hasPendingEvents() const {
    /* FIXME probably here should be returning mEnabled but instead
	mHasPendingEvents. It does not work, so we cheat.*/
    //LOGD("OrientationSensor::~hasPendingEvents %d", mHasPendingEvent ? 1 : 0 );
    return mHasPendingEvent;
}


int OrientationSensor::setDelay(int32_t handle, int64_t ns)
{
    LOGD("OrientationSensor::~setDelay(%d, %d)", handle, ns);
    /* FIXME needs changes to the kernel driver.
       We need to add a IOCTL that can set the samplingrate
       the driver in ther kernel supports this allready only need
       to add a IOCTL on both sides for that*/
    return 0;
}


int OrientationSensor::readEvents(sensors_event_t* data, int count)
{
    //LOGD("OrientationSensor::~readEvents() %d", count);
    if (count < 1)
        return -EINVAL;
        
    if (mHasPendingEvent) {
        mHasPendingEvent = false;
        mPendingEvent.timestamp = getTimestamp();
        *data = mPendingEvent;
        return mEnabled ? 1 : 0;
    }
        
    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;
	
    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_REL) {
            float value = event->value;
            if (event->code == EVENT_TYPE_YAW) {
                mPendingEvent.orientation.azimuth = value * CONVERT_O_A;
            } else if (event->code == EVENT_TYPE_PITCH) {
                mPendingEvent.orientation.pitch = value * CONVERT_O_P;
            } else if (event->code == EVENT_TYPE_ROLL) {
                mPendingEvent.orientation.roll = value * CONVERT_O_R;
            }
        } else if (type == EV_SYN) {
            mPendingEvent.timestamp = timevalToNano(event->time);
            if (mEnabled) {
                *data++ = mPendingEvent;
                count--;
                numEventReceived++;
            }
        } else {
            LOGE("OrientationSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }
 
	//LOGD("OrientationSensor::~readEvents() numEventReceived = %d", numEventReceived);
	return numEventReceived++;
		
}
