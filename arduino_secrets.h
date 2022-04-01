// SSID Credentials
#define SECRET_SSID "YourSSID"			// Your SSID
#define SECRET_PASS "YourSSIDPassword"  // Your SSID Password

// NTP Settings
#define TIMESERVER "be.pool.ntp.org"  // NTP server (works with local gateway/firewall if it hosts NTP)
bool NTP_ENABLED = true;              // Enable or disable NTP
const int LOCALPORT = 8888;           // Local UDP Port, default port = 2390

// DST Settings
bool DST_ENABLED = true;              // Enable or disable DST