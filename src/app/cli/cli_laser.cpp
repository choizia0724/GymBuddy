#include <Arduino.h>
#include "src/devices/laser/laser.h"
#include "cli_laser.h"

void handleSerialLaserCommand() {
  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.isEmpty()) return;

  String upper = line; upper.toUpperCase();

  if (upper == "LASER ON") {
    Laser::on();
    Serial.println("Laser enabled (duty=" + String(Laser::duty()) + "%, freq=" + String(Laser::freq()) + "Hz)");
    return;
  }
  if (upper == "LASER OFF") {
    Laser::off();
    Serial.println("Laser disabled");
    return;
  }
  if (upper.startsWith("LASER F=")) {
    long freq = upper.substring(8).toInt();
    if (freq >= Laser::MIN_FREQ_HZ && freq <= Laser::MAX_FREQ_HZ) {
      Laser::setFreq(freq);
      Serial.println("Laser frequency set to " + String(freq) + "Hz");
    } else {
      Serial.println("Frequency out of range (" + String(Laser::MIN_FREQ_HZ) + "-" + String(Laser::MAX_FREQ_HZ) + "Hz)");
    }
    return;
  }
  if (upper.startsWith("LASER D=")) {
    long duty = upper.substring(8).toInt();
    if (duty >= 0 && duty <= 100) {
      Laser::setDuty(duty);
      Serial.println("Laser duty set to " + String(duty) + "%");
    } else {
      Serial.println("Duty out of range (0-100%)");
    }
    return;
  }

  Serial.println("Unknown command. Use 'LASER ON', 'LASER OFF', 'LASER F=<2000-5000>', 'LASER D=<0-100>'");
}