//#######################################################################################################
//#################################### Plugin 242: CNY70  ###############################################
//#######################################################################################################

//#ifdef PLUGIN_BUILD_TESTING

#define PLUGIN_242
#define PLUGIN_ID_242 242
#define PLUGIN_NAME_242 "Generic - E-Meter with CNY-70"
#define PLUGIN_VALUENAME1_242 "Triggerstate"
#define PLUGIN_VALUENAME2_242 "Consumption"
#define PLUGIN_VALUENAME3_242 "Raw"

uint8_t Plugin_242_MeasurementCount = 0; // counter for measurements to calculate average
uint32_t Plugin_242_valueSum = 0;        // sum of values to calculate average
float Plugin_242_avgValue = 0;

uint32_t Plugin_242_avgValuePrevious = 0;        // sum of values to calculate average
int16_t Plugin_242_cumulativeSum = 0;

unsigned long Plugin_242_triggerTime;
unsigned long Plugin_242_triggerTimePrevious;

uint8_t Plugin_242_avgCounterMax = 10;

boolean Plugin_242_triggerstate;

boolean Plugin_242(byte function, struct EventStruct *event, String &string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
    {
      Device[++deviceCount].Number = PLUGIN_ID_242;
      Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
      Device[deviceCount].VType = SENSOR_TYPE_TRIPLE;
      Device[deviceCount].Ports = 0;
      Device[deviceCount].PullUpOption = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption = true;
      Device[deviceCount].ValueCount = 3;
      Device[deviceCount].SendDataOption = true;
      Device[deviceCount].TimerOption = false;
      Device[deviceCount].GlobalSyncOption = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME:
    {
      string = F(PLUGIN_NAME_242);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES:
    {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_242));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_242));
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME3_242));
      break;
    }

    case PLUGIN_INIT:
    {
      String log = F("connect IR Led to PIN: ");
      log += Settings.TaskDevicePin1[event->TaskIndex];
      addLog(LOG_LEVEL_INFO, log);

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_LOAD:
    {

      addFormCheckBox(string, F("Send raw data"), F("plugin_242_sendraw"), Settings.TaskDevicePluginConfig[event->TaskIndex][0]);
      addFormNote(string, F("on for calibration"));

      addFormTextBox(string, F("Trigger Level"), F("plugin_242_triggerlevel"), String(Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0]), 8);

      addFormTextBox(string, F("Hysteresis"), F("plugin_242_hyst"), String(Settings.TaskDevicePluginConfigFloat[event->TaskIndex][1]), 8);

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE:
    {
      Settings.TaskDevicePluginConfig[event->TaskIndex][0] = isFormItemChecked(F("plugin_242_sendraw"));
      Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0] = getFormItemFloat(F("plugin_242_triggerlevel"));
      Settings.TaskDevicePluginConfigFloat[event->TaskIndex][1] = getFormItemFloat(F("plugin_242_hyst"));

      success = true;
      break;
    }

    case PLUGIN_TEN_PER_SECOND:
    {
      String debuglog = F("");

      // perform measurement with IR LED off
      digitalWrite(Settings.TaskDevicePin1[event->TaskIndex], LOW);
      // wait 10 milliseconds
      delay(10);
      // read the analog in value:
      int16_t Plugin_242_sensorValueOff = analogRead(A0);

      // turn IR LED back on
      digitalWrite(Settings.TaskDevicePin1[event->TaskIndex], HIGH);
      delay(10);
      // read the analog in value:
      int16_t Plugin_242_sensorValueOn = analogRead(A0);

      // calculate difference
      int16_t Plugin_242_sensorValue = Plugin_242_sensorValueOff - Plugin_242_sensorValueOn;

      // build sums for average calculation
      Plugin_242_MeasurementCount++;
      Plugin_242_valueSum += Plugin_242_sensorValue;

      Plugin_242_avgValue = (float)(Plugin_242_valueSum / Plugin_242_MeasurementCount);

      debuglog += F("Raw Value: ");
      debuglog += Plugin_242_sensorValue;
      debuglog += F(" avgValue: ");
      debuglog += Plugin_242_avgValue;

      addLog(LOG_LEVEL_DEBUG, debuglog);

      success = true;
      break;
    }

    case PLUGIN_ONCE_A_SECOND:
    {
      String log = F("");

      UserVar[event->BaseVarIndex + 2] = Plugin_242_avgValue;

      int16_t difference = 0;

      // Previous value is 0 after init. Do not use this value for calculation.
      if(Plugin_242_avgValuePrevious != 0){
        difference = Plugin_242_avgValue - Plugin_242_avgValuePrevious;
      }

      Plugin_242_cumulativeSum += difference;

      log += F("Avg: ");
      log += Plugin_242_avgValue;
      log += F(" Difference: ");
      log += difference;
      log += F(" Cumulative Sum: ");
      log += Plugin_242_cumulativeSum;


      boolean state = Plugin_242_triggerstate;

      // compare with threshold value
      float valueLowThreshold = Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0] - (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][1] / 2);
      float valueHighThreshold = Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0] + (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][1] / 2);
      if (Plugin_242_cumulativeSum <= valueLowThreshold)
      state = false;
      if (Plugin_242_cumulativeSum >= valueHighThreshold)
      state = true;

      // if value changed
      if (state != Plugin_242_triggerstate)
      {
        Plugin_242_triggerstate = state;
        //statusLED(true);
        UserVar[event->BaseVarIndex] = state ? 1 : 0;

        log += F(" Trigger changed state: ");
        log += String(UserVar[event->BaseVarIndex], 0);

        if (state == false)
        {
          unsigned long rotationTime = timePassedSince(Plugin_242_triggerTimePrevious);
          Plugin_242_triggerTime = rotationTime;
          log += F(" Zeit (ms): ");
          log += rotationTime;

          unsigned long msPerHour = 3600 * 1000;

          UserVar[event->BaseVarIndex + 1] = (float)(msPerHour / rotationTime / 0.075);
          Plugin_242_triggerTimePrevious = millis();
          saveUserVarToRTC();

          // 3600 / gestoppte Durchschnittszeit / 75 U/kWh = aktueller laufender Verbrauch
          log += F(" Verbrauch (WH): ");
          log += UserVar[event->BaseVarIndex + 1];
        } 
        else 
        {
          // reset sum to eleminate value drift
          Plugin_242_cumulativeSum = 0;
        }

        sendData(event);
      }

      // reset Values
      Plugin_242_avgValuePrevious = Plugin_242_avgValue;
      Plugin_242_MeasurementCount = 0;
      Plugin_242_valueSum = 0;

      if (Settings.TaskDevicePluginConfig[event->TaskIndex][0]){
        sendData(event);
      }

      addLog(LOG_LEVEL_INFO, log);

      success = true;
      break;
    }

    case PLUGIN_READ:
    {
      // We do not actually read the value as this is already done 10x/second
      // Instead we just send the last known state stored in Uservar
      String log = F("PLUGIN_READ Triggerstate: ");
      log += String(UserVar[event->BaseVarIndex]);
      log += F(" Consuption: ");
      log += String(UserVar[event->BaseVarIndex + 1]);
      log += F(" Raw: ");
      log += String(UserVar[event->BaseVarIndex + 2]);
      log += F(" Time: now(): ");
      log += now();
      log += F(" getNtpTime(): ");
      log += getNtpTime();
      log += F(" millis(): ");
      log += millis();

      addLog(LOG_LEVEL_INFO, log);

      saveUserVarToRTC();

      if( Plugin_242_triggerstate ){
        // reset sum to eleminate value drift
        Plugin_242_cumulativeSum = 0;
      }

      success = true;
      break;
    }
  }
  return success;
}

//#endif
