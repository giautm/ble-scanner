/*
  BLE-Scanner

  (c) 2020 Christian.Lorenz@gromeck.de

  module to handle the BLE scan


  This file is part of BLE-Scanner.

  BLE-Scanner is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  BLE-Scanner is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with BLE-Scanner.  If not, see <https://www.gnu.org/licenses/>.

*/

#include "config.h"
#include "state.h"
#include "bluetooth.h"
#include "mqtt.h"
#include "ble-manufacturer.h"
#include "scandev.h"
#include "util.h"

static BLEScan *_scan = NULL;
static time_t _last_scan = 0;
static time_t _last_activescan = 0;

class BLEScannerAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
#if DBG_BT
      DbgMsg("BLE: found advertised device: %s  address type: 0x%02x", advertisedDevice.getAddress().toString().c_str(), advertisedDevice.getAddressType());
#endif

      /*
         we only put devices onto the list, which don't use random addresses
      */
      if (advertisedDevice.getAddressType() == BLE_ADDR_TYPE_PUBLIC) {
        /*
           check the service UUIDs
        */
        bool hasBatteryService = false;

        for (int n = 0; n < advertisedDevice.getServiceUUIDCount(); n++) {
          hasBatteryService = (hasBatteryService || advertisedDevice.getServiceUUID(n).equals(BLEBatteryService));
        }

        /*
           set the manufacturer ids
        */
        uint16_t manufacturer_id = BLE_MANUFACTURER_ID_UNKNOWN;

        if (advertisedDevice.haveManufacturerData())
          advertisedDevice.getManufacturerData().copy((char *) &manufacturer_id, 2, 0);

        ScanDevAdd(advertisedDevice.getAddress(),
                   advertisedDevice.getName().c_str(),
                   manufacturer_id,
                   advertisedDevice.getRSSI(),
                   hasBatteryService);

      }
    }
};

/*
   setup
*/
void BluetoothSetup(void)
{
  /*
     check and correct the config
  */
  FIX_RANGE(_config.bluetooth.scan_time, BLUETOOTH_SCAN_TIME_MIN, BLUETOOTH_SCAN_TIME_MAX);
  FIX_RANGE(_config.bluetooth.pause_time, BLUETOOTH_PAUSE_TIME_MIN, BLUETOOTH_PAUSE_TIME_MAX);
  FIX_RANGE(_config.bluetooth.activescan_timeout, BLUETOOTH_ACTIVESCAN_TIMEOUT_MIN, BLUETOOTH_ACTIVESCAN_TIMEOUT_MAX);
  FIX_RANGE(_config.bluetooth.absence_cycles, BLUETOOTH_ABSENCE_CYCLES_MIN, BLUETOOTH_ABSENCE_CYCLES_MAX);
  _config.bluetooth.publish_absence = _config.bluetooth.publish_absence ? true : false;
  FIX_RANGE(_config.bluetooth.publish_timeout, BLUETOOTH_PUBLISH_TIMEOUT_MIN, BLUETOOTH_PUBLISH_TIMEOUT_MAX);
  FIX_RANGE(_config.bluetooth.battcheck_timeout, BLUETOOTH_BATTCHECK_TIMEOUT_MIN, BLUETOOTH_BATTCHECK_TIMEOUT_MAX);

  /*
     set the timeout values in the status table
  */
  LogMsg("BLE: setting up timout values in the status table");
  StateModifyTimeout(STATE_SCANNING, (_config.bluetooth.scan_time + 5) * 1000);
  StateModifyTimeout(STATE_PAUSING, _config.bluetooth.pause_time * 1000);

#if DBG_BT
  DbgMsg("BLE: init ...");
#endif

  /*
     init the device
  */
  if (!BLEDevice::getInitialized())
    BLEDevice::init(__TITLE__);

  /*
     create a scan
  */
#if DBG_BT
  DbgMsg("BLE: create a scan ...");
#endif
  if (!_scan && !(_scan = BLEDevice::getScan())) {
    LogMsg("BLE: BLEDevice::getScan() failed");
  }
  _scan->setAdvertisedDeviceCallbacks(new BLEScannerAdvertisedDeviceCallbacks(), false, true);
}

/*
   cyclic call
*/
void BluetoothUpdate(void)
{
}

/*
   init the scanning
*/
bool BluetoothScanStart(void)
{
#if DBG_BT
  DbgMsg("BLE: BluetoothScanStart");
#endif

  /*
    active/passive scan
  */
  bool active = false;

  if (now() - _last_activescan > _config.bluetooth.activescan_timeout) {
    active = true;
    _last_activescan = now();
  }
  _scan->setActiveScan(active);

  int scan_interval = 3000; // ms
  _scan->setInterval(scan_interval);
  _scan->setWindow(scan_interval - 1);

  /*
     start the scan
  */
#if DBG_BT
  DbgMsg("BLE: start %s scan ...", (active) ? "active" : "passive");
#endif
  _scan->start(_config.bluetooth.scan_time, false);
  _last_scan = now();

  return true;
}

/*
   stop the current scan
*/
bool BluetoothScanStop(void)
{
#if DBG_BT
  DbgMsg("BLE: BluetoothScanStop");
#endif
  _scan->stop();
  _scan->clearResults();

  return true;
}

/*
   callback class to connect to the device
*/
class BLEScannerClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient *client) {
    }
    void onDisconnect(BLEClient* pclient) {
    }
};

/*
   get the charactersitics from the battery service
*/
bool BluetoothBatteryCheck(BLEAddress device, uint8_t *battery_level)
{
#if DBG_BT
  DbgMsg("BLE: create a client ...");
#endif
  BLEClient *client = BLEDevice::createClient();

#if DBG_BT
  DbgMsg("BLE: connect device %s ...", device.toString().c_str());
#endif
  client->setClientCallbacks(new BLEScannerClientCallbacks());
  if (!client->connect(device)) {
    LogMsg("BLE: couldn't connect to client device %s to read battery level", device.toString().c_str());
    return false;
  }

  /*
     select the service
  */
#if DBG_BT
  DbgMsg("BLE: create remote service for battery service ...");
#endif
  BLERemoteService *service = client->getService(BLEBatteryService);

  if (!service) {
    LogMsg("BLE: couldn't create service for client device %s to read battery level", device.toString().c_str());
    client->disconnect();
    return false;
  }

  /*
     get the characteristic
  */
#if DBG_BT
  DbgMsg("BLE: get characteristics ...");
#endif
  BLERemoteCharacteristic *characteristic = service->getCharacteristic(BLEBatteryCharacteristics);

  if (!characteristic) {
    LogMsg("BLE: couldn't create characteristics for client device %s to read battery level", device.toString().c_str());
    client->disconnect();
    return false;
  }
  if (characteristic->canRead() && battery_level) {
#if DBG_BT
    DbgMsg("BLE: reading characteristic");
#endif
    *battery_level = characteristic->readUInt8();
#if DBG_BT
    DbgMsg("BLE: characteristic value=%u", *battery_level);
#endif
  }

#if DBG_BT
  DbgMsg("BLE: disconnecting device");
#endif
  client->disconnect();

  return true;
}/**/
