#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "implot.h"
#include <d3d9.h>
#include <tchar.h>
#include <chrono>
#include <cmath>
#include <vector>
#include <imgui_internal.h>

// Data
static LPDIRECT3D9              g_pD3D = NULL;
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};
static bool                     g_paused = false;
static bool                     g_vel = false;
static bool                     g_yaw = false;
static double                   g_time = 0.0;
static double                   g_time_step = 0.1;
static std::vector<double>      g_times;
static std::vector<double>      cmd_linearVel;
static std::vector<double>      odom_linearVel;
static std::vector<double>      cmd_angularVel;
static std::vector<double>      odom_angularVel;
static std::vector<double>      odom_yaw;
static bool                     g_show_cmd_linearVel = false;
static bool                     g_show_odom_linearVel = false;
static bool                     g_show_cmd_angularVel = false;
static bool                     g_show_odom_angularVel = false;
static ImPlotRect               g_plotLimits;
static bool                     g_plotYaw_open = false;
static bool                     g_angle_window_first_open = true;  // Flag to track the first launch of the angle window

// Keyboard input booleans
static bool                     g_key_W = false;
static bool                     g_key_S = false;
static bool                     g_key_X = false;
static bool                     g_key_A = false;
static bool                     g_key_D = false;
static bool                     g_key_Z = false;
static bool                     g_key_C = false;
static bool                     g_key_Q = false;
static bool                     g_key_E = false;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void UpdateValues() {
    if (!g_paused) {
        g_time += g_time_step;

        // Simulate values
        cmd_linearVel.push_back(0.5 + 0.5 * sin(g_time));
        odom_linearVel.push_back(0.5 + 0.5 * cos(g_time));
        cmd_angularVel.push_back(0.5 + 0.5 * sin(g_time) * 0.5);
        odom_angularVel.push_back(0.5 + 0.5 * cos(g_time) * 0.5);
        odom_yaw.push_back(fmod(g_time, 2 * IM_PI) - IM_PI);

        g_times.push_back(g_time);
    }
}

void PlotValues() {
    if (ImPlot::BeginPlot("Plot")) {
        ImPlot::SetupAxes("Time (s)", "Value");

        if (!g_paused) {
            ImPlot::SetupAxisLimits(ImAxis_X1, g_time - 10.0, g_time, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1, ImGuiCond_Always);
        }

        if (g_show_cmd_linearVel) {
            ImPlot::PlotLine("cmd.linearVel", g_times.data(), cmd_linearVel.data(), g_times.size());
        }
        if (g_show_odom_linearVel) {
            ImPlot::PlotLine("odom.linearVel", g_times.data(), odom_linearVel.data(), g_times.size());
        }
        if (g_show_cmd_angularVel) {
            ImPlot::PlotLine("cmd.angularVel", g_times.data(), cmd_angularVel.data(), g_times.size());
        }
        if (g_show_odom_angularVel) {
            ImPlot::PlotLine("odom.angularVel", g_times.data(), odom_angularVel.data(), g_times.size());
        }

        ImPlot::EndPlot();
    }
}

void DrawArrow(ImDrawList* draw_list, ImVec2 center, float angle, float length, ImU32 color, float thickness) {
    // Calculate the end point of the arrow
    ImVec2 arrow_head = ImVec2(center.x + length * cos(angle), center.y + length * sin(angle));
    
    // Calculate the points for the left and right lines of the arrowhead
    ImVec2 arrow_left = ImVec2(center.x + (length - 10) * cos(angle + IM_PI / 6), center.y + (length - 10) * sin(angle + IM_PI / 6));
    ImVec2 arrow_right = ImVec2(center.x + (length - 10) * cos(angle - IM_PI / 6), center.y + (length - 10) * sin(angle - IM_PI / 6));
    
    // Draw the main line of the arrow from the center to the arrowhead
    draw_list->AddLine(center, arrow_head, color, thickness);
    
    // Draw the left and right lines of the arrowhead
    draw_list->AddLine(arrow_head, arrow_left, color, thickness);
    draw_list->AddLine(arrow_head, arrow_right, color, thickness);
}

