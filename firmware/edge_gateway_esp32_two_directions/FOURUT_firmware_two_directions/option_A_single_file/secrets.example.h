#ifndef FOURUT_SECRETS_H
#define FOURUT_SECRETS_H

/*
 * Local credential template.
 *
 * 1. Copy this file to secrets.h in the same folder as the .ino file.
 * 2. Fill in local values.
 * 3. Never commit secrets.h.
 */

#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define AWS_IOT_ENDPOINT "your-endpoint-ats.iot.your-region.amazonaws.com"

static const char AWS_CERT_CA[] = R"EOF(
-----BEGIN CERTIFICATE-----
YOUR_ROOT_CA_CERTIFICATE
-----END CERTIFICATE-----
)EOF";

static const char AWS_CERT_CRT[] = R"EOF(
-----BEGIN CERTIFICATE-----
YOUR_DEVICE_CERTIFICATE
-----END CERTIFICATE-----
)EOF";

static const char AWS_CERT_PRIVATE[] = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
YOUR_PRIVATE_KEY
-----END RSA PRIVATE KEY-----
)EOF";

#endif
