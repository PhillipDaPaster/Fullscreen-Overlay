#include "overlay.h"
#include "../librarys/banding/banding.h"
#include <iostream>

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;


std::pair<HWND, WNDCLASSEXW> CreateWindowWithBand(std::wstring name) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = name.c_str();

    ATOM res = RegisterClassExW(&wc);
    if (!res) {
        DWORD dwError = GetLastError();
#if defined(_DEBUG)
        printf("RegisterClassExW error: 0x%08X\n", dwError);
#endif
        return { nullptr, {} };
    }

    CreateWindowInBand pCreateWindowInBand = reinterpret_cast<CreateWindowInBand>(GetProcAddress(LoadLibraryA("user32.dll"), "CreateWindowInBand"));
    if (!pCreateWindowInBand) {
#if defined(_DEBUG)
        printf("Failed to get CreateWindowInBand\n");
#endif
        return { nullptr, {} };
    }

    HWND hwnd = pCreateWindowInBand(WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, res, name.c_str(), WS_POPUP, 0, 0, 0, 0, NULL, NULL, wc.hInstance, LPVOID(res), ZBID_UIACCESS);
    if (!hwnd) {
        DWORD dwError = GetLastError();
#if defined(_DEBUG)
        printf("pCreateWindowInBand error: 0x%08X\n", dwError);
#endif
        return { nullptr, {} };
    }

    return { hwnd, wc };
}

std::pair<HWND, WNDCLASSEXW> CreateWindowExW(std::wstring name) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = name.c_str();

    if (!::RegisterClassExW(&wc)) {
        DWORD dwError = GetLastError();
#if defined(_DEBUG)
        printf("RegisterClassExW error: 0x%08X\n", dwError);
#endif
        return { nullptr, {} };
    }

    HWND hwnd = ::CreateWindowExW(WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, wc.lpszClassName, L"BeanerBot", WS_POPUP, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);

    if (!hwnd) {
        DWORD dwError = GetLastError();
#if defined(_DEBUG)
        printf("CreateWindowExW error: 0x%08X\n", dwError);
#endif
        return { nullptr, {} };
    }

#if defined(_DEBUG)
    printf("Window created hwnd:%p\n", hwnd);
#endif

    return { hwnd, wc };
}


