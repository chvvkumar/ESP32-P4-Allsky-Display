# Health API Usage Examples

## Command Line Examples

### Basic Health Check
```bash
# Get health report
curl http://allskyesp32.lan:8080/api/health

# Pretty print JSON response
curl -s http://allskyesp32.lan:8080/api/health | python3 -m json.tool

# Save to file
curl -s http://allskyesp32.lan:8080/api/health > device_health.json
```

### Extract Specific Health Metrics
```bash
# Get overall status only (requires jq)
curl -s http://allskyesp32.lan:8080/api/health | jq '.overall.status'

# Get memory usage percentage
curl -s http://allskyesp32.lan:8080/api/health | jq '.memory.heap_usage_percent'

# Get WiFi signal strength
curl -s http://allskyesp32.lan:8080/api/health | jq '.network.rssi'

# Get all recommendations
curl -s http://allskyesp32.lan:8080/api/health | jq '.recommendations[]'
```

### Monitoring Script Example
```bash
#!/bin/bash
# health_monitor.sh - Check device health every 5 minutes

DEVICE_URL="http://allskyesp32.lan:8080"
LOG_FILE="health_log.txt"

while true; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    STATUS=$(curl -s "$DEVICE_URL/api/health" | jq -r '.overall.status')
    TEMP=$(curl -s "$DEVICE_URL/api/health" | jq -r '.system.temperature')
    HEAP=$(curl -s "$DEVICE_URL/api/health" | jq -r '.memory.heap_usage_percent')
    
    echo "$TIMESTAMP | Status: $STATUS | Temp: ${TEMP}°C | Heap: ${HEAP}%" >> "$LOG_FILE"
    
    # Alert if critical
    if [ "$STATUS" == "CRITICAL" ] || [ "$STATUS" == "FAILING" ]; then
        echo "ALERT: Device health is $STATUS!" | mail -s "ESP32 Health Alert" admin@example.com
    fi
    
    sleep 300  # 5 minutes
done
```

## Python Examples

### Basic Health Check
```python
import requests
import json

# Get health report
response = requests.get('http://allskyesp32.lan:8080/api/health')
health = response.json()

print(f"Overall Status: {health['overall']['status']}")
print(f"Message: {health['overall']['message']}")
print(f"Critical Issues: {health['overall']['critical_issues']}")
print(f"Warnings: {health['overall']['warnings']}")

# Print recommendations
if health['recommendations']:
    print("\nRecommendations:")
    for i, rec in enumerate(health['recommendations'], 1):
        print(f"  {i}. {rec}")
```

### Health Monitoring Class
```python
import requests
from datetime import datetime
import time

class DeviceHealthMonitor:
    def __init__(self, device_url):
        self.device_url = device_url
        self.health_url = f"{device_url}/api/health"
        
    def get_health(self):
        """Fetch current health report"""
        try:
            response = requests.get(self.health_url, timeout=5)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error fetching health: {e}")
            return None
    
    def check_memory(self, health):
        """Check memory health"""
        memory = health.get('memory', {})
        status = memory.get('status', 'UNKNOWN')
        heap_usage = memory.get('heap_usage_percent', 0)
        psram_usage = memory.get('psram_usage_percent', 0)
        
        print(f"Memory Status: {status}")
        print(f"  Heap Usage: {heap_usage:.1f}%")
        print(f"  PSRAM Usage: {psram_usage:.1f}%")
        
        return status in ['EXCELLENT', 'GOOD']
    
    def check_network(self, health):
        """Check network health"""
        network = health.get('network', {})
        status = network.get('status', 'UNKNOWN')
        rssi = network.get('rssi', 0)
        disconnects = network.get('disconnect_count', 0)
        
        print(f"Network Status: {status}")
        print(f"  Signal: {rssi} dBm")
        print(f"  Disconnects: {disconnects}")
        
        return status in ['EXCELLENT', 'GOOD']
    
    def check_system(self, health):
        """Check system health"""
        system = health.get('system', {})
        status = system.get('status', 'UNKNOWN')
        temp = system.get('temperature', 0)
        boot_count = system.get('boot_count', 0)
        last_crash = system.get('last_boot_crash', False)
        
        print(f"System Status: {status}")
        print(f"  Temperature: {temp:.1f}°C")
        print(f"  Boot Count: {boot_count}")
        print(f"  Last Boot Crash: {'Yes' if last_crash else 'No'}")
        
        return status in ['EXCELLENT', 'GOOD'] and not last_crash
    
    def monitor_continuously(self, interval=300):
        """Monitor health continuously"""
        print(f"Starting health monitoring (interval: {interval}s)")
        
        while True:
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            print(f"\n{'='*60}")
            print(f"Health Check: {timestamp}")
            print('='*60)
            
            health = self.get_health()
            if health:
                overall = health.get('overall', {})
                print(f"Overall: {overall.get('status', 'UNKNOWN')}")
                print(f"Message: {overall.get('message', 'N/A')}")
                print()
                
                # Check components
                self.check_memory(health)
                print()
                self.check_network(health)
                print()
                self.check_system(health)
                
                # Show recommendations
                if health.get('recommendations'):
                    print("\nRecommendations:")
                    for i, rec in enumerate(health['recommendations'], 1):
                        print(f"  {i}. {rec}")
            
            time.sleep(interval)

# Usage
if __name__ == '__main__':
    monitor = DeviceHealthMonitor('http://allskyesp32.lan:8080')
    
    # Single check
    health = monitor.get_health()
    if health:
        print(f"Device Status: {health['overall']['status']}")
    
    # Continuous monitoring (uncomment to enable)
    # monitor.monitor_continuously(interval=300)
```