void PlotYaw() {
    if (g_angle_window_first_open && g_plotYaw_open) {
        odom_yaw.clear();
        g_angle_window_first_open = false;
    }

    if (ImGui::Begin("Yaw Plot", &g_plotYaw_open)) {
        ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 center = ImVec2(p.x + 150, p.y + 150);

        // Draw black background
        draw_list->AddRectFilled(ImVec2(center.x - 150, center.y - 150), ImVec2(center.x + 150, center.y + 150), IM_COL32(0, 0, 0, 255));

        // Define arrow properties
        float arrow_length = 80.0f;  // Arrow length
        float arrow_thickness = 3.0f;
        ImU32 arrow_color = IM_COL32(255, 0, 0, 255);  // Red color

        // Draw previous arrows
        for (size_t i = 0; i < odom_yaw.size(); ++i) {
            float angle = static_cast<float>(odom_yaw[i]);
            DrawArrow(draw_list, center, angle, arrow_length, arrow_color, arrow_thickness);
        }

        // Draw current arrow with emphasis (thicker line)
        if (!odom_yaw.empty()) {
            float current_angle = static_cast<float>(odom_yaw.back());
            DrawArrow(draw_list, center, current_angle, arrow_length, arrow_color, arrow_thickness + 2.0f);
        }

        ImGui::Dummy(ImVec2(300, 300));
    } else {
        g_plotYaw_open = false;
        g_angle_window_first_open = true;  // Reset the flag only when the window is closed
    }
    ImGui::End();
}


void ProcessKeyboardInputs() {
    ImGuiIO& io = ImGui::GetIO();
    g_key_W = io.KeysDown[ImGuiKey_W];
    g_key_S = io.KeysDown[ImGuiKey_S];
    g_key_X = io.KeysDown[ImGuiKey_X];
    g_key_A = io.KeysDown[ImGuiKey_A];
    g_key_D = io.KeysDown[ImGuiKey_D];
    g_key_Z = io.KeysDown[ImGuiKey_Z];
    g_key_C = io.KeysDown[ImGuiKey_C];
    g_key_Q = io.KeysDown[ImGuiKey_Q];
    g_key_E = io.KeysDown[ImGuiKey_E];
}

/*
void ShowKeyboardInputsWindow() {
    ImGui::Begin("Keyboard Inputs");
    ImGui::Text("W: %s", g_key_W ? "Pressed" : "Released");
    ImGui::Text("S: %s", g_key_S ? "Pressed" : "Released");
    ImGui::Text("X: %s", g_key_X ? "Pressed" : "Released");
    ImGui::Text("A: %s", g_key_A ? "Pressed" : "Released");
    ImGui::Text("D: %s", g_key_D ? "Pressed" : "Released");
    ImGui::Text("Z: %s", g_key_Z ? "Pressed" : "Released");
    ImGui::Text("C: %s", g_key_C ? "Pressed" : "Released");
    ImGui::Text("Q: %s", g_key_Q ? "Pressed" : "Released");
    ImGui::Text("E: %s", g_key_E ? "Pressed" : "Released");
    ImGui::End();
}
*/
int main(int, char**) {
    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("AnscerControls"), NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, _T("Odom Debugging"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Main loop
    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Update time and data
        UpdateValues();
        ProcessKeyboardInputs();

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // start/stop buttons
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Buttons");
        if (ImGui::Button("Stop")) g_paused = true;
        if (ImGui::Button("Resume")) g_paused = false;
        ImGui::End();

        // Control panel
        ImGui::SetNextWindowPos(ImVec2(0, 100), ImGuiCond_FirstUseEver);
        ImGui::Begin("control panal");
        if (ImGui::Button("velocity"))  g_vel = !g_vel;
        if (ImGui::Button("angleodom")) {
            g_yaw = !g_yaw;
            if (g_yaw) {
                g_plotYaw_open = true;
                g_angle_window_first_open = true;  // Reset the flag when the angle button is clicked
            }
        }
        ImGui::End();

        if (g_vel) {
            // Plot window
            ImGui::SetNextWindowPos(ImVec2(200, 0), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(1080, 800), ImGuiCond_FirstUseEver);
            ImGui::Begin("velocity Plot");
            PlotValues();
            ImGui::End();

            // Value Selection
            ImGui::SetNextWindowPos(ImVec2(0, 50), ImGuiCond_FirstUseEver);
            ImGui::Begin("Values");
            if (ImGui::Button("cmd.linearVel")) g_show_cmd_linearVel = !g_show_cmd_linearVel;
            if (ImGui::Button("odom.linearVel")) g_show_odom_linearVel = !g_show_odom_linearVel;
            if (ImGui::Button("cmd.angularVel")) g_show_cmd_angularVel = !g_show_cmd_angularVel;
            if (ImGui::Button("odom.angularVel")) g_show_odom_angularVel = !g_show_odom_angularVel;
            ImGui::End();
        }

        if (g_yaw) {
            PlotYaw();
        }

        // Show keyboard inputs window
        //ShowKeyboardInputsWindow();

        // Rendering
        ImGui::Render();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

        if (result == D3DERR_DEVICELOST && g_pd3dDevice->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
            ResetDevice();
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            g_d3dpp.BackBufferWidth = LOWORD(lParam);
            g_d3dpp.BackBufferHeight = HIWORD(lParam);
            ResetDevice();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)  // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