void c_Overlay::Render(int mode, std::wstring windowname, std::wstring windowclass) {
    HWND GameHwnd = FindWindowW(windowclass.c_str(), windowname.c_str());

    int x = 0, y = 0, w = GetSystemMetrics(SM_CXSCREEN), h = GetSystemMetrics(SM_CYSCREEN);
    if (GetWindowRect(GameHwnd, &GameRect)) {
        x = GameRect.left;
        y = GameRect.top;
        w = GameRect.right - GameRect.left;
        h = GameRect.bottom - GameRect.top;
    }

    auto [hwnd, wc] = std::make_pair<HWND, WNDCLASSEXW>(nullptr, {});

    switch (mode) {
    case Mode::CreateWin:
        std::tie(hwnd, wc) = CreateWindowExW(L"CreateWin");
        break;
    case Mode::Banding:
        auto error = BandingCheck();
#if defined(_DEBUG)
        if (ERROR_SUCCESS != error)
            printf("UIAccess error: 0x%08X\n", dwError);
#endif
        std::tie(hwnd, wc) = CreateWindowWithBand(L"Banding"); // dont print before this it causes double prints
        break;
    }

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
    MARGINS margin = { -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margin);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);
    bool done = false;
    while (!done) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            showmenu = !showmenu;
        }

        MSG msg;
        while (::PeekMessage(&msg, hwnd, 0U, 0U, PM_REMOVE)) { 
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                done = true;
                break;
            }
        }


        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        HWND FHwnd = GetForegroundWindow();
        if (!(FHwnd == hwnd || FHwnd == GameHwnd)) { // just to stop crazy gpu usage
            Sleep(16);
        }

        RECT TmpRect{};
        POINT TmpPoint{};
        GetClientRect(GameHwnd, &TmpRect);
        ClientToScreen(GameHwnd, &TmpPoint);

        if (TmpRect.left != GameRect.left || TmpRect.bottom != GameRect.bottom ||
            TmpRect.top != GameRect.top || TmpRect.right != GameRect.right ||
            TmpPoint.x != GamePoint.x || TmpPoint.y != GamePoint.y) {
            GameRect = TmpRect;
            GamePoint = TmpPoint;

            // set z-order aka place the overlay right above the game window -- phillip
            HWND hwnd_above = GetWindow(GameHwnd, GW_HWNDPREV);
            SetWindowPos(hwnd, hwnd_above, TmpPoint.x, TmpPoint.y, GameRect.right - GameRect.left, GameRect.bottom - GameRect.top, SWP_NOREDRAW);
        }

        SetWindowPos(hwnd, (FHwnd == GameHwnd) ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowLong(hwnd, GWL_EXSTYLE, showmenu ? WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | ((FHwnd == GameHwnd) ? WS_EX_TOPMOST : 0) : WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

        static bool init = false;
        if (!init) {
            static ImWchar Ranges[] = {
                0x0020, 0xFFFF, // Unicode U+0020 to U+FFFF
                0x0020, 0x00FF, // Basic Latin + Latin Supplement
                0x0100, 0x017F, // Latin Extended-A
                0x0180, 0x024F, // Latin Extended-B
                0x0250, 0x02AF, // IPA Extensions
                0x0370, 0x03FF, // Greek and Coptic
                0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
                0x2DE0, 0x2DFF, // Cyrillic Extended-A
                0xA640, 0xA69F, // Cyrillic Extended-B
                0x0590, 0x05FF, // Hebrew
                0x0600, 0x06FF, // Arabic
                0x0900, 0x097F, // Devanagari
                0x2000, 0x206F, // General Punctuation
                0x20A0, 0x20CF, // Currency Symbols
                0x2100, 0x214F, // Letterlike Symbols
                0x2200, 0x22FF, // Mathematical Operators
                0x2300, 0x23FF, // Miscellaneous Technical
                0x2500, 0x257F, // Box Drawing
                0x25A0, 0x25FF, // Geometric Shapes
                0x2600, 0x26FF, // Miscellaneous Symbols
                0x3000, 0x30FF, // CJK Symbols and Punctuations, Hiragana, Katakana
                0x31F0, 0x31FF, // Katakana Phonetic Extensions
                0x4E00, 0x9FAF, // CJK Ideograms
                0xFF00, 0xFFEF, // Half-width characters
                0x1F300, 0x1F5FF, // Miscellaneous Symbols and Pictographs
                0x1F600, 0x1F64F, // Emoticons
                0x1F680, 0x1F6FF, // Transport and Map Symbols
                0x1F700, 0x1F77F, // Alchemical Symbols
                0x1F900, 0x1F9FF, // Supplemental Symbols and Pictographs
                0,
            };

            ImFontConfig FontConfig;
            FontConfig.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags::ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags::ImGuiFreeTypeBuilderFlags_MonoHinting;
            FontConfig.OversampleH = 2;
            FontConfig.OversampleV = 2;

            io.Fonts->Clear();
            io.Fonts->AddFontDefault(&FontConfig);
            io.Fonts->Build();
            init = true;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        SetWindowDisplayAffinity(hwnd, globals::streamproof ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE); // steamproof

        if (showmenu)
            gui.RenderMenu();

        ImGui::GetBackgroundDrawList()->AddText(ImVec2(GameRect.left + 10, GameRect.top + 10), ImColor(255, 255, 255, 255), (std::string(windowname.begin(), windowname.end()) + " | External").c_str());

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(globals::vsync ? 1 : 0, 0x00000100UL);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

bool c_Overlay::CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void c_Overlay::CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void c_Overlay::CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void c_Overlay::CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