### Alert System Example
```python
import requests
import smtplib
from email.mime.text import MIMEText
from datetime import datetime

class HealthAlerter:
    def __init__(self, device_url, smtp_config):
        self.device_url = device_url
        self.smtp_config = smtp_config
        self.last_alert_time = {}
        
    def send_alert(self, subject, message):
        """Send email alert"""
        msg = MIMEText(message)
        msg['Subject'] = subject
        msg['From'] = self.smtp_config['from']
        msg['To'] = self.smtp_config['to']
        
        with smtplib.SMTP(self.smtp_config['server'], self.smtp_config['port']) as server:
            if self.smtp_config.get('use_tls'):
                server.starttls()
            if self.smtp_config.get('username'):
                server.login(self.smtp_config['username'], self.smtp_config['password'])
            server.send_message(msg)
    
    def check_and_alert(self):
        """Check health and send alerts if needed"""
        try:
            response = requests.get(f"{self.device_url}/api/health", timeout=5)
            health = response.json()
            
            overall_status = health['overall']['status']
            
            # Alert on critical status
            if overall_status in ['CRITICAL', 'FAILING']:
                alert_key = f"overall_{overall_status}"
                if self._should_alert(alert_key, cooldown_minutes=30):
                    subject = f"ESP32 Health Alert: {overall_status}"
                    message = self._format_alert_message(health)
                    self.send_alert(subject, message)
            
            # Alert on high temperature
            temp = health['system']['temperature']
            if temp > 80:
                alert_key = "high_temperature"
                if self._should_alert(alert_key, cooldown_minutes=60):
                    subject = "ESP32 Temperature Alert"
                    message = f"Device temperature is high: {temp:.1f}°C\n\n"
                    message += "Recommended actions:\n"
                    message += "- Check device ventilation\n"
                    message += "- Reduce ambient temperature\n"
                    message += "- Consider adding cooling\n"
                    self.send_alert(subject, message)
            
            # Alert on memory issues
            if health['memory']['status'] in ['CRITICAL', 'FAILING']:
                alert_key = "memory_critical"
                if self._should_alert(alert_key, cooldown_minutes=60):
                    subject = "ESP32 Memory Alert"
                    message = self._format_memory_alert(health['memory'])
                    self.send_alert(subject, message)
                    
        except Exception as e:
            print(f"Error checking health: {e}")
    
    def _should_alert(self, alert_key, cooldown_minutes=30):
        """Check if enough time has passed since last alert"""
        now = datetime.now()
        last_time = self.last_alert_time.get(alert_key)
        
        if not last_time:
            self.last_alert_time[alert_key] = now
            return True
        
        elapsed = (now - last_time).total_seconds() / 60
        if elapsed >= cooldown_minutes:
            self.last_alert_time[alert_key] = now
            return True
        
        return False
    
    def _format_alert_message(self, health):
        """Format comprehensive alert message"""
        msg = f"Device Health Status: {health['overall']['status']}\n"
        msg += f"Message: {health['overall']['message']}\n\n"
        
        msg += f"Critical Issues: {health['overall']['critical_issues']}\n"
        msg += f"Warnings: {health['overall']['warnings']}\n\n"
        
        msg += "Component Status:\n"
        msg += f"  Memory: {health['memory']['status']}\n"
        msg += f"  Network: {health['network']['status']}\n"
        msg += f"  MQTT: {health['mqtt']['status']}\n"
        msg += f"  System: {health['system']['status']}\n"
        msg += f"  Display: {health['display']['status']}\n\n"
        
        if health['recommendations']:
            msg += "Recommendations:\n"
            for i, rec in enumerate(health['recommendations'], 1):
                msg += f"  {i}. {rec}\n"
        
        return msg
    
    def _format_memory_alert(self, memory):
        """Format memory-specific alert"""
        msg = f"Memory Status: {memory['status']}\n\n"
        msg += f"Heap Usage: {memory['heap_usage_percent']:.1f}%\n"
        msg += f"Free Heap: {memory['free_heap']:,} bytes\n"
        msg += f"Min Free Heap: {memory['min_free_heap']:,} bytes\n\n"
        msg += f"PSRAM Usage: {memory['psram_usage_percent']:.1f}%\n"
        msg += f"Free PSRAM: {memory['free_psram']:,} bytes\n"
        msg += f"Min Free PSRAM: {memory['min_free_psram']:,} bytes\n"
        return msg

# Usage
smtp_config = {
    'server': 'smtp.gmail.com',
    'port': 587,
    'use_tls': True,
    'username': 'your-email@gmail.com',
    'password': 'your-app-password',
    'from': 'your-email@gmail.com',
    'to': 'admin@example.com'
}

alerter = HealthAlerter('http://allskyesp32.lan:8080', smtp_config)
alerter.check_and_alert()
```

