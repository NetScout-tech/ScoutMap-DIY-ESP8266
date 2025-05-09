#include "structs.h"

// Инициализация переменных
String selectedSSID = "";
String selectedPassword = "";
bool awaitingPassword = false;
bool awaitingNetworkSelection = false;
int networkCount = 0;
String lastHostsArg = "";
String scanMode = "";
int startPort = 0;
int endPort = 0;

// Платформо-зависимые массивы
port_type_t portList[MAX_PORT_LIST] = {0};
int portListCount = 0;
int hostCount = 0;
Host hosts[MAX_HOSTS];

// Остальные переменные
bool scanningHosts = false;
bool scanningPorts = false;
int totalSteps = 0;
int currentStep = 0;
float progress = 0.0;
bool debugMode = false;
bool silentMode = false;
bool uploadReport = false;
int currentHostIndex = 0;
int currentPortIndex = 0;
int currentPort = 0;
bool deviceConnected = false;
String apiKey = "";
bool browserMode = false;
bool awaitingLocationName = false;