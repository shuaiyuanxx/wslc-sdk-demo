// WSLC SDK Demo — Client/Server
//
// Demonstrates the full WSL Containers developer workflow:
//
//   Build time (PR #14551):
//     The echo server container image is automatically built from
//     container/Dockerfile during MSBuild via <WslcImage>.
//
//   Run time:
//     Debug  — attaches to an existing wslc CLI session (WslcGetCliSession)
//     Release — creates a standalone session and pulls the image from
//               registry (WslcCreateSession + WslcPullSessionImage)
//
//     1. Acquires a session (and image)
//     2. Creates and starts a container with port mapping
//     3. Sends messages to the server, prints responses
//     4. Ctrl+C to gracefully stop and clean up
//

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wslcsdk.h>
#include <stdio.h>
#include <string>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")

#define SERVER_PORT 9000
#define SEND_INTERVAL_MS 2000

// --------------------------------------------------------------------------
// Globals for cleanup
// --------------------------------------------------------------------------

static volatile bool g_running = true;
static WslcContainer g_container = nullptr;
static WslcSession g_session = nullptr;

static BOOL WINAPI ConsoleHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
    {
        printf("\n[client] Ctrl+C received, shutting down...\n");
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

static void PrintError(const wchar_t* context, HRESULT hr, PWSTR errorMessage)
{
    printf("[ERROR] %ls: HRESULT=0x%08lx", context, hr);
    if (errorMessage)
    {
        printf(" - %ls", errorMessage);
        CoTaskMemFree(errorMessage);
    }
    printf("\n");
}

// --------------------------------------------------------------------------
// TCP client helpers
// --------------------------------------------------------------------------

static SOCKET ConnectToServer(int port, int maxRetries)
{
    for (int attempt = 1; attempt <= maxRetries; attempt++)
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(port));
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
        {
            // Set receive timeout
            DWORD timeout = SEND_INTERVAL_MS;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            return sock;
        }

        closesocket(sock);
        if (attempt < maxRetries)
        {
            printf("    Waiting for server (attempt %d/%d)...\n", attempt, maxRetries);
            Sleep(1000);
        }
    }
    return INVALID_SOCKET;
}

// --------------------------------------------------------------------------
// Main
// --------------------------------------------------------------------------

int wmain()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    printf("========================================================\n");
    printf("  WSLC SDK Demo — Echo Client/Server\n");
    printf("========================================================\n");
    printf("  Build:   Container image built automatically by MSBuild\n");
    printf("  Runtime: SDK creates container, client sends messages\n");
    printf("  Press Ctrl+C to stop\n");
    printf("========================================================\n\n");

    HRESULT hr;
    PWSTR errorMessage = nullptr;

    // ------------------------------------------------------------------
    // Step 1: Acquire a session (and image)
    // ------------------------------------------------------------------

#ifdef _DEBUG
    // Debug: attach to an existing wslc CLI session (image already local)
    printf("[1] Connecting to wslc CLI session...\n");

    hr = WslcGetCliSession(&g_session, &errorMessage);
    if (FAILED(hr))
    {
        PrintError(L"WslcGetCliSession", hr, errorMessage);
        printf("    Ensure a wslc session is running (try: wslc image list)\n");
        goto cleanup;
    }
    printf("    Connected.\n\n");
#else
    // Release: create a standalone session and pull the image from registry
    printf("[1] Creating session and pulling image from registry...\n");

    {
        WslcSessionSettings sessionSettings{};
        hr = WslcInitSessionSettings(L"demo-session", L"C:\\wslc\\demo", &sessionSettings);
        if (FAILED(hr))
        {
            printf("[ERROR] WslcInitSessionSettings: 0x%08lx\n", hr);
            goto cleanup;
        }

        hr = WslcCreateSession(&sessionSettings, &g_session, &errorMessage);
        if (FAILED(hr))
        {
            PrintError(L"WslcCreateSession", hr, errorMessage);
            goto cleanup;
        }
        printf("    Session created.\n");

        WslcPullImageOptions pullOptions{};
        pullOptions.uri = "echo-server:latest"; // Replace with full registry URI for production
        pullOptions.progressCallback = nullptr;
        pullOptions.progressCallbackContext = nullptr;
        pullOptions.authInfo = nullptr;

        hr = WslcPullSessionImage(g_session, &pullOptions, &errorMessage);
        if (FAILED(hr))
        {
            PrintError(L"WslcPullSessionImage", hr, errorMessage);
            goto cleanup;
        }
        printf("    Image pulled.\n\n");
    }
