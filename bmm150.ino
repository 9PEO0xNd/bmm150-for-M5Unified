/*
 * @Author: Sorzn
 * @Date: 2019-11-21 19:58:40
 * @LastEditTime: 2019-11-22 20:36:35
 * @Description: M5Stack project
 * @FilePath: /bmm150/bmm150.ino
 */

//#include <SD.h>
#include <Wire.h>

#include <Preferences.h>
#include <M5Unified.h>
#include <math.h>
#include "bmm150.h"
#include "bmm150_defs.h"
//#include <M5StackUpdater.h>

Preferences prefs;

struct bmm150_dev dev;
bmm150_mag_data mag_offset;
bmm150_mag_data mag_max;
bmm150_mag_data mag_min;

int8_t i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{
  Wire.beginTransmission(dev_id);
  Wire.write(reg_addr);
  uint8_t i = 0;
  if (Wire.endTransmission(false) == 0 &&
    Wire.requestFrom(dev_id, (uint8_t)len)) {
    while (Wire.available()) {
      read_data[i++] = Wire.read();  // Put read results in the Rx buffer
    }
    return BMM150_OK;
  }
  else
  {
    return BMM150_E_DEV_NOT_FOUND;
  }
}

int8_t i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *read_data, uint16_t len)
{
  Wire.beginTransmission(dev_id);
  Wire.write(reg_addr);
  for (int i = 0; i < len; i++) {
        Wire.write(*(read_data + i));  // Put data in Tx buffer
  }
  if(Wire.endTransmission(false)==0)
  {
    return BMM150_OK;
  }
  else
  {
    return BMM150_E_DEV_NOT_FOUND;
  }
}

int8_t bmm150_initialization()
{
    int8_t rslt = BMM150_OK;

    /* Sensor interface over SPI with native chip select line */
    dev.dev_id = 0x10;
    dev.intf = BMM150_I2C_INTF;
    dev.read = i2c_read;
    dev.write = i2c_write;
    dev.delay_ms = delay;

    /* make sure max < mag data first  */
    mag_max.x = -2000;
    mag_max.y = -2000;
    mag_max.z = -2000;

    /* make sure min > mag data first  */
    mag_min.x = 2000;
    mag_min.y = 2000;
    mag_min.z = 2000;

    rslt = bmm150_init(&dev);
    dev.settings.pwr_mode = BMM150_NORMAL_MODE;
    rslt |= bmm150_set_op_mode(&dev);
    dev.settings.preset_mode = BMM150_PRESETMODE_ENHANCED;
    rslt |= bmm150_set_presetmode(&dev);
    return rslt;
}

void bmm150_offset_save()
{
    prefs.begin("bmm150", false);
    prefs.putBytes("offset", (uint8_t *)&mag_offset, sizeof(bmm150_mag_data));
    prefs.end();
}

void bmm150_offset_load()
{
    if(prefs.begin("bmm150", true))
    {
        prefs.getBytes("offset", (uint8_t *)&mag_offset, sizeof(bmm150_mag_data));
        prefs.end();
        Serial.printf("bmm150 load offset finish.... \r\n");
    }
    else
    {
        Serial.printf("bmm150 load offset failed.... \r\n");
    }
}

void initScreen()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(textdatum_t::middle_left);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::DejaVu18);
}

void setup() 
{
//    M5.begin(true, false, true, false);
  auto cfg = M5.config();

  cfg.clear_display = true;  // default=true. clear the screen when begin.
  cfg.led_brightness = 128;   // default= 0. system LED brightness (0=off / 255=max) (※ not NeoPixel)

  M5.begin(cfg);
//  checkSDUpdater(SD);
 
  Wire.begin(21, 22, 400000UL);

  initScreen();

  if(bmm150_initialization() != BMM150_OK)
  {
    M5.Display.drawCentreString("BMM150 init failed", 160, 110);
    for(;;)
    {
      delay(100);
    }
  }

  bmm150_offset_load();
}

void bmm150_calibrate(uint32_t calibrate_time)
{
    uint32_t calibrate_timeout = 0;

    calibrate_timeout = millis() + calibrate_time;
    Serial.printf("Go calibrate, use %d ms \r\n", calibrate_time);
    Serial.printf("running ...");

    while (calibrate_timeout > millis())
    {
        bmm150_read_mag_data(&dev);
        if(dev.data.x)
        {
            mag_min.x = (dev.data.x < mag_min.x) ? dev.data.x : mag_min.x;
            mag_max.x = (dev.data.x > mag_max.x) ? dev.data.x : mag_max.x;
        }

        if(dev.data.y)
        {
            mag_max.y = (dev.data.y > mag_max.y) ? dev.data.y : mag_max.y;
            mag_min.y = (dev.data.y < mag_min.y) ? dev.data.y : mag_min.y;
        }

        if(dev.data.z)
        {
            mag_min.z = (dev.data.z < mag_min.z) ? dev.data.z : mag_min.z;
            mag_max.z = (dev.data.z > mag_max.z) ? dev.data.z : mag_max.z;
        }
        delay(100);
    }

    mag_offset.x = (mag_max.x + mag_min.x) / 2;
    mag_offset.y = (mag_max.y + mag_min.y) / 2;
    mag_offset.z = (mag_max.z + mag_min.z) / 2;
    bmm150_offset_save();

    Serial.printf("\n calibrate finish ... \r\n");
    Serial.printf("mag_max.x: %.2f x_min: %.2f \t", mag_max.x, mag_min.x);
    Serial.printf("y_max: %.2f y_min: %.2f \t", mag_max.y, mag_min.y);
    Serial.printf("z_max: %.2f z_min: %.2f \r\n", mag_max.z, mag_min.z);
}

void loop() 
{
    char text_string[100];
    M5.update();
    bmm150_read_mag_data(&dev);
    float head_dir = atan2(dev.data.x -  mag_offset.x, dev.data.y - mag_offset.y) * 180.0 / M_PI;
    Serial.printf("Magnetometer data, heading %.2f\n", head_dir);
    Serial.printf("MAG X : %.2f \t MAG Y : %.2f \t MAG Z : %.2f \n", dev.data.x, dev.data.y, dev.data.z);
    Serial.printf("MID X : %.2f \t MID Y : %.2f \t MID Z : %.2f \n", mag_offset.x, mag_offset.y, mag_offset.z);
    
    sprintf(text_string, "MAG X: %.2f  ", dev.data.x);
    M5.Display.drawString(text_string, 10, 20);
    sprintf(text_string, "MAG Y: %.2f  ", dev.data.y);
    M5.Display.drawString(text_string, 10, 50);
    sprintf(text_string, "MAG Z: %.2f  ", dev.data.z);
    M5.Display.drawString(text_string, 10, 80);
    sprintf(text_string, "HEAD Angle: %.0f  ", head_dir);
    M5.Display.drawString(text_string, 10, 110);
    M5.Display.drawCentreString("Press BtnA enter calibrate", 160, 150);
    
    if(M5.BtnA.wasPressed())
    {
        initScreen();
        M5.Display.drawCentreString("Flip + rotate core calibration", 160, 110);
        bmm150_calibrate(10000);
        initScreen();
    }

    delay(100);
}
