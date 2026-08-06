/*
 * Chamber Master - Web Dashboard Module (HTML UI, REST API, mDNS)
 */

#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include "config.h"

void setupWiFiAndServer();
void handleRoot();
void handleStatus();
void handleStartCooldown();
void serviceWebClient();

#endif // WEB_DASHBOARD_H