#endif

    // ------------------------------------------------------------------
    // Step 2: Create and configure the container
    // ------------------------------------------------------------------

    {
        printf("[2] Creating container from 'echo-server:latest'...\n");

        WslcContainerSettings containerSettings{};
        hr = WslcInitContainerSettings("echo-server:latest", &containerSettings);
        if (FAILED(hr))
        {
            printf("[ERROR] WslcInitContainerSettings: 0x%08lx\n", hr);
            goto cleanup;
        }

        WslcSetContainerSettingsName(&containerSettings, "demo-echo-server");
        WslcSetContainerSettingsFlags(&containerSettings, WSLC_CONTAINER_FLAG_AUTO_REMOVE);

        // Port mapping: host:9000 → container:9000
        WslcContainerPortMapping portMapping{};
        portMapping.windowsPort = SERVER_PORT;
        portMapping.containerPort = SERVER_PORT;
        portMapping.protocol = WSLC_PORT_PROTOCOL_TCP;
        WslcSetContainerSettingsPortMappings(&containerSettings, &portMapping, 1);

        // Networking: bridged mode for port mapping
        WslcSetContainerSettingsNetworkingMode(&containerSettings, WSLC_CONTAINER_NETWORKING_MODE_BRIDGED);

        hr = WslcCreateContainer(g_session, &containerSettings, &g_container, &errorMessage);
        if (FAILED(hr))
        {
            PrintError(L"WslcCreateContainer", hr, errorMessage);
            goto cleanup;
        }
        printf("    Container created.\n\n");
    }

    // ------------------------------------------------------------------
    // Step 3: Start the container
    // ------------------------------------------------------------------

    printf("[3] Starting echo server container...\n");

    hr = WslcStartContainer(g_container, WSLC_CONTAINER_START_FLAG_NONE, &errorMessage);
    if (FAILED(hr))
    {
        PrintError(L"WslcStartContainer", hr, errorMessage);
        goto cleanup;
    }
    printf("    Server container started (port %d).\n\n", SERVER_PORT);

    // ------------------------------------------------------------------
    // Step 4: Client loop — send messages, print responses
    // ------------------------------------------------------------------

    {
        printf("[4] Connecting to server...\n");

        SOCKET sock = ConnectToServer(SERVER_PORT, 10);
        if (sock == INVALID_SOCKET)
        {
            printf("[ERROR] Could not connect to server on port %d\n", SERVER_PORT);
            goto cleanup;
        }
        printf("    Connected to echo server!\n\n");

        printf("--- Sending messages (Ctrl+C to stop) ---\n\n");

        int msgCount = 0;
        char sendBuf[256];
        char recvBuf[4096];

        while (g_running)
        {
            msgCount++;

            // Build a message with timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm tm_buf;
            localtime_s(&tm_buf, &time_t_now);

            int len = sprintf_s(sendBuf, "Hello from Windows! Message #%d", msgCount);

            // Send
            int sent = send(sock, sendBuf, len, 0);
            if (sent == SOCKET_ERROR)
            {
                printf("[client] Send failed (error %d), reconnecting...\n", WSAGetLastError());
                closesocket(sock);
                sock = ConnectToServer(SERVER_PORT, 3);
                if (sock == INVALID_SOCKET)
                {
                    printf("[client] Reconnect failed, exiting.\n");
                    break;
                }
                continue;
            }

            // Receive response
            int received = recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);
            if (received > 0)
            {
                recvBuf[received] = '\0';
                char timeStr[16];
                strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm_buf);

                printf("[%s] Sent: %-35s | Response: %s\n", timeStr, sendBuf, recvBuf);
            }
            else if (received == 0)
            {
                printf("[client] Server disconnected.\n");
                break;
            }

            // Wait before next message
            for (int i = 0; i < SEND_INTERVAL_MS / 100 && g_running; i++)
                Sleep(100);
        }

        if (sock != INVALID_SOCKET)
            closesocket(sock);
    }

    // ------------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------------

cleanup:
    printf("\n[5] Cleaning up...\n");

    if (g_container)
    {
        printf("    Stopping container...\n");
        WslcStopContainer(g_container, WSLC_SIGNAL_SIGTERM, 5, nullptr);
        WslcDeleteContainer(g_container, WSLC_DELETE_CONTAINER_FLAG_FORCE, nullptr);
        WslcReleaseContainer(g_container);
    }

    if (g_session)
    {
#ifndef _DEBUG
        // We created this session, so terminate it before releasing
        WslcTerminateSession(g_session);
#endif
        WslcReleaseSession(g_session);
    }

    WSACleanup();
    CoUninitialize();

    printf("    Done.\n");
    return 0;
}