## JavaScript/Node.js Examples

### Basic Health Check
```javascript
const axios = require('axios');

async function checkHealth() {
    try {
        const response = await axios.get('http://allskyesp32.lan:8080/api/health');
        const health = response.data;
        
        console.log('Overall Status:', health.overall.status);
        console.log('Message:', health.overall.message);
        console.log('Critical Issues:', health.overall.critical_issues);
        console.log('Warnings:', health.overall.warnings);
        
        if (health.recommendations.length > 0) {
            console.log('\nRecommendations:');
            health.recommendations.forEach((rec, i) => {
                console.log(`  ${i + 1}. ${rec}`);
            });
        }
        
        return health;
    } catch (error) {
        console.error('Error fetching health:', error.message);
        return null;
    }
}

checkHealth();
```

### Home Assistant Integration
```yaml
# configuration.yaml
rest:
  - resource: http://allskyesp32.lan:8080/api/health
    scan_interval: 300
    sensor:
      - name: "AllSky Health Status"
        value_template: "{{ value_json.overall.status }}"
        
      - name: "AllSky Memory Usage"
        value_template: "{{ value_json.memory.heap_usage_percent }}"
        unit_of_measurement: "%"
        
      - name: "AllSky Temperature"
        value_template: "{{ value_json.system.temperature }}"
        unit_of_measurement: "°C"
        device_class: temperature
        
      - name: "AllSky WiFi Signal"
        value_template: "{{ value_json.network.rssi }}"
        unit_of_measurement: "dBm"
        device_class: signal_strength

automation:
  - alias: "AllSky Health Alert"
    trigger:
      - platform: state
        entity_id: sensor.allsky_health_status
        to: "CRITICAL"
    action:
      - service: notify.notify
        data:
          title: "AllSky Camera Alert"
          message: "Device health is CRITICAL! Check the device immediately."
```

## Grafana Dashboard Example

Use Prometheus with node_exporter or create a custom exporter that queries the health API and exposes metrics for Grafana visualization.

```python
# prometheus_exporter.py
from prometheus_client import start_http_server, Gauge
import requests
import time

# Define metrics
health_status = Gauge('allsky_health_status', 'Overall health status (0-4)')
heap_usage = Gauge('allsky_heap_usage_percent', 'Heap memory usage percentage')
psram_usage = Gauge('allsky_psram_usage_percent', 'PSRAM usage percentage')
temperature = Gauge('allsky_temperature_celsius', 'CPU temperature')
wifi_rssi = Gauge('allsky_wifi_rssi_dbm', 'WiFi signal strength')
disconnect_count = Gauge('allsky_network_disconnects', 'Network disconnect count')

STATUS_MAP = {
    'EXCELLENT': 0,
    'GOOD': 1,
    'WARNING': 2,
    'CRITICAL': 3,
    'FAILING': 4
}

def collect_metrics():
    try:
        response = requests.get('http://allskyesp32.lan:8080/api/health', timeout=5)
        health = response.json()
        
        # Update metrics
        status_value = STATUS_MAP.get(health['overall']['status'], 4)
        health_status.set(status_value)
        
        heap_usage.set(health['memory']['heap_usage_percent'])
        psram_usage.set(health['memory']['psram_usage_percent'])
        temperature.set(health['system']['temperature'])
        wifi_rssi.set(health['network']['rssi'])
        disconnect_count.set(health['network']['disconnect_count'])
        
    except Exception as e:
        print(f"Error collecting metrics: {e}")

if __name__ == '__main__':
    start_http_server(9100)
    print("Prometheus exporter running on port 9100")
    
    while True:
        collect_metrics()
        time.sleep(60)  # Update every minute
```
